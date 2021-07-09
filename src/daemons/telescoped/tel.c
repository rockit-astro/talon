/* main dispatch and execution functions for the mount itself. */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "cliserv.h"
#include "configfile.h"
#include "csimc.h"
#include "misc.h"
#include "running.h"
#include "strops.h"
#include "telenv.h"
#include "telstatshm.h"
#include "tts.h"
#include "virmc.h"

#include "teled.h"

// ICE
#undef TELESCOPED_DEBUG
#if defined(TELESCOPED_DEBUG)
#define debug_printf printf
#define debug_fflush fflush
#else
#define debug_printf                                                                                                   \
    {                                                                                                                  \
    }
#define debug_fflush                                                                                                   \
    {                                                                                                                  \
    }
#endif /* NDEBUG */

int xtrack_mode;
static double xstrack; /* when current xtrack started */
static double xttrack;

static void tel_set_xdelta(double HA, double DEC);
// ICE

/* handy loop "for each motor" we deal with here */
#define FEM(p) for ((p) = HMOT; (p) <= RMOT; (p)++)
#define NMOT (TEL_RM - TEL_HM + 1)

#define VEL_MAX 32768

/* the current activity, if any */
static void (*active_func)(int first, ...);

/* one of these... */
static void tel_poll(void);
static void tel_reset(int first);
static void tel_home(int first, ...);
static void tel_limits(int first, ...);
static void tel_stow(int first, ...);
static void tel_radecep(int first, ...);
static void tel_radeceod(int first, ...);
static void tel_op(int first, ...);
static void tel_altaz(int first, ...);
static void tel_hadec(int first, ...);
static void tel_stop(int first, ...);
static void tel_jog(int first, char jog_dir[], int velocity);
static void offsetTracking(int first, double harcsecs, double darcsecs);
static void tel_cover(int first, ...);
static void tel_status(int first, ...); // IEEC
static void tel_park(int first, ...);   // RGW

/* helped along by these... */
static int dbformat(char *msg, Obj *op, double *drap, double *ddecp);
static void initCfg(void);
static void hd2xyr(double ha, double dec, double *xp, double *yp, double *rp);
static void readRaw(void);
static void mkCook(void);
static void dummyTarg(void);
static void stopTel(int fast);
static int onTarget(MotorInfo **mipp);
static int atTarget(void);
static int trackObj(Obj *op, int first);
static void findAxes(Now *np, Obj *op, double *xp, double *yp, double *rp);
static int chkLimits(int wrapok, double *xp, double *yp, double *rp);
static void jogTrack(int first, char dircode, int velocity);
static void jogSlew(int first, char dircode, int velocity);
static int checkAxes(void);
static char *sayWhere(double alt, double az);

/* RGW */
static void readStats(void);

/* config entries */
static double TRACKACC;  /* tracking accuracy, rads. 0 means 1 enc step*/
static double FGUIDEVEL; /* fine jogging motion rate, rads/sec */
static double CGUIDEVEL; /* coarse jogging motion rate, rads/sec */
static int TRACKINT;     /* tracking interval for each e/mtrack, secs */
static int XTRACKINT;    /* ICE tracking interval for each xtrack, secs */

#define PPTRACK 60 /* number of positions to e/mtrack */

/* offsets to apply to target object location, if any */
static double r_offset; /* delta ra to be added */
static double d_offset; /* delta dec to be added */

#define MAXJITTER 10.0 /* max clock vs host difference */
static double strack;  /* when current e/mtrack started */

/* called when we receive a message from the Tel fifo.
 * as well as regularly with !msg just to update things.
 */
/* ARGSUSED */
void tel_msg(msg) char *msg;
{
    double a, b, c;
    char jog_dir[8];
    int vel;
    Obj o;
    int jogargs = 0;

    /* dispatch -- stop by default */

    if (msg)
    {
        tdlog("KMI - Dispatch message %s", msg);
    }

    if (!msg)
        tel_poll();
    else if (strncasecmp(msg, "reset", 5) == 0)
        tel_reset(1);
    else if (strncasecmp(msg, "home", 4) == 0)
        tel_home(1, msg);
    else if (strncasecmp(msg, "limits", 6) == 0)
        tel_limits(1, msg);
    else if (strncasecmp(msg, "stow", 4) == 0)
        tel_stow(1, msg);
    else if (strncasecmp(msg, "park", 4) == 0)
        tel_park(1, msg);
    else if (strncasecmp(msg, "OpenCover", 9) == 0)
        tel_cover(1, "O");
    else if (strncasecmp(msg, "CloseCover", 10) == 0)
        tel_cover(1, "C");
    else if (strncasecmp(msg, "status", 6) == 0) // IEEC
        tel_status(1);                           // IEEC
    else if (sscanf(msg, "RA:%lf Dec:%lf Epoch:%lf", &a, &b, &c) == 3)
        tel_radecep(1, a, b, c);
    else if (sscanf(msg, "RA:%lf Dec:%lf", &a, &b) == 2)
        tel_radeceod(1, a, b);
    else if (dbformat(msg, &o, &a, &b) == 0)
        tel_op(1, &o, a, b);
    else if (sscanf(msg, "Alt:%lf Az:%lf", &a, &b) == 2)
        tel_altaz(1, a, b);
    else if (sscanf(msg, "HA:%lf Dec:%lf", &a, &b) == 2)
        tel_hadec(1, a, b);
    else if ((jogargs = sscanf(msg, "j%7[NSEWnsew0] %d", jog_dir, &vel)) == 2)
        tel_jog(1, jog_dir, vel); // Variable-velocity jog, KMI 8/19/05
    // Vel ranges from 0 to VEL_MAX, indicating some fraction of the
    // max velocity for a particular axis
    else if (jogargs == 1)
        tel_jog(1, jog_dir, VEL_MAX); // Slow/fast jog
    else if (sscanf(msg, "Offset %lf,%lf", &a, &b) == 2)
        offsetTracking(1, a, b);
    // ICE
    else if (sscanf(msg, "xdelta(%lf,%lf)", &a, &b) == 2)
    {
        debug_printf("HA = %lf DEC = %lf\n", a, b);
        debug_fflush(stdout);
        tel_set_xdelta(a, b);
    }
    // ICE
    else
        tel_stop(1);
}

/* no new messages.
 * goose the current objective, if any, else just update cooked position.
 */
static void tel_poll()
{
    if (virtual_mode)
    {
        MotorInfo *mip;
        FEM(mip)
        {
            if (mip->have)
                vmcService(mip->axis);
        }
    }
    if (active_func)
        (*active_func)(0);
    else
    {
        /* idle -- just update */
        readRaw();
        mkCook();
        dummyTarg();
    }
    /*	readStats();*/
}

/* stop and reread config files */
static void tel_reset(int first)
{
    MotorInfo *mip;

    FEM(mip)
    {
        if (virtual_mode)
        {
            vmcReset(mip->axis);
        }
        else
        {
            if (mip->have)
                csiiClose(mip);
        }
    }

    initCfg();
    init_cfg();

    FEM(mip)
    {
        if (virtual_mode)
        {
            if (vmcSetup(mip->axis, mip->maxvel, mip->maxacc, mip->step, mip->esign))
            {
                mip->ishomed = 0;
            }
        }
        else
        {
            if (mip->have)
            {
                csiiOpen(mip);
                csiSetup(mip);
            }
        }
    }

    stopTel(0);
    active_func = NULL;
    fifoWrite(Tel_Id, 0, "Reset complete");
}

/* seek telescope axis home positions.. all or as per HDR */
static void tel_home(int first, ...)
{
    static int want[NMOT];
    static int nwant;
    MotorInfo *mip;
    int i;

    /* maintain just cpos and raw */
    readRaw();

    if (first)
    {
        char *msg;

        /* get the whole command */
        va_list ap;
        va_start(ap, first);
        msg = va_arg(ap, char *);
        va_end(ap);

        /* start fresh */
        stopTel(0);
        memset((void *)want, 0, sizeof(want));

        /* find which axes to do, or all if none specified */
        nwant = 0;
        if (strchr(msg, 'H'))
        {
            nwant++;
            want[TEL_HM] = 1;
        }
        if (strchr(msg, 'D'))
        {
            nwant++;
            want[TEL_DM] = 1;
        }
        if (strchr(msg, 'R'))
        {
            nwant++;
            want[TEL_RM] = 1;
        }
        if (!nwant)
        {
            nwant = 3;
            want[TEL_HM] = want[TEL_DM] = want[TEL_RM] = 1;
        }

        FEM(mip)
        {
            i = mip - &telstatshmp->minfo[0];
            if (want[i])
            {
                switch (axis_home(mip, Tel_Id, 1))
                {
                case -1:
                    /* abort all axes if any fail */
                    stopTel(1);
                    active_func = NULL;
                    return;
                case 1:
                    continue;
                case 0:
                    want[i] = 0;
                    nwant--;
                    break;
                }
            }
        }

        /* if get here, set new state */
        active_func = tel_home;
        telstatshmp->telstate = TS_HOMING;
        telstatshmp->telstateidx++;
        toTTS("The telescope is seeking the home position.");
    }

    /* continue to seek home on each axis still not done */
    FEM(mip)
    {
        i = mip - &telstatshmp->minfo[0];
        if (want[i])
        {
            switch (axis_home(mip, Tel_Id, 0))
            {
            case -1:
                /* abort all axes if any fail */
                stopTel(1);
                active_func = NULL;
                return;
            case 1:
                continue;
            case 0:
                fifoWrite(Tel_Id, 1, "Axis %d: home complete", mip->axis);
                want[i] = 0;
                nwant--;
                break;
            }
        }
    }

    /* really done when none left */
    if (!nwant)
    {
        telstatshmp->telstate = TS_STOPPED;
        telstatshmp->telstateidx++;
        active_func = NULL;
        fifoWrite(Tel_Id, 0, "Scope homing complete");
        toTTS("The telescope has found the home position.");
    }
}

