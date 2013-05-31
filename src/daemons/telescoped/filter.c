/* handle the Filter channel.
 * a generic wheel or tray driven by a stepper.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "strops.h"
#include "telstatshm.h"
#include "running.h"
#include "misc.h"
#include "telenv.h"
#include "csimc.h"
#include "virmc.h"
#include "cliserv.h"
#include "tts.h"

#include "teled.h"

#include "buildcfg.h"

#ifndef RIGEL_FILTER
#define RIGEL_FILTER 0
#endif

/* the current activity, if any */
static void (*active_func) (int first, ...);

/* one of these... */
static void filter_poll(void);
static void filter_reset(int first);
static void filter_home(int first, ...);
static void filter_limits(int first, ...);
static void filter_stop(int first, ...);
static void filter_set(int first, ...);
static void filter_status(void); //IEEC
static void filter_jog(int first, ...);

/* helped along by these... */
static void initCfg(void);
static void stopFilter(int fast);
void readFilter (void);
static void showFilter (void);
static int readFilInfo(void);

/* filled with filter/focus/temp info */
static FilterInfo *filinfo;
static int nfilinfo;

static int I1STEP;
static int IOFFSET;
static int deffilt;

// support for filter.cmc script
static int isUsingScript;  // config setting IUSESCRIPT
static int scriptTimeout;  // config setting ISCRIPTTO
static double script_to;   // mjd when timeout occurs

// support for external filter wheels (SBIG and FLI)
static int ISBIGFILTER = 0;
static int IFLIFILTER = 0;

#define EXTFILT   (ISBIGFILTER | IFLIFILTER)

// implementation calls.  Returns 1 if successful, 0 if not
int (*extFilt_reset_func)();
int (*extFilt_shutdown_func)();
int (*extFilt_home_func)();
int (*extFilt_select_func)(int filterNumber);

#define extFilt_reset (*extFilt_reset_func)
#define extFilt_shutdown (*extFilt_shutdown_func)
#define extFilt_home (*extFilt_home_func)
#define extFilt_select(f) (*extFilt_select_func)(f)

#ifdef USE_FLI
#include "fli_filter.c"
#endif

#ifdef USE_SBIG
#include "sbig_filter.c"
#endif


/* called when we receive a message from the Filter fifo.
 * if !msg just update things.
 */
/* ARGSUSED */
void
filter_msg (msg)
char *msg;
{
    char jog[10];

    /* do reset before checking for `have' to allow for new config file */
    if (msg && strncasecmp (msg, "reset", 5) == 0) {
        filter_reset(1);
        return;
    }

    if (!IMOT->have) {
        if (msg)
            fifoWrite (Filter_Id, 0, "Ok, but filter not really installed");
        return;
    }

    /* setup? */
    if (!virtual_mode && !EXTFILT) {
        if (!MIPCFD(IMOT)) {
            tdlog ("Filter command before initial Reset: %s", msg?msg:"(NULL)");
            return;
        }
    }

    if (!msg)
        filter_poll();
    else if (strncasecmp (msg, "home", 4) == 0)
        filter_home(1);
    else if (strncasecmp (msg, "stop", 4) == 0)
        filter_stop(1);
    else if (strncasecmp (msg, "limits", 6) == 0)
        filter_limits(1);
    else if (strncasecmp (msg, "status", 6) == 0) //IEEC
        filter_status();                         //IEEC
    else if (sscanf (msg, "j%1[0+-]", jog) == 1)
        filter_jog (1, jog[0]);
    else
        filter_set (1, msg);
}

/* Return number of filter by name (FILT0 R -> 0, FILT1 G -> 1, etc) */
/* (code modified from findFilter) */
int
findFilterNumber(char name)
{
    FilterInfo *fip, *lfip;
    int filtNum = 0;

    if (!filinfo && readFilInfo() < 0)
        die();

    if (name == '\0')
        return deffilt;

    if (islower(name))
        name = toupper(name);
    lfip = &filinfo[nfilinfo];
    for (fip = filinfo; fip < lfip; fip++) {
        char n = fip->name[0];
        if (islower(n))
            n = toupper(n);
        if (name == n)
            return filtNum;

        filtNum++;
    }

    return -1;
}


/* search the filter info for the given name.
 * if name == '\0' return the default filter.
 * return matching FilterInfo, or null if not in list, or die() if no list.
 */
