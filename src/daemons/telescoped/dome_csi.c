/* handle the dome and shutter.
 *
 * functions that respond directly to fifos begin with dome_.
 * middle-layer support functions begin with d_ or s_.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "strops.h"
#include "telstatshm.h"
#include "running.h"
#include "rot.h"
#include "misc.h"
#include "telenv.h"
#include "csimc.h"
#include "virmc.h"
#include "cliserv.h"
#include "tts.h"

#include "teled.h"  // will bring in buildcfg.h

// this enable this for certain real time tracing logs to be emitted
#ifndef VERBOSE_DOME_LOG
#define VERBOSE_DOME_LOG 0
#endif

#ifndef TTY_DOME
#define TTY_DOME 	0		// this flag enables TTY based dome controller (BSGC)
#endif

#if TTY_DOME
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

// useful for debugging with a single node for testing scripts, etc.  Normally set to "is_virtual_mode() virtual_mode"
//#define is_virtual_mode() 0
#define is_virtual_mode()   virtual_mode


#define DBLOG(msg) tdlog("::::DBLOG: %s",msg)

/* config entries */
int	DOMEAXIS = -1;
static double	DOMETO;
static double	DOMETOL;
static double	DOMEZERO;
static double	DOMESTEP;
static int	DOMESIGN;
static double	DOMEMOFFSET;
static double	SHUTTERTO;
static double	SHUTTERAZ;
static double	SHUTTERAZTOL;
static int	MOTORONLY;

#if TTY_DOME
// TTY Control specific
static char DOMETTY[32] = "/dev/ttyS1";
#define SPEED B9600
static int domeFD;

static int openDomeTTY(void);
static int domeTTYReady(void);
static int periodicPoll(void);
static int domeTTYResult(char *buf, int size);

static void domeTTYRoof(int dir);
static void domeTTYSeek(double az);
static void domeTTYRotate(int dir);
static void domeTTYStop(void);
static double domeTTYReadPos(void);

#endif

static double	dome_to;	/* mjd when operation will timeout */

/* the current activity, if any */
static void (*active_func) (int first, ...);

/* one of these... */
static void csi_dome_poll(void);
static void csi_dome_reset(int first, ...);
static void csi_dome_open(int first, ...);
static void csi_dome_close(int first, ...);
static void csi_dome_autoOn(int first, ...);
static void csi_dome_autoOff(int first, ...);
static void csi_dome_home(int first, ...);
static void csi_dome_setaz(int first, ...);
static void csi_dome_stop(int first, ...);
static void csi_dome_status (int first, ...); //IEEC
static void csi_dome_jog (int first, ...);
static void csi_dome_readpos(int first, ...);
/* helped along by these... */
static int csi_d_emgstop(char *msg);
static int csi_d_chkWx(char *msg);
static void csi_d_auto (void);
static double csi_d_telaz(void);
static void csi_d_cw(void);
static void csi_d_ccw(void);
static void csi_d_stop(void);
static void csi_d_readpos2(void);
static void csi_initCfg(void);
static void csi_openChannels(void);
static void csi_closeChannels(void);

/* later entries */
static int csi_d_goShutterPower(void);
static char * csi_doorType(void);
static char * csi_enclType(void);
static int setaz_error = 0;	// set if there is an error during dome_setaz, used by goShutterPower

/* handy shortcuts */
#define	DS		(telstatshmp->domestate)
#define	SS		(telstatshmp->shutterstate)
#define	AD		(telstatshmp->autodome)
#define	AZ		(telstatshmp->domeaz)
#define	TAZ		(telstatshmp->dometaz)
#define	SMOVING		(SS == SH_OPENING || SS == SH_CLOSING)
#define	DMOVING		(DS == DS_ROTATING || DS == DS_HOMING)
#define	DHAVE		(DS != DS_ABSENT)
#define	SHAVE		(SS != SH_ABSENT)

/* control and status connections */
static int cfd =0, sfd = 0;

/* called when we receive a message from the Dome fifo plus periodically with
 *   !msg to just update things.
 */
/* ARGSUSED */
void
csi_dome_msg (msg)
char *msg;
{
    char jog_dir[2];
    double az;

    /* do reset before checking for `have' to allow for new config file */
    if (msg && strncasecmp (msg, "reset", 5) == 0) {
    	csi_dome_reset(1);
        return;
    }

    /* worth it? */
    if (!DHAVE && !SHAVE) {
        if (msg)
            fifoWrite (Dome_Id, 0, "Ok, but dome really not installed");
        return;
    }

#if !TTY_DOME
    /* setup? */
	if (virtual_mode)
	{
		if (!DOMEAXIS == -1 && msg)
		{
			tdlog("Dome command before initial Reset: %s", msg ? msg : "(NULL)");
			return;
		}
	}
	else
	{
		if (!cfd && msg)
		{
			tdlog("Dome command before initial Reset: %s", msg ? msg : "(NULL)");
			return;
		}
	}
#endif

    /* top priority are emergency stop and weather alerts */
    if (csi_d_emgstop(msg) || csi_d_chkWx(msg))
        return;

    /* handle normal messages and polling */
    if (!msg)
    	csi_dome_poll();
    else if (strncasecmp (msg, "stop", 4) == 0)
    	csi_dome_stop(1);
    else if (strncasecmp (msg, "open", 4) == 0)
    	csi_dome_open(1);
    else if (strncasecmp (msg, "close", 5) == 0)
    	csi_dome_close(1);
    else if (strncasecmp (msg, "auto", 4) == 0)
    	csi_dome_autoOn(1);
    else if (strncasecmp (msg, "off", 3) == 0)
    	csi_dome_autoOff(1);
    else if (strncasecmp (msg, "home", 4) == 0)
    	csi_dome_home(1);
    else if (strncasecmp (msg, "status", 6) == 0) //IEEC
    	csi_dome_status(1);                       //IEEC
    else if (sscanf (msg, "Az:%lf", &az) == 1)
    	csi_dome_setaz (1, az);
    else if (sscanf (msg, "j%1[0+-]", jog_dir) == 1)
    	csi_dome_jog (1, jog_dir[0]);
    else {
        fifoWrite (Dome_Id, -1, "Unknown command: %.20s", msg);
        csi_dome_stop (1);	/* default for any unrecognized message */
    }

}

static double nextreadmjd;
#define POLL_DELAY (1.5/SPD)

/* maintain current action */
static void
csi_dome_poll ()
{
    Now *np = &telstatshmp->now;

    if (is_virtual_mode()) {
        if (DHAVE || SHAVE) {
            vmcService(DOMEAXIS);
        }
    }

    if (active_func)
        (*active_func)(0);

    if (DHAVE)
    {
        if (AD) csi_d_auto();
        if (mjd > nextreadmjd)
        {
            if (active_func)
            {
            	csi_d_readpos2();
            }
            else
            {
            	csi_dome_readpos(1);
            }
            nextreadmjd = mjd + POLL_DELAY;
        }
    }
}

