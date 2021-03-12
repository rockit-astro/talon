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

#include "teled.h"

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

static double	dome_to;	/* mjd when operation will timeout */

/* the current activity, if any */
static void (*active_func) (int first, ...);

/* one of these... */
static void dome_poll(void);
static void dome_reset(int first, ...);
static void dome_open(int first, ...);
static void dome_close(int first, ...);
static void dome_autoOn(int first, ...);
static void dome_autoOff(int first, ...);
static void dome_home(int first, ...);
static void dome_setaz(int first, ...);
static void dome_stop(int first, ...);
static void dome_status (int first, ...); //IEEC
static void dome_jog (int first, ...);
static void dome_alarmset(int first, int alon);

/* helped along by these... */
static int d_emgstop(char *msg);
static int d_chkWx(char *msg);
static void d_readpos(void);
static void d_auto(void);
static double d_telaz(void);
static void d_cw(void);
static void d_ccw(void);
static void d_stop(void);
static void initCfg(void);
static void openChannels(void);
static void closeChannels(void);

static void d_readshpos(void);
static void d_getalarm(void);

/* later entries */
static int d_goShutterPower(void);
static char * doorType(void);
static char * enclType(void);
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
dome_msg (msg)
char *msg;
{
    char jog_dir[2];
    double az;
    int alon;

    /* do reset before checking for `have' to allow for new config file */
    if (msg && strncasecmp (msg, "reset", 5) == 0) {
    	dome_reset(1);
        return;
    }

    /* worth it? */
    if (!DHAVE && !SHAVE) {
        if (msg)
            fifoWrite (Dome_Id, 0, "Ok, but dome really not installed");
        return;
    }

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

    if (msg && sscanf (msg, "alarmset %d", &alon) == 1)
    {
        dome_alarmset(1, alon);
        return;
    }

    /* top priority are emergency stop and weather alerts */
    if (d_emgstop(msg) || d_chkWx(msg))
        return;

    /* handle normal messages and polling */
    if (!msg)
    	dome_poll();
    else if (strncasecmp (msg, "stop", 4) == 0)
    	dome_stop(1);
    else if (strncasecmp (msg, "open", 4) == 0)
    	dome_open(1);
    else if (strncasecmp (msg, "close", 5) == 0)
    	dome_close(1);
    else if (strncasecmp (msg, "auto", 4) == 0)
    	dome_autoOn(1);
    else if (strncasecmp (msg, "off", 3) == 0)
    	dome_autoOff(1);
    else if (strncasecmp (msg, "home", 4) == 0)
    	dome_home(1);
    else if (strncasecmp (msg, "status", 6) == 0) //IEEC
    	dome_status(1);                       //IEEC
    else if (sscanf (msg, "Az:%lf", &az) == 1)
    	dome_setaz (1, az);
    else if (sscanf (msg, "j%1[0+-]", jog_dir) == 1)
    	dome_jog (1, jog_dir[0]);
    else {
        fifoWrite (Dome_Id, -1, "Unknown command: %.20s", msg);
        dome_stop (1);	/* default for any unrecognized message */
    }

}

/* maintain current action */
static void
dome_poll ()
{
    if (virtual_mode) {
        if (DHAVE || SHAVE) {
            vmcService(DOMEAXIS);
        }
    }

    if (active_func)
        (*active_func)(0);

    if (DHAVE)
    {
        d_readpos();
        if (AD) d_auto();
    }
}

