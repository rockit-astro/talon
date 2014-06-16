/* handle the Focus channel */

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
#include "csimc.h"
#include "virmc.h"
#include "telenv.h"
#include "cliserv.h"
#include "tts.h"
#include "focustemp.h"

#include "teled.h"

// now a configuration setting... so is MAXINTERP
double MINAFDT	= 2.0; /* minimum autofocus temp change to cause a move */

// config setting for enabling/disabling temperature based "autofocus"
int OUSETEMPFOC = 1; // default to on

// during setup, allow a reprieve if we don't have step set yet
static int noOffsetOnHome = 0;

/* the current activity, if any */
static void (*active_func) (int first, ...);

/* one of these... */
static void focus_poll(void);
static void focus_reset(int first);
static void focus_home(int first, ...);
static void focus_limits(int first, ...);
static void focus_stop(int first, ...);
static void focus_auto(int first, ...);
static void focus_offset(int first, ...);
static void focus_jog(int first, ...);
static void focus_status(void); //IEEC

/* helped along by these... */
static void initCfg(void);
static void stopFocus(int fast);
void readFocus (void);
static void autoFocus (void);
static double targetPosition (FilterInfo *fip, double newtemp);
static double focusTemp(void);

static double OJOGF;
static int OSHAREDNODE;

// moved from inside autofocus code to here
static char last_filter;	/* filter we last checked */
static double last_temp;	/* temp we last read */
static int last_rawgoal;
static int focusInPlace;

/* called when we receive a message from the Focus fifo.
 * if !msg just update things.
 */
/* ARGSUSED */
void
focus_msg (msg)
char *msg;
{
    char jog[10];

    /* do reset before checking for `have' to allow for new config file */
    if (msg && strncasecmp (msg, "reset", 5) == 0) {
        focus_reset(1);
        return;
    }

    if (!OMOT->have) {
        if (msg)
            fifoWrite (Focus_Id, 0, "Ok, but focuser not really installed");
        return;
    }

    /* setup? */
    if (!virtual_mode) {
        if (!MIPCFD(OMOT)) {
            tdlog ("Focus command before initial Reset: %s", msg?msg:"(NULL)");
            return;
        }
    }

    if (!msg)
        focus_poll();
    else if (strncasecmp (msg, "home", 4) == 0)
        focus_home(1);
    else if (strncasecmp (msg, "stop", 4) == 0)
        focus_stop(1);
    else if (strncasecmp (msg, "limits", 6) == 0)
        focus_limits(1);
    else if (strncasecmp (msg, "auto", 4) == 0)
        focus_auto(1);
    else if (strncasecmp (msg, "status", 6) == 0) //IEEC
        focus_status();                          //IEEC
    else if (sscanf (msg, "j%1[0+-]", jog) == 1)
        focus_jog (1, jog[0]);
    else
        focus_offset (1, atof(msg));
}

/* no new messages.
 * goose the current objective, if any.
 */
static void
focus_poll()
{
    if (virtual_mode) {
        MotorInfo *mip = OMOT;
        vmcService(mip->axis);
    }
    if (active_func)
        (*active_func)(0);
    else if (telstatshmp->autofocus)
        autoFocus();
    /* TODO: monitor while idle? */
}

/* stop and reread config files */
static void
focus_reset(int first)
{
    MotorInfo *mip = OMOT;
    int had = mip->have;

    initCfg();

    focusInPlace = 0;

    /* TODO: for some reason focus behaves badly if you just close/reopen.
     * N.B. "had" relies on telstatshmp being zeroed when telescoped starts.
     */
    if (mip->have) {
        if (virtual_mode) {
            if (vmcSetup(mip->axis,mip->maxvel,mip->maxacc,mip->step,mip->sign)) {
                mip->ishomed = 0;
            }
            vmcReset(mip->axis);
        } else {
            if (!had) csiiOpen (mip);

            // STO 2007-01-20
            // This is a concession to the implementation that places a dome on the
            // same CSIMC board as the focuser.  If this is done, we must defer
            // initialization until the dome code can handle it.
            if (!OSHAREDNODE)    csiSetup(mip);
        }
        if (!OSHAREDNODE)
        {
            stopFocus(0);
            readFocus ();
            fifoWrite (Focus_Id, 0, "Reset complete");
        }
        else
        {
            fifoWrite(Focus_Id, 0, "Reset deferred on Dome shared node");
        }
    } else {
        if (!virtual_mode) {
            if (had) csiiClose (mip);
        }
        fifoWrite (Focus_Id, 0, "Not installed");
    }
}