#define RESET_DELAY (5/SPD)
static double reset_time;
/* read config files, stop dome; don't mess much with shutter state */
static void
csi_dome_reset (int first, ...)
{
    Now *np = &telstatshmp->now;

    if (first) {
        ++setaz_error;
        csi_initCfg();

        if (is_virtual_mode()) { // set the virtual dome controller up
            vmcSetup(DOMEAXIS,.2,.1,DOMESTEP,DOMESIGN);
        }

        if (DHAVE || SHAVE) {
        	csi_openChannels();
//			d_stop();
            if (DHAVE) {
                dome_to = mjd + DOMETO;
            }
            if (SHAVE && SMOVING) {
                SS = SH_IDLE;
            }
            reset_time = 0;
            active_func = csi_dome_reset;
        } else {
        	csi_closeChannels();
        }
        return;
    }

    if (DHAVE) {
        if (reset_time == 0)
        {
            reset_time = mjd + RESET_DELAY;
            dome_to = mjd + DOMETO;
        }

        if (mjd > dome_to) {
            fifoWrite (Dome_Id, -2, "Reset timed out");
            active_func = NULL;
            return;
        }

    }

    active_func = NULL;
    if (DHAVE || SHAVE)
    {

        if (!is_virtual_mode()) {
#if !TTY_DOME
            // -- Version 1.5 or greater of nodeDome.cmc --//
            // set the encoder steps (used by script!) to value in cfg file
            if (MOTORONLY) {
                csi_w (cfd, "msteps=%.0f;\n",DOMESTEP);
            } else {
                MotorInfo* mip = OMOT;
                if (mip->have && mip->axis == DOMEAXIS)
                {
                    fifoWrite(Dome_Id, 1, "Setting shared focus reset parameters");
                    double scale = mip->step/(2*PI);
                    if (mip->homelow)
                    {
                        csi_w(cfd, "ipolar | homebit;\n");
                    } else {
                        csi_w(cfd, "ipolar &= homebit;\n");
                    }
                    csi_w(cfd, "maxvel=%.0f; maxacc=%.0f; limacc=%.0f; msteps=%d;\n",
                          mip->maxvel*scale,mip->maxacc*scale,mip->slimacc*scale,mip->step);
                }
                csi_w (cfd, "esteps=%.0f;\n",DOMESTEP);
            }
            // set the sign variable in the script to the sign we use here
            csi_w (cfd, "s=%d;\n",DOMESIGN);
            // set the dome timeout variable in the script if not set by the script itself
            csi_w (cfd, "w=w?w:%.0f;\n",DOMETO * SPD * 1000);
            // set the roof open/close times if not set by the script itself
            csi_w (cfd, "r=r?r:%.0f\n;",SHUTTERTO * SPD * 750.0); // 3/4 of timeout
            csi_w (cfd, "v=v?v:%.0f\n;",SHUTTERTO * SPD * 750.0); // 3/4 of timeout
            // set the roof open/close timeouts
            csi_w (cfd, "t=t?t:%.0f\n;",SHUTTERTO * SPD * 1000.0); // full timeout
            csi_w (cfd, "u=u?u:%.0f\n;",SHUTTERTO * SPD * 1000.0); // full timeout

            fifoWrite(Dome_Id, 0, "Reset complete");
            setaz_error = 0;
//            d_stop();

           	int status;
        	status = csi_rix(cfd, "domeStatus();");
        	if (status == 0) SS = SH_CLOSED;
        	else if (status == 1) SS = SH_OPEN;
        	else if (status == 2) SS = SH_CLOSING;
        	else if (status == 3) SS = SH_OPENING;
        	else SS = SH_OPENING;
#endif
        }
    }
    else
        fifoWrite (Dome_Id, 0, "Not installed");

}

/* move dome to shutter power position before opening or closing

	return true (1) if OK to open / close shutter
	return false (0) if still moving there, or otherwise unable to activate door

*/
static int csi_d_goShutterPower(void)
{
    // define temp holder for active_func pointer
    void (*af_hold) (int first, ...);
    double shtdif;

    /* get dome in place first, if have one */
    if (!DHAVE) return 1; // go ahead if we're just a roof...
    if (!SHUTTERAZ && !SHUTTERAZTOL) // these are defined as zero if not needed for dome type
        return 1; // go ahead and open it

    // check on current situation
    if (DS == DS_STOPPED) {
    	csi_d_readpos2();   // STO: 11-19-09... need to have current read in order for this to work!
        //tdlog("Detected as stopped, checking diff where AZ=%g and SHUTTERAZ=%g",AZ,SHUTTERAZ);
        shtdif = SHUTTERAZ - AZ;
        if (shtdif < 0) shtdif = -shtdif;
        //tdlog("Diff is %g, tolerance is %g",shtdif, SHUTTERAZTOL);
        if (shtdif <= SHUTTERAZTOL)
        {
            //tdlog("Less than tolerance, returning 1");
            return 1; // we're there!
        }

        fifoWrite(Dome_Id, 1, "Aligning Dome for %s power", csi_doorType());

        // save and replace current active function so we don't forget what we're doing
        af_hold = active_func;
        // move us there
        csi_dome_setaz(1, SHUTTERAZ);
        // back to waiting for shutter power position
        active_func = af_hold;

    }
    else
    {
        // save and replace current active function so we don't forget what we're doing
        af_hold = active_func;
        // move us there
        csi_dome_setaz(0);
        // back to waiting for shutter power position
        active_func = af_hold;
    }

    // If there was an error while turning, just bail out here....
    if (setaz_error)
        active_func = NULL;

    return (0); // we're in process of doing something... wait for stop to reassess.
}

/* Some terminology about door... if we have a dome, it's a shutter, if not it's a roof...*/
static char * csi_doorType(void)
{
    if (DHAVE)	return ("shutter");
    else		return ("roof");
}
static char * csi_enclType(void)
{
    if (DHAVE)	return("dome");
    else		return("roof");
}

/* open shutter or roof */
static void
csi_dome_open (int first, ...)
{
    Now *np = &telstatshmp->now;
    char buf[1024];
    int n;

    /* nothing to do if no shutter */
    if (!SHAVE) {
        fifoWrite (Dome_Id, -3, "No %s to open", csi_doorType());
        return;
    }

    if (first) {
        /* set new state */
        dome_to = mjd + SHUTTERTO;
        active_func = csi_dome_open;
        AD = 0;
    }

    // If we need to rotate to power position first, do so
    if (!csi_d_goShutterPower()) {
        return;
    }

    /* initiate open if not under way */
    if (SS != SH_OPENING) {
        if (is_virtual_mode()) {
            vmc_w(DOMEAXIS,"roofseek(1);");
        } else {
#if TTY_DOME
            domeTTYRoof(1);
#else
            csi_w (cfd, "roofseek(1);");
#endif
        }
        SS = SH_OPENING;
        fifoWrite (Dome_Id, 2, "Starting open");
        toTTS ("The %s is now opening.", csi_doorType());
        active_func = csi_dome_open;
        return;
    }

    /* check for time out */
    if (mjd > dome_to) {
        fifoWrite (Dome_Id, -5, "Open timed out");
        toTTS ("Opening of %s timed out.", csi_doorType());
        csi_d_stop();
        SS = SH_IDLE;
        active_func = NULL;
        return;
    }

    /* check progress */
    if (is_virtual_mode()) {
        if (!vmc_isReady(DOMEAXIS))
            return;
        if (vmc_r(DOMEAXIS, buf, sizeof(buf)) <=0)
            return;
    } else {
#if TTY_DOME
        //if(!domeTTYReady())
        if (!periodicPoll())
            return;
        if (domeTTYResult(buf,sizeof(buf)) < 0)
            return;
#else
        if (!csiIsReady(cfd))
            return;
        if (csi_r (cfd, buf, sizeof(buf)) <= 0)
            return;
#endif
    }
    if (!buf[0])
        return;
    n = atoi(buf);
    if (n == 0 && buf[0] != '0') {
        /* consider no leading number a bug in the script */
        tdlog ("Bogus roofseek() string: '%s'", buf);
        n = -1;
    }
    if (n < 0) {
    	csi_d_stop();
        fifoWrite (Dome_Id, n, "Open error: %s", buf+2); /* skip -n */
        toTTS ("Error opening %s: %s", csi_doorType(), buf+2);
        SS = SH_IDLE;
        active_func = NULL;
        csi_dome_stop(1);
        return;
    }
    if (n > 0) {
        fifoWrite (Dome_Id, n, "%s", buf+1); /* skip n */
        toTTS ("Progress of %s: %s", csi_doorType(), buf+1);
        return;
    }

    /* ok! */
    fifoWrite (Dome_Id, 0, "Open complete");
    toTTS ("The %s is now open.", csi_doorType());
    SS = SH_OPEN;
    active_func = NULL;
}

