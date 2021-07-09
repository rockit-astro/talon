/*******************************************\

    Virtual Motion Controller

    Designed to mimic an actual motor/encoder/switch
    setup of a real telescope system.
    Loosely analogous to the CSIMC setup.

    Simulated node is a motor, no encoder, any number of motor steps
    positive sign

    S. Ohmert June 21, 2001

\********************************************/

#include "virmc.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRACE_ON 0
#if TRACE_ON
#define TRACE fprintf(stderr,
#else
void TRACE_EAT(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_end(ap);
}
#define TRACE TRACE_EAT(
#endif

// local functions
static long oGetTime(VCNodePtr pvc);
static int oTrackProgram(VCNodePtr pvc);
static void oMoveToTarget(VCNodePtr pvc);
static void oMotorGo(VCNodePtr pvc);
static void oResetEdgeLatch(VCNodePtr pvc, char edgeBits);
static char *oReadcmd(char *str);
static long oGetParm(char *str, int num);
static int oIsCmd(char *str, char *cmd);

// background functions
static void domeseeker(int node);
static void roofseeker(int node);

// local variables
static int everBeenInit = 0; // a sanity "been init at least once" flag
static VCNode vmcNode[NVNODES];
typedef void (*ActFunc)(int);
static ActFunc active_func[NVNODES];

// Main service loop.  This is called at each iteration of tel_poll
// If we are tracking, vmcTrackProgram is executed to keep target current
// vmcMoveToTarget handles acceleration
// vmcGo moves us there
void vmcService(int node)
{
    VCNodePtr pvc;

    //	static int cnt = 0;
    //	TRACE "%d\n",cnt++);

    if (!everBeenInit)
        return;
    pvc = &vmcNode[node];

    //		TRACE "vmcService %d. Tracking = %d\n",node,pvc->tracking);

    if (pvc->tracking)
    {
        oTrackProgram(pvc);
    }
    if (pvc->targetSet)
    {
        oMoveToTarget(pvc);
    }

    oMotorGo(pvc);

    // run our background (script) process if we've set one up.
    if (active_func[node])
        (*active_func[node])(node);
}

// Stuff we get called upon to do

// Reset a node
void vmcReset(int node)
{
    VCNodePtr pvc = &vmcNode[node];

    TRACE "vmcReset %d\n",node);

    vmcResetClock(node);
    vmcSetTrackingOffset(node, 0);
    vmcSetTimeout(node, 0);
    vmcSetTargetPosition(node, 0);
    vmcStop(node);
    oResetEdgeLatch(pvc, HOMEBIT | PLIMBIT | NLIMBIT);
}

// Set up a node with new values
// return 1 if we must home after this, or 0 if homing is unaffected by change
int vmcSetup(int node, double maxvelr, double maxaccr, long steps, int sign)
{
    VCNodePtr pvc = &vmcNode[node];
    double scale = 0.5 + steps / (2 * PI);
    int musthome = 0;

    TRACE "vmcSetup %d: %g %g %ld %d\n",node, maxvelr, maxaccr, steps, sign);

    // reset any previous allocation

    if (everBeenInit)
    {
        if (pvc->trackPath)
            free(pvc->trackPath);
        pvc->trackPath = NULL;
        pvc->numTrackPts = 0;

        if (steps != pvc->countsPerRev || sign != pvc->sign)
        {
            musthome = 1;
        }
    }
    else
    {
        // trash everything to zero just in case it's not already
        memset(vmcNode, 0, sizeof(vmcNode));
        everBeenInit = 1;
        musthome = 1;
    }

    pvc->maxVel = maxvelr * scale;
    //	pvc->maxAcc = maxaccr*scale;
    pvc->countsPerRev = steps;
    pvc->sign = sign;

                                                  TRACE "   vmcSetup results: maxVel = %d counts/rev = %ld sign = %d musthome=%d\n",pvc->maxVel,pvc->countsPerRev, pvc->sign, musthome);

                                                  return musthome;
}

// Set a target encoder position that we will move toward at current speed
void vmcSetTargetPosition(int node, long position)
{
    VCNodePtr pvc = &vmcNode[node];

    vmcStop(node);

    if (node == 2)
        TRACE "vmcSetTargetPosition %d\n",node);

    while (abs(position) > pvc->countsPerRev)
    {
        if (position < 0)
        {
            position += pvc->countsPerRev;
        }
        else
        {
            position -= pvc->countsPerRev;
        }
    }

    pvc->targetPos = position;
    pvc->targetSet = 1;
    pvc->velocity = 0;
}

