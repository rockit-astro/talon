/* dialog and XImage code to implement the magnifying glass */

#include <stdio.h>
#include <stdlib.h>
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
#include "xtools.h"
#include "fits.h"
#include "wcs.h"
#include "fieldstar.h"
#include "camera.h"

extern void gfit (int pix[], int n, double *maxp, double *cenp, double *fwhmp);

#define	BORDER	5	/* size of border in each section, pixels */
#define	NGRID	5	/* number of grid spacings (+1 total lines) */
#define	NCACHE	100	/* size of XPoints cache */
#define	CH	12	/* char height -- TODO */

static void da_exp_cb (Widget w, XtPointer client, XtPointer call);
static void showpl_cb (Widget w, XtPointer client, XtPointer call);
static void makeGCs (Display *dsp, Window win);
static void makeGlassXImage (Display *dsp);
static void magCB (Widget w, XtPointer client, XtPointer call);
static void sizeCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);
static void unmapCB (Widget w, XtPointer client, XtPointer call);
static void fillGlass (int wx, int wy, unsigned wid, unsigned hei);
static void glassStats (int ix, int iy, int w, int h);
static void displayStats (int mean, int median, double sd, int min, int max,
    int maxx, int maxy);
static void glassPlots (Display *dsp, int x, int y, int w, int h);
static void plot_gaussian (Drawable win, int ww, int wh, int rw, int rh,
    int minp, int maxp, int ix0, int iy0);

static int glassMag;	/* mag factor; 1 means no glass */
static int glassSize;	/* width and height of the glass area BEFORE mag */

static Widget glass_w;	/* the main glass form dialog */
static Widget stats_w[8];/* stats labels */
static Widget da_w;	/* plot drawing area */
static Widget showpl_w;	/* whether to show plots TB */
static Widget snap_w;	/* snap-to-max TB */
static Widget gauss_w;	/* Gaussian fit overlay TB */

static XImage *glassXI;	/* glass XImage -- NULL means make a new one */
static GC glassGC;	/* GC for use drawing the glass */
static GC plotGC;	/* GC to use for misc stuff in plots */
static Pixel grid_p;	/* color to use for plotting grid */
static Pixel gauss_p;	/* color to use for plotting the gaussian */

/* toggle whether the Glass dialog is up */
void
manageGlass ()
{
	if (!glass_w)
	    createGlass();

	if (XtIsManaged(glass_w))
	    raiseShell (glass_w);
	else
	    XtManageChild (glass_w);
}