/* read config files, stop dome; don't mess much with shutter state */
static void
dome_reset (int first, ...)
{
    Now *np = &telstatshmp->now;

    if (first) {
        ++setaz_error;
        initCfg();

        if (virtual_mode) { // set the virtual dome controller up
            vmcSetup(DOMEAXIS,.1,.1,DOMESTEP,DOMESIGN);
        }

        if (DHAVE || SHAVE) {
        	openChannels();
            d_stop();
            if (DHAVE) {
                dome_to = mjd + DOMETO;
            }
            if (SHAVE && SMOVING) {
                SS = SH_IDLE;
            }
            active_func = dome_reset;
        } else {
        	closeChannels();
        }
    }

    if (DHAVE) {
        if (mjd > dome_to) {
        fifoWrite (Dome_Id, -2, "Reset timed out");
        d_stop();   /* ?? */
        active_func = NULL;
        return;
        }
        d_readpos();
        if (DS != DS_STOPPED)
        return;
    }

    active_func = NULL;
    if (DHAVE || SHAVE)
    {
        fifoWrite (Dome_Id, 0, "Reset complete");
        d_stop();
    
        if(!virtual_mode) {
            /* Version 1.5 or greater of nodeDome.cmc */
            // set the encoder steps (used by script!) to value in cfg file
          if(DHAVE)
            csi_w (cfd, "esteps=%.0f;",DOMESTEP);

            // set the sign variable in the script to the sign we use here
            csi_w (cfd, "s=%d;",DOMESIGN);
            // set the dome timeout variable in the script if not set by the script itself
            csi_w (cfd, "w=w?w:%.0f;",DOMETO * SPD * 1000);
            // set the roof open/close times if not set by the script itself
            csi_w (cfd, "r=r?r:%.0f;",SHUTTERTO * SPD * 750.0); // 3/4 of timeout
            csi_w (cfd, "v=v?v:%.0f;",SHUTTERTO * SPD * 750.0); // 3/4 of timeout
            // set the roof open/close timeouts
            csi_w (cfd, "t=t?t:%.0f;",SHUTTERTO * SPD * 1000.0); // full timeout
            csi_w (cfd, "u=u?u:%.0f;",SHUTTERTO * SPD * 1000.0); // full timeout
        }
    }
    else
        fifoWrite (Dome_Id, 0, "Not installed");
}

/* move dome to shutter power position before opening or closing

	return true (1) if OK to open / close shutter
	return false (0) if still moving there, or otherwise unable to activate door

*/
static int d_goShutterPower(void)
{
    // define temp holder for active_func pointer
    void (*af_hold) (int first, ...);
    double shtdif;

    /* get dome in place first, if have one */
    if (!DHAVE) return 1; // go ahead if we're just a roof...
    if (!SHUTTERAZ && !SHUTTERAZTOL) // these are defined as zero if not needed for dome type
        return 1; // go ahead and open it

    // check on current situation
    d_readpos();
    if (DS == DS_STOPPED) {
        shtdif = SHUTTERAZ - AZ;
        if (shtdif < 0) shtdif = -shtdif;
        if (shtdif <= SHUTTERAZTOL)
            return 1; // we're there!

        fifoWrite(Dome_Id, 1, "Aligning Dome for %s power", doorType());

        // save and replace current active function so we don't forget what we're doing
        af_hold = active_func;
        // move us there
        dome_setaz(1, SHUTTERAZ);
        // back to waiting for shutter power position
        active_func = af_hold;
    }
    else
    {
        // save and replace current active function so we don't forget what we're doing
        af_hold = active_func;
        // move us there
        dome_setaz(0);
        // back to waiting for shutter power position
        active_func = af_hold;
    }

    // If there was an error while turning, just bail out here....
    if (setaz_error)
        active_func = NULL;

    return (0); // we're in process of doing something... wait for stop to reassess.
}

/* Some terminology about door... if we have a dome, it's a shutter, if not it's a roof...*/
static char * doorType(void)
{
    if (DHAVE)	return ("shutter");
    else		return ("roof");
}

static char * enclType(void)
{
    if (DHAVE)	return("dome");
    else		return("roof");
}

