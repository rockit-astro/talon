/* code to support the angle/velocity measurement dialog */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/ToggleB.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/DrawingA.h>

#include "P_.h"
#include "astro.h"
#include "fits.h"
#include "wcs.h"
#include "fieldstar.h"
#include "xtools.h"
#include "camera.h"


#define	BORDER	5	/* size of border in each section, pixels */
#define	NCACHE	100	/* size of XPoints cache */
#define	MAXVTICKS 5	/* approx number of vertical tick marks in graph */
#define	MAXHTICKS 5	/* approx number of horizontal tick marks in graph */

static void createMeasure (void);
static void createGCs (Display *dsp, Window win);
static void mdaCB (Widget w, XtPointer client, XtPointer call);
static void hideCB (Widget w, XtPointer client, XtPointer call);
static void p2RefCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);
static void updateAll (void);
static void wcoords (int ix, int iy, int *wx, int *wy);
static void newRef (int ix, int iy);
static void drawPlot (PlotState ps);

static Widget measure_w;	/* main measure dialog */
static Widget tablerc_w;		/* main RC, can be turned on and off */
static Widget mda_w;		/* drawing area for cross-section plot */
static Widget p1x_w;		/* p1 x label */
static Widget p1y_w;		/* p1 y label */
static Widget p1ra_w;		/* p1 ra label */
static Widget p1dec_w;		/* p1 dec label */
static Widget p1jd_w;		/* p1 jd label */
static Widget p2x_w;		/* p2 x label */
static Widget p2y_w;		/* p2 y label */
static Widget p2ra_w;		/* p2 ra label */
static Widget p2dec_w;		/* p2 dec label */
static Widget p2jd_w;		/* p2 jd label */
static Widget delx_w;		/* delta x label */
static Widget dely_w;		/* delta y label */
static Widget delra_w;		/* delta ra label */
static Widget deldec_w;		/* delta dec label */
static Widget deljd_w;		/* delta jd label */
static Widget sep_w;		/* angular separation label */
static Widget psep_w;		/* pixel separation label */
static Widget vel_w;		/* velocity label */
static Widget min_w;		/* min pixel label */
static Widget max_w;		/* max pixel label */
static Widget mean_w;		/* mean pixel label */
static Widget median_w;		/* median pixel label */
static Widget sd_w;		/* SD label */
static GC plotGC;		/* plot GC */
static Pixel plot_p;		/* plot color */
static Pixel grid_p;		/* grid color */
static Pixel line_p;		/* line over image color */

static int p1set, p2set;	/* whether p1 and p2 are set */
static double p1ra, p1dec, p1jd;
static int p1radecok;
static int p1x, p1y;
static double p2ra, p2dec, p2jd;
static int p2radecok;
static int p2x, p2y;

static char blank[] = "             ";
#define	BLANKSZ	(sizeof(blank)-1)

/* toggle whether the measure dialog is up */
void
manageMeasure ()
{
	if (!measure_w)
	    createMeasure();

	if (XtIsManaged(measure_w))
	    raiseShell (measure_w);
	else
	    XtManageChild (measure_w);
}

/* called whenever the flipping changes */
void
flipMeasure (int lrflip, int tbflip)
{
	FImage *fip = &state.fimage;

	if (!fip->image)
	    return;
	if (!measure_w)
	    createMeasure();

	if (lrflip) {
	    if (p1set)
		p1x = fip->sw - p1x - 1;
	    if (p2set)
		p2x = fip->sw - p2x - 1;
	}

	if (tbflip) {
	    if (p1set)
		p1y = fip->sh - p1y - 1;
	    if (p2set)
		p2y = fip->sh - p2y - 1;
	}

	updateAll();
}