/* create the Glass dialog */
void
createGlass()
{
	Widget rb_w, w;
	Widget rc_w, hrc_w;
	Widget sep_w;
	Widget close_w;
	Widget go, g2, g4, g8;
	Widget s16, s32, s64;
	Arg args[20];
	int n;
	int i;

	/* create form */
	
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	glass_w = XmCreateFormDialog (toplevel_w, "Glass", args,n);
	XtAddCallback (glass_w, XmNunmapCallback, unmapCB, NULL);
	
	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Glass"); n++;
	XtSetValues (XtParent(glass_w), args, n);
	XtVaSetValues (glass_w, XmNcolormap, camcm, NULL);

	/* create a vertical row/column to hold most things */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	rc_w = XmCreateRowColumn (glass_w, "RC", args, n);
	XtManageChild (rc_w);

	/* create the stat labels */

	n = 0;
	w = XmCreateLabel (rc_w, "L1", args, n);
	XtManageChild (w);
	set_xmstring (w, XmNlabelString, "Glass statistics");

	n = 0;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNnumColumns, 2); n++;
	hrc_w = XmCreateRowColumn (rc_w, "BoxRC", args, n);
	XtManageChild (hrc_w);

	    for (i = 0; i < XtNumber(stats_w); i++) {
		n = 0;
		XtSetArg (args[n], XmNrecomputeSize, True); n++;
		stats_w[i] = XmCreateLabel (hrc_w, "Stats", args, n);
		XtManageChild (stats_w[i]);
	    }

	/* set to initialize size */
	displayStats (0, 0, 0.0, 0, 0, 0, 0);

	/* add a separator */

	n = 0;
	sep_w = XmCreateSeparator (rc_w, "Sep1", args, n);
	XtManageChild (sep_w);

	/* create a radio box for the glass mag factor */

	n = 0;
	w = XmCreateLabel (rc_w, "L2", args, n);
	XtManageChild (w);
	set_xmstring (w, XmNlabelString, "Magnification factor");

	n = 0;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNspacing, 10); n++;
	rb_w = XmCreateRadioBox (rc_w, "Factor", args, n);
	XtManageChild (rb_w);

	    n = 0;
	    go = XmCreateToggleButton (rb_w, "1x", args, n);
	    XtManageChild (go);
	    XtAddCallback (go, XmNvalueChangedCallback, magCB, (XtPointer)1);
	    if (XmToggleButtonGetState (go))
		glassMag = 1;

	    n = 0;
	    g2 = XmCreateToggleButton (rb_w, "2x", args, n);
	    XtManageChild (g2);
	    XtAddCallback (g2, XmNvalueChangedCallback, magCB, (XtPointer)2);
	    if (XmToggleButtonGetState (g2))
		glassMag = 2;

	    n = 0;
	    g4 = XmCreateToggleButton (rb_w, "4x", args, n);
	    XtManageChild (g4);
	    XtAddCallback (g4, XmNvalueChangedCallback, magCB, (XtPointer)4);
	    if (XmToggleButtonGetState (g4))
		glassMag = 4;

	    n = 0;
	    g8 = XmCreateToggleButton (rb_w, "8x", args, n);
	    XtManageChild (g8);
	    XtAddCallback (g8, XmNvalueChangedCallback, magCB, (XtPointer)8);
	    if (XmToggleButtonGetState (g8))
		glassMag = 8;

	    if (glassMag == 0) {
		msg ("No Camera*Glass.Factor set -- defaulting to 4x");
		XmToggleButtonSetState (g4, True, True);
		glassMag = 4;
	    }

	/* add a separator */

	n = 0;
	sep_w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (sep_w);

	n = 0;
	w = XmCreateLabel (rc_w, "L3", args, n);
	XtManageChild (w);
	set_xmstring (w, XmNlabelString, "Glass size");

	/* create a radio box for the glass size */

	n = 0;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	rb_w = XmCreateRadioBox (rc_w, "Size", args, n);
	XtManageChild (rb_w);

	    n = 0;
	    s16 = XmCreateToggleButton (rb_w, "16x16", args, n);
	    XtManageChild (s16);
	    XtAddCallback (s16, XmNvalueChangedCallback, sizeCB, (XtPointer)16);
	    if (XmToggleButtonGetState (s16))
		glassSize = 16;

	    n = 0;
	    s32 = XmCreateToggleButton (rb_w, "32x32", args, n);
	    XtManageChild (s32);
	    XtAddCallback (s32, XmNvalueChangedCallback, sizeCB, (XtPointer)32);
	    if (XmToggleButtonGetState (s32))
		glassSize = 32;

	    n = 0;
	    s64 = XmCreateToggleButton (rb_w, "64x64", args, n);
	    XtManageChild (s64);
	    XtAddCallback (s64, XmNvalueChangedCallback, sizeCB, (XtPointer)64);
	    if (XmToggleButtonGetState (s64))
		glassSize = 64;

	    if (glassSize == 0) {
		msg ("No Camera*Glass.Size set -- defaulting to 64x64");
		XmToggleButtonSetState (s64, True, True);
		glassSize = 64;
	    }

	/* add a separator */

	n = 0;
	sep_w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (sep_w);

	/* snap option */

	n = 0;
	XtSetArg (args[n], XmNset, True); n++; /* KMI 10/19/03 */
	snap_w = XmCreateToggleButton (rc_w, "Snap", args, n);
	set_xmstring (snap_w, XmNlabelString, "Snap to max");
	XtManageChild (snap_w);

	/* whether to even show the plots */

	n = 0;
	XtSetArg (args[n], XmNset, True); n++; /* KMI 10/19/03 */
	showpl_w = XmCreateToggleButton (rc_w, "ShowPlots", args, n);
	set_xmstring (showpl_w, XmNlabelString, "Show 1D Plots");
	XtAddCallback (showpl_w, XmNvalueChangedCallback, showpl_cb, NULL);
	XtManageChild (showpl_w);

	/* gaussian fit option */

	n = 0;
	XtSetArg (args[n], XmNset, True); n++; /* KMI 10/19/03 */
	gauss_w = XmCreateToggleButton (rc_w, "Gauss", args, n);
	set_xmstring (gauss_w, XmNlabelString, "Overlay Gaussian Fit");
	if (XmToggleButtonGetState(showpl_w))
	    XtManageChild (gauss_w);

	/* add a separator */

	n = 0;
	sep_w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (sep_w);

	n = 0;

	/* make the drawing area for the plots */

	n = 0;
	da_w = XmCreateDrawingArea (rc_w, "PlotDA", args, n);
	XtAddCallback (da_w, XmNexposeCallback, da_exp_cb, NULL);
	if (XmToggleButtonGetState(showpl_w))
	    XtManageChild (da_w);

	/* make the close button at the bottom */

	n = 0;
	close_w = XmCreatePushButton (rc_w, "Close", args, n);
	XtManageChild (close_w);
	XtAddCallback (close_w, XmNactivateCallback, closeCB, NULL);
}