/* open shutter or roof */
static void
dome_open (int first, ...)
{
    Now *np = &telstatshmp->now;
    char buf[1024];
    int n;

    /* nothing to do if no shutter */
    if (!SHAVE) {
        fifoWrite (Dome_Id, -3, "No %s to open", doorType());
        return;
    }

    if (first) {
        /* set new state */
        dome_to = mjd + SHUTTERTO;
        active_func = dome_open;
        AD = 0;
    }

    // If we need to rotate to power position first, do so
    if (!d_goShutterPower()) {
        return;
    }

    /* initiate open if not under way */
    if (SS != SH_OPENING) {
        if (virtual_mode) {
            vmc_w(DOMEAXIS,"roofseek(1);");
        } else {
            csi_w (cfd, "roofseek(1);");
        }
        SS = SH_OPENING;
        fifoWrite (Dome_Id, 2, "Starting open");
        toTTS ("The %s is now opening.", doorType());
        active_func = dome_open;
        return;
    }

    /* check for time out */
    if (mjd > dome_to) {
        fifoWrite (Dome_Id, -5, "Open timed out");
        toTTS ("Opening of %s timed out.", doorType());
        d_stop();
        SS = SH_IDLE;
        active_func = NULL;
        return;
    }

    /* check progress */
    if (virtual_mode) {
        if (!vmc_isReady(DOMEAXIS))
            return;
        if (vmc_r(DOMEAXIS, buf, sizeof(buf)) <=0)
            return;
    } else {
        if (!csiIsReady(cfd))
            return;
        if (csi_r (cfd, buf, sizeof(buf)) <= 0)
            return;
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
    	d_stop();

        if (n == -2) {
            tdlog("Dome alarm activated");
            // telstatshmp->domealarm = 1;
        }
        // else
        // /* Alarm has been reset */
        // telstatshmp->domealarm = 0;

        fifoWrite (Dome_Id, n, "Open error: %s", buf+2); /* skip -n */
        toTTS ("Error opening %s: %s", doorType(), buf+2);
        SS = SH_IDLE;
        active_func = NULL;
        dome_stop(1);
        return;
    }
    if (n > 0) {
        fifoWrite (Dome_Id, n, "%s", buf+1); /* skip n */
        toTTS ("Progress of %s: %s", doorType(), buf+1);
        // telstatshmp->domealarm = 0;
        return;
    }

    /* ok! */
    fifoWrite (Dome_Id, 0, "Open complete");
    toTTS ("The %s is now open.", doorType());
    // telstatshmp->domealarm = 0;
    SS = SH_OPEN;
    active_func = NULL;
}

/* close shutter or roof */
static void
dome_close (int first, ...)
{
    Now *np = &telstatshmp->now;
    char buf[128];
    int n;

    /* nothing to do if no shutter */
    if (!SHAVE) {
        fifoWrite (Dome_Id, -3, "No %s to close", doorType());
        return;
    }

    if (first) {
        /* set new state */
        dome_to = mjd + SHUTTERTO;
        active_func = dome_close;
        AD = 0;
    }

    // If we need to rotate to power position first, do so
    if (!d_goShutterPower()) {
        return;
    }

    /* initiate close if not under way */
    if (SS != SH_CLOSING) {
        if (virtual_mode) {
            vmc_w(DOMEAXIS,"roofseek(-1);");
        } else {
            csi_w (cfd, "roofseek(-1);");
        }
        SS = SH_CLOSING;
        fifoWrite (Dome_Id, 2, "Starting close");
        toTTS ("The %s is now closing.", doorType());
        active_func = dome_close;
        return;
    }

    /* check for time out */
    if (mjd > dome_to) {
        fifoWrite (Dome_Id, -5, "Close timed out");
        toTTS ("Closing of %s timed out.", doorType());
        d_stop();
        SS = SH_IDLE;
        active_func = NULL;
        return;
    }

    /* check progress */
    if (virtual_mode) {
        if (!vmc_isReady(DOMEAXIS))
            return;
        if (vmc_r(DOMEAXIS, buf, sizeof(buf)) <=0)
            return;
    } else {
        if (!csiIsReady(cfd))
            return;
        if (csi_r (cfd, buf, sizeof(buf)) <= 0)
            return;
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
        d_stop();

        if(n == -2) {
          tdlog("Dome alarm activated");
          // telstatshmp->domealarm = 1;
        }
        // else
        // telstatshmp->domealarm = 0;

        fifoWrite (Dome_Id, n, "Close error: %s", buf+2); /* skip -n */
        toTTS ("Error closing %s: %s", doorType(), buf+2);
        SS = SH_IDLE;
        active_func = NULL;
        dome_stop(1);
        return;
    }
    if (n > 0) {
        fifoWrite (Dome_Id, n, "%s", buf+1); /* skip n */
        toTTS ("Progress of %s: %s", doorType(), buf+1);
        // telstatshmp->domealarm = 0;
        return;
    }

    /* ok! */
    fifoWrite (Dome_Id, 0, "Close complete");
    toTTS ("The %s is now closed.", doorType());
    // telstatshmp->domealarm = 0;
    SS = SH_CLOSED;
    active_func = NULL;
}

