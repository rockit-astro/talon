/* manage the sounds */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <unistd.h>

#include <Xm/Xm.h>
#include <Xm/ToggleB.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "telstatshm.h"
#include "running.h"
#include "tts.h"

#include "xobs.h"
#include "widgets.h"

#define POLLSECS        5               /* secs to poll for ttsd to start */

static void pollCB (XtPointer client, XtIntervalId *id);
static void oneBeep (int percent, int pitch, int duration);
static int startTTS (void);
static void stopTTS (void);
static int chkTTS (void);

static XtIntervalId pollId;	/* also used to mean ttsd did not start */
static int beeped_last_time;
static char ttsd[] = "ttsd";

/* return 1 if we can tell sound is on, else 0.
 * keep GUI sound control up to date as well.
 * N.B. this only works if we are using the speech daemon, not just beeping.
 */
int
soundIsOn()
{
	int onhere = XmToggleButtonGetState (g_w[CSOUND_W]);
	int soundon = pollId || !chkTTS();

	/* keep controls in sync with reality */
	if (onhere != soundon) 
	    XmToggleButtonSetState (g_w[CSOUND_W], soundon, False);

	return (soundon);
}

void
soundCB (Widget w, XtPointer client, XtPointer call)
{
	/* let them turn sounds off without a confirmation */
	if (!XmToggleButtonGetState(w)) {
	    if (pollId) {
		XtRemoveTimeOut (pollId);
		pollId = (XtIntervalId) 0;
		beeped_last_time = 0;
		msg ("Beeping is off.");
	    } else {
		msg ("Speech is off.");
		toTTS ("Good bye.");
		sleep (1);
		stopTTS();
	    }
	    return;
	}

	if (!rusure (toplevel_w, "generate sounds during setup operations")) {
	    XmToggleButtonSetState (w, False, True);
	    return;
	}

	/* ok, we warned them */
	if (startTTS() < 0) {
	    msg ("Beeping is enabled.");
	    pollId = XtAppAddTimeOut (app, 0, pollCB, 0);
	} else {
	    msg ("Speech is enabled.");
	    toTTS ("Speech is enabled.");
	}
}

static void
pollCB (XtPointer client, XtIntervalId *id)
{
	int beep_this_time;

	/* beep if hunting or slewing or the like */
	switch (telstatshmp->telstate) {
	case TS_STOPPED: /* FALLTHRU */
	case TS_TRACKING: 
	    beep_this_time = 0;
	    break;
	default:
	    beep_this_time = 1;
	    break;
	}

	/* or beep if focus is moving */
	if ((OMOT->have && OMOT->cvel != 0))
	    beep_this_time = 1;

	/* beep -- just a matter of pitch */
	if (beep_this_time) {
	    oneBeep (OffTargPercent, OffTargPitch, OffTargDuration);
	    beeped_last_time = 1;
	} else if (beeped_last_time) {
	    oneBeep (OnTargPercent, OnTargPitch, OnTargDuration);
	    beeped_last_time = 0;
	}

	/* repeat */
	pollId = XtAppAddTimeOut (app, BeepPeriod, pollCB, 0);
}

static void
oneBeep (int percent, int pitch, int duration)
{
	XKeyboardControl kbc;
	unsigned long kbmask;
	kbmask = KBBellPercent | KBBellPitch | KBBellDuration;
	kbc.bell_percent = percent;
	kbc.bell_pitch = pitch;
	kbc.bell_duration = duration;
	XChangeKeyboardControl (XtDisplay(toplevel_w), kbmask, &kbc);
	XBell (XtDisplay(toplevel_w), 100);
}

/* see that ttsd is running.
 * return -1 if trouble, else 0.
 */
static int
startTTS ()
{
	char cmd[128];
	int i;

	if (chkTTS() == 0)
	    return (0);

	sprintf (cmd, "rund %s", ttsd);

	if (system (cmd) == 0) {
	    for (i = 0; i < POLLSECS; i++) {
		sleep (1);
		if (testlock_running (ttsd) == 0)
		    return (0);
	    }
	}

	return (-1);
}

/* stop ttsd */
static void
stopTTS()
{
	unlock_running (ttsd, 1);
}

/* return 0 if ttsd is running, else -1 */
static int
chkTTS()
{
	return (testlock_running(ttsd));
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: sound.c,v $ $Date: 2001/04/19 21:12:06 $ $Revision: 1.1.1.1 $ $Name:  $"};
