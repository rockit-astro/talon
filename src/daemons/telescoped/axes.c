/* code to handle generic axis fucntions, such as homing and finding limits.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

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

#include "teled.h"

static void recordLimit (MotorInfo *mip, char dir);
static void recordStep (MotorInfo *mip, int motdiff, int encdiff);

// external access to read code for filter/focus for allowing read to work during limits
extern void readFilter();
extern void readFocus();


/* find home, direction as per POSSIDE.
 * return 1 if in-progress, 0 done, else -1.
 * status logged to fid.
 * STO 10-9-2002 -- updated shm mip info to include 'ishomed' flag
 * and am setting this accordingly here
 */
int
axis_home (MotorInfo *mip, FifoId fid, int first)
{
    if (virtual_mode) {
        vmcSetHome(mip->axis);
        mip->ishomed = 1;
        return 0;
    } else {
        static double mjdto[TEL_NM];
        int i = mip - &telstatshmp->minfo[0];
        int axis = (int)mip->axis;
        int cfd = MIPCFD(mip);
        char buf[1024];
        int n;

        /* sanity check */
        if (i < 0 || i >= TEL_NM) {
            tdlog ("Bug!! bad mip sent to axis_home: %d\n", i);
            die();
        }

        /* kindly oblige if not connected */
        if (!mip->have)
            return (1);

        if (first) {
            int posside = (mip->posside ? 1 : -1) * mip->sign;

            /* issue command */
            csi_w (cfd, "findhome(%d);", posside);

            /* public state */
            mip->homing = 1;
            mip->ishomed = 0;
            mip->cvel = mip->maxvel;
            mip->dpos = 0;

            /* estimate a timeout -- only clue is initial limit estimates */
            mjdto[i] = telstatshmp->now.n_mjd +
                       4.5*(mip->poslim-mip->neglim)/mip->maxvel/SPD;
            /* seeks at half speed */
        }

        /* check for timeout */
        if (telstatshmp->now.n_mjd > mjdto[i]) {
            csiStop (mip, 1);
            fifoWrite (fid, -1, "Axis %d timed out finding home", axis);
            return (-1);
        }

        /* check for motion errors */
        if (axisMotionCheck (mip, buf) < 0) {
            csiStop (mip, 1);
            mip->cvel = 0;
            mip->homing = 0;
            fifoWrite (fid, -2, "Axis %d homing motion error: %s ", axis, buf);
            return (-1);
        }

        /* things seem to be proceeding all right. get status.
         * stop if see < 0, done when see 0, else just report.
         */
        if (!csiIsReady(cfd))
            return (1);

        if (csi_r (cfd, buf, sizeof(buf)) <= 0)
            return(1);

        n = atoi(buf);
        if (n == 0 && buf[0] != '0') {
            /* consider no leading number a bug in the script */
            tdlog ("Bogus findhome() string: '%s'", buf);
            csiStop (mip, 1);
            mip->cvel = 0;
            mip->homing = 0;
            fifoWrite (fid, n, "Axis %d homing: %s", axis, buf);
            return (-1);
        }
        if (n < 0) {
            csiStop (mip, 1);
            mip->cvel = 0;
            mip->homing = 0;
            fifoWrite (fid, n, "Axis %d homing: %s", axis, buf+2);
            return (-1);
        }
        if (n == 0) {
            csiStop (mip, 0);
            mip->cvel = 0;
            mip->homing = 0;
            mip->ishomed = 1;
            return (0);
        }
        fifoWrite (fid, n, "Axis %d homing: %s", axis, buf+1);

        // STO: 10-01-2002 -- Update timeout on positive message
        mjdto[i] = telstatshmp->now.n_mjd +
        		4.5*(mip->poslim-mip->neglim)/mip->maxvel/SPD;
//ICE set 4.5 for double opteration instead of interger.
//4*(mip->poslim-mip->neglim)/mip->maxvel/SPD;
//

        return (1);
    }
}

