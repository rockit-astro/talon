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
static double	SHUTTERTO;
static double	dome_to;	/* mjd when operation will timeout */

/* the current activity, if any */
static void (*active_func) (int first, ...);

/* one of these... */
static void dome_poll(void);
static void dome_reset(int first, ...);
static void dome_open(int first, ...);
static void dome_close(int first, ...);
static void dome_stop(int first, ...);
static void dome_alarmset(int first, int alon);

/* helped along by these... */
static int d_emgstop(char *msg);
static int d_chkWx(char *msg);
static void d_stop(void);
static void initCfg(void);
static void openChannels(void);
static void closeChannels(void);
static void d_getalarm(void);

/* later entries */
static char * doorType(void);
static char * enclType(void);


#define	SS		(telstatshmp->shutterstate)
#define	SMOVING		(SS == SH_OPENING || SS == SH_CLOSING)
#define	SHAVE		(SS != SH_ABSENT)

/* control and status connections */
static int cfd = 0, sfd = 0;

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
    if (!SHAVE) {
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
        if (SHAVE) {
            vmcService(DOMEAXIS);
        }
    }

    if (active_func)
        (*active_func)(0);
}

/* read config files, stop dome; don't mess much with shutter state */
static void
dome_reset (int first, ...)
{
    Now *np = &telstatshmp->now;

    if (first) {
        initCfg();

        if (virtual_mode) {
            // set the virtual dome controller up
            vmcSetup(DOMEAXIS,.1,.1,0,0);
        }

        if (SHAVE) {
            openChannels();
            d_stop();
            if (SHAVE && SMOVING) {
                SS = SH_IDLE;
            }
            active_func = dome_reset;
        } else {
        	closeChannels();
        }
    }

    active_func = NULL;
    if (SHAVE)
    {
        fifoWrite (Dome_Id, 0, "Reset complete");
        d_stop();
    
        if (!virtual_mode) {
            /* Version 1.5 or greater of nodeDome.cmc */
            // set the encoder steps (used by script!) to value in cfg file
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

/* Some terminology about door... if we have a dome, it's a shutter, if not it's a roof...*/
static char * doorType(void)
{
    return ("roof");
}

static char * enclType(void)
{
    return("roof");
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
            telstatshmp->domealarm = 1;
        }
        else
            /* Alarm has been reset */
            telstatshmp->domealarm = 0;

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
        telstatshmp->domealarm = 0;
        return;
    }

    /* ok! */
    fifoWrite (Dome_Id, 0, "Open complete");
    toTTS ("The %s is now open.", doorType());
    telstatshmp->domealarm = 0;
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

        if (n == -2) {
            tdlog("Dome alarm activated");
            telstatshmp->domealarm = 1;
        }
        else
            telstatshmp->domealarm = 0;

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
        telstatshmp->domealarm = 0;
        return;
    }

    /* ok! */
    fifoWrite (Dome_Id, 0, "Close complete");
    toTTS ("The %s is now closed.", doorType());
    telstatshmp->domealarm = 0;
    SS = SH_CLOSED;
    active_func = NULL;
}

/* stop everything */
static void
dome_stop (int first, ...)
{
    Now *np = &telstatshmp->now;

    if (!SHAVE) {
        fifoWrite (Dome_Id, 0, "Ok, but nothing to stop really");
        return;
    }

    if (first) {
    	d_stop();
        dome_to = mjd + SHUTTERTO;
        active_func = dome_stop;
    }

    if (mjd > dome_to) {
        fifoWrite (Dome_Id, -5, "Stop timed out");
        toTTS ("Stop of %s timed out.",enclType());
        d_stop();	/* ?? */
        active_func = NULL;
        return;
    }

    active_func = NULL;
    fifoWrite (Dome_Id, 0, "Stop complete");
    toTTS ("The %s is now stopped.", enclType());
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

        telstatshmp->domealarm = alon;
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
    return 0;
}

/* if a weather alert is in progress respond and return 1, else return 0 */
static int
d_chkWx(char *msg)
{
    // TODO: Implement heartbeat timeout
    int wxalert = 0;

    if (!wxalert || !SHAVE)
        return(0);

    if (msg || (active_func && active_func != dome_close))
        fifoWrite (Dome_Id,
                   -16, "Command cancelled.. weather alert in progress");

    if (active_func != dome_close && SS != SH_CLOSED) {
        fifoWrite (Dome_Id, 9, "Weather alert asserted -- closing %s", doorType());
        dome_close (1);
    }

    dome_poll();

    return (1);
}


/* initiate a stop */
static void
d_stop(void)
{
    if (!virtual_mode) {
        if (cfd) {
            csi_intr (cfd);
            csiDrain(cfd);
            csi_w (cfd, "roofseek(0);");
            csiDrain(cfd);
        }
    } else {
        vmc_w(DOMEAXIS, "roofseek(0);");
    }
}


/* (re) read the dome confi file */
static void
initCfg()
{
#define NDCFG   (sizeof(dcfg)/sizeof(dcfg[0]))

    static int SHUTTERHAVE;
    static CfgEntry dcfg[] = {
        {"DOMEAXIS",	CFG_INT, &DOMEAXIS},
        {"SHUTTERHAVE",	CFG_INT, &SHUTTERHAVE},
        {"SHUTTERTO",	CFG_DBL, &SHUTTERTO},
    };
    int n;

    /* read the file */
    n = readCfgFile (1, dcfn, dcfg, NDCFG);
    if (n != NDCFG) {
        cfgFileError (dcfn, n, (CfgPrFp)tdlog, dcfg, NDCFG);
        die();
    }

    /* some effect shm -- but try not to disrupt already useful info */
    if (!SHUTTERHAVE)
        SS = SH_ABSENT;
    else if (SS == SH_ABSENT)
        SS = SH_IDLE;
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

static void d_getalarm () {
  int on;

  if (!SHAVE || virtual_mode)
    return;

  tdlog("Checking dome alarm");

  on = csi_rix(sfd, "=olevel & door_error_alarm();");
  if (on)
      on = 1;

  if (on != telstatshmp->domealarm)
      tdlog("Dome alarm %s", on ? "activated" : "deactivated");

  telstatshmp->domealarm = on;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: dome.c,v $ $Date: 2006/02/22 18:04:58 $ $Revision: 1.2 $ $Name:  $"};