/* seek the home position */
static void
focus_home(int first, ...)
{
    MotorInfo *mip = OMOT;
    double ugoal, unow, newtemp;
    FilterInfo *fip;

    if (first) {
        stopFocus(0);
        if (axis_home (mip, Focus_Id, 1) < 0) {
            active_func = NULL;
            return;
        }

        /* new state */
        active_func = focus_home;
        toTTS ("The focus motor is seeking the home position.");
    }

    switch (axis_home (mip, Focus_Id, 0)) {
    case -1:
        stopFocus(1);
        active_func = NULL;
        return;
    case  1:
        break;
    case  0:
        active_func = NULL;
        fip = findFilter ('\0');
        newtemp = focusTemp();
        ugoal = targetPosition (fip, newtemp);

        if (noOffsetOnHome) {
            fifoWrite (Focus_Id,0,"Homing complete.");
        }
        else {
            fifoWrite (Focus_Id,1,"Homing complete. Now going to %.1fum",ugoal);
//sleep caca!!!!            sleep(2);
        }
        toTTS ("The focus motor has found home and is now going to the initial position.");
        readFocus();
        unow = mip->cpos*mip->step/(2*PI*mip->focscale);
        mip->cvel = 0;
        mip->homing = 0;
        if (!noOffsetOnHome) focus_offset (1, ugoal - unow);
        break;
    }
}

static void
focus_limits(int first, ...)
{
    MotorInfo *mip = OMOT;

    /* maintain cpos and raw */
    // readFocus();

    if (first) {
        mip->enchome = 1; // hack flag that we are limiting focus
        if (axis_limits (mip, Focus_Id, 1) < 0) {
            stopFocus(1);
            active_func = NULL;
            return;
        }

        /* new state */
        active_func = focus_limits;
        toTTS ("The focus motor is seeking both limit positions.");
    }

    switch (axis_limits (mip, Focus_Id, 0)) {
    case -1:
        stopFocus(1);
        active_func = NULL;
        mip->limiting = mip->enchome = 0;
        return;
    case  1:
        break;
    case  0:
        stopFocus(0);
        active_func = NULL;
        initCfg();		/* read new limits */
        fifoWrite (Focus_Id, 0, "Limits found");
        toTTS ("The focus motor has found both limit positions.");
        mip->limiting = mip->enchome = 0;
        mip->ishomed = 1; // we really are homed
        break;
    }
}

static void
focus_stop(int first, ...)
{
    MotorInfo *mip = OMOT;
    int cfd = MIPCFD(mip);

    /* stay current */
    readFocus();

    if (first) {
        /* issue stop */
        stopFocus(0);
        active_func = focus_stop;
    }

    /* wait for really stopped */
    if (virtual_mode) {
        if (vmcGetVelocity(mip->axis) != 0) return;
    } else {
        if (csi_rix (cfd, "=mvel;") != 0) return;
    }

    /* if get here, it has really stopped */
    active_func = NULL;
    readFocus();
    fifoWrite (Focus_Id, 0, "Stop complete");
}

static void
focus_auto(int first, ...)
{
    /* set mode and might as well get started right away */
    stopFocus(0);
    telstatshmp->autofocus = 1;
    focusInPlace = 0;
    autoFocus();

    /* if still on, report success */
    if (telstatshmp->autofocus)
        fifoWrite (Focus_Id, 0, "Auto-focus enabled");
}

static void
focus_status(void)
{
    /* IEEC function to provide the focus status through fifo calls */
    MotorInfo *mip = OMOT;
    double upos = 0, ugoal = 0;

    upos = (mip->cpos*mip->step) / (2*PI*mip->focscale);
    ugoal = (mip->dpos*mip->step) / (2*PI*mip->focscale);
    if(virtual_mode)
        fifoWrite(Focus_Id, 0, "Focus position is %g um ", upos);
    else
    {
        if (telstatshmp->autofocus && mip->ishomed)
        {
            fifoWrite (Focus_Id, 0, "Auto-focus enabled at position %g um ",upos);
        }
        else if(mip->cvel)
        {
            if(mip->ishomed)
                fifoWrite(Focus_Id, 0, "Focus moving from %g to %g um ", upos, ugoal);
            else
                fifoWrite (Focus_Id, 0, "Focus moving to unknown position ");
        }
        else
        {
            if(mip->ishomed)
                fifoWrite(Focus_Id, 0, "Focus stopped at %g um ", upos);
            else
                fifoWrite (Focus_Id, 0, "Focus stopped at unknown position ");
        }
    }
    return;
}

