/* code to create and manage the state.imageDA, ximagep and the daGC.
 */

#include <stdio.h>
#include <stdlib.h>

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include <Xm/DrawingA.h>

#include "P_.h"
#include "astro.h"
#include "xtools.h"
#include "fits.h"
#include "wcs.h"
#include "fieldstar.h"
#include "camera.h"

extern Widget toplevel_w;

static void mkDA (int w, int h);
static void mkXImage (Display *dsp, int w, int h);
static void drawXImage(void);
static void makeGC(void);
static void daExpCB (Widget w, XtPointer client, XtPointer call);
static void daActionCB (Widget w, XtPointer client, XEvent *ev,
    Boolean *continue_to_dispatch);
static void doReport (int x, int y);

/* using toplevel_w, find the depth the server wants images and the number of
 * bit per pixel in the arrays we store them in.
 */
void
initDepth()
{
	int depth;

	XtVaGetValues (toplevel_w, XmNdepth, (XtArgVal)&depth, NULL);
	if (depth < 8) {
	    printf ("Server must support images at least 8 bits deep\n");
	    exit (1);
	}

	state.depth = depth;
	state.bpp = depth > 16 ? 32 : depth > 8 ? 16 : 8;
}

/* reapply the lut to update state.ximagep and redraw the scene.
 * this is intended to just redraw the current image with a new lut. if the
 * image is being changed or is being cropped, use newXImage().
 * this is safe to call before we've read in an image.
 */
void
updateXImage()
{
	if (!state.fimage.image)
	    return;

	FtoXImage();
	drawXImage();
}

/* given state.fimage make a new ximagep for it and a new DrawingArea for it
 *   off state.imageSW.
 * then fill in ximagep by calling FtoXImage().
 * this is intended for when the image changes or is cropped to a new size.
 * pixels will get drawn due to the expose of the new DrawingArea.
 * this is harmless to call before we've read in an image.
 */
void
newXImage()
{
	Display *dsp = XtDisplay (toplevel_w);
	Arg args[20];
	Dimension spacing;
	int se;
	int sw, sh;
	int new;
	int n;
	int w, h;

	if (!state.fimage.image)
	    return;

	watch_cursor(1);

	/* compute the effective size of the display area */

	if (state.crop) {
	    w = state.aoi.w;
	    h = state.aoi.h;
	} else {
	    w = state.fimage.sw;
	    h = state.fimage.sh;
	}

	w = w*state.mag/MAGDENOM;	/* don't use *= */
	h = h*state.mag/MAGDENOM;	/* don't use *= */

	/* (re)create the DrawingArea and XImage, if none or new size */

	new = 0;
	if (!state.imageDA || !state.ximagep || w != state.ximagep->width
					     || h != state.ximagep->height) {
	    mkXImage (dsp, w, h);
	    mkDA (w, h);
	    new = 1;
	}

	/* size the scrolled window to where the scroll bars are barely not
	 * needed but never larger than current screen size.
	 */

	get_something (state.imageSW, XmNspacing, (char *)&spacing);
	se = atoi (getXRes (toplevel_w, "ScreenEdge", "100"));

	sw = DisplayWidth (dsp, DefaultScreen(dsp)) - spacing - se;
	sh = DisplayHeight (dsp, DefaultScreen(dsp)) - spacing - se;
	if (w < sw)
	    sw = w;
	if (h < sh)
	    sh = h;

	n = 0;
	XtSetArg (args[n], XmNwidth, sw+spacing); n++;
	XtSetArg (args[n], XmNheight, sh+spacing); n++;
	XtSetValues (state.imageSW, args, n);
	XtManageChild (state.imageSW);

	/* apply the current lut to fill in ximagep */

	FtoXImage();

	/* expose will do the XPutImage if new, else do refresh here */
	if (!new)
	    refreshScene(0, 0, w, h);

	/* done */
	watch_cursor(0);
}

/* redraw the given portion of the current scene */
void
refreshScene(x, y, w, h)
int x, y, w, h;
{
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);

	if (!state.daGC)
	    makeGC();

	XPutImage (dsp, win, state.daGC, state.ximagep, x, y, x, y, w, h);
	drawAOI (False, &state.aoi);
	if (state.showgsc)
	    markGSC();
	drawMarkers();
}

