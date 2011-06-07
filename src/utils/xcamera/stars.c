/* code to handle GSC stars and calibration */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/ToggleB.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>

#include "P_.h"
#include "astro.h"
#include "xtools.h"
#include "fits.h"
#include "wcs.h"
#include "fieldstar.h"
#include "ps.h"
#include "camera.h"

static void makeGCs(void);

static GC gscGC;
static Pixel posGSC, negGSC;

static GC starGC;

#define	MYMIN(a,b)	((a) < (b) ? (a) : (b))
#define	MYMAX(a,b)	((a) > (b) ? (a) : (b))

static int findImageCenterFOV (FImage *fip, double *rap, double *decp, double
    *fovp, char errmsg[]);
static void toHSV (double r, double g, double b, double *hp, double *sp,
    double *vp);
static void toRGB (double h, double s, double v, double *rp, double *gp,
    double *bp);

/* called when asked to mark the GSC staGCs.
 * if set, mark stars; else remove them.
 */
/* ARGSUSED */
void
markGSCCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	FImage *fip = &state.fimage;

	msg("");

	state.showgsc = XmToggleButtonGetState (w);

	if (!fip->image)
	    return;

	if (state.showgsc)
	    markGSC();
	else
	    refreshScene (0, 0, fip->sw, fip->sh); /* refresh to erase */
}

/* called when asked to mark the stars once.
 */
/* ARGSUSED */
void
markStarsCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);
	FImage *fip = &state.fimage;
	StarStats *sp;
	int n;
	int i;
	int shift;

	msg("");
	if (!fip->image)
	    return;

	watch_cursor (1);

	n = findStatStars (fip->image, fip->sw, fip->sh, &sp);
	msg ("Found %d stars", n);

	if (!starGC)
	    makeGCs();

	shift = state.mag/MAGDENOM/2;	/* try to center when zoomed */
	for (i = 0; i < n; i++) {
	    int x, y;

	    x = sp[i].x + .5;
	    y = sp[i].y + .5;
	    image2window (&x, &y);

	    XDrawRectangle (dsp, win, starGC, x-5+shift, y-5+shift, 10, 10);
	}

	if (n >= 0)
	    free((char *)sp);

	watch_cursor (0);
}

/* called when asked to mark the streaks.
 */