/* activate autodome lock */
/* ARGSUSED */
static void
dome_autoOn (int first, ...)
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
dome_autoOff (int first, ...)
{
    if (!DHAVE) {
        fifoWrite (Dome_Id, 0, "Ok, but no dome really");
    } else {
        /* just stop and reset the flag */
        AD = 0;
        d_stop();
        fifoWrite (Dome_Id, 0, "Auto dome off");
    }
}

/* find dome home */
static void
dome_home (int first, ...)
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
        if (virtual_mode) {
            vmc_w(DOMEAXIS,"finddomehome();");
        } else {
            csi_w (cfd, "finddomehome();");
        }

        /* set timeout and new state */
        dome_to = mjd + DOMETO;
        active_func = dome_home;
        DS = DS_HOMING;
        AD = 0;
        TAZ = DOMEZERO;
        toTTS ("The dome is seeking the home position.");
    }

    /* check for time out */
    if (mjd > dome_to) {
        fifoWrite (Dome_Id, -5, "Home timed out");
        toTTS ("Dome home timed out.");
        d_stop();
        DS = DS_STOPPED;
        active_func = NULL;
        return;
    }

    /* check progress */
    if (virtual_mode) {
        if (!vmc_isReady(DOMEAXIS))
            return;
        if (vmc_r(DOMEAXIS, buf, sizeof(buf)) <=0)
            return;
    } else {
        if (!csiIsReady(cfd))
            return;
        if (csi_r (cfd, buf, sizeof(buf)) <= 0)
            return;
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
    	d_stop();
        fifoWrite (Dome_Id, n, "Home error: %s", buf+2); /* skip -n */
        toTTS ("Dome home error. %s", buf+2);
        DS = DS_STOPPED;
        active_func = NULL;
        dome_stop(1);
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
dome_status (int first, ...)
{
  	int status, pos;
    double daz;
    char buf[1024];

    if (first) 
    {
        if (DHAVE || SHAVE)
        {
            if (!virtual_mode) 
            {
                // -- Version 1.5 or greater of nodeDome.cmc --//
            	if(csi_wr(cfd, buf, sizeof(buf), "domeStatus();")>0)
                    status = atoi(&buf[0]);
                else
                    status = -1;
               
                 /* CSIMC output (buf) could be directly passed to FIFOs, 
                    but better define error level as -1 (instead of 4) */
                switch(status)
                {
                    case 0:
                        fifoWrite(Dome_Id, 0, "Shutter is closed");
                        break;
                    case 1:
                        fifoWrite(Dome_Id, 1, "Shutter is open");
                        break;
                    case 2:
                        fifoWrite(Dome_Id, 2, "Shutter is closing");
                        break;
                    case 3:
                        fifoWrite(Dome_Id, 3, "Shutter is opening");
                        break;
                    default:
                        fifoWrite(Dome_Id, -1, "Error retrieving shutter status");
                        break;
                }
                if (DHAVE)
                {
                    status = csi_rix(cfd, "=isDomeHomed();");
                    if(status==0)
                        fifoWrite(Dome_Id, 0, "Dome orientation is unknown");
                    else if (status==1)
                    {
						pos = csi_rix(cfd, "=epos;") * DOMESIGN;
						daz = (2* PI * pos / DOMESTEP) + DOMEZERO;
						range(&daz, 2*PI);
                        fifoWrite(Dome_Id, 1, "Dome orientation is %g",daz);
                    }
            	    else
                        fifoWrite(Dome_Id, -1, "Error retrieving dome orientation");
                }
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
dome_setaz (int first, ...)
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
        if (virtual_mode) {
            sprintf(buf,"domeseek(%ld,%ld);", tenc, tol);
            vmc_w(DOMEAXIS,buf);
        } else {
            csi_w (cfd, "domeseek(%ld,%ld);", tenc, tol);
        }
        /* set state */
        AD = 0;
        dome_to = mjd + DOMETO;
        active_func = dome_setaz;
        toTTS ("The dome is rotating towards the %s.", cardDirLName (TAZ));
        DS = DS_ROTATING;
    }

    /* check for time out */
    if (mjd > dome_to) {
        fifoWrite (Dome_Id, -5, "Azimuth timed out");
        toTTS ("Roof azimuth command timed out.");
        d_stop();
        DS = DS_STOPPED;
        active_func = NULL;
        ++setaz_error;
        return;
    }
    /* check progress */
    if (virtual_mode) {
        if (!vmc_isReady(DOMEAXIS))
            return;
        if (vmc_r(DOMEAXIS, buf, sizeof(buf)) <=0)
            return;
    } else {
        if (!csiIsReady(cfd))
            return;
        if (csi_r (cfd, buf, sizeof(buf)) <= 0)
            return;
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
    	d_stop();
        fifoWrite (Dome_Id, n, "Az error: %s", buf+2); /* skip -n */
        toTTS ("Dome azimuth error. %s", buf+2);
        DS = DS_STOPPED;
        active_func = NULL;
        ++setaz_error;
        dome_stop(1);
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
}

/* stop everything */
static void
dome_stop (int first, ...)
{
    Now *np = &telstatshmp->now;

    if (!DHAVE && !SHAVE) {
        fifoWrite (Dome_Id, 0, "Ok, but nothing to stop really");
        return;
    }

    if (first) {
    	d_stop();
        dome_to = mjd + DOMETO;
        active_func = dome_stop;
        if (DHAVE)
            AD = 0;
    }

    if (mjd > dome_to) {
        fifoWrite (Dome_Id, -5, "Stop timed out");
        toTTS ("Stop of %s timed out.",enclType());
        d_stop();	/* ?? */
        if (DHAVE) {
            DS = DS_STOPPED;
            AD = 0;
            TAZ = AZ;
        }
        active_func = NULL;
        return;
    }

    active_func = NULL;
    fifoWrite (Dome_Id, 0, "Stop complete");
    if (DHAVE) {
        DS = DS_STOPPED;
        TAZ = AZ;
    }
    toTTS ("The %s is now stopped.", enclType());
}

/* jog: + means CW, - means CCW, 0 means stop */
static void
dome_jog (int first, ...)
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
            d_cw();
            active_func = dome_jog;
            DS = DS_ROTATING;
            break;
        case '-':
            fifoWrite (Dome_Id, 6, "Paddle command CCW");
            toTTS ("The dome is rotating counter clockwise.");
            d_ccw();
            active_func = dome_jog;
            DS = DS_ROTATING;
            break;
        case '0':
            fifoWrite (Dome_Id, 7, "Paddle command stop");
            d_stop();
            active_func = NULL;
            DS = DS_STOPPED;
            break;
        default:
            fifoWrite (Dome_Id, -14, "Bogus jog code: %c", dircode);
            active_func = NULL;
            dome_stop(1);
            break;
        }

        return;
    }
    
    d_readpos();
}

static void dome_alarmset (int first, int alon) {
    if(!SHAVE) {
        fifoWrite(Dome_Id, 0, "Ok, but really no shutter");
        return;
    }

    if(virtual_mode) {
        fifoWrite(Dome_Id, 0, "Ok, but really no alarm");
        return;
    }

    if(first) {
        csi_w(cfd, "alarm_set(%d);", alon);

        /* ok! */
        fifoWrite (Dome_Id, 0, "%s alarm complete", alon ? "Set" : "Reset");
        toTTS ("The alarm is now %s", alon ? "on" : "off");

        // telstatshmp->domealarm = alon;
    }
}

/* middle-layer support functions */

/* check the emergency stop bit.
 * while on, stop everything and return 1, else return 0
 */
static int
d_emgstop(char *msg)
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

    if (msg || (active_func && active_func != dome_stop))
        fifoWrite (Dome_Id,
                   -15, "Command cancelled.. emergency stop is active");

    if (active_func != dome_stop && DS != DS_STOPPED) {
        fifoWrite (Dome_Id, 8, "Emergency stop asserted -- stopping %s", enclType());
        AD = 0;
        dome_stop (1);
    }

    dome_poll();

    return (1);
}

