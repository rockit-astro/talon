/* dialog to allow control of the base stuff like mag, flip, aoi */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/ToggleB.h>
#include <Xm/Separator.h>
#include <Xm/TextF.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>

#include "P_.h"
#include "astro.h"
#include "xtools.h"
#include "fits.h"
#include "fieldstar.h"
#include "telenv.h"
#include "ps.h"
#include "camera.h"

#define	PLOTSZ		30		/* plot size, pixels */
#define	PLOTTICS	5		/* number of tick marks each axis */

static void cropCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);
void resetCB (Widget w, XtPointer client, XtPointer call);
static void plotAOICB (Widget w, XtPointer client, XtPointer call);
static void addTicks (FILE *fp, char axis, int start, int len, int bin);
static void flipCB (Widget w, XtPointer client, XtPointer call);
static void mtbCB (Widget w, XtPointer client, XtPointer call);
static void magSet (int numerator);
static void makeGC(void);

static void createDump (void);
static void dumpAOICB (Widget w, XtPointer client, XtPointer call);
static void dumpCancelCB (Widget w, XtPointer client, XtPointer call);
static void dumpOKCB (Widget w, XtPointer client, XtPointer call);

static Widget basic_w;	/* main basic dialog */
Widget crop_w;	/* cropping on togglebutton. Remove static: KMI 10/20/03 */
static Widget aoix_w;	/* AOI X label */
static Widget aoiy_w;	/* AOI Y label */
static Widget aoiw_w;	/* AOI W label */
static Widget aoih_w;	/* AOI H label */
static Widget mean_w;
static Widget median_w;
static Widget sd_w;
static Widget min_w;
static Widget max_w;
static Widget maxat_w;
static Widget maxsd_w;
static Widget dump_w;
static Widget dumpfn_w;

/* KMI NOTE: If you change the size of this array, also update
             NUM_MTB in main.c       */
MTB mtb[] = {
    {"Mag1_8", "1/8 X", MAGDENOM/8},
    {"Mag1_4", "1/4 X", MAGDENOM/4},
    {"Mag1_2", "1/2 X", MAGDENOM/2},
    {"Mag1",   " 1  X", MAGDENOM},
    {"Mag2",   " 2  X", MAGDENOM*2},
    {"Mag4",   " 4  X", MAGDENOM*4},
};

typedef enum {
    FLIP_TB, FLIP_LR
} FlipCodes;

static GC aoiGC;	/* GC to use to draw the AOI */

/* toggle whether the Basic dialog is up */
void
manageBasic ()
{
	if (!basic_w)
	    createBasic();

	if (XtIsManaged(basic_w))
	    raiseShell (basic_w);
	else
	    XtManageChild (basic_w);
}