/* close shutter or roof */
static void
csi_dome_close (int first, ...)
{
    Now *np = &telstatshmp->now;
    char buf[128];
    int n;

    /* nothing to do if no shutter */
    if (!SHAVE) {
        fifoWrite (Dome_Id, -3, "No %s to close", csi_doorType());
        return;
    }

    if (first) {
        /* set new state */
        dome_to = mjd + SHUTTERTO;
        active_func = csi_dome_close;
        AD = 0;
    }

    // If we need to rotate to power position first, do so
    if (!csi_d_goShutterPower()) {
        return;
    }

    /* initiate close if not under way */
    if (SS != SH_CLOSING) {
        if (is_virtual_mode()) {
            vmc_w(DOMEAXIS,"roofseek(-1);");
        } else {
#if TTY_DOME
            domeTTYRoof(-1);
#else
            csi_w (cfd, "roofseek(-1);");
#endif
        }
        SS = SH_CLOSING;
        fifoWrite (Dome_Id, 2, "Starting close");
        toTTS ("The %s is now closing.", csi_doorType());
        active_func = csi_dome_close;
        return;
    }

    /* check for time out */
    if (mjd > dome_to) {
        fifoWrite (Dome_Id, -5, "Close timed out");
        toTTS ("Closing of %s timed out.", csi_doorType());
        csi_d_stop();
        SS = SH_IDLE;
        active_func = NULL;
        return;
    }

    /* check progress */
    if (is_virtual_mode()) {
        if (!vmc_isReady(DOMEAXIS))
            return;
        if (vmc_r(DOMEAXIS, buf, sizeof(buf)) <=0)
            return;
    } else {
#if TTY_DOME
        //if(!domeTTYReady())
        if (!periodicPoll())
            return;
        if (domeTTYResult(buf,sizeof(buf)) < 0)
            return;
#else
        if (!csiIsReady(cfd))
            return;
        if (csi_r (cfd, buf, sizeof(buf)) <= 0)
            return;
#endif
    }
    if (!buf[0])
        return;
    n = atoi(buf);
    if (n == 0 && buf[0] != '0') {
        /* consider no leading number a bug in the script */
        tdlog ("Bogus roofseek() string: '%s'", buf);
        n = -1;
    }
    if (n < 0) {
    	csi_d_stop();
        fifoWrite (Dome_Id, n, "Close error: %s", buf+2); /* skip -n */
        toTTS ("Error closing %s: %s", csi_doorType(), buf+2);
        SS = SH_IDLE;
        active_func = NULL;
        csi_dome_stop(1);
        return;
    }
    if (n > 0) {
        fifoWrite (Dome_Id, n, "%s", buf+1); /* skip n */
        toTTS ("Progress of %s: %s", csi_doorType(), buf+1);
        return;
    }

    /* ok! */
    fifoWrite (Dome_Id, 0, "Close complete");
    toTTS ("The %s is now closed.", csi_doorType());
    SS = SH_CLOSED;
    active_func = NULL;
}

/* activate autodome lock */
/* ARGSUSED */
static void
csi_dome_autoOn (int first, ...)
{
    if (!DHAVE) {
        fifoWrite (Dome_Id, 0, "Ok, but no dome really");
    } else {
        /* just set the flag, let poll do the work */
        AD = 1;
        fifoWrite (Dome_Id, 0, "Auto dome on");
    }
}

/* deactivate autodome lock */
/* ARGSUSED */
static void
csi_dome_autoOff (int first, ...)
{
    if (!DHAVE) {
        fifoWrite (Dome_Id, 0, "Ok, but no dome really");
    } else {
        /* just stop and reset the flag */
        AD = 0;
        csi_d_stop();
        fifoWrite (Dome_Id, 0, "Auto dome off");
    }
}

/* find dome home */
static void
csi_dome_home (int first, ...)
{
    Now *np = &telstatshmp->now;
    char buf[128];
    int n;

    if (!DHAVE) {
        fifoWrite (Dome_Id, 0, "Ok, but really no dome to home");
        return;
    }

    if (first) {

        /* start moving towards desired side of home */
        if (is_virtual_mode()) {
            vmc_w(DOMEAXIS,"finddomehome();");
        } else {
#if TTY_DOME
            // no homing necessary for this dome
#else
            csi_w (cfd, "finddomehome();");
#endif
        }

        /* set timeout and new state */
        dome_to = mjd + DOMETO;
        active_func = csi_dome_home;
        DS = DS_HOMING;
        AD = 0;
        TAZ = DOMEZERO;
        toTTS ("The dome is seeking the home position.");
    }

    /* check for time out */
    if (mjd > dome_to) {
        fifoWrite (Dome_Id, -5, "Home timed out");
        toTTS ("Dome home timed out.");
        csi_d_stop();
        DS = DS_STOPPED;
        active_func = NULL;
        return;
    }

    /* check progress */
    if (is_virtual_mode()) {
        if (!vmc_isReady(DOMEAXIS))
            return;
        if (vmc_r(DOMEAXIS, buf, sizeof(buf)) <=0)
            return;
    } else {
#if TTY_DOME
        // no homing necessary for this dome
        strcpy(buf,"0: homed\n");
#else
        if (!csiIsReady(cfd))
            return;
        if (csi_r (cfd, buf, sizeof(buf)) <= 0)
            return;
#endif
    }
    if (!buf[0])
        return;
    n = atoi(buf);
    if (n == 0 && buf[0] != '0') {
        /* consider no leading number a bug in the script */
        tdlog ("Bogus finddomehome() string: '%s'", buf);
        n = -1;
    }
    if (n < 0) {
    	csi_d_stop();
        fifoWrite (Dome_Id, n, "Home error: %s", buf+2); /* skip -n */
        toTTS ("Dome home error. %s", buf+2);
        DS = DS_STOPPED;
        active_func = NULL;
        csi_dome_stop(1);
        return;
    }
    if (n > 0) {
        fifoWrite (Dome_Id, n, "%s", buf+1); /* skip n */
        toTTS ("Dome progress. %s", buf+1);
        return;
    }

    /* ok! */
    fifoWrite (Dome_Id, 0, "Home complete");
    toTTS ("The Dome is now home.");
    DS = DS_STOPPED;
    active_func = NULL;
}


/* get dome status (introduced by IEEC) */
static void
csi_dome_status (int first, ...)
{
  	int status = -1;
    int pos;
    double daz;
    char *  rixRead = "=epos;";

    if (first) 
    {
        if (DHAVE || SHAVE)
        {
            if (!is_virtual_mode()) 
            {
#if !TTY_DOME
                // -- Version 1.5 or greater of nodeDome.cmc --//
            	status = csi_rix(cfd, "domeStatus();");
            	if (status < 0 || status > 3) 
                    fifoWrite(Dome_Id, -1, "Error retrieving shutter status");
                if (DHAVE)
                {
                    status = csi_rix(cfd, "isDomeHomed();");
                    if(status==0)
                        fifoWrite(Dome_Id, 0, "Dome orientation is unknown");
                    else if (status==1)
					{
                        if (MOTORONLY)
                            rixRead = "=mpos;";
						pos = csi_rix(cfd,rixRead) * DOMESIGN;
						daz = (2* PI * pos / DOMESTEP) + DOMEZERO;
						range(&daz, 2*PI);
                        fifoWrite(Dome_Id, 1, "Dome orientation is %g",daz);
                    }
            	    else
                        fifoWrite(Dome_Id, -1, "Error retrieving dome orientation");
                }
#endif
            }
    		else
            {
                status = vmc_isReady(DOMEAXIS);
                if(status==0)
                    fifoWrite(Dome_Id, 0, "Dome is unresponsive");
                else if (status==1)
                    fifoWrite(Dome_Id, 1, "Dome is ready");
         	    else
                    fifoWrite(Dome_Id, -1, "Error retrieving dome status");
            }
        }
        else
            fifoWrite (Dome_Id, -1, "No dome installed");
    }
    return;
}


