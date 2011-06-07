/* code to open or fetch FITS files for skyview.
 * this includes the net connections to STScI and ESO.
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#if defined(__STDC__)
#include <stdlib.h>
#include <string.h>
#else
extern void *malloc();
#endif

#if defined(_POSIX_SOURCE)
#include <unistd.h>
#else
extern int close();
extern int unlink();
#endif

#if defined(VMS) && !defined(O_WRONLY)
#include <file.h>
#endif
#if !defined(O_WRONLY) || !defined(O_CREAT)
#include <fcntl.h>
#endif

#ifndef X_OK
#define	X_OK	1
#endif

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/DrawingA.h>
#include <Xm/RowColumn.h>
#include <Xm/ToggleB.h>
#include <Xm/TextF.h>
#include <Xm/Scale.h>
#include <Xm/FileSB.h>
#include <Xm/MessageB.h>
#include <Xm/PanedW.h>
#include <Xm/Text.h>


#include "P_.h"
#include "astro.h"
#include "fits.h"
#include "net.h"
#include "ps.h"
#include "patchlevel.h"

#define	MAXDSSFOV	(20.)	/* max field size we retreive, arcmins*/
#define	MINDSSFOV	(5.)	/* min field size we retreive, arcmins*/

extern Widget toplevel_w;
extern XtAppContext xe_app;
#define XtD XtDisplay(toplevel_w)
extern Colormap xe_cm;

extern Widget svshell_w;

extern char *expand_home P_((char *path));
extern char *getShareDir P_((void));
extern char *getPrivateDir P_((void));
extern char *getXRes P_((char *name, char *def));
extern char *syserrstr P_((void));
extern int confirm P_((void));
extern int existsh P_((char *filename));
extern int get_color_resource P_((Widget w, char *cname, Pixel *p));
extern int gray_ramp P_((Display *dsp, Colormap cm, Pixel **pixp));
extern int openh P_((char *name, int flags, ...));
extern int xy2RADec P_((FImage *fip, double x, double y, double *rap,
    double *decp));
extern void defaultTextFN P_((Widget w, int cols, char *x, char *y));
extern void get_something P_((Widget w, char *resource, XtArgVal value));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void mm_newcaldate P_((double newmjd));
extern void msg_manage P_((void));
extern void pm_set P_((int percentage));
extern void pm_down P_((void));
extern void pm_up P_((void));
extern void prompt_map_cb P_((Widget w, XtPointer client, XtPointer call));
extern void query P_((Widget tw, char *msg, char *label1, char *label2,
    char *label3, void (*func1)(void), void (*func2)(void),
    void (*func3)(void)));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void stopd_up P_((void));
extern void stopd_down P_((void));
extern void sv_getcenter P_((int *aamode, double *fov,
    double *altp, double *azp, double *rap, double *decp));
extern void sv_newfits P_((void));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *msg, int app_modal));

void sf_off P_((void));
void sf_go_cb P_((Widget w, XtPointer client, XtPointer call));

static void build_histo P_((void));
static void build_colormap P_((void));
static Pixel gray_pixel P_((int fp));
static int sf_readFile P_((char *name));
static void sf_create P_((void));
static void sf_newLOHI P_((void));
static void sf_newImage P_((void));
static void initFSB P_((Widget w));
static void initPubShared P_((Widget rc_w, Widget fsb_w));
static void sf_save_cb P_((Widget w, XtPointer client, XtPointer call));
static void save_file P_((void));
static void sf_open_cb P_((Widget w, XtPointer client, XtPointer call));
static void sf_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void sf_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void sf_exp_cb P_((Widget w, XtPointer client, XtPointer call));
static void sf_motion_eh P_((Widget w, XtPointer client, XEvent *ev,
    Boolean *dispatch));
static void sf_gmlohi_cb P_((Widget w, XtPointer client, XtPointer call));
static void sf_wide_cb P_((Widget w, XtPointer client, XtPointer call));
static void sf_setdate_cb P_((Widget w, XtPointer client, XtPointer call));
static void sf_narrow_cb P_((Widget w, XtPointer client, XtPointer call));
static void sf_gamma_cb P_((Widget w, XtPointer client, XtPointer call));
static void sf_inv_cb P_((Widget w, XtPointer client, XtPointer call));
static void sf_drawHistogram P_((void));
static int loweredge P_((void));
static void sf_showHeader P_((void));
static void sf_setSaveName P_((char *newfn, int usesuf));

static void makeGlassImage P_((Display *dsp));
static void makeGlassGC P_((Display *dsp, Window win));
static void fillGlass P_((int lr, int tb, int wx, int wy));

static void eso_fits P_((void));
static void stsci_fits P_((void));
static void fits_read_cb P_((XtPointer client, int *fd, XtInputId *id));
static int dsswcs P_((FImage *fip));
static int dssxy2RADec P_((FImage *fip, double X, double Y, double *rap,
    double *decp));
static int wcsok P_((FImage *fip));
static int fitsObs P_((double *mjdp));
static void newfim P_((FImage *fip));
static int prepOpen P_((char fn[], char errmsg[]));

static Widget sf_w;		/* main dialog */
static Widget gunzip_w;		/* TB for whether to retrieve compressed */
static Widget savefn_w;		/* TF for save filename */
static Widget stsci_w;		/* TB for STScI, else ESO */
static Widget fsb_w;		/* FSB for opening a file */
static Widget hdr_w;		/* ScrolledText for the FITS header */
static Widget fda_w;		/* Drawing area for the contrast map */
static Widget gamma_w;		/* gamma scale */
static Widget autoname_w;	/* TB for whether to auto set save filename */ 
static Widget obsdate_w;	/* label for obs date string */
static Widget setobsdate_w;	/* PB to set main to obs date */
static Widget live_w;		/* TB whether want live dragging */
static Widget dss1_w;		/* TB set to use DSS 1 */
static Widget dss2r_w;		/* TB set to use DSS 2 red */
static Widget dss2b_w;		/* TB set to use DSS 2 blue */

static FImage fim;		/* current FITS image */
static Pixmap fpm;		/* current pixmap with image */
static int fimok;		/* 1 if fim is ok, else 0 */
static Pixel *gray;		/* gray-scale ramp for drawing image */
static int ngray;		/* number of pixels usable in gray[] */
static int fdepth;		/* depth of image, in bits */
static int fbpp;		/* bits per pixel in image: 1, 8, 16 or 32 */
static XtIntervalId readId;	/* set while working on reading from socket  */
static int lopix, hipix;	/* lo and hi image pixel values */
static int *histo;		/* malloced histogram of current image */
static int h_mini, h_maxi;	/* histo[] index to first and last pixel used */
static int h_peaki;		/* index of largest value in histo[] */
static char *colormap;		/* malloced gray[] indeces for current image */
static int want_inv;		/* set when want inverse video effect */

/* which survey */
typedef enum {
    DSS_1, DSS_2R, DSS_2B
} Survey;
static Survey whichSurvey P_((void));

static char hxr[] = "FITSHistoColor";	/* X resource name of histogram color */
static char mxr[] = "FITSMapColor";	/* X resource name of colormap color */
#define	MMH	10		/* map marker height */
#define	MMW	5		/* map marker half-width */

static char fitsp[] = "FITSpattern";	/* resource name of FITS file pattern */
static char gcmd[] = "gunzip";		/* gunzip command */

static XImage *glass_xim;       /* glass XImage -- 0 means new or can't */
static GC glassGC;              /* GC for glass border */

#define GLASSSZ         50      /* mag glass width and heigth, pixels */
#define GLASSMAG        2       /* mag glass factor (may be any integer > 0) */

/* choice of colormap method */
typedef enum { LH_GAMMA, LH_HEQ } LowHi;
static LowHi lowhi;		/* default is set from new TBs */
#define	GAM_MAX		3.0	/* gamma ranges from 1/GAM_MAX .. GAM_MAX */

static char skyfitscategory[] = "Sky View -- Images";	/* Save category */

/* called to manage the fits dialog.
 */
void
sf_manage()
{
	if (!sf_w)
	    sf_create();

	XtManageChild(sf_w);
}

/* called to unmanage the fits dialog.
 * this is only called when the Sky View is also going away, so we
 * can discard memory for the image, histogram and colormap too.
 */
void
sf_unmanage()
{
	if (!sf_w)
	    return;

	sf_off();

	if (histo) {
	    free (histo);
	    histo = 0;
	}
	if (colormap) {
	    free (colormap);
	    colormap = 0;
	}

	XtUnmanageChild (sf_w);
}

/* return 1 if dialog is up, else 0.
 */
int
sf_ismanaged()
{
	return (sf_w && XtIsManaged(sf_w));
}

/* return 1 if there is currently a valid FITS image available, else 0 */
int
sf_ison()
{
	return (fimok);
}

/* discard any current FITS image */
void
sf_off()
{
	if (fimok) {
	    resetFImage (&fim);
	    fimok = 0;
	}
	if (fpm) {
	    XFreePixmap (XtD, fpm);
	    fpm = (Pixmap)0;
	}
}