/* if a weather alert is in progress respond and return 1, else return 0 */
static int
d_chkWx(char *msg)
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

    if (msg || (active_func && active_func != dome_close))
        fifoWrite (Dome_Id,
                   -16, "Command cancelled.. weather alert in progress");

    if (active_func != dome_close && SS != SH_CLOSED) {
        fifoWrite (Dome_Id, 9, "Weather alert asserted -- closing %s", doorType());
        AD = 0;
        dome_close (1);
    }

    dome_poll();

    return (1);
}

/* read current position and set AZ and DS */
static void
d_readpos ()
{
	double az;
//	int stopped;

	/* read fresh */
    int pos;
    if(virtual_mode) {
		pos = vmc_rix (sfd, "=epos;") * DOMESIGN;					
	} else {
	    pos = csi_rix (sfd, "=epos;") * DOMESIGN;
	}
    az = (2*PI)*pos/DOMESTEP + DOMEZERO;
	range (&az, 2*PI);
//	stopped = delra(az-AZ) <= (1.5*2*PI)/DOMESTEP;

	/* update */
//	DS = stopped ? DS_STOPPED : DS_ROTATING;
	AZ = az;
}

/* return the az the dome should be for the desired telescope information */
static double
d_telaz()
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
    if(telstatshmp->telstate == TS_TRACKING) {
        return(telstatshmp->Caz);
    }
    return(telstatshmp->Daz);   // TODO: account for domeoffset
}