FilterInfo *
findFilter (char name)
{
    FilterInfo *fip, *lfip;

    if (!filinfo && readFilInfo() < 0)
        die();

    if (name == '\0')
        return (&filinfo[deffilt]);

    if (islower(name))
        name = toupper(name);
    lfip = &filinfo[nfilinfo];
    for (fip = filinfo; fip < lfip; fip++) {
        char n = fip->name[0];
        if (islower(n))
            n = toupper(n);
        if (name == n)
            return (fip);
    }

    return (NULL);
}

/* no new messages.
 * goose the current objective, if any.
 */
static void
filter_poll()
{
    if (virtual_mode) {
        MotorInfo *mip = IMOT;
        vmcService(mip->axis);
    }
    if (active_func)
        (*active_func)(0);
}

/* stop and reread config files */
static void
filter_reset(int first)
{
    MotorInfo *mip = IMOT;

    if (mip->have) {
        if (!virtual_mode) {
            if (EXTFILT)
            {
                extFilt_shutdown();
            }
            else
            {
                csiiClose (mip);
            }
        }
    }

    initCfg();

    if (mip->have) {
        if (virtual_mode) {
            if (vmcSetup(mip->axis,mip->maxvel,mip->maxacc,mip->step,mip->sign)) {
                mip->ishomed = 0;
            }
            vmcReset(mip->axis);
        } else {
            if (EXTFILT)
            {
                if (extFilt_reset())
                {
                    fifoWrite(Filter_Id, 0, "Reset complete");
                }
                else
                {
                    fifoWrite(Filter_Id, -1, "** Reset Failed **");
                    telstatshmp->filter = '?';
                }
                return;
            }
            csiiOpen (mip);
            csiSetup(mip);
        }
        stopFilter(0);
        readFilter();
        showFilter();
        fifoWrite (Filter_Id, 0, "Reset complete");
    } else {
        fifoWrite (Filter_Id, 0, "Not installed");
    }
}

/* seek the home position */
static void
filter_home(int first, ...)
{
    MotorInfo *mip = IMOT;

    if (EXTFILT) {
        if (extFilt_home())
        {
            fifoWrite(Filter_Id, 1, "Homing complete. Now going to %s.",
                      filinfo[deffilt].name);
            mip->ishomed = 1;
            filter_set(1, filinfo[deffilt].name);
        }
        else
        {
            fifoWrite(Filter_Id, -1, "** Homing Failed **");
            mip->ishomed = 0;
            telstatshmp->filter = '?';
        }
        return;
    }

    if (first) {
        if (axis_home (mip, Filter_Id, 1) < 0) {
            stopFilter(1);
            return;
        }

        /* new state */
        active_func = filter_home;
    }

    switch (axis_home (mip, Filter_Id, 0)) {
    case -1:
        stopFilter(1);
        active_func = NULL;
        return;
    case  1:
        break;
    case  0:
        active_func = NULL;
        fifoWrite (Filter_Id, 1, "Homing complete. Now going to %s.",
                   filinfo[deffilt].name);
        toTTS ("The filter wheel has found home and is now going to the %s position.",
               filinfo[deffilt].name);
        filter_set (1, filinfo[deffilt].name);
        break;
    }
}

static void
filter_limits(int first, ...)
{
    MotorInfo *mip = IMOT;

    if (EXTFILT)
    {
        fifoWrite(Filter_Id, 0, "Limits found");
        mip->ishomed = 1;
        mip->limiting = mip->enchome = 0;
        return;
    }

    if (first) {
        mip->enchome = 1; // Hack flag that we are limiting filter
        if (axis_limits (mip, Filter_Id, 1) < 0) {
            stopFilter(1);
            active_func = NULL;
            return;
        }

        /* new state */
        active_func = filter_limits;
    }

    switch (axis_limits (mip, Filter_Id, 0)) {
    case -1:
        stopFilter(1);
        active_func = NULL;
        mip->limiting = mip->enchome = 0;
        return;
    case  1:
        break;
    case  0:
        stopFilter(0);
        active_func = NULL;
        initCfg();		/* read new limits */
        fifoWrite (Filter_Id, 0, "Limits found");
        toTTS ("The filter wheel has found both limit positions.");
        mip->ishomed = 1; // we really are homed... initCfg mucks us up.
        mip->limiting = mip->enchome = 0;
        break;
    }
}