/* handle the operation of the magnifying glass.
 * this is called whenever there is left button activity over the image.
 * TODO: when glassMag is 1, just do the outline and the plots.
 */
void
doGlass (dsp, win, ps, wx, wy, wid, hei)
Display *dsp;
Window win;
PlotState ps;
int wx, wy;		/* window coords of cursor */
unsigned wid, hei;	/* window size */
{
	static int lastwx, lastwy;
	int rx, ry, rw, rh;	/* region */
	int ix, iy;	/* glass center x and y, image pixels */
	int size;	/* glass size, image pixels */

	/* convert window to image coords */
	ix = wx*MAGDENOM/state.mag;
	iy = wy*MAGDENOM/state.mag;
	size = glassSize*MAGDENOM/state.mag;
	if (state.crop) {
	    ix += state.aoi.x;
	    iy += state.aoi.y;
	}

	if (!glassXI)
	    makeGlassXImage (dsp);

	if (!glassGC)
	    makeGCs (dsp, win);

	/* if snapping, change to location IN PLACE at max pixel under entire
	 * glass
     *
     * Added XtIsManaged check - KMI 10/19/03 
	 */
	if (XmToggleButtonGetState (snap_w) && XtIsManaged(glass_w)) {
	    FImage *fip = &state.fimage;
	    CamPixel *image = (CamPixel *)(fip->image);
	    int maxx = ix;
	    int maxy = iy;
	    int maxp = 0;

	    for (rx = ix - size/2; rx < ix + size/2; rx++) {
		if (rx < 0 || rx >= fip->sw)
		    continue;
		for (ry = iy - size/2; ry < iy + size/2; ry++) {
		    CamPixel p;
		    if (ry < 0 || ry >= fip->sh)
			continue;
		    p = image[ry*fip->sw + rx];
		    if ((int)p > maxp) {
			maxx = rx;
			maxy = ry;
			maxp = (int)p;
		    }
		}
	    }

	    ix = maxx;
	    iy = maxy;
	    if (state.crop) {
		maxx -= state.aoi.x;
		maxy -= state.aoi.y;
	    }
	    wx = maxx*state.mag/MAGDENOM;
	    wy = maxy*state.mag/MAGDENOM;
	}

	if (ps == RUN_PLOT) {
	    /* motion with button 1 down.
	     * put back old pixels that won't just be covered again.
	     */

	    /* first the vertical strip that is uncovered */

	    rh = glassSize*glassMag;
	    ry = lastwy - (glassSize*glassMag/2);
	    if (ry < 0) {
		rh += ry;
		ry = 0;
	    }
	    if (wx < lastwx) {
		rw = lastwx - wx;	/* cursor moved left */
		rx = wx + (glassSize*glassMag/2);
	    } else {
		rw = wx - lastwx;	/* cursor moved right */
		rx = lastwx - (glassSize*glassMag/2);
	    }
	    if (rx < 0) {
		rw += rx;
		rx = 0;
	    }

	    if (rw > 0 && rh > 0)
		XPutImage (dsp, win, state.daGC, state.ximagep,
							rx, ry, rx, ry, rw, rh);

	    /* then the horizontal strip that is uncovered */

	    rw = glassSize*glassMag;
	    rx = lastwx - (glassSize*glassMag/2);
	    if (rx < 0) {
		rw += rx;
		rx = 0;
	    }
	    if (wy < lastwy) {
		rh = lastwy - wy;	/* cursor moved up */
		ry = wy + (glassSize*glassMag/2);
	    } else {
		rh = wy - lastwy;	/* cursor moved down */
		ry = lastwy - (glassSize*glassMag/2);
	    }
	    if (ry < 0) {
		rh += ry;
		ry = 0;
	    }

	    if (rw > 0 && rh > 0)
		XPutImage (dsp, win, state.daGC, state.ximagep,
							rx, ry, rx, ry, rw, rh);
	}

	if (ps == START_PLOT || ps == RUN_PLOT) {
	    /* button 1 just pressed or new motion while inside window.
	     * show glass and save location we drew to.
	     */

	    fillGlass (wx, wy, wid, hei);
	    XPutImage (dsp, win, state.daGC, glassXI, 0, 0,
			wx-(glassSize*glassMag/2), wy-(glassSize*glassMag/2),
			glassSize*glassMag, glassSize*glassMag);
	    lastwx = wx;
	    lastwy = wy;

	    /* kinda hard to tell boundry of glass so draw a line around it */
	    XDrawRectangle (dsp, win, glassGC,
			wx-(glassSize*glassMag/2), wy-(glassSize*glassMag/2),
			glassSize*glassMag-1, glassSize*glassMag-1);

	    /* if glass dialog is up, set the stats and plot too */
	    if (XtIsManaged(glass_w)) {
		glassStats (ix, iy, size, size);
		if (XmToggleButtonGetState(showpl_w))
		    glassPlots (dsp, ix, iy, size, size);
	    }
	}

	if (ps == END_PLOT) {
	    /* button 1 released.
	     * restore all old pixels.
	     */

	    rx = lastwx - (glassSize*glassMag/2);
	    rw = glassSize*glassMag;
	    if (rx < 0) {
		rw += rx;
		rx = 0;
	    }

	    ry = lastwy - (glassSize*glassMag/2);
	    rh = glassSize*glassMag;
	    if (ry < 0) {
		rh += ry;
		ry = 0;
	    }

	    if (rw > 0 && rh > 0)
		XPutImage (dsp, win, state.daGC, state.ximagep,
						    rx, ry, rx, ry, rw, rh);

	    /* redraw the AOI since it might have gotten roamed over */
	    drawAOI (False, &state.aoi);

	    /* same with the GSC stars */
	    if (state.showgsc)
		markGSC();

	    /* and markers */
	    drawMarkers();
	}
}