/* update everthing based on new loc */
void
doMeasure (PlotState ps, int x, int y, int raddecok, double ra, double dec)
{
	if (!measure_w || !XtIsManaged(measure_w))
	    return;

	if (p1set) {
	    p2x = x;
	    p2y = y;
	    p2ra = ra;
	    p2dec = dec;
	    p2radecok = raddecok;
	    (void) getRealFITS (&state.fimage, "JD", &p2jd);
	    p2set = 1;
	} else {
	    p1x = x;
	    p1y = y;
	    p1ra = ra;
	    p1dec = dec;
	    p1radecok = raddecok;
	    (void) getRealFITS (&state.fimage, "JD", &p1jd);
	    p1set = 1;
	}

	if (ps != END_PLOT)
	    updateAll();

	drawPlot(ps);
}

/* set the reference to the given position */
void
doMeasureSetRef (int ix, int iy)
{
	if (!measure_w || !XtIsManaged(measure_w))
	    return;

	newRef (ix, iy);
}

/* create the measure dialog */
static void
createMeasure()
{
	Widget f_w;
	Widget w;
	Arg args[20];
	int n;

	/* create form */
	
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_ANY); n++;
	measure_w = XmCreateFormDialog (toplevel_w, "Measure", args, n);
	
	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Rubber Band"); n++;
	XtSetValues (XtParent(measure_w), args, n);
	XtVaSetValues (measure_w, XmNcolormap, camcm, NULL);

	/* create a RowColumn box for everything */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNnumColumns, 8); n++;	/* really nrows */
	XtSetArg (args[n], XmNisAligned, False); n++;
	tablerc_w = XmCreateRowColumn (measure_w, "RC", args, n);
	XtManageChild (tablerc_w);

	    /* header */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreateLabel (tablerc_w, "H1", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreateLabel (tablerc_w, "H2", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "X");

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreateLabel (tablerc_w, "H3", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "Y");

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreateLabel (tablerc_w, "H4", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "RA");

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreateLabel (tablerc_w, "H5", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "Dec");

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreateLabel (tablerc_w, "H6", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "JD");

	    /* header seps */

	    n = 0;
	    w = XmCreateSeparator (tablerc_w, "Sep", args, n);
	    XtManageChild (w);
	    w = XmCreateSeparator (tablerc_w, "Sep", args, n);
	    XtManageChild (w);
	    w = XmCreateSeparator (tablerc_w, "Sep", args, n);
	    XtManageChild (w);
	    w = XmCreateSeparator (tablerc_w, "Sep", args, n);
	    XtManageChild (w);
	    w = XmCreateSeparator (tablerc_w, "Sep", args, n);
	    XtManageChild (w);
	    w = XmCreateSeparator (tablerc_w, "Sep", args, n);
	    XtManageChild (w);

	    /* P1 stuff */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    w = XmCreateLabel (tablerc_w, "P1L", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "Reference:");

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    p1x_w = XmCreateLabel (tablerc_w, "P1X", args, n);
	    XtManageChild (p1x_w);
	    set_xmstring (p1x_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    p1y_w = XmCreateLabel (tablerc_w, "P1Y", args, n);
	    XtManageChild (p1y_w);
	    set_xmstring (p1y_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    p1ra_w = XmCreateLabel (tablerc_w, "P1RA", args, n);
	    XtManageChild (p1ra_w);
	    set_xmstring (p1ra_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    p1dec_w = XmCreateLabel (tablerc_w, "P1Dec", args, n);
	    XtManageChild (p1dec_w);
	    set_xmstring (p1dec_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    p1jd_w = XmCreateLabel (tablerc_w, "P1JD", args, n);
	    XtManageChild (p1jd_w);
	    set_xmstring (p1jd_w, XmNlabelString, blank);

	    /* P2 stuff */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    w = XmCreateLabel (tablerc_w, "P2L", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "Target:");

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    p2x_w = XmCreateLabel (tablerc_w, "P2X", args, n);
	    XtManageChild (p2x_w);
	    set_xmstring (p2x_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    p2y_w = XmCreateLabel (tablerc_w, "P2Y", args, n);
	    XtManageChild (p2y_w);
	    set_xmstring (p2y_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    p2ra_w = XmCreateLabel (tablerc_w, "P2RA", args, n);
	    XtManageChild (p2ra_w);
	    set_xmstring (p2ra_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    p2dec_w = XmCreateLabel (tablerc_w, "P2Dec", args, n);
	    XtManageChild (p2dec_w);
	    set_xmstring (p2dec_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    p2jd_w = XmCreateLabel (tablerc_w, "P2JD", args, n);
	    XtManageChild (p2jd_w);
	    set_xmstring (p2jd_w, XmNlabelString, blank);

	    /* delta stuff */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    w = XmCreateLabel (tablerc_w, "DL", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "Delta:");

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    delx_w = XmCreateLabel (tablerc_w, "DelX", args, n);
	    XtManageChild (delx_w);
	    set_xmstring (delx_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    dely_w = XmCreateLabel (tablerc_w, "DelY", args, n);
	    XtManageChild (dely_w);
	    set_xmstring (dely_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    delra_w = XmCreateLabel (tablerc_w, "DelRA", args, n);
	    XtManageChild (delra_w);
	    set_xmstring (delra_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    deldec_w = XmCreateLabel (tablerc_w, "DelDec", args, n);
	    XtManageChild (deldec_w);
	    set_xmstring (deldec_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    deljd_w = XmCreateLabel (tablerc_w, "DelJD", args, n);
	    XtManageChild (deljd_w);
	    set_xmstring (deljd_w, XmNlabelString, blank);

	    /* second seps */

	    n = 0;
	    w = XmCreateSeparator (tablerc_w, "Sep", args, n);
	    XtManageChild (w);
	    w = XmCreateSeparator (tablerc_w, "Sep", args, n);
	    XtManageChild (w);
	    w = XmCreateSeparator (tablerc_w, "Sep", args, n);
	    XtManageChild (w);
	    w = XmCreateSeparator (tablerc_w, "Sep", args, n);
	    XtManageChild (w);
	    w = XmCreateSeparator (tablerc_w, "Sep", args, n);
	    XtManageChild (w);
	    w = XmCreateSeparator (tablerc_w, "Sep", args, n);
	    XtManageChild (w);

	    /* pixel and angle sep and velocity */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreateLabel (tablerc_w, "PSepL", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "Pixel Sep:");

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    psep_w = XmCreateLabel (tablerc_w, "PSep", args, n);
	    XtManageChild (psep_w);
	    set_xmstring (psep_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreateLabel (tablerc_w, "SepL", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "Sky Sep:");

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    sep_w = XmCreateLabel (tablerc_w, "Sep", args, n);
	    XtManageChild (sep_w);
	    set_xmstring (sep_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	    w = XmCreateLabel (tablerc_w, "VelL", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "ArcSec/Sec:");

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    vel_w = XmCreateLabel (tablerc_w, "Vel", args, n);
	    XtManageChild (vel_w);
	    set_xmstring (vel_w, XmNlabelString, blank);

	    /* line stats */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    min_w = XmCreateLabel (tablerc_w, "MinP", args, n);
	    XtManageChild (min_w);
	    set_xmstring (min_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    max_w = XmCreateLabel (tablerc_w, "MaxP", args, n);
	    XtManageChild (max_w);
	    set_xmstring (max_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    mean_w = XmCreateLabel (tablerc_w, "MeanP", args, n);
	    XtManageChild (mean_w);
	    set_xmstring (mean_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    median_w = XmCreateLabel (tablerc_w, "MedianP", args, n);
	    XtManageChild (median_w);
	    set_xmstring (median_w, XmNlabelString, blank);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    sd_w = XmCreateLabel (tablerc_w, "SdP", args, n);
	    XtManageChild (sd_w);
	    set_xmstring (sd_w, XmNlabelString, blank);

	/* bottom controls */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNfractionBase, 10); n++;
	f_w = XmCreateForm (measure_w, "F", args, n);
	XtManageChild (f_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNtopOffset, 5); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 1); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 3); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomOffset, 5); n++;
	    w = XmCreatePushButton (f_w, "P2Ref", args, n);
	    XtAddCallback (w, XmNactivateCallback, p2RefCB, NULL);
	    set_xmstring (w, XmNlabelString, "Ref <- Targ");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNtopOffset, 5); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 4); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 6); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomOffset, 5); n++;
	    w = XmCreateToggleButton (f_w, "P2Ref", args, n);
	    XtAddCallback (w, XmNvalueChangedCallback, hideCB, NULL);
	    set_xmstring (w, XmNlabelString, "Hide table");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNtopOffset, 5); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 7); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 9); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomOffset, 5); n++;
	    w = XmCreatePushButton (f_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, closeCB, NULL);
	    XtManageChild (w);

	/* drawing area -- top attachment can be changed dynamically */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, tablerc_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, f_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNheight, 150); n++;
	mda_w = XmCreateDrawingArea (measure_w, "PDA", args, n);
	XtAddCallback (mda_w, XmNresizeCallback, mdaCB, NULL);
	XtAddCallback (mda_w, XmNexposeCallback, mdaCB, NULL);
	XtManageChild (mda_w);
}

static void
createGCs (dsp, win)
Display *dsp;
Window win;
{
	if (get_color_resource (dsp, myclass, "GlassBorderColor", &plot_p)<0){
	    msg ("Can not get GlassBorderColor -- using White");
	    plot_p = WhitePixel (dsp, DefaultScreen(dsp));
	}
	if (get_color_resource (dsp, myclass, "GlassGridColor", &grid_p) < 0) {
	    msg ("Can not get GlassGridColor -- using White");
	    grid_p = WhitePixel (dsp, DefaultScreen(dsp));
	}
	if (get_color_resource (dsp, myclass, "AOIColor", &line_p) < 0){
	    msg ("Can not get AOIColor -- using White");
	    line_p = WhitePixel (dsp, DefaultScreen(dsp));
	}

	plotGC = XCreateGC (dsp, win, 0L, NULL);
}

/* called when drawing area is resized or exposed */
/* ARGSUSED */
static void
mdaCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Display *dsp = XtDisplay (w);
	Window win = XtWindow (w);

	/* might get called early */
	if (win)
	    XClearWindow (dsp, win);
}

/* called when Hide TB changes */
/* ARGSUSED */
static void
hideCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static Dimension oldw, oldh;
	int set = XmToggleButtonGetState (w);

	if (set) {
	    /* hide the table and save the mda size */
	    XtVaGetValues (mda_w, XmNwidth, &oldw, XmNheight, &oldh, NULL);
	    XtVaSetValues (mda_w, XmNtopAttachment, XmATTACH_FORM, NULL);
	    XtUnmanageChild (tablerc_w);
	} else {
	    /* show the table -- restore mda size */
	    XtVaSetValues (mda_w, XmNwidth, oldw, XmNheight, oldh, NULL);
	    XtVaSetValues (mda_w, XmNtopAttachment, XmATTACH_WIDGET,
	    	XmNtopWidget, tablerc_w, NULL);
	    XtManageChild (tablerc_w);
	}
}

/* called to make p2 the new reference */
/* ARGSUSED */
static void
p2RefCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	newRef (p2x, p2y);
}

/* called to close down */
/* ARGSUSED */
static void
closeCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (measure_w);
}