/* called to put up or remove the watch cursor.  */
void
sf_cursor (c)
Cursor c;
{
	Window win;

	if (sf_w && (win = XtWindow(sf_w)) != 0) {
	    Display *dsp = XtDisplay(sf_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* return copies of the current filename and OBJECT or TARGET keywords.
 * if either can not be determined, the returned string will be 0 length.
 * N.B. we assume the caller supplies "enough" space.
 */
void
sf_getName (fn, on)
char *fn;	/* filename */
char *on;	/* object name */
{
	char *savefn;
	int i, n;

	savefn = XmTextFieldGetString (savefn_w);
	n = strlen (savefn);

	for (i = n-1; i >= 0 && savefn[i] != '/'; --i)
	    continue;
	strcpy (fn, &savefn[i+1]);
	XtFree (savefn);

	if (getStringFITS (&fim, "OBJECT", on) < 0 &&
				getStringFITS (&fim, "TARGET", on) < 0)
	    *on = '\0';
}

/* return pointer to the current FImage if any, else NULL */
FImage *
sf_getFImage ()
{
	return (fimok ? &fim : NULL);
}

/* return the current Pixmap */
Pixmap
sf_getPixmap ()
{
	return (fpm);
}

/* send the current image to the postscript machine, appropriately flipped
 * and through the colormap.
 */
void
sf_ps (w, lr, tb)
Window w;
int lr, tb;
{
	unsigned short *ip;

	if (!fimok)
	    return;

	/* draw.
	 */
	ip = (unsigned short *) fim.image;
	XPSImage (w, ip, colormap, ngray-1, want_inv, 0,0,fim.sh,fim.sw,lr,tb);
}

/* build a new pixmap, fpm, from the XImage fim with the given flipping
 * orientation and the current colormap options.
 * w is just used to get the pixmap depth.
 */
void
sf_newPixmap (w, lr, tb)
Widget w;
int lr, tb;
{
	Display *dsp = XtDisplay (w);
	Window win = XtWindow (w);
	GC gc = DefaultGC(dsp, DefaultScreen(dsp));
	XImage *f_xim;		/* XImage: adjusted for flipping and lo/hi */
	char *data;		/* pixel data for f_xim */
	int nbytes;		/* total bytes in data[] */
	char buf[1024];		/* misc buffer */
	CamPixel *ip;		/* quick index into fim.image */
	int nrows, ncols;	/* size of image */
	int x, y;

	if (!fimok)
	    return;

	watch_cursor (1);

	ncols = fim.sw;
	nrows = fim.sh;

	/* establish depth and bits per pixel */
	get_something (w, XmNdepth, (XtArgVal)&fdepth);
	fbpp = (fdepth == 1 || ngray == 2) ? 1 :
				(fdepth>=17 ? 32 : (fdepth >= 9 ? 16 : 8));

	/* get memory for image pixels */
	nbytes = (nrows+7)*(ncols+7)*fbpp/8;
	data = (char *) malloc (nbytes);
	if (!data) {
	    (void) sprintf(buf, "Can not get %d bytes for FITS pixels", nbytes);
	    xe_msg (buf, 1);
	    watch_cursor (0);
	    return;
	}

	/* create the XImage */
	f_xim = XCreateImage (dsp, XDefaultVisual (dsp, DefaultScreen(dsp)),
	    /* depth */         fbpp == 1 ? 1 : fdepth,
	    /* format */        fbpp == 1 ? XYBitmap : ZPixmap,
	    /* offset */        0,
	    /* data */          data,
	    /* width */         ncols,
	    /* height */        nrows,
	    /* pad */           fbpp < 8 ? 8 : fbpp,
	    /* bpl */           0);
	if (!f_xim) {
	    xe_msg ("Can not create shadow XImage", 1);
	    free ((void *)data);
	    watch_cursor (0);
	    return;
	}

        f_xim->bitmap_bit_order = LSBFirst;
	f_xim->byte_order = LSBFirst;

	/* fill image via gray map and allow for flipping */
	ip = (CamPixel *) fim.image;
	for (y = 0; y < nrows; y++) {
	    int flipy = tb ? nrows-y-1 : y;
	    if (lr) {
		for (x = ncols-1; x >= 0; --x) {
		    int fp = (int)(*ip++);
		    Pixel xp = gray_pixel (fp);
		    XPutPixel (f_xim, x, flipy, xp);
		}
	    } else {
		for (x = 0; x < ncols; x++) {
		    int fp = (int)(*ip++);
		    Pixel xp = gray_pixel (fp);
		    XPutPixel (f_xim, x, flipy, xp);
		}
	    }
	}

	/* create the pixmap */
	if (fpm)
	    XFreePixmap (dsp, fpm);
	fpm = XCreatePixmap (dsp, win, ncols, nrows, fdepth);

	/* copy the image to the pixmap */
	XPutImage (dsp, fpm, gc, f_xim, 0, 0, 0, 0, ncols, nrows);

	/* finished with the image */
	free ((void *)f_xim->data);
	f_xim->data = NULL;
	XDestroyImage (f_xim);

	watch_cursor (0);
}

/* compute the histogram for fim and put it in histo[].
 */
static void
build_histo()
{
	FImage *fip = &fim;
	CamPixel *ip = (CamPixel *)fip->image;
	int npix = fip->sw*fip->sh;
	int i;

	/* insure memory */
	if (!histo) {
	    histo = (int *) malloc (MAXCAMPIX * sizeof(histo[0]));
	    if (!histo) {
		printf ("No memory for histogram\n");
		exit (1);
	    }
	}

	/* zero the histogram */
	memset ((void *)histo, 0, MAXCAMPIX*sizeof(histo[0]));

	/* find histogram */
	for (i = 0; i < npix; i++)
	    histo[(int)(*ip++)]++;

	/* find boundary stats */
	for (i = 0; i < MAXCAMPIX && histo[i] == 0; i++)
	    continue;
	h_mini = i < MAXCAMPIX ? i : 0;
	for (i = MAXCAMPIX; --i >= 0 && histo[i] == 0; )
	    continue;
	h_maxi = i >= 0 ? i : 0;
	h_peaki = 0;
	for (i = h_mini; i <= h_maxi; i++)
	    if (histo[i] > histo[h_peaki])
		h_peaki = i;
}

/* compute the colormap for fim and put in colormap, allowing for method
 * and lo/hi.
 * N.B. if we need the histogram, we assume it's already built.
 */
static void
build_colormap()
{
	int range = hipix - lopix;
	int total, sum, i;

	/* insure memory */
	if (!colormap) {
	    colormap = malloc (MAXCAMPIX*sizeof(colormap[0]));
	    if (!colormap) {
		printf ("No memory for colormap\n");
		exit (1);
	    }
	}

	switch (lowhi) {
	case LH_GAMMA:
	    for (i = 0; i < lopix; i++)
		colormap[i] = 0;
	    XmScaleGetValue (gamma_w, &sum);
	    for (; i < hipix; i++)
		colormap[i] =
			(int)(pow((double)(i-lopix)/range, sum/100.0)*ngray);
	    for (; i < MAXCAMPIX; i++)
		colormap[i] = ngray-1;
	    break;

	case LH_HEQ:
	    for (i = 0; i < lopix; i++)
		colormap[i] = 0;
	    for (total = 0; i <= hipix; i++)
		total += histo[i];
	    for (sum = 0, i = lopix; i <= hipix; i++)
		colormap[i] = (sum += histo[i])*(ngray-1)/total;
	    for (; i < MAXCAMPIX; i++)
		colormap[i] = ngray-1;
	    break;
	}
}

/* given a FITS pixel, return an X pixel */
static Pixel
gray_pixel (fp)
int fp;
{
	int gp = (int)colormap[fp];

	if (want_inv)
	    gp = (ngray-1) - gp;

	return (gray[gp]);

}

/* handle the operation of the magnifying glass.
 * this is called whenever there is left button activity over the image.
 */
void
sf_doGlass (dsp, win, b1p, m1, lr, tb, wx, wy)
Display *dsp;
Window win;
int b1p, m1;	/* button/motion state */
int lr, tb;	/* flipping flags */
int wx, wy;	/* window coords of cursor */
{
	static int lastwx, lastwy;
	int rx, ry, rw, rh;		/* region */

	if (!fimok)
	    return;

	/* check for first-time stuff */
	if (!glass_xim)
	    makeGlassImage (dsp);
	if (!glass_xim)
	    return; /* oh well */
	if (!glassGC)
	    makeGlassGC (dsp, win);

	if (m1) {

	    /* motion: put back old pixels that won't just be covered again */

	    /* first the vertical strip that is uncovered */

	    rh = GLASSSZ*GLASSMAG;
	    ry = lastwy - (GLASSSZ*GLASSMAG/2);
	    if (ry < 0) {
		rh += ry;
		ry = 0;
	    }
	    if (wx < lastwx) {
		rw = lastwx - wx;	/* cursor moved left */
		rx = wx + (GLASSSZ*GLASSMAG/2);
	    } else {
		rw = wx - lastwx;	/* cursor moved right */
		rx = lastwx - (GLASSSZ*GLASSMAG/2);
	    }
	    if (rx < 0) {
		rw += rx;
		rx = 0;
	    }

	    if (rw > 0 && rh > 0)
		XCopyArea (dsp, fpm, win, glassGC, rx, ry, rw, rh, rx, ry);

	    /* then the horizontal strip that is uncovered */

	    rw = GLASSSZ*GLASSMAG;
	    rx = lastwx - (GLASSSZ*GLASSMAG/2);
	    if (rx < 0) {
		rw += rx;
		rx = 0;
	    }
	    if (wy < lastwy) {
		rh = lastwy - wy;	/* cursor moved up */
		ry = wy + (GLASSSZ*GLASSMAG/2);
	    } else {
		rh = wy - lastwy;	/* cursor moved down */
		ry = lastwy - (GLASSSZ*GLASSMAG/2);
	    }
	    if (ry < 0) {
		rh += ry;
		ry = 0;
	    }

	    if (rw > 0 && rh > 0)
		XCopyArea (dsp, fpm, win, glassGC, rx, ry, rw, rh, rx, ry);
	}

	if (b1p || m1) {

	    /* start or new location: show glass and save new location */

	    fillGlass (lr, tb, wx, wy);
	    XPutImage (dsp, win, glassGC, glass_xim, 0, 0,
			wx-(GLASSSZ*GLASSMAG/2), wy-(GLASSSZ*GLASSMAG/2),
			GLASSSZ*GLASSMAG, GLASSSZ*GLASSMAG);
	    lastwx = wx;
	    lastwy = wy;

	    /* kinda hard to tell boundry of glass so draw a line around it */
	    XDrawRectangle (dsp, win, glassGC,
			wx-(GLASSSZ*GLASSMAG/2), wy-(GLASSSZ*GLASSMAG/2),
			GLASSSZ*GLASSMAG-1, GLASSSZ*GLASSMAG-1);
	}
}

/* t00fri: include possibility to read .fth compressed files */
static int
prepOpen (fn, errmsg)
char fn[];
char errmsg[];
{
	int fd;
	int l;

	l = strlen (fn);
	if (l < 4 || strcmp(fn+l-4, ".fth")) {
	    /* just open directly */
	    fd = openh (fn, O_RDONLY);
	    if (fd < 0)
		strcpy (errmsg, syserrstr());
	} else {
	    /* ends with .fth so need to run through fdecompress
	     * TODO: this is a really lazy way to do it --
	     */
	    char cmd[2048];
	    char tmp[128];
	    int s;

	    tmpnam (tmp);
	    strcat (tmp, ".fth");
	    sprintf (cmd, "cp %s %s; fdecompress -r %s", fn, tmp, tmp);
	    s = system (cmd);
	    if (s != 0) {
		sprintf (errmsg, "Can not execute `%s' ", cmd);
		if (s < 0)
		    strcat (errmsg, syserrstr());
		fd = -1;
	    } else {
		tmp[strlen(tmp)-1] = 's';
		fd = openh (tmp, O_RDONLY);
		(void) unlink (tmp);	/* once open, remove the .fts copy */
		if (fd < 0)
		    sprintf (errmsg, "Can not decompress %s: %s", tmp,
							    syserrstr());
	    }
	}

	return (fd);
}

/* attempt to open a FITS file. also insure it has WCS and other fields.
 * if all ok, fill in fim and return 0, else return -1.
 */
static int
sf_readFile (name)
char *name;
{
	char buf[1024];
	FImage sfim, *fip = &sfim;
	double eq;
	char errmsg[1024];
	int fd;
	int s;

	/* init */
	initFImage (fip);

	/* open the fits file */
	fd = prepOpen (name, errmsg);
	if (fd < 0) {
	    (void) sprintf (buf, "%s: %s", name, errmsg);
	    xe_msg (buf, 1);
	    return(-1);
	}

	/* read in */
	s = readFITS (fd, fip, buf);
	close (fd);
	if (s < 0) {
	    char buf2[1024];
	    (void) sprintf (buf2, "%s: %s", name, buf);
	    xe_msg (buf2, 1);
	    return(-1);
	}

	/* we only support 16 bit integer images */
	if (fip->bitpix != 16) {
	    (void) sprintf (buf, "%s: must be BITPIX 16", name);
	    xe_msg (buf, 1);
	    resetFImage (fip);
	    return (-1);
	}

	/* make sure it has WCS fields */
	if (wcsok (fip) < 0) {
	    resetFImage (fip);	/* already wrote xe_msg() */
	    return (-1);
	}

	/* EQUINOX is required, but we'll accept EPOCH in a pinch */
	if (getRealFITS (fip, "EQUINOX", &eq) < 0) {
	    if (getRealFITS (fip, "EPOCH", &eq) < 0) {
		(void) sprintf (buf, "%s: Neither EQUINOX nor EPOCH", name);
		xe_msg (buf, 1);
		resetFImage (fip);
		return (-1);
	    } else {
		setRealFITS (fip, "EQUINOX", eq, 10, "Copied from EPOCH");
	    }
	}

	/* ok!*/
	newfim (fip);
	return (0);
}

/* set up an initial guess at the lopix and hipix settings */
static void
sf_newLOHI()
{
	switch (lowhi) {
	case LH_GAMMA:
	    lopix = loweredge();
	    hipix = h_maxi;
	    break;

	case LH_HEQ:
	    lopix = loweredge();
	    hipix = h_maxi;
	    break;
	}
}

/* create, but do not manage, the fits dialog and all its supporting dialogs */
static void
sf_create()
{
	Widget tf_w, bf_w;
	Widget heq_w, g_w, rc_w, rb_w;
	Widget go_w;
	Widget inv_w;
	Widget p_w, fr_w;
	Widget n_w, w_w;
	Widget h_w;
	Widget w;
	EventMask mask;
	Arg args[20];
	int n;

	/* create form */
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNallowResize, True); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	XtSetArg (args[n], XmNmarginWidth, 5); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	sf_w = XmCreateFormDialog (svshell_w, "SkyFITS", args, n);
	set_something (sf_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (sf_w, XmNhelpCallback, sf_help_cb, NULL);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Sky FITS"); n++;
	XtSetValues (XtParent(sf_w), args, n);

	/* top and bottom halves are in their own forms, then
	 * each form in a paned window
	 */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	p_w = XmCreatePanedWindow (sf_w, "FITSPW", args, n);
	XtManageChild (p_w);

	/* the top form */

	n = 0;
	tf_w = XmCreateForm (p_w, "TF", args, n);
	XtManageChild (tf_w);

	    /* controls to fetch networked images */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNtopOffset, 6); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    go_w = XmCreatePushButton (tf_w, "Get", args, n);
	    wtip (go_w, "Retrieve image of Sky View center over Internet");
	    XtAddCallback (go_w, XmNactivateCallback, sf_go_cb, NULL);
	    XtManageChild (go_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNtopOffset, 8); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNleftWidget, go_w); n++;
	    XtSetArg (args[n], XmNleftOffset, 4); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    w = XmCreateLabel (tf_w, "GL", args, n);
	    set_xmstring (w,XmNlabelString,"Digitized Sky Survey image ...");
	    XtManageChild (w);

	    /* institution selection */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, go_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 3); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 5); n++;
	    w = XmCreateLabel (tf_w, "From:", args, n);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, go_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 1); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 25); n++;
	    XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	    XtSetArg (args[n], XmNspacing, 10); n++;
	    XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	    rb_w = XmCreateRadioBox (tf_w, "GRB", args, n);
	    XtManageChild (rb_w);

		n = 0;
		XtSetArg (args[n], XmNspacing, 4); n++;
		stsci_w = XmCreateToggleButton (rb_w, "STScI", args, n);
		wtip (stsci_w, "Get image from Maryland USA");
		XtManageChild (stsci_w);
		sr_reg (stsci_w, NULL, skyfitscategory, 1);

		/* stsci sets logic */
		n = 0;
		XtSetArg (args[n], XmNspacing, 4); n++;
		XtSetArg(args[n],XmNset,!XmToggleButtonGetState(stsci_w)); n++;
		w = XmCreateToggleButton (rb_w, "ESO", args, n);
		wtip (stsci_w, "Get image from Germany");
		XtManageChild (w);

	    /* survey selection */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, rb_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 3); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 5); n++;
	    w = XmCreateLabel (tf_w, "Survey:", args, n);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, rb_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 1); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 25); n++;
	    XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	    XtSetArg (args[n], XmNspacing, 10); n++;
	    XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	    rb_w = XmCreateRadioBox (tf_w, "GRB", args, n);
	    XtManageChild (rb_w);

		n = 0;
		XtSetArg (args[n], XmNspacing, 4); n++;
		dss1_w = XmCreateToggleButton (rb_w, "DSS1", args, n);
		set_xmstring (dss1_w, XmNlabelString, "DSS 1");
		wtip (dss1_w, "Original DSS");
		XtManageChild (dss1_w);
		sr_reg (dss1_w, NULL, skyfitscategory, 1);

		n = 0;
		XtSetArg (args[n], XmNspacing, 4); n++;
		dss2r_w = XmCreateToggleButton (rb_w, "DSS2R", args, n);
		set_xmstring (dss2r_w, XmNlabelString, "DSS 2 Red");
		wtip (dss2r_w, "DSS 2, Red band (90% complete)");
		XtManageChild (dss2r_w);
		sr_reg (dss2r_w, NULL, skyfitscategory, 1);

		n = 0;
		XtSetArg (args[n], XmNspacing, 4); n++;
		dss2b_w = XmCreateToggleButton (rb_w, "DSS2B", args, n);
		set_xmstring (dss2b_w, XmNlabelString, "DSS 2 Blue");
		wtip (dss2b_w, "DSS 2, Blue band (50% complete)");
		XtManageChild (dss2b_w);
		sr_reg (dss2b_w, NULL, skyfitscategory, 1);

	    /* compressed? */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, rb_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 1); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 5); n++;
	    XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
	    XtSetArg (args[n], XmNindicatorOn, True); n++;
	    XtSetArg (args[n], XmNspacing, 4); n++;
	    gunzip_w = XmCreateToggleButton (tf_w, "UseGzip", args, n);
	    wtip(gunzip_w,"Retrieve compressed then uncompress using `gunzip'");
	    set_xmstring (gunzip_w, XmNlabelString,
				    "Compressed (recommended if have gunzip)");
	    XtManageChild (gunzip_w);
	    sr_reg (gunzip_w, NULL, skyfitscategory, 1);

	    /* header, with possible date */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, gunzip_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 10); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    h_w = XmCreateLabel (tf_w, "Lab", args, n);
	    set_xmstring (h_w, XmNlabelString, "FITS Header:");
	    XtManageChild (h_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, gunzip_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 10); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    setobsdate_w = XmCreatePushButton (tf_w, "SO", args, n);
	    set_xmstring (setobsdate_w, XmNlabelString, "Set time");
	    XtAddCallback (setobsdate_w, XmNactivateCallback, sf_setdate_cb, 0);
	    wtip(setobsdate_w,"Set main XEphem time to this Observation time");
	    XtManageChild (setobsdate_w);
	    XtSetSensitive (setobsdate_w, False); /* set true when have date */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, gunzip_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 10); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNleftWidget, h_w); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNrightWidget, setobsdate_w); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    obsdate_w = XmCreateLabel (tf_w, "ObsDate", args, n);
	    set_xmstring (obsdate_w, XmNlabelString, " ");
	    wtip(obsdate_w, "Best-guess of time of Observation");
	    XtManageChild (obsdate_w);

	    /* scrolled text in which to display the header */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, setobsdate_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 2); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomOffset, 10); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNautoShowCursorPosition, False); n++;
	    XtSetArg (args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	    XtSetArg (args[n], XmNeditable, False); n++;
	    XtSetArg (args[n], XmNcursorPositionVisible, False); n++;
	    hdr_w = XmCreateScrolledText (tf_w, "Header", args, n);
	    wtip (hdr_w, "Scrolled text area containing FITS File header");
	    XtManageChild (hdr_w);

	/* the bottom form */

	n = 0;
	bf_w = XmCreateForm (p_w, "BF", args, n);
	XtManageChild (bf_w);

	    /* label and support for contrast map */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNtopOffset, 16); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    n_w = XmCreatePushButton (bf_w, "Narrow", args, n);
	    XtAddCallback (n_w, XmNactivateCallback, sf_narrow_cb, 0);
	    wtip (n_w, "Concentrate gray scale on peak");
	    XtManageChild (n_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNtopOffset, 16); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNrightWidget, n_w); n++;
	    XtSetArg (args[n], XmNrightOffset, 10); n++;
	    w_w = XmCreatePushButton (bf_w, "Wide", args, n);
	    XtAddCallback (w_w, XmNactivateCallback, sf_wide_cb, 0);
	    wtip (w_w, "Set gray scale wide open");
	    XtManageChild (w_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNtopOffset, 16); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNrightWidget, w_w); n++;
	    XtSetArg (args[n], XmNrightOffset, 15); n++;
	    inv_w = XmCreateToggleButton (bf_w, "Inverse", args, n);
	    XtAddCallback (inv_w, XmNvalueChangedCallback, sf_inv_cb, 0);
	    wtip (inv_w, "When pushed in, image is displayed black-on-white");
	    XtManageChild (inv_w);
	    want_inv = XmToggleButtonGetState (inv_w);
	    sr_reg (inv_w, NULL, skyfitscategory, 1);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNtopOffset, 16); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNrightWidget, inv_w); n++;
	    XtSetArg (args[n], XmNrightOffset, 15); n++;
	    live_w = XmCreateToggleButton (bf_w, "Live", args, n);
	    set_xmstring (live_w, XmNlabelString, "Live dragging");
	    wtip (live_w,
		"When pushed in, image is updated while controls are moved");
	    XtManageChild (live_w);
	    sr_reg (live_w, NULL, skyfitscategory, 1);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNtopOffset, 16); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNrightWidget, live_w); n++;
	    XtSetArg (args[n], XmNrightOffset, 15); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    w = XmCreateLabel (bf_w, "GML", args, n);
	    set_xmstring (w, XmNlabelString, "Gray scale:");
	    XtManageChild (w);

	    /* scale methods in a RB */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, n_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 14); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	    rb_w = XmCreateRadioBox (bf_w, "GrayMethod", args, n);
	    XtManageChild (rb_w);

		n = 0;
		heq_w = XmCreateToggleButton (rb_w, "HEq", args, n);
		XtAddCallback (heq_w, XmNvalueChangedCallback, sf_gmlohi_cb,
							    (XtPointer)LH_HEQ);
		set_xmstring (heq_w, XmNlabelString, "Hist Eq");
		wtip (heq_w,"Assign equal number of pixels to each gray level");
		XtManageChild (heq_w);
		sr_reg (heq_w, NULL, skyfitscategory, 1);

		/* "HEq" sets logic state */
		n = 0;
		XtSetArg (args[n], XmNset, !XmToggleButtonGetState(heq_w)); n++;
		g_w = XmCreateToggleButton (rb_w, "Gamma", args, n);
		XtAddCallback (g_w, XmNvalueChangedCallback, sf_gmlohi_cb,
							(XtPointer)LH_GAMMA);
		set_xmstring (g_w, XmNlabelString, "Gamma");
		wtip (g_w, "Gray scale based on (pixel)^Gamma.");
		XtManageChild (g_w);

		/* set the lowhi enum to match */
		lowhi = XmToggleButtonGetState(heq_w) ? LH_HEQ : LH_GAMMA;

	    /* gamma scale between rb and right edge */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, n_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 6); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNleftWidget, rb_w); n++;
	    XtSetArg (args[n], XmNleftOffset, 6); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	    XtSetArg (args[n], XmNminimum, (int)(1./GAM_MAX*100)); n++;
	    XtSetArg (args[n], XmNmaximum, (int)(GAM_MAX*100)); n++;
	    XtSetArg (args[n], XmNscaleMultiple, 20); n++;
	    XtSetArg (args[n], XmNdecimalPoints, 2); n++;
	    XtSetArg (args[n], XmNshowValue, True); n++;
	    XtSetArg (args[n], XmNsensitive, lowhi == LH_GAMMA); n++;
	    gamma_w = XmCreateScale (bf_w, "GammaScale", args, n);
	    XtAddCallback (gamma_w, XmNdragCallback, sf_gamma_cb, NULL);
	    XtAddCallback (gamma_w, XmNvalueChangedCallback, sf_gamma_cb, NULL);
	    wtip (gamma_w, "Select new Gamma factor");
	    XtManageChild (gamma_w);
	    sr_reg (gamma_w, NULL, skyfitscategory, 1);

	    /* drawing area in a frame for the contrast map */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, gamma_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 6); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    fr_w = XmCreateFrame (bf_w, "HF", args, n);
	    XtManageChild (fr_w);

		n = 0;
		fda_w = XmCreateDrawingArea (fr_w, "Histogram", args, n);
		XtAddCallback (fda_w, XmNexposeCallback, sf_exp_cb, NULL);
		mask = Button1MotionMask | ButtonPressMask | ButtonReleaseMask
						       | PointerMotionHintMask;
		XtAddEventHandler (fda_w, mask, False, sf_motion_eh, NULL);
		XtManageChild (fda_w);
		wtip (fda_w,
		    "Histogram and colormap.. slide points to change limits");

	    /* label, go PB, Auto name TB and TF for saving a file */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, fr_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 16); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    w = XmCreateLabel (bf_w, "Save", args, n);
	    set_xmstring (w, XmNlabelString, "Save as FITS file:");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, fr_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 16); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 45); n++;
	    w = XmCreatePushButton (bf_w, "Save", args, n);
	    XtAddCallback (w, XmNactivateCallback, sf_save_cb, NULL);
	    wtip (w, "Save the current image to the file named below");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, fr_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 16); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    autoname_w = XmCreateToggleButton (bf_w, "AutoName", args, n);
	    set_xmstring (autoname_w, XmNlabelString, "Auto name");
	    XtManageChild (autoname_w);
	    wtip (autoname_w, "When on, automatically chooses a filename based on RA and Dec");
	    sr_reg (autoname_w, NULL, skyfitscategory, 1);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, autoname_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 6); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    savefn_w = XmCreateTextField (bf_w, "SaveFN", args, n);
	    defaultTextFN (savefn_w, 0, getPrivateDir(), "xxx.fts");
	    XtAddCallback (savefn_w, XmNactivateCallback, sf_save_cb, NULL);
	    wtip (savefn_w, "Enter name of file to write, then press Enter");
	    XtManageChild (savefn_w);

	    /* the Open FSB */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, savefn_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 16); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    w = XmCreateLabel (bf_w, "Lab", args, n);
	    set_xmstring (w, XmNlabelString, "Open a FITS file:");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, savefn_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 16); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	    XtSetArg (args[n], XmNspacing, 5); n++;
	    rc_w = XmCreateRowColumn (bf_w, "USRB", args, n);
	    XtManageChild (rc_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    /* t00fri: keeps FILE scrolled list width correct */
            XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	    fsb_w = XmCreateFileSelectionBox (bf_w, "FSB", args, n);
	    XtManageChild (fsb_w);
	    initFSB(fsb_w);
	    initPubShared (rc_w, fsb_w);

	/* get a grayscale ramp to use */
	ngray = gray_ramp (XtD, xe_cm, &gray);
}