/* move to the given azimuth. also turns off Auto mode. */
static void
csi_dome_setaz (int first, ...)
{
    Now *np = &telstatshmp->now;
    char buf[128];
    int n;
    setaz_error = 0; // reset any previous error flag

    /* nothing to do if no dome */
    if (!DHAVE) {
        fifoWrite (Dome_Id, -10, "No dome to turn");
        ++setaz_error;
        return;
    }

    if (first) {
        va_list ap;
        double taz;
        long tenc, tol;

        /* fetch new target az */
        va_start (ap, first);
        taz = va_arg (ap, double);
        va_end (ap);

        /* issue command */
        range (&taz, 2*PI);
        TAZ = taz;

        /* sto: Must offset by DOMEZERO for this to make sense */
        taz -= DOMEZERO;

        tenc = DOMESIGN * DOMESTEP*taz/(2*PI);
        tol = DOMESTEP*DOMETOL/(2*PI);
        /* DEBUG CHECK */
        tdlog("SETAZ:  taz = %g  tenc = %ld  tol = %ld\n",taz,tenc,tol);
        /**/
        if (is_virtual_mode()) {
            sprintf(buf,"domeseek(%ld,%ld);", tenc, tol);
            vmc_w(DOMEAXIS,buf);
        } else {
#if TTY_DOME
            domeTTYSeek(taz);
#else
            csi_w (cfd, "domeseek(%ld,%ld);", tenc, tol);
#endif
        }
        /* set state */
        AD = 0;
        dome_to = mjd + DOMETO;
        active_func = csi_dome_setaz;
        toTTS ("The dome is rotating towards the %s.", cardDirLName (TAZ));
        DS = DS_ROTATING;
    }

    /* check for time out */
    if (mjd > dome_to) {
        fifoWrite (Dome_Id, -5, "Azimuth timed out");
        toTTS ("Roof azimuth command timed out.");
        csi_d_stop();
        DS = DS_STOPPED;
        active_func = NULL;
        ++setaz_error;
        return;
    }
    /* check progress */
    if (is_virtual_mode()) {
        if (!vmc_isReady(DOMEAXIS))
            return;
        if (vmc_r(DOMEAXIS, buf, sizeof(buf)) <=0)
            return;
    } else {
#if TTY_DOME
        //if(!domeTTYReady())
        if (!periodicPoll())
            return;
        if (domeTTYResult(buf,sizeof(buf)) < 0)
            return;
#else
        if (!csiIsReady(cfd))
            return;
        if (csi_r (cfd, buf, sizeof(buf)) <= 0)
            return;
#endif
    }
    if (!buf[0])
        return;
    n = atoi(buf);
    if (n == 0 && buf[0] != '0') {
        /* consider no leading number a bug in the script */
        tdlog ("Bogus domeseek() string: '%s'", buf);
        n = -1;
    }
    if (n < 0) {
    	csi_d_stop();
        fifoWrite (Dome_Id, n, "Az error: %s", buf+2); /* skip -n */
        toTTS ("Dome azimuth error. %s", buf+2);
        DS = DS_STOPPED;
        active_func = NULL;
        ++setaz_error;
        csi_dome_stop(1);
        return;
    }
    if (n > 0) {
        fifoWrite (Dome_Id, n, "%s", buf+1); /* skip n */
        toTTS ("Dome progress. %s", buf+1);
        return;
    }

    /* ok! */
    fifoWrite (Dome_Id, 0, "Azimuth command complete");
    toTTS ("The dome is now pointing to the %s.", cardDirLName (AZ));
    DS = DS_STOPPED;
    active_func = NULL;
    setaz_error = 0;
}

/* stop everything */
static void
csi_dome_stop (int first, ...)
{
    Now *np = &telstatshmp->now;

    if (!DHAVE && !SHAVE) {
        fifoWrite (Dome_Id, 0, "Ok, but nothing to stop really");
        return;
    }

    if (first) {
    	csi_d_stop();
        dome_to = mjd + DOMETO;
        active_func = csi_dome_stop;
        AD = 0;
#if TTY_DOME
        domeTTYRotate(0);
#endif
    }

    if (mjd > dome_to) {
        fifoWrite (Dome_Id, -5, "Stop timed out");
        toTTS ("Stop of %s timed out.",csi_enclType());
        csi_d_stop();	/* ?? */
        DS = DS_STOPPED;
        AD = 0;
        TAZ = AZ;
        active_func = NULL;
        return;
    }

    active_func = NULL;
    fifoWrite (Dome_Id, 0, "Stop complete");
    DS = DS_STOPPED;
    TAZ = AZ;
    toTTS ("The %s is now stopped.",csi_enclType());
}

/* jog: + means CW, - means CCW, 0 means stop */
static void
csi_dome_jog (int first, ...)
{
    char dircode;

    if (!DHAVE) {
        fifoWrite (Dome_Id, -13, "No Dome to jog");
        return;
    }

    if (first) {
        va_list ap;

        /* fetch direction code */
        va_start (ap, first);
        //dircode = va_arg (ap, char);
        dircode = va_arg (ap, int);  // char is promoted to int, so pass int...
        va_end (ap);

        /* no more AD */
        AD = 0;

        /* do it */
        switch (dircode) {
        case '+':
            fifoWrite (Dome_Id, 5, "Paddle command CW");
            toTTS ("The dome is rotating clockwise.");
            csi_d_cw();
            active_func = csi_dome_jog;
            DS = DS_ROTATING;
            break;
        case '-':
            fifoWrite (Dome_Id, 6, "Paddle command CCW");
            toTTS ("The dome is rotating counter clockwise.");
            csi_d_ccw();
            active_func = csi_dome_jog;
            DS = DS_ROTATING;
            break;
        case '0':
            fifoWrite (Dome_Id, 7, "Paddle command stop");
            csi_d_stop();
            active_func = NULL;
            DS = DS_STOPPED;
            break;
        default:
            fifoWrite (Dome_Id, -14, "Bogus jog code: %c", dircode);
            active_func = NULL;
            csi_dome_stop(1);
            break;
        }

        return;
    }
}

/* middle-layer support functions */

/* check the emergency stop bit.
 * while on, stop everything and return 1, else return 0
 */
static int
csi_d_emgstop(char *msg)
{

    /* NOTE: History here is that "roofestop" calls made this frequently
       cause a problem with the CSI interface / buffer flow.
       Emergency stop detection is disabled in this version.
       This has not been thoroughly revisited -- may be able to make work
    */

    int on;

    on = (DMOVING || SMOVING);
    if (!on) {
        return(0); // don't check estop if not moving
    }

    // this will set a variable (e) that we will subsequently read
//	csi_w (cfd, "roofestop();");
//	on &= csi_rix(sfd, "=e;");

    on = 0;

    if (!on) {
        return (0);
    }

    if (msg || (active_func && active_func != csi_dome_stop))
        fifoWrite (Dome_Id,
                   -15, "Command cancelled.. emergency stop is active");

    if (active_func != csi_dome_stop && DS != DS_STOPPED) {
        fifoWrite (Dome_Id, 8, "Emergency stop asserted -- stopping %s",csi_enclType());
        AD = 0;
        csi_dome_stop (1);
    }

    csi_dome_poll();

    return (1);
}

/* if a weather alert is in progress respond and return 1, else return 0 */
static int
csi_d_chkWx(char *msg)
{
    WxStats *wp = &telstatshmp->wxs;
    int wxalert;

    if (wp != NULL) {
        wxalert = (time(NULL) - wp->updtime < 30) && wp->alert;
    } else {
        wxalert = 0;
    }

    if (!wxalert || !SHAVE)
        return(0);

    if (msg || (active_func && active_func != csi_dome_close))
        fifoWrite (Dome_Id,
                   -16, "Command cancelled.. weather alert in progress");

    if (active_func != csi_dome_close && SS != SH_CLOSED) {
        fifoWrite (Dome_Id, 9, "Weather alert asserted -- closing %s",csi_doorType());
        AD = 0;
        csi_dome_close (1);
    }

    csi_dome_poll();

    return (1);
}

/* read and update the current position */
static void
csi_dome_readpos (int first, ...)
{
    Now *   np = &telstatshmp->now;
    char    buf[1024];
    double  az;
    int     pos;
//    char *  rixRead = "=x;";
    char *  rixRead = "=epos;";

    if (MOTORONLY)
    {
        rixRead = "=mpos;";
    }

    if (!DHAVE && !SHAVE)
    {
        fifoWrite (Dome_Id, 0, "Ok, but nothing to read really");
        return;
    }

    if (is_virtual_mode())
    {
        pos = vmc_rix(sfd, rixRead) * DOMESIGN;
        active_func = NULL;
        az = (2*PI)*pos/DOMESTEP + DOMEZERO;
        range (&az, 2*PI);
        AZ = az;
        return;
    }

    if (first)
    {
        dome_to = mjd + DOMETO;
        active_func = csi_dome_readpos;
#if !TTY_DOME
        csi_w (cfd, rixRead);
#endif
    }

    if (mjd > dome_to)
    {
        //fifoWrite (Dome_Id, -5, "ReadPos timed out");
        active_func = NULL;
        return;
    }

    /* check progress */
#if TTY_DOME
    az = domeTTYReadPos();
#else
    if (!csiIsReady(cfd))
        return;
    if (csi_r (cfd, buf, sizeof(buf)) <= 0)
        return;
#endif
    if (!buf[0])
        return;

    pos = atoi(buf);
    pos *= DOMESIGN;

    /* ok! */
    active_func = NULL;
    az = (2*PI)*pos/DOMESTEP + DOMEZERO;
    range (&az, 2*PI);
    AZ = az;
}