/* create the Basic dialog */
void
createBasic()
{
	Widget rb_w, rc_w;
	Widget w;
	Arg args[20];
	int t;
	int n;

	/* create form */
	
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_GROW); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	basic_w = XmCreateFormDialog (toplevel_w, "Basic", args,n);
	
	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Basic Ops"); n++;
	XtSetValues (XtParent(basic_w), args, n);
	XtVaSetValues (basic_w, XmNcolormap, camcm, NULL);

	/* create a vertical RowColumn for everything */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	XtSetArg (args[n], XmNisAligned, True); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	rc_w = XmCreateRowColumn (basic_w, "RC", args, n);
	XtManageChild (rc_w);

	/* put the options in a radio box */

	n = 0;
	XtSetArg (args[n], XmNmarginWidth, 0); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	XtSetArg (args[n], XmNisAligned, True); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	rb_w = XmCreateRadioBox (rc_w, "RB", args, n);
	XtManageChild (rb_w);

	for (t = 0; t < XtNumber(mtb); t++) {
		MTB *mtbp = &mtb[t];

		n = 0;
		w = XmCreateToggleButton (rb_w, mtbp->name, args, n);
		XtManageChild (w);
		wlprintf (w, mtbp->label);
		XtAddCallback (w, XmNvalueChangedCallback, mtbCB, 
						(XtPointer) mtbp->numerator);
		if (XmToggleButtonGetState (w))
		    state.mag = mtbp->numerator;
		mtbp->w = w;
	}

	/* flipper toggles */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (w);

	n = 0;
	w = XmCreateToggleButton (rc_w, "FLR", args, n);
	XtAddCallback (w, XmNvalueChangedCallback, flipCB, (XtPointer)FLIP_LR);
	wlprintf (w, "Flip L/R");
	XtManageChild (w);

	n = 0;
	w = XmCreateToggleButton (rc_w, "FTB", args, n);
	XtAddCallback (w, XmNvalueChangedCallback, flipCB, (XtPointer)FLIP_TB);
	wlprintf (w, "Flip T/B");
	XtManageChild (w);

	/* mark AOI section */

	n = 0;
	XtSetArg (args[n], XmNseparatorType, XmDOUBLE_LINE); n++;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (w);

	n = 0;
	w = XmCreateLabel (rc_w, "AOIH", args, n);
	wlprintf (w, "AOI");
	XtManageChild (w);

	/* AOI reset button */
	n = 0;
	w = XmCreatePushButton (rc_w, "Reset", args, n);
	XtManageChild (w);
	wlprintf (w, "Reset");
	XtAddCallback (w, XmNactivateCallback, resetCB, NULL);

	/* plot AOI button */
	n = 0;
	w = XmCreatePushButton (rc_w, "Plot", args, n);
	XtManageChild (w);
	wlprintf (w, "3-D Gnuplot");
	XtAddCallback (w, XmNactivateCallback, plotAOICB, NULL);

	/* dump AOI button */
	n = 0;
	w = XmCreatePushButton (rc_w, "Dump", args, n);
	XtManageChild (w);
	wlprintf (w, "ASCII Export");
	XtAddCallback (w, XmNactivateCallback, dumpAOICB, NULL);

	/* crop control toggle button */

	n = 0;
	crop_w = XmCreateToggleButton (rc_w, "Crop", args, n);
	XtManageChild (crop_w);
	XtAddCallback (crop_w, XmNvalueChangedCallback, cropCB, NULL);
	wlprintf (crop_w, "Crop to");
	state.crop = XmToggleButtonGetState (crop_w);

	/* make aoi size values */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (w);

	n = 0;
	aoix_w = XmCreateLabel (rc_w, "X", args, n);
	XtManageChild (aoix_w);

	n = 0;
	aoiy_w = XmCreateLabel (rc_w, "Y", args, n);
	XtManageChild (aoiy_w);

	n = 0;
	aoiw_w = XmCreateLabel (rc_w, "W", args, n);
	XtManageChild (aoiw_w);

	n = 0;
	aoih_w = XmCreateLabel (rc_w, "H", args, n);
	XtManageChild (aoih_w);

	/* aoi stats */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (w);

	n = 0;
	mean_w = XmCreateLabel (rc_w, "H", args, n);
	XtManageChild (mean_w);

	n = 0;
	median_w = XmCreateLabel (rc_w, "H", args, n);
	XtManageChild (median_w);

	n = 0;
	sd_w = XmCreateLabel (rc_w, "H", args, n);
	XtManageChild (sd_w);

	n = 0;
	min_w = XmCreateLabel (rc_w, "H", args, n);
	XtManageChild (min_w);

	n = 0;
	max_w = XmCreateLabel (rc_w, "H", args, n);
	XtManageChild (max_w);

	n = 0;
	maxat_w = XmCreateLabel (rc_w, "At", args, n);
	XtManageChild (maxat_w);

	n = 0;
	maxsd_w = XmCreateLabel (rc_w, "MSD", args, n);
	XtManageChild (maxsd_w);

	/* make the close button */
	n = 0;
	w = XmCreatePushButton (rc_w, "Close", args, n);
	XtManageChild (w);
	XtAddCallback (w, XmNactivateCallback, closeCB, NULL);
}