/* init the directory and pattern resources of the given FileSelectionBox.
 * we try to pull these from the basic program resources.
 */
static void
initFSB (fsb_w)
Widget fsb_w;
{
	Widget w;

	/* set default dir and pattern */
	set_xmstring (fsb_w, XmNdirectory, getPrivateDir());
	set_xmstring (fsb_w, XmNpattern, getXRes (fitsp, "*.f*t*"));

	/* change some button labels.
	 * N.B. can't add tips because these are really Gadgets.
	 */
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_OK_BUTTON);
	set_xmstring (w, XmNlabelString, "Open");
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_CANCEL_BUTTON);
	set_xmstring (w, XmNlabelString, "Close");

	/* some other tips */
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_FILTER_TEXT);
	wtip (w, "Current directory and pattern; press `Filter' to rescan");
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_TEXT);
	wtip (w, "FITS file name to be read if press `Open'");

	/* connect an Open handler */
	XtAddCallback (fsb_w, XmNokCallback, sf_open_cb, NULL);

	/* connect a Close handler */
	XtAddCallback (fsb_w, XmNcancelCallback, sf_close_cb, NULL);

	/* connect a Help handler */
	XtAddCallback (fsb_w, XmNhelpCallback, sf_help_cb, NULL);
}

/* callback from the Public dir PB */
/* ARGSUSED */
static void
sharedDirCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Widget fsb_w = (Widget)client;
	char buf[1024];

	(void) sprintf (buf, "%s/fits", getShareDir());
	set_xmstring (fsb_w, XmNdirectory, expand_home(buf));
}

