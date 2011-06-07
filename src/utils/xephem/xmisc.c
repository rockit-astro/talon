/* misc handy X Windows functions.
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#if defined(__STDC__)
#include <stdlib.h>
#else
extern void *malloc();
#endif
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/PushB.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"

extern Widget toplevel_w;
extern char myclass[];

extern int explodeGIF P_((unsigned char *rawpix, int nrawpix, int *wp,
    int *hp, unsigned char *pixap[], unsigned char ra[], unsigned char ga[],
    unsigned char ba[], char errmsg[]));
extern void xe_msg P_((char *msg, int app_modal));
extern void toHSV (double r, double g, double b, double *hp, double *sp,
    double *vp);
extern void toRGB (double h, double s, double v, double *rp, double *gp,
    double *bp);


#define	MAXGRAY	30		/* max colors in a grayscale ramp */

XFontStruct *getXResFont (char *rn);
char *getXRes P_((char *name, char *def));
int alloc_ramp P_((Display *dsp, XColor *basep, Colormap cm, Pixel pix[],
    int maxn));

/* font and GCs we manage */
static XFontStruct *viewsfsp;
static XFontStruct *trackingfsp;
/* names of resource colors for the planets.
 * N.B. should be in the same order as the defines in astro.h
 */
static char *objcolnames[NOBJ-3] = {
    "mercuryColor", "venusColor", "marsColor", "jupiterColor",
    "saturnColor", "uranusColor", "neptuneColor", "plutoColor",
    "sunColor", "moonColor"
};
/* name of resource for all other solar system objects */
static char solsyscolname[] = "solSysColor";
/* names of resource colors for stellar types.
 */
static char *starcolnames[] = {
    "hotStarColor", "mediumStarColor", "coolStarColor",
    "otherStellarColor"
};
/* name of resource for all other objects */
static char otherobjname[] = "otherObjColor";

static GC objgcs[XtNumber(objcolnames)];
static GC stargcs[XtNumber(starcolnames)];
static GC solsys_gc;
static GC other_gc;

/* info used to make XmButton look like XmButton or XmLabel */
static Arg look_like_button[] = {
    {XmNtopShadowColor, (XtArgVal) 0},
    {XmNbottomShadowColor, (XtArgVal) 0},
    {XmNtopShadowPixmap, (XtArgVal) 0},
    {XmNbottomShadowPixmap, (XtArgVal) 0},
    {XmNfillOnArm, (XtArgVal) True},
    {XmNtraversalOn, (XtArgVal) True},
};
static Arg look_like_label[] = {
    {XmNtopShadowColor, (XtArgVal) 0},
    {XmNbottomShadowColor, (XtArgVal) 0},
    {XmNtopShadowPixmap, (XtArgVal) 0},
    {XmNbottomShadowPixmap, (XtArgVal) 0},
    {XmNfillOnArm, (XtArgVal) False},
    {XmNtraversalOn, (XtArgVal) False},
};
static int look_like_inited;

/* handy way to set one resource for a widget.
 * shouldn't use this if you have several things to set for the same widget.
 */
void
set_something (w, resource, value)
Widget w;
char *resource;
XtArgVal value;
{
	Arg a[1];

	if (!w) {
	    printf ("set_something (%s) called with w==0\n", resource);
	    exit(1);
	}

	XtSetArg (a[0], resource, value);
	XtSetValues (w, a, 1);
}

/* handy way to get one resource for a widget.
 * shouldn't use this if you have several things to get for the same widget.
 */
void
get_something (w, resource, value)
Widget w;
char *resource;
XtArgVal value;
{
	Arg a[1];

	if (!w) {
	    printf ("get_something (%s) called with w==0\n", resource);
	    exit(1);
	}

	XtSetArg (a[0], resource, value);
	XtGetValues (w, a, 1);
}

/* return the given XmString resource from the given widget as a char *.
 * N.B. based on a sample in Heller, pg 178, the string back from
 *   XmStringGetLtoR should be XtFree'd. Therefore, OUR caller should always
 *   XtFree (*txtp).
 */