/* find limit positions and H/D motor steps and signs.
 * N.B. set tax->h*lim as soon as we know TEL_HM limits.
 */
static void tel_limits(int first, ...)
{
    static int want[NMOT];
    static int nwant;
    MotorInfo *mip;
    int i;

    /* stay up to date */
    readRaw();
    mkCook();

    if (first)
    {
        char *msg;

        /* get the whole command */
        va_list ap;
        va_start(ap, first);
        msg = va_arg(ap, char *);
        va_end(ap);

        /* start fresh */
        stopTel(0);
        memset((void *)want, 0, sizeof(want));

        /* find which axes to do, or all if none specified */
        nwant = 0;
        if (strchr(msg, 'H'))
        {
            nwant++;
            want[TEL_HM] = 1;
        }
        if (strchr(msg, 'D'))
        {
            nwant++;
            want[TEL_DM] = 1;
        }
        if (strchr(msg, 'R'))
        {
            nwant++;
            want[TEL_RM] = 1;
        }
        if (!nwant)
        {
            nwant = 3;
            want[TEL_HM] = want[TEL_DM] = want[TEL_RM] = 1;
        }

        FEM(mip)
        {
            i = mip - &telstatshmp->minfo[0];
            if (want[i])
            {
                switch (axis_limits(mip, Tel_Id, 1))
                {
                case -1:
                    /* abort all axes if any fail */
                    stopTel(1);
                    active_func = NULL;
                    return;
                case 1:
                    continue;
                case 0:
                    want[i] = 0;
                    nwant--;
                    break;
                }
            }
        }

        /* new state */
        active_func = tel_limits;
        telstatshmp->telstate = TS_LIMITING;
        telstatshmp->telstateidx++;
        toTTS("The telescope is seeking the limit positions.");
    }

    /* continue to seek limits on each axis still not done */
    FEM(mip)
    {
        i = mip - &telstatshmp->minfo[0];
        if (want[i])
        {
            switch (axis_limits(mip, Tel_Id, 0))
            {
            case -1:
                /* abort all axes if any fail */
                stopTel(1);
                active_func = NULL;
                return;
            case 1:
                continue;
            case 0:
                fifoWrite(Tel_Id, 2, "Axis %d: limits complete", mip->axis);
                mip->cvel = 0;
                want[i] = 0;
                nwant--;
                break;
            }
        }
    }

    /* really done when none left */
    if (!nwant)
    {
        //        char buf[128];

        stopTel(0);
        initCfg(); /* read new limits */
        active_func = NULL;
        fifoWrite(Tel_Id, 0, "All Scope limits are complete.");
        toTTS("The telescope has found the limit positions.");

        /* N.B. save TEL_HM limits in tax */
        telstatshmp->tax.hneglim = HMOT->neglim;
        telstatshmp->tax.hposlim = HMOT->poslim;

        // Move to the stow position
        mip->ishomed = 1; // we really are homed
                          //        fifoWrite (Tel_Id, 0, "Now moving to stow position");
                          //        allstop();
                          //        sprintf (buf, "Alt:%g Az:%g", STOWALT, STOWAZ);
                          //        tel_msg (buf);
    }
}

/* Place the telescope in STOW position
 */
static void tel_stow(int first, ...)
{
    char buf[128];

    fifoWrite(Tel_Id, 0, "Telescope stow underway");
    allstop();
    sprintf(buf, "Alt:%g Az:%g", STOWALT, STOWAZ);
    tel_msg(buf);
}

/* Place the telescope in PARK position
 */
static void tel_park(int first, ...)
{
    char buf[128];

    fifoWrite(Tel_Id, 0, "Telescope park underway");
    allstop();
    //	sprintf(buf, "Alt:%g Az:%g", STOWALT, STOWAZ);
    //	tel_msg(buf);
    csi_w(MIPCFD(HMOT), "xdel=0;");
    csi_w(MIPCFD(HMOT), "clock=0;");
    //	csi_w(MIPCFD(HMOT), "maxvel=s;");
    csi_w(MIPCFD(HMOT), "timeout=300000;");
    csi_w(MIPCFD(HMOT), "etpos=%d;", 0);
    csi_w(MIPCFD(DMOT), "xdel=0;");
    csi_w(MIPCFD(DMOT), "clock=0;");
    //	csi_w(MIPCFD(DMOT), "maxvel=s;");
    csi_w(MIPCFD(DMOT), "timeout=300000;");
    csi_w(MIPCFD(DMOT), "etpos=%d;", -10475000);
}

/* handle tracking an astrometric position */
static void tel_radecep(int first, ...)
{
    static Obj o;

    if (first)
    {
        Now *np = &telstatshmp->now;
        Obj newo, *op = &newo;
        double Mjd, ra, dec, ep;
        va_list ap;

        /* fetch values */
        va_start(ap, first);
        ra = va_arg(ap, double);
        dec = va_arg(ap, double);
        ep = va_arg(ap, double);
        va_end(ap);

        /* fill in op */
        memset((void *)op, 0, sizeof(*op));
        op->f_RA = ra;
        op->f_dec = dec;
        year_mjd(ep, &Mjd);
        op->f_epoch = Mjd;
        op->o_type = FIXED;
        strcpy(op->o_name, "<Anon>");

        /* this is the new target */
        o = *op;
        active_func = tel_radecep;
        telstatshmp->telstate = TS_HUNTING;
        telstatshmp->telstateidx++;
        telstatshmp->jogging_ison = 0;
        r_offset = d_offset = 0;

        obj_cir(np, op); /* just for sayWhere */
        toTTS("The telescope is slewing %s.", sayWhere(op->s_alt, op->s_az));
    }

    if (trackObj(&o, first) < 0)
        active_func = NULL;
}

/* handle tracking an apparent position */
static void tel_radeceod(int first, ...)
{
    static Obj o;

    if (first)
    {
        Now *np = &telstatshmp->now;
        Obj newo, *op = &newo;
        double ra, dec;
        va_list ap;

        /* fetch values */
        va_start(ap, first);
        ra = va_arg(ap, double);
        dec = va_arg(ap, double);
        va_end(ap);

        /* fill in op */
        ap_as(np, J2000, &ra, &dec);
        memset((void *)op, 0, sizeof(*op));
        op->f_RA = ra;
        op->f_dec = dec;
        op->f_epoch = J2000;
        op->o_type = FIXED;
        strcpy(op->o_name, "<Anon>");

        /* this is the new target */
        o = *op;
        active_func = tel_radeceod;
        telstatshmp->telstate = TS_HUNTING;
        telstatshmp->telstateidx++;
        telstatshmp->jogging_ison = 0;
        r_offset = d_offset = 0;

        obj_cir(np, op); /* just for sayWhere */
        toTTS("The telescope is slewing %s.", sayWhere(op->s_alt, op->s_az));
    }

    if (trackObj(&o, first) < 0)
        active_func = NULL;
}

/* handle tracking an object */
static void tel_op(int first, ...)
{
    static Obj o;

    if (first)
    {
        Now *np = &telstatshmp->now;
        Obj *op;
        va_list ap;

        va_start(ap, first);
        op = va_arg(ap, Obj *);
        r_offset = va_arg(ap, double);
        d_offset = va_arg(ap, double);
        va_end(ap);

        /* this is the new target */
        o = *op;
        active_func = tel_op;
        telstatshmp->telstate = TS_HUNTING;
        telstatshmp->telstateidx++;
        telstatshmp->jogging_ison = 0;

        obj_cir(np, op); /* just for sayWhere */
        toTTS("The telescope is slewing towards %s, %s.", op->o_name, sayWhere(op->s_alt, op->s_az));
    }

    if (trackObj(&o, first) < 0)
        active_func = NULL;
}