/* ARGSUSED */
void
markStreaksCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);
	FImage *fip = &state.fimage;
	StreakData *streakList=NULL;
	int n;
	int ns;
	int i;
	int shift;
	static Pixel streakC,starMarkC;
	
	msg("");
	if (!fip->image)
	    return;

	watch_cursor (1);
		
	(void) findStarsAndStreaks(fip->image, fip->sw, fip->sh,
							  NULL,NULL,NULL,&streakList,&n);
	
	if (!starGC)
	    makeGCs();

	/* get the normal GSC color */
	if(!streakC) {
		/* get the star color */
		if (get_color_resource (dsp, myclass, "StarMarkColor", &starMarkC) < 0) {
	    	msg ("Can't get StarMarkColor -- using White");
		    starMarkC = WhitePixel (dsp, DefaultScreen(dsp));
		}
		if (0 > get_color_resource (dsp, myclass, "StreakMarkColor", &streakC)) {
			streakC = starMarkC;
		}
	}
	XSetForeground (dsp, starGC, streakC);
	
	shift = state.mag/MAGDENOM/2;	/* try to center when zoomed */
	ns = 0;
	for (i = 0; i < n; i++) {
		StreakData *pStr = &streakList[i];
		int summary = (pStr->flags & STREAK_MASK);
		if(summary != STREAK_NO) {
		    int x1, y1, x2, y2;
		    ns++;
		    if(!(pStr->flags & STREAK_MASK)) {
		    	// use the peak-to-peak coordinates for drawing streaks
			    x1 = streakList[i].startX;
			    y1 = streakList[i].startY;
    			x2 = streakList[i].endX;
		    	y2 = streakList[i].endY;
			} else {
				// use the full extent for the "maybes"
			    x1 = streakList[i].walkStartX;
			    y1 = streakList[i].walkStartY;
    			x2 = streakList[i].walkEndX;
			    y2 = streakList[i].walkEndY;
			}
		
		    image2window (&x1, &y1);
		    image2window (&x2, &y2);

		    x1+=shift;
		    x2+=shift;
	    	y1+=shift;
		    y2+=shift;
		    if(summary == STREAK_MAYBE) {
		    	// draw the "maybes" as a circled object
				int dx = x2 - x1;
				int dy = y2 - y1;
				int len = sqrt(dx*dx+dy*dy);
				int cx = x1 + dx/2;
				int cy = y1 + dy/2;
				
				XDrawArc(dsp,win,starGC,cx-len/2-4,cy-len/2-4,len+8,len+8,0,360*64);
		    	XDrawLine(dsp,win,starGC,x1,y1,x2,y2);
			} else {
				// draw the streak itself
				int x,y;
				int dy = y2-y1;
				int dx = x2-x1;
				if(dx < 0) dx = -dx;
				if(dy > dx) {			
					for(y=0; y<=dy; y++) {
						x = y/streakList[i].slope;
						XFillRectangle(dsp,win,starGC,x1+x-2,y1+y-2,4,4);
					}
				} else {
					double m = streakList[i].slope;
					if(x1 < x2) {
						for(x=0; x<=dx; x++) {
							y = x * m;
							XFillRectangle(dsp,win,starGC,x1+x-2,y1+y-2,4,4);
						}
					} else {
						for(x=0; x<=dx; x++) {
							y = x * m;
							XFillRectangle(dsp,win,starGC,x1-x-2,y1-y-2,4,4);
						}
					}
				}
			}
		}
	}
	XSetForeground (dsp, starGC, starMarkC);
	
	if(streakList) free(streakList);
	
	msg ("Found %d qualifying items in streak search", ns);
	
	watch_cursor (0);
}

void
markSmearsCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);
	FImage *fip = &state.fimage;
	SmearData *smearList=NULL;
	int numSmears;
	int i;
	int shift;
	int rt;
	char str[1024];
	static Pixel streakC,starMarkC;
	
	msg("");
	if (!fip->image)
	    return;

	watch_cursor (1);
	
	stopchk_up(toplevel_w, "Smear Detect / WCS Search", "Press Stop to abandon");
	
	SetWCSFunction(setWCSFITS); // must call before calling findSmears (at least once)	
	rt = findSmears(fip, &smearList, &numSmears, 1, !usno_ok, huntrad, stopchk, str);
	if(rt < 0 ) { // error in detection
	
		watch_cursor(0);
		msg("Error finding smears: %s",str);
		stopchk_down();
		return;
	} else {
	    resetGSC();
	    updateFITS();
	    gscSetDialog();
	    refreshScene (0, 0, fip->sw, fip->sh);
	}

	stopchk_down();

		
	if (!starGC)
	    makeGCs();

	/* get the normal GSC color */
	if(!streakC) {
		/* get the star color */
		if (get_color_resource (dsp, myclass, "StarMarkColor", &starMarkC) < 0) {
	    	msg ("Can't get StarMarkColor -- using White");
		    starMarkC = WhitePixel (dsp, DefaultScreen(dsp));
		}
		if (0 > get_color_resource (dsp, myclass, "StreakMarkColor", &streakC)) {
			streakC = starMarkC;
		}
	}
	XSetForeground (dsp, starGC, streakC);
	
	shift = state.mag/MAGDENOM/2;	/* try to center when zoomed */
	for (i = 0; i < numSmears; i++) {
		// Draw Smear
		SmearData *pStr = &smearList[i];
		if(pStr->length) {
			int x = pStr->startx - 1;
			int y = pStr->starty - MAXSMEARWIDTH/2 - 1;
			int w = (pStr->length * state.mag/MAGDENOM) + 2;
			int h = (MAXSMEARWIDTH * state.mag/MAGDENOM) + 2;
	
		    image2window (&x, &y);
		    x += shift;
			y += shift;
		
			XDrawRectangle (dsp, win, starGC, x, y, w, h);
		} else {
			// Draw Anomoly
			int rad = (MAXSMEARWIDTH * state.mag/MAGDENOM) + 2;
			int x = pStr->startx;
			int y = pStr->starty;

		    image2window (&x, &y);
		    x += shift;
			y += shift;
						
			XSetForeground (dsp, starGC, starMarkC);
			
			XDrawArc(dsp,win,starGC,x-rad,y-rad,rad*2,rad*2,0,360*64);
				
			XSetForeground (dsp, starGC, streakC);
		}
			
	}
	
	XSetForeground (dsp, starGC, starMarkC);
	
	if(smearList) free(smearList);
	
	if(rt > 0) msg(str); // report return message if WCS failed
	else msg ("Found %d qualifying items in smear search", numSmears);
	
	watch_cursor (0);
}