/* callback from the plot drawing area.
 * just erase it, since we don't store the last plot.
 * also, maintain a 2x1 aspect ratio.
 */
static void
da_exp_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	Display *dsp = XtDisplay(w);
	Window win = XtWindow(w);
	Dimension wid;

	switch (c->reason) {
	case XmCR_EXPOSE: {
	    static int before;
	    XExposeEvent *e = &c->event->xexpose;
	    if (!before) {
		XSetWindowAttributes swa;
		unsigned long mask = CWBitGravity;
		swa.bit_gravity = ForgetGravity;
		XChangeWindowAttributes (dsp, win, mask, &swa);
		before = 1;
	    }
	    /* wait for the last in the series */
	    if (e->count != 0)
		return;
	    break;
	    }
	default:
	    printf ("Unexpected da_w event. type=%d\n", c->reason);
	    return;
	}

	XClearWindow (dsp, win);

	get_something (w, XmNwidth, (char *)&wid);
	set_something (w, XmNheight, (char *)(2*wid));
}

/* called when the Show plot TB changes state */
/* ARGSUSED */
static void
showpl_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (XmToggleButtonGetState(showpl_w)) {
	    XtManageChild (da_w);
	    XtManageChild (gauss_w);
	} else {
	    XtUnmanageChild (da_w);
	    XtUnmanageChild (gauss_w);
	}
}

/* make the GC to draw a border around the glass and get other colors.
 */