/* handle slewing to a horizon location */
static void tel_altaz(int first, ...)
{
    if (first)
    {
        Now *np = &telstatshmp->now;
        double pa, ra, lst, ha, dec;
        double alt, az;
        MotorInfo *mip;
        double x, y, r;
        va_list ap;

        /* gather target values */
        va_start(ap, first);
        alt = va_arg(ap, double);
        az = va_arg(ap, double);
        va_end(ap);

        /* find target axis positions once */
        aa_hadec(lat, alt, az, &ha, &dec);
        telstatshmp->jogging_ison = 0;
        r_offset = d_offset = 0;
        hd2xyr(ha, dec, &x, &y, &r);
        if (chkLimits(1, &x, &y, &r) < 0)
        {
            active_func = NULL;
            ;
            return; /* Tel_Id already informed */
        }

        /* set new state */
        telstatshmp->telstate = TS_SLEWING;
        telstatshmp->telstateidx++;
        active_func = tel_altaz;

        /* set new raw destination */
        HMOT->dpos = x;
        DMOT->dpos = y;
        RMOT->dpos = r;

        /* and new cooked destination just for prying eyes */
        telstatshmp->Dalt = alt;
        telstatshmp->Daz = az;
        telstatshmp->DAHA = ha;
        telstatshmp->DADec = dec;
        tel_hadec2PA(ha, dec, &telstatshmp->tax, lat, &pa);
        telstatshmp->DPA = pa;
        now_lst(np, &lst);
        ra = hrrad(lst) - ha;
        range(&ra, 2 * PI);
        telstatshmp->DARA = ra;
        ap_as(np, J2000, &ra, &dec);
        telstatshmp->DJ2kRA = ra;
        telstatshmp->DJ2kDec = dec;

        toTTS("The telescope is slewing towards the %s, at %.0f degrees altitude.", cardDirLName(az), raddeg(alt));

        /* issue move command to each axis */
        FEM(mip)
        {
            if (mip->have)
            {

                // make sure we're homed to begin with
                char buf[128];
                if (axisHomedCheck(mip, buf))
                {
                    active_func = NULL;
                    stopTel(0);
                    fifoWrite(Tel_Id, -1, "Error: %s", buf);
                    toTTS("Error: %s", buf);
                    return;
                }

                if (virtual_mode)
                {
                    vmcSetTargetPosition(mip->axis, mip->sign * mip->step * mip->dpos / (2 * PI));
                }
                else
                {
                    // ICE
                    csi_w(MIPCFD(mip), "xdel=0;");
                    csi_w(MIPCFD(mip), "clock=0;");
                    //					csi_w(MIPCFD(mip), "maxvel=s;");
                    csi_w(MIPCFD(mip), "timeout=300000;");
                    // ICE

                    if (mip->haveenc)
                    {
                        csi_w(MIPCFD(mip), "etpos=%.0f;", mip->esign * mip->estep * mip->dpos / (2 * PI));
                        // ICE
                        debug_printf("Slew to etpos=%.0f\n", mip->esign * mip->estep * mip->dpos / (2 * PI));
                        // ICE
                    }
                    else
                    {
                        csi_w(MIPCFD(mip), "mtpos=%.0f;", mip->sign * mip->step * mip->dpos / (2 * PI));
                        // ICE
                        debug_printf("Slew to mtpos=%.0f\n", mip->sign * mip->step * mip->dpos / (2 * PI));
                        // ICE
                    }
                }
            }
        }
    }

    /* stay up to date */
    readRaw();
    mkCook();

    if (checkAxes() < 0)
    {
        stopTel(1);
        active_func = NULL;
    }

    if (atTarget() == 0)
    {
        stopTel(0);
        fifoWrite(Tel_Id, 0, "Slew complete");
        toTTS("The telescope slew is complete");
        active_func = NULL;
    }
    // ICE
    debug_printf("AMOT=%d\tZMOT=%d\n", HMOT->raw, DMOT->raw);
    // ICE
}

/* handle slewing to an equatorial location */
static void tel_hadec(int first, ...)
{
    if (first)
    {
        Now *np = &telstatshmp->now;
        double alt, az, pa, ra, lst, ha, dec;
        MotorInfo *mip;
        double x, y, r;
        va_list ap;

        /* gather params */
        va_start(ap, first);
        ha = va_arg(ap, double);
        dec = va_arg(ap, double);
        va_end(ap);

        /* find target axis positions once */
        telstatshmp->jogging_ison = 0;
        r_offset = d_offset = 0;
        hd2xyr(ha, dec, &x, &y, &r);
        if (chkLimits(1, &x, &y, &r) < 0)
        {
            active_func = NULL;
            ;
            return; /* Tel_Id already informed */
        }

        /* set new state */
        telstatshmp->telstate = TS_SLEWING;
        telstatshmp->telstateidx++;
        active_func = tel_hadec;

        /* set raw destination */
        HMOT->dpos = x;
        DMOT->dpos = y;
        RMOT->dpos = r;

        /* and cooked desination, just for enquiring minds */
        telstatshmp->DAHA = ha;
        telstatshmp->DADec = dec;
        tel_hadec2PA(ha, dec, &telstatshmp->tax, lat, &pa);
        hadec_aa(lat, ha, dec, &alt, &az);
        telstatshmp->DPA = pa;
        telstatshmp->Dalt = alt;
        telstatshmp->Daz = az;
        now_lst(np, &lst);
        ra = hrrad(lst) - ha;
        range(&ra, 2 * PI);
        telstatshmp->DARA = ra;
        ap_as(np, J2000, &ra, &dec);
        telstatshmp->DJ2kRA = ra;
        telstatshmp->DJ2kDec = dec;

        toTTS("The telescope is slewing %s.", sayWhere(alt, az));

        /* issue move command to each axis */
        FEM(mip)
        {
            if (mip->have)
            {

                // make sure we're homed to begin with
                char buf[128];
                if (axisHomedCheck(mip, buf))
                {
                    active_func = NULL;
                    stopTel(0);
                    fifoWrite(Tel_Id, -1, "Error: %s", buf);
                    toTTS("Error: %s", buf);
                    return;
                }

                if (virtual_mode)
                {
                    vmcSetTargetPosition(mip->axis, mip->sign * mip->step * mip->dpos / (2 * PI));
                }
                else
                {
                    // ICE
                    csi_w(MIPCFD(mip), "xdel=0;");
                    csi_w(MIPCFD(mip), "clock=0;");
                    //				csi_w(MIPCFD(mip), "maxvel=s;");
                    csi_w(MIPCFD(mip), "timeout=300000;");
                    // ICE

                    if (mip->haveenc)
                    {
                        csi_w(MIPCFD(mip), "etpos=%.0f;", mip->esign * mip->estep * mip->dpos / (2 * PI));
                        // ICE
                        debug_printf("Slew to etpos=%.0f\n", mip->esign * mip->estep * mip->dpos / (2 * PI));
                        // ICE
                    }
                    else
                    {
                        csi_w(MIPCFD(mip), "mtpos=%.0f;", mip->sign * mip->step * mip->dpos / (2 * PI));
                        // ICE
                        debug_printf("Slew to mtpos=%.0f\n", mip->sign * mip->step * mip->dpos / (2 * PI));
                        // ICE
                    }
                }
            }
        }
    }

    /* stay up to date */
    readRaw();
    mkCook();

    if (checkAxes() < 0)
    {
        stopTel(1);
        active_func = NULL;
    }

    if (atTarget() == 0)
    {
        stopTel(0);
        fifoWrite(Tel_Id, 0, "Slew complete");
        toTTS("The telescope slew is complete");
        active_func = NULL;
    }
    // ICE
    debug_printf("HMOT=%d\tDMOT=%d\n", HMOT->raw, DMOT->raw);
    // ICE
}

/* politely stop all axes */
static void tel_stop(int first, ...)
{
    MotorInfo *mip;

    if (first)
    {
        /* issue stops */
        stopTel(0);
        active_func = tel_stop;
    }

    /* wait for all to be stopped */
    FEM(mip)
    {
        if (mip->have)
        {
            if (virtual_mode)
            {
                if (vmcGetVelocity(mip->axis) != 0)
                    return;
            }
            else
            {
                if (csi_rix(MIPCFD(mip), "=mvel;") != 0)
                    return;
            }
        }
    }

    /* if get here, everything has stopped */
    telstatshmp->telstate = TS_STOPPED;
    telstatshmp->telstateidx++;
    active_func = NULL;
    fifoWrite(Tel_Id, 0, "Stop complete");
    toTTS("The telescope is now stopped.");
    readRaw();
}

/* respond to a request for jogging.
 */
static void tel_jog(int first, char jog_dir[], int velocity)
{
    if (telstatshmp->telstate == TS_TRACKING)
        jogTrack(first, jog_dir[0], velocity);
    else
        jogSlew(first, jog_dir[0], velocity);
}

// Support for mirror cover
static void tel_cover(int first, ...)
{
    MotorInfo *mip = HMOT;
    int cfd = MIPCFD(mip);
    Now *np = &telstatshmp->now;
    char buf[1024];
    int n;
    // hard code our timeout at 30 seconds -- for the context
    // at hand that's plenty... some future slow opening cover
    // might need more.
    double script_to = 30 + mjd;

    if (virtual_mode)
        return;

    if (first)
    {
        char *msg;
        int cfd = MIPCFD(mip);

        /* get the whole command */
        va_list ap;
        va_start(ap, first);
        msg = va_arg(ap, char *);
        va_end(ap);

        active_func = tel_cover;
        switch (msg[0])
        {
        case 'O':
            csi_w(cfd, "cover(1);");
            break;
        case 'C':
            csi_w(cfd, "cover(0);");
            break;
        default:
            active_func = NULL;
            break;
        }
    }

    if (mjd > script_to)
    {
        fifoWrite(Tel_Id, -5, "Cover script has timed out");
        toTTS("Cover script has timed out.");
        active_func = NULL;
        return;
    }

    /* check progress */
    if (!csiIsReady(cfd))
        return;
    if (csi_r(cfd, buf, sizeof(buf)) <= 0)
        return;
    if (!buf[0])
        return;
    n = atoi(buf);
    if (n == 0 && buf[0] != '0')
    {
        /* consider no leading number a bug in the script */
        tdlog("Invalid 'cover' return: '%s'", buf);
        n = -1;
    }
    if (n < 0)
    { // error
        active_func = NULL;
        fifoWrite(Tel_Id, n, "Cover error: %s", buf + 2); /* skip -n */
        toTTS("Cover error: %s", buf + 2);
        return;
    }
    if (n > 0)
    { // progress messages
        fifoWrite(Tel_Id, n, "Cover %s", buf + 2);
        return;
    }

    /* ok! */
    active_func = NULL;
    fifoWrite(Tel_Id, 0, "Mirror cover command complete");
    toTTS("The mirror cover command is complete.");
}