// Get the current speed
int vmcGetVelocity(int node)
{
    VCNodePtr pvc = &vmcNode[node];

    TRACE "vmcGetVelocity %d\n",node);

    return pvc->velocity;
}

// Get the current position
long vmcGetPosition(int node)
{
    VCNodePtr pvc = &vmcNode[node];

    oMotorGo(pvc);

    //	TRACE "vmcGetPosition %d = %ld\n",node, pvc->currentPos);

    return pvc->currentPos;
}

// Reset the clock for this node
void vmcResetClock(int node)
{
    VCNodePtr pvc = &vmcNode[node];

    TRACE "vmcResetClock %d\n",node);

    ftime(&pvc->timeRef);
}

// return milliseconds elapsed since last reset for this node
long vmcGetClock(int node)
{
    VCNodePtr pvc = &vmcNode[node];

    //	TRACE "vmcGetClock %d\n",node);

    return oGetTime(pvc);
}

static long oGetTime(VCNodePtr pvc)
{
    struct timeb now;
    int dsec;
    int dms;

    ftime(&now);

    dsec = now.time - pvc->timeRef.time;
    dms = now.millitm - pvc->timeRef.millitm;

    return dsec * 1000 + dms;
}

// Set the timeout value
void vmcSetTimeout(int node, int timeout)
{
    VCNodePtr pvc = &vmcNode[node];

    TRACE "vmcSetTimeout %d = %d\n",node,timeout);

    pvc->timeout = timeout;
}

// Set the tracking offset value
void vmcSetTrackingOffset(int node, int offset)
{
    VCNodePtr pvc = &vmcNode[node];

    TRACE "vmcSetTrackingOffset %d %d\n",node, offset);

    pvc->toffset = offset;
    if (pvc->tracking)
        oTrackProgram(pvc);
}

// Jog the motor a set amount
void vmcJog(int node, int amt)
{
    VCNodePtr pvc = &vmcNode[node];

    TRACE "vmcJog %d %d\n", node, amt);

    pvc->velocity = amt;

    pvc->tracking = 0; // definitely not tracking now
    pvc->targetSet = 0;
}

// Stop
void vmcStop(int node)
{
    VCNodePtr pvc = &vmcNode[node];

    //	TRACE "vmcStop %d\n",node);

    pvc->velocity = 0;
    pvc->targetPos = pvc->currentPos;
    pvc->tracking = 0;
    pvc->targetSet = 0;
}

// Accept a list of idealized encoder positions for tracking
// path is represented in encoder radians -- must convert to encoder positions
// return 0 for success
int vmcSetTrackPath(int node, int num, int startMs, int ivalMs, double *path)
{
    int i;
    double scale;

    VCNodePtr pvc = &vmcNode[node];

    TRACE "vmcSetTrackPath %d, %d items at %d, %d apart\n",node,num,startMs,ivalMs);

    pvc->lastPos = pvc->targetPos = pvc->currentPos;
    pvc->velocity = 0;
    pvc->lastTime = oGetTime(pvc);

    if (pvc->trackPath)
        free(pvc->trackPath); // free previous
    pvc->trackPath = malloc(num * sizeof(double));
    if (!pvc->trackPath)
        return -1;

    scale = 0.5 + pvc->countsPerRev / (2 * PI);
    //	TRACE "positions (scale = %g):\n",scale);
    for (i = 0; i < num; i++)
    {
        pvc->trackPath[i] = path[i] * scale + 0.5;
        //		TRACE "%d = %g => %g\n",i,path[i],pvc->trackPath[i]);
    }
    pvc->trackIval = ivalMs;
    pvc->trackStart = startMs;
    pvc->numTrackPts = num;

    pvc->tracking = 1;
    pvc->targetSet = 1;

    pvc->targetPos = pvc->trackPath[0];

    return 0;
}

//
// "Home" the controller
//
void vmcSetHome(int node)
{
    VCNodePtr pvc = &vmcNode[node];
    vmcStop(node);
    pvc->currentPos = pvc->lastPos = pvc->targetPos = pvc->homePos;
}

////////////////////////////////
//
// Internal functions
//
////////////////////////////////

