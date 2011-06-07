/* dialog to display and control the contrast functions
 * "contrast" refers to lo and hi pixel bandpass.
 * we also do the histogram stuff here.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/ToggleB.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/Label.h>
#include <Xm/TextF.h>
#include <Xm/DrawingA.h>
#include <Xm/Frame.h>

#include "xtools.h"
#include "fits.h"
#include "fieldstar.h"
#include "camera.h"

extern int tickmarks (double min, double max, int numdiv, double ticks[]);

#define	HISCALE		100	/* histogram multiplier for better resolution */
#define	HITICKS		5	/* number histogram tick marks */

static void autoCB (Widget w, XtPointer client, XtPointer call);
static void invCB (Widget w, XtPointer client, XtPointer call);
static void lutCB (Widget w, XtPointer client, XtPointer call);
static void aoiCB (Widget w, XtPointer client, XtPointer call);
static void aoiNotCB (Widget w, XtPointer client, XtPointer call);
static void allCB (Widget w, XtPointer client, XtPointer call);
static void lohiCB (Widget w, XtPointer client, XtPointer call);
static void newHistCB (Widget w, XtPointer client, XtPointer call);
static void hdaInCB (Widget w, XtPointer client, XEvent *ev, Boolean *cont);
static void hdaLvCB (Widget w, XtPointer client, XEvent *ev, Boolean *cont);
static void hdaRptCB (Widget w, XtPointer client, XEvent *ev, Boolean *cont);
static void hdaExpCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);
static void mkGC (void);
static int histoXPeak (int lo, int hi, int x, int wid, int *pixpeakp);

static void drawHistogram (void);

static Widget win_w;		/* main window dialog */
static Widget auto_w;		/* tb to select whether we apply hist */
static Widget wide_w;		/* tb to select wide contrast */
static Widget narrow_w;		/* tb to select narrow contrast */
static Widget aoi_w;		/* tb to select if stats apply just to AOI */
static Widget aoinot_w;		/* tb to select if stats apply to all but AOI */
static Widget all_w;		/* tb to select if stats apply to entire image*/
static Widget lohi_w;		/* tb to select if histo is just lo..hi */
static Widget hlog_w;		/* tb to select if histo shown as log() */
static Widget lotf_w;		/* window lo value text field */
static Widget hitf_w;		/* window hi value text field */
static Widget hda_w;		/* histogram drawing area */
static GC histGC;		/* histogram drawing GC */
static Pixel h_p, b_p, r_p;	/* histo, bar, and report colors */

void
manageWin ()
{
	if (!win_w)
	    createWin();

	if (XtIsManaged(win_w))
	    raiseShell (win_w);
	else
	    XtManageChild (win_w);
}