static void
makeGCs (dsp, win)
Display *dsp;
Window win;
{
	XGCValues gcv;
	unsigned int gcm;
	Pixel p;

	if (get_color_resource (dsp, myclass, "GlassBorderColor", &p) < 0) {
	    msg ("Can not get GlassBorderColor -- using White");
	    p = WhitePixel (dsp, DefaultScreen(dsp));
	}
	gcm = GCForeground;
	gcv.foreground = p;
	glassGC = XCreateGC (dsp, win, gcm, &gcv);

	if (get_color_resource (dsp, myclass, "GlassGridColor", &grid_p) < 0) {
	    msg ("Can not get GlassGridColor -- using White");
	    grid_p = WhitePixel (dsp, DefaultScreen(dsp));
	}
	if (get_color_resource (dsp, myclass, "GlassGaussColor", &gauss_p) < 0){
	    msg ("Can not get GlassGaussColor -- using White");
	    gauss_p = WhitePixel (dsp, DefaultScreen(dsp));
	}

	plotGC = XCreateGC (dsp, win, 0L, &gcv);
}

/* called when one of the magnification toggle buttons changes state.
 * client is the new mag factor: 1, 2, or 4.
 */
/* ARGSUSED */
static void
magCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (XmToggleButtonGetState(w))
	    glassMag = (int) client;

	if (glassXI) {
	    XDestroyImage (glassXI);
	    glassXI = NULL;
	}
}

/* called when one of the size toggle buttons changes state.
 * client is the new size: 32 or 64
 */
/* ARGSUSED */
static void
sizeCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (XmToggleButtonGetState(w))
	    glassSize = (int) client;

	if (glassXI) {
	    XDestroyImage (glassXI);
	    glassXI = NULL;
	}
}

/* called when the main glass dialog in unmapped */
/* ARGSUSED */
static void
unmapCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* turn these off because they are useless now but still interfere */

    /* Commented out - KMI 10/19/03 */

	/*XmToggleButtonSetState (snap_w, False, False);
	XmToggleButtonSetState (gauss_w, False, False); */
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
	XtUnmanageChild (glass_w);
}

/* make glassXI of size glassSize*glassMag.
 */
static void
makeGlassXImage (dsp)
Display *dsp;
{
	int e = glassSize*glassMag;
	char *glassd;

	if (glassXI)
	    XDestroyImage (glassXI);	/* frees data too */

	glassd = XtMalloc (e * e * state.bpp/8);

	glassXI = XCreateImage (dsp, XDefaultVisual (dsp, 0),
	    /* depth */         state.depth,
	    /* format */        ZPixmap,
	    /* offset */        0,
	    /* data */          glassd,
	    /* width */         e,
	    /* height */        e,
	    /* pad */           state.bpp,
	    /* bpl */           0);

	glassXI->bitmap_bit_order = LSBFirst;
	glassXI->byte_order = LSBFirst;
}

/* fill glassXI with magnified view of ximagep */
static void
fillGlass (wx, wy, wid, hei)
int wx, wy;	/* window coords of glass center */
unsigned wid;	/* window width */
unsigned hei;	/* window height */
{
	int hs = glassSize/2;
	int x, y;	/* input coords (on ximagep) */
	int xg, yg;	/* output coords (on glassXI) */

	xg = yg = 0;
	for (y = wy - hs; y < wy + hs; y++) {
	    for (x = wx - hs; x < wx + hs; x++) {
		Pixel p;
		int i, j;

		if (y < 0 || y >= hei || x < 0 || x >= wid)
		    p = 0;
		else
		    p = XGetPixel (state.ximagep, x, y);

		for (i = 0; i < glassMag; i++)
		    for (j = 0; j < glassMag; j++)
			XPutPixel (glassXI, xg+i, yg+j, p);
		xg += glassMag;
	    }

	    xg = 0;
	    yg += glassMag;
	}
}