/* initiate a stop */
static void
d_stop(void)
{
    if (!virtual_mode) {
        if (cfd) {
            csi_intr (cfd);
            csiDrain(cfd);
            if (DHAVE) {
                csi_w (cfd, "dome_stop();");
                csiDrain(cfd);
            }
            csi_w (cfd, "roofseek(0);");
            csiDrain(cfd);
        }
    } else {
        if (DHAVE)
            vmc_w(DOMEAXIS, "dome_stop();");
        vmc_w(DOMEAXIS, "roofseek(0);");
    }
}

/* start cw */
static void
d_cw()
{
    if (virtual_mode) {
        char buf[128];
        sprintf(buf,"domejog(%d);", DOMESIGN);
        vmc_w (DOMEAXIS, buf);
    } else {
        csi_w (cfd, "domejog(%d);", DOMESIGN);
    }
}

/* start ccw */
static void
d_ccw()
{
    if (virtual_mode) {
        char buf[128];
        sprintf(buf,"domejog(%d);", -DOMESIGN);
        vmc_w (DOMEAXIS, buf);
    } else {
        csi_w (cfd, "domejog(%d);", -DOMESIGN);
    }
}

/* keep AZ within DOMETOL of telaz() */
static void
d_auto()
{
    char buf[128];
    int n;
    double scale = DOMESTEP/(2*PI);
    double diff;

    // First make sure the door is open
    if(SHAVE) {
        if(SS != SH_OPEN) {
            if(SS != SH_OPENING)            // if not actually opening
            {
                if(active_func == NULL) {   // and not aligning
                    dome_open(1);           // go ahead and initiate the open
                }               
            }
            AD = 1;  // keep this turned on, as the opening process will turn it off in a couple places
            return;
        }
    }
    
    diff = TAZ - d_telaz();
    if(diff < 0) diff = -diff;  

    /*
    tdlog("Auto: Dome is %s. Target: %7.4g  Actual: %7.4g  d_telaz: %7.4g  diff: %7.4g DOMETOL: %7.4g",
            (DS !=DS_ROTATING) ? "STOPPED" : "ROTATING",
            TAZ, AZ, d_telaz(),
            diff, DOMETOL);
    */

    if(DS != DS_ROTATING) {
    
        if(diff < DOMETOL)
            return; // already there
            
        TAZ = d_telaz();
        
        if(virtual_mode) {
            char buf[128];
            sprintf(buf,"domeseek (%.0f, %.0f);", (TAZ-DOMEZERO)*DOMESIGN*scale,DOMETOL*scale);
            vmc_w(DOMEAXIS,buf);
        } else {
            csi_w (cfd, "domeseek (%.0f, %.0f);", (TAZ-DOMEZERO)*DOMESIGN*scale,DOMETOL*scale);
        }
    }

    /* check progress */
    if(virtual_mode) {
        if(!vmc_isReady(DOMEAXIS))
            return;
        if(vmc_r(DOMEAXIS, buf, sizeof(buf)) <=0)
            return;
    } else {
        if (!csiIsReady(cfd))
            return;
        if(csi_r (cfd, buf, sizeof(buf)) <= 0)
            return;
    }       
    if(!buf[0])
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
}