void
get_xmstring (w, resource, txtp)
Widget w;
char *resource;
char **txtp;
{
	static char me[] = "get_xmstring()";
	static char hah[] = "??";

	if (!w) {
	    printf ("%s: called for %s with w==0\n", me, resource);
	    exit(1);
	} else {
	    XmString str;
	    get_something(w, resource, (XtArgVal)&str); 
	    if (!XmStringGetLtoR (str, XmSTRING_DEFAULT_CHARSET, txtp)) {
		/*
		fprintf (stderr, "%s: can't get string resource %s\n", me,
								resource);
		exit (1);
		*/
		(void) strcpy (*txtp = XtMalloc(sizeof(hah)), hah);
	    }
	    XmStringFree (str);
	}
}

void
set_xmstring (w, resource, txt)
Widget w;
char *resource;
char *txt;
{
	XmString str;

	if (!w) {
	    printf ("set_xmstring called for %s with w==0\n", resource);
	    return;
	}

	str = XmStringCreateLtoR (txt, XmSTRING_DEFAULT_CHARSET);
	set_something (w, resource, (XtArgVal)str);
	XmStringFree (str);
}

/* return 1 if w is on screen else 0 */
int
isUp (w)
Widget w;
{
	XWindowAttributes wa;
	Display *dsp;
	Window win;

	if (!w)
	    return (0);
	dsp = XtDisplay(w);
	win = XtWindow(w);
	return (win && XGetWindowAttributes(dsp, win, &wa) &&
						wa.map_state == IsViewable);
}

/* may be connected as the mapCallback to any convenience Dialog to
 * position it centered the cursor (allowing for the screen edges).
 */
/* ARGSUSED */
void
prompt_map_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Window root, child;
	int rx, ry, wx, wy;	/* rx/y: cursor loc on root window */
	unsigned sw, sh;	/* screen width/height */
	Dimension ww, wh;	/* this widget's width/height */
	Position x, y;		/* final location */
	unsigned mask;
	Arg args[20];
	int n;

	XQueryPointer (XtDisplay(w), XtWindow(w),
				&root, &child, &rx, &ry, &wx, &wy, &mask);
	sw = WidthOfScreen (XtScreen(w));
	sh = HeightOfScreen(XtScreen(w));
	n = 0;
	XtSetArg (args[n], XmNwidth, &ww); n++;
	XtSetArg (args[n], XmNheight, &wh); n++;
	XtGetValues (w, args, n);

	x = rx - ww/2;
	if (x < 0)
	    x = 0;
	else if (x + ww >= (int)sw)
	    x = sw - ww;
	y = ry - wh/2;
	if (y < 0)
	    y = 0;
	else if (y + wh >= (int)sh)
	    y = sh - wh;

	n = 0;
	XtSetArg (args[n], XmNx, x); n++;
	XtSetArg (args[n], XmNy, y); n++;
	XtSetValues (w, args, n);
}

/* get the named color for w's colormap in *p, else set to White.
 * return 0 if the color was found, -1 if White had to be used.
 */
int
get_color_resource (w, cname, p)
Widget w;
char *cname;
Pixel *p;
{
	Display *dsp = XtDisplay(w);
	Colormap cm;
	XColor defxc, dbxc;
	Arg arg;
	char *cval;

	XtSetArg (arg, XmNcolormap, &cm);
	XtGetValues (w, &arg, 1);
	cval = getXRes (cname, NULL);

	if (!cval || !XAllocNamedColor (dsp, cm, cval, &defxc, &dbxc)) {
	    char msg[128];
	    if (!cval)
		(void) sprintf (msg, "Can not find resource `%.80s'", cname);
	    else
		(void) sprintf (msg, "Can not XAlloc color `%.80s'", cval);
	    (void) sprintf (msg+strlen(msg), " ... using White");
	    xe_msg(msg, 0);
	    *p = WhitePixel (dsp, DefaultScreen(dsp));
	    return (-1);
	} else {
	    *p = defxc.pixel;
	    return (0);
	}
}

/* get the XFontStruct we want to use when drawing text for the display views.
 */
void
get_views_font (dsp, fspp)
Display *dsp;
XFontStruct **fspp;
{
	if (!viewsfsp)
	    viewsfsp = getXResFont ("viewsFont");

	*fspp = viewsfsp;
}

/* set the XFontStruct we want to use when drawing text for the display views.
 * this also means setting all the GCs for objects.
 */
void
set_views_font (dsp, fsp)
Display *dsp;
XFontStruct *fsp;
{
	Font fid;
	int i;

	/* TODO: free old? */
	viewsfsp = fsp;

	/* may not have created gcs yet */
	if (!other_gc)
	    return;

	/* update all Fonts in GCs */
	fid = fsp->fid;
	for (i = 0; i < XtNumber(objcolnames); i++)
	    XSetFont (dsp, objgcs[i], fid);
	for (i = 0; i < XtNumber(starcolnames); i++)
	    XSetFont (dsp, stargcs[i], fid);
	XSetFont (dsp, solsys_gc, fid);
	XSetFont (dsp, other_gc, fid);
}