static void
updateAll()
{
	char str[32];

	if (p1set) {
	    wlprintf (p1x_w, "%*d", 6, p1x);
	    wlprintf (p1y_w, "%*d", 6, p1y);
	    if (p1radecok) {
		fs_sexa (str, radhr(p1ra), 2, 360000);
		wlprintf (p1ra_w, "%s", str);
		fs_sexa (str, raddeg(p1dec), 3, 36000);
		wlprintf (p1dec_w, "%s", str);
	    } else {
		wlprintf (p1ra_w, "%s", blank);
		wlprintf (p1dec_w, "%s", blank);
	    }
	    wlprintf (p1jd_w, "%13.5f", p1jd);
	} else {
	    wlprintf (p1x_w, "%s", blank);
	    wlprintf (p1y_w, "%s", blank);
	    wlprintf (p1ra_w, "%s", blank);
	    wlprintf (p1dec_w, "%s", blank);
	    wlprintf (p1jd_w, "%s", blank);
	}

	if (p2set) {
	    wlprintf (p2x_w, "%*d", 6, p2x);
	    wlprintf (p2y_w, "%*d", 6, p2y);
	    if (p2radecok) {
		fs_sexa (str, radhr(p2ra), 2, 360000);
		wlprintf (p2ra_w, "%s", str);
		fs_sexa (str, raddeg(p2dec), 3, 36000);
		wlprintf (p2dec_w, "%s", str);
	    } else {
		wlprintf (p2ra_w, "%s", blank);
		wlprintf (p2dec_w, "%s", blank);
	    }
	    wlprintf (p2jd_w, "%13.5f", p2jd);
	} else {
	    wlprintf (p2x_w, "%s", blank);
	    wlprintf (p2y_w, "%s", blank);
	    wlprintf (p2ra_w, "%s", blank);
	    wlprintf (p2dec_w, "%s", blank);
	    wlprintf (p2jd_w, "%s", blank);
	}

	if (p1set && p2set) {
	    double tmp, csep, sep;

	    wlprintf (delx_w, "%*d", 6, p2x-p1x);
	    wlprintf (dely_w, "%*d", 6, p2y-p1y);
	    if (p1radecok && p2radecok) {
		fs_sexa (str, radhr(p2ra-p1ra), 2, 360000);
		wlprintf (delra_w, "%s", str);
		fs_sexa (str, raddeg(p2dec-p1dec), 3, 36000);
		wlprintf (deldec_w, "%s", str);
		csep = sin(p1dec)*sin(p2dec) +
					cos(p1dec)*cos(p2dec)*cos(p2ra-p1ra);
		sep = fabs(csep) <= 1.0 ? acos(csep) : 0.0;
		fs_sexa (str, raddeg(sep), 2, 360000);
		wlprintf (sep_w, "%s", str);
		tmp = p2jd != p1jd ? raddeg(sep)/fabs(p2jd-p1jd)/24.0 : 0;
		wlprintf (vel_w, "%13.5f", tmp);
	    } else {
		wlprintf (delra_w, "%s", blank);
		wlprintf (deldec_w, "%s", blank);
		wlprintf (sep_w, "%s", blank);
		wlprintf (vel_w, "%s", blank);
	    }

	    wlprintf (deljd_w, "%13.5f", p2jd-p1jd);

	    wlprintf (psep_w, "%*.1f", 10,
				    sqrt(pow(p2x-p1x,2.) + pow(p2y-p1y,2.)));
	} else {
	    Display *dsp = XtDisplay (mda_w);
	    Window win = XtWindow (mda_w);

	    if (win)
		XClearWindow (dsp, win);

	    wlprintf (delx_w, "%s", blank);
	    wlprintf (dely_w, "%s", blank);
	    wlprintf (delra_w, "%s", blank);
	    wlprintf (deldec_w, "%s", blank);
	    wlprintf (deljd_w, "%s", blank);
	    wlprintf (sep_w, "%s", blank);
	    wlprintf (psep_w, "%s", blank);
	    wlprintf (vel_w, "%s", blank);
	}
}