static void tel_status(int first, ...)
{
    /* IEEC function to provide telescope status through fifo calls */
    int hstatus, dstatus;
    hstatus = dstatus = -1;

    readRaw();
    mkCook();
    if (virtual_mode)
    {
        fifoWrite(Tel_Id, 0, "Telescope equatorial position ( RA , Dec ): ( %g , %g )", telstatshmp->CJ2kRA,
                  telstatshmp->CJ2kDec);
        fifoWrite(Tel_Id, 0, "Telescope altazimutal position ( Az , Alt ):  (%g , %g )", telstatshmp->Caz,
                  telstatshmp->Calt);
    }
    else
    {
        if (HMOT->have)
            hstatus = csi_rix(MIPCFD(HMOT), "=isHomed();");
        if (DMOT->have)
            dstatus = csi_rix(MIPCFD(DMOT), "=isHomed();");

        if (hstatus == 1 && dstatus == 1)
        {
            fifoWrite(Tel_Id, 0, "Telescope equatorial position ( RA , Dec ): ( %g , %g )", telstatshmp->CJ2kRA,
                      telstatshmp->CJ2kDec);
            fifoWrite(Tel_Id, 0, "Telescope altazimutal position ( Az , Alt ): ( %g , %g )", telstatshmp->Caz,
                      telstatshmp->Calt);
        }
        else if (hstatus == 0 && dstatus == 0)
            fifoWrite(Tel_Id, 2, "Telescope position is completely unknown");
        else if (hstatus == 1 && dstatus == 0)
            fifoWrite(Tel_Id, 1, "Declination axis position is unknown");
        else if (hstatus == 0 && dstatus == 1)
            fifoWrite(Tel_Id, 1, "Hour angle axis position is unknown");
        else
            fifoWrite(Tel_Id, -1, "Error reading telescope position");
    }
    return;
}

/* aux support functions */

/* dig out a message with a db line in it.
 * may optionally be preceded by "dRA:x dDec:y #".
 * return 0 if ok, else -1.
 * N.B. always set dra/ddec.
 */
static int dbformat(char *msg, Obj *op, double *drap, double *ddecp)
{
    if (sscanf(msg, "dRA:%lf dDec:%lf #", drap, ddecp) == 2)
        msg = strchr(msg, '#') + 1;
    else
        *drap = *ddecp = 0.0;

    return (db_crack_line(msg, op, NULL));
}

/* build and load an e/mtrack sequence for op.
 * time starts at np. it is ok to modify np->n_mjd.
 * N.B. we assume clocks have been set to 0
 */
static void buildTrack(Now *np, Obj *op)
{
    double *x, *y, *r;
    double *xyr[NMOT];
    double mjd0;
    MotorInfo *mip;
    int i;

    /* malloc each then store so we can effectively access them via a mip */
    x = (double *)malloc(PPTRACK * sizeof(double));
    y = (double *)malloc(PPTRACK * sizeof(double));
    r = (double *)malloc(PPTRACK * sizeof(double));
    xyr[TEL_HM] = x;
    xyr[TEL_DM] = y;
    xyr[TEL_RM] = r;

    /* build list of PPTRACK values beginning at mjd */
    mjd0 = mjd;
    for (i = 0; i < PPTRACK; i++)
    {
        mjd = mjd0 + i * TRACKINT / (PPTRACK * SPD);
        findAxes(np, op, &x[i], &y[i], &r[i]);
        (void)chkLimits(1, &x[i], &y[i], &r[i]); /* let limit protect */
    }

    /* send to each controller */
    FEM(mip)
    {
        double scale;
        double *xyrp;
        int cfd;

        if (!mip->have || mip->xtrack)
            continue;

        if (virtual_mode)
        {

            xyrp = xyr[mip - telstatshmp->minfo];
            //	    tdlog ("Creating track profile:");
            vmcSetTrackPath(mip->axis, PPTRACK, 0, 1000.0 * TRACKINT / PPTRACK + 0.5, xyrp);
        }
        else
        {

            xyrp = xyr[mip - telstatshmp->minfo];
            cfd = MIPCFD(mip);
            //	    tdlog ("Creating track profile:");
            if (mip->haveenc)
            {
                scale = mip->esign * mip->estep / (2 * PI);
                csi_w(cfd, "etrack");
                debug_printf("etrack");
            }
            else
            {
                scale = mip->sign * mip->step / (2 * PI);
                csi_w(cfd, "mtrack");
                debug_printf("mtrack");
            }
            csi_w(cfd, "(0,%.0f", 1000. * TRACKINT / PPTRACK + .5);
            debug_printf("(0,%.0f", 1000. * TRACKINT / PPTRACK + .5);

            /* TODO: pack into longer commands */
            for (i = 0; i < PPTRACK; i++)
            {
                csi_w(cfd, ",%.0f", scale * xyrp[i] + .5);
                debug_printf(",%.0f", scale * xyrp[i] + .5);
            }
            csi_w(cfd, ");");
            debug_printf(");\n");

            //			debug_printf ("mip->esign = %d\tmip->estep = %d\n", mip->esign, mip->estep);
            //			debug_printf ("scale = %.10f\txyrp = %.10f\n", scale, xyrp[0]);

        } // !virtual_mode
    }
    fflush(stdout);

    /* done */
    free((void *)x);
    free((void *)y);
    free((void *)r);
}

// ICE
static void tel_set_xdelta(double HA, double DEC)
{
    long int stepsHA;
    long int stepsDEC;

    if (HMOT->have && HMOT->xtrack)
    {
        double radsHA = HA * (2 * PI) / 360.0;
        stepsHA = HMOT->esign * HMOT->estep * radsHA / (2 * PI);
        csi_w(MIPCFD(HMOT), "xdel=%d;", stepsHA);
        debug_printf("HA xdelta %d encoder steps\n", stepsHA);
        fifoWrite(Tel_Id, 0, "HA xdelta %d encoder steps\n", stepsHA);
    }
    else
    {
        debug_printf("HA axis not present or not in xtrack mode\n");
        fifoWrite(Tel_Id, -1, "HA axis not present or not in xtrack mode");
    }

    if (DMOT->have && DMOT->xtrack)
    {
        double radsDEC = DEC * (2 * PI) / 360.0;
        stepsDEC = DMOT->esign * DMOT->estep * radsDEC / (2 * PI);
        csi_w(MIPCFD(DMOT), "xdel=%d;", stepsDEC);
        debug_printf("DEC xdelta %d encoder steps\n", stepsDEC);
        fifoWrite(Tel_Id, 0, "DEC xdelta %d encoder steps\n", stepsDEC);
    }
    else
    {
        debug_printf("DEC axis not present or not in xtrack mode\n");
        fifoWrite(Tel_Id, -1, "DEC axis not present or not in xtrack mode");
    }

    fflush(stdout);
    return;
}

static int buildXTrack(Now *np, Obj *op)
{
    double x, y, r;
    double *xyr[NMOT];
    double scale;
    double *xyrp;
    int cfd;
    Now localNow = *np;
    MotorInfo *mip;
    int axis;
    static int axrelax[NMOT];
    static float axjit[NMOT];

    double tnow = (localNow.n_mjd - xstrack) * SPD;

    // RGW
    if (xttrack == 0)
    {
        int idx;
        for (idx = 0; idx < NMOT; idx++)
        {
            axrelax[idx] = 4;
            axjit[idx] = 0.5;
        }
    }

    while ((xttrack - tnow) <= 2 * XTRACKINT)
    {
        xyr[TEL_HM] = &x;
        xyr[TEL_DM] = &y;
        xyr[TEL_RM] = &r;

        /* find track values mjd track time*/
        localNow.n_mjd = xstrack + xttrack / SPD;
        findAxes(&localNow, op, &x, &y, &r);
        (void)chkLimits(1, &x, &y, &r); /* let limit protect */

        FEM(mip)
        {
            //			printf("ttrack = %.0f\n", ttrack);
            if (mip->have && mip->xtrack)
            {
                // make sure we're homed to begin with
                char buf[128];
                if (axisHomedCheck(mip, buf))
                {
                    active_func = NULL;
                    stopTel(0);
                    fifoWrite(Tel_Id, -1, "Error: %s", buf);
                    toTTS("Error: %s", buf);
                    return -1;
                }

                axis = mip - telstatshmp->minfo;
                xyrp = xyr[axis];
                cfd = MIPCFD(mip);
                if (mip->haveenc)
                    scale = mip->esign * mip->estep / (2 * PI);
                else
                    scale = mip->sign * mip->step / (2 * PI);

                double pos = scale * (*xyrp);
                double rpos = pos + axrelax[axis];
                if (axrelax[axis] == 0)
                    rpos += axjit[axis];
                rpos = round(rpos);
                if (xttrack == 0)
                {
                    csi_w(cfd, "xdel=0;");
                    csi_w(cfd, "clock=0;");
                    //					csi_w(cfd, "maxvel=s;");
                    csi_w(cfd, "timeout=%d;", 1000); // 1 second
                    csi_w(cfd, "xtrack(%d,%.0f,%.0f,%.0f,%.0f,%.0f);", mip->haveenc ? 1 : 0, 0.0, round(xttrack), rpos,
                          0.0, 0.0);
                    debug_printf("axis=%d time=%.0f -> xtrack(%d,%.0f,%.0f,%.0f,%.0f,%.0f)\n", axis, tnow,
                                 mip->haveenc ? 1 : 0, 0.0, round(xttrack), rpos, 0.0, 0.0);
                }
                else
                {
                    csi_w(cfd, "xpos(%.0f,%.0f);", round(xttrack), rpos);
                    debug_printf("axis=%d time=%.0f -> xpos(%.0f,%.0f);\n", axis, tnow, round(xttrack), rpos);
                }
                axrelax[axis] /= 2;
                axjit[axis] = -axjit[axis];

                fflush(stdout);
            }
        }
        xttrack += XTRACKINT;
    }
    return 0;
}

double xgetvar(int sfd, int index)
{
    double value;
    char str[32];
    char msg[32];
    sprintf(msg, "xgetvar(%d);", index);
    csi_wr(sfd, str, sizeof(str), msg);
    str[strlen(str) - 1] = 0;
    if (sscanf(str, "%lf", &value) == 1)
    {
        return value;
    }
    debug_printf("%s\n", str);

    return 0;
}