/* get the XFontStruct we want to use when drawing text while tracking cursor.
 */
void
get_tracking_font (dsp, fspp)
Display *dsp;
XFontStruct **fspp;
{
	if (!trackingfsp)
	    trackingfsp = getXResFont ("cursorTrackingFont");

	*fspp = trackingfsp;
}

/* set the XFontStruct we want to use when drawing text while tracking cursor.
 */
void
set_tracking_font (dsp, fsp)
Display *dsp;
XFontStruct *fsp;
{
	/* TODO: free old? */
	trackingfsp = fsp;
}

/* make all the various GCs used for objects from obj_pickgc().
 * TODO: reclaim old stuff if called again, but beware of hoarding users.
 */
void
make_objgcs()
{
	Display *dsp = XtDisplay(toplevel_w);
	Window win = RootWindow (dsp, DefaultScreen (dsp));
	unsigned long gcm;
	XFontStruct *fsp;
	XGCValues gcv;
	Pixel p;
	int i;

	gcm = GCFont | GCForeground;
	get_views_font (dsp, &fsp);
	gcv.font = fsp->fid;

	/* make the planet gcs */
	for (i = 0; i < XtNumber(objgcs); i++) {
	    (void) get_color_resource (toplevel_w, objcolnames[i], &p);
	    gcv.foreground = p;
	    objgcs[i] = XCreateGC (dsp, win, gcm, &gcv);
	}

	/* make the gc for other solar system objects */
	(void) get_color_resource (toplevel_w, solsyscolname, &p);
	gcv.foreground = p;
	solsys_gc = XCreateGC (dsp, win, gcm, &gcv);

	/* make the star color gcs */
	for (i = 0; i < XtNumber(stargcs); i++) {
	    (void) get_color_resource (toplevel_w, starcolnames[i], &p);
	    gcv.foreground = p;
	    stargcs[i] = XCreateGC (dsp, win, gcm, &gcv);
	}

	/* make the gc for everything else */
	(void) get_color_resource (toplevel_w, otherobjname, &p);
	gcv.foreground = p;
	other_gc = XCreateGC (dsp, win, gcm, &gcv);
}

/* given an object, return a GC for it.
 * Use the colors defined for objects in the X resources, else White.
 */
void
obj_pickgc(op, w, gcp)
Obj *op;
Widget w;
GC *gcp;
{
	/* insure GCs are ready */
	if (!other_gc)
	    make_objgcs();

	if (op->o_type == PLANET && op->pl.pl_code < XtNumber(objgcs))
	    *gcp = objgcs[op->pl.pl_code];
	else if (is_ssobj(op))
	    *gcp = solsys_gc;
	else if (is_type (op, FIXEDM))
	    switch (op->f_class) {
	    case 'Q': case 'J': case 'L': case 'T': case '\0':
		*gcp = stargcs[3];
		break;
	    case 'S': case 'B': case 'D': case 'M': case 'V':
		/* Sept 2000 S&T for article about latest spectral classes */
		switch (op->f_spect[0]) {
		case 'O': case 'B': case 'A': case 'W':
		    *gcp = stargcs[0];
		    break;
		case 'F': case 'G': case 'K':	/* FALLTHRU */
		default:
		    *gcp = stargcs[1];
		    break;
		case 'M': case 'L': case 'N': case 'R': case 'S': case 'C':
		    *gcp = stargcs[2];
		    break;
		}
		break;
	    default:
		*gcp = other_gc;
		break;
	    }
	else
	    *gcp = other_gc;
}

/* given any widget built from an XmLabel return pointer to the first
 * XFontStruct in its XmFontList.
 */
void
get_xmlabel_font (w, f)
Widget w;
XFontStruct **f;
{
	static char me[] = "get_xmlable_font";
	XmFontList fl;
	XmFontContext fc;
	XmStringCharSet charset;

	get_something (w, XmNfontList, (XtArgVal)&fl);
	if (XmFontListInitFontContext (&fc, fl) != True) {
	    printf ("%s: No Font context!\n", me);
	    exit(1);
	}
	if (XmFontListGetNextFont (fc, &charset, f) != True) {
	    printf ("%s: no font!\n", me);
	    exit(1);
	}
	XmFontListFreeFontContext (fc);
}