//ICE
//#define NO_HOME_CHECK
//ICE
/* Check to see if we have homed this axis yet */
int axisHomedCheck(MotorInfo *mip, char msgbuf[])
{
//ICE
//#warning "@@@@@@@@@@@@@@@@@@@@@@@@@ONLY FOR TEST @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"	tel.c	/saft/daemons/telescoped.csi	line 215	C/C++ Problem
#ifdef NO_HOME_CHECK
	return 0;
#else
//#warning "@@@@@@@@@@@@@@@@@@@@@@@@@ONLY FOR TEST @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"	tel.c	/saft/daemons/telescoped.csi	line 215	C/C++ Problem

    if (mip->ishomed) {
        return 0;
    } else {
        sprintf (msgbuf, "Axis %d must be homed", mip->axis);
        return (-1);
    }
#endif
//ICE
}

/* check if the given axis is about to hit a limit.
 * return 0 if ok else write message in msgbuf[], stop and return -1.
 */
int
axisLimitCheck (MotorInfo *mip, char msgbuf[])
{
    if (virtual_mode) {
        // TODO: virtualize limits
        return 0;
    } else {
        double dt, nxtpos;

        /* check for this axis effectly not being used now */
        if (mip->cvel == 0)
            return (0);

        /* check for predicted hit @ cvel */
        dt = telstatshmp->dt/1000.0;
        nxtpos = mip->cpos + mip->cvel*dt;
        if (mip->cvel > 0 && nxtpos >= mip->poslim) {
            csiStop (mip, 1);
            sprintf (msgbuf, "Axis %d predicted to hit Pos limit", mip->axis);
            return (-1);
        }
        if (mip->cvel < 0 && nxtpos <= mip->neglim) {
            csiStop (mip, 1);
            sprintf (msgbuf, "Axis %d predicted to hit Neg limit", mip->axis);
            return (-1);
        }

        /* still ok */
        return (0);
    }
}

/* check for sane motion for the given motor.
 * return 0 if ok, put reason in msgbuf and return -1
 */
int
axisMotionCheck (MotorInfo *mip, char msgbuf[])
{
#if 0
    /* TODO */
    static double last_cpos[TEL_NM];	/* last mip->cpos */
    static double last_changed[TEL_NM];	/* last mjd it changed */
    int i = mip - telstatshmp->minfo;
    double now = telstatshmp->now.n_mjd;
    double v;
    int stuck;

    /* can't do anything without an encoder to cross-check */
    if (!mip->have || !mip->haveenc)
        return (0);

    /* sanity check mip */
    if (i < 0 || i >= TEL_NM) {
        tdlog ("Bug!! bad mip sent to axisMotionCheck: %d\n", i);
        die();
    }

    /* velocity */
    v = (mip->cpos - last_cpos[i])/(now - last_changed[i]);

    if (v > mip->cvel) {
        /* too fast! */
    }
    if (!v && mip->cvel) {
        /* stuck! */
    }

    /* record change */
    if (last_cpos[i] != mip->cpos) {
        last_changed[i] = now;
        last_cpos[i] = mip->cpos;
    }

    return (stuck ? -1 : 0);
#else
    return (0);
#endif
}

/* hunt for both axes' limits and record in home.cfg. if have an encoder, also
 * find and record new motor step scale.
 * return 1 if in-progress, 0 done, else -1.
 */
