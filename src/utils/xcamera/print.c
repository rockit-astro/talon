/* handle the easy part of printing -- good stuff is in ps.c */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <Xm/Xm.h>

#include "P_.h"
#include "astro.h"
#include "xtools.h"
#include "strops.h"
#include "fits.h"
#include "wcs.h"
#include "fieldstar.h"
#include "camera.h"
#include "ps.h"

#define	NGRAY	60	/* number of gray levels on paper */

static void print_it(void);
static void do_image (void);
static void annotate(void);

/* callback to ask about printing */
void
printCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XPSAsk ("Camera", print_it);
}

/* get the XFontStruct we want to use when drawing text for the display views.
 * if can't open it, use the default font in a GC.
 */
void
get_views_font (dsp, fspp)
Display *dsp;
XFontStruct **fspp;
{
	static XFontStruct *fsp;

	if (!fsp) {
	    static char resname[] = "viewsFont";
	    char *fname = getXRes (toplevel_w, resname, "fixed");

	    /* use XLoadQueryFont because it returns gracefully if font is not
	     * found; XLoadFont calls the default X error handler.
	     */
	    if (!fname || (fsp = XLoadQueryFont (dsp, fname)) == 0) {
		char buf[256];

		if (!fname)
		    sprintf(buf,"Can't find resource `%.180s'", resname);
		else
		    sprintf (buf, "Can't load font `%.180s'", fname);
		strcat (buf, "... using default GC font");
		msg(buf);

		fsp = XQueryFont (dsp,
			XGContextFromGC (DefaultGC (dsp, DefaultScreen (dsp))));
		if (!fsp) {
		    printf ("No default Font!\n");
		    exit(1);
		}
	    }
	}
	
	*fspp = fsp;
}

/* proceed to generate a postscript file.
 * call XPSClose() when finished.
 */
static void
print_it ()
{
	FImage *fip = &state.fimage;
	XFontStruct *fsp;
	int w, h;

	if (!fip->image) {
	    msg ("No image");
	    return;
	}

	watch_cursor(1);

	/* get effective size */
	w = (state.crop ? state.aoi.w : fip->sw) * state.mag / MAGDENOM;
	h = (state.crop ? state.aoi.h : fip->sh) * state.mag / MAGDENOM;

	/* fit view in square across the top and prepare to capture X calls */
	XPSXBegin (XtWindow(state.imageDA), 0, 0, w, h, 1*72, 10*72,
						(int)(6.5*72), (int)(6.5*72));

        /* get and register the font to use for all gcs from obj_pickgc() */
	get_views_font (XtDisplay(toplevel_w), &fsp);
	XPSRegisterFont (fsp->fid, "Helvetica");

	/* render the image */
	do_image();

	/* redraw everything else */
	refreshScene (0, 0, fip->sw, fip->sh);

        /* no more X captures */
	XPSXEnd();

	/* add some extra info */
	annotate ();

	/* finished */
	XPSClose();

	watch_cursor(0);
}

static void
do_image()
{
	Window win = XtWindow (state.imageDA);
	FImage *fip = &state.fimage;
	unsigned short *pix;
	char cm[NCAMPIX];
	int i, nlohi;
	double scale;
	AOI *aoip;

	/* set the colormap */
	for (i = 0; i <= (int)state.lo; i++)
	    cm[i] = 0;
	nlohi = (int)state.hi - (int)state.lo;
	for (/* continue i */; i < (int)state.hi; i++)
	    cm[i] = (i-(int)state.lo)*NGRAY/nlohi;
	for (/* continue i */; i < NCAMPIX; i++)
	    cm[i] = NGRAY-1;

	/* set the image size */
	pix = (unsigned short *)fip->image;
	aoip = state.crop ? &state.aoi : NULL;
	scale = (double)state.mag/MAGDENOM;

	/* draw to postscript */
	XPSImage (win, pix, cm, NGRAY-1, state.inverse, aoip, scale,
							    fip->sw, fip->sh);
}