// Track according to path set
// return 0 for success
static int oTrackProgram(VCNodePtr pvc)
{
    long now, span;
    double p1, p2, rat;
    int i;

    //	TRACE "oTrackProgram\n");

    // Find where we're at in the list
    now = oGetTime(pvc);
    now += 100; // add some time for interpolation

    span = now - pvc->trackStart;
    i = span / pvc->trackIval;
    if (i >= pvc->numTrackPts - 1)
    {
        // we should have been refreshed by now...
        // we're pretty much screwed.  Drop out of tracking.
        TRACE "Out of track points... aborting tracking\n");
        pvc->tracking = 0;
        pvc->targetSet = 0;
        pvc->velocity = 0;
        return -1;
    }

    // we need two points to interpolate from
    p1 = pvc->trackPath[i];
    p2 = pvc->trackPath[i + 1];

    // how far past the ideal time for p1 are we?

    span = now - pvc->trackIval * i;

    // and what ratio is that of our interval?
    rat = (double)span / (double)pvc->trackIval;

    // so what is the relative ratio of the difference in position?
    // add this to p1 to get where we should be now
    p1 += (p2 - p1) * rat;

    // okay -- let's move there.
    pvc->targetPos = (long)(p1 + 0.5);
    pvc->targetPos += pvc->toffset; // apply a jog offset if one in effect
    pvc->targetSet = 1;

    //	TRACE "Track point %d, pos = %ld\n",i,pvc->targetPos);

    return 0;
}

// Move according to velocity
// Check limit and home switches
static void oMotorGo(VCNodePtr pvc)
{
    // Move according to current velocity and the increment of real time
    int wantVel;
    long now = oGetTime(pvc);
    long ival = now - pvc->lastTime;
    long amt;

    wantVel = pvc->velocity;
    absclamp(pvc->velocity, pvc->maxVel);
    if (pvc->velocity != wantVel)
        pvc->clamped = 1;
    else
        pvc->clamped = 0;

    amt = (ival * pvc->velocity) / 1000;

    if (!amt && wantVel)
        return; // not enough time has elapsed to do anything

    pvc->currentPos += amt * pvc->sign;

    // check the motion past switches and latch the bits
    if (pvc->targetSet)
    {
        // if we've gone past target, latch to target and stop
        if ((pvc->lastPos <= pvc->targetPos && pvc->currentPos >= pvc->targetPos) ||
            (pvc->lastPos >= pvc->targetPos && pvc->currentPos <= pvc->targetPos))
        {
            pvc->currentPos = pvc->targetPos;
            pvc->targetSet = 0;
        }
    }

    if ((pvc->lastPos < pvc->homePos && pvc->currentPos >= pvc->homePos) ||
        (pvc->lastPos > pvc->homePos && pvc->currentPos <= pvc->homePos))
    {
        pvc->iedge |= HOMEBIT;
    }
    if (pvc->currentPos <= pvc->negLimit)
    {
        pvc->iedge |= NLIMBIT;
    }
    if (pvc->currentPos >= pvc->posLimit)
    {
        pvc->iedge |= PLIMBIT;
    }

    // if(pvc == &vmcNode[2])
    //	TRACE "oMotorGo: %ld --> %ld ==> %ld\n",pvc->lastPos,pvc->currentPos,pvc->targetPos);

    pvc->lastPos = pvc->currentPos;
    pvc->lastTime = now;
}

static void oResetEdgeLatch(VCNodePtr pvc, char edgeBits)
{
    pvc->iedge &= ~edgeBits;
}

// Move toward current target
static void oMoveToTarget(VCNodePtr pvc)
{
    double amt = pvc->sign * (pvc->targetPos - pvc->currentPos);

    if (abs(amt) > pvc->countsPerRev / 2)
    {
        if (amt > 0)
            amt = -(pvc->countsPerRev - amt);
        else
            amt = pvc->countsPerRev + amt;
    }

    absclamp(amt, pvc->maxVel);
    pvc->velocity = amt;

    if (pvc == &vmcNode[2])
        TRACE "oMoveToTarget: %ld ==> %ld  speed = %.2f\n",pvc->currentPos,pvc->targetPos,pvc->velocity);
}

// ---------------------------------------------------------------------------------
//
// Code that mimics the CSIMC interfaces... sort of
//
// ---------------------------------------------------------------------------------

char vmcResponse[NVNODES][256];

static char *oReadcmd(char *str)
{
    static char rtbuf[40];
    char *p = rtbuf;

    while (*str == '_' || isalnum(*str))
    {
        *p++ = *str++;
    }
    *p = 0;

    //	TRACE "oReadCmd %s\n",rtbuf);
    return rtbuf;
}

static long oGetParm(char *str, int num)
{
    int c;
    long val;

    while (*str && *str != '(')
    {
        str++;
    }
    str++;
    for (c = 0; c < num; c++)
    {
        while (*str && *str != ',')
        {
            str++;
        }
        str++;
    }
    val = atol(str);
    //	TRACE "oGetParm %d = (%ld) %s\n",num,val,str);
    return val;
}