/* called when asked to mark header ra/dec location once */
/* ARGSUSED */
void
markHdrRADecCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);
	FImage *fip = &state.fimage;
	char errmsg[1024];
	char ra[100], dec[100];
	double rar, decr;
	double x, y;
	int ix, iy, r;
	int iw, ih;

	msg("");
	if (!fip->image)
	    return;

	if (!starGC)
	    makeGCs();

	/* get the OBJRA or RA and OBJDEC or DEC fields, converted to rads */
	if (getStringFITS(fip,"OBJRA",ra)<0 && getStringFITS(fip,"RA",ra)<0){
	    msg ("No OBJRA or RA string fields in header");
	    return;
	}
	if (scansex (ra, &rar) < 0) {
	    msg ("Bad RA format: %s", ra);
	    return;
	}
	rar = hrrad(rar);
	if(getStringFITS(fip,"OBJDEC",dec)<0 && getStringFITS(fip,"DEC",dec)<0){
	    msg ("No OBJDEC or DEC string fields in header");
	    return;
	}
	if (scansex (dec, &decr) < 0) {
	    msg ("Bad DEC format: %s", dec);
	    return;
	}
	decr = degrad(decr);

	/* get pixel at that location */
	if (rd2kxy (fip, rar, decr, &x, &y) < 0) {
	    msg ("No WCS headers");
	    return;
	}
	ix = (int)floor(x + 0.5);
	iy = (int)floor(y + 0.5);

	/* see if it's even on the image */
	if (getNAXIS (fip, &iw, &ih, errmsg) < 0) {
	    msg ("%s", errmsg);
	    return;
	}
	if (ix < 0 || ix >= iw || iy < 0 || iy >= ih) {
	    int left  = ix < 0;
	    int down  = iy >= ih;
	    int up    = iy < 0;
	    int right = ix >= iw;

	    if (up) {
		if (left)       strcpy (errmsg, "to the upper left");
		else if (right) strcpy (errmsg, "to the upper right");
		else            strcpy (errmsg, "the top");
	    } else if (down) {
		if (left)       strcpy (errmsg, "to the lower left");
		else if (right) strcpy (errmsg, "to the lower right");
		else            strcpy (errmsg, "the bottom");
	    } else if (left)
				strcpy (errmsg, "to the left");
	    else
				strcpy (errmsg, "to the right");

	    msg ("Header RA/DEC is off %s at [%d,%d] from center", errmsg,
							    ix-iw/2, iy-ih/2);
	    return;
	}

	/* draw a target */
	r = 5;
	image2window (&ix, &iy);
	XSetForeground (dsp, gscGC, state.inverse ? negGSC : posGSC);
	XDrawArc (dsp, win, gscGC, ix-r, iy-r, 2*r, 2*r, 0, 360*64);
	XDrawLine (dsp, win, gscGC, ix-2*r, iy, ix-r, iy);
	XDrawLine (dsp, win, gscGC, ix+2*r, iy, ix+r, iy);
	XDrawLine (dsp, win, gscGC, ix, iy-2*r, ix, iy-r);
	XDrawLine (dsp, win, gscGC, ix, iy+2*r, ix, iy+r);
}

/* if already have state.fs_* arrays just draw them,
 * else do fresh lookup first.
 */