/* callback from the Private dir PB */
/* ARGSUSED */
static void
privateDirCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Widget fsb_w = (Widget)client;

	set_xmstring (fsb_w, XmNdirectory, getPrivateDir());
}

/* build a set of PB in RC rc_w so that they
 * set the XmNdirectory resource in the FSB fsb_w and invoke the Filter.
 */
static void
initPubShared (rc_w, fsb_w)
Widget rc_w, fsb_w;
{
	Arg args[20];
	char tip[1024];
	Widget w;
	int n;

	n = 0;
	w = XmCreateLabel (rc_w, "Dir", args, n);
	set_xmstring (w, XmNlabelString, "Go to:");
	XtManageChild (w);

	n = 0;
	w = XmCreatePushButton (rc_w, "Private", args, n);
	XtAddCallback(w, XmNactivateCallback, privateDirCB, (XtPointer)fsb_w);
	sprintf (tip, "Set directory to %s", getPrivateDir());
	wtip (w, XtNewString(tip));
	XtManageChild (w);

	n = 0;
	w = XmCreatePushButton (rc_w, "Shared", args, n);
	XtAddCallback(w, XmNactivateCallback, sharedDirCB, (XtPointer)fsb_w);
	sprintf (tip, "Set directory to %s/fits", getShareDir());
	wtip (w, XtNewString(tip));
	XtManageChild (w);
}

/* called when Get PB or toolbar PB is hit */
/* ARGSUSED */
void
sf_go_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!sf_w)
	    sf_create();

	if (readId) {
	    xe_msg ("DSS download is already in progress.", 1);
	    return;
	}

	if (XmToggleButtonGetState(stsci_w))
	    stsci_fits();
	else
	    eso_fits();
}

/* called when CR is hit in the Save text field or the Save PB is hit */
/* ARGSUSED */
static void
sf_save_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char *fn;

	if (!fimok) {
	    xe_msg ("No current FITS file to save", 1);
	    return;
	}

	fn = XmTextFieldGetString (savefn_w);
	if (!fn || (int)strlen(fn) < 1) {
	    xe_msg ("Please specify a filename", 1);
	    XtFree (fn);
	    return;
	}

	if (existsh (fn) == 0 && confirm()) {
	    char buf[1024];
	    (void) sprintf (buf, "%s exists: Ok to Overwrite?", fn);
	    query (toplevel_w, buf, "Yes -- Overwrite", "No -- Cancel",
						NULL, save_file, NULL, NULL);
	} else
	    save_file();

	XtFree (fn);
}

/* save to file named in savefn_w.
 * we already know everything is ok to just do it now.
 */
