/* code to display batch queue */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/ToggleB.h>
#include <Xm/Scale.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/Form.h>
#include <Xm/Separator.h>
#include <Xm/ArrowB.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "misc.h"
#include "xtools.h"
#include "configfile.h"
#include "telstatshm.h"
#include "running.h"
#include "cliserv.h"

#include "xobs.h"
#include "widgets.h"

static void batchManage(int whether);
static void makeBatch(void);
static void bprintf (char *fmt, ...);
static void batch_updateStatus(void);
static void guiBatch (int whether);

static Widget batch_w;			/* the main shell */

/* we build an array of widgets in batchrc_w dynamically as they are needed */
static Widget batchrc_w;
static int ibatchw, nbatchw;
static Widget *batchw;

/* return 1 if batch mode is currently on, else 0.
 * keep GUI batch control up to date as well.
 */
int
batchIsOn()
{
	int onhere = XmToggleButtonGetState (g_w[CBATCH_W]);
	int telrun = !chkTelrun();

	/* keep controls in sync with reality */
	if (onhere != telrun) {
	    if (telrun)
		batchOn();
	    else
		batchOff();
	}

	return (telrun);
}

void
batchOn()
{
	/* auto-show queue if we are a slave, else make user do it via CB */
	if (!xobs_alone)
	    batchManage(1);

	guiBatch (1);
	closePipesAndCallbacks();
	monitorTelrun(1);
}

void
batchOff()
{
	/* only really turn off things if we are alone */
	if (xobs_alone) {
	    stopTelrun();
	    initPipesAndCallbacks();
	}

	guiBatch (0);
	batchManage(0);
	monitorTelrun(0);
}

void
batchUpdate()
{
	if (!batch_w || !XtIsManaged(batch_w))
	    return;

	batch_updateStatus();
}

static void
batchManage(int whether)
{
	if (!batch_w)
	    makeBatch();

	if (whether)
	    XtManageChild (batch_w);
	else
	    XtUnmanageChild (batch_w);
}

void
batchCB (Widget w, XtPointer client, XtPointer call)
{
	int set = ((XmToggleButtonCallbackStruct *)call)->set;

	char *prompt = set ? "defer control to batch processing" :
			     "stop batch processing and resume manual control";

	if (!rusure (toplevel_w, prompt)) {
	    XmToggleButtonSetState (w, !set, False);
	    return;
	}

	if (set) {
	    /* make sure telrun is running, and turn off our controls */
	    if (chkTelrun() < 0 && startTelrun() < 0) {
		/* already issued msg() */
		XmToggleButtonSetState (w, False, False);
	    } else {
		msg ("Turning Batch mode on");
		batchOn();
		batchManage(1);
	    }
	} else {
	    /* make sure telrun is dead, and turn on our controls */
	    msg ("Turning Batch mode off");
	    batchOff();
	}

	updateStatus(1);
}

/* build the GUI.
 */
static void
makeBatch()
{
	Widget cb_w;
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_GROW); n++;
	XtSetArg (args[n], XmNtitle, "Batch Queue Status"); n++;
	XtSetArg (args[n], XmNautoUnmanage, True); n++;
	XtSetArg (args[n], XmNdefaultPosition, True); n++;
	batch_w = XmCreateFormDialog (toplevel_w, "BF", args, n);
	XtManageChild (batch_w);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 40); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 60); n++;
	cb_w = XmCreatePushButton (batch_w, "Close", args, n);
	/*
	XtAddCallback (cb_w, XmNactivateCallback, close_cb, NULL);
	*/
	XtManageChild (cb_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, cb_w); n++;
	XtSetArg (args[n], XmNbottomOffset, 10); n++;
	batchrc_w = XmCreateRowColumn (batch_w, "RC", args, n);
	XtManageChild (batchrc_w);
}

static void
batch_updateStatus()
{

	/* init which widget to use */
	ibatchw = 0;

	/* tell the story */
	if (chkTelrun() < 0) {
	    bprintf ("Batch mode is not active.");
	} else {
	    bprintf ("Observing queue is empty.");
	}

	/* clear out to as many as before */
	while (ibatchw < nbatchw)
	    XtUnmanageChild (batchw[ibatchw++]);
}

static void
bprintf (char *fmt, ...)
{
	char newbuf[256], *lastbuf;
	XmString laststr;
	va_list ap;
	Arg args[20];
	Widget w;
	int n;

	/* if never used this before, make it now */
	if (ibatchw >= nbatchw) {
	    batchw = (Widget *) XtRealloc((void *)batchw,
						(++nbatchw)*sizeof(Widget));
	    n = 0;
	    batchw[ibatchw] = XmCreateLabel (batchrc_w, "L", args, n);
	}

	/* up we go */
	XtManageChild (batchw[ibatchw]);

	/* use ibatchw and advance */
	w = batchw[ibatchw++];

	va_start (ap, fmt);
	vsprintf (newbuf, fmt, ap);
	va_end (ap);

	XtSetArg (args[0], XmNlabelString, &laststr);
	XtGetValues (w, args, 1);
	XmStringGetLtoR (laststr, XmSTRING_DEFAULT_CHARSET, &lastbuf);

	if (strcmp (newbuf, lastbuf)) {
	    XmString str = XmStringCreateLtoR (newbuf,XmSTRING_DEFAULT_CHARSET);
	    XtSetArg (args[0], XmNlabelString, str);
	    XtSetValues (w, args, 1);
	    XmStringFree (str);
	}

	XmStringFree (laststr);
	XtFree (lastbuf);
}

static void
guiBatch (int whether)
{
	XmToggleButtonSetState (g_w[CBATCH_W], whether, False);

	guiSensitive (!whether);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: batch.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $"};