// ICE

/* if first or TRACKINT has expired and needs refreshed compute and load a new
 *   tracking profile.
 * also always handle jogginf, limit checks, telstat info, whether on track.
 * return -1 when tracking is just not possible, 0 when ok to keep trying.
 */
static int trackObj(Obj *op, int first)
{
    Now *np = &telstatshmp->now; /* pointer to live one */
    Now now = telstatshmp->now;  /* stable and changeable copy */
    double ra, dec, lst, ha;
    double x, y, r;
    int clocknow;
    MotorInfo *mip;

    FEM(mip)
    {
        if (mip->xtrack)
            xtrack_mode = 1;
    }

    // ICE
    if (xtrack_mode && virtual_mode == 0)
    {
        if (first)
        {
            xttrack = 0;
            xstrack = now.n_mjd;
        }

        int error;
        error = buildXTrack(&now, op);
        if (error)
            return -1;
    }
    //	else
    {
        /* download tracking profile if new or expired */
        if (first || mjd > strack + TRACKINT / SPD)
        {
            /* sync all clocks to 0 */
            /* N.B. use MIPSFD to insure precedes main loop clock reads */
            FEM(mip)
            {
                if (mip->have && !mip->xtrack)
                {
                    if (virtual_mode)
                    {
                        vmcResetClock(mip->axis);
                    }
                    else
                    {
                        csi_w(MIPSFD(mip), "clock=0;");
                    }
                }
            }

            /* record when this TRACKINT began */
            strack = now.n_mjd;

            /* set all timeouts to TRACKINT */
            FEM(mip)
            {
                if (mip->have && !mip->xtrack)
                {
                    if (virtual_mode)
                    {
                        vmcSetTimeout(mip->axis, TRACKINT * 1000);
                    }
                    else
                    {
                        csi_w(MIPSFD(mip), "timeout=%d;", TRACKINT * 1000);
                    }
                }
            }

            /* if just starting, reset any lingering track offset */
            if (first)
            {
                FEM(mip)
                {
                    if (mip->have && !mip->xtrack)
                    {
                        // make sure we're homed to begin with
                        char buf[128];
                        if (axisHomedCheck(mip, buf))
                        {
                            active_func = NULL;
                            stopTel(0);
                            fifoWrite(Tel_Id, -1, "Error: %s", buf);
                            toTTS("Error: %s", buf);
                            return -1;
                        }

                        if (virtual_mode)
                        {
                            vmcSetTrackingOffset(mip->axis, 0);
                        }
                        else
                        {
                            csi_w(MIPSFD(mip), "toffset=0;");
                        }
                    }
                }
            }

            /* now build and install tracking profiles */
            buildTrack(&now, op);
        }
    }
    // ICE

    /* update actual position info */
    readRaw();

    /* quick, get current value of typical clock.
     * use this to compute desired to avoid host computer time jitter
     */
    mip = HMOT->have ? HMOT : DMOT; /* surely we have one ! */
    if (virtual_mode)
    {
        clocknow = vmcGetClock(mip->axis);
    }
    //	else if (mip->xtrack)
    //	{
    //		clocknow = (int) (xgetvar(MIPSFD(mip), 4) * 1000.0);
    //	}
    else
    {
        clocknow = csi_rix(MIPSFD(mip), "=clock;");
    }

    mkCook();

    /* check axes */
    if (checkAxes() < 0)
    {
        stopTel(1);
        return (-1);
    }

    /* find desired topocentric apparent place and axes @ clocknow */
    if (mip->xtrack && !virtual_mode)
        now.n_mjd = xstrack + clocknow / (SPD * 1000.);
    else
        now.n_mjd = strack + clocknow / (SPD * 1000.);
    x = fabs(mjd - now.n_mjd) * SPD;
    if (x > MAXJITTER)
    {
        fifoWrite(Tel_Id, -5, "Motion controller clock drift exceeds %g sec: %g", MAXJITTER, x);
        fifoWrite(Tel_Id, -5, "clocknow=%d. strack=%g", clocknow, strack);
        stopTel(0);
        return (-1);
    }
    findAxes(&now, op, &x, &y, &r);

    if (chkLimits(1, &x, &y, &r) < 0)
    {
        stopTel(0);
        return (-1);
    }

    telstatshmp->Dalt = op->s_alt;
    telstatshmp->Daz = op->s_az;
    telstatshmp->DARA = ra = op->s_ra;
    telstatshmp->DADec = dec = op->s_dec;
    now_lst(&now, &lst);
    ha = hrrad(lst) - ra;
    haRange(&ha);
    telstatshmp->DAHA = ha;
    ap_as(&now, J2000, &ra, &dec);
    telstatshmp->DJ2kRA = ra;
    telstatshmp->DJ2kDec = dec;
    HMOT->dpos = x;
    DMOT->dpos = y;
    RMOT->dpos = r;

    /* check progress, revert to hunting if lose track */
    switch (telstatshmp->telstate)
    {
    case TS_HUNTING:
        if (atTarget() == 0)
        {
            fifoWrite(Tel_Id, 3, "All axes have tracking lock");
            fifoWrite(Tel_Id, 0, "Now tracking");
            toTTS("The telescope is now tracking.");
            telstatshmp->telstate = TS_TRACKING;
            telstatshmp->telstateidx++;
        }
        break;
    case TS_TRACKING:
        if (!telstatshmp->jogging_ison && onTarget(&mip) < 0)
        {
            //			fifoWrite(Tel_Id, 4, "Axis %d lost tracking lock", mip->axis);
            fifoWrite(Tel_Id, 4, "Axis %d lost tracking lock deltaradians=%f (set TRACKACC telescope.cfg)", mip->axis,
                      delra(mip->cpos - mip->dpos));
            toTTS("The telescope has lost tracking lock.");
            telstatshmp->telstate = TS_HUNTING;
            telstatshmp->telstateidx++;
        }
        break;

    default:
        break;
    }

    // ICE
#ifdef TELESCOPED_DEBUG
    if (xtrack_mode && virtual_mode == 0)
    {
        if (HMOT->xtrack)
        {
            debug_printf("clock = %lf\t", clocknow / 1000.0);

            // double HApos = xxgetpos(MIPSFD(HMOT));
            long HApos = HMOT->raw;
            debug_printf("HApos = %ld\t", HApos);
            double HAVel = xgetvar(MIPSFD(HMOT), 3);
            debug_printf("HAvel = %lf\t", HAVel);
            double HAVg = xgetvar(MIPSFD(HMOT), 1);
            debug_printf("HAvg = %lf\t", HAVg);
            double HAerr = xgetvar(MIPSFD(HMOT), 2);
            debug_printf("HAerr = %lf\t", HAerr);
        }
        debug_printf("\t");

        if (DMOT->xtrack)
        {
            //			double DECpos = xxgetpos(MIPSFD(DMOT));
            long DECpos = DMOT->raw;
            debug_printf("DECpos = %ld\t", DECpos);
            double DECVel = xgetvar(MIPSFD(DMOT), 3);
            debug_printf("DECvel = %lf\t", DECVel);
            double DECVg = xgetvar(MIPSFD(DMOT), 1);
            debug_printf("DECvg = %lf\t", DECVg);
            double DECerr = xgetvar(MIPSFD(DMOT), 2);
            debug_printf("DECerr = %lf\t", DECerr);
        }
        debug_printf("\n");
    }

    debug_fflush(stdout);
#endif
    // ICE

    /* ok */
    return (0);
}

/* compute axes for op at np, including fixed schedule offsets if any.
 * return 0 if ok, -1 if exceeds limits
 * N.B. o_type of *op may be different upon return.
 */
static void findAxes(Now *np, Obj *op, double *xp, double *yp, double *rp)
{
    double ha, dec;
    Obj fobj;

    if (r_offset || d_offset)
    {
        /* find offsets to op as a fixed object */
        double ra, dec;

        epoch = J2000;
        obj_cir(np, op);
        ra = op->s_ra;
        dec = op->s_dec;

        /* apply offsets */
        ra += r_offset;
        dec += d_offset;

        op = &fobj;
        op->o_type = FIXED;
        op->f_RA = ra;
        op->f_dec = dec;
        op->f_epoch = J2000;
    }

    epoch = EOD;
    obj_cir(np, op);
    aa_hadec(lat, op->s_alt, op->s_az, &ha, &dec);
    hd2xyr(ha, dec, xp, yp, rp);
}

/* convert an ha/dec to scope x/y/r, allowing for mesh corrections.
 * in many ways, this is the reverse of mkCook().
 */
static void hd2xyr(double ha, double dec, double *xp, double *yp, double *rp)
{
    TelAxes *tap = &telstatshmp->tax;
    double mdha, mddec;
    double x, y, r;

    tel_mount_cor(ha, dec, &mdha, &mddec);
    // tdlog("hd2xyr: ha %.4lf dec %.4lf mdha %.4lf mddec %.4lf\n",ha,dec,mdha,mddec);
    ha += mdha;
    dec += mddec;
    hdRange(&ha, &dec);
    // tdlog("ha and dec after hdRange: %.4lf  %.4lf\n",ha,dec);
    tel_hadec2xy(ha, dec, tap, &x, &y);
    // tdlog("tel_hadec2xy returns x=%.4lf and y=%.4lf\n",x,y);
    tel_ideal2realxy(tap, &x, &y);
    // tdlog("tel_ideal2realxy changes these to x=%.4lf  y=%.4lf\n",x,y);
    if (RMOT->have)
    {
        Now *np = &telstatshmp->now;
        tel_hadec2PA(ha, dec, tap, lat, &r);
        r += tap->R0 * RMOT->sign;
    }
    else
        r = 0;

    *xp = x;
    *yp = y;
    *rp = r;
}