static void
csi_d_readpos2()
{
    char *  rixRead = "=epos;";

    if (MOTORONLY)
    {
        rixRead = "=mpos;";
    }
    int pos;
    if(is_virtual_mode())
    {
        pos = vmc_rix(sfd, rixRead);
    }
    else
    {
        pos = csi_rix(sfd, rixRead);
    }
    pos *= DOMESIGN;
    double az = (2*PI)*pos/DOMESTEP + DOMEZERO;
    range (&az, 2*PI);
    AZ = az;
}


// Put center point of dome slightly ahead of telescope target; let it then precess into it
#define LEADAZOFF 0.12
/* return the az the dome should be for the desired telescope information */
static double
csi_d_telaz()
{
#if 0 // this function does not work right... why do it this way anyway?

    TelAxes *tap = &telstatshmp->tax;	/* scope orientation */
    double Y;	/* scope's "dec" angle to scope's pole */
    double X;	/* scope's "ha" angle, canonical from south */
    double Z;	/* angle from zenith to scope pole = DT - lat */
    double p[3];	/* point to track */
    double Az;	/* dome az to be found */

    /* coord system: +z to zenith, +y south, +x west. */
    Y = PI/2 - (DMOT->dpos + tap->YC);
    X = (HMOT->dpos - tap->XP) + PI;
    Z = tap->DT - telstatshmp->now.n_lat;

    /* init straight up, along z */
    p[0] = p[1] = 0.0;
    p[2] = 1.0;

    /* translate along x by mount offset */
    p[0] += (tap->GERMEQ && tap->GERMEQ_FLIP) ? -DOMEMOFFSET : DOMEMOFFSET;

    /* rotate about x by -Y to affect scope's DMOT position */
//	rotx (p, -Y);
// sto: this is wrong.. Sign of Y is wrong
    rotx (p, Y);

    /* rotate about z by X to affect scope's HMOT position */
    rotz (p, X);

    /* rotate about x by Z to affect scope's tilt from zenith */
    rotx (p, Z);

    /* dome az is now projection onto xy plane, angle E of N */
    Az = atan2 (-p[0], -p[1]);
    if (Az < 0)
        Az += 2*PI;

    return (Az);
#endif

    /* TODO: account for domeoffset
       which is something the commented-out code above
       tried to do... but it's not needed in normal case of
       a single telescope in the center of the dome structure.
    */

    double raz;
    if (((telstatshmp->telstate != TS_SLEWING
       && telstatshmp->telstate != TS_HUNTING)
       || telstatshmp->jogging_ison)
       && (telstatshmp->Calt < 1.5)) // ignore current tracking if we're close to zenith (avoid dome twirls)
    {
	#if VERBOSE_DOME_LOG
        tdlog("Dome: current tele azimuth = %f (telstate = %d)\n",
                    telstatshmp->Caz,
                    telstatshmp->telstate);
	#endif
	raz = (telstatshmp->Caz);
    }
    else
    {
	#if VERBOSE_DOME_LOG
    	tdlog("Dome: desired tele azimuth = %f\n", telstatshmp->Daz);
	#endif

	raz = (telstatshmp->Daz);
	// offset by a little bit leading
	raz += LEADAZOFF;
    }
    return raz;
}

/* initiate a stop */
static void
csi_d_stop(void)
{
    if (!is_virtual_mode()) {
#if TTY_DOME
        domeTTYStop();
#else
        if (cfd) {
            csi_intr (cfd);
            csiDrain(cfd);
            csi_w (cfd, "dome_stop();");
            csiDrain(cfd);
            csi_w (cfd, "roofseek(0);");
            csiDrain(cfd);
        }
        if (sfd) {
            csi_intr(sfd);
            csiDrain(sfd);
        }
#endif
    } else {
        vmc_w(DOMEAXIS, "dome_stop();");
        vmc_w(DOMEAXIS, "roofseek(0);");
    }
}

/* start cw */
static void
csi_d_cw()
{
    if (is_virtual_mode()) {
        char buf[128];
        sprintf(buf,"domejog(%d);", DOMESIGN);
        vmc_w (DOMEAXIS, buf);
    } else {
#if TTY_DOME
        domeTTYRotate(1);
#else
        csi_w (cfd, "domejog(%d);", DOMESIGN);
#endif
    }
}

/* start ccw */
static void
csi_d_ccw()
{
    if (is_virtual_mode()) {
        char buf[128];
        sprintf(buf,"domejog(%d);", -DOMESIGN);
        vmc_w (DOMEAXIS, buf);
    } else {
#if TTY_DOME
        domeTTYRotate(-1);
#else
        csi_w (cfd, "domejog(%d);", -DOMESIGN);
#endif
    }
}

/* keep AZ within DOMETOL of telaz() */
static void
csi_d_auto()
{
//	char buf[128];
//	int	n;
//	double scale = DOMESTEP/(2*PI);
    double diff;

    // First make sure the door is open
    if (SHAVE) {
        if (SS != SH_OPEN) {
            if (SS != SH_OPENING) 			// if not actually opening
            {
                if (active_func == NULL) {	// and not aligning
                	csi_dome_open(1);			// go ahead and initiate the open
                }
            }
            AD = 1;	 // keep this turned on, as the opening process will turn it off in a couple places
            return;
        }
    }

    diff = TAZ - csi_d_telaz();
    if (diff < 0) diff = -diff;

#if VERBOSE_DOME_LOG
    tdlog("Auto: Dome is %s. Target: %7.4g  Actual: %7.4g  d_telaz: %7.4g  diff: %7.4g DOMETOL: %7.4g",
    		(DS !=DS_ROTATING) ? "STOPPED" : "ROTATING",
    		TAZ, AZ, d_telaz(),
    		diff, DOMETOL);
#endif

    if (DS != DS_ROTATING) {

        if (diff < DOMETOL)
        {
        	csi_d_stop();
            return; // already there
        }

        TAZ = csi_d_telaz();

        if (active_func != csi_dome_setaz)
        {
        	csi_d_stop();
        	csi_dome_setaz(1, TAZ);
        }
        AD = 1;  // keep turned on
    }

#if 0

    if (is_virtual_mode()) {
        char buf[128];
        sprintf(buf,"domeseek (%.0f, %.0f);", (TAZ-DOMEZERO)*DOMESIGN*scale,DOMETOL*scale);
        vmc_w(DOMEAXIS,buf);
    } else {
#if TTY_DOME
        domeTTYSeek(TAZ-DOMEZERO);
#else
        csi_w (cfd, "domeseek (%.0f, %.0f);", (TAZ-DOMEZERO)*DOMESIGN*scale,DOMETOL*scale);
#endif
    }
}

/* check progress */
if (is_virtual_mode()) {
    if (!vmc_isReady(DOMEAXIS))
        return;
    if (vmc_r(DOMEAXIS, buf, sizeof(buf)) <=0)
        return;
} else {
#if TTY_DOME
    //if(!domeTTYReady())
    if (!periodicPoll())
        return;
    if (domeTTYResult(buf,sizeof(buf)) < 0)
        return;
#else
    if (!csiIsReady(cfd))
        return;
    if (csi_r (cfd, buf, sizeof(buf)) <= 0)
        return;
#endif
}
if (!buf[0])
    return;
n = atoi(buf);

// error
if (n < 0) {
    d_stop();
    fifoWrite (Dome_Id, n, "Az error: %s", buf+2); /* skip -n */
    toTTS ("Dome azimuth error. %s", buf+2);
    DS = DS_STOPPED;
    active_func = NULL;
    dome_stop(1);
    return;
}

// we're moving
if (n > 0) {
    DS = DS_ROTATING;
    return;
}

// n == 0 : we've stopped
DS = DS_STOPPED;

// err.. bogus feedback!
if (buf[0] != '0') {
    /* consider no leading number a bug in the script */
    tdlog ("Bogus domeseek() string: '%s'", buf);
    d_stop();
    active_func = NULL;
    return;
}
#endif
}