static void
save_file()
{
	char buf[1024];
	char *fn;
	int fd;

	fn = XmTextFieldGetString (savefn_w);

	fd = openh (fn, O_CREAT|O_WRONLY, 0666);
	if (fd < 0) {
	    (void) sprintf (buf, "%s: %s", fn, syserrstr());
	    xe_msg (buf, 1);
	    XtFree (fn);
	    return;
	}

	if (writeFITS (fd, &fim, buf, 1) < 0) {
	    char buf2[1024];
	    (void) sprintf (buf2, "%s: %s", fn, buf);
	    xe_msg (buf2, 1);
	} else {
	    (void) sprintf (buf, "%s:\nwritten successfully", fn);
	    xe_msg (buf, confirm());
	}

	(void) close (fd);
	XtFree (fn);
}

/* called when a file selected by the FSB is to be opened */
static void
/* ARGSUSED */
sf_open_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmFileSelectionBoxCallbackStruct *s =
				    (XmFileSelectionBoxCallbackStruct *)call;
	char *sp;

	if (s->reason != XmCR_OK) {
	    printf ("%s: Unknown reason = 0x%x\n", "sf_open_cb()", s->reason);
	    exit (1);
	}

	watch_cursor(1);

	XmStringGetLtoR (s->value, XmSTRING_DEFAULT_CHARSET, &sp);
	if (sf_readFile (sp) == 0) {
	    /* file opened ok. commit */
	    sf_newImage();

	    /* copy name to save buffer if autoname is on */
	    if (XmToggleButtonGetState(autoname_w)) {
		/* use last component of FSB filename */
		int n = strlen(sp);
		int i;

		for (i = n-1; i >= 0 && sp[i] != '/'; --i)
		    continue;
		sf_setSaveName (&sp[i+1], 1);
	    }
	}

	XtFree (sp);

	watch_cursor(0);
}

/* just read a fits image into fim -- do everything necessary to display it */
static void
sf_newImage()
{
	watch_cursor(1);

	/* put the fits fields into the scrolled text */
	sf_showHeader();

	/* build the histogram */
	build_histo();

	/* pick a good initial lopix/hipix */
	sf_newLOHI();

	/* assign the colors */
	build_colormap();

	/* draw the histogram */
	sf_drawHistogram();

	/* tell skyview to redraw */
	sv_newfits();

	watch_cursor(0);
}

/* ARGSUSED */
static void
sf_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (sf_w);
}

/* ARGSUSED */
static void
sf_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg[] = {
"Read in local FITS files or read from Network.",
"Resulting image will be displayed in Sky View."
};

	hlp_dialog ("Sky FITS", msg, sizeof(msg)/sizeof(msg[0]));
}

/* button motion and press/release event handler for the gray map */
/* ARGSUSED */
static void
sf_motion_eh (w, client, ev, continue_to_dispatch)
Widget w;
XtPointer client;
XEvent *ev;
Boolean *continue_to_dispatch;
{
	static int moving_lopix;
	Display *dsp = XtDisplay(w);
	Window win = XtWindow(w);
	Dimension wid, hei;
	Window root, child;
	int rx, ry, x, y;
	unsigned mask;
	int evt = ev->type;
	int m1, b1p, b1r;
	int newpix;

	/* do nothing if no current image */
	if (!fimok)
	    return;

	/* what happened? */
	b1p = evt == ButtonPress   && ev->xbutton.button == Button1;
	b1r = evt == ButtonRelease && ev->xbutton.button == Button1;
	m1  = evt == MotionNotify  && ev->xmotion.state  == Button1Mask;

	/* ignore everything else */
	if (!(b1p || b1r || m1))
	    return;

	watch_cursor(1);

	/* where are we? */
	XQueryPointer (dsp, win, &root, &child, &rx, &ry, &x, &y, &mask);
	get_something (w, XmNwidth, (XtArgVal)&wid);
	get_something (w, XmNheight, (XtArgVal)&hei);
	if (x < 0)         x = 0;
	if (x >= (int)wid) x = wid-1;
	if (y < 0)         y = 0;
	if (y >= (int)hei) y = hei-1;

	/* scale x to pixel value */
	newpix = h_mini + x*(h_maxi - h_mini)/(int)wid;

	/* if button was just pressed, choose which end to track */
	if (b1p)
	    moving_lopix = newpix < (lopix + hipix)/2;

	/* track the current end -- but never cross over */
	if (moving_lopix && newpix < hipix)
	    lopix = newpix;
	else if (!moving_lopix && newpix > lopix)
	    hipix = newpix;

	/* rebuild colormap and redraw histogram to show new markers */
	build_colormap();
	sf_drawHistogram();

	/* display net result if dragging else just when release */
	if (XmToggleButtonGetState(live_w) || b1r)
	    sv_newfits();

	watch_cursor(0);
}

/* expose callback for the gray map */
/* ARGSUSED */
static void
sf_exp_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	Display *dsp = XtDisplay(fda_w);
	Window win = XtWindow(fda_w);

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
	    printf ("Unexpected fda_w event. type=%d\n", c->reason);
	    exit(1);
	}

	sf_drawHistogram();
}

/* callback from the lohi TBs.
 * client is one of LowHi enum.
 */
/* ARGSUSED */
static void
sf_gmlohi_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* save new setting, but only from the one coming on */
	if (!XmToggleButtonGetState(w))
	    return;
	lowhi = (LowHi)(client);

	/* gamma scale useable only when using gamma option */
	XtSetSensitive (gamma_w, lowhi == LH_GAMMA);

	/* nothing more if no image */
	if (!fimok)
	    return;

	/* reassign the colors */
	build_colormap();

	/* draw the histogram */
	sf_drawHistogram();

	/* tell skyview to redraw */
	sv_newfits();
}

/* callback to open the gray scale quite wide */
/* ARGSUSED */
static void
sf_wide_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!fimok)
	    return;

	lopix = loweredge();
	hipix = h_maxi;

	/* reassign the colors */
	build_colormap();

	/* redraw histogram to show new markers */
	sf_drawHistogram();

	/* tell skyview to redraw */
	sv_newfits();
}

/* callback to set main time to match FITS */
/* ARGSUSED */
static void
sf_setdate_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	double newmjd;

	if (fitsObs(&newmjd) == 0)
	    mm_newcaldate(newmjd);
}

/* callback to narrow the gray scale */
/* ARGSUSED */
static void
sf_narrow_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!fimok)
	    return;

	lopix = (h_mini + h_peaki)/2;
	hipix = h_peaki + (h_maxi - h_peaki)/5;

	/* reassign the colors */
	build_colormap();

	/* redraw histogram to show new markers */
	sf_drawHistogram();

	/* tell skyview to redraw */
	sv_newfits();
}

/* callback from Drag or ValueChanged on the gamma scale */
/* ARGSUSED */
static void
sf_gamma_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        XmScaleCallbackStruct *s = (XmScaleCallbackStruct *)call;

	if (!fimok)
	    return;

	watch_cursor(1);

	build_colormap();
	sf_drawHistogram();
	if (XmToggleButtonGetState(live_w) || s->reason == XmCR_VALUE_CHANGED)
	    sv_newfits();

	watch_cursor(0);
}


/* callback for the gray map Inverse Vid TB */
/* ARGSUSED */
static void
sf_inv_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	want_inv = XmToggleButtonGetState(w);

	if (!fimok)
	    return;

	sv_newfits();
}

/* return the lowest pixel value which accounts for the first percent of all
 * pixels.
 */
static int
loweredge()
{
	int npix = fim.sw * fim.sh;
	int i, sum = 0;

	for (i = 0; i < MAXCAMPIX; i++) {
	    sum += histo[i];
	    if (sum > npix/100)
		break;
	}

	return (i);
}

/* draw histgram, if any, in the drawing area, and the lo/hi markers.
 * we stretch and only show from h_mini .. h_maxi
 * N.B. we might be called before the first expose
 */
static void
sf_drawHistogram()
{
#define	MAXPTS	50
	static GC gc;
	static Pixel ip, hp, mp;
	Display *dsp = XtDisplay(fda_w);
	Window win = XtWindow(fda_w);
	int h_peak;
	XPoint pts[MAXPTS];
	Dimension wid, hei;
	int np;
	int x;

	/* might not be far enough */
	if (!win || !histo)
	    return;
	h_peak = histo[h_peaki];

	/* clear */
	XClearArea (dsp, win, 0, 0, 0, 0, False);

	/* nothing to do if no image yet */
	if (!fimok)
	    return;

	/* if first time, build a gc and gather some colors */
	if (!gc) {
	    gc = XCreateGC (dsp, win, 0L, NULL);
	    get_something (fda_w, XmNforeground, (XtArgVal)&ip);
	    if (get_color_resource (fda_w, hxr, &hp) < 0)
		hp = WhitePixel(dsp,DefaultScreen(dsp));
	    if (get_color_resource (fda_w, mxr, &mp) < 0)
		mp = WhitePixel(dsp,DefaultScreen(dsp));
	}
	    

	/* get window size */
	get_something (fda_w, XmNwidth, (XtArgVal)&wid);
	get_something (fda_w, XmNheight, (XtArgVal)&hei);

	/* draw the histogram all across each x of the window */
	XSetForeground (dsp, gc, hp);
	for (np = x = 0; x < (int)wid; x++) {
	    int i, p, y;
	    int l, h;

	    /* find lo and hi pixel values binned into this x */
	    l = h_mini + x*(h_maxi - h_mini)/(int)wid;
	    h = h_mini + (x+1)*(h_maxi - h_mini)/(int)wid;

	    /* find peak in histogram portion shown at this x */
	    p = 0;
	    h = h < MAXCAMPIX ? h : MAXCAMPIX;
	    for (i = l; i < h; i++)
		if (histo[i] > p)
		    p = histo[i];

	    /* scale to window y value */
	    y = (int)(hei-1) - p*(int)(hei-1)/h_peak;

	    /* add another line segment if its y changes */
	    if (np == 0 || y != pts[np-1].y || x == wid-1) {
		XPoint *xp = &pts[np++];
		xp->x = x;
		xp->y = y;
	    }
	    if (np == MAXPTS) {
		XDrawLines (dsp, win, gc, pts, np, CoordModeOrigin);
		pts[0].x = pts[np-1].x;
		pts[0].y = pts[np-1].y;
		np = 1;
	    }
	}
	if (np > 1)
	    XDrawLines (dsp, win, gc, pts, np, CoordModeOrigin);

	/* then overlay with the colormap within the lo/hi */
	XSetForeground (dsp, gc, mp);
	for (np = x = 0; x < (int)wid; x++) {
	    int i, p, y;
	    int l, h;

	    /* find lo and hi pixel values binned into this x */
	    l = h_mini + x*(h_maxi - h_mini)/(int)wid;
	    h = h_mini + (x+1)*(h_maxi - h_mini)/(int)wid;

	    /* only draw inside the indicators */
	    if (h < lopix || l > hipix)
		continue;

	    /* find peak in colormap portion shown at this x */
	    p = 0;
	    h = h < MAXCAMPIX ? h : MAXCAMPIX;
	    for (i = l; i < h; i++)
		if (colormap[i] > p)
		    p = colormap[i];

	    /* scale to window y value */
	    y = (int)(hei-1) - p*(int)(hei-1)/(ngray-1);

	    /* add another line segment if its y changes */
	    if (np == 0 || y != pts[np-1].y || x == wid-1) {
		XPoint *xp = &pts[np++];
		xp->x = x;
		xp->y = y;
	    }
	    if (np == MAXPTS) {
		XDrawLines (dsp, win, gc, pts, np, CoordModeOrigin);
		pts[0].x = pts[np-1].x;
		pts[0].y = pts[np-1].y;
		np = 1;
	    }
	}
	if (np > 1)
	    XDrawLines (dsp, win, gc, pts, np, CoordModeOrigin);

	/* add the indicators */
	XSetForeground (dsp, gc, ip);
	x = (lopix - h_mini)*(int)wid/(h_maxi - h_mini);
	pts[0].x = x-MMW; pts[0].y = 0;
	pts[1].x = x;     pts[1].y = MMH;
	pts[2].x = x+MMW; pts[2].y = 0;
	XFillPolygon (dsp, win, gc, pts, 3, Convex, CoordModeOrigin);

	x = (hipix - h_mini)*(int)wid/(h_maxi - h_mini);
	pts[0].x = x-MMW; pts[0].y = 0;
	pts[1].x = x;     pts[1].y = MMH;
	pts[2].x = x+MMW; pts[2].y = 0;
	XFillPolygon (dsp, win, gc, pts, 3, Convex, CoordModeOrigin);
}