/* using the raw values, compute the astro position.
 * in many ways, this is the reverse of hd2xyz().
 */
static void mkCook()
{
    Now *np = &telstatshmp->now;
    TelAxes *tap = &telstatshmp->tax;
    double lst, ra, ha, dec, alt, az;
    double mdha, mddec;
    double x, y, r;

    /* handy axis values */
    x = HMOT->cpos;
    y = DMOT->cpos;
    r = RMOT->cpos;

    /* back out non-ideal axes info */
    tel_realxy2ideal(tap, &x, &y);

    /* convert encoders to apparent ha/dec */
    tel_xy2hadec(x, y, tap, &ha, &dec);

    /* back out the mesh corrections */
    tel_mount_cor(ha, dec, &mdha, &mddec);
    telstatshmp->mdha = mdha;
    telstatshmp->mddec = mddec;
    ha -= mdha;
    dec -= mddec;
    hdRange(&ha, &dec);

    /* find horizon coords */
    hadec_aa(lat, ha, dec, &alt, &az);
    telstatshmp->Calt = alt;
    telstatshmp->Caz = az;

    /* find apparent equatorial coords */
    unrefract(pressure, temp, alt, &alt);
    aa_hadec(lat, alt, az, &ha, &dec);
    now_lst(np, &lst);
    lst = hrrad(lst);
    ra = lst - ha;
    range(&ra, 2 * PI);
    telstatshmp->CARA = ra;
    telstatshmp->CAHA = ha;
    telstatshmp->CADec = dec;
    telstatshmp->Clst = lst;

    /* find J2000 astrometric equatorial coords */
    ap_as(np, J2000, &ra, &dec);
    telstatshmp->CJ2kRA = ra;
    telstatshmp->CJ2kDec = dec;

    /* find position angle */
    tel_hadec2PA(ha, dec, tap, lat, &r);
    telstatshmp->CPA = r;
}

/* read the raw values */
static void readRaw()
{
    MotorInfo *mip;

    FEM(mip)
    {
        if (!mip->have)
            continue;

        if (virtual_mode)
        {
            mip->raw = vmcGetPosition(mip->axis);
            mip->cpos = (2 * PI) * mip->sign * mip->raw / mip->step;
        }
        //		else if (mip->xtrack)
        //		{
        //			mip->raw = (int) xgetvar(MIPSFD(mip), 0);
        //			if (mip->haveenc)
        //			{
        //				mip->cpos = (2 * PI) * mip->esign * mip->raw / mip->estep;
        //			}
        //			else
        //			{
        //				mip->cpos = (2 * PI) * mip->sign  * mip->raw / mip->step;
        //			}
        ////			debug_printf("mip->raw[%d] = %d\t", MIPSFD(mip), mip->raw);
        ////			debug_printf("mip->cpos[%d] = %lf\n", MIPSFD(mip), mip->cpos);
        //		}
        else
        {
            if (mip->haveenc)
            {
                double draw;
                int raw;
                /* just change by half-step if encoder changed by 1 */
                raw = csi_rix(MIPSFD(mip), "=epos;");
                draw = abs(raw - mip->raw) == 1 ? (raw + mip->raw) / 2.0 : raw;
                mip->raw = raw;
                mip->cpos = (2 * PI) * mip->esign * draw / mip->estep;
            }
            else
            {
                mip->raw = csi_rix(MIPSFD(mip), "=mpos;");
                mip->cpos = (2 * PI) * mip->sign * mip->raw / mip->step;
            }
        }
    }
}

/* issue a stop to all telescope axes */
static void stopTel(int fast)
{
    MotorInfo *mip;

    FEM(mip)
    {
        if (mip->have)
        {
            if (virtual_mode)
            {
                vmcStop(mip->axis);
            }
            else
            {
                int cfd = MIPCFD(mip);
                csi_intr(cfd);
                csi_w(MIPSFD(mip), "mtvel=0;");
            }
            mip->cvel = 0;
            mip->limiting = 0;
            mip->homing = 0;
        }
    }

    telstatshmp->jogging_ison = 0;
    telstatshmp->telstate = TS_STOPPED; /* well, soon anyway */
    telstatshmp->telstateidx++;
}

/* return 0 if all axes are within acceptable margin of desired, else -1 if
 * if any are out of range and set *mipp to offending axis.
 * N.B. use this only while tracking; use atTarget() when first acquiring.
 */
static int onTarget(MotorInfo **mipp)
{
    MotorInfo *mip;

    FEM(mip)
    {
        double trackacc;

        if (!mip->have)
            continue;

        /* tolerance: "0" means +-1 enc tick */
        trackacc = TRACKACC == 0.0 ? 1.5 * (2 * PI) / (mip->haveenc ? mip->estep : mip->step) : TRACKACC;

        if (delra(mip->cpos - mip->dpos) > trackacc)
        {
            *mipp = mip;
            return (-1);
        }
    }

    /* all ok */
    return (0);
}

/* return 0 if all axes are within TRACKACC, else -1.
 * N.B. use this when first acquiring; use onTarget() while tracking.
 */
static int atTarget()
{
    static double mjd0;
    Now *np = &telstatshmp->now;
    MotorInfo *mip;

    FEM(mip)
    {
        double trackacc;

        if (!mip->have)
            continue;

        /* tolerance: "0" means +-1 enc tick */
        trackacc = TRACKACC == 0.0 ? 1.5 * (2 * PI) / (mip->haveenc ? mip->estep : mip->step) : TRACKACC;

        if (delra(mip->cpos - mip->dpos) > trackacc)
        {
            mjd0 = 0;
            return (-1);
        }
    }

    /* if get here, all axes are within TRACKACC this time
     * but still doesn't count until/unless it stays on for a second.
     */
    if (!mjd0)
    {
        mjd0 = mjd;
        return (-1);
    }
    if (mjd >= mjd0 + 1. / SPD)
        return (0);
    return (-1);
}

/* check each canonical axis value for being beyond the hardware limit.
 * if wrapok, wrap the input values whole revolutions to accommodate limit.
 * if find any trouble, send failed message to Tel_Id and return -1.
 * else (all ok) return 0.
 */
static int chkLimits(int wrapok, double *xp, double *yp, double *rp)
{
    double *valp[NMOT];
    MotorInfo *mip;
    char str[64];

    /* store so we can effectively access them via a mip */
    valp[TEL_HM] = xp;
    valp[TEL_DM] = yp;
    valp[TEL_RM] = rp;

    FEM(mip)
    {
        double *vp = valp[mip - telstatshmp->minfo];
        double v = *vp;

        if (!mip->have)
            continue;

        while (v <= mip->neglim)
        {
            if (!wrapok)
            {
                fs_sexa(str, raddeg(v), 4, 3600);
                fifoWrite(Tel_Id, -2, "Axis %d: %s hits negative limit", mip->axis, str);
                return (-1);
            }
            v += 2 * PI;
        }

        while (v >= mip->poslim)
        {
            if (!wrapok)
            {
                fs_sexa(str, raddeg(v), 4, 3600);
                fifoWrite(Tel_Id, -3, "Axis %d: %s hits positive limit", mip->axis, str);
                return (-1);
            }
            v -= 2 * PI;
        }

        /* double-check */
        if (v <= mip->neglim || v >= mip->poslim)
        {
            fs_sexa(str, raddeg(v), 4, 3600);
            fifoWrite(Tel_Id, -4, "Axis %d: %s trapped within limits gap", mip->axis, str);
            return (-1);
        }

        /* pass back possibly updated */
        *vp = v;
    }

    /* if get here, all ok */
    return (0);
}

/* set all desireds to currents */
static void dummyTarg()
{
    HMOT->dpos = HMOT->cpos;
    DMOT->dpos = DMOT->cpos;
    RMOT->dpos = RMOT->cpos;

    telstatshmp->DJ2kRA = telstatshmp->CJ2kRA;
    telstatshmp->DJ2kDec = telstatshmp->CJ2kDec;
    telstatshmp->DARA = telstatshmp->CARA;
    telstatshmp->DADec = telstatshmp->CADec;
    telstatshmp->DAHA = telstatshmp->CAHA;
    telstatshmp->Dalt = telstatshmp->Calt;
    telstatshmp->Daz = telstatshmp->Caz;
    telstatshmp->DPA = telstatshmp->CPA;
}

/* called when get a j* jog command while TRACKING.
 * we do not set active_func so we do not look at first.
 */
static void jogTrack(int first, char dircode, int velocity)
{
    MotorInfo *mip = NULL;
    double gvel = 0;
    double scale;
    int stpv;

    /* establish axis and vel */
    switch (dircode)
    {
    case 'N':
        mip = DMOT;
        gvel = CGUIDEVEL;
        break;
    case 'n':
        mip = DMOT;
        gvel = FGUIDEVEL;
        break;
    case 'S':
        mip = DMOT;
        gvel = -CGUIDEVEL;
        break;
    case 's':
        mip = DMOT;
        gvel = -FGUIDEVEL;
        break;
    case 'E':
        mip = HMOT;
        gvel = CGUIDEVEL;
        break;
    case 'e':
        mip = HMOT;
        gvel = FGUIDEVEL;
        break;
    case 'W':
        mip = HMOT;
        gvel = -CGUIDEVEL;
        break;
    case 'w':
        mip = HMOT;
        gvel = -FGUIDEVEL;
        break;
    case '0': /* hold current position */
        if (!virtual_mode)
        {
            mip = HMOT;
            if (mip->have)
                csi_intr(MIPCFD(mip)); /* kill while() */
            mip = DMOT;
            if (mip->have)
                csi_intr(MIPCFD(mip)); /* kill while() */
            return;
        }
    }

    /* sanity checks */
    if (!mip)
    {
        tdlog("Bogus jog direction code '%c'", dircode);
        return;
    }
    if (!mip->have)
    {
        tdlog("No axis to move %c", dircode);
        return;
    }

    /* ok, issue the jog */
    if (mip->haveenc)
        scale = mip->esign * mip->estep / (2 * PI);
    else
        scale = mip->sign * mip->step / (2 * PI);
    stpv = floor(gvel * scale + .5);
    if (virtual_mode)
    {
        vmcSetTrackingOffset(mip->axis, stpv);
    }
    else
    {
        // ICE
        if (mip->xtrack)
            csi_w(MIPCFD(mip), "while(1) {xdel += %d/5; pause(200);}", stpv);
        else
            // ICE
            csi_w(MIPCFD(mip), "while(1) {toffset += %d/5; pause(200);}", stpv);
    }
    telstatshmp->jogging_ison = 1;
}