void
createWin()
{
	Widget rc_w;
	Widget rb_w;
	Widget f_w;
	Widget bw_w;
	Widget close_w;
	Widget w;
	Arg args[20];
	int n;

	/* create form */
	
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	XtSetArg (args[n], XmNfractionBase, 15); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	XtSetArg (args[n], XmNmarginWidth, 0); n++;
	win_w = XmCreateFormDialog (toplevel_w, "Win", args,n);
	
	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Contrast"); n++;
	XtSetValues (XtParent(win_w), args, n);
	XtVaSetValues (win_w, XmNcolormap, camcm, NULL);

	/* put most stuff in a row column */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	rc_w = XmCreateRowColumn (win_w, "RC", args, n);
	XtManageChild (rc_w);

	    /* auto window controls */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    auto_w = XmCreateToggleButton (rc_w, "AutoWin", args, n);
	    XtManageChild (auto_w);
	    wlprintf (auto_w, "Auto contrast");
	    XtAddCallback (auto_w, XmNvalueChangedCallback, autoCB, NULL);

	    n = 0;
	    XtSetArg (args[n], XmNmarginWidth, 20); n++;
	    rb_w = XmCreateRadioBox (rc_w, "WinRB", args, n);
	    XtManageChild (rb_w);

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		narrow_w = XmCreateToggleButton (rb_w, "Narrow", args, n);
		XtManageChild (narrow_w);
		wlprintf (narrow_w, "Narrow: -SD/3 .. +SD/3");
		XtAddCallback (narrow_w, XmNvalueChangedCallback, autoCB, NULL);

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		wide_w = XmCreateToggleButton (rb_w, "Wide", args, n);
		XtManageChild (wide_w);
		wlprintf (wide_w, "Wide: -SD .. +3*SD");
		XtAddCallback (wide_w, XmNvalueChangedCallback, autoCB, NULL);

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		w = XmCreateToggleButton (rb_w, "Full", args, n);
		XtManageChild (w);
		wlprintf (w, "Full: Min .. Max");

	    /* AOI controls */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    w = XmCreateLabel (rc_w, "AOIL", args, n);
	    XtManageChild (w);
	    wlprintf (w, "Contrast based on:");

	    n = 0;
	    XtSetArg (args[n], XmNmarginWidth, 20); n++;
	    rb_w = XmCreateRadioBox (rc_w, "AOIRB", args, n);
	    XtManageChild (rb_w);

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		aoi_w = XmCreateToggleButton (rb_w, "AOIOnly", args, n);
		XtManageChild (aoi_w);
		wlprintf (aoi_w, "AOI Only");
		XtAddCallback (aoi_w, XmNvalueChangedCallback, aoiCB, NULL);

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		aoinot_w = XmCreateToggleButton (rb_w, "AOINot", args, n);
		XtManageChild (aoinot_w);
		wlprintf (aoinot_w, "All but AOI");
		XtAddCallback (aoinot_w, XmNvalueChangedCallback, aoiNotCB, 0);

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		all_w = XmCreateToggleButton (rb_w, "All", args, n);
		XtManageChild (all_w);
		wlprintf (all_w, "Entire image");
		XtAddCallback (all_w, XmNvalueChangedCallback, allCB, 0);

		/* default is the aoi image */
		XmToggleButtonSetState (aoi_w, True, False);
		XmToggleButtonSetState (aoinot_w, False, False);
		XmToggleButtonSetState (all_w, False, False);
		state.aoistats = 1;
		state.aoinot = 0;

	    /* inverse video and luts control */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    w = XmCreateToggleButton (rc_w, "Inverse", args, n);
	    XtManageChild (w);
	    wlprintf (w, "Inverse Video");
	    XtAddCallback (w, XmNvalueChangedCallback, invCB, NULL);
	    state.inverse = XmToggleButtonGetState (w) ? 1 : 0;

	    n = 0;
	    XtSetArg (args[n], XmNmarginWidth, 20); n++;
	    rb_w = XmCreateRadioBox (rc_w, "LUTRB", args, n);
	    XtManageChild (rb_w);

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		bw_w = XmCreateToggleButton (rb_w, "BWLut", args, n);
		XtManageChild (bw_w);
		wlprintf (bw_w, "Gray Scale");
		XtAddCallback (bw_w, XmNvalueChangedCallback, lutCB,
							    (XtPointer)BW_LUT);
		if (XmToggleButtonGetState(bw_w))
		    initXPixels (BW_LUT);

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		w = XmCreateToggleButton (rb_w, "HeatLut", args, n);
		XtManageChild (w);
		wlprintf (w, "Heat");
		XtAddCallback (w, XmNvalueChangedCallback, lutCB,
							(XtPointer)HEAT_LUT);
		if (XmToggleButtonGetState(w))
		    initXPixels (HEAT_LUT);

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		w = XmCreateToggleButton (rb_w, "RainbowLut", args, n);
		XtManageChild (w);
		wlprintf (w, "Rainbow");
		XtAddCallback (w, XmNvalueChangedCallback, lutCB,
							(XtPointer)RAINBOW_LUT);
		if (XmToggleButtonGetState(w))
		    initXPixels (RAINBOW_LUT);

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		w = XmCreateToggleButton (rb_w, "StandardLut", args, n);
		XtManageChild (w);
		wlprintf (w, "Standard");
		XtAddCallback (w, XmNvalueChangedCallback, lutCB,
						    (XtPointer)STANDARD_LUT);
		if (XmToggleButtonGetState(w))
		    initXPixels (STANDARD_LUT);

		/* if _no_ buttons were on force BW as a default.
		 * N.B. don't use the lutCB -- things are not set up enough yet.
		 */
		if (state.luttype == UNDEF_LUT) {
		    XmToggleButtonSetState (bw_w, True, False);
		    initXPixels (STANDARD_LUT);
		}

	    n = 0;
	    w = XmCreateSeparator (rc_w, "Sep4", args, n);
	    XtManageChild (w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "HL", args, n);
	    wlprintf (w, "Histogram");
	    XtManageChild (w);

	    /* add a toggle to select whether histo is whole or just lo..hi */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    lohi_w = XmCreateToggleButton (rc_w, "LOHIOnly", args, n);
	    wlprintf (lohi_w, "Plot just Lo .. Hi");
	    XtAddCallback (lohi_w, XmNvalueChangedCallback, newHistCB, NULL);
	    XtManageChild (lohi_w);

	    /* add a toggle to select whether to show log() */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    hlog_w = XmCreateToggleButton (rc_w, "HistLog", args, n);
	    wlprintf (hlog_w, "Plot log(N)");
	    XtAddCallback (hlog_w, XmNvalueChangedCallback, newHistCB, NULL);
	    XtManageChild (hlog_w);

	/* show the contrast hi and lo */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNtopOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 1); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 3); n++;
	w = XmCreateLabel (win_w, "LOL", args, n);
	XtManageChild (w);
	wlprintf (w, "Lo:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNtopOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 3); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 7); n++;
	XtSetArg (args[n], XmNcolumns, 6); n++;
	lotf_w = XmCreateTextField (win_w, "LOTF", args, n);
	XtAddCallback (lotf_w, XmNactivateCallback, lohiCB, NULL);
	XtManageChild (lotf_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNtopOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 8); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 10); n++;
	w = XmCreateLabel (win_w, "HIL", args, n);
	XtManageChild (w);
	wlprintf (w, "Hi:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNtopOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 10); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 14); n++;
	XtSetArg (args[n], XmNcolumns, 6); n++;
	hitf_w = XmCreateTextField (win_w, "HITF", args, n);
	XtAddCallback (hitf_w, XmNactivateCallback, lohiCB, NULL);
	XtManageChild (hitf_w);

	/* put a Close button at the bottom */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 2); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 13); n++;
	close_w = XmCreatePushButton (win_w, "Close", args, n);
	XtAddCallback (close_w, XmNactivateCallback, closeCB, NULL);
	XtManageChild (close_w);

	/* make a drawing area for the histogram in a frame */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, hitf_w); n++;
	XtSetArg (args[n], XmNtopOffset, 5); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, close_w); n++;
	XtSetArg (args[n], XmNbottomOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 5); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightOffset, 5); n++;
	f_w = XmCreateFrame (win_w, "HF", args, n);
	XtManageChild (f_w);

	    n = 0;
	    XtSetArg (args[n], XmNheight, 150); n++;
	    hda_w = XmCreateDrawingArea (f_w, "Histogram", args, n);
	    XtAddEventHandler (hda_w, ButtonPressMask, False, hdaInCB, NULL);
	    XtAddEventHandler (hda_w, PointerMotionMask, False, hdaRptCB, NULL);
	    XtAddEventHandler (hda_w, LeaveWindowMask, False, hdaLvCB, NULL);
	    XtAddCallback (hda_w, XmNexposeCallback, hdaExpCB, NULL);
	    XtManageChild (hda_w);
}