/* (re) read the dome confi file */
static void
csi_initCfg()
{
#define NCSIDCFG   (sizeof(csidcfg)/sizeof(csidcfg[0]))

    static int DOMEHAVE;
    static int SHUTTERHAVE;

    static CfgEntry csidcfg[] = {
        {"DOMEHAVE",	CFG_INT, &DOMEHAVE},
        {"DOMEAXIS",	CFG_INT, &DOMEAXIS},
        {"DOMETO",		CFG_DBL, &DOMETO},
        {"DOMETOL",		CFG_DBL, &DOMETOL},
        {"DOMEZERO",	CFG_DBL, &DOMEZERO},
        {"DOMESTEP",	CFG_DBL, &DOMESTEP},
        {"DOMESIGN",	CFG_INT, &DOMESIGN},
        {"DOMEMOFFSET",	CFG_DBL, &DOMEMOFFSET},
        {"SHUTTERHAVE",	CFG_INT, &SHUTTERHAVE},
        {"SHUTTERTO",	CFG_DBL, &SHUTTERTO},
        {"SHUTTERAZ",	CFG_DBL, &SHUTTERAZ},
        {"SHUTTERAZTOL",CFG_DBL, &SHUTTERAZTOL},
    };

#define NCSIOPTCFG   (sizeof(csioptcfg)/sizeof(csioptcfg[0]))
    static CfgEntry csioptcfg[] = {
        {"MOTORONLY",CFG_INT, &MOTORONLY},
    };

    int n;

    /* read the file */
    n = readCfgFile (1, csidcfn, csidcfg, NCSIDCFG);
    if (n != NCSIDCFG) {
        cfgFileError (csidcfn, n, (CfgPrFp)tdlog, csidcfg, NCSIDCFG);
        die();
    }

    // read optional
    MOTORONLY = 0; // default to off (encoder assumed)
    readCfgFile(0, csidcfn, csioptcfg, NCSIOPTCFG);

    if (abs(DOMESIGN) != 1) {
        tdlog ("DOMESIGN must be +-1\n");
        die();
    }

    /* let user specify neg */
    range (&DOMEZERO, 2*PI);

    /* we want in days */
    DOMETO /= SPD;
    SHUTTERTO /= SPD;

    /* some effect shm -- but try not to disrupt already useful info */
    if (!DOMEHAVE) {
        DS = DS_ABSENT;
    } else if (DS == DS_ABSENT) {
    	csi_d_stop();
        DS = DS_STOPPED;
    }
    if (!SHUTTERHAVE)
        SS = SH_ABSENT;
    else if (SS == SH_ABSENT)
        SS = SH_IDLE;

    /* no auto any more */
    AD = 0;
}

/* make sure cfd and sfd are open else exit */
static void
csi_openChannels()
{
    if (!is_virtual_mode()) {
#if TTY_DOME
        tdlog("Opening TTY on port %s\n",DOMETTY);
        domeFD = openDomeTTY();
        if (domeFD <= 0) {
            tdlog ("Error opening dome TTY on port %s\n", DOMETTY);
            die();
        }
        tdlog("domeFD is %d\n",domeFD);
#else
        if (!cfd)
            cfd = csiOpen (DOMEAXIS);
        if (cfd < 0) {
            tdlog ("Error opening dome channel to addr %d\n", DOMEAXIS);
            exit(1);	/* die's allstop uses CSIMC too */
        }

        if (!sfd)
            sfd = csiOpen (DOMEAXIS);
        if (sfd < 0) {
            tdlog ("Error opening dome channel to addr %d\n", DOMEAXIS);
            exit(1);	// die's allstop uses CSIMC too
        }

#endif
    }
    else {
        vmcReset(DOMEAXIS);
        cfd = DOMEAXIS;
        sfd = DOMEAXIS;
    }
}

/* close cfd and sfd */
static void
csi_closeChannels()
{
    if (!is_virtual_mode()) {
#if !TTY_DOME
        if (cfd) {
            csiClose (cfd);
            cfd = 0;
        }

        if (sfd) {
            csiClose (sfd);
            sfd = 0;
        }

#endif
    }
    else {
        sfd = 0;
        cfd = 0;
    }
}

//------------------------------------------------------------------------------------

#if TTY_DOME

// data 0 bit masks
#define UNKNOWN_BITS                0x0060  // not sure what these are, but they appear to be on normally.
#define RW_AIRSEAL                  0x0008
#define RD_NOTREADY                 0x0004
#define WR_ALLRETURN RD_NOTREADY
#define RD_MANUAL                   0x0002
#define RW_EMG                      0x0001

// data 1 bit masks
#define RD_HISCREEN_UPLIMIT         0x8000
#define RD_HISCREEN_DOWNLIMIT       0x4000
#define RW_HISCREEN_UP              0x2000
#define RW_HISCREEN_DOWN            0x1000
#define RD_LOSCREEN_UPLIMIT         0x0800
#define RD_LOSCREEN_DOWNLIMIT       0x0400
#define RW_LOSCREEN_UP              0x0200
#define RW_LOSCREEN_DOWN            0x0100
#define RD_SLIT_OPEN_LIMIT          0x0080
#define RD_SLIT_CLOSE_LIMIT         0x0040
#define RW_SLIT_OPEN                0x0020
#define RW_SLIT_CLOSE               0x0010
#define RW_DOME_CW                  0x0002
#define RW_DOME_CCW                 0x0001
#define WD_DOME_GOABS		    0x0004

#define SHUTTER_OPEN_TEST			(RD_SLIT_OPEN_LIMIT | RD_HISCREEN_UPLIMIT | RD_LOSCREEN_DOWNLIMIT)
#define SHUTTER_CLOSE_TEST			(RD_SLIT_CLOSE_LIMIT /*| RD_HISCREEN_DOWNLIMIT | RD_LOSCREEN_UPLIMIT*/)

// Structure defining dome status communicated by Bisei dome controller
struct {
    unsigned short data0;   // RW_AIRSEAL, RD_NOTREADY / WR_ALLRETURN, RD_MANUAL, RW_EMG
    unsigned short data1;   // RD_HISCREEN_UPLIMIT, RD_HISCREEN_DOWNLIMIT, RW_HISCREEN_UP, RW_HISCREEN_DOWN,
    // RD_LOSCREEN_UPLIMIT, RD_LOSCREEN_DOWNLIMIT, RW_LOSCREEN_UP, RW_LOSCREEN_DOWN
    // RD_SLIT_OPEN_LIMIT, RD_SLIT_CLOSE_LIMIT, RW_SLIT_OPEN, RW_SLIT_CLOSE, RW_DOME_CW, RW_DOME_CCW
    unsigned short domePos; // degrees * 10 representing dome position (R/W)
    unsigned short hiscreenPos; // hiscreen position * 10 (R/W)
    unsigned short loscreenPos; // loscreen position * 10 (R/W)

} domeStatus, domeCmdStatus;

// NOTE: Dome position is 180 degrees off... dome is pointing south, not north.

static int doingWhat; // tells us what action we're currently doing
enum {DO_NOTHING=0,DO_WAIT_ROOF_OPEN,DO_WAIT_ROOF_CLOSE,DO_WAIT_SEEK,DO_ROTATE};
static int commandActive = 0;
                           static int pollIsValid = 0;
                                                    static int targetPos;

#define DEBUG 0
#if DEBUG
#define TRACE tdlog
#else
                                                    void TRACE_EATX(char * fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    va_end (ap);
}
#define TRACE TRACE_EATX
#endif

                                                    static int output = 0; // debug output control

// local to here
                                                                        static void commandDome(void);
                                                                        static int sendDomeCommand(char *cmd);
                                                                        static int getDomeResponse(char *buf,int retsize);
                                                                        static int parseDomeResult(char *buf);
//static void domeSeekDir(void);
                                                                        static int domePosAdjust(int domePos);
                                                                        static int truePosAdjust(int desiredPos);

                                                                        /* open connection to dome or exit */
                                                                        static int openDomeTTY()
{
    struct termios tio;
    int fd;

    fd = open (DOMETTY, O_RDWR|O_NONBLOCK);
    if (fd < 0) {
        tdlog ("open(%s): %s\n", DOMETTY, strerror(errno));
        return -1;
    }
    fcntl (fd, F_SETFL, 0);	/* nonblock back off */

    memset (&tio, 0, sizeof(tio));
    tio.c_cflag = CS8 | CREAD | CLOCAL;
    tio.c_iflag = IGNBRK;
    tio.c_cc[VMIN] = 0;		/* start timer when call read() */
    tio.c_cc[VTIME] = ACKWT/100;	/* wait up to n 1/10ths seconds */
    cfsetospeed (&tio, SPEED);
    cfsetispeed (&tio, SPEED);
    if (tcsetattr (fd, TCSANOW, &tio) < 0) {
        tdlog ("tcsetattr(%s): %s\n", DOMETTY, strerror(errno));
        return -1;
    }

    return fd;

}