/* fill the hdr_w scrolled text with the FITS header entries.
 * keep hdrl up to date.
 */
static void
sf_showHeader ()
{
	char *header;
	int i;

	/* room for each FITS line, with nl and a final END and \0 */
	header = malloc ((fim.nvar+1)*(FITS_HCOLS+1) + 1);
	if (!header) {
	    xe_msg ("No memory to display FITS header", 0);
	    return;
	}

	/* copy from fim.var to header, adding \n after each line */
	for (i = 0; i < fim.nvar; i++) {
	    memcpy(header + i*(FITS_HCOLS+1), fim.var[i], FITS_HCOLS);
	    header[(i+1)*(FITS_HCOLS+1)-1] = '\n';
	}
	
	/* add END and '\0' to make it a real string */
	(void) sprintf (&header[i*(FITS_HCOLS+1)], "END");

	XmTextSetString (hdr_w, header);
	free (header);

	/* scroll to the top */
	XmTextShowPosition (hdr_w, (XmTextPosition)0);
}

/* set end of savefn_w based on newfn.
 * if usesuf, use the suffix on newfn, else preserve any already on savefn.
 */
static void
sf_setSaveName(newfn, usesuf)
char *newfn;
int usesuf;
{
	char buf[1024];
	char *fn;
	char *s;
	char *f, *l;

	fn = XmTextFieldGetString (savefn_w);

	/* set f to where to begin: right-most / or beginning if none */
	for (f = s = fn; *s != '\0'; s++)
	    if (*s == '/')
		f = s+1;

	(void) sprintf (buf, "%.*s%s", f-fn, fn, newfn);

	if (!usesuf) {
	    /* preserve suffix already in savefn. */
	    for (l = s; s >= f; --s)
		if (*s == '.') {
		    l = s;
		    break;
		}

	    (void) strcat (buf, *l == '.' ? l : ".fts");
	}

	XtFree (fn);

	XmTextFieldSetString (savefn_w, buf);
}

/* make glass_xim of size GLASSSZ*GLASSMAG and same genre as fim.
 * leave glass_xim NULL if trouble.
 */
static void
makeGlassImage (dsp)
Display *dsp;
{
	int nbytes = (GLASSSZ*GLASSMAG+7) * (GLASSSZ*GLASSMAG+7) * fbpp/8;
	char *glasspix = (char *) malloc (nbytes);

	if (!glasspix) {
	    char msg[1024];
	    (void) sprintf (msg, "Can not malloc %d for Glass pixels", nbytes);
	    xe_msg (msg, 0);
	    return;
	}

	glass_xim = XCreateImage (dsp, XDefaultVisual (dsp, DefaultScreen(dsp)),
	    /* depth */         fbpp == 1 ? 1 : fdepth,
	    /* format */        fbpp == 1 ? XYBitmap : ZPixmap,
	    /* offset */        0,
	    /* data */          glasspix,
	    /* width */         GLASSSZ*GLASSMAG,
	    /* height */        GLASSSZ*GLASSMAG,
	    /* pad */           fbpp < 8 ? 8 : fbpp,
	    /* bpl */           0);

	if (!glass_xim) {
	    free ((void *)glasspix);
	    xe_msg ("Can not make Glass XImage", 0);
	    return;
	}

        glass_xim->bitmap_bit_order = LSBFirst;
	glass_xim->byte_order = LSBFirst;
}

/* make glassGC */
static void
makeGlassGC (dsp, win)
Display *dsp;
Window win;
{
	XGCValues gcv;
	unsigned int gcm;
	Pixel p;

	if (get_color_resource (fda_w, "GlassBorderColor", &p) < 0) {
	    xe_msg ("Can not get GlassBorderColor -- using White", 0);
	    p = WhitePixel (dsp, 0);
	}
	gcm = GCForeground;
	gcv.foreground = p;
	glassGC = XCreateGC (dsp, win, gcm, &gcv);
}

/* fill glass_xim with GLASSSZ*GLASSMAG view of fim centered at coords
 * xc,yc. take care at the edges.
 */
static void
fillGlass (lr, tb, xc, yc)
int lr, tb;	/* flipping flags */
int xc, yc;	/* center of glass, in window coords */
{
	CamPixel *ip = (CamPixel *)(fim.image);
	int ncols = fim.sw;
	int nrows = fim.sh;
	int sx, sy;	/* coords in fim */
	int gx, gy;	/* coords in glass_xim */
	int gdx, gdy;	/* glass delta in each loop */

	if (lr) {
	    xc = ncols - xc;
	    gx = (GLASSSZ - 1) * GLASSMAG;
	    gdx = -GLASSMAG;
	} else {
	    gx = 0;
	    gdx = GLASSMAG;
	}
	if (tb) {
	    yc = nrows - yc;
	    gy = (GLASSSZ - 1) * GLASSMAG;
	    gdy = -GLASSMAG;
	} else {
	    gy = 0;
	    gdy = GLASSMAG;
	}
	    
	for (sy = yc-GLASSSZ/2; sy < yc+GLASSSZ/2; sy++) {
	    for (sx = xc-GLASSSZ/2; sx < xc+GLASSSZ/2; sx++) {
		int i, j;
		Pixel p;
		int v;

		if (sx < 0 || sx >= ncols || sy < 0 || sy >= nrows)
		    v = 0;
		else
		    v = ip[sy*ncols + sx];
		p = gray_pixel (v);

		for (i = 0; i < GLASSMAG; i++)
		    for (j = 0; j < GLASSMAG; j++)
			XPutPixel (glass_xim, gx+i, gy+j, p);

		gx += gdx;
	    }
	    gx = lr ? (GLASSSZ - 1) * GLASSMAG : 0;
	    gy += gdy;
	}
}

/* return 0 if find the string str in buf, else -1 */
static int
chk4str (str, buf)
char str[];
char buf[];
{
	int l = strlen (str);

	while (*buf != '\0')
	    if (strncmp (str, buf++, l) == 0)
		return (0);
	return (-1);
}

/* return 0 if have gunzip else -1 */
static int
chk_gunzip()
{
#define	NOGZEXIT	88		/* any impossible gzip exit value */
	static int know = 1;		/* 0 or -1 when know for sure */
	int wstatus;
	int pid;

	/* only really test once */
	if (know <= 0)
	    return (know);

	/* fork/exec and see how it exits */
	pid = fork();
	if (pid < 0)
	    return (know = -1);
	if (pid == 0) {
	    /* new process: exec gunzip else exit crazy */
	    int nullfd = open ("/dev/null", O_RDWR);
	    if (nullfd < 0)
		_exit(NOGZEXIT);
	    dup2 (nullfd, 0);
	    dup2 (nullfd, 1);
	    dup2 (nullfd, 2);
	    execlp (gcmd, gcmd, NULL);
	    /* does not return if works */
	    _exit(NOGZEXIT);
	}

	/* parent waits for exit status */
	know = (waitpid (pid, &wstatus, 0) == pid && WIFEXITED(wstatus)
				&& WEXITSTATUS(wstatus) != NOGZEXIT) ? 0 : -1;
	return (know);
}

/* return 1 if have/want to use gunzip, else 0 */
static int
use_gunzip()
{
	int want = XmToggleButtonGetState (gunzip_w);

	if (!want)
	    return (0);

	if (chk_gunzip() < 0) {
	    char msg[1024];
	    (void) sprintf (msg,
		    "Can not find %s.\nProceeding without compression", gcmd);
	    XmToggleButtonSetState (gunzip_w, False, False);
	    XtSetSensitive (gunzip_w, 0);
	    xe_msg (msg, 1);
	    return (0);
	}

	return (1);
}

/* setup the pipe between gunzip and xephem to decompress the data.
 * return pid if ok, else -1.
 */