/* handle a relative focus move, in microns */
static void
focus_offset(int first, ...)
{
    static int rawgoal;
    MotorInfo *mip = OMOT;
    int cfd = MIPCFD(mip);


    /* maintain current info */
    readFocus();

    if (first) {
        va_list ap;
        double delta, goal;
        char buf[128];

        // make sure we're homed to begin with
        if (axisHomedCheck(mip, buf)) {
            active_func = NULL;
            stopFocus(0);
            fifoWrite (Focus_Id, -1, "Focus error: %s", buf);
            toTTS ("Focus error: %s", buf);
            return;
        }

        /* fetch offset, in microns, canonical direction */
        va_start (ap, first);
        delta = va_arg (ap, double);
        va_end (ap);

        /* compute goal, in rads from home; check against limits */
        goal = mip->cpos + (2*PI)*delta*mip->focscale/mip->step;
        if (goal > mip->poslim) {
            fifoWrite (Focus_Id, -1, "Move is beyond positive limit");
            active_func = NULL;
            return;
        }
        if (goal < mip->neglim) {
            fifoWrite (Focus_Id, -2, "Move is beyond negative limit");
            active_func = NULL;
            return;
        }

        /* ok, go for the gold, er, goal */
        if (virtual_mode) {
            rawgoal = (int)floor(mip->sign*mip->step*goal/(2*PI) + 0.5);
            vmcSetTargetPosition(mip->axis, rawgoal);
        } else {
            if (mip->haveenc) {
                rawgoal = (int)floor(mip->esign*mip->estep*goal/(2*PI) + 0.5);
                csi_w (cfd, "etpos=%d;", rawgoal);
            } else {
                rawgoal = (int)floor(mip->sign*mip->step*goal/(2*PI) + 0.5);
                csi_w (cfd, "mtpos=%d;", rawgoal);
            }
        }
        mip->cvel = mip->maxvel;
        mip->dpos = goal;
        active_func = focus_offset;
        telstatshmp->autofocus = 0;
    }

// ICE added for not to send csimc commands on virtual mode
    int isworking = 0;
    if (!virtual_mode) isworking = csi_rix(cfd,"=working;");
// ICE added for not to send csimc commands on virtual mode

    /* done when we reach goal */
    if ((mip->haveenc && abs(mip->raw-rawgoal) < 1 && isworking==0 )
            || (!mip->haveenc && mip->raw == rawgoal) ) {
        active_func = NULL;
        stopFocus(0);
        fifoWrite (Focus_Id, 0, "Focus offset complete");
        toTTS ("The focus motor is in position.");
    }
}

/* handle a joystick jog command */
static void
focus_jog(int first, ...)
{
    MotorInfo *mip = OMOT;
    int cfd = MIPCFD(mip);
    char buf[1024];

    /* maintain current info */
    readFocus();
    mip->dpos = mip->cpos;	/* just for looks */

    if (first) {
        va_list ap;
        char dircode;

        /* fetch offset, in microns, canonical direction */
        va_start (ap, first);
        //dircode = va_arg (ap, char);
        dircode = va_arg (ap, int); // char is promoted to int, so pass int...
        va_end (ap);

        /* certainly no auto any more */
        telstatshmp->autofocus = 0;

        /* crack the code */
        switch (dircode) {
        case '0':	/* stop */
            focus_stop (1);		/* gentle and reports accurately */
            return;

        case '+':	/* go canonical positive */
            if (mip->cpos >= mip->poslim) {
                fifoWrite (Focus_Id, -4, "At positive limit");
                return;
            }
            if (virtual_mode) {
                vmcJog(mip->axis,(long)(mip->sign*MAXVELStp(mip)*OJOGF));
            } else {
                csi_w (cfd, "mtvel=%.0f;", mip->sign*MAXVELStp(mip)*OJOGF);
            }
            mip->cvel = mip->maxvel*OJOGF;
            active_func = focus_jog;
            fifoWrite (Focus_Id, 1, "Paddle command in");
            break;

        case '-':	/* go canonical negative */
            if (mip->cpos <= mip->neglim) {
                fifoWrite (Focus_Id, -5, "At negative limit");
                return;
            }
            if (virtual_mode) {
                vmcJog(mip->axis,0 - (long) (mip->sign*MAXVELStp(mip)*OJOGF));
            } else {
                csi_w (cfd, "mtvel=%.0f;", -mip->sign*MAXVELStp(mip)*OJOGF);
            }
            mip->cvel = -mip->maxvel*OJOGF;
            active_func = focus_jog;
            fifoWrite (Focus_Id, 2, "Paddle command out");
            break;

        default:
            tdlog ("focus_jog(): bogus dircode: %c 0x%x", dircode, dircode);
            active_func = NULL;
            return;
        }
    }

    /* this is under user control -- about all we can do is watch for lim */
    if (axisLimitCheck (mip, buf) < 0) {
        stopFocus(1);
        active_func = NULL;
        fifoWrite (Focus_Id, -7, "%s", buf);
    }
}