int
axis_limits (MotorInfo *mip, FifoId fid, int first)
{
    // make sure we're homed first
    if (first) {
        char buf[128];
        if (axisHomedCheck(mip, buf)) {
            fifoWrite(fid, -1, buf);
            return -1;
        }
    }


    if (virtual_mode) {
        // TODO: virtualize limits
        return 0;
    } else {
        static double mjdto[TEL_NM];	/* timeout */
        static char seeking[TEL_NM];	/* canonical dir we seek, '+'/'-' */
        static char found[TEL_NM];	/* last can dir we found, '+'/'-' */
        static int motbeg[TEL_NM];	/* motor at beginning of sweep */
        static int encbeg[TEL_NM];	/* encoder at beginning of sweep */
        int i = mip - telstatshmp->minfo;
        int axis = (int)mip->axis;
        int cfd = MIPCFD(mip);
        char buf[1024];
        int hwdir;
        int n;

        if (!mip->havelim) {
            fifoWrite(fid,0,"Axis %d: ok, but no limit switches are configured",
                      mip->axis);
            return (-1);
        }

        /* sanity check */
        if (i < 0 || i >= TEL_NM) {
            tdlog ("Bug!! bad mip sent to axis_limits: %d\n", i);
            die();
        }

        /* kindly oblige if not connected */
        if (!mip->have)
            return (0);

        if (first) {
            /* always start +, for no good reason.
             * N.B. for dec, it does looks better so it ends looking south
             */
            seeking[i] = '+';
            hwdir = mip->sign;
            found[i] = '\0';

            /* issue command */
            fifoWrite (fid, 1, "Axis %d: seeking %c limit", axis, seeking[i]);
            csi_w (cfd, "findlim(%d);", hwdir);

            /* for the eavesdroppers */
            mip->limiting = 1;

            /* estimate a timeout -- only clue is initial limit estimates */
            mjdto[i] = telstatshmp->now.n_mjd +
                       4.5*(mip->poslim-mip->neglim)/mip->maxvel/SPD;
            /* seeks at half speed */

        }

        /* check for timeout */
        if (telstatshmp->now.n_mjd > mjdto[i]) {
            csiStop(mip, 1);
            fifoWrite (fid, -1, "Axis %d timed out finding limits", axis);
            return (-1);
        }

        /* check for motion errors */
        if (axisMotionCheck (mip, buf) < 0) {
            csiStop(mip, 1);
            fifoWrite(fid, -2, "Axis %d motion error finding limits: %s", axis,
                      buf);
            return (-1);
        }

        /* things seem to be proceeding all right. get status if ready;
         * stop if see < 0, done when see 0, else just report.
         */
        if (!csiIsReady(cfd))
            return (1);
        if (csi_r (cfd, buf, sizeof(buf)) <= 0)
            return(1);

        n = atoi(buf);
        if (n == 0 && buf[0] != '0') {
            /* consider no leading number a bug in the script */
            tdlog ("Bogus findlim() string: '%s'", buf);
            csiStop (mip, 1);
            mip->cvel = 0;
            mip->homing = 0;
            fifoWrite (fid, n, "Axis %d: %s", axis, buf);
            return (-1);
        }
        if (n < 0) {
            csiStop (mip, 1);
            mip->cvel = 0;
            mip->homing = 0;
            fifoWrite (fid, n, "Axis %d: %s", axis, buf+2);	/* skip -n */
            return (-1);
        }
        if (n > 0) {
            fifoWrite (fid, n, "Axis %d: %s", axis, buf+1);	/* skip n */
            return (1);
        }

        // HACK TO ALLOW READING WITH LIMITS
        if (mip->enchome) {
            if (mip == &telstatshmp->minfo[TEL_IM]) {
                readFilter();
            }
            else if (mip == &telstatshmp->minfo[TEL_OM]) {
                readFocus();
            }
        }

        /* found a limit */
        if (!found[i]) {
            /* this is the first limit we have "run into" (sorry) */
            fifoWrite (fid, 2, "Axis %d: found %c limit", axis, seeking[i]);

            /* record */
            found[i] = seeking[i];
            recordLimit (mip, found[i]);
            if (mip->haveenc) {
                /* get motor and enc positions to find scale */
                motbeg[i] = csi_rix (cfd, "=mpos;");
                encbeg[i] = csi_rix (cfd, "=epos;");
            }

            /* turn around */
            if (seeking[i] == '+') {
                seeking[i] = '-';
                hwdir = -mip->sign;
            } else {
                seeking[i] = '+';
                hwdir = mip->sign;
            }

            /* issue command */
            fifoWrite (fid, 3, "Axis %d: seeking %c limit", axis, seeking[i]);
            csi_w (cfd, "findlim(%d);", hwdir);

            /* continue */
            return (1);

        } else if (found[i] != seeking[i]) {
            /* ran into a different limit than before. this is good */
            recordLimit (mip, seeking[i]);
            if (mip->haveenc) {
                /* get motor and enc again and compute/record scale */
                int motend, encend;
                motend = csi_rix (cfd, "=mpos;");
                encend = csi_rix (cfd, "=epos;");
                recordStep (mip, motend-motbeg[i], encend-encbeg[i]);
            }

            /* would like to back away but caller often issues stop and
             * rereads config file.
             */

            /* done */
            mip->cvel = 0;
            mip->limiting = 0;
            fifoWrite (fid, 0, "Axis %d: found %c limit", axis, seeking[i]);
            return (0);
        } else {
            /* appears we keep seeing same limit on */
            mip->cvel = 0;
            mip->limiting = 0;
            csiStop(mip, 1);
            fifoWrite (fid, -5, "Axis %d: %c limit appears stuck on", axis,
                       seeking[i]);
            return (-1);
        }
    }
}