/* get the font named by the given X resource, else fixed, else bust */
XFontStruct *
getXResFont (rn)
char *rn;
{
	static char fixed[] = "fixed";
	char *fn = getXRes (rn, NULL);
	Display *dsp = XtDisplay(toplevel_w);
	XFontStruct *fsp;
	char msg[1024];

	if (!fn) {
	    (void) sprintf (msg, "No resource `%s' .. using fixed", rn);
	    xe_msg (msg, 0);
	    fn = fixed;
	}
	
	/* use XLoadQueryFont because it returns gracefully if font is not
	 * found; XLoadFont calls the default X error handler.
	 */
	fsp = XLoadQueryFont (dsp, fn);
	if (!fsp) {
	    (void) sprintf (msg, "No font `%s' for `%s' .. using fixed", fn,rn);
	    xe_msg(msg, 0);

	    fsp = XLoadQueryFont (dsp, fixed);
	    if (!fsp) {
		printf ("Can't even get %s!\n", fixed);
		exit(1);
	    }
	}

	return (fsp);
}


/* load the greek font into *greekfspp then create a new gc at *greekgcp and
 *   set its font to the font id..
 * leave *greekgcp and greekfssp unchanged if there's any problems.
 */
void
loadGreek (dsp, win, greekgcp, greekfspp)
Display *dsp;
Drawable win;
GC *greekgcp;
XFontStruct **greekfspp;
{
	static char grres[] = "viewsGreekFont";
	XFontStruct *fsp;	/* local fast access */
	GC ggc;			/* local fast access */
	unsigned long gcm;
	XGCValues gcv;
	char buf[1024];
	char *greekfn;
	
	greekfn = getXRes (grres, NULL);
	if (!greekfn) {
	    (void) sprintf (buf, "No resource: %s", grres);
	    xe_msg (buf, 0);
	    return;
	}

	fsp = XLoadQueryFont (dsp, greekfn);
	if (!fsp) {
	    (void) sprintf (buf, "No font %.100s: %.800s", grres, greekfn);
	    xe_msg (buf, 0);
	    return;
	}

	gcm = GCFont;
	gcv.font = fsp->fid;
	ggc = XCreateGC (dsp, win, gcm, &gcv);
	if (!ggc) {
	    XFreeFont (dsp, fsp);
	    xe_msg ("Can not make Greek GC", 0);
	    return;
	}

	*greekgcp = ggc;
	*greekfspp = fsp;

	return;
}

/* return a gray-scale ramp of pixels at *pixp, and the number in the ramp
 * N.B. don't change the pixels -- they are shared with other users.
 */
int
gray_ramp (dsp, cm, pixp)
Display *dsp;
Colormap cm;
Pixel **pixp;
{
	static Pixel gramp[MAXGRAY];
	static int ngray;

	if (ngray == 0) {
	    /* get gray ramp pixels once */
	    XColor white;

	    white.red = white.green = white.blue = ~0;
	    ngray = alloc_ramp (dsp, &white, cm, gramp, MAXGRAY);
	    if (ngray < MAXGRAY) {
		char msg[1024];
		(void) sprintf (msg, "Wanted %d but only found %d grays.",
							     MAXGRAY, ngray);
		xe_msg (msg, 0);
	    }
	}

	*pixp = gramp;
	return (ngray);
}

/* try to fill pix[maxn] with linear ramp from black to whatever is in base.
 * each entry will be unique; return said number, which can be <= maxn.
 * N.B. if we end up with just 2 colors, we set pix[0]=0 and pix[1]=1 in
 *   anticipation of caller using a XYBitmap and thus XPutImage for which
 *   color 0/1 uses the background/foreground of a GC.
 */