/* handle the operation of the AOI */
void
doAOI (ps, x, y)
PlotState ps;
int x, y;		/* image coords of cursor */
{
	static AOI localAOI;	/* don't interfere with image2window() */

	if (ps == START_PLOT) {

	    /* set initial location and size of candidate AOI.
	     * take care that it has non-0 width allowing for mag; must be
	     * > 0 in any case for aoi stat computations later.
	     */

	    localAOI.x = x;
	    localAOI.y = y;

	    if (state.mag < MAGDENOM) {
		localAOI.w = MAGDENOM/state.mag;
		localAOI.h = MAGDENOM/state.mag;
	    } else {
		localAOI.w = 10;
		localAOI.h = 10;
	    }

	    drawAOI (True, &localAOI);

	} else if (ps == RUN_PLOT) {

	    /* track new AOI */

	    /* only allow growing right and down.
	     * beware of edges.
	     */

	    if (x >= state.fimage.sw)
		x = state.fimage.sw - 1;
	    if (x > localAOI.x)
		localAOI.w = x - localAOI.x + 1;	/* inclusive */

	    if (y >= state.fimage.sh)
		y = state.fimage.sh - 1;
	    if (y > localAOI.y)
		localAOI.h = y - localAOI.y + 1;	/* inclusive */

	    drawAOI (True, &localAOI);

	} else if (ps == END_PLOT) {

	    /* update the real AOI now and
	     * notify all screens that care
	     */

	    state.aoi = localAOI;
	    updateAOI();

	    if (state.aoistats || state.crop) {
		computeStats();
		setWindow();
		updateWin();

		/* TODO: the following call starts all over. that is ok but if
		 * the mag is so big there are scroll bars now they get slid
		 * back. unless we are cropping too, all we really need do now
		 * is call FtoXImage() to recompute the lut and then force
		 * an expose.
		 */
		newXImage();
	    }
	}
}

/* call this to draw an AOI at *aoip.
 * we save pixels underneath each time and can optionally restore them.
 */
void
drawAOI(restore, aoip)
Bool restore;
AOI *aoip;
{
	static int lastx, lasty;		/* last loc, window coords */
	static unsigned lastw, lasth;		/* last size, window coords */
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);
	int x, y;				/* window coords */
	unsigned w, h;

	/* restore if asked to */

	if (restore) {
	    x = lastx;
	    y = lasty;
	    w = lastw;
	    h = lasth;

	    XPutImage (dsp, win, state.daGC, state.ximagep,
						x, y,     x, y,     w-1, 1);
	    XPutImage (dsp, win, state.daGC, state.ximagep,
						x, y+h-1, x, y+h-1, w-1, 1);
	    XPutImage (dsp, win, state.daGC, state.ximagep,
						x, y,     x, y,     1, h-1);
	    XPutImage (dsp, win, state.daGC, state.ximagep,
						x+w-1, y, x+w-1, y, 1, h);
	}

	/* convert from image to window coords */

	x = aoip->x;
	y = aoip->y;
	image2window (&x, &y);

	w = aoip->w;
	h = aoip->h;
	w = w*state.mag/MAGDENOM;	/* don't use *= */
	h = h*state.mag/MAGDENOM;	/* don't use *= */

	/* save location of rectangle we are about to draw */

	lastx = x;
	lasty = y;
	lastw = w;
	lasth = h;

	/* draw the AOI */

	if (!aoiGC)
	    makeGC();
	XPSDrawRectangle (dsp, win, aoiGC, x, y, w-1, h-1);
}

/* update the displayed values for the aoi */
void
updateAOI()
{
	FImage *fip = &state.fimage;
	char buf[1024];
	StarDfn sd;
	StarStats ss;

	wlprintf (aoix_w, "X: %4d", state.aoi.x);
	wlprintf (aoiy_w, "Y: %4d", state.aoi.y);
	wlprintf (aoiw_w, "W: %4d", state.aoi.w);
	wlprintf (aoih_w, "H: %4d", state.aoi.h);

	aoiStatsFITS (fip->image, fip->sw, state.aoi.x, state.aoi.y,
				    state.aoi.w, state.aoi.h, &state.aoiStats);

	wlprintf (mean_w,   "Mean:   %6d", state.aoiStats.mean);
	wlprintf (median_w, "Median: %6d", state.aoiStats.median);
	wlprintf (sd_w,     "Std D: %7.1f", state.aoiStats.sd);
	wlprintf (min_w,    "Min:    %6d", state.aoiStats.min);
	wlprintf (max_w,    "Max:    %6d", state.aoiStats.max);
	wlprintf (maxat_w,  " at: %4d %4d", state.aoiStats.maxx,
							state.aoiStats.maxy);
	sd.rsrch = 10;
	sd.rAp = 0;
	sd.how = SSHOW_HERE;
	if (starStats ((CamPixel *)fip->image, fip->sw, fip->sh, &sd, 
		    state.aoiStats.maxx, state.aoiStats.maxy, &ss, buf) == 0)
	    wlprintf (maxsd_w,  " SD>M:%8.1f", (ss.p - ss.Sky)/ss.rmsSky);
	else
	    wlprintf (maxsd_w,  "              ");
}