static void filter_status(void)
{
    /* IEEC function to return filter status through fifos */

    MotorInfo *mip = IMOT;
   
    /* No status provided for virtual_mode */
    if (!virtual_mode)
    { 
        if(mip->cvel)
        {
           if(mip->ishomed)
                fifoWrite(Filter_Id, 0, "Filter is moving to requested position");
            else
                fifoWrite(Filter_Id, 1, "Filter is moving to unknown position");
        }
        else
        {
            if(mip->ishomed)
                fifoWrite(Filter_Id, 2, "Filter is placed at %c",telstatshmp->filter);
            else
                fifoWrite(Filter_Id, 3, "Filter is placed at unknown position");
        }
    }
    return;
}

static void
filter_stop(int first, ...)
{
    MotorInfo *mip = IMOT;
    int cfd = MIPCFD(mip);

    if (EXTFILT) {
        fifoWrite(Filter_Id, 0, "Stop complete");
        return;
    }

    /* stay current */
    readFilter();

    if (first) {
        /* issue stop */
        stopFilter(0);
        active_func = filter_stop;
    }

    /* wait for really stopped */
    if (virtual_mode) {
        if (vmcGetVelocity(mip->axis) != 0) return;
    } else {
        if (csi_rix (cfd, "=mvel;") != 0) return;
    }

    /* if get here, it has really stopped */
    active_func = NULL;
    readFilter();
    showFilter();
    fifoWrite (Filter_Id, 0, "Stop complete");
}