static int sendDomeCommand(char *cmd)
{
    int length,wrote;

    if (domeFD <=0) {
        TRACE("domeFD not set\n");
        return -1;
    }


    if (commandActive) {
        TRACE("command active\n");
        return -1;
    }

    commandActive = 1;

    // flush before sending new command
    (void) tcflush (domeFD, TCIOFLUSH);

    length = strlen(cmd);
    wrote = write(domeFD,cmd,length);
//   TRACE("DomeTTY: Wrote (%d/%d) %s\n",wrote,length,cmd);
    if (wrote == -1) {
        tdlog("Error: %s",strerror(errno));
    }

    pollIsValid = 0; // invalidate responses until we get a fresh one

    return wrote;
}

static void commandDome()
{
    char cmd[128];
    char cmd2[128];
    int i,fcs;

    sprintf(cmd,"@00WD0010%04X%04X%04X%04X%04X",
            domeCmdStatus.data0,domeCmdStatus.data1,domeCmdStatus.domePos,
            domeCmdStatus.hiscreenPos,domeCmdStatus.loscreenPos);

    // compute checksum
    fcs = 0;
    for (i=0; i<strlen(cmd); i++) {
        fcs ^= cmd[i];
    }

    sprintf(cmd2,"%s%02X*\r\n",cmd,fcs);

    TRACE("Sending: %s",cmd2);
    sendDomeCommand(cmd2);
}

static int getDomeResponse(char *buf,int retsize)
{
    int rb,count;

// TRACE("getDomeResponse()");

    if (domeFD <=0) {
        TRACE("domeFD not set\n");
        return -1;
    }

    if (!commandActive) {
        TRACE("Reading response when no command is pending\n");
    }
    commandActive = 0;

    // data is ready, read it in
    count = 0;
    while (count < retsize) {
        rb = read(domeFD,&buf[count],1);
        if (rb < 0) {
            tdlog("Error reading: %s\n",strerror(errno));
            return -1;
        }
        if (rb == 0) {
            //TRACE("Read 0\n");
            continue;
        }
//        TRACE("Got a character: %c\n",buf[count]);
        if (buf[count] == 0x0D) {
            break;
        }
        count++;
    }
    buf[count] = '\0';
    return 0;
}

/* Look for a response from the dome controller */
static int periodicPoll(void)
{
    time_t now;
    static time_t lastTime;

    if (domeTTYReady()) {
        return 1; // if it's ready, it's ready...
    }

    // if not, see if we should ask it for status
    // but we don't want to do this more than once per second
    now = time(NULL);
//    TRACE("periodic poll: now = %ld lastTime = %ld\n",now,lastTime);
    if (now != lastTime) {
//    	TRACE("Doing periodic poll\n");
        sendDomeCommand("@00RD0000000553*\r\n");
        lastTime = now;
        pollIsValid = 1;
    }

    // and we'll pick up again on the subsequent poll
    return(0);
}

/* See if data is ready */
static int domeTTYReady(void)
{
    fd_set rfds;
    struct timeval tv;
    int retval;

//    TRACE("domeTTYReady() ");

    if (domeFD <=0) {
        TRACE("domeFD not set\n");
        return 0;
    }

    /* Watch domeFD to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(domeFD, &rfds);
    /* just poll */
    tv.tv_sec = 0;
    tv.tv_usec = 1000;

    retval = select(domeFD+1, &rfds, NULL, NULL, &tv);

    if (retval < 0) {
        TRACE("Error in select in domeTTYReady (%d)\n",retval);
        return 0;
    }

//    TRACE("ready = %d\n",retval);
    return retval;
}

/* If data is ready, parse it according to current expected result and return a cmc-type response */
static int domeTTYResult(char *buf, int size)
{
    char domeCmd[256];
    static int lastDo = DO_NOTHING;
    static int lastSeek;
    static int oneShot, twoShot;


    if (lastDo != doingWhat) {
        oneShot = twoShot = 0;
    }

    // TRACE("domeTTYResult()... doing what = %d, lastdo = %d\n",doingWhat,lastDo);

    if (0 != getDomeResponse(domeCmd, sizeof(domeCmd))) {
        strcpy(buf,"-1: Error reading Dome TTY\n");
        return 0;
    }
    output = 1;
    if (0 != parseDomeResult(domeCmd))
    {
        strcpy(buf,"-1: Dome communication error\n");
        lastDo = DO_NOTHING;
        output=0;
        return 0;
    }
    output=0;

    strcpy(buf,""); // assume we wait silently

    if (!pollIsValid) {
        return -1;
    }

    if (domeStatus.data0 & (RD_NOTREADY | RD_MANUAL | RW_EMG))
    {
        sprintf(buf,"-1: Dome %s%s%s\n",
                (domeStatus.data0 & RD_NOTREADY)?"Not Ready ":"",
                (domeStatus.data0 & RD_MANUAL)?"Manual mode is on ":"",
                (domeStatus.data0 & RW_EMG)?"Emergency Status!":"");

        lastDo = DO_NOTHING;
        return 0;
    }

    switch (doingWhat) {
    case DO_WAIT_ROOF_OPEN:
        // TRACE("Waiting for roof open\n");
        if (domeStatus.data1 & RW_SLIT_OPEN) {
            // it's opening
            if (!oneShot) {
                strcpy(buf,"1: Opening shutter\n");
                oneShot = 1;
            }
        }
        if ((domeStatus.data1 & SHUTTER_OPEN_TEST) == SHUTTER_OPEN_TEST) {
            // both screen and shutter open
            strcpy(buf,"0: Shutter open, screens retracted\n");
        }
        else if (domeStatus.data1 & RD_SLIT_OPEN_LIMIT) {
            // we expect to be open all the way
            if (!twoShot) strcpy(buf,"1: Shutter open, waiting for screens\n");
            twoShot = 1;
        }
        break;

    case DO_WAIT_ROOF_CLOSE:
        // TRACE("Waiting for roof close\n");
        if (domeStatus.data1 & RW_SLIT_CLOSE) {
            // it's closing
            if (!oneShot) {
                strcpy(buf,"1: Closing shutter\n");
                oneShot = 1;
            }
        }
        if ((domeStatus.data1 & SHUTTER_CLOSE_TEST) == SHUTTER_CLOSE_TEST) {
            // both screen and shutter open
            strcpy(buf,"0: Shutter closed, screens cover\n");
        }
        else if (domeStatus.data1 & RD_SLIT_CLOSE_LIMIT) {
            // we expect to be closed all the way
            if (!twoShot) strcpy(buf,"1: Shutter closed, waiting for screens\n");
            twoShot = 1;
        }
        break;

    case DO_ROTATE:
    case DO_WAIT_SEEK:
        if (abs(domeStatus.domePos-targetPos) < 5) {
            // TRACE("WAIT SEEK DONE\n");
            sprintf(buf,"1: Dome is at target of %d.%d degrees\n",domePosAdjust(targetPos)/10,domePosAdjust(targetPos)%10);
            domeTTYRotate(0);
            doingWhat = DO_NOTHING;
            sprintf(buf,"0: done");
        } else {
            if (abs(lastSeek-domeStatus.domePos) > 35) { // 3.5 degrees gives a nice visual  :)
                sprintf(buf,"1: Dome at %d.%d degrees\n",domePosAdjust(domeStatus.domePos)/10,domePosAdjust(domeStatus.domePos)%10);
                lastSeek = domeStatus.domePos;
                DS = DS_ROTATING;
            }
        }
        break;

    case DO_NOTHING:
    default:
//         TRACE("Doing nothing\n");
        DS = DS_STOPPED;
        lastSeek = domeStatus.domePos;
        break;
    }

    lastDo = doingWhat;
    return 0;
}


static void domeTTYRoof(int dir)
{
    domeCmdStatus = domeStatus;

    switch (dir) {
    case 0:
//            TRACE("DomeTTY: Roof Stop");
        domeCmdStatus.data1 = 0;
        break;
    case 1:
//            TRACE("DomeTTY: Shutter and screen open");
        domeCmdStatus.data1 = RW_HISCREEN_UP | RW_LOSCREEN_DOWN | RW_SLIT_OPEN;
        doingWhat = DO_WAIT_ROOF_OPEN;
        break;
    case -1:
//            TRACE("DomeTTY: Shutter and screen close");
        domeCmdStatus.data1 = /*RW_HISCREEN_DOWN | RW_LOSCREEN_UP |*/  RW_SLIT_CLOSE;
        doingWhat = DO_WAIT_ROOF_CLOSE;
        break;
    default:
//            TRACE("DomeTTY: Roof: Unknown (%d)",dir);
        domeCmdStatus.data1 = 0;
        break;
    }

    commandDome();

}