/* called whenever we want to compute new stats and update all the displays
 * that care.
 */
void
newStats()
{
	computeStats();

	if (XmToggleButtonGetState(auto_w)) {
	    setWindow();
	    newXImage();
	}

	updateWin();
}

/* given the state.stats info, draw the Win menu display */
void
updateWin()
{
	wtprintf (lotf_w, "%d", state.lo);
	wtprintf (hitf_w, "%d", state.hi);

	drawHistogram();
}

/* call to turn off auto contrast */
void
noAutoWin()
{
	XmToggleButtonSetState (auto_w, False, True);
}

/* pick a new state.lo/hi and recompute state.lut.
 * if want auto contrast we compute from the stats, else we use the lo/hitf_w.
 */
void
setWindow()
{
	int lo, hi;

	if (XmToggleButtonGetState(auto_w)) {
	    if (XmToggleButtonGetState(wide_w)) {
		lo = (int)state.stats.mean - state.stats.sd;
		if (lo < 0)
		    lo = 0;
		hi = (int)state.stats.mean + 3*state.stats.sd;
		if (hi >= NCAMPIX)
		    hi = NCAMPIX - 1;
	    } else if (XmToggleButtonGetState(narrow_w)) {
		lo = (int)state.stats.mean - state.stats.sd/3;
		if (lo < 0)
		    lo = 0;
		hi = (int)state.stats.mean + state.stats.sd/3;
		if (hi >= NCAMPIX)
		    hi = NCAMPIX - 1;
	    } else {
		lo = state.stats.min;
		hi = state.stats.max;
	    }
	} else {
	    String s;

	    s = XmTextFieldGetString (lotf_w);
	    lo = atoi (s);
	    XtFree (s);
	    s = XmTextFieldGetString (hitf_w);
	    hi = atoi (s);
	    XtFree (s);
	}

	state.lo = lo;
	state.hi = hi;

	setLut ();
}