static void
initCfg()
{
#define NOCFG   (sizeof(ocfg)/sizeof(ocfg[0]))
#define NHCFG   (sizeof(hcfg)/sizeof(hcfg[0]))

    static int OHAVE, OHASLIM, OAXIS;
    static int OSTEP, OSIGN, OPOSSIDE, OHOMELOW;
    static int OHAVEENC, OESIGN, OESTEP;
    //static int OSHAREDNODE;
    // defined above for direct access by init code
    static double OMAXVEL, OMAXACC, OSLIMACC, OSCALE;

    static CfgEntry ocfg[] = {
        {"OAXIS",		CFG_INT, &OAXIS},
        {"OHAVE",		CFG_INT, &OHAVE},
        {"OHASLIM",		CFG_INT, &OHASLIM},
        {"OPOSSIDE",	CFG_INT, &OPOSSIDE},
        {"OHOMELOW",	CFG_INT, &OHOMELOW},
        {"OSTEP",		CFG_INT, &OSTEP},   // if we have an encoder, this is encoder steps
        {"OSIGN",		CFG_INT, &OSIGN},   // if we have an encoder, this is encoder sign
        {"OMAXVEL",		CFG_DBL, &OMAXVEL},
        {"OMAXACC",		CFG_DBL, &OMAXACC},
        {"OSLIMACC",	CFG_DBL, &OSLIMACC},
        {"OSCALE",		CFG_DBL, &OSCALE},
        {"OJOGF",		CFG_DBL, &OJOGF},
    };
    static int maxInterp;
    static CfgEntry ocfg2[] = {
        {"MAXINTERP",	CFG_INT,  &maxInterp},
        {"MINAFDT",		CFG_DBL,  &MINAFDT},
    };

    static CfgEntry ocfg3[] = {
        {"OHAVEENC",	CFG_INT,  &OHAVEENC},
        {"OUSETEMPFOC", CFG_INT,  &OUSETEMPFOC},
    };

    static CfgEntry ocfg4[] = {
        {"OSHAREDNODE", CFG_INT, &OSHAREDNODE},
    };

    static double OPOSLIM, ONEGLIM;

    static CfgEntry hcfg[] = {
        {"OPOSLIM",		CFG_DBL, &OPOSLIM},
        {"ONEGLIM",		CFG_DBL, &ONEGLIM},
    };

    // if we have an encoder, read MOTOR step and sign from home.cfg
    static CfgEntry hcfg2[] = {
        {"OSTEP",	CFG_INT, &OSTEP},
        {"OSIGN",	CFG_INT, &OSIGN},
    };

    MotorInfo *mip = OMOT;
    int n;
    int oldhomed = mip->ishomed;

    n = readCfgFile (1, ocfn, ocfg, NOCFG);
    if (n != NOCFG) {
        cfgFileError (ocfn, n, (CfgPrFp)tdlog, ocfg, NOCFG);
        die();
    }

    n = readCfgFile (1, hcfn, hcfg, NHCFG);
    if (n != NHCFG) {
        cfgFileError (hcfn, n, (CfgPrFp)tdlog, hcfg, NHCFG);
        die();
    }

    // read the optional MAXINTERP and MINAFDT keywords
    maxInterp = 0; // zero is not allowed -- use this to test setting/validity
    readCfgFile (1, ocfn, ocfg2, sizeof(ocfg2)/sizeof(ocfg2[0]));
    if (maxInterp) focusPositionSetMaxInterp(maxInterp);

    // read the optional OHAVEENC keyword.
    // If set, then read the sign,step, and home values
    // Also read the autofocus disable option flag here
    OHAVEENC = 0;
    OUSETEMPFOC = 1; // default to true
    OESTEP = 0;
    OESIGN = 1;
    readCfgFile(1, ocfn, ocfg3, 2);
    if (OHAVEENC) {
        // if we have an encoder, sign/step refer to the encoder
        OESIGN = OSIGN;
        OESTEP = OSTEP;
        // get the motor sign/step from home.cfg
        // on error, defaults will == encoder value
        // subsequent Find Limits operation will write correct values
        n = readCfgFile(1,hcfn,hcfg2,sizeof(hcfg2)/sizeof(hcfg2[0]));
        if (n != sizeof(hcfg2)/sizeof(hcfg2[0])) {
            noOffsetOnHome = 1; // don't try to move to first position  if we don't have steps set yet
        }
    }

    // read the optional OSHAREDNODE keyword, meaning we share this csimc board with the dome control
    // Note that this is incompatible with OHAVEENC
    OSHAREDNODE = 0;
    readCfgFile(1, ocfn, ocfg4, 1);
    if (OSHAREDNODE && OHAVEENC)
    {
        fifoWrite(Focus_Id,-1,"Configuration error -- See Log");
        tdlog("OSHAREDNODE is not compatible with OHAVEENC\n");
        die();
    }

    memset ((void *)mip, 0, sizeof(*mip));

    mip->axis = OAXIS;
    mip->have = OHAVE;
    mip->haveenc = OHAVEENC;
    mip->enchome = 0;
    mip->estep = OESTEP;
    mip->havelim = OHASLIM;
    mip->posside = OPOSSIDE ? 1 : 0;
    mip->homelow = OHOMELOW ? 1 : 0;
    mip->step = OSTEP;

    if (abs(OSIGN) != 1) {
        tdlog ("OSIGN must be +-1\n");
        die();
    }
    mip->sign = OSIGN;

    if (abs(OESIGN) != 1) {
        tdlog ("OESIGN must be +-1\n");
        die();
    }
    mip->esign = OESIGN;

    mip->limmarg = 0;
    mip->maxvel = fabs(OMAXVEL);
    mip->maxacc = OMAXACC;
    mip->slimacc = OSLIMACC;
    mip->poslim = OPOSLIM;
    mip->neglim = ONEGLIM;

    mip->focscale = OSCALE;

    // Read in the focus position table
    focusPositionReadData();

    mip->ishomed = oldhomed;

#undef NOCFG
#undef NHCFG
}