int
alloc_ramp (dsp, basep, cm, pix, maxn)
Display *dsp;
XColor *basep;
Colormap cm;
Pixel pix[];
int maxn;
{
	int nalloc, nunique;
	double r, g, b, h, s, v;
	XColor xc;
	int i, j;

	/* work in HSV space from 0..V */
	r = basep->red/65535.;
	g = basep->green/65535.;
	b = basep->blue/65535.;
	toHSV (r, g, b, &h, &s, &v);

	/* first just try to get them all */
	for (nalloc = 0; nalloc < maxn; nalloc++) {
	    toRGB (h, s, v*nalloc/(maxn-1), &r, &g, &b);
	    xc.red = (int)(r*65535);
	    xc.green = (int)(g*65535);
	    xc.blue = (int)(b*65535);
	    if (XAllocColor (dsp, cm, &xc))
		pix[nalloc] = xc.pixel;
	    else
		break;
	}

	/* see how many are actually unique */
	nunique = 0;
	for (i = 0; i < nalloc; i++) {
	    for (j = i+1; j < nalloc; j++)
		if (pix[i] == pix[j])
		    break;
	    if (j == nalloc)
		nunique++;
	}

	if (nunique < maxn) {
	    /* rebuild the ramp using just nunique entries.
	     * N.B. we assume we can get nunique colors again right away.
	     */
	    XFreeColors (dsp, cm, pix, nalloc, 0);

	    if (nunique <= 2) {
		/* we expect caller to use a XYBitmap via GC */
		pix[0] = 0;
		pix[1] = 1;
		nunique = 2;
	    } else {
		for (i = 0; i < nunique; i++) {
		    toRGB (h, s, v*i/(nunique-1), &r, &g, &b);
		    xc.red = (int)(r*65535);
		    xc.green = (int)(g*65535);
		    xc.blue = (int)(b*65535);
		    if (!XAllocColor (dsp, cm, &xc)) {
			nunique = i;
			break;
		    }
		    pix[i] = xc.pixel;
		}
	    }
	}

	return (nunique);
}

/* create an XImage of size wXh.
 * return XImage * if ok else NULL and xe_msg().
 */
static XImage *
create_xim (w, h)
int w, h;
{
	Display *dsp = XtDisplay(toplevel_w);
	XImage *xip;
	int mdepth;
	int mbpp;
	int nbytes;
	char *data;

	/* establish depth and bits per pixel */
	get_something (toplevel_w, XmNdepth, (XtArgVal)&mdepth);
	if (mdepth < 8) {
	    fprintf (stderr, "Require at least 8 bit pixel depth\n");
	    return (NULL);
	}
	mbpp = mdepth>=17 ? 32 : (mdepth >= 9 ? 16 : 8);
	nbytes = w*h*mbpp/8;

	/* get memory for image pixels.  */
	data = malloc (nbytes);
	if (!data) {
	    fprintf(stderr,"Can not get %d bytes for image pixels", nbytes);
	    return (NULL);
	}

	/* create the XImage */
	xip = XCreateImage (dsp, DefaultVisual (dsp, DefaultScreen(dsp)),
	    /* depth */         mdepth,
	    /* format */        ZPixmap,
	    /* offset */        0,
	    /* data */          data,
	    /* width */         w,
	    /* height */        h,
	    /* pad */           mbpp,
	    /* bpl */           0);
	if (!xip) {
	    fprintf (stderr, "Can not create %dx%d XImage\n", w, h);
	    free ((void *)data);
	    return (NULL);
	}

        xip->bitmap_bit_order = LSBFirst;
	xip->byte_order = LSBFirst;

	/* ok */
	return (xip);
}

/* like XFreeColors but frees the pixels in xcols[nxcols]
 */
void
freeXColors (dsp, cm, xcols, nxcols)
Display *dsp;
Colormap cm;
XColor xcols[];
int nxcols;
{
	unsigned long *xpix = (Pixel*)XtMalloc(nxcols * sizeof(unsigned long));
	int i;

	for (i = 0; i < nxcols; i++)
	    xpix[i] = xcols[i].pixel;

	XFreeColors (dsp, cm, xpix, nxcols, 0);

	XtFree ((void *)xpix);
}

/* given a raw (still compressed) gif file in gif[ngif], malloc its 1-byte
 * pixels in *gifpix[*wp][*hp] and fill xcols[256] with X pixels and colors
 * that work when indexed by an gifpix entry.
 * return 0 if ok, else -1 with err[] containing a reason why not.
 */