void
markGSC()
{
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);
	double ra, dec, fov;
	char buf[1024];
	int i;

	watch_cursor (1);

	if (!state.fs_a) {
	    int nstars;

	    /* find coordinates of center of image */
	    if (findImageCenterFOV (&state.fimage, &ra, &dec, &fov, buf) < 0) {
		msg ("Image lacks WCS fields: %s", buf);
		watch_cursor (0);
		return;
	    }

	    /* start with USNO stars, if found */
	    if (usno_ok == 0) {
		nstars = USNOFetch (ra, dec, fov, gsclimit, &state.fs_a, buf);
		if (nstars < 0) {
		    msg ("USNO error: %s", buf);
		    watch_cursor (0);
		    return;
		}
	    } else
		nstars = 0;

	    /* add nearby gsc stars */
	    nstars = GSCFetch (ra, dec, fov, gsclimit, &state.fs_a, nstars,buf);
	    if (nstars < 0) {
		msg ("GSC error: %s", buf);
		resetGSC();
		watch_cursor (0);
		return;
	    }

	    msg ("Mag limit %g: Found %d Field stars", gsclimit, nstars);
	    state.fs_n = nstars;
	}

	if (!gscGC)
	    makeGCs();

	XSetForeground (dsp, gscGC, state.inverse ? negGSC : posGSC);
	for (i = 0; i < state.fs_n; i++) {
	    double ra = state.fs_a[i].ra;
	    double dec = state.fs_a[i].dec;
	    double mag = state.fs_a[i].mag;
	    double dblx, dbly;
	    int x, y, r;

	    /* convert from RA/Dec to image coords */
	    if (rd2kxy (&state.fimage, ra, dec, &dblx, &dbly) < 0) {
		msg ("Error convering %g/%g to x/y", ra, dec);
		break;
	    }

	    /* convert from image to window coords */
	    x = (int)(dblx+0.5);
	    y = (int)(dbly+0.5);
	    image2window (&x, &y);

	    r = 2 + (15 - mag) + 0.5;
	    if (r < 2)
		r = 2;
	    r = r*state.mag/MAGDENOM;
	    XPSDrawArc (dsp, win, gscGC, x-r, y-r, 2*r+1, 2*r+1, 0, 360*64);

	    (void) sprintf (buf, "%.0f", state.fs_a[i].mag*10);
	    XPSDrawString (dsp, win, gscGC, x+2*r, y, buf, strlen(buf));
	}

	watch_cursor (0);
}

/* reclaim GSC list, if any */
void
resetGSC()
{
	if (state.fs_a) {
	    free ((char *)state.fs_a);
	    state.fs_a = NULL;
	}
	state.fs_n = 0;
}

static void
makeGCs()
{
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);
	XGCValues gcv;
	XColor xc;
	double r, g, b, h, s, v;
	unsigned int gcm;
	XFontStruct *fsp;
	Pixel p;

	/* get the normal GSC color */
	if (get_color_resource (dsp, myclass, "GSCColor", &posGSC) < 0) {
	    msg ("Can't get GSCColor -- using White");
	    posGSC = WhitePixel (dsp, DefaultScreen(dsp));
	}
	get_views_font (dsp, &fsp);	/* use same font registered for ps */
	gcv.font = fsp->fid;
	gcm = GCFont;
	gscGC = XCreateGC (dsp, win, gcm, &gcv);

	/* make the negative color by flipping V and HSV-space */
	xc.pixel = posGSC;
	XQueryColor (dsp, camcm, &xc);
	toHSV ((double)xc.red/65535.0, (double)xc.green/65535.0,
					(double)xc.blue/65535.0, &h, &s, &v);
	v = 1 - v;
	toRGB (h, s, v, &r, &g, &b);
	xc.red = floor(r*65535.);
	xc.green = floor(g*65535.);
	xc.blue = floor(b*65535.);
	if (XAllocColor (dsp, camcm, &xc))
	    negGSC = xc.pixel;
	else
	    negGSC = BlackPixel (dsp, DefaultScreen(dsp));

	/* get the star color */
	if (get_color_resource (dsp, myclass, "StarMarkColor", &p) < 0) {
	    msg ("Can't get StarMarkColor -- using White");
	    p = WhitePixel (dsp, DefaultScreen(dsp));
	}
	gcm = GCForeground;
	gcv.foreground = p;
	starGC = XCreateGC (dsp, win, gcm, &gcv);
}