static int oIsCmd(char *str, char *cmd)
{
    return (0 == strcmp(cmd, oReadcmd(str)));
}

// Write a command to the virtual controller
void vmc_w(int node, char *string)
{
    VCNodePtr pvc = &vmcNode[node];
    long val;

    if (oIsCmd(string, "dome_stop"))
    {
        vmcStop(node);
        return;
    }

    if (oIsCmd(string, "roofseek"))
    {
        pvc->miscVal[0] = oGetParm(string, 0);
        pvc->miscVal[1] = oGetTime(pvc);
        active_func[node] = roofseeker;
        vmcResponse[node][0] = 0;
        return;
    }

    if (oIsCmd(string, "finddomehome"))
    {
        vmcSetHome(node);
        sprintf(vmcResponse[node], "0: Dome homed\n");
        active_func[node] = NULL;
        return;
    }

    if (oIsCmd(string, "domeseek"))
    {
        pvc->miscVal[0] = oGetParm(string, 0);
        pvc->miscVal[1] = oGetParm(string, 1);
        pvc->miscVal[2] = pvc->currentPos;
        pvc->miscVal[3] = 0; // oGetTime(pvc);
        //		TRACE "domeseek(%ld, %ld)\n",pvc->miscVal[0],pvc->miscVal[1]);
        vmcSetTargetPosition(node, pvc->miscVal[0]);
        active_func[node] = domeseeker;
        vmcResponse[node][0] = 0;
        vmcService(node); // do first iteration
        return;
    }

    if (oIsCmd(string, "domejog"))
    {
        val = oGetParm(string, 0);
        vmcJog(node, val * pvc->maxVel);
        return;
    }

    TRACE "vmc_w (%d) : %s\n",node,string);
    sprintf(vmcResponse[node], "-1: Untrapped vmc command '%s' on node %d\n", string, node);
    active_func[node] = NULL;
}

int vmc_r(int node, char *buf, int size)
{
    strncpy(buf, vmcResponse[node], size);
    strcpy(vmcResponse[node], "");
    return (strlen(buf));
}

int vmc_isReady(int node)
{
    return (strlen(vmcResponse[node]) > 0);
}

long vmc_rix(int node, char *string)
{
    if (0 == strcmp(string, "roofestop();"))
    {
        return 0; // no emergency stop in effect
    }

    if (0 == strcmp(string, "=epos;"))
    {
        return vmcGetPosition(node);
    }

    if (0 == strcmp(string, "=mpos;"))
    {
        return vmcGetPosition(node);
    }

    TRACE "vmc_rix (%d) : %s\n",node,string);
    return (0);
}

//-----------------------------------
// fake script reiterators
//-----------------------------------

void domeseeker(int node)
{
    VCNodePtr pvc = &vmcNode[node];
    long val = pvc->miscVal[0];
    long tol = pvc->miscVal[1];
    long start = pvc->miscVal[2];
    long pct, a, b;

    a = mAbs(val - pvc->currentPos);
    if (a > pvc->countsPerRev / 2)
        a = pvc->countsPerRev - a;
    b = mAbs(val - start);
    if (b > pvc->countsPerRev / 2)
        b = pvc->countsPerRev - b;
    if (!b)
        pct = 100;
    else
        pct = 100 - ((a * 100) / b);

    if (pct < 100) // && a > tol)
    {
        if (oGetTime(pvc) - pvc->miscVal[3] > 2000)
        {
            sprintf(vmcResponse[node], "1 %ld%%", pct);
            pvc->miscVal[3] = oGetTime(pvc);
            TRACE "%s epos = %ld (a=%ld, b=%ld)\n",vmcResponse[node], pvc->currentPos, a, b);
        }
    }
    else
    {
        sprintf(vmcResponse[node], "0: Dome arrived at %ld (+/- %ld)\n", val, tol);
        //		TRACE "%s\n", vmcResponse[node]);
        active_func[node] = NULL;
    }
}

void roofseeker(int node)
{
    VCNodePtr pvc = &vmcNode[node];
    long dir = pvc->miscVal[0];
    long startTime = pvc->miscVal[1];
    long now = oGetTime(pvc);

    if (dir == 0 || now - startTime > 5000) // five seconds to open / close our fake door
    {
        if (dir == 0)
            sprintf(vmcResponse[node], "0: Roof stopped\n");
        if (dir > 0)
            sprintf(vmcResponse[node], "0: Roof open\n");
        if (dir < 0)
            sprintf(vmcResponse[node], "0: Roof closed\n");
        active_func[node] = NULL;
    }
    // no status messages along the way...
}