/* called when either the "auto win" toggle button or one of it's
 * algorithm selection toggle buttons changes state.
 * what it amounts to is "just do it" if we want auto contrasting.
 */
/* ARGSUSED */
static void
autoCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (XmToggleButtonGetState(auto_w)) {
	    setWindow();
	    updateWin();
	    updateXImage();
	}
}

/* called when the "inverse video" tb changes state.
 * what it amounts to is "just do it" if we want auto contrast.
 */
/* ARGSUSED */
static void
invCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	state.inverse = XmToggleButtonGetState (w) ? 1 : 0;

	setWindow();
	updateWin();
	updateXImage();
}

/* called to change luts.
 * client is one of the LutType enums.
 */
/* ARGSUSED */
static void
lutCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *p = (XmToggleButtonCallbackStruct *)call;

	if (p->set) {
	    LutType lt = (LutType)client;

	    initXPixels (lt);
	    setWindow();
	    updateWin();
	    updateXImage();
	}
}

/* called when the aoi-onlytoggle button changes state.
 */
/* ARGSUSED */
static void
aoiCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!XmToggleButtonGetState(w))
	    return;

	state.aoistats = 1;
	state.aoinot = 0;

	newStats();
}

/* called when the aoi-not toggle button changes state.
 */
/* ARGSUSED */
static void
aoiNotCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!XmToggleButtonGetState(w))
	    return;

	state.aoistats = 1;
	state.aoinot = 1;

	newStats();
}

/* called when the entire-image toggle button changes state.
 */
/* ARGSUSED */
static void
allCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!XmToggleButtonGetState(w))
	    return;

	state.aoistats = 0;
	state.aoinot = 0;

	newStats();
}

/* called when the lo or hi text field is activated with CR
 */
/* ARGSUSED */
static void
lohiCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	String lostr = XmTextFieldGetString (lotf_w);
	String histr = XmTextFieldGetString (hitf_w);
	int lo, hi;

	lo = atoi(lostr);
	hi = atoi(histr);
	XtFree (lostr);
	XtFree (histr);

	if (lo < 0 || lo >= NCAMPIX) {
	    msg ("Low limit must be in range 0..%d: %d", NCAMPIX-1, lo);
	    wtprintf (lotf_w, "%d", state.lo);
	    wtprintf (hitf_w, "%d", state.hi);
	    return;
	}
	if (lo >= hi) {
	    msg ("Low contrast limit must be less than hi limit: %d", lo);
	    wtprintf (lotf_w, "%d", state.lo);
	    wtprintf (hitf_w, "%d", state.hi);
	    return;
	}

	if (hi < 0 || hi >= NCAMPIX) {
	    msg ("Hi limit must be in range 0..%d: %d", NCAMPIX-1, hi);
	    wtprintf (lotf_w, "%d", state.lo);
	    wtprintf (hitf_w, "%d", state.hi);
	    return;
	}
	if (hi <= lo) {
	    msg ("Hi contrast limit must be greater than low limit: %d", hi);
	    wtprintf (lotf_w, "%d", state.lo);
	    wtprintf (hitf_w, "%d", state.hi);
	    return;
	}

	/* force back to non-auto mode */
	XmToggleButtonSetState (auto_w, False, False);

	/* update everything */
	setWindow();
	updateWin();
	updateXImage();
}

