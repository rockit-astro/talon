/* main() for xobs.
 * (C) 1998 Elwood Charles Downey
 * we offer direct control, as well as organized delegation to supporting
 * processes.
 */

/* title string for main shell widget */
static char version[] = "1.28";

/* revision history:
 * 1.28 13 Sep 00: wait longer for telescoped, due to linux stale socket times
 * 1.27 13 Aug 00: update @ 10hz; add Pole Offset; show coords while limiting
 * 1.26 26 Jan 99: limit scrolled text total length
 * 1.25 14 Dec 98: show teltun.log when in batch mode
 * 1.24  9 Dec 98: add support for TTS
 * 1.23 12 Nov 98: never bring up batch dialog automagically
 * 1.22 10 Nov 98: use camerad for autofocus
 * 1.21  8 Nov 98: set our batch mode if telrun started elsewhere.
 * 1.20 12 Oct 98: save telshm stats with focus images.
 * 1.19  5 Oct 98: Batch shutter reports new options.
 * 1.18 24 Sep 98: change Rotator to show current PA, not error.
 * 1.17  9 Sep 98: conform title. change hidden switch to -q.
 * 1.16 21 Aug 98: flat lights; Clear => Here; new GERM/FLIP cal support.
 * 1.15  5 Aug 98: better handling of filters
 * 1.14  4 Aug 98: start telescoped if not running.
 * 1.13  1 Aug 98: change from LimitsOff to Confirm, and use TB indicators.
 * 1.12 30 Jul 98: improve batch display.
 * 1.11 28 Jul 98: clean up dome/shutter widgets wrt batch control
 * 1.10 14 Jul 98: autofocus back to stddev 
 * 1.9   8 Jun 98: paddle can now also be used from keyboard arrow keys
 * 1.8   1 Jun 98: more rework for new telescoped fifos
 * 1.7  26 May 98: rework for new telescoped fifos
 * 1.6  23 Apr 98: label Raw paddle axes from config file
 * 1.5  16 Jan 98: goose xephem more often, but only if changed or just opened;
 *		   never confirm Stop; use < and > to mean relative focus moves.
 * 1.4   2 Jan 98: add rusure to filter change
 * 1.3  26 Dec 97: better focus control behavior
 * 1.2  23 Dec 97: better lookup text field handling
 * 1.1  17 Dec 97: fix mem bug; add passive mode
 * 1.01 10 Dec 97: good enough to use
 * 0.03  9 Dec 97: try it on real hardware
 * 0.02 25 Nov 97: better widget handle scheme
 * 0.01 12 Nov 97: start
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <Xm/Xm.h>
#include <X11/Shell.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/Form.h>
#include <Xm/Separator.h>
#include <Xm/FileSB.h>
#include <Xm/MainW.h>
#include <Xm/RowColumn.h>
#include <Xm/ScrollBar.h>
#include <Xm/ToggleB.h>
#include <Xm/BulletinB.h>
#include <Xm/SelectioB.h>
#include <Xm/MessageB.h>
#include <Xm/TextF.h>
#include <Xm/DrawingA.h>
#include <Xm/Frame.h>

#include <X11/Xmu/Editres.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "strops.h"
#include "misc.h"
/* #include "db.h" */
#include "telenv.h"
#include "telstatshm.h"
#include "running.h"
#include "cliserv.h"
#include "xtools.h"
#include "xobs.h"

/* global data */
Widget toplevel_w;
XtAppContext app;
char myclass[] = "XObs";
TelStatShm *telstatshmp;
Obj sunobj, moonobj;
int xobs_alone;

#define SHMPOLL_PERIOD  100	/* statshm polling period, ms */

static void chkDaemon (char *name, char *fifo, int required, int to);
static void initShm(void);
static void onsig(int sn);
static void periodic_check (void);

static char *progname;

static XrmOptionDescRec options[] = {
    {"-q", ".quiet", XrmoptionIsArg, NULL},
};