/* handle setting a given filter position */
static void
filter_set(int first, ...)
{
    static int rawgoal;
    MotorInfo *mip = IMOT;
    int cfd = MIPCFD(mip);
    char buf[1024];
    int n;
    Now *np = &telstatshmp->now;
    static int rehoming;
    static int rehomed;

    /* maintain current info */
    //readFilter();

#ifndef USE_FLI
    if (first) {
#endif
        char newf;
        va_list ap;
        FilterInfo *fip;
        double goal;

#ifdef USE_FLI
        if(!virtual_mode || first)
        {
        /* fetch new filter name */
        va_start (ap, first);
        newf = *(va_arg (ap, char *));
        va_end (ap);
        }
#endif

        if (EXTFILT)
        {
            int filterNum = findFilterNumber(newf);
            if (extFilt_select(filterNum))
            {
                telstatshmp->filter = filinfo[filterNum].name[0];
                fifoWrite(Filter_Id, 0, "Filter in place");
            } else {
                telstatshmp->filter = '?';
                fifoWrite(Filter_Id, 0, "** Failed setting %s filter **", filinfo ? filterNum >=0 ? filinfo[filterNum].name : "" : "");
            }
            // Always turn autofocus on after changing filter
            telstatshmp->autofocus = 1;
            return;
        }

#ifndef USE_FLI
        /* fetch new filter name */
        va_start (ap, first);
        newf = *(va_arg (ap, char *));
        va_end (ap);
#else
    if(first)
    {
#endif

        // make sure we're homed to begin with
        if (axisHomedCheck(mip, buf)) {
            active_func = NULL;
            stopFilter(1);
            fifoWrite (Filter_Id, -1, "Filter error: %s", buf);
            toTTS ("Filter error: %s", buf);
            return;
        }

        rehoming = 0;
        rehomed = 0;

        /* always need filter info */
        if (!filinfo && readFilInfo() < 0)
            die();

        /* find name in list */
        fip = findFilter (newf);
        if (!fip) {
            fifoWrite (Filter_Id, -7, "No filter is named %c", newf);
            return;
        }
        /* See if this is the same as the current filter */
        readFilter();
        showFilter();

#if RIGEL_FILTER
        if (telstatshmp->filter == newf) {
            active_func = NULL;
            stopFilter(1);
            fifoWrite (Filter_Id, 0, "Filter is current");
            toTTS ("Using the current filter wheel position.");
            return; // already selected -- we're done!
        }
#endif
        /* compute raw goal */
        rawgoal = IOFFSET + (fip - filinfo)*I1STEP;

        /* compute canonical goal */
        if (mip->haveenc) {
            /* need to convert rawgoal to encoder steps! */
            rawgoal = rawgoal * (mip->esign*mip->estep) /
                      (mip->sign*mip->step);

            goal = (2*PI)*mip->esign*rawgoal/mip->estep;
        } else {
            goal = (2*PI)*mip->sign*rawgoal/mip->step;
        }
        if (mip->havelim) {

            /* Removed, because in the case where limit switch doubles as home switch, this won't work

            	   if (goal > mip->poslim) {
                            fifoWrite (Filter_Id, -1, "Hits positive limit");
                            return;
                        }
                        if (goal < mip->neglim) {
                            fifoWrite (Filter_Id, -2, "Hits negative limit");
                            return;
                        }
            */
        }

        // turn off autofocus if while we are changing filter
        telstatshmp->autofocus = 0;

        // Support registered index switch engineering change, July 2002
        if (!virtual_mode && isUsingScript) {
            script_to = mjd + scriptTimeout;
            active_func = filter_set;
            csi_w (cfd, "seekFilter(%d, %d);",rawgoal, I1STEP/8);
        }
        else {
            /* ok, go for the gold, er, goal */
            if (virtual_mode) {
                vmcSetTargetPosition(mip->axis,rawgoal);
            } else {
                if (mip->haveenc) {
                    csi_w (cfd, "etpos=%d;", rawgoal);
                } else {
                    csi_w (cfd, "mtpos=%d;", rawgoal);
                }
            }
            mip->cvel = mip->sign*mip->maxvel;
            mip->dpos = goal;
            active_func = filter_set;
            toTTS ("The filter wheel is rotating to the %s position.", fip->name);
        }
    }

    /* already checked within soft limits when set up move */

    if (!virtual_mode && isUsingScript) {
        /* check for timeout */
        if (mjd > script_to) {
            fifoWrite (Filter_Id, -5, "Filter Script timed out");
            toTTS ("Filter setting has timed out.");
            stopFilter(1);
            active_func = NULL;
            readFilter();
            showFilter();
            return;
        }

        /* check progress */
        if (!csiIsReady(cfd))
            return;
        if (csi_r (cfd, buf, sizeof(buf)) <= 0)
            return;
        if (!buf[0])
            return;
        n = atoi(buf);
        if (n == 0 && buf[0] != '0') {
            /* consider no leading number a bug in the script */
            tdlog ("Invalid 'seekFilter' return: '%s'", buf);
            n = -1;
        }
        if (n < 0) { // error
            active_func = NULL;
            stopFilter(1);
            fifoWrite (Filter_Id, n, "Filter error: %s", buf+2); /* skip -n */
            toTTS ("Filter error: %s", buf+2);
            readFilter();
            showFilter();
            return;
        }
        if (n > 0) { // progress messages
            if (n == 10) { // special: We are rehoming; reset timeout
                fifoWrite(Filter_Id, 99, "Filter is rehoming");
                rehoming = 1;
                if (!rehomed) {
                    script_to = mjd + scriptTimeout*4;  // additional timeout time for homing
                }
            }
            else {
                fifoWrite (Filter_Id, n, "%s", buf+2);
            }
            return;
        }

        // if we had forced a rehoming, our "success" is a successful home
        // so keep looking for the end of our filter set.
        if (rehoming) {
            rehoming = 0;
            rehomed = 1;
            return;
        }

        /* ok! */
        active_func = NULL;
        stopFilter(1);
        fifoWrite (Filter_Id, 0, "Filter in place");
        toTTS ("The filter wheel is in position.");
        readFilter();
        showFilter();
    }
    else {
        // non-script (original) version
        readFilter();

        /* done when step is correct */
        if (mip->raw == rawgoal) {
            active_func = NULL;
            stopFilter(1);
            fifoWrite (Filter_Id, 0, "Filter in place");
            toTTS ("The filter wheel is in position.");
            // Always turn autofocus on after changing filter
            telstatshmp->autofocus = 1;
        }
    }

}

/* jog the given direction */
static void
filter_jog(int first, ...)
{
    MotorInfo *mip = IMOT;

    /* maintain current info */
    readFilter();
    showFilter();

    if (first) {
        va_list ap;
        char jogcode;
        char c;
        int  n;

        /* fetch jog direction code */
        va_start (ap, first);
        //jogcode = va_arg(ap, char);
        jogcode = va_arg(ap, int); // char is promoted to int, so pass int...
        va_end (ap);

        /* ignore if not still */
        if (mip->cvel != 0)
            return;

        switch (jogcode) {
        case '-':
            /* advance to next whole pos, with wrap */
            c = telstatshmp->filter;
            n = findFilterNumber(c);
            if (n >=0)
            {
                n++;
                if (n >= nfilinfo) n = 0;
                fifoWrite(Filter_Id, 0, "Advancing to the %s filter", filinfo[n].name);
                filter_set (1, filinfo[n].name);
            }
            break;
        case '+':
            /* advance to prior whole pos, with wrap */
            c = telstatshmp->filter;
            n = findFilterNumber(c);
            if (n >=0)
            {
                n--;
                if (n < 0) n = nfilinfo-1;
                if (n>=0)
                {
                    fifoWrite(Filter_Id, 0, "Advancing to the %s filter", filinfo[n].name);
                    filter_set (1, filinfo[n].name);
                }
            }
            break;
        case '0':
            return;
        default:
            fifoWrite (Filter_Id, -3, "Unknown jog code: %c", jogcode);
            return;
        }
    }
}


static void
initCfg()
{
#define NICFG   (sizeof(icfg)/sizeof(icfg[0]))
#define NHCFG   (sizeof(hcfg)/sizeof(hcfg[0]))
#define NOPTCFG (sizeof(optcfg)/sizeof(optcfg[0]))

    static int IHAVE, IHASLIM, IAXIS;
    static int ISTEP, ISIGN, IPOSSIDE, IHOMELOW;
    static int IHAVEENC,IESIGN,IESTEP;
    static double IMAXVEL, IMAXACC, ISLIMACC;

    static CfgEntry icfg[] = {
        {"IAXIS",		CFG_INT, &IAXIS},
        {"IHAVE",		CFG_INT, &IHAVE},
        {"IHASLIM",		CFG_INT, &IHASLIM},
        {"IPOSSIDE",	CFG_INT, &IPOSSIDE},
        {"IHOMELOW",	CFG_INT, &IHOMELOW},
        {"ISTEP",		CFG_INT, &ISTEP},  // if encoder, == enc steps, else motor
        {"ISIGN",		CFG_INT, &ISIGN},  // if encoder == enc sign, else motor
        {"I1STEP",		CFG_INT, &I1STEP},
        {"IOFFSET",		CFG_INT, &IOFFSET},
        {"IMAXVEL",		CFG_DBL, &IMAXVEL},
        {"IMAXACC",		CFG_DBL, &IMAXACC},
        {"ISLIMACC",	CFG_DBL, &ISLIMACC},
    };

    static CfgEntry icfg2[] = {
        {"IHAVEENC",	CFG_INT,  &IHAVEENC},
    };

    static double IPOSLIM, INEGLIM;

    static CfgEntry hcfg[] = {
        {"IPOSLIM",		CFG_DBL, &IPOSLIM},
        {"INEGLIM",		CFG_DBL, &INEGLIM},
    };

    // if we have an encoder, read MOTOR step and sign from home.cfg
    static CfgEntry hcfg2[] = {
        {"ISTEP",	CFG_INT, &ISTEP},
        {"ISIGN",	CFG_INT, &ISIGN},
    };

    static CfgEntry optcfg[] = {
        {"IUSESCRIPT",	CFG_INT, &isUsingScript},
        {"ISCRIPTTO",	CFG_INT, &scriptTimeout},
        {"ISBIGFILTER",	CFG_INT, &ISBIGFILTER},
        {"IFLIFILTER",	CFG_INT, &IFLIFILTER},
    };

    MotorInfo *mip = IMOT;
    int n;
    int oldhomed = mip->ishomed;

    n = readCfgFile (1, icfn, icfg, NICFG);
    if (n != NICFG) {
        cfgFileError (icfn, n, (CfgPrFp)tdlog, icfg, NICFG);
        die();
    }
    n = readCfgFile (1, hcfn, hcfg, NHCFG);
    if (n != NHCFG) {
        cfgFileError (hcfn, n, (CfgPrFp)tdlog, hcfg, NHCFG);
        die();
    }

    // read optional items
    isUsingScript = 0;
    scriptTimeout = 120; // default
    if (!virtual_mode) { // NOTE: Virtual Mode ALWAYS sets isUsingScript FALSE and ignores EXTFILT settings
        readCfgFile (1, icfn, optcfg, NOPTCFG);
    }

    if (ISBIGFILTER && IFLIFILTER)
    {
        tdlog("You can't specify both ISBIGFILTER and IFLIFILTER to be non-zero in filter.cfg");
        die();
    }
#if USE_SBIG
    if (ISBIGFILTER) set_for_sbig();
#endif
#if USE_FLI
    if (IFLIFILTER) set_for_fli();
#endif

    // read the optional IHAVEENC keyword.
    // If set, then read the sign,step, and home values
    IHAVEENC = 0;
    readCfgFile(1, icfn, icfg2, 1);
    if (IHAVEENC) {
        // if we have an encoder, sign/step refer to the encoder
        IESIGN = ISIGN;
        IESTEP = ISTEP;
        // get the motor sign/step from home.cfg
        // on error, defaults will == encoder value
        // subsequent Find Limits operation will write correct values
        (void) readCfgFile(1,hcfn,hcfg2,sizeof(hcfg2)/sizeof(hcfg2[0]));
    }
    else {
        IESTEP = 0;
        IESIGN = 1;
    }

    memset ((void *)mip, 0, sizeof(*mip));
    mip->axis = IAXIS;

    mip->have = IHAVE;
	//ICE haveenc mode variable XTRACK at telescope.cfg
	mip->xtrack = 0;
	mip->haveenc = IHAVEENC;
	//ICE
	//ICEmip->haveenc = IHAVEENC;
    mip->enchome = 0;
    mip->havelim = IHASLIM;
    mip->posside = IPOSSIDE ? 1 : 0;
    mip->homelow = IHOMELOW ? 1 : 0;
    mip->step = ISTEP;
    mip->estep = IESTEP;

    if (abs(ISIGN) != 1) {
        tdlog ("ISIGN must be +-1\n");
        die();
    }
    mip->sign = ISIGN;

    if (abs(IESIGN) != 1) {
        tdlog ("IESIGN must be +-1\n");
        die();
    }
    mip->esign = IESIGN;

    mip->limmarg = 0;
    mip->maxvel = fabs(IMAXVEL);
    mip->maxacc = IMAXACC;
    mip->slimacc = ISLIMACC;
    mip->poslim = IPOSLIM;
    mip->neglim = INEGLIM;

    /* (re)read fresh filter info */
    if (readFilInfo() < 0)
        die();

    mip->ishomed = oldhomed;

#undef NICFG
#undef NHCFG
}

static void
stopFilter(int fast)
{
    MotorInfo *mip = IMOT;

    if (virtual_mode) {
        vmcStop(mip->axis);
    } else {
        csiStop (mip, fast);
    }
    mip->cvel = 0;
    mip->homing = 0;
    mip->limiting = 0;
    readFilter();
    showFilter();
}

/* read the raw value */
void
readFilter ()
{
    MotorInfo *mip = IMOT;
    if (!mip->have)
        return;

    if (virtual_mode) {
        mip->raw = vmc_rix (mip->axis, "=mpos;");
        mip->cpos = (2*PI) * mip->sign * mip->raw / mip->step;
    } else {

        if (mip->haveenc) {
            double draw;
            int    raw;

            /* just change by half-step if encoder changed by 1 */
            raw = csi_rix (MIPSFD(mip), "=epos;");
            draw = abs(raw - mip->raw)==1 ? (raw + mip->raw)/2.0 : raw;
            mip->raw = raw;
            mip->cpos = (2*PI) * mip->esign * draw / mip->estep;
        } else {
            mip->raw = csi_rix (MIPCFD(mip), "=mpos;");
            mip->cpos = (2*PI) * mip->sign * mip->raw / mip->step;
        }
    }
}

/* fill telescope->filter */
static void
showFilter()
{
    MotorInfo *mip = IMOT;

    /* always need filter info */
    if (!filinfo && readFilInfo() < 0)
        die();

    if (mip->cvel > 0) {
        telstatshmp->filter = '>';
    } else if (mip->cvel < 0) {
        telstatshmp->filter = '<';
    } else {
        int fs;
        int fm = 0;
#if RIGEL_FILTER
        fs = (mip->raw - IOFFSET)/I1STEP;
        if (isUsingScript) {
            fm = 0;
        } else {
            fm = (mip->raw - IOFFSET)%I1STEP;
        }
#else
        if (isUsingScript) {
            fs = csi_rix(MIPCFD(mip), "=curFilter();");
        } else {
            fs = (mip->raw - IOFFSET)/I1STEP;
            fm = (mip->raw - IOFFSET)%I1STEP;
        }
#endif
        if (fs < 0 || fs >= nfilinfo || fm != 0) {
            telstatshmp->filter = '?';
        } else {
            telstatshmp->filter = filinfo[fs].name[0];
        }
    }
}

/* read icfn and fill in filinfo.
 * return 0 if ok, else write to Filter_Id and -1.
 */
static int
readFilInfo()
{
    char errmsg[1024];

    nfilinfo = readFilterCfg (1, icfn, &filinfo, &deffilt, errmsg);
    if (nfilinfo <= 0) {
        if (nfilinfo < 0)
            fifoWrite (Filter_Id, -5, "%s", errmsg);
        else
            fifoWrite (Filter_Id, -6, "%s: no entries", basenm(icfn));
        return (-1);
    }
    return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: filter.c,v $ $Date: 2007/06/09 10:08:50 $ $Revision: 1.24 $ $Name:  $"};