/* we are currently at a limit -- record in config file */
static void
recordLimit (MotorInfo *mip, char dir)
{
    char name[64], valu[64];

    if (mip == &telstatshmp->minfo[TEL_HM])
        name[0] = 'H';
    else if (mip == &telstatshmp->minfo[TEL_DM])
        name[0] = 'D';
    else if (mip == &telstatshmp->minfo[TEL_RM])
        name[0] = 'R';
    else if (mip == &telstatshmp->minfo[TEL_OM])
        name[0] = 'O';
    else if (mip == &telstatshmp->minfo[TEL_IM])
        name[0] = 'I';
    else {
        /* who could it be?? */
        tdlog ("Bogus mip passed to recordLimit: %ld", (long)mip);
        return;
    }

    /* store new */
    if (dir == '+') {
        mip->poslim = mip->cpos;
        strcpy (name+1, "POSLIM");
        sprintf (valu, "%.6f", mip->poslim);
    } else {
        mip->neglim = mip->cpos;
        strcpy (name+1, "NEGLIM");
        sprintf (valu, "%.6f", mip->neglim);
    }

    if (writeCfgFile (hcfn, name, valu, NULL) < 0)
        tdlog ("%s: %s in recordLimit", hcfn, name);
}

/* compute and record a new motor step and sign */
static void
recordStep (MotorInfo *mip, int motdiff, int encdiff)
{
    char name[64], valu[64];

    if (!mip->haveenc)
        return;

    /* decide name */
    if (mip == &telstatshmp->minfo[TEL_HM])
        name[0] = 'H';
    else if (mip == &telstatshmp->minfo[TEL_DM])
        name[0] = 'D';
    else if (mip == &telstatshmp->minfo[TEL_IM])
        name[0] = 'I';
    else if (mip == &telstatshmp->minfo[TEL_OM])
        name[0] = 'O';
    else {
        tdlog ("Bogus mip passed to recordStep: %ld", (long)mip);
        return;
    }
    strcpy (name+1, "STEP");

    /* compute new steps around, load and install */
    mip->step = (int)floor(fabs((double)mip->estep*motdiff/encdiff) + 0.5);
    sprintf (valu, "%d", mip->step);
    if (writeCfgFile (hcfn, name, valu, NULL) < 0)
        tdlog ("%s: %s in recordStep", hcfn, name);

    strcpy (name+1, "SIGN");
    mip->sign = (double)motdiff*encdiff > 0 ? mip->esign : -mip->esign;
    sprintf (valu, "%d", mip->sign);
    if (writeCfgFile (hcfn, name, valu, NULL) < 0)
        tdlog ("%s: %s in recordStep", hcfn, name);

    /* set new maxvel from new step */
    csiSetup (mip);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: axes.c,v $ $Date: 2002/12/09 01:28:15 $ $Revision: 1.8 $ $Name:  $"};