/* called when get a j* command while in any state other than TRACKING.
 * we do set not active_func so we do not look at first.
 */
static void jogSlew(int first, char dircode, int velocity)
{
    MotorInfo *mip = 0;
    char msg[1024];

    if (velocity > VEL_MAX)
        return;
    if (velocity < 0)
        return;

    int velPct = velocity * 100 / VEL_MAX;

    /* TODO: slave the rotator */

    /* not really used, but shm will show */
    telstatshmp->jdha = telstatshmp->jddec = 0;

    switch (dircode)
    {
    case 'N':
        mip = DMOT;
        mip->cvel = mip->maxvel * velocity / VEL_MAX;
        sprintf(msg, "up, velocity = %d%%", velPct);
        break;
    case 'n':
        mip = DMOT;
        mip->cvel = CGUIDEVEL;
        sprintf(msg, "up, slow");
        break;
    case 'S':
        mip = DMOT;
        mip->cvel = -mip->maxvel * velocity / VEL_MAX;
        sprintf(msg, "down, velocity = %d%%", velPct);
        break;
    case 's':
        mip = DMOT;
        mip->cvel = -CGUIDEVEL;
        sprintf(msg, "down, slow");
        break;
    case 'E':
        mip = HMOT;
        mip->cvel = mip->maxvel * velocity / VEL_MAX;
        sprintf(msg, "CCW, velocity = %d%%", velPct);
        break;
    case 'e':
        mip = HMOT;
        mip->cvel = CGUIDEVEL;
        sprintf(msg, "CCW, slow");
        break;
    case 'W':
        mip = HMOT;
        mip->cvel = -mip->maxvel * velocity / VEL_MAX;
        sprintf(msg, "CW, velocity = %d%%", velPct);
        break;
    case 'w':
        mip = HMOT;
        mip->cvel = -CGUIDEVEL;
        sprintf(msg, "CW, slow");
        break;
    case '0': /* stop here */
        stopTel(0);
        fifoWrite(Tel_Id, 0, "Paddle command stop");
        telstatshmp->jogging_ison = 0;
        return;
    }

    /* sanity checks */
    if (!mip)
    {
        tdlog("Bogus jog direction code '%c'", dircode);
        return;
    }
    if (!mip->have)
    {
        tdlog("No axis to move %c", dircode);
        return;
    }

    /* ok, issue the jog */
    if (virtual_mode)
    {
        vmcJog(mip->axis, CVELStp(mip));
    }
    else
    {
        // ICE
        csi_w(MIPCFD(mip), "clock=0;");
        //		csi_w(MIPCFD(mip), "maxvel=s;");
        csi_w(MIPCFD(mip), "timeout=300000;");
        // ICE
        csi_w(MIPCFD(mip), "mtvel=%d;", CVELStp(mip));
    }
    telstatshmp->telstate = TS_SLEWING;
    telstatshmp->telstateidx++;
    fifoWrite(Tel_Id, 5, "Paddle command %s", msg);
    telstatshmp->jogging_ison = 1;
}

/** Apply an absolute tracking offset in arcseconds to each axis */
static void offsetTracking(int first, double harcsecs, double darcsecs)
{
    long hcounts, dcounts;

    (void)first; // not used in this context

    if (telstatshmp->telstate != TS_TRACKING)
    {
        fifoWrite(Tel_Id, -1, "Telescope is not tracking -- offset ignored");
        return;
    }

    hcounts = (long)(harcsecs * (HMOT->estep * HMOT->esign) /
                     1296000.0); // divide by 360, then by 60 then by 60 == steps per arcsecond
    dcounts = (long)(darcsecs * (DMOT->estep * DMOT->esign) /
                     1296000.0); // divide by 360, then by 60 then by 60 == steps per arcsecond

    /* okay, issue the offsets */
    if (virtual_mode)
    {
        vmcSetTrackingOffset(HMOT->axis, hcounts);
        vmcSetTrackingOffset(DMOT->axis, dcounts);
    }
    else
    {
        csi_w(MIPCFD(HMOT), "toffset = %d;", hcounts);
        csi_w(MIPCFD(DMOT), "toffset = %d;", dcounts);
    }

    // Turn on jogging ... this produces the offset and also serves as a flag that we have done this
    telstatshmp->jogging_ison = 1;

    fifoWrite(Tel_Id, 0, "Tracking offset by %3.3f x %3.3f arcseconds (%ld x %ld steps)", harcsecs, darcsecs, hcounts,
              dcounts);
}