/* set state.aoi to the initial desired size; 
 * unless the image is already cropped (as evidenced by the
 * existance of CROPX/CROPY FITS fields) we set it a little
 * in from the outside because all CCDs have some sort of
 * support somewhere that masks the images.
 */
void
resetAOI()
{
#define	CCDMASKWIDTH	32
	int x;

	if (getIntFITS (&state.fimage, "CROPX", &x) < 0) {
	    state.aoi.x = CCDMASKWIDTH;
	    state.aoi.y = CCDMASKWIDTH;
	    state.aoi.w = state.fimage.sw - 2*CCDMASKWIDTH;
	    state.aoi.h = state.fimage.sh - 2*CCDMASKWIDTH;
	} else {
	    state.aoi.x = 0;
	    state.aoi.y = 0;
	    state.aoi.w = state.fimage.sw;
	    state.aoi.h = state.fimage.sh;
	}

	updateAOI();
}

void
resetCrop()
{
	state.crop = 0;
	XmToggleButtonSetState (crop_w, False, False);
}

/* called when the crop toggle button changes state.
 */
/* ARGSUSED */
static void
cropCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	state.crop = XmToggleButtonGetState (w) ? 1 : 0;

	/* if turning off crop, reduce mag to no more than unity since the
	 * window is likely to become huge
	 */
	if (!state.crop && state.mag > MAGDENOM)
	    magSet (MAGDENOM);

	newXImage();
}

/* called when the close push button is activated.
 */
/* ARGSUSED */
static void
closeCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (basic_w);
}

/* called when the reset push button is activated.
 */
/* ARGSUSED */
void
resetCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* reduce mag to no more than unity since the
	 * window is likely to become huge
	 */
	if (state.mag > MAGDENOM)
	    magSet (MAGDENOM);

	resetAOI();
	resetCrop();
	computeStats();
	setWindow();
	updateWin();
	newXImage();
}

/* called when the plot AOI button is activated.
 */