/* given image coords find window coords.
 */
static void
wcoords (int ix, int iy, int *wx, int *wy)
{
	*wx = ix;
	*wy = iy;

	image2window (wx, wy);
}

/* new ref position to ix/iy */
static void
newRef (int ix, int iy)
{
	double ra, dec;

	p1x = ix;
	p1y = iy;
	p1set = 1;

	if (!xy2rd (&state.fimage, (double)ix, (double)iy, &ra, &dec)) {
	    p1ra = ra;
	    p1dec = dec;
	    p1radecok = 1;
	} else
	    p1radecok = 0;
	    
	(void) getRealFITS (&state.fimage, "JD", &p1jd);

	p2set = 0;

	msg ("x=%4d y=%4d  New cross-section reference", ix, iy);

	updateAll();
}

static void
drawPlot (PlotState ps)
{
#define	THICKLN(gc)	XSetLineAttributes(dsp,gc,2,LineSolid,CapButt,JoinMiter)
#define	THINLN(gc)	XSetLineAttributes(dsp,gc,0,LineSolid,CapButt,JoinMiter)
	static int lastp2set, lastp2wx, lastp2wy;
	static Pixel xor_p;
	Display *dsp = XtDisplay (mda_w);
	Window win = XtWindow (mda_w);
	FImage *fip = &state.fimage;
	CamPixel *image = (CamPixel *)fip->image;
	int hist[NCAMPIX];
	XPoint xpts[NCACHE];
	double hticks[MAXHTICKS+2];
	double vticks[MAXVTICKS+2];
	double sep, tsep;
	int nhticks;
	int nvticks;
	int p1wx, p1wy;
	int p2wx, p2wy;
	int nxpts;
	Dimension d;
	double sum, sum2;
	double sd, sd2;
	int mean, median;
	int daw, dah;
	int npix;
	int maxpix, minpix;
	int tmaxpix, tminpix, trange;
	int i, n, x;

	if (!plotGC)
	    createGCs (dsp, win);

	switch (ps) {
	case START_PLOT:
	    /* save new end then draw */
	    if (!p2set)
		return;
	    wcoords (p2x, p2y, &p2wx, &p2wy);
	    lastp2wx = p2wx;
	    lastp2wy = p2wy;
	    lastp2set = 1;
	    if (!p1set)
		return;
	    wcoords (p1x, p1y, &p1wx, &p1wy);
	    break;

	case RUN_PLOT:
	    /* first erase last, if any, then save new end and draw  */
	    if (!p1set)
		return;
	    wcoords (p1x, p1y, &p1wx, &p1wy);
	    if (lastp2set) {
		XSetForeground (dsp, plotGC, xor_p);
		XSetFunction (dsp, plotGC, GXxor);
		THICKLN(plotGC);
		XDrawLine (dsp, XtWindow(state.imageDA), plotGC, p1wx, p1wy,
							lastp2wx, lastp2wy);
	    }
	    if (!p2set)
		return;
	    wcoords (p2x, p2y, &p2wx, &p2wy);
	    lastp2wx = p2wx;
	    lastp2wy = p2wy;
	    lastp2set = 1;
	    break;

	case END_PLOT:
	    /* just erase last, if any */
	    if (!p1set)
		return;
	    wcoords (p1x, p1y, &p1wx, &p1wy);
	    if (lastp2set) {
		XSetForeground (dsp, plotGC, xor_p);
		XSetFunction (dsp, plotGC, GXxor);
		THICKLN(plotGC);
		XDrawLine (dsp, XtWindow(state.imageDA), plotGC, p1wx, p1wy,
							    lastp2wx, lastp2wy);
	    }
	    return;
	}

	/* get a typical pixel value and xor it to make some color */
	xor_p = line_p ^ state.lut[image[p2y*fip->sw + p2x]];

	/* draw line on image */
	XSetForeground (dsp, plotGC, xor_p);
	XSetFunction (dsp, plotGC, GXxor);
	THICKLN(plotGC);
	XDrawLine (dsp, XtWindow(state.imageDA), plotGC, p1wx, p1wy, p2wx,p2wy);
	XSetFunction (dsp, plotGC, GXcopy);

	/* get drawing area dimensions */
	get_something (mda_w, XmNwidth, (char *)&d);
	daw = (int)d;
	get_something (mda_w, XmNheight, (char *)&d);
	dah = (int)d;

	/* fresh */
	XClearWindow (dsp, win);

	/* find min and max pixel */
	maxpix = 0;
	minpix = MAXCAMPIX;
	for (x = BORDER; x < daw-BORDER; x++) {
	    double frac = (double)(x-BORDER)/(daw-2*BORDER);
	    int tix = p1x + frac*(p2x-p1x);
	    int tiy = p1y + frac*(p2y-p1y);
	    int pix = image[tiy*fip->sw + tix];
	    if (pix > maxpix)
		maxpix = pix;
	    if (pix < minpix)
		minpix = pix;
	}

	/* find vertical tick marks */
	nvticks = tickmarks ((double)minpix, (double)maxpix, MAXVTICKS, vticks);
	if (nvticks < 2)
	    return;

	/* resync with the tick marks */
	tminpix = (int)floor(vticks[0] + 0.5);
	tmaxpix = (int)floor(vticks[nvticks-1] + 0.5);
	trange = tmaxpix - tminpix;

	/* find horizontal tick marks */
	sep = sqrt(pow(p2x-p1x,2.) + pow(p2y-p1y,2.));
	nhticks = tickmarks (0.0, sep, MAXHTICKS, hticks);
	if (nhticks < 2)
	    return;

	/* total sep for tick marks */
	tsep = hticks[nhticks-1];

	/* draw grid */
	XSetForeground (dsp, plotGC, grid_p);
	THINLN(plotGC);
	for (i = 0; i < nhticks; i++) {
	    /* the vertical rules */
	    int t = BORDER + i*(daw-2*BORDER)/(nhticks-1);
	    XDrawLine (dsp, win, plotGC, t, dah-BORDER, t, BORDER);
	    if (i > 0 && i < nhticks-1) {
		char buf[64];
		(void) sprintf (buf, "%g", hticks[i]);
		XDrawString (dsp, win, plotGC, t+5, dah-BORDER,buf,strlen(buf));
	    }
	}
	for (i = 0; i < nvticks; i++) {
	    /* the horizontal rules */
	    int t = BORDER + i*(dah-2*BORDER)/(nvticks-1);
	    XDrawLine (dsp, win, plotGC, BORDER, t, daw-BORDER, t);
	    if (i > 0) {
		char buf[64];
		(void) sprintf (buf, "%d", (int)floor(vticks[nvticks-1-i]+0.5));
		XDrawString (dsp, win, plotGC, BORDER+5, t, buf, strlen(buf));
	    }
	}

	/* plot the cross section, and gather stats */
	XSetForeground (dsp, plotGC, plot_p);
	THINLN(plotGC);
	nxpts = 0;
	memset ((void *)hist, 0, sizeof(hist));
	sum = sum2 = 0;
	npix = 0;
	for (x = BORDER; x < daw-BORDER; x++) {
	    double frac = (tsep/sep)*(x-BORDER)/(daw-2*BORDER);
	    if (frac <= 1.0) {
		int tix = p1x + frac*(p2x-p1x);
		int tiy = p1y + frac*(p2y-p1y);
		int pix = (int)image[tiy*fip->sw + tix];
		int y = dah-BORDER-(pix-tminpix)*(dah-2*BORDER)/trange;
		XPoint *xp = &xpts[nxpts];

		npix++;
		hist[pix]++;
		sum += (double)pix;
		sum2 += (double)pix*(double)pix;

		xp->x = x;
		xp->y = y;
		if (++nxpts == NCACHE) {
		    XDrawLines (dsp, win, plotGC, xpts, nxpts, CoordModeOrigin);
		    xpts[0] = xpts[nxpts-1];
		    nxpts = 1;
		}
	    }
	}
	if (nxpts > 0)
	    XDrawLines (dsp, win, plotGC, xpts, nxpts, CoordModeOrigin);

	/* label the stats */
	if (npix < 2) {
	    sd = mean = 0;
	} else {
	    mean = (sum/npix + 0.5);
	    sd2 = (sum2 - sum*sum/npix)/(npix-1);
	    sd = sd2 <= 0.0 ? 0.0 : sqrt(sd2);
	}

	/* median pixel is one with equal counts below and above */
	median = n = 0;
	for (i = 0; i < NCAMPIX; i++) {
	    n += hist[i];
	    if (n >= npix/2) {
		median = i;
		break;
	    }
	}

	wlprintf (min_w, "Min:%d", minpix);
	wlprintf (max_w, "Max:%d", maxpix);
	wlprintf (mean_w, "Mean:%d", mean);
	wlprintf (median_w, "Med:%d", median);
	wlprintf (sd_w, "Std D:%.1f", sd);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: measure.c,v $ $Date: 2001/04/19 21:12:00 $ $Revision: 1.1.1.1 $ $Name:  $"};
