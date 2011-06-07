/* this is a simple progress meter facility.
 * the graphics are easy; the harder part is using it well.
 */

#include <stdio.h>
#include <time.h>

#if defined(__STDC__)
#include <stdlib.h>
#endif

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/DrawingA.h>
#include <Xm/Label.h>
#include <Xm/Frame.h>
#include <Xm/PushB.h>

#include "P_.h"

extern Widget toplevel_w;
#define	XtD	XtDisplay(toplevel_w)
extern Colormap xe_cm;

#define	PERPREC	2		/* percentage precision of display */

extern void get_something P_((Widget w, char *resource, XtArgVal value));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void wtip P_((Widget w, char *tip));

static void pm_create_formw P_((void));
static void pm_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void pm_exp_cb P_((Widget w, XtPointer client, XtPointer call));
static void show_progress P_((int p, int all));
static void make_gc P_((void));

static Widget pm_formw;		/* the main dialog form */
static Widget pm_da;		/* graphics drawing area */
static Widget p_w;		/* percentage label */

static unsigned long pm_fgpix;	/* drawing meter foreground color */
static unsigned long pm_bgpix;	/* drawing meter background color */
static int last_per;		/* last percentage drawn */
static long t0;			/* initial time at this run */
static int p0;			/* initial percentage at this run */
static GC pm_gc;		/* GC */
static int nup;			/* up/down stack */

/* called to manage the progress meter */
void
pm_manage()
{
	if (!pm_formw)
	    pm_create_formw();

	if (!XtIsManaged(pm_formw)) {
	    XtManageChild (pm_formw);
	    nup = 1;
	}
}

/* insure the progress meter is up */
void
pm_up()
{
	if (!pm_formw)
	    pm_create_formw();

	if (!XtIsManaged(pm_formw))
	    XtManageChild (pm_formw);

	nup++;
}

/* pop an instance of the progress meter, and unmanage if goes to 0 */
void
pm_down()
{
	if (--nup <= 0) {
	    nup = 0;
	    XtUnmanageChild (pm_formw);
	}
}

/* set the given percentage on the progress meter.
 */
void
pm_set (p)
int p;
{
	if (!pm_formw || !XtIsManaged(pm_formw))
	    return;

	show_progress (p, 0);
}

void
pm_cursor(c)
Cursor c;
{
	Window win;

	if (pm_formw && (win = XtWindow(pm_formw)) != 0) {
	    Display *dsp = XtDisplay(pm_formw);
	    if (c)
		XDefineCursor(dsp, win, c);
	    else
		XUndefineCursor(dsp, win);
	}
}

/* create the progress meter dialog */
static void
pm_create_formw()
{
	Arg args[20];
	Widget fr_w, c_w;
	int n;

	/* create form */

	n = 0;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 10); n++;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	pm_formw = XmCreateFormDialog (toplevel_w, "Progress", args,n);
	set_something (pm_formw, XmNcolormap, (XtArgVal)xe_cm);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Progress"); n++;
	XtSetValues (XtParent(pm_formw), args, n);

	/* Close PB on the bottom */
	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 30); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 70); n++;
	c_w = XmCreatePushButton (pm_formw, "Close", args, n);
	XtAddCallback (c_w, XmNactivateCallback, pm_close_cb, NULL);
	wtip (c_w, "Close this dialog");
	XtManageChild (c_w);

	/* the progress label */
	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, c_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	p_w = XmCreateLabel (pm_formw, "Progress", args, n);
	wtip (p_w, "Percentage complete and time remaining");
	XtManageChild (p_w);

	/* the drawing area in a frame */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, p_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNmarginWidth, 5); n++;
	XtSetArg (args[n], XmNmarginHeight, 5); n++;
	fr_w = XmCreateFrame (pm_formw, "F", args, n);
	XtManageChild (fr_w);

	    n = 0;
	    pm_da = XmCreateDrawingArea (fr_w, "DA", args, n);
	    XtAddCallback (pm_da, XmNexposeCallback, pm_exp_cb, NULL);
	    wtip (pm_da,
		"Progress bar moves left to right to show 0 .. 100% complete");
	    XtManageChild (pm_da);
}

/* called from the Close PB */
/* ARGSUSED */
static void
pm_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (pm_formw);
}

/* called when the drawing area gets an expose.
 */
/* ARGSUSED */
static void
pm_exp_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;

	switch (c->reason) {
	case XmCR_EXPOSE: {
	    /* turn off gravity so we get expose events for either shrink or
	     * expand.
	     */
	    static int before;
	    XExposeEvent *e = &c->event->xexpose;

	    if (!before) {
		XSetWindowAttributes swa;
                unsigned long mask = CWBitGravity;

		swa.bit_gravity = ForgetGravity;
		XChangeWindowAttributes (e->display, e->window, mask, &swa);
		before = 1;
	    }

	    /* wait for the last in the series */
	    if (e->count != 0)
		return;
	    break;
	    }
	default:
	    printf ("Unexpected pm_formw event. type=%d\n", c->reason);
	    exit(1);
	}

	show_progress (last_per, 1);
}

/* display the given percentage graphically to the nearest PERPREC percentage.
 * redraw everything if all is set or if per < last_per.
 * do as little as possible if we aren't going to do any drawing.
 * update last_per when finished.
 */
static void
show_progress (p, all)
int p;
int all;
{
	int pbin;

	if (!pm_gc)
	    make_gc();

	/* round to nearest PERPREC*/
	pbin = ((p+(PERPREC/2))/PERPREC) * PERPREC;
	if (pbin > 100)
	    pbin = 100;

	/* show current percentage, and try to show time remaining */
	if (pbin < last_per || last_per == 0) {
	    /* new run when new precentage is less than prev (or first time) */
	    t0 = time(NULL);
	    p0 = pbin;
	} else if (all || pbin > last_per) {
	    long t1 = time(NULL);
	    char buf[64];

	    if (pbin-p0 >= 5 && t1-t0 >= 5) { /* minimum before estimating */
		int dt = (100-pbin)*(t1 - t0)/(pbin - p0);
		(void) sprintf (buf, "%3d%%  %3d:%02d", pbin, dt/60, dt%60);
	    } else {
		(void) sprintf (buf, "%d%%", pbin);
	    }
	    set_xmstring (p_w, XmNlabelString, buf);
	}

	/* show it graphically */
	if (all || pbin != last_per) {
	    Window win = XtWindow(pm_da);
	    Dimension daw, dah;
	    int perw;

	    get_something (pm_da, XmNwidth, (XtArgVal)&daw);
	    get_something (pm_da, XmNheight, (XtArgVal)&dah);

	    perw = pbin*(int)daw/100;

	    XSetForeground (XtD, pm_gc, pm_fgpix);
	    XFillRectangle (XtD, win, pm_gc, 0, 0, perw, (int)dah);
	    XSetForeground (XtD, pm_gc, pm_bgpix);
	    XFillRectangle (XtD, win, pm_gc, perw, 0, (int)daw-perw, (int)dah);

	    XmUpdateDisplay (toplevel_w);

	    last_per = pbin;
	}
}

/* make the pm_gc GC and extablish the pm_{f,b}gpix pixels. */
static void
make_gc()
{
	pm_gc = XCreateGC (XtD, XtWindow(pm_da), 0L, NULL);

	get_something (pm_da, XmNforeground, (XtArgVal)&pm_fgpix);
	get_something (pm_da, XmNbackground, (XtArgVal)&pm_bgpix);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: progress.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