/* using the C* fields in fip, find image center and fov.
 * return 0 if ok, else excuse in msg[] and -1.
 */
static int
findImageCenterFOV (FImage *fip,
double *rap, double *decp, double *fovp, char errmsg[])
{
	double ref, refpix;
	double incx, incy;

	/* find pixel sizes in each dimension */
	if (getRealFITS (fip, "CDELT1", &incx) < 0) {
	    sprintf (errmsg, "No CDELT1 field");
	    return (-1);
	}
	incx = degrad(incx);
	if (getRealFITS (fip, "CDELT2", &incy) < 0) {
	    sprintf (errmsg, "No CDELT2 field");
	    return (-1);
	}
	incy = degrad(incy);

	/* fov is just diagonal times size of each pixel */
	*fovp = sqrt(fip->sw*fip->sw*incx*incx + fip->sh*fip->sh*incy*incy);

	/* RA at center based on reference pixel value and it's location */
	if (getRealFITS (fip, "CRVAL1", &ref) < 0) {
	    sprintf (errmsg, "No CRVAL1 field");
	    return (-1);
	}
	ref = degrad (ref);
	if (getRealFITS (fip, "CRPIX1", &refpix) < 0) {
	    sprintf (errmsg, "No CRPIX1 field");
	    return (-1);
	}
	*rap = ref + ((fip->sw-1)/2.0-refpix)*incx;

	/* DEC at center based on reference pixel value and it's location */
	if (getRealFITS (fip, "CRVAL2", &ref) < 0) {
	    sprintf (errmsg, "No CRVAL2 field");
	    return (-1);
	}
	ref = degrad (ref);
	if (getRealFITS (fip, "CRPIX2", &refpix) < 0) {
	    sprintf (errmsg, "No CRPIX2 field");
	    return (-1);
	}
	*decp = ref + ((fip->sh-1)/2.0-refpix)*incy;

	return (0);
}

static void
toHSV (double r, double g, double b, double *hp, double *sp, double *vp)
{
	double min, max, diff;
	double h, s, v;

	max = MYMAX (MYMAX (r, g), b);
	min = MYMIN (MYMIN (r, g), b);
	diff = max - min;
	v = max;
	s = max != 0 ? diff/max : 0;
	if (s == 0)
	    h = 0;
	else {
	    if (r == max)
		h = (g - b)/diff;
	    else if (g == max)
		h = 2 + (b - r)/diff;
	    else
		h = 4 + (r - g)/diff;
	    h /= 6;
	    if (h < 0)
		h += 1;
	}

	*hp = h;
	*sp = s;
	*vp = v;
}

static void
toRGB (double h, double s, double v, double *rp, double *gp, double *bp)
{
	double r, g, b;
	double f, p, q, t;
	int i;

	if (v == 0) 
	    r = g = b = 0.0;
	else if (s == 0)
	    r = g = b = v;
	else {
	    if (h >= 1)
		h = 0;
	    i = (int)floor(h * 6);
	    f = h * 6.0 - i;
	    p = v * (1.0 - s);
	    q = v * (1.0 - s * f);
	    t = v * (1.0 - s * (1.0 - f));

	    switch (i) {
	    case 0: r = v; g = t; b = p; break;
	    case 1: r = q; g = v; b = p; break;
	    case 2: r = p; g = v; b = t; break;
	    case 3: r = p; g = q; b = v; break;
	    case 4: r = t; g = p; b = v; break;
	    case 5: r = v; g = p; b = q; break;
	    default: r = g = b = 1; break;
	    }
	}

	*rp = r;
	*gp = g;
	*bp = b;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: stars.c,v $ $Date: 2003/03/13 00:40:47 $ $Revision: 1.7 $ $Name:  $"};