static void
stopFocus(int fast)
{
    MotorInfo *mip = OMOT;

    if (virtual_mode) {
        vmcStop(mip->axis);
    } else {
        csiStop (mip, fast);
    }
    telstatshmp->autofocus = 0;
    OMOT->homing = 0;
    OMOT->limiting = 0;
    OMOT->cvel = 0;

    //STO: 20010523 Focus stop (red light) visual bug due to position mismatch on stop
    OMOT->dpos = OMOT->cpos;

    // STO: 2002-06-28
    // Reset the filter and temperature used for autofocus to force first autofocus to find position
    last_filter = 0;
    last_temp = 0;
}

/* read the raw value */
void
readFocus ()
{
    MotorInfo *mip = OMOT;

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
            mip->raw = csi_rix (MIPSFD(mip), "=mpos;");
            mip->cpos = (2*PI) * mip->sign * mip->raw / mip->step;
        }
    }
}

/* keep an eye on the focus and insure it tracks scan.filter (if scan.running,
 * else filter) and temperature. as to which temp to use, use highest defined
 * WxStats.auxt else now.n_temp.
 * if trouble, write to Focus_id and turn autofocus back off.
 * N.B. be graceful if not enough temp info and during filter changes.
 */
static void
autoFocus()
{
    MotorInfo *mip = OMOT;
    int cfd = MIPCFD(mip);
    FilterInfo *fip;
    double newtemp;
    double ugoal, goal;
    int rawgoal;
    char newfilter;
    char buf[128];

    // if we've disabled it, never mind
    if (!OUSETEMPFOC) return;

    /* if under way, just check for success or hard fail */
    if (mip->cvel) {
        readFocus();
		double target_error = (2*PI)/mip->step; //radians for 1 motor step
		if (!virtual_mode && mip->haveenc) target_error = (2*PI)/mip->estep; //radians 1 encoder step

        if (fabs(mip->cpos - mip->dpos) < target_error)
            mip->cvel = 0;
        else
            return;
    }

    // make sure we're homed to begin with
    if (axisHomedCheck(mip, buf)) {
        telstatshmp->autofocus = 0;
        fifoWrite (Focus_Id, -1, "Focus error: %s", buf);
        toTTS ("Focus error: %s", buf);
        return;
    }

    /* find expected filter */
    newfilter = telstatshmp->scan.starttm ? telstatshmp->scan.filter
                : telstatshmp->filter;

    if (!isalnum (newfilter))
    {
        if (newfilter)
            return;	/* turning? */
        else
            newfilter = '\0'; /* IEEC: No filter defined */
    }
    if (islower (newfilter))
        newfilter = toupper (newfilter);

    /* find the entry for this filter */
    fip = findFilter (newfilter);
    if (!fip) {
        fifoWrite (Focus_Id, -8, "Autofocus failed: no filter named %c",
                   newfilter);
        telstatshmp->autofocus = 0;
        return;
    }

    /* get focus temp */
    newtemp = focusTemp();

	if (focusInPlace == 0)
	{
		int epos = csi_rix(cfd,"=epos;");
		if (abs(last_rawgoal - epos) <= 1)
		{
			focusInPlace = 1;
	        fifoWrite (Focus_Id, 0, "Autofocus offset complete");
		}
	}

    /* nothing to do if same filter and about same temp again */
    if (newfilter == last_filter && fabs(newtemp-last_temp) <= MINAFDT)
    {
        return;
    }

    /* interpolate temperatures to find new focus position. */
    ugoal = targetPosition (fip, newtemp);

    /* file contains canonical microns, we want canonical rads */
    goal = ugoal * (2*PI)*mip->focscale/mip->step;

    /* clamp goals to within limits */
    if (goal > mip->poslim) {
        fifoWrite (Focus_Id, -3,
                   "Auto move hits positive limit for %s at %.1fC",
                   fip->name, newtemp);
        goal = mip->poslim;
    }
    if (goal < mip->neglim) {
        fifoWrite (Focus_Id, -4,
                   "Auto move hits negative limit for %s at %.1fC",
                   fip->name, newtemp);
        goal = mip->neglim;
    }

    /* go */
    if (virtual_mode) {
        rawgoal = mip->sign*(int)floor(mip->step*goal/(2*PI) + 0.5);
        vmcSetTargetPosition(mip->axis, rawgoal);
    } else {
        if (mip->haveenc) {
            rawgoal = (int)floor(mip->esign*mip->estep*goal/(2*PI) + 0.5);
            csi_w (cfd, "etpos=%d;", rawgoal);
        } else {
            rawgoal = (int)floor(mip->sign*mip->step*goal/(2*PI) + 0.5);
            csi_w (cfd, "mtpos=%d;", rawgoal);
        }

    }
    mip->cvel = mip->maxvel * (goal > mip->cpos ? 1 : -1);
    mip->dpos = goal;

    fifoWrite (Focus_Id, 4, "Auto moving to %.1fum for %s at %.1fC", ugoal,
               fip->name, newtemp);

    /* remember new goals */
    last_temp = newtemp;
    last_filter = newfilter;
    last_rawgoal = rawgoal;
    focusInPlace = 0;
}

/* given a temperature and a fip, find the interpolated position, in microns,
 * canonical direction.
 */
static double
targetPosition (FilterInfo *fip, double newtemp)
{
    double ugoal;

    /*
    if (fip->t1 != fip->t0)
        ugoal = (newtemp - fip->t0)*(fip->f1 - fip->f0)/(fip->t1 - fip->t0)
    							    + fip->f0;
    else
        ugoal = fip->f0;	// pick one
    */

    if (focusPositionFind(fip->name[0],newtemp,&ugoal) < 0) {
        ugoal = fip->f0;
    }

    return ugoal;

}

/* get the temp to use to set focus.
 * first aux sensor takes priority over ambient
 */
static double
focusTemp()
{
    double newtemp = telstatshmp->now.n_temp;
    WxStats *wxp = &telstatshmp->wxs;
    int i;

    for (i = MAUXTP; --i >= 0; ) {
        if (wxp->auxtmask & (1<<i)) {
            newtemp = wxp->auxt[i];
            break;
        }
    }

    return (newtemp);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: focus.c,v $ $Date: 2007/06/09 10:08:50 $ $Revision: 1.15 $ $Name:  $"};