static int
setup_gunzip_pipe(int sockfd)
{
	int gzfd[2];		/* file descriptors for gunzip pipe */
	int pid;
	int errfd;

	/* make the pipe to gunzip */
	if (pipe(gzfd) < 0) {
	    xe_msg ("Can not make pipe for gunzip", 1);
	    return (-1);
	}

	/* no zombies */
	signal (SIGCHLD, SIG_IGN);
  
	/* fork/exec gunzip */
	switch((pid = fork())) {
	case 0:			/* child: put gunzip between socket and us */
	    close (gzfd[0]);	/* do not need read side of pipe */
	    dup2 (sockfd, 0);	/* socket becomes gunzip's stdin */
	    close (sockfd);	/* do not need after dup */
	    dup2 (gzfd[1], 1);	/* write side of pipe becomes gunzip's stdout */
	    close (gzfd[1]);	/* do not need after dup */
	    errfd = open ("/dev/null", O_RDWR);
	    if (errfd >= 0) {
		dup2 (errfd, 2);/* dump gunzip's stderr */
		close (errfd);
	    }
	    execlp (gcmd, gcmd, "-c", NULL);
	    _exit (1);		/* exit in case gunzip disappeared */
	    break;		/* :) */

	case -1:	/* fork failed */
	    xe_msg ("Can not fork for gunzip", 1);
	    return (-1);

	default:	/* parent */
	    break;
	}
	
	/* put gunzip between the socket and us */
	close (gzfd[1]);	/* do not need write side of pipe */
	dup2 (gzfd[0], sockfd);	/* read side of pipe masquarades as socket */
	close (gzfd[0]);	/* do not need after dup */

	/* return gunzip's pid */
	return (pid);
}

static Survey
whichSurvey()
{
	if (XmToggleButtonGetState(dss2r_w))
	    return (DSS_2R);
	if (XmToggleButtonGetState(dss2b_w))
	    return (DSS_2B);
	return (DSS_1);
}

/* start an input stream reading a FITS image from ESO */
static void
eso_fits()
{
	static char host[] = "archive.eso.org";
	static FImage sfim, *fip = &sfim;
	double fov, alt, az, ra, dec;
	char rastr[32], *rap, decstr[32], *decp;
	char buf[1024], msg[1024];
	char *survey;
	int gzpid;
	int sockfd;
	int aamode;
	int sawfits;

	/* do not turn off watch until completely finished */
	watch_cursor (1);

	/* let user abort */
	stopd_up();

	/* init fip (not reset because we copy the malloc'd fields to fim) */
	initFImage (fip);

	/* find current skyview center and size, in degrees */
	sv_getcenter (&aamode, &fov, &alt, &az, &ra, &dec);
	fov = 60*raddeg(fov);
	ra = radhr (ra);
	dec = raddeg (dec);

	if (fov > MAXDSSFOV)
	    fov = MAXDSSFOV;
	if (fov < MINDSSFOV)
	    fov = MINDSSFOV;

	/* get desired survey */
	switch (whichSurvey()) {
	default:
	case DSS_1:  survey = "DSS1"; break;
	case DSS_2R: survey = "DSS2-red"; break;
	case DSS_2B: survey = "DSS2-blue"; break;
	}

	/* format and send the request.
	 * N.B. ESO can't tolerate leading blanks in ra dec specs
	 */
	fs_sexa (rastr, ra, 2, 3600);
	for (rap = rastr; *rap == ' '; rap++)
	    continue;
	fs_sexa (decstr, dec, 3, 3600);
	for (decp = decstr; *decp == ' '; decp++)
	    continue;
	(void) sprintf (buf, "/dss/dss?ra=%s&dec=%s&equinox=J2000&Sky-Survey=%s&mime-type=%s&x=%.0f&y=%.0f HTTP/1.0\r\nUser-Agent: xephem/%s\r\n\r\n",
						rap, decp, survey,
			use_gunzip() ? "display/gz-fits" : "application/x-fits",
						fov, fov, PATCHLEVEL);
	(void) sprintf (msg, "Command to %s:", host);
	xe_msg (msg, 0);
	xe_msg (buf, 0);
	sockfd = httpGET (host, buf, msg);
	if (sockfd < 0) {
	    xe_msg (msg, 1);
	    stopd_down();
	    watch_cursor (0);
	    return;
	}

	/* read back the header -- ends with a blank line */
	sawfits = 0;
	while (recvline (sockfd, buf, sizeof(buf)) > 1) {
	    xe_msg (buf, 0);
	    if (chk4str ("application/x-fits", buf) == 0
					 || chk4str ("image/x-fits", buf) == 0)
		sawfits = 1;
	}

	/* if do not see a fits file, show what we do find */
	if (!sawfits) {
	    xe_msg (" ", 0);
	    xe_msg ("Message from server", 1);
	    xe_msg ("------------------", 0);
	    while (recvline (sockfd, buf, sizeof(buf)) > 0)
		xe_msg (buf, 0);
	    xe_msg ("------------------", 0);
	    xe_msg ("End of Message from server", 0);
	    msg_manage();
	    watch_cursor (0);
	    close (sockfd);
	    stopd_down();
	    return;
	}

	/* possibly connect via gunzip -- weird if have gunzip but can't */
	if (use_gunzip()) {
	    gzpid = setup_gunzip_pipe(sockfd);
	    if (gzpid < 0) {
		watch_cursor (0);
		close (sockfd);
		stopd_down();
		return;
	    }
	} else
	    gzpid = -1;
		

	/* read the FITS header portion */
	if (readFITSHeader (sockfd, fip, buf) < 0) {
	    watch_cursor (0);
	    xe_msg (buf, 1);
	    resetFImage (fip);
	    close (sockfd);
	    if (gzpid > 0)
		kill (gzpid, SIGTERM);
	    stopd_down();
	    return;
	}

	/* see if WCS headers are ok (or can be derived) */
	if (wcsok (fip) < 0) {
	    watch_cursor (0);
	    resetFImage (fip);
	    close (sockfd);
	    if (gzpid > 0)
		kill (gzpid, SIGTERM);
	    stopd_down();
	    return;
	}

	/* ok, start reading the pixels and give user a way to abort */
	pm_up();	/* for your viewing pleasure */
	readId = XtAppAddInput (xe_app, sockfd, (XtPointer)XtInputReadMask,
						fits_read_cb, (XtPointer)fip);
}

/* start an input stream reading a FITS image from STScI */
static void
stsci_fits()
{
	static char host[] = "archive.stsci.edu";
	static FImage sfim, *fip = &sfim;
	double fov, alt, az, ra, dec;
	char buf[1024], msg[1024];
	char *survey;
	int gzpid;
	int sockfd;
	int aamode;
	int sawfits;

	/* do not turn off watch until completely finished */
	watch_cursor (1);

	/* let user abort */
	stopd_up();

	/* init fip (not reset because we copy the malloc'd fields to fim) */
	initFImage (fip);

	/* find current skyview center and size, in degrees */
	sv_getcenter (&aamode, &fov, &alt, &az, &ra, &dec);
	fov = 60*raddeg(fov);
	ra = raddeg (ra);
	dec = raddeg (dec);

	if (fov > MAXDSSFOV)
	    fov = MAXDSSFOV;
	if (fov < MINDSSFOV)
	    fov = MINDSSFOV;

	/* get desired survey */
	switch (whichSurvey()) {
	default:
	case DSS_1:  survey = "1"; break;
	case DSS_2R: survey = "2r"; break;
	case DSS_2B: survey = "2b"; break;
	}

	/* format and send the request */
	(void) sprintf(buf,"/cgi-bin/dss_search?ra=%.5f&dec=%.5f&equinox=J2000&v=%s&height=%.0f&width=%.0f&format=FITS&compression=%s&version=3 HTTP/1.0\nUser-Agent: xephem/%s\n\n",
						ra, dec, survey, fov, fov,
						use_gunzip() ? "gz" : "NONE",
						PATCHLEVEL);
	(void) sprintf (msg, "Command to %s:", host);
	xe_msg (msg, 0);
	xe_msg (buf, 0);
	sockfd = httpGET (host, buf, msg);
	if (sockfd < 0) {
	    xe_msg (msg, 1);
	    stopd_down();
	    watch_cursor (0);
	    return;
	}

	/* read back the header -- ends with a blank line */
	sawfits = 0;
	while (recvline (sockfd, buf, sizeof(buf)) > 1) {
	    xe_msg (buf, 0);
	    if (chk4str ("image/x-fits", buf) == 0)
		sawfits = 1;
	}

	/* if do not see a fits file, show what we do find */
	if (!sawfits) {
	    xe_msg (" ", 0);
	    xe_msg ("Message from server", 1);
	    xe_msg ("------------------", 0);
	    while (recvline (sockfd, buf, sizeof(buf)) > 0)
		xe_msg (buf, 0);
	    xe_msg ("------------------", 0);
	    xe_msg ("End of Message from server", 0);
	    msg_manage();
	    watch_cursor (0);
	    close (sockfd);
	    stopd_down();
	    return;
	}


	/* possibly connect via gunzip -- weird if have gunzip but can't */
	if (use_gunzip()) {
	    gzpid = setup_gunzip_pipe(sockfd);
	    if (gzpid < 0) {
		watch_cursor (0);
		close (sockfd);
		stopd_down();
		return;
	    }
	} else
	    gzpid = -1;

	/* read the FITS header portion */
	if (readFITSHeader (sockfd, fip, buf) < 0) {
	    watch_cursor (0);
	    xe_msg (buf, 1);
	    resetFImage (fip);
	    close (sockfd);
	    if (gzpid > 0)
		kill (gzpid, SIGTERM);
	    stopd_down();
	    return;
	}

	/* see if WCS headers are ok (or can be derived) */
	if (wcsok (fip) < 0) {
	    watch_cursor (0);
	    resetFImage (fip);
	    close (sockfd);
	    if (gzpid > 0)
		kill (gzpid, SIGTERM);
	    stopd_down();
	    return;
	}

	/* ok, start reading the pixels and give user a way to abort */
	pm_up();	/* for your viewing pleasure */
	readId = XtAppAddInput (xe_app, sockfd, (XtPointer)XtInputReadMask,
						fits_read_cb, (XtPointer)fip);
}

/* called whenever there is more data available on the sockfd.
 */