/* compute and display the stats of a wXh region of pixels centered at x/y */
static void
glassStats (x, y, w, h)
int x, y;		/* image coords of cursor center */
int w, h;		/* size of region, in image pixels */
{
	FImage *fip = &state.fimage;
	AOIStats a;
	int x0, x1;
	int y0, y1;
	int nx, ny;

	x0 = x - w/2; if (x0 < 0) x0 = 0;
	x1 = x + w/2; if (x1 > fip->sw) x1 = fip->sw;
	y0 = y - h/2; if (y0 < 0) y0 = 0;
	y1 = y + h/2; if (y1 > fip->sh) y1 = fip->sh;
	nx = x1 - x0;
	ny = y1 - y0;

	aoiStatsFITS (fip->image, fip->sw, x0, y0, nx, ny, &a);
	displayStats (a.mean, a.median, a.sd, a.min, a.max, a.maxx,a.maxy);
}

static void
displayStats (int mean, int median, double sdev, int min, int max, int maxx,
int maxy)
{
	FImage *fip = &state.fimage;
	char buf[1024];
	StarDfn sd;
	StarStats ss;
	double sdm;

	wlprintf (stats_w[0],	"   Min:%7d", min);
	wlprintf (stats_w[1],	"  Mean:%7d", mean);
	wlprintf (stats_w[2],	"Median:%7d", median);
	wlprintf (stats_w[3],	" Std D:%7.1f", sdev);

	wlprintf (stats_w[4],	"  Max: %7d", max);
	wlprintf (stats_w[5],	"   at X:%6d", maxx);
	wlprintf (stats_w[6],	"   at Y:%6d", maxy);

	sd.rsrch = 10;
	sd.rAp = 0;
	sd.how = SSHOW_HERE;
	if (max > 0 && starStats ((CamPixel *)fip->image, fip->sw, fip->sh,
						&sd, maxx, maxy, &ss, buf) == 0)
	    sdm = (ss.p - ss.Sky)/ss.rmsSky;
	else
	    sdm = 0;
	wlprintf (stats_w[7],	"   SD>M:%6.1f", sdm);
}

/* compute and display the cross-section plots in da_w.
 */
static void
glassPlots (dsp, x, y, w, h)
Display *dsp;
int x, y;		/* image coords of cursor center */
int w, h;		/* size of region, in image pixels */
{
	Window win = XtWindow(da_w);
	FImage *fip = &state.fimage;
	CamPixel p, *image;
	Dimension dawdim, dahdim;
	XPoint xpts[NCACHE];
	int nxpts;
	int daw, dah;
	int minp, maxp;
	int wx, wy;
	int i;

	/* get display window size */
	get_something (da_w, XmNwidth, (char *)&dawdim);
	get_something (da_w, XmNheight, (char *)&dahdim);
	daw = (int)dawdim;
	dah = (int)dahdim;

	/* find max and min in each direction for scaling */
	maxp = 0;
	minp = MAXCAMPIX;
	image = (CamPixel *)(fip->image) + fip->sw*y; /* y'th row */
	for (wx = x - w/2; wx < x + w/2; wx++) {
	    if (wx < 0 || wx >= fip->sw)
		continue;
	    p = image[wx];
	    if ((int)p > maxp)
		maxp = (int)p;
	    if ((int)p < minp)
		minp = (int)p;
	}
	image = (CamPixel *)(fip->image) + x; /* x'th col */
	for (wy = y - h/2; wy < y + h/2; wy++) {
	    if (wy < 0 || wy >= fip->sh)
		continue;
	    p = image[wy*fip->sw];
	    if ((int)p > maxp)
		maxp = (int)p;
	    if ((int)p < minp)
		minp = (int)p;
	}

	/* expand so it shrinks a bit inside the plot -- also 
	 * allows for gaussian to shoot up ok.
	 */
	minp *= 0.95;
	maxp *= 1.05;

	XClearWindow (dsp, win);

	/* draw the background grid */
	XSetForeground (dsp, plotGC, grid_p);
	for (i = 0; i <= NGRID; i++) {
	    int t;

	    /* the horizontal rules */
	    t = BORDER + i*(dah/2-2*BORDER)/(NGRID);
	    XDrawLine (dsp, win, plotGC, BORDER, t, daw-BORDER, t);
	    t = dah/2 + BORDER + i*(dah/2-2*BORDER)/(NGRID);
	    XDrawLine (dsp, win, plotGC, BORDER, t, daw-BORDER, t);

	    /* the vertical rules */
	    t = BORDER + i*(daw-2*BORDER)/(NGRID);
	    XDrawLine (dsp, win, plotGC, t, BORDER, t, dah/2-BORDER);
	    XDrawLine (dsp, win, plotGC, t, dah-BORDER, t, dah/2+BORDER);
	}

	/* do the horizontal cross section in the bottom half */
	image = (CamPixel *)(fip->image) + fip->sw*y; /* y'th row */
	nxpts = 0;
	for (wx = BORDER; wx < daw-BORDER; wx++) {
	    int ix = x - w/2 + (wx-BORDER)*w/(daw-2*BORDER);
	    CamPixel p = ix < 0 || ix >= fip->sw ? 0 : image[ix];
	    XPoint *xp = &xpts[nxpts++];

	    wy = dah-BORDER - ((int)p-minp)*(dah/2-2*BORDER)/(maxp-minp);
	    xp->x = wx;
	    xp->y = wy;
	    if (nxpts >= NCACHE) {
		XDrawLines (dsp, win, glassGC, xpts, nxpts, CoordModeOrigin);
		xpts[0] = xpts[nxpts-1]; /* wrap to next set */
		nxpts = 1;
	    }
	}
	if (nxpts > 0)
	    XDrawLines (dsp, win, glassGC, xpts, nxpts, CoordModeOrigin);

	/* do the vertical cross section in the top half */
	image = (CamPixel *)(fip->image) + x; /* x'th column */
	nxpts = 0;
	for (wy = BORDER; wy < dah/2-BORDER; wy++) {
	    int iy = y - h/2 + (wy-BORDER)*h/(dah/2-2*BORDER);
	    CamPixel p = iy < 0 || iy >= fip->sh ? 0 : image[iy*fip->sw];
	    XPoint *xp = &xpts[nxpts++];

	    wx = BORDER + ((int)p-minp)*(daw-2*BORDER)/(maxp-minp);
	    xp->x = wx;
	    xp->y = wy;
	    if (nxpts >= NCACHE) {
		XDrawLines (dsp, win, glassGC, xpts, nxpts, CoordModeOrigin);
		xpts[0] = xpts[nxpts-1]; /* wrap to next set */
		nxpts = 1;
	    }
	}
	if (nxpts > 0)
	    XDrawLines (dsp, win, glassGC, xpts, nxpts, CoordModeOrigin);

	if (XmToggleButtonGetState(gauss_w))
	    plot_gaussian (win, daw, dah, w, h, minp, maxp, x, y);
}