static void domeTTYSeek(double az)
{
    int deg10;

    // convert radians to degrees * 10
    deg10 = 0.5+(az * 572.9577951);
    //TRACE("DomeTTY: Dome Seek to %g radians (%d.%d degrees) East of North\n",az,deg10/10,deg10%10);

    domeCmdStatus.domePos = targetPos = truePosAdjust(deg10);

    doingWhat = DO_WAIT_SEEK;
    domeCmdStatus.data0 = 0;
    domeCmdStatus.data1 = WD_DOME_GOABS;
    domeCmdStatus.hiscreenPos = domeCmdStatus.loscreenPos = 0;
    commandDome();
}

/*

// this would be kinda cool... but there really isn't support
// in Talon for this.
// No real need to implement.
// To do this for auto mode dome (the only practical way to implement)
// it should combine domeTTYSeek as well, so that both the dome and the
// screens move simultaneously.  The DO_WAIT_SEEK switch of domeTTYResult
// would need to change also so that it looked for both rotation completion
// and screen completion before announcing success.

static void domeTTYScreenAlt(double alt)
{
    int deg10Lo, deg10Hi;
    int deg10Win = 450;

    // convert radians to degrees * 10
    deg10Lo = 0.5+(alt * 572.9577951) - (deg10Win/2);
    deg10Hi = 0.5+(alt * 572.9577951) + (deg10Win/2);
    TRACE("DomeTTY: Screen seek to %d.%d - %d.%d degrees\n",deg10Lo/10,deg10Lo%10,deg10Hi/10,deg10Hi%10);

    doingWhat = DOME_WAIT_SCREENS;
    domeCmdStatus.data0 = 0;
    domeCmdStatus.data1 = WD_SCREENS_GOABS;
    domeCmdStatus.hiscreenPos = deg10Hi;
    domeCmdStatus.loscreenPos = deg10Lo;
    commandDome();
}
*/
/*
 -- REMOVED -- No longer needed because we got abspos to work

static void domeSeekDir()
{
    int t = domeCmdStatus.domePos;
    int n = domeStatus.domePos;
    int d = abs(t - n);
    int s = 1;
	int lastStatus = domeCmdStatus.data1;

	if(doingWhat != DO_WAIT_SEEK) {
		lastStatus = 0;
	}

    TRACE("DOME SEEK DIR\n");

    if(t < n) s = -1;

    if(d > 1800) {
        d = 3600 - d;
        s = -s;
    }
    while(d > 3600) d-=3600; // probably can't happen anyway

    // see if within tolerance
    if(d  < (DOMETOL * 573)) {
        domeCmdStatus.data1 = 0;        // stop if we're there
        commandDome();
//  TRACE("STOPPING DOME");
  		periodicPoll();
    }
    else {
        if(s < 0) {
//  TRACE("SEEKING CCW\n");
            domeCmdStatus.data1 = RW_DOME_CCW;
//			domeCmdStatus.data1 = WD_DOME_GOABS;
        }
        else {
//  TRACE("SEEKING CW\n");
            domeCmdStatus.data1 = RW_DOME_CW;
//			domeCmdStatus.data1 = WD_DOME_GOABS;
        }
    }
	if(lastStatus != domeCmdStatus.data1) {
    	commandDome();
    }
}
*/

static void domeTTYRotate(int dir)
{
    domeCmdStatus = domeStatus;
    switch (dir) {
    case 0:
//            TRACE("DomeTTY: Rotate Stop");
        domeCmdStatus.data1 = 0;
        break;
    case 1:
//            TRACE("DomeTTY: Rotate CW");
        domeCmdStatus.data1 = RW_DOME_CW;
        output=1;
        break;
    case -1:
//            TRACE("DomeTTY: Rotate CCW");
        domeCmdStatus.data1 = RW_DOME_CCW;
        output=0;
        break;
    default:
//            TRACE("DomeTTY: Rotate: Unknown (%d)",dir);
        domeCmdStatus.data1 = 0;
        DS = DS_STOPPED;
        break;
    }

    commandDome();
    commandActive = 0;
    doingWhat = DO_ROTATE;

}

static void domeTTYStop(void)
{

    // TRACE("DomeTTY: Stop");
    domeCmdStatus = domeStatus;
    domeCmdStatus.data1 = 0;

    commandDome();
    commandActive = 0;
    doingWhat = DO_NOTHING;
    DS = DS_STOPPED;

}

static double domeTTYReadPos(void)
{
    double pos;
    char buf[256];

    if (AD || doingWhat == DO_WAIT_SEEK) {
        if (periodicPoll()) {
            if (domeTTYResult(buf,sizeof(buf)) < 0) {
                tdlog("DomeTTY: Failed to read position\n");
            }
        }
    }

    pos = ((double) domePosAdjust(domeStatus.domePos)+0.5) / 572.9577951;

//TRACE ("Dome position = (%0X) %.2lf doingWhat = %d, expecting %d\n",domeStatus.domePos,pos,doingWhat,DO_WAIT_SEEK);

    return pos;
}

static int parseDomeResult(char *buf)
{
    int parsed = 0;
    unsigned int rsp,d0,d1,d2,d3,d4,fcs;
    rsp=d0=d1=d2=d3=d4=fcs = 0;

//if(output)
//    TRACE("DomeTTY: ParseResult (%d bytes): %s",strlen(buf),buf);

    // determine type of response
    if (buf[3] == 'R') {
        // parse the response code
        parsed = (1==sscanf(buf,"@00RD%02X",&rsp));
        // parse the rest of the query for data
        if (parsed) parsed = (6==sscanf(&buf[7],"%04X%04X%04X%04X%04X%02X*",&d0,&d1,&d2,&d3,&d4,&fcs));
    }
    else {
        // parse the response code
        parsed = (1==sscanf(buf,"@00WD%02X",&rsp));
        // parse the result of a command
        if (parsed) parsed = (1==sscanf(&buf[7],"%02X*",&fcs));
    }

    if (parsed)
    {
        if (rsp == 0) {
            if (buf[3] == 'R') { // only update on reads
                domeStatus.data0 = d0;
                domeStatus.data1 = d1;
                domeStatus.domePos = d2;
                domeStatus.hiscreenPos = d3;
                domeStatus.loscreenPos = d4;
//            TRACE("DataParse: %04X %04X dome=%d hiscr=%d loscr=%d\n",d0,d1,d2,d3,d4);
            }
            return 0;
        }
#if DEBUG
        else {
            tdlog("DomeTTY ParseResult: Error in response (%d = ",rsp);
            switch (rsp) {
            case 0x10:
                tdlog("Parity Error");
                break;
            case 0x11:
                tdlog("Stop Bit Error");
                break;
            case 0x12:
                tdlog("Over Data Error");
                break;
            case 0x13:
                tdlog("FCS (checksum) Error");
                break;
            case 0x14:
                tdlog("Command Error");
                break;
            case 0x15:
                tdlog("Data Name Error");
                break;
            case 0x18:
                tdlog("Data Max Error");
                break;
            }
            tdlog(")\n");
        }
#endif

    }
    else {
        TRACE("DomeTTY ParseResult: Failed to scan\n");
    }

    return -1;
}

// Adjust for 180 degree offset of dome
// dome pos in, true pos out
static int domePosAdjust(int domePos)
{
//	return domePos;

// if we keep the same rotation
    int true = domePos + 1800;
    while (true >= 3600) true -= 3600;

// if we must also reverse rotation
//	int true = 1800 - domePos;
//	while(true < 0) true += 3600;

    return true;
}

// desired pos in, dome pos out
static int truePosAdjust(int desiredPos)
{
//	return desiredPos;

// if we keep the same rotation
    int dome = desiredPos - 1800;
    while (dome < 0) dome += 3600;

// if we must also reverse rotation
//	int dome = 1800 - true;
//	while(dome < 0) dome += 3600;

    return dome;
}

#endif