/* called when cursor leaves the drawing area */
/* ARGSUSED */
static void
hdaLvCB (w, client, ev, cont)
Widget w;
XtPointer client;
XEvent *ev;
Boolean *cont;
{
	/* redraw to erase report */
	drawHistogram();
}

/* called when there is motion in the drawing area
 */
/* ARGSUSED */
static void
hdaRptCB (w, client, ev, cont)
Widget w;
XtPointer client;
XEvent *ev;
Boolean *cont;
{
	Dimension wid;
	char str[32];
	int lo, hi;
	int peak, pixpeak;
	int rx, ry;
	int l, x;

	/* insure we have the GC and pixels needed to draw.
	 * N.B. relies on sensible background in histGC too.
	 */
	if (!histGC)
	    mkGC();
	XSetForeground (ev->xany.display, histGC, r_p);

	/* get histogram range, depending on toggle */
	if (XmToggleButtonGetState (lohi_w)) {
	    lo = (int)state.lo;
	    hi = (int)state.hi;
	} else {
	    lo = 0;
	    hi = NCAMPIX - 1;
	}

	/* find where we are and get stats */
	x = ev->xbutton.x;
	get_something (w, XmNwidth, (char *)&wid);
	peak = histoXPeak (lo, hi, x, (int)wid, &pixpeak);

	/* report pixel and peak */
	rx = 2*wid/3;	/* TODO better */
	l = sprintf (str, "Pix %5d", pixpeak);
	ry = 12;	/* TODO better */
	XDrawImageString (ev->xany.display, ev->xany.window, histGC, rx, ry,
									str, l);
	l = sprintf (str, "N %7d", peak);
	ry *= 2;	/* TODO better */
	XDrawImageString (ev->xany.display, ev->xany.window, histGC, rx, ry,
									str, l);
}

/* called when there is input activity in the drawing area
 */
/* ARGSUSED */
static void
hdaInCB (w, client, ev, cont)
Widget w;
XtPointer client;
XEvent *ev;
Boolean *cont;
{
	Dimension wid;
	int lox, hix, midx;
	int lo, hi;
	int v;
	int x;

	/* get histogram range, depending on toggle */
	if (XmToggleButtonGetState (lohi_w)) {
	    lo = (int)state.lo;
	    hi = (int)state.hi;
	} else {
	    lo = 0;
	    hi = NCAMPIX - 1;
	}

	/* can happen when no image every displayed yet */
	if (hi - lo <= 0)
	    return;

	/* figure out which half we are in and set the lo or hi endpoint.
	 * changing the text fields will update the range in setWindow()
	 * N.B. never let the new lo and hi be equal.
	 */
	x = ev->xbutton.x;
	get_something (w, XmNwidth, (char *)&wid);
	v = lo + x*(hi-lo)/(int)wid;
	lox = ((int)state.lo-lo)*(int)wid/(hi-lo);
	hix = ((int)state.hi-lo)*(int)wid/(hi-lo);
	midx = (lox + hix)/2;
	if (x < midx) {
	    if (v < hi)
		wtprintf (lotf_w, "%d", v);
	} else {
	    if (v > lo)
		wtprintf (hitf_w, "%d", v);
	}

	/* force back to manual contrast control */
	XmToggleButtonSetState (auto_w, False, False);

	/* update everything */
	setWindow();
	updateWin();
	updateXImage();
}

/* called when the histogram drawing area gets an expose
 */