int
gif2X (dsp, cm, gif, ngif, wp, hp, gifpix, xcols, err)
Display *dsp;		/* X server */
Colormap cm;		/* colormap for xcols[] */
unsigned char gif[];	/* raw (still compressed) gif file contents */
int ngif;		/* bytes in gif[] */
int *wp, *hp;		/* size of exploded gif */
unsigned char **gifpix;	/* ptr to array we malloc for gif pixels */
XColor xcols[256];	/* X pixels and colors when indexed by gifpix */
char err[];		/* error message if we return -1 */
{
	unsigned char gifr[256], gifg[256], gifb[256];
	int i;

	/* uncompress */
	if (explodeGIF(gif, ngif, wp, hp, gifpix, gifr, gifg, gifb, err) < 0)
	    return (-1);

	/* allocate colors -- don't be too fussy */
	for (i = 0; i < 256; i++) {
	    XColor *xcp = xcols+i;
	    xcp->red   = ((short)(gifr[i] & 0xf8) << 8) | 0x7ff;
	    xcp->green = ((short)(gifg[i] & 0xf8) << 8) | 0x7ff;
	    xcp->blue  = ((short)(gifb[i] & 0xf8) << 8) | 0x7ff;
	    if (!XAllocColor (dsp, cm, xcp)) {
		strcpy (err, "Can not get all image map colors");
		free ((void *)(*gifpix));
		if (i > 0)
		    freeXColors (dsp, cm, xcols, i);
		return (-1);
	    }
	}

	/* ok */
	return (0);
}

/* given a raw gif file in gif[ngif] return a new pixmap and its size.
 * return 0 if ok, else fill why[] and return -1.
 */
int
gif2pm (dsp, cm, gif, ngif, wp, hp, pmp, why)
Display *dsp;
Colormap cm;
unsigned char gif[];
int ngif;
int *wp, *hp;
Pixmap *pmp;
char why[];
{
	Window win = RootWindow(dsp, DefaultScreen(dsp));
	unsigned char *gifpix;
	XColor xcols[256];
	XImage *xip;
	Pixmap pm;
	GC gc;
	int w, h;
	int x, y;

	/* get X version of image */
	if (gif2X (dsp, cm, gif, ngif, &w, &h, &gifpix, xcols, why) < 0)
	    return (-1);

	/* create XImage */
	xip = create_xim (w, h);
	if (!xip) {
	    freeXColors (dsp, cm, xcols, 256);
	    free ((void *)gifpix);
	    strcpy (why, "No memory for image");
	    return (-1);
	}

	/* N.B. now obligued to free xip */

	/* fill XImage with image */
	for (y = 0; y < h; y++) {
	    int yrow = y*w;
	    for (x = 0; x < w; x++) {
		int gp = (int)gifpix[x + yrow];
		unsigned long p = xcols[gp].pixel;
		XPutPixel (xip, x, y, p);
	    }
	}

	/* create pixmap and fill with image */
	pm = XCreatePixmap (dsp, win, w, h, xip->depth);
	gc = DefaultGC (dsp, DefaultScreen(dsp));
	XPutImage (dsp, pm, gc, xip, 0, 0, 0, 0, w, h);

	/* free gifpix and xip */
	free ((void *)gifpix);
	free ((void *)xip->data);
	xip->data = NULL;
	XDestroyImage (xip);

	/* that's it! */
	*wp = w;
	*hp = h;
	*pmp = pm;
	return (0);
}

/* search for  the named X resource from all the usual places.
 * this looks in more places than XGetDefault().
 * we just return it as a string -- caller can do whatever.
 * return def if can't find it anywhere.
 * N.B. memory returned is _not_ malloced so leave it be.
 */
char *
getXRes (name, def)
char *name;
char *def;
{
	static char notfound[] = "_Not_Found_";
	char *res = NULL;
	XtResource xr;

	xr.resource_name = name;
	xr.resource_class = "AnyClass";
	xr.resource_type = XmRString;
	xr.resource_size = sizeof(String);
	xr.resource_offset = 0;
	xr.default_type = XmRImmediate;
	xr.default_addr = (XtPointer)notfound;

	XtGetApplicationResources (toplevel_w, (void *)&res, &xr, 1, NULL, 0);
	if (!res || strcmp (res, notfound) == 0)
	    res = def;

	return (res);
}

/* set the given application (ie, myclass.name) resource */
void
setXRes (name, val)
char *name, *val;
{
	XrmDatabase db = XrmGetDatabase (XtDisplay(toplevel_w));
	char buf[1024];

	sprintf (buf, "%s.%s:%s", myclass, name, val);
	XrmPutLineResource (&db, buf);
}

/* build and return a private colormap for toplevel_w.
 * nnew is how many colors we expect to add.
 */