/* plot the gaussion fit to the given row and col */
static void
plot_gaussian (win, ww, wh, rw, rh, minp, maxp, ix0, iy0)
Drawable win;	/* where too draw */
int ww, wh;	/* window width and height */
int rw, rh;	/* region width and height */
int minp, maxp;	/* drawing range, pixels */
int ix0, iy0;	/* image pixel used as center */
{
	Display *dsp = XtDisplay(da_w);
	FImage *fip = &state.fimage;
	XPoint xpts[NCACHE];
	double hmax, hcen, hfwhm, hsig;
	double vmax, vcen, vfwhm, vsig;
	double ra, dec;
	char str[1024];
	double pixsz;
	int inpixels;
	int nowcs;
	StarStats ss;
	StarDfn sd;
	int nxpts;
	int wx, wy;

	/* do the fitting
	 */
	sd.rAp = rw/2;		/* will set ss.rAp */
	sd.rsrch = 0;		/* doesn't matter with SSHOW_HERE */
	sd.how = SSHOW_HERE;
	if (starStats ((CamPixel *)fip->image, fip->sw, fip->sh, &sd,
						    ix0, iy0, &ss, str) < 0)
	    return; /* ok well */

	hmax = ss.xmax - ss.Sky;
	hcen = ss.x - ix0 + ss.rAp;
	hfwhm = ss.xfwhm;
	hsig = hfwhm/2.354;
	vmax = ss.ymax - ss.Sky;
	vcen = ss.y - iy0 + ss.rAp;
	vfwhm = ss.yfwhm;
	vsig = vfwhm/2.354;

	/* get the pixel size */
	if (getRealFITS (fip, "CDELT1", &pixsz) < 0) {
	    msg ("No CDELT1 -- reporting FWHM in Pixels");
	    pixsz = 1;
	    inpixels = 1;
	} else {
	    pixsz = fabs(pixsz)*3600.0; /* want in arcseconds */
	    inpixels = 0;
	}

	/* do the horizontal cross section in the bottom half */
	XSetForeground (dsp, plotGC, gauss_p);
	nxpts = 0;
	for (wx = 0; wx < ww-2*BORDER; wx++) {
	    double gx = (double)wx*rw/(ww-2*BORDER)-0.5; /* center the pixel*/
	    double gy = hmax*exp(-(gx-hcen)*(gx-hcen)/(2*hsig*hsig)) + ss.Sky;
	    XPoint *xp = &xpts[nxpts++];

	    wy = wh-BORDER - (gy-minp)*(wh/2-2*BORDER)/(maxp-minp);
	    xp->x = wx+BORDER;
	    xp->y = wy;
	    if (nxpts >= NCACHE) {
		XDrawLines (dsp, win, plotGC, xpts, nxpts, CoordModeOrigin);
		xpts[0] = xpts[nxpts-1]; /* wrap to next set */
		nxpts = 1;
	    }
	}
	if (nxpts > 0)
	    XDrawLines (dsp, win, plotGC, xpts, nxpts, CoordModeOrigin);

	/* draw the stats */
	wy = wh/2+BORDER+CH;
	wx = hcen < 9*rw/20 ? ww/2 : BORDER+2;
	if (inpixels)
	    (void) sprintf (str, "FWHM %4.2f pix", hfwhm);
	else
	    (void) sprintf (str, "FWHM %4.2f\"", hfwhm*pixsz);
	XDrawString(dsp, win, plotGC, wx, wy, str, strlen(str));
	(void) sprintf (str, "Peak %d+%.0f", ss.Sky, ss.xmax-ss.Sky);
	XDrawString(dsp, win, plotGC, wx, wy+CH, str, strlen(str));

	/* do the vertical cross section in the top half */
	nxpts = 0;
	for (wy = BORDER; wy < wh/2-BORDER; wy++) {
	    double gy = (double)(wy-BORDER)*rh/(wh/2-2*BORDER)-0.5;
	    double gx = vmax*exp(-(gy-vcen)*(gy-vcen)/(2*vsig*vsig)) + ss.Sky;
	    XPoint *xp = &xpts[nxpts++];

	    wx = BORDER + (gx-minp)*(ww-2*BORDER)/(maxp-minp);
	    xp->x = wx;
	    xp->y = wy;
	    if (nxpts >= NCACHE) {
		XDrawLines (dsp, win, plotGC, xpts, nxpts, CoordModeOrigin);
		xpts[0] = xpts[nxpts-1]; /* wrap to next set */
		nxpts = 1;
	    }
	}
	if (nxpts > 0)
	    XDrawLines (dsp, win, plotGC, xpts, nxpts, CoordModeOrigin);

	/* find centroid center in RA and Dec -- recall fit uses 0 as the
	 * left edge of the pixel but we want center.
	 */
	nowcs = xy2rd (fip, ss.x, ss.y, &ra, &dec) < 0;

	/* draw the stats */
	wy = vcen < rh/2 ? wh/2-4*(CH+3) : BORDER+CH;
	wx = ww/2;
	if (!nowcs) {
	    (void) strcpy (str, "RA ");
	    fs_sexa (str+3, radhr(ra), 3, 360000);
	    XDrawString (dsp, win, plotGC, wx, wy, str, strlen(str));

	    (void) strcpy (str, "Dec");
	    fs_sexa (str+3, raddeg(dec), 3, 36000);
	    XDrawString(dsp, win, plotGC, wx, wy+CH, str, strlen(str));
	}
	if (inpixels)
	    (void) sprintf (str, "FWHM %4.2f pix", vfwhm);
	else
	    (void) sprintf (str, "FWHM %4.2f\"", vfwhm*pixsz);
	XDrawString(dsp, win, plotGC, wx, wy+2*CH, str, strlen(str));
	(void) sprintf (str, "Peak %d+%.0f", ss.Sky, ss.ymax-ss.Sky);
	XDrawString(dsp, win, plotGC, wx, wy+3*CH, str, strlen(str));
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: glass.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $"};