/* (re) read the dome confi file */
static void
initCfg()
{
#define NDCFG   (sizeof(dcfg)/sizeof(dcfg[0]))

    static int DOMEHAVE;
    static int SHUTTERHAVE;

    static CfgEntry dcfg[] = {
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
    int n;

    /* read the file */
    n = readCfgFile (1, dcfn, dcfg, NDCFG);
    if (n != NDCFG) {
        cfgFileError (dcfn, n, (CfgPrFp)tdlog, dcfg, NDCFG);
        die();
    }

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
    	d_stop();
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
openChannels()
{
    if (!virtual_mode) {
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
    }
    else {
        vmcReset(DOMEAXIS);
        cfd = DOMEAXIS;
        sfd = DOMEAXIS;
    }
}

/* close cfd and sfd */
static void
closeChannels()
{
    if (!virtual_mode) {
        if (cfd) {
            csiClose (cfd);
            cfd = 0;
        }

        if (sfd) {
            csiClose (sfd);
            sfd = 0;
        }
    }
    else {
        sfd = 0;
        cfd = 0;
    }
}

static void d_readshpos () {
  int pos;

  if(!SHAVE || virtual_mode)
    return;

  tdlog("Getting shutter status");

  pos = csi_rix(sfd, "=get_shutter_status();");

  tdlog("Got dome position %d", pos);

  if(pos == 1) {
    SS = SH_OPEN;
  }
  else if(pos == 2) {
    SS = SH_CLOSED;
  }
}

static void d_getalarm () {
  int on;

  if(!SHAVE || virtual_mode)
    return;

  tdlog("Checking dome alarm");

  on = csi_rix(sfd, "=olevel & door_error_alarm();");
  if(on)
    on = 1;

  //if(on != telstatshmp->domealarm)
  //  tdlog("Dome alarm %s", on ? "activated" : "deactivated");

  //telstatshmp->domealarm = on;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: dome.c,v $ $Date: 2006/02/22 18:04:58 $ $Revision: 1.2 $ $Name:  $"};