Colormap
createCM(nnew)
int nnew;
{
#define	NPRECM	50  /* try to preload new cm with NPRECM colors from def cm */
	Display *dsp = XtDisplay (toplevel_w);
	Window win = RootWindow (dsp, DefaultScreen(dsp));
	Colormap defcm = DefaultColormap (dsp, DefaultScreen(dsp));
	int defcells = DisplayCells (dsp, DefaultScreen(dsp));
	Visual *v = DefaultVisual (dsp, DefaultScreen(dsp));
	Colormap newcm;

	/* make a new colormap */
	newcm = XCreateColormap (dsp, win, v, AllocNone);

	/* preload with some existing colors to hedge flashing, if room */
	if (nnew + NPRECM < defcells) {
	    XColor preload[NPRECM];
	    int i;

	    for (i = 0; i < NPRECM; i++)
		preload[i].pixel = (unsigned long) i;
	    XQueryColors (dsp, defcm, preload, NPRECM);
	    for (i = 0; i < NPRECM; i++)
		(void) XAllocColor (dsp, newcm, &preload[i]);
	}

	return (newcm);
}

/* depending on the "install" resource and whether cm can hold nwant more
 * colors, return a new colormap or cm again.
 */
Colormap
checkCM(cm, nwant)
Colormap cm;
int nwant;
{
	Display *dsp = XtDisplay(toplevel_w);
	char *inst;

	/* get the install resource value */
	inst = getXRes ("install", "guess");

	/* check each possible value */
	if (strcmp (inst, "no") == 0)
	    return (cm);
	else if (strcmp (inst, "yes") == 0)
	    return (createCM (nwant));
	else if (strcmp (inst, "guess") == 0) {
	    /* get a smattering of colors and opt for private cm if can't.
	     * we use alloc_ramp() because it verifies unique pixels.
	     * we use three to not overstress the resolution of colors.
	     */
	    Pixel *rr, *gr, *br;
	    int neach, nr, ng, nb;
	    XColor xc;

	    /* grab some to test */
	    neach = nwant/3;
	    xc.red = 255 << 8;
	    xc.green = 0;
	    xc.blue = 0;
	    rr = (Pixel *) malloc (neach * sizeof(Pixel));
	    nr = alloc_ramp (dsp, &xc, cm, rr, neach);
	    xc.red = 0;
	    xc.green = 255 << 8;
	    xc.blue = 0;
	    gr = (Pixel *) malloc (neach * sizeof(Pixel));
	    ng = alloc_ramp (dsp, &xc, cm, gr, neach);
	    xc.red = 0;
	    xc.green = 0;
	    xc.blue = 255 << 8;
	    br = (Pixel *) malloc (neach * sizeof(Pixel));
	    nb = alloc_ramp (dsp, &xc, cm, br, neach);

	    /* but don't keep them.
	     * N.B. alloc_ramp just cheats us with B&W if it returns 2.
	     */
	    if (nr > 2)
		XFreeColors (dsp, cm, rr, nr, 0);
	    if (ng > 2)
		XFreeColors (dsp, cm, gr, ng, 0);
	    if (nb > 2)
		XFreeColors (dsp, cm, br, nb, 0);
	    free ((void *)rr);
	    free ((void *)gr);
	    free ((void *)br);

	    if (nr + ng + nb < 3*neach)
		return (createCM(nwant));
	} else
	    printf ("Unknown install `%s' -- defaulting to No\n", inst);

	return (cm);
}

/* explicitly handle pending X events when otherwise too busy */
void
XCheck (app)
XtAppContext app;
{
        while ((XtAppPending (app) & XtIMXEvent) == XtIMXEvent)
	    XtAppProcessEvent (app, XtIMXEvent);
}

/* center the scrollbars in the given scrolled window */
void
centerScrollBars(sw_w)
Widget sw_w;
{
	int min, max, slidersize, value;
	XmScrollBarCallbackStruct sbcs;
	Widget sb_w;

	/* you would think setting XmNvalue would be enough but seems we
	 * must trigger the callback too, at least on MKS
	 */
	sbcs.reason = XmCR_VALUE_CHANGED;
	sbcs.event = NULL;	/* ? */

	get_something (sw_w, XmNhorizontalScrollBar, (XtArgVal)&sb_w);
	get_something (sb_w, XmNminimum, (XtArgVal)&min);
	get_something (sb_w, XmNmaximum, (XtArgVal)&max);
	get_something (sb_w, XmNsliderSize, (XtArgVal)&slidersize);
	sbcs.value = value = (min+max-slidersize)/2;
	set_something (sb_w, XmNvalue, (XtArgVal)value);
	XtCallCallbacks (sb_w, XmNvalueChangedCallback, &sbcs);

	get_something (sw_w, XmNverticalScrollBar, (XtArgVal)&sb_w);
	get_something (sb_w, XmNminimum, (XtArgVal)&min);
	get_something (sb_w, XmNmaximum, (XtArgVal)&max);
	get_something (sb_w, XmNsliderSize, (XtArgVal)&slidersize);
	sbcs.value = value = (min+max-slidersize)/2;
	set_something (sb_w, XmNvalue, (XtArgVal)value);
	XtCallCallbacks (sb_w, XmNvalueChangedCallback, &sbcs);	
}