static void
annotate()
{
	typedef struct {
	    char lbl[80];
	    char val[80];
	} LblEntry;
	LblEntry lbl[32], *lp = lbl;
	FImage *fip = &state.fimage;
	AOI *aoip = &state.aoi;
        char buf[128];
	int ctr = 306;  /* = 8.5*72/2 */
	int lx = 150, rx = 405;
	double tmp, tmp2;
	double cdelt1, cdelt2;
	int knowscale;
	int i, n;
	int row, right;

	/* N.B. make sure we have _somthing_ in these top lines for the box */

	if (!getStringFITS (fip, "OBJECT", buf)) {
	    lp->lbl[0] = '\0';
	    strcpy (lp->val, buf);
	} else {
	    lp->lbl[0] = '\0';
	    lp->val[0] = '\0';
	}
	lp++;

	if (!getStringFITS (fip, "ORIGIN", buf)) {
	    lp->lbl[0] = '\0';
	    strcpy (lp->val, buf);
	} else {
	    lp->lbl[0] = '\0';
	    lp->val[0] = '\0';
	}
	lp++;

	lp->lbl[0] = '\0';
	lp->val[0] = '\0';
	if (!getStringFITS (fip, "TELESCOP", buf)) {
	    strcat (lp->val, buf);
	}
	if (!getStringFITS (fip, "INSTRUME", buf)) {
	    strcat (lp->val, "  ");
	    strcat (lp->val, buf);
	}
	lp++;

	/* gap for box line */
	lp->lbl[0] = '\0';
	lp->val[0] = '\0';
	lp++;

	/* now stuff in the bottom box */

	strcpy (lp->lbl, "File name");
	strcpy (lp->val, basenm(state.fname));
	lp++;

	if (!getStringFITS (fip, "OBSERVER", buf)) {
	    strcpy (lp->lbl, "Observer");
	    strcpy (lp->val, buf);
	    lp++;
	}

	if (!getStringFITS (fip, "DATE-OBS", buf)) {
	    int d, m, y;
	    if (sscanf (buf, "%d/%d/%d", &d, &m, &y) == 3
				|| sscanf (buf, "%d-%d-%d", &y, &m, &d) == 3) {
		strcpy (lp->lbl, "UTC Date");
		sprintf (lp->val, "%d-%s-%d", d, monthName(m), y);
		lp++;
	    }
	}

	if (!getStringFITS (fip, "TIME-OBS", buf)) {
	    strcpy (lp->lbl, "UTC Time");
	    strcpy (lp->val, buf);
	    lp++;
	}

	if (!getRealFITS (fip, "JD", &tmp)) {
	    strcpy (lp->lbl, "Julian Date");
	    sprintf (lp->val, "%13.5f", tmp);
	    lp++;
	}

	if (!getRealFITS (fip, "EXPTIME", &tmp)) {
	    strcpy (lp->lbl, "Exp time");
	    sprintf (lp->val, "%g Seconds", tmp);
	    lp++;
	}

	if (!getStringFITS (fip, "FILTER", buf)) {
	    strcpy (lp->lbl, "Filter Code");
	    sprintf (lp->val, "%.1s", buf);
	    lp++;
	}

	if (!getStringFITS (fip, "ELEVATION", buf) && !scansex (buf, &tmp)
								&& tmp != 0.0) {
	    strcpy (lp->lbl, "Elevation (Z)");
	    sprintf (lp->val, "%.1f degrees (%.2f)", tmp, 1.0/sin(degrad(tmp)));
	    lp++;
	}

	knowscale = !getRealFITS (fip, "CDELT1", &cdelt1) &&
					!getRealFITS (fip, "CDELT2", &cdelt2);

	if (state.crop) {

	    strcpy (lp->lbl, "Image size");
	    sprintf (lp->val, "%dW x %dH pixels", aoip->w, aoip->h);
	    lp++;

	    if (knowscale) {
		strcpy (lp->lbl, "Image size");
		sprintf (lp->val, "%4.1fW x %4.1fH arc minutes",
			    fabs(cdelt1*aoip->w*60.), fabs(cdelt2*aoip->h*60.));
		lp++;
	    }

	    if (!xy2rd (fip, aoip->x + aoip->w/2., aoip->y + aoip->h/2.,
								&tmp, &tmp2)) {
		char buf2[32];
		strcpy (lp->lbl, "Image Center");
		fs_sexa (buf, radhr(tmp), 2, 36000);
		fs_sexa (buf2, raddeg(tmp2), 3, 3600);
		sprintf (lp->val, "%s %s (%g)", buf, buf2, pEyear());
		lp++;
	    }

	} else {

	    strcpy (lp->lbl, "Image size");
	    sprintf (lp->val, "%dW x %dH pixels", fip->sw, fip->sh);
	    lp++;

	    if (knowscale) {
		strcpy (lp->lbl, "Image size");
		sprintf (lp->val, "%4.1fW x %4.1fH arc minutes",
			    fabs(cdelt1*fip->sw*60.), fabs(cdelt2*fip->sh*60.));
		lp++;
	    }

	    if (!xy2rd (fip, fip->sw/2., fip->sh/2., &tmp, &tmp2)) {
		char buf2[32];
		strcpy (lp->lbl, "Image Center");
		fs_sexa (buf, radhr(tmp), 2, 36000);
		fs_sexa (buf2, raddeg(tmp2), 3, 3600);
		sprintf (lp->val, "%s %s (%g)", buf, buf2, pEyear());
		lp++;
	    }

	    strcpy (lp->lbl, "AOI size");
	    sprintf (lp->val, "%dW x %dH pixels", aoip->w, aoip->h);
	    lp++;

	    if (knowscale) {
		strcpy (lp->lbl, "AOI size");
		sprintf (lp->val, "%4.1fW x %4.1fH arc minutes",
			    fabs(cdelt1*aoip->w*60.), fabs(cdelt2*aoip->h*60.));
		lp++;
	    }

	    if (!xy2rd (fip, aoip->x + aoip->w/2., aoip->y + aoip->h/2.,
								&tmp, &tmp2)) {
		char buf2[32];
		strcpy (lp->lbl, "AOI Center");
		fs_sexa (buf, radhr(tmp), 2, 36000);
		fs_sexa (buf2, raddeg(tmp2), 3, 3600);
		sprintf (lp->val, "%s %s (%g)", buf, buf2, pEyear());
		lp++;
	    }
	}
	
	if (!getRealFITS (fip, "CROTA2", &tmp)) {
	    strcpy (lp->lbl, "Image rotation");
	    sprintf (lp->val, "%.2f Degrees E of N", tmp);
	    lp++;
	}

	if (!getRealFITS(fip,"FWHMH",&tmp) && !getRealFITS (fip,"FWHMV",&tmp2)){

	    strcpy (lp->lbl, "Median FWHM");
	    if (cdelt1 != 0.0 && cdelt2 != 0.0) {
		tmp *= fabs(cdelt1)*3600.;
		tmp2 *= fabs(cdelt2)*3600.;
		sprintf (lp->val, "%.1fW x %.1fH arc seconds", tmp, tmp2);
	    } else {
		sprintf (lp->val, "%.1fW x %.1fH pixels", tmp, tmp2);
	    }
	    lp++;
	}

	n = lp - lbl;
	if (n > XtNumber(lbl)) {
	    printf ("Bug! too many labels: %d\n", n);
	    exit(1);
	}
	row = 0;
	right = 0;
	for (i = 0; i < n; i++) {
	    int x, y;

	    lp = &lbl[i];

	    y = AROWY(14 - row);
	    if (lp->lbl[0] != '\0') {
		x = right ? rx : lx;
		sprintf (buf, "(%s) %d %d rstr", lp->lbl, x, y);
		XPSDirect (buf);
		sprintf (buf, "(%s) %d %d lstr", lp->val, x+15, y);
		XPSDirect (buf);
		right ^= 1;
	    } else {
		sprintf (buf, "(%s) %d %d cstr", lp->val, ctr, y);
		XPSDirect (buf);
		right = 0;
	    }
	    if (!right)
		row++;
	}

	i = AROWY(15);
	n = AROWY(10-(n-3)/2);	/* !! */
	sprintf (buf, "newpath 52 %d moveto 560 %d lineto stroke", i, i);
	XPSDirect (buf);
	sprintf (buf, "newpath 52 %d moveto 560 %d lineto stroke", n, n);
	XPSDirect (buf);
	sprintf (buf, "newpath 52 %d moveto 52 %d lineto stroke", i, n);
	XPSDirect (buf);
	sprintf (buf, "newpath 560 %d moveto 560 %d lineto stroke", i, n);
	XPSDirect (buf);
	i = (AROWY(11) + AROWY(12))/2;
	sprintf (buf, "newpath 52 %d moveto 560 %d lineto stroke", i, i);
	XPSDirect (buf);

	i = AROWY(7);
	if (knowscale) {
	    sprintf (buf, "newpath %d %d moveto %d %d lineto stroke", ctr, i,
								    ctr, i+20);
	    XPSDirect (buf);
	    sprintf (buf, "(%c) %d %d cstr", cdelt2>0?'S':'N', ctr, i+25);
	    XPSDirect (buf);

	    sprintf (buf, "newpath %d %d moveto %d %d lineto stroke", ctr, i,
								    ctr+20, i);
	    XPSDirect (buf);
	    sprintf (buf, "(%c) %d %d cstr", cdelt1>0?'E':'W', ctr+30, i-5);
	    XPSDirect (buf);
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: print.c,v $ $Date: 2001/04/19 21:12:00 $ $Revision: 1.1.1.1 $ $Name:  $"};