/* ARGSUSED */
static void
hdaExpCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static int before;
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	XExposeEvent *e = &c->event->xexpose;

	if (c->reason != XmCR_EXPOSE) {
	    msg ("Unexpected event from hda: %d", c->reason);
	    return;
	}

	/* first time through we turn off gravity so we get expose events
	 * for either shrink or expand.
	 */

	if (!before) {
	    XSetWindowAttributes swa;
	    swa.bit_gravity = ForgetGravity;
	    XChangeWindowAttributes (e->display, e->window, 
							CWBitGravity, &swa);
	    before = 1;
	}

	/* wait for the last in the series */
	if (e->count != 0)
	    return;

	drawHistogram ();
}

/* called when the close button is activated.
 */
/* ARGSUSED */
static void
closeCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (win_w);
}

/* called when all we need do it redo the histogram and stats */
/* ARGSUSED */
static void
newHistCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	updateWin();
}

/* draw im_hist in the hda_w widget.
 * this may safely be called before the hda_w has been exposed or before an
 *   image has been read in.
 */
static void
drawHistogram ()
{
	Display *dsp = XtDisplay (hda_w);
	Window win = XtWindow(hda_w);
	Dimension wid, hei;	/* display window size; wid also = # of bins */
	int histbin[2048];	/* peak of hist in each bin; only wid used */
	int maxbin;		/* largest entry in histbin */
	int lo, hi;		/* first and last hist index to use */
	int aslog;		/* whether to show as log() */
	int i;

	if (!win || !state.fimage.image)
	    return;

	if (!histGC)
	    mkGC();

	/* fill with background */
	XClearWindow (dsp, win);

	/* get hist window size */
	get_something (hda_w, XmNwidth, (char *)&wid);
	get_something (hda_w, XmNheight, (char *)&hei);
	if (wid == 0 || hei == 0 || (int)wid > XtNumber(histbin))
	    return;

	/* beware this can happen before the winhist dialog is first up too */
	if (wid == 0)
	    return;

	/* establish histogram lo and hi */
	if (XmToggleButtonGetState(lohi_w)) {
	    lo = (int)state.lo;
	    hi = (int)state.hi;
	} else {
	    lo = 0;
	    hi = NCAMPIX-1;
	}
	if (hi == lo)
	    return;

	/* get log option */
	aslog = XmToggleButtonGetState (hlog_w);

	/* compute total count in each bin, finding max along the way.
	 */
	maxbin = 0;
	for (i = 0; i < (int)wid; i++) {
	    int binpeak = histoXPeak (lo, hi, i, (int)wid, NULL);

	    if (aslog)
		binpeak = (int)floor(log10(binpeak+1.0)*HISCALE + 0.5);
	    else
		binpeak *= HISCALE;
	    histbin[i] = binpeak;
	    if (binpeak > maxbin)
		maxbin = binpeak;
	}

	/* draw grid and histogram.
	 * guard against maxbin being 0.
	 * this can happen for a blank image or if we computed stats for
	 * all but the aoi and it was the entire image.
	 */
	if (maxbin > 0) {
	    /* connect the dots, normalized to maxbin */
	    double ticks[HITICKS+2];
	    XPoint xpts[100];
	    int npts = 0;
	    int nt;

	    /* draw horizontal scale */
	    XSetForeground (dsp, histGC, b_p);
	    if (aslog) {
		nt = (int)ceil((double)maxbin/HISCALE);
		for (i = 0; i < nt; i++) {
		    double ti = pow(10.0, (double)i);
		    int y = (int)hei - i*HISCALE*(int)hei/maxbin;  /* dw +y */
		    char str[32];

		    XDrawLine (dsp, win, histGC, 0, y, wid, y);
		    sprintf (str, "%g", ti);
		    XDrawString (dsp, win, histGC, 0, y, str, strlen(str));
		}
	    } else {
		nt = tickmarks (0.0, (double)maxbin/HISCALE, HITICKS, ticks);
		for (i = 0; i < nt; i++) {
		    double ti = ticks[i];
		    int y = (int)hei - ti*HISCALE*(int)hei/maxbin;  /* dw +y */
		    char str[32];

		    if (y < 15)
			break;	/* avoid conflict with top */

		    XDrawLine (dsp, win, histGC, 0, y, wid, y);
		    sprintf (str, "%g", ti);
		    XDrawString (dsp, win, histGC, 0, y, str, strlen(str));
		}
	    }

	    /* draw vertical scale */
	    nt = tickmarks ((double)lo, (double)hi, HITICKS, ticks);
	    for (i = 0; i < nt; i++) {
		int ti = (int)floor(ticks[i] + 0.5);
		int x = wid*(ti-lo)/(hi-lo);
		char str[32];

		if (x < 15 || x > wid-20)	/* guess at borders */
		    continue;
		XDrawLine (dsp, win, histGC, x, 0, x, hei);
		sprintf (str, "%d", ti);
		XDrawString (dsp, win, histGC, x+2, 12, str, strlen(str));
	    }

	    XSetForeground (dsp, histGC, h_p);
	    for (i = 0; i < (int)wid; i++) {
		XPoint *xp = &xpts[npts++];
		xp->x = i;
		xp->y = (int)hei - histbin[i]*(int)hei/maxbin;  /* dw +y */
		if (npts == XtNumber(xpts)) {
		    XDrawLines (dsp, win, histGC, xpts, npts, CoordModeOrigin);
		    xpts[0] = xpts[npts-1]; /* wrap to next set */
		    npts = 1;
		}
	    }
	    if (npts > 0)
		XDrawLines (dsp, win, histGC, xpts, npts, CoordModeOrigin);
	}

	/* now draw the window level bars if showing full range */
	if (!XmToggleButtonGetState (lohi_w)) {
	    XSegment xseg[2];

	    xseg[0].x1 = xseg[0].x2 = ((int)state.lo-lo)*(int)wid/(hi-lo);
	    xseg[0].y1 = 0;
	    xseg[0].y2 = hei;
	    xseg[1].x1 = xseg[1].x2 = ((int)state.hi-lo)*(int)wid/(hi-lo);
	    xseg[1].y1 = 0;
	    xseg[1].y2 = hei;

	    XSetForeground (dsp, histGC, b_p);
	    XDrawSegments (dsp, win, histGC, xseg, 2);
	}
}