/* reread the config files -- exit if trouble */
static void initCfg()
{
#define NTDCFG (sizeof(tdcfg) / sizeof(tdcfg[0]))
#define NHCFG (sizeof(hcfg) / sizeof(hcfg[0]))

    // ICE
    static int HXTRACK, DXTRACK, HHAVEENC, DHAVEENC;
    // ICE

    static double HMAXVEL, HMAXACC, HSLIMACC, HDAMP, HTRENCWT;
    static int HHAVE, HAXIS, HENCHOME, HPOSSIDE, HHOMELOW, HESTEP, HESIGN;

    static double DMAXVEL, DMAXACC, DSLIMACC, DDAMP, DTRENCWT;
    static int DHAVE, DAXIS, DENCHOME, DPOSSIDE, DHOMELOW, DESTEP, DESIGN;

    static double RMAXVEL, RMAXACC, RSLIMACC, RDAMP;
    static int RHAVE, RAXIS, RHASLIM, RPOSSIDE, RHOMELOW, RSTEP, RSIGN;

    static int GERMEQ;
    static int ZENFLIP;

    static CfgEntry tdcfg[] = {
        // ICE
        {"HXTRACK", CFG_INT, &HXTRACK},
        {"DXTRACK", CFG_INT, &DXTRACK},
        {"HHAVEENC", CFG_INT, &HHAVEENC},
        {"DHAVEENC", CFG_INT, &DHAVEENC},
        {"XTRACKINT", CFG_INT, &XTRACKINT},
        // ICE
        {"HHAVE", CFG_INT, &HHAVE},
        {"HAXIS", CFG_INT, &HAXIS},
        {"HHOMELOW", CFG_INT, &HHOMELOW},
        {"HPOSSIDE", CFG_INT, &HPOSSIDE},
        {"HESTEP", CFG_INT, &HESTEP},
        {"HESIGN", CFG_INT, &HESIGN},
        {"HMAXVEL", CFG_DBL, &HMAXVEL},
        {"HMAXACC", CFG_DBL, &HMAXACC},
        {"HSLIMACC", CFG_DBL, &HSLIMACC},

        {"DHAVE", CFG_INT, &DHAVE},
        {"DAXIS", CFG_INT, &DAXIS},
        {"DHOMELOW", CFG_INT, &DHOMELOW},
        {"DPOSSIDE", CFG_INT, &DPOSSIDE},
        {"DESTEP", CFG_INT, &DESTEP},
        {"DESIGN", CFG_INT, &DESIGN},
        {"DMAXVEL", CFG_DBL, &DMAXVEL},
        {"DMAXACC", CFG_DBL, &DMAXACC},
        {"DSLIMACC", CFG_DBL, &DSLIMACC},

        {"RHAVE", CFG_INT, &RHAVE},
        {"RAXIS", CFG_INT, &RAXIS},
        {"RHASLIM", CFG_INT, &RHASLIM},
        {"RHOMELOW", CFG_INT, &RHOMELOW},
        {"RPOSSIDE", CFG_INT, &RPOSSIDE},
        {"RSTEP", CFG_INT, &RSTEP},
        {"RSIGN", CFG_INT, &RSIGN},
        {"RMAXVEL", CFG_DBL, &RMAXVEL},
        {"RMAXACC", CFG_DBL, &RMAXACC},
        {"RSLIMACC", CFG_DBL, &RSLIMACC},

        {"TRACKINT", CFG_INT, &TRACKINT},
        {"GERMEQ", CFG_INT, &GERMEQ},
        {"ZENFLIP", CFG_INT, &ZENFLIP},
        {"TRACKACC", CFG_DBL, &TRACKACC},
        {"FGUIDEVEL", CFG_DBL, &FGUIDEVEL},
        {"CGUIDEVEL", CFG_DBL, &CGUIDEVEL},
    };

    static double HT, DT, XP, YC, NP, R0;
    static double HPOSLIM, HNEGLIM, DPOSLIM, DNEGLIM, RNEGLIM, RPOSLIM;
    static int HSTEP, HSIGN, DSTEP, DSIGN;

    static CfgEntry hcfg[] = {
        {"HT", CFG_DBL, &HT},           {"DT", CFG_DBL, &DT},           {"XP", CFG_DBL, &XP},
        {"YC", CFG_DBL, &YC},           {"NP", CFG_DBL, &NP},           {"R0", CFG_DBL, &R0},

        {"HPOSLIM", CFG_DBL, &HPOSLIM}, {"HNEGLIM", CFG_DBL, &HNEGLIM}, {"DPOSLIM", CFG_DBL, &DPOSLIM},
        {"DNEGLIM", CFG_DBL, &DNEGLIM}, {"RNEGLIM", CFG_DBL, &RNEGLIM}, {"RPOSLIM", CFG_DBL, &RPOSLIM},

        {"HSTEP", CFG_INT, &HSTEP},     {"HSIGN", CFG_INT, &HSIGN},     {"DSTEP", CFG_INT, &DSTEP},
        {"DSIGN", CFG_INT, &DSIGN},
    };

    static int LARGEXP;
    static CfgEntry hcfg2[] = {
        {"LARGEXP", CFG_INT, &LARGEXP},
    };

    MotorInfo *mip;
    TelAxes *tap;
    int n;
    int oldhomed;

    /* read in everything */
    n = readCfgFile(1, tdcfn, tdcfg, NTDCFG);
    if (n != NTDCFG)
    {
        cfgFileError(tdcfn, n, (CfgPrFp)tdlog, tdcfg, NTDCFG);
        die();
    }
    n = readCfgFile(1, hcfn, hcfg, NHCFG);
    if (n != NHCFG)
    {
        cfgFileError(hcfn, n, (CfgPrFp)tdlog, hcfg, NHCFG);
        die();
    }

    // Added fix for RA home switch being > 180 degrees from north
    LARGEXP = 0;
    (void)readCfgFile(1, hcfn, hcfg2, 1);
    if (LARGEXP)
    {
        HT -= (PI / 2);
        XP += (PI / 2);
    }

    /* misc checks */
    if (TRACKINT <= 0)
    {
        tdlog("TRACKINT must be > 0\n");
        die();
    }

    /* install H */

    mip = &telstatshmp->minfo[TEL_HM];
    oldhomed = mip->ishomed;
    memset((void *)mip, 0, sizeof(*mip)); // this is what is clearing the home status!
    mip->ishomed = oldhomed;
    mip->axis = HAXIS;
    mip->have = HHAVE;
    // ICE haveenc mode variable XTRACK at telescope.cfg
    mip->xtrack = HXTRACK && !virtual_mode;
    mip->haveenc = HHAVEENC;
    // ICE
    // ICEmip->haveenc = 1;
    mip->enchome = HENCHOME;
    mip->havelim = 1;
    if (HPOSSIDE != 0 && HPOSSIDE != 1)
    {
        tdlog("HPOSSIDE must be 0 or 1\n");
        die();
    }
    mip->posside = HPOSSIDE;
    if (HHOMELOW != 0 && HHOMELOW != 1)
    {
        tdlog("HHOMELOW must be 0 or 1\n");
        die();
    }
    mip->homelow = HHOMELOW;
    mip->step = HSTEP;
    if (abs(HSIGN) != 1)
    {
        tdlog("HSIGN must be +-1\n");
        die();
    }
    mip->sign = HSIGN;
    mip->estep = HESTEP;
    if (abs(HESIGN) != 1)
    {
        tdlog("HESIGN must be +-1\n");
        die();
    }
    mip->esign = HESIGN;
    mip->limmarg = 0;
    if (HMAXVEL <= 0)
    {
        tdlog("HMAXVEL must be > 0\n");
        die();
    }
    mip->maxvel = HMAXVEL;
    mip->maxacc = HMAXACC;
    mip->slimacc = HSLIMACC;
    mip->poslim = HPOSLIM;
    mip->neglim = HNEGLIM;
    mip->trencwt = HTRENCWT;
    mip->df = HDAMP;

    /* install D */

    mip = &telstatshmp->minfo[TEL_DM];
    oldhomed = mip->ishomed;
    memset((void *)mip, 0, sizeof(*mip));
    mip->ishomed = oldhomed;
    mip->axis = DAXIS;
    mip->have = DHAVE;
    // ICE haveenc mode variable XTRACK at telescope.cfg
    mip->xtrack = DXTRACK && !virtual_mode;
    mip->haveenc = DHAVEENC;
    // ICE
    // ICE mip->haveenc = 1;
    mip->enchome = DENCHOME;
    mip->havelim = 1;
    if (DPOSSIDE != 0 && DPOSSIDE != 1)
    {
        tdlog("DPOSSIDE must be 0 or 1\n");
        die();
    }
    mip->posside = DPOSSIDE;
    if (DHOMELOW != 0 && DHOMELOW != 1)
    {
        tdlog("DHOMELOW must be 0 or 1\n");
        die();
    }
    mip->homelow = DHOMELOW;
    mip->step = DSTEP;
    if (abs(DSIGN) != 1)
    {
        tdlog("DSIGN must be +-1\n");
        die();
    }
    mip->sign = DSIGN;
    mip->estep = DESTEP;
    if (abs(DESIGN) != 1)
    {
        tdlog("DESIGN must be +-1\n");
        die();
    }
    mip->esign = DESIGN;
    mip->limmarg = 0;
    if (DMAXVEL <= 0)
    {
        tdlog("DMAXVEL must be > 0\n");
        die();
    }
    mip->maxvel = DMAXVEL;
    mip->maxacc = DMAXACC;
    mip->slimacc = DSLIMACC;
    mip->poslim = DPOSLIM;
    mip->neglim = DNEGLIM;
    mip->trencwt = DTRENCWT;
    mip->df = DDAMP;

    /* install R */

    mip = &telstatshmp->minfo[TEL_RM];
    oldhomed = mip->ishomed;
    memset((void *)mip, 0, sizeof(*mip));
    mip->ishomed = oldhomed;
    mip->axis = RAXIS;
    mip->have = RHAVE;
    // ICE haveenc mode variable XTRACK at telescope.cfg
    mip->xtrack = 0;
    mip->haveenc = 0;
    // ICE
    // ICEmip->haveenc = 0;
    mip->enchome = 0;
    mip->havelim = RHASLIM;
    if (RPOSSIDE != 0 && RPOSSIDE != 1)
    {
        tdlog("RPOSSIDE must be 0 or 1\n");
        die();
    }
    mip->posside = RPOSSIDE;
    if (RHOMELOW != 0 && RHOMELOW != 1)
    {
        tdlog("RHOMELOW must be 0 or 1\n");
        die();
    }
    mip->homelow = RHOMELOW;
    mip->step = RSTEP;
    if (abs(RSIGN) != 1)
    {
        tdlog("RSIGN must be +-1\n");
        die();
    }
    mip->sign = RSIGN;
    mip->estep = RSTEP;
    mip->esign = RSIGN;
    mip->limmarg = 0;
    if (RMAXVEL <= 0)
    {
        tdlog("RMAXVEL must be > 0\n");
        die();
    }
    mip->maxvel = RMAXVEL;
    mip->maxacc = RMAXACC;
    mip->slimacc = RSLIMACC;
    mip->poslim = RPOSLIM;
    mip->neglim = RNEGLIM;
    mip->trencwt = 0;
    mip->df = RDAMP;

    tap = &telstatshmp->tax;
    memset((void *)tap, 0, sizeof(*tap));
    tap->GERMEQ = GERMEQ;
    tap->ZENFLIP = ZENFLIP;
    tap->HT = HT;
    tap->DT = DT;
    tap->XP = XP;
    tap->YC = YC;
    tap->NP = NP;
    tap->R0 = R0;

    tap->hneglim = telstatshmp->minfo[TEL_HM].neglim;
    tap->hposlim = telstatshmp->minfo[TEL_HM].poslim;

    telstatshmp->dt = 100; /* not critical */

    /* re-read the mesh  file */
    init_mount_cor();

#undef NTDCFG
#undef NHCFG
}

/* check for stuck axes.
 * return 0 if all ok, else -1
 */
static int checkAxes()
{
    MotorInfo *mip;
    char buf[1024];
    int nbad = 0;

    /* check all axes to learn more */
    FEM(mip)
    {
        if (axisLimitCheck(mip, buf) < 0)
        {
            fifoWrite(Tel_Id, -8, "%s", buf);
            nbad++;
        }
        else if (axisMotionCheck(mip, buf) < 0)
        {
            fifoWrite(Tel_Id, -9, "%s", buf);
            nbad++;
        }
    }

    return (nbad > 0 ? -1 : 0);
}

/* return a static string to pronounce the location to reasonable precision */
static char *sayWhere(double alt, double az)
{
    static char w[64];

    if (alt > degrad(72))
        strcpy(w, "very hi");
    else if (alt > degrad(54))
        strcpy(w, "hi");
    else if (alt > degrad(36))
        strcpy(w, "half way up");
    else if (alt > degrad(18))
        strcpy(w, "low");
    else
        strcpy(w, "very low");

    strcat(w, ", in the ");
    strcat(w, cardDirLName(az));

    return (w);
}

/* RGW */
/* Read stats() and print to log every minute */
static void readStats(void)
{
    static time_t tlast = 0;
    MotorInfo *fd[] = {HMOT, DMOT};

    time_t t = time(NULL);
    if (t - tlast < 60)
        return;
    tlast = t;
    int idx;
    for (idx = 0; idx < 2; idx++)
    {
        MotorInfo *mip = fd[idx];
        int pexpect = (idx == 0) ? 2 : 1;
        csi_w(MIPSFD(mip), "stats();");
        char buf[1024];
        int pcnt = 0;
        while (csi_r(MIPSFD(mip), buf, sizeof(buf)) > 0 && pcnt < pexpect)
        {
            tdlog("%s", buf);
            if (strstr(buf, "Peer"))
                pcnt++;
        }
    }
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid,
                         "@(#) $RCSfile: tel.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.15 $ $Name:  $"};
