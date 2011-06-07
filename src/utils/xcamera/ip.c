/* code to perform the image processing operations */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <Xm/Xm.h>

#include "fits.h"
#include "fieldstar.h"
#include "camera.h"

/* fill in state.xpixels[] with an array of NXCOLS pixels from the desired lut
 * type. if luttype != UNDEF_LUT first XFreeColor the existing pixels.
 */
void
initXPixels(lt)
LutType lt;
{
	Display *dsp = XtDisplay (toplevel_w);
	XColor xc;
	int i;

	/* harmless to ask for the same type again */
	if (lt == state.luttype)
	    return;

	/* if already allocated, free so we can get again */
	if (state.luttype != UNDEF_LUT) {
	    XFreeColors (dsp, camcm, state.xpixels, NXCOLS, 0);
	    state.luttype = UNDEF_LUT;
	}

	/* we know we'll always be setting all components */
	xc.flags = DoRed | DoGreen | DoBlue;

	/* we do the BW lut mannually -- all others come from tables */
	if (lt == BW_LUT) {
	    for (i = 0; i < NXCOLS; i++) {
		xc.red = xc.green = xc.blue = 65535L*i/NXCOLS;
		if (XAllocColor (dsp, camcm, &xc) == 0) {
		    printf ("Can not allocate color #f%04x%04x%04x\n",
						    xc.red, xc.green, xc.blue);
		    state.xpixels[i] = state.xpixels[i-1]; /* reuse */
		} else
		    state.xpixels[i] = xc.pixel;
	    }
	} else {
	    LutDef *lut;

	    switch (lt) {
	    case HEAT_LUT: lut = heat_lut; break;
	    case RAINBOW_LUT: lut = rainbow_lut; break;
	    case STANDARD_LUT: lut = standard_lut; break;
	    default:
		printf ("Bug! Bad lut type: %d\n", lt);
		exit (1);
	    }

	    for (i = 0; i < NXCOLS; i++) {
		xc.red = (short)(lut[i*LUTLN/NXCOLS].red*65535);
		xc.green = (short)(lut[i*LUTLN/NXCOLS].green*65535);
		xc.blue = (short)(lut[i*LUTLN/NXCOLS].blue*65535);
		if (XAllocColor (dsp, camcm, &xc) == 0) {
		    printf ("Can not allocate color #f%04x%04x%04x\n",
						    xc.red, xc.green, xc.blue);
		    state.xpixels[i] = state.xpixels[i-1]; /* reuse */
		} else
		    state.xpixels[i] = xc.pixel;
	    }
	}

	state.luttype = lt;
}

/* fill in state.lut[] so it returns black pixels for indices at or below
 * state.lo, white for indeces at or above state.hi, and a linear grey-scale
 * ramp in between. state.xpixels[] is an array of NXCOLS from black to white.
 * if state.inverse, then it's just the opposite.
 */
void
setLut ()
{
	int ngray = state.hi - state.lo;
	Pixel p;
	int i;

	p = state.inverse ? state.xpixels[NXCOLS-1] : state.xpixels[0];
	for (i = 0; i <= (int)state.lo; i++)
	    state.lut[i] = p;

	if (state.inverse)
	    for (/* continue i */; i < (int)state.hi; i++)
		state.lut[i] =
			state.xpixels[(ngray - (i-(int)state.lo))*NXCOLS/ngray];
	else 
	    for (/* continue i */; i < (int)state.hi; i++)
		state.lut[i] = state.xpixels[(i-(int)state.lo)*NXCOLS/ngray];

	p = state.inverse ? state.xpixels[0] : state.xpixels[NXCOLS-1];
	for (/* continue i */; i < NCAMPIX; i++)
	    state.lut[i] = p;
}
 
/* compute state.stats
 * only tricky part is that we might want the stats _outside_ the aoi too!
 */
void
computeStats()
{
	FImage *fip = &state.fimage;
	AOIStats *ap = &state.stats;

	/* winhist option stats */

	if (!state.aoistats || !state.aoinot) {
	    if (state.aoistats)
		aoiStatsFITS (fip->image, fip->sw, state.aoi.x, state.aoi.y,
						state.aoi.w, state.aoi.h, ap);
	    else
		aoiStatsFITS (fip->image, fip->sw, 0, 0, fip->sw, fip->sh, ap);
	} else {
	    /* we want the stats *outside* the aoi.
	     * we reinvent much of the wheel here.
	     */
	    AOIStats a;
	    double sd2;
	    int npix, npix2;
	    int i, n;

	    /* gets stats for the whole image and within the aoi */
	    aoiStatsFITS (fip->image, fip->sw, 0, 0, fip->sw, fip->sh, ap);
	    aoiStatsFITS (fip->image, fip->sw, state.aoi.x, state.aoi.y,
						state.aoi.w, state.aoi.h, &a);

	    /* fix hist by rolling the aoi counts back out */
	    for (i = 0; i < NCAMPIX; i++)
		ap->hist[i] -= a.hist[i];

	    /* fix sum and sum2 then recompute the mean and sd */
	    npix = fip->sw * fip->sh - state.aoi.w * state.aoi.h;
	    ap->sum -= a.sum;
	    ap->sum2 -= a.sum2;
	    ap->mean = (int)(ap->sum/npix + 0.5);
	    sd2 = (ap->sum2 - ap->sum * ap->sum/npix)/(npix-1);
	    ap->sd = sd2 <= 0.0 ? 0.0 : sqrt(sd2);

	    /* first pixel is lowest in image; last is highest */
	    for (i = 0; i < NCAMPIX; i++)
		if (ap->hist[i] > 0) {
		    ap->min = i;
		    break;
		}
	    for (i = NCAMPIX-1; i >= 0; --i)
		if (ap->hist[i] > 0) {
		    ap->max = i;
		    break;
		}

	    /* median pixel is one with equal counts below and above */
	    n = 0;
	    npix2 = npix/2;
	    for (i = 0; i < NCAMPIX; i++) {
		n += ap->hist[i];
		if (n >= npix2) {
		    ap->median = i;
		    break;
		}
	    }

	    /* TODO: maxx and maxy */
	    ap->maxx = ap->maxy = 0;
	}
}

/* convert from image to window coords, IN PLACE */
void
image2window (xp, yp)
int *xp, *yp;
{
	if (state.crop) {
	    *xp -= state.aoi.x;
	    *yp -= state.aoi.y;
	}
	*xp = *xp * state.mag / MAGDENOM;       /* don't use *= */
	*yp = *yp * state.mag / MAGDENOM;       /* don't use *= */
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: ip.c,v $ $Date: 2001/05/23 15:36:02 $ $Revision: 1.2 $ $Name:  $"};