static void
mkXImage (Display *dsp, int w, int h)
{
	char *data;

	if (state.ximagep) {
	    XDestroyImage (state.ximagep);	/* also frees the data array */
	    state.ximagep = NULL;
	}

	/* create a new XImage and its ximagep */

	data = XtMalloc (w*h * state.bpp/8);

	state.ximagep = XCreateImage (dsp, XDefaultVisual (dsp, 0),
	/* depth */         state.depth,
	/* format */        ZPixmap,
	/* offset */        0,
	/* data */          data,
	/* width */         w,
	/* height */        h,
	/* pad */           state.bpp,
	/* bpl */           0);

	state.ximagep->bitmap_bit_order = LSBFirst;
	state.ximagep->byte_order = LSBFirst;
}

/* create a new DrawingArea for the state.imageSW to manage. */
static void
mkDA (int w, int h)
{
	Arg args[20];
	EventMask mask;
	int n;

	if (state.imageDA) {
	    XtDestroyWidget (state.imageDA);
	    state.imageDA = 0;
	}

	n = 0;
	XtSetArg (args[n], XmNwidth, w); n++;
	XtSetArg (args[n], XmNheight, h); n++;
	state.imageDA = XmCreateDrawingArea (state.imageSW, "DA", args, n);
	XtAddCallback (state.imageDA, XmNexposeCallback, daExpCB, NULL);
	mask = Button1MotionMask | Button2MotionMask | PointerMotionMask
		| ButtonPressMask | ButtonReleaseMask | PointerMotionHintMask;
	XtAddEventHandler (state.imageDA, mask, False, daActionCB, NULL);
	set_something (state.imageSW, XmNworkWindow, (char *)state.imageDA);
	XtManageChild (state.imageDA);
}

/* send the entire XImage to the DrawingArea
 * also draw the current aoi and GSC stars, if any.
 */
static void
drawXImage()
{
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);
	Dimension w, h;

	if (!state.daGC)
	    makeGC();

	get_something (state.imageDA, XmNwidth, (char *)&w);
	get_something (state.imageDA, XmNheight, (char *)&h);

	XPutImage (dsp, win, state.daGC, state.ximagep, 0, 0, 0, 0, w, h);

	drawAOI (False, &state.aoi);
	if (state.showgsc)
	    markGSC();
	drawMarkers();
}

static void
makeGC()
{
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);
	XGCValues gcv;
	unsigned int gcm;
	unsigned long fg, bg;

	gcm = GCForeground | GCBackground;
	get_something (state.imageDA, XmNforeground, (char *)&fg);
	get_something (state.imageDA, XmNbackground, (char *)&bg);
	gcv.foreground = bg;
	gcv.background = fg;
	state.daGC = XCreateGC (dsp, win, gcm, &gcv);
}

/* called when we get an Expose event from the DrawingArea
 */
/* ARGSUSED */
static void
daExpCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *cp = (XmDrawingAreaCallbackStruct *)call;
	XExposeEvent *ep = &cp->event->xexpose;
	    
	if (cp->reason != XmCR_EXPOSE) {
	    msg ("Unexpected imageDA event. type=%d\n", cp->reason);
	    return;
	}

	refreshScene(ep->x, ep->y, ep->width, ep->height);
}

/* called on any interesting event over the imageDA window.
 */