/* ARGSUSED */
static void
plotAOICB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	FImage *fip = &state.fimage;
	AOI *aoip = &state.aoi;
	char datafn[L_tmpnam];
	char cmdfn[L_tmpnam];
	char gnucmd[1024];
	CamPixel *ip;
	AOIStats as;
	FILE *fp;
	int base;
	int ssz;
	int y;

	/* find base pixel */
	aoiStatsFITS (fip->image,fip->sw,aoip->x,aoip->y,aoip->w,aoip->h,&as);
	base = as.min - 1;	/* to allow for log() */

	/* find sample size to yield PLOTSZ points in smallest dimension */
	ssz = (aoip->h < aoip->w ? aoip->h : aoip->w)/PLOTSZ;
	if (ssz < 1)
	    ssz = 1;

	/* save aoi in data file, sans base, sampled to PLOTSZ, rows flipped */
	//tmpnam (datafn);
	mkstemp(datafn);
	fp = fopen (datafn, "w");
	if (!fp) {
	    msg ("%s: %s", datafn, strerror(errno));
	    return;
	}
	ip = (CamPixel *)fip->image;
	for (y = aoip->y + aoip->h - ssz; y >= aoip->y; y -= ssz) {
	    CamPixel *erow, *row = &ip[y*fip->sw + aoip->x];
	    for (erow = row + aoip->w; row < erow; row += ssz) {
		int sx, sy;
		int maxs = (int)*row;
		for (sy = 0; sy < ssz; sy++) {
		    CamPixel *srow = &row[sy*fip->sw];
		    for (sx = 0; sx < ssz; sx++) {
			int s = (int)*srow++;
			if (s > maxs)
			    maxs = s;
		    }
		}

		fprintf (fp, "%5d\n", maxs - base);
	    }
	    fprintf (fp, "\n");
	}
	fclose (fp);
	
	/* build the initial gnuplot command file */
	//tmpnam (cmdfn);
	mkstemp(cmdfn);
	fp = fopen (cmdfn, "w");
	if (!fp) {
	    msg ("%s: %s", cmdfn, strerror(errno));
	    return;
	}
	fprintf (fp, "set title 'Camera Log Plot of AOI at %dx%d+%d+%d'\n",
					aoip->w, aoip->h, aoip->x, aoip->y);
	fprintf (fp, "set data style lines\n");
	fprintf (fp, "set view 60,30\n");
	fprintf (fp, "set contour\n");
	fprintf (fp, "set xlabel 'X'\n");
	fprintf (fp, "set ylabel 'Y'\n");
	fprintf (fp, "set zlabel 'Pixel'\n");
	fprintf (fp, "set logscale z\n");
	fprintf (fp, "set hidden3d\n");
	addTicks (fp, 'x', aoip->x, aoip->w, ssz);
	addTicks (fp, 'y', aoip->y, aoip->h, ssz);
	fprintf (fp, "splot '%s'\n", datafn);
	fclose (fp);

	/* run gnuplot in its own xterm, remove temp files when finished.
	 * N.B. gnuplot doesn't seem to find .gnuplot in cwd, just HOME.
	 */
	msg("Plotting %s via gnuplot %s", datafn, cmdfn);
	sprintf (gnucmd, "(xterm -geometry 80x24 -title Gnuplot -e sh -c 'cd /tmp; HOME=/tmp; echo load \\\"%s\\\" > .gnuplot; gnuplot; rm -f .gnuplot %s %s') &", cmdfn, cmdfn, datafn);
	if (system (gnucmd) < 0)
	    msg ("Gnuplot failed");
}

/* add tick marks to the plot */
static void
addTicks (FILE *fp, char axis, int start, int len, int bin)
{
	double ticks[PLOTTICS+2];
	int i, n;

	n = tickmarks ((double)start, (double)(start+len), PLOTTICS, ticks);
	fprintf (fp, "set %ctics (", axis);
	for (i = 0; i < n; i++) {
	    int x = (int)((ticks[i]-start)/bin);
	    fprintf (fp, "%s'%g' %d", i == 0 ? "" : ", ", ticks[i], 
					    axis == 'x' ? x : len/bin - x);
	}

	fprintf (fp, ")\n");
}

static void
makeGC()
{
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);
	XGCValues gcv;
	unsigned int gcm;
	Pixel aoip;

	if (get_color_resource (dsp, myclass, "AOIColor", &aoip) < 0) {
	    msg ("Can't get AOIColor -- using White");
	    aoip = WhitePixel (dsp, DefaultScreen(dsp));
	}
	gcm = GCForeground;
	gcv.foreground = aoip;
	aoiGC = XCreateGC (dsp, win, gcm, &gcv);
}

/* set a new mag factor */
static void
magSet (int numerator)
{
	int i;

	for (i = 0; i < XtNumber(mtb); i++)
	    if (mtb[i].numerator == numerator) {
		XmToggleButtonSetState (mtb[i].w, True, True);
		return;
	    }

	printf ("magSet(%d) not found\n", numerator);
}

/* called when either flip TB changes.
 * client is FLIP_LR or FLIP_TB.
 */