static void
fits_read_cb (client, fd, id)
XtPointer client;
int *fd;
XtInputId *id;
{
	FImage *fip = (FImage *)client;
	int sockfd = *fd;
	char buf[1024];

	/* read another chunk */
	if (readIncFITS (sockfd, fip, buf) < 0) {
	    xe_msg (buf, 1);
	    resetFImage (fip);
	    close (sockfd);
	    XtRemoveInput (readId);
	    readId = (XtInputId)0;
	    stopd_down();
	    pm_down();
	    watch_cursor (0);
	    return;
	}

	/* report progress */
	pm_set (fip->nbytes * 100 / fip->totbytes);
	XmUpdateDisplay (toplevel_w);

	/* keep going if expecting more */
	if (fip->nbytes < fip->totbytes)
	    return;

	/* finished reading */
	stopd_down();
	pm_down();
	close (sockfd);
	XtRemoveInput (readId);
	readId = (XtInputId)0;

	/* YES! */

	/* set save name from image center, if enabled */
	if (XmToggleButtonGetState(autoname_w)) {
	    int dneg, rh, rm, dd, dm;
	    double ra, dec;
	    char buf[64];

	    (void) xy2RADec (fip, fip->sw/2.0, fip->sh/2.0, &ra, &dec);
	    ra = radhr(ra);
	    dec = raddeg(dec);
	    if ((dneg = (dec < 0)))
		dec = -dec;
	    rh = (int)floor(ra);
	    rm = (int)floor((ra - rh)*60.0 + 0.5);
	    if (rm == 60) {
		if (++rh == 24)
		    rh = 0;
		rm = 0;
	    }
	    dd = (int)floor(dec);
	    dm = (int)floor((dec - dd)*60.0 + 0.5);
	    if (dm == 60) {
		dd++;
		dm = 0;
	    }
	    (void) sprintf (buf, "%02d%02d%c%02d%02d", rh, rm,
						    dneg ? '-' : '+', dd, dm);

	    sf_setSaveName (buf, 0);
	}

	/* commit and display */
	newfim (fip);
	sf_newImage();
	watch_cursor (0);
}

/* install fip as the new fim.
 * N.B. malloced fields in fip are just re-used, so do not reset it on return.
 */
static void
newfim (fip)
FImage *fip;
{
	double objsmjd;

	resetFImage (&fim);
	(void) memcpy ((void *)&fim, (void *)fip, sizeof(fim));
	fimok = 1;

	if (fitsObs(&objsmjd) == 0) {
	    int mm, yy, d, h, m, s;
	    double dd, dh, dm, ds;
	    char buf[128];

	    mjd_cal (objsmjd, &mm, &dd, &yy);
	    d = (int)dd;
	    dh = (dd - d)*24.;
	    h = (int)dh;
	    dm = (dh - h)*60.;
	    m = (int)dm;
	    ds = (dm - m)*60.;
	    if (ds > 59.5) {
		s = 0;
		if (++m == 60) {
		    m = 0;
		    h += 1; /* TODO: roll date if hits 24 */
		}
	    } else
		s = (int)ds;

	    sprintf (buf, "%d-%d-%d %02d:%02d:%02d", yy, mm, d, h, m, s);
	    set_xmstring (obsdate_w, XmNlabelString, buf);
	    XtSetSensitive (setobsdate_w, True);
	} else {
	    set_xmstring (obsdate_w, XmNlabelString, " ");
	    XtSetSensitive (setobsdate_w, False);
	}
}

/* check for the WCS headers.
 * if it looks like a DSS image, try to infer the fields.
 * return 0 if ok, else xe_msg() and return -1.
 */
static int
wcsok (fip)
FImage *fip;
{
	double ra, dec;

	/* if have DSS convert to WCS so we know we can go both ways.
	 * only dss RD->xy I can find solves from xy->RD YUK!
	 */
	if (getRealFITS (fip, "PLTRAH", &ra) == 0) {
	    if (dsswcs (fip) < 0)
		return (-1);
	}

	/* now try it */
	if (xy2RADec (fip, 0.0, 0.0, &ra, &dec) == 0)
	    return (0);

	/* oh well */
	xe_msg ("No WCS fields in header", 1);
	return (-1);
}

/* given a DSS image, build the WCS headers.
 * return 0 if ok, else issue xe_msg() and return -1.
 */
static int
dsswcs (fip)
FImage *fip;
{
	double a0, d0, a1, d1;
	double px, py;
	double pltscale;
	double rot;

	/* find RA and Dec at the center of our image */
	if (dssxy2RADec (fip, fip->sw/2.0, fip->sh/2.0, &a0, &d0) < 0)
	    return (-1);

	/* use center as reference */
	setRealFITS (fip, "CRPIX1", fip->sw/2.0, 10, NULL);
	setRealFITS (fip, "CRPIX2", fip->sh/2.0, 10, NULL);
	setRealFITS (fip, "CRVAL1", raddeg(a0), 10, NULL);
	setRealFITS (fip, "CRVAL2", raddeg(d0), 10, NULL);

	/* set scale */
	if (getRealFITS (fip, "PLTSCALE", &pltscale) < 0
			    || getRealFITS (fip, "XPIXELSZ", &px) < 0
			    || getRealFITS (fip, "YPIXELSZ", &py) < 0) {
	    xe_msg ("No Plate scale fields", 1);
	    return(-1);
	}
	setRealFITS (fip, "CDELT1", -pltscale*px/3600000.0, 10, NULL);
	setRealFITS (fip, "CDELT2",  pltscale*py/3600000.0, 10, NULL);

	/* coord sys */
	setStringFITS (fip, "CTYPE1", "RA---TAN", NULL);
	setStringFITS (fip, "CTYPE2", "DEC--TAN", NULL);

	/* find rotation CW to make N up.
	 * TODO: ugly! gotta better way??
	 */
	if (dssxy2RADec (fip, fip->sw/2.0, 0.0, &a1, &d1) < 0)
	    return (-1);
	if (d0 < 0) {
	    double A = a0-a1;
	    double b = PI/2+d0;
	    double c = PI/2+d1;
	    rot = raddeg(atan2(c*sin(A), c*cos(A)-b)) - 180;
	} else {
	    double A = a1-a0;
	    double b = PI/2-d0;
	    double c = PI/2-d1;
	    rot = raddeg(atan2(c*sin(A), c*cos(A)-b));
	}
	setRealFITS (fip, "CROTA2", rot, 10, "ADDED BY XEPHEM");

	return (0);
}

/* convert pixel coords to ra/dec, using DSS header fields.
 * return 0 if ok, else issue xe_msg() and return -1.
 * this is all based on the equations on pages 14-16 of The Digitized Sky
 * Survey release notes.
 */
static int
dssxy2RADec (fip, X, Y, rap, decp)
FImage *fip;
double X, Y;
double *rap, *decp;
{
	char buf[128];
	double cnpix1, cnpix2;
	double a[14], b[14];
	double rh, rm, rs;
	double dd, dm, ds;
	double xc, px, yc, py;
	double x, y, x2y2;
	double pltscale;
	double rac, decc, ra;
	double xi, nu;
	double tandecc;
	int i;

	if (getRealFITS (fip, "CNPIX1", &cnpix1) < 0
			    || getRealFITS (fip, "CNPIX2", &cnpix2) < 0) {
	    xe_msg ("No CNPIX fields", 1);
	    return(-1);
	}

	if (getRealFITS (fip, "PLTSCALE", &pltscale) < 0
			    || getRealFITS (fip, "PPO3", &xc) < 0
			    || getRealFITS (fip, "PPO6", &yc) < 0
			    || getRealFITS (fip, "XPIXELSZ", &px) < 0
			    || getRealFITS (fip, "YPIXELSZ", &py) < 0) {
	    xe_msg ("No Plate scale fields", 1);
	    return(-1);
	}

	X += cnpix1 - 0.5;
	Y += cnpix2 - 0.5;
	x = (xc - px*X)/1000.0;
	y = (py*Y - yc)/1000.0;

	for (i = 1; i <= 13; i++) {
	    char ax[32], ay[32];
	    (void) sprintf (ax, "AMDX%d", i);
	    (void) sprintf (ay, "AMDY%d", i);
	    if (getRealFITS (fip, ax, &a[i]) < 0 ||
					    getRealFITS (fip, ay, &b[i]) < 0) {
		xe_msg ("No AMD fields", 1);
		return (-1);
	    }
	}
	x2y2 = x*x + y*y;
	xi = a[1]*x + a[2]*y + a[3] + a[4]*x*x + a[5]*x*y + a[6]*y*y
		+ a[7]*x2y2 + a[8]*x*x*x + a[9]*x*x*y + a[10]*x*y*y
		+ a[11]*y*y*y + a[12]*x*x2y2 + a[13]*x*x2y2*x2y2;
	xi = degrad(xi/3600.0);
	nu = b[1]*y + b[2]*x + b[3] + b[4]*y*y + b[5]*x*y + b[6]*x*x
		+ b[7]*x2y2 + b[8]*y*y*y + b[9]*x*y*y + b[10]*x*x*y
		+ b[11]*x*x*x + b[12]*y*x2y2 + b[13]*y*x2y2*x2y2;
	nu = degrad(nu/3600.0);


	if (getRealFITS (fip, "PLTRAH", &rh) < 0
			    || getRealFITS (fip, "PLTRAM", &rm) < 0
			    || getRealFITS (fip, "PLTRAS", &rs) < 0
			    || getRealFITS (fip, "PLTDECD", &dd) < 0
			    || getRealFITS (fip, "PLTDECM", &dm) < 0
			    || getRealFITS (fip, "PLTDECS", &ds) < 0
			    || getStringFITS (fip, "PLTDECSN", buf) < 0) {
	    xe_msg ("No PLT* fields", 1);
	    return(-1);
	}
	rac = rs/3600.0 + rm/60.0 + rh;
	rac = hrrad(rac);
	decc = ds/3600.0 + dm/60.0 + dd;
	if (buf[0] == '-')
	    decc = -decc;
	decc = degrad(decc);
	tandecc = tan(decc);

	ra = atan2 (xi/cos(decc), 1.0-nu*tandecc) + rac;
	if (ra < 0)
	    ra += 2*PI;
	*rap = ra;
	*decp = atan (((nu + tandecc)*cos(ra-rac))/(1.0 - nu*tandecc));

	return (0);
}

/* poke around in the headers and try to find the mjd of the observation.
 * return 0 if think we found something, else -1
 */
static int
fitsObs(mjdp)
double *mjdp;
{
	FImage *fip = sf_getFImage();
	char buf[128];
	double x;

	if (!fip)
	    return (-1);

	if (getRealFITS (fip, "JD", &x) == 0) {
	    *mjdp = x - MJD0;
	    return (0);
	}

	if (getRealFITS (fip, "EPOCH", &x) == 0) {
	    year_mjd (x, mjdp);
	    return (0);
	}

	if (getStringFITS (fip, "DATE-OBS", buf) == 0) {
	    int dd, mm, yy;
	    if (sscanf (buf, "%d%*[/-]%d%*[/-]%d", &dd, &mm, &yy) == 3) {
		yy += (yy < 50 ? 2000 : 1900);
		cal_mjd (mm, (double)dd, yy, mjdp);
		return (0);
	    }
	}
	
	return (-1);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: skyfits.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