/* ARGSUSED */
static void
daActionCB (w, client, ev, continue_to_dispatch)
Widget w;
XtPointer client;
XEvent *ev;
Boolean *continue_to_dispatch;
{
	Display *dsp = XtDisplay (w);
	Window win = XtWindow(w);
	Window root, child;
	int rx, ry, wx, wy;	/* window coords */
	int ix, iy;		/* image coords */
	int inside;
	unsigned mask;
	Dimension wid, hei;
	int evt = ev->type;
	int mo, bp, br;
	int m1, b1p, b1r, m2, b2p, b2r, b3p;

	/* what happened? */
	mo  = evt == MotionNotify;
	bp  = evt == ButtonPress;
	br  = evt == ButtonRelease;
	m1  = mo && ev->xmotion.state  == Button1Mask;
	b1p = bp && ev->xbutton.button == Button1;
	b1r = br && ev->xbutton.button == Button1;
	m2  = mo && ev->xmotion.state  == Button2Mask;
	b2p = bp && ev->xbutton.button == Button2;
	b2r = br && ev->xbutton.button == Button2;
	b3p = bp && ev->xbutton.button == Button3;

	/* where are we? */
	XQueryPointer (dsp, win, &root, &child, &rx, &ry, &wx, &wy, &mask);

	/* window to image coords */
	ix = wx*MAGDENOM/state.mag;
	iy = wy*MAGDENOM/state.mag;
	if (state.crop) {
	    ix += state.aoi.x;
	    iy += state.aoi.y;
	}

	get_something (w, XmNwidth, (char *)&wid);
	get_something (w, XmNheight, (char *)&hei);
	inside = wx >= 0 && wx < (int)wid && wy >= 0 && wy < (int)hei;

	/* what do we do? 
	 * report location, if buttons down or option enabled.
	 * button 1 is for reporting pixel values and running the glass.
	 * button 2 is for setting an AOI.
	 * button 3 is for reporting photometry or resetting measure ref.
	 */

	if ((state.roam || bp || m1 || m2) && inside)
	    doReport (ix, iy);

	if (b1p || (m1 && inside) || b1r) {
	    FImage *fip = &state.fimage;
	    double ra, dec;
	    int radecok;

	    /* measure can do and undo separatly because it doesn't actually
	     * save pixels but glass cleverly only undoes and does real
	     * changed pixels at once with RUN. so, can't have measure line
	     * visible when updating glass. (Could use only START?END with
	     * glas but then it shows the unmag scene between each update).
	     */
 	    radecok = xy2rd (fip, ix, iy, &ra, &dec) == 0;
 	    if (b1p) {
		doGlass (dsp, win, START_PLOT, wx, wy, wid, hei);
		doMeasure (START_PLOT, ix, iy, radecok, ra, dec);
	    } else if (m1) {
		doMeasure (END_PLOT, ix, iy, radecok, ra, dec);
		doGlass (dsp, win, RUN_PLOT, wx, wy, wid, hei);
		doMeasure (START_PLOT, ix, iy, radecok, ra, dec);
	    } else {
		doMeasure (END_PLOT, ix, iy, radecok, ra, dec);
		doGlass (dsp, win, END_PLOT, wx, wy, wid, hei);
	    }
	}
	if (b2p || (m2 && inside) || b2r) {
	    PlotState ps = b2p ? START_PLOT : b2r ? END_PLOT : RUN_PLOT;
	    doAOI (ps, ix, iy);
	}
	if (b3p && inside) {
	    doPhotom (ix, iy);
	    doMeasureSetRef (ix, iy);
	}
}

/* report the location and value of the pixel at image coords x/y */
static void
doReport (x, y)
int x, y;		/* image coords of cursor */
{
	FImage *fip = &state.fimage;
	CamPixel *p = (CamPixel *) fip->image;
	int v = p[fip->sw*y + x];
	double ra, dec;

	if (!xy2rd (fip, (double)x, (double)y, &ra, &dec)) {
	    char rastr[64], decstr[64];
	    double e = pEyear();

	    fs_sexa (rastr, radhr(ra), 2, 36000);
	    fs_sexa (decstr, raddeg(dec), 3, 3600);
	    if (e == 2000)
		msg ("x=%4d y=%4d RA=%s Dec=%s  v=%5d", x, y, rastr, decstr, v);
	    else
		msg ("x=%4d y=%4d RA=%s Dec=%s (%g) v=%5d", x, y,
							rastr, decstr, e, v);
	} else {
	    msg ("x=%4d y=%4d v=%5d", x, y, v);
	}
}

/* called to set or unset the watch cursor.
 * allow for nested requests.
 */
void
watch_cursor(want)
int want;
{
	static Cursor wc;
	static int nreqs;
	Display *dsp;
	Window win;
	Cursor c;

	if (!state.imageSW || !(dsp = XtDisplay (state.imageSW)) ||
					    !(win = XtWindow(state.imageSW)))
	    return;

	if (!wc)
	    wc = XCreateFontCursor (dsp, XC_watch);

	if (want) {
	    if (nreqs++ > 0)
		return;
	    c = wc;
	} else {
	    if (--nreqs > 0)
		return;
	    c = (Cursor)0;
	}

	if (want)
	    XDefineCursor (dsp, win, c);
	else
	    XUndefineCursor (dsp, win);

	XFlush (dsp);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: ximage.c,v $ $Date: 2001/04/19 21:12:00 $ $Revision: 1.1.1.1 $ $Name:  $"};