/* ARGSUSED */
static void
flipCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int set = XmToggleButtonGetState (w);
	FlipCodes code = (int)client;
	FImage *fip = &state.fimage;

	switch (code) {
	case FLIP_LR:
	    state.lrflip = set;
	    if (!fip->image)
		return;
	    state.aoi.x = fip->sw - state.aoi.w - state.aoi.x;
	    flipMeasure (1, 0);
	    flipImgCols ((CamPixel *)fip->image, fip->sw, fip->sh);
	    newXImage();
	    updateAOI();	/* to show new pixel coords */
	    break;
	case FLIP_TB:
	    state.tbflip = set;
	    if (!fip->image)
		return;
	    state.aoi.y = fip->sh - state.aoi.h - state.aoi.y;
	    flipMeasure (0, 1);
	    flipImgRows ((CamPixel *)fip->image, fip->sw, fip->sh);
	    newXImage();
	    updateAOI();	/* to show new pixel coords */
	    break;
	default:
	    fprintf (stderr, "Unknown flip code:%d\n", code);
	    break;
	}
}

/* called when a magnification toggle button changes state.
 * client is the new mag value as a numerator over MAGDENOM.
 */
/* ARGSUSED */
static void
mtbCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!XmToggleButtonGetState(w))
	    return;

	state.mag = (int) client;
	newXImage();
}

/* create a dump prompt */
static void
createDump()
{
	Widget w;
	Widget sep_w;
	Arg args[20];
	int n;

	/* create form */
	
	n = 0;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 10); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_ANY); n++;
	XtSetArg (args[n], XmNfractionBase, 10); n++;
	dump_w = XmCreateFormDialog (toplevel_w, "Dump", args, n);
	
	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "ASCII Dump"); n++;
	XtSetValues (XtParent(dump_w), args, n);
	XtVaSetValues (dump_w, XmNcolormap, camcm, NULL);

	/* filename prompt */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	dumpfn_w = XmCreateTextField (dump_w, "DTF", args, n);
	XtAddCallback (dumpfn_w, XmNactivateCallback, dumpOKCB, NULL);
	XtManageChild (dumpfn_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, dumpfn_w); n++;
	w = XmCreateLabel (dump_w, "DTF", args, n);
	XtManageChild (w);
	set_xmstring (w, XmNlabelString, "Filename: ");
	
	/* sep */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, dumpfn_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (dump_w, "Sep", args, n);
	XtManageChild (sep_w);

	/* controls */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 2); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 4); n++;
	w = XmCreatePushButton (dump_w, "Ok", args, n);
	XtAddCallback (w, XmNactivateCallback, dumpOKCB, NULL);
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 6); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 8); n++;
	w = XmCreatePushButton (dump_w, "Cancel", args, n);
	XtAddCallback (w, XmNactivateCallback, dumpCancelCB, NULL);
	XtManageChild (w);
}

/* called when the dump Cancel button is activated.
 */
/* ARGSUSED */
static void
dumpCancelCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (dump_w);
}

/* called when the dump AOI button is activated.
 */
/* ARGSUSED */
static void
dumpAOICB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!state.fimage.image) {
	    msg ("No image.");
	    return;
	}

	if (!dump_w)
	    createDump();

	if (XtIsManaged(dump_w))
	    XtUnmanageChild (dump_w);
	else
	    XtManageChild (dump_w);
}

/* called when the dump Ok button is activated.
 * N.B. also used with TextField.
 */
/* ARGSUSED */
static void
dumpOKCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	FImage *fip = &state.fimage;
	CamPixel *ip = (CamPixel *)fip->image;
	AOI *aoip = &state.aoi;
	char *fn;
	FILE *fp;
	int x, y;

	fn = XmTextFieldGetString (dumpfn_w);
	fp = fopen (fn, "w");
	if (!fp) {
	    msg ("%s: %s", fn, strerror(errno));
	    XtFree (fn);
	    return;
	}

	ip += fip->sw*aoip->y + aoip->x;
	for (y = aoip->y; y < aoip->y + aoip->h; y++) {
	    for (x = aoip->x; x < aoip->x + aoip->w; x++)
		fprintf (fp, "%4d %4d %5d\n", x, y, *ip++);
	    ip += fip->sw - aoip->w;
	}

	fclose (fp);

	msg ("%s complete.", fn);
	XtFree (fn);
	XtUnmanageChild (dump_w);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: basic.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.3 $ $Name:  $"};