/* set the XmNcolumns resource of the given Text or TextField widget to the
 * full length of the current string.
 */
void
textColumns (w)
Widget w;
{
	Arg args[10];
	char *bp;
	int n;

	if (XmIsText(w))
	    bp = XmTextGetString (w);
	else if (XmIsTextField(w))
	    bp = XmTextFieldGetString (w);
	else
	    return;

	n = 0;
	XtSetArg (args[n], XmNcolumns, strlen(bp)); n++;
	XtSetValues (w, args, n);

	XtFree (bp);

}

/* check the given XmText or XmTextField value:
 * if empty, fill with "x/y", or just "x" if !y.
 * then set size to accommodate if setcols.
 */
void
defaultTextFN (w, setcols,  x, y)
Widget w;
int setcols;
char *x, *y;
{
	char *tp = XmTextGetString (w);

	if (tp[0] == '\0') {
	    char buf[1024];
	    if (y)
		sprintf (buf, "%s/%s", x, y);
	    else
		strcpy (buf, x);
	    XmTextSetString (w, buf);
	}
	XtFree (tp);

	if (setcols)
	    textColumns (w);
}

/* turn cursor on/off to follow focus */
static void
textFixCursorCB(w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmAnyCallbackStruct *ap = (XmAnyCallbackStruct *)call;
	Arg a;

	XtSetArg (a, XmNcursorPositionVisible, ap->reason == XmCR_FOCUS);
	XtSetValues (w, &a, 1);
}

/* call just after creation to work around ugly grey cursor in idle Text or
 * TextField
 */
void
fixTextCursor (w)
Widget w;
{
	Arg a;

	XtSetArg (a, XmNcursorPositionVisible, False);
	XtSetValues (w, &a, 1);

	XtAddCallback (w, XmNfocusCallback, textFixCursorCB, 0);
	XtAddCallback (w, XmNlosingFocusCallback, textFixCursorCB, 0);
}

/* set up look_like_button[] and look_like_label[] */
void
setButtonInfo()
{
	Pixel topshadcol, botshadcol, bgcol;
	Pixmap topshadpm, botshadpm;
	Widget sample;
	Arg args[20];
	int n;

	n = 0;
	sample = XmCreatePushButton (toplevel_w, "TEST", args, n);

	n = 0;
	XtSetArg (args[n], XmNtopShadowColor, &topshadcol); n++;
	XtSetArg (args[n], XmNbottomShadowColor, &botshadcol); n++;
	XtSetArg (args[n], XmNtopShadowPixmap, &topshadpm); n++;
	XtSetArg (args[n], XmNbottomShadowPixmap, &botshadpm); n++;
	XtSetArg (args[n], XmNbackground, &bgcol); n++;
	XtGetValues (sample, args, n);

	look_like_button[0].value = topshadcol;
	look_like_button[1].value = botshadcol;
	look_like_button[2].value = topshadpm;
	look_like_button[3].value = botshadpm;
	look_like_label[0].value = bgcol;
	look_like_label[1].value = bgcol;
	look_like_label[2].value = XmUNSPECIFIED_PIXMAP;
	look_like_label[3].value = XmUNSPECIFIED_PIXMAP;

	XtDestroyWidget (sample);
}

/* manipulate the given XmButton resources so it indeed looks like a button
 * or like a label.
 */
void
buttonAsButton (w, whether)
Widget w;
int whether;
{
	if (!look_like_inited) {
	    setButtonInfo();
	    look_like_inited = 1;
	}

	XtSetValues (w, whether ? look_like_button : look_like_label, 6);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: xmisc.c,v $ $Date: 2001/04/19 21:12:04 $ $Revision: 1.1.1.1 $ $Name:  $"};