int
main (int ac, char *av[])
{
	progname = basenm(av[0]);

	/* connect to log file */
	telOELog (progname);

	/* see whether we are alone */
	xobs_alone = lock_running(progname) == 0;

	/* connect to X server */
	toplevel_w = XtVaAppInitialize (&app, myclass, options,
					XtNumber(options), &ac, av, fallbacks,
				XmNallowShellResize, False,
				XmNiconName, myclass,
				NULL);

	/* secret switch: turn off confirms and tips if silent */
	if (getXRes (toplevel_w, "quiet", NULL)) {
	    tip_seton(0);
	    rusure_seton(0);
	}

#ifdef WANT_EDITRES
	/* support editres */
        XtAddEventHandler (toplevel_w, (EventMask)0, True,
	                                        _XEditResCheckMessages, NULL);
#endif /*  WANT_EDITRES */

	/* handle some signals */
	signal (SIGPIPE, SIG_IGN);
	signal (SIGTERM, onsig);
	signal (SIGINT, onsig);
	signal (SIGQUIT, onsig);
	signal (SIGBUS, onsig);
	signal (SIGSEGV, onsig);
	signal (SIGHUP, onsig);

	/* init stuff */
	chkDaemon ("telescoped -v", "Tel", 1, 60);	/*long for csimcd stale socket*/
	initCfg();
	initShm();
	mkGUI(version);

	/* connect fifos if alone and no telrun running */
	if (xobs_alone)
	    initPipesAndCallbacks();

	/* start a periodic timer */
	periodic_check();

	/* up */
	XtRealizeWidget(toplevel_w);

	if (!xobs_alone) {
	    msg ("Another xobs is running -- this one will remain forever passive.");
	    guiSensitive (0);
	}

	/* go */
	msg ("Welcome, telescope user.");
	XtAppMainLoop(app);

	printf ("%s: XtAppMainLoop() returned ?!\n", progname);
	return (1);     /* for lint */
}

void
die()
{
	unlock_running (progname, 0);
	exit(0);
}

static void
initShm()
{
	int shmid;
	long addr;

	shmid = shmget (TELSTATSHMKEY, sizeof(TelStatShm), 0);
	if (shmid < 0) {
	    perror ("shmget TELSTATSHMKEY");
	    unlock_running (progname, 0);
	    exit (1);
	}

	addr = (long) shmat (shmid, (void *)0, 0);
	if (addr == -1) {
	    perror ("shmat TELSTATSHMKEY");
	    unlock_running (progname, 0);
	    exit (1);
	}

	telstatshmp = (TelStatShm *) addr;
}

/* start the given daemon on the given channel if not already running.
 * wait for connect to work up to 'to' secs.
 * die if required, else ignore.
 * N.B. this is *not* where we build the permanent fifo connection.
 */
static void
chkDaemon (char *dname, char *fifo, int required, int to)
{
	char buf[1024];
	int fd[2];
	int i;

	/* ok if responding to lock */
	if (testlock_running(dname) == 0)
	    return;

	/* nope. execute it, via rund */
	sprintf (buf, "rund %s", dname);
	if (system (buf) != 0) {
	    if (required) {
		daemonLog ("Can not %s\n", buf);
		exit (1);
	    } else
		return;
	}

	/* give it a few seconds to build the fifo */
	for (i = 0; i < to; i++) {
	    sleep (1);
	    if (cli_conn (fifo, fd, buf) == 0) {
		/* ok, it's running, that's all we need to know */
		(void) close (fd[0]);
		(void) close (fd[1]);
		return;
	    }
	}

	/* no can do if get here */
	if (required) {
	    daemonLog ("Can not connect to %s's fifos: %s\n", dname, buf);
	    exit (1);
	}
}

static void
onsig(int sn)
{
	die();
}

static void
periodic_check ()
{
	updateStatus(0);
	XtAppAddTimeOut (app, SHMPOLL_PERIOD,
				     (XtTimerCallbackProc)periodic_check, 0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: xobs.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $"};