static void
mkGC()
{
	Display *dsp = XtDisplay (hda_w);
	Window win = XtWindow(hda_w);
	Pixel bg_p;

	histGC = XCreateGC (dsp, win, 0L, NULL);
	get_something (hda_w, XmNbackground, (char *)&bg_p);
	XSetBackground (dsp, histGC, bg_p);
	get_something (hda_w, XmNforeground, (char *)&h_p);
	if (get_color_resource (dsp, myclass, "GlassGridColor", &b_p) < 0)
	    b_p = WhitePixel (dsp, DefaultScreen(dsp));
	if (get_color_resource (dsp, myclass, "GlassGaussColor", &r_p) < 0)
	    r_p = WhitePixel (dsp, DefaultScreen(dsp));
}

/* return the peak of state.stats.hist value near x in the histo plot window.
 * also pass back the pixel where peak occured.
 */
static int
histoXPeak (lo, hi, x, wid, pixpeakp)
int lo, hi;	/* lo and hi pixel values in window */
int x, wid;	/* x and wid in window corrds */
int *pixpeakp;	/* pixel of peak */
{
	int k0, k1, k;
	int pixpeak, binpeak;
	int b;

	/* find beginning of histogram for this column and the next */
	k0 = lo + x*(hi-lo)/wid;
	if (k0 < 0)
	    k0 = 0;
	k1 = lo + (x+1)*(hi-lo)/wid;
	if (k1 > NCAMPIX)
	    k1 = NCAMPIX-1;

	/* find the max histogram value in range [k0 .. k1) */
	binpeak = state.stats.hist[k0];
	pixpeak = k0;
	for (k = k0+1; k < k1; k++)
	    if ((b = state.stats.hist[k]) > binpeak) {
		binpeak = b;
		pixpeak = k;
	    }

	if (pixpeakp)
	    *pixpeakp = pixpeak;
	return (binpeak);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: winhist.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $"};
