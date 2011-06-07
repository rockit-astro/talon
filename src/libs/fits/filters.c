/* various fits file filters */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "P_.h"
#include "astro.h"
#include "fits.h"
#include "lstsqr.h"


#define	BORDER	32	/* ignore this much around the edge */


/* compare two ints as per qsort() */
static int
vcmp_f (const void *i1p, const void *i2p)
{
	int i1 = *((int *)i1p);
	int i2 = *((int *)i2p);

	return (i2 - i1);
}

/* modify CamPixel image from by passing over it a median filter of size
 * (2*hsize+1)*(2*hsize+1). put the result in to.
 * return 0 if ok, else -1.
 * N.B. we assume fip and tip are the same size and have separate pixel memory.
 */
int
medianFilter (FImage *from, FImage *to, int hsize)
{
	int ninside = (2*hsize+1)*(2*hsize+1);
	CamPixel *fip = (CamPixel *)from->image;
	CamPixel *tip = (CamPixel *)to->image;
	int w = from->sw;
	int h = from->sh;
	int *subfr;
	int x, y, i, j;
	int wrap;

	subfr = (int *)malloc (ninside * sizeof(int));
	if (!subfr) {
	    fprintf (stderr, "Can not malloc(%d) for median filter\n", ninside);
	    return (-1);
	}
	fip += hsize + w*hsize;
	tip += hsize + w*hsize;
	wrap = 2*hsize;

	for (y = h-wrap; --y >= 0; ) {
	    for (x = w-wrap; --x >= 0; ) {

		int n = 0;

		for (i = -hsize; i <= hsize; i++)
		    for (j = -hsize; j <= hsize; j++)
			subfr[n++] = (int)fip[i*w+j];

		qsort ((void *)subfr, ninside, sizeof(int), vcmp_f);

		*tip++ = subfr[ninside/2];
		fip++;
	    }

	    fip += wrap;
	    tip += wrap;
	}

	free (subfr);

	return (0);
}

/* flat field: find best-fit polynomial */

typedef struct {
    int x, y;		/* image location */
    double z;		/* median value in vacinity patch */
} ImMed;

static ImMed *immed;	/* malloced list of patch info */
static int nimmed;	/* number of patches */
static int porder;	/* polynomial order to fit */

/* evaluate the polynomial surface described by p[] at [x,y].
 * poly is: p[0] + p[1]y + p[2]yy + ... p[porder+1]x + p[porder+2]xy + ...
 */
static double
surface_z (int x, int y, double p[])
{
	double X, Y, z;
	int i, j, n;

	X = 1;
	n = 0;
	z = 0;
	for (i = 0; i < porder+1; i++) {
	    Y = 1;
	    for (j = 0; j < porder+1; j++) {
		z += p[n++]*X*Y;
		Y *= y;
	    }
	    X *= x;
	}

	return (z);
}

/* return the chi-sqr of the candidate model in p[] */
static double
surface_chisqr (double p[])
{
	ImMed *imp = immed;
	double cs = 0;
	int i;

	for (i = 0; i < nimmed; i++) {
	    double c = surface_z (imp->x, imp->y, p) - imp->z;
	    cs += c*c;
	    imp++;
	}
	return (cs);
}

/* flatten CamPixel image `from' into `to' as follows:
 *   goal is to find best order-n 2d polynomial, which has (n+1)*(n+1) terms.
 *   break image into (nterms+1)**2 patches, find center and median of each.
 *   find polynomial coefficients which best fit the medians.
 *   find the mean of the entire poly surface,
 *   multiply each pixel by the ratio of the interpolated surface mean/value.
 * return 0 if ok, else -1.
 * N.B. we assume from/to are the same size and have separate pixel memory.
 */
int
flatField (FImage *from, FImage *to, int order)
{
	CamPixel *fip = (CamPixel *)from->image;
	CamPixel *tip = (CamPixel *)to->image;
	int w = from->sw;
	int h = from->sh;
	double *p0, *p1;
	int weach, heach;
	int nterms;
	int nside;
	ImMed *imp;
	int n, i, j, x, y;
	double dx, dy, dz, z0;
	double mean;

	/* polynomial has (order+1)**2 terms */
	nterms = (order+1)*(order+1);
	p0 = (double *) calloc (nterms, sizeof(double));
	if (!p0)
	    return (-1);
	p1 = (double *) calloc (nterms, sizeof(double));
	if (!p1) {
	    free ((void *)p0);
	    return (-1);
	}

	/* find median in (nterms+1) patches in each dimension (sans BORDER)*/
	porder = order;
	nside = nterms+1;
	nimmed = nside*nside;
	immed = (ImMed *) calloc (nimmed, sizeof(ImMed));
	if (!immed) {
	    free ((void *)p0);
	    free ((void *)p1);
	    return (-1);
	}
	weach = (w - 2*BORDER)/nside;		/* centered within BORDER */
	heach = (h - 2*BORDER)/nside;		/* centered within BORDER */
	imp = immed;
	for (x = BORDER; x < BORDER + nside*weach; x += weach) {
	    for (y = BORDER; y < BORDER + nside*heach; y += heach) {
		AOIStats s;

		aoiStatsFITS ((char *)fip, w, x, y, weach, heach, &s);
		imp->x = x + weach/2;		/* patch center */
		imp->y = y + heach/2;		/* patch center */
		imp->z = (double)s.median;	/* patch median */
		imp++;
	    }
	}

	/* need two guesses as seed.
	 * first is flat plane at central patch's median.
	 * second uses slopes from ul to ur and bl corners.
	 * N.B. we rely on p0 being initted to all 0
	 */
	p0[0] = immed[nimmed/2].z;
	n = 0;
	dx = (nside-1)*weach;			/* center distance across */
	dy = (nside-1)*heach;			/* center distance down */
	z0 = immed[0].z;
	dz = ((immed[nside-1].z-z0) + (immed[nimmed-1].z-z0))/2;
	for (i = 0; i < order+1; i++)
	    for (j = 0; j < order+1; j++)
		p1[n++] = (i==0&&j==0) ? z0 :
				dz / (pow(dx,(double)i) * pow(dy,(double)j));

	/* find best-fit surface to the median patches */
	if (lstsqr (surface_chisqr, p0, p1, nterms, 0.01) < 0)
	    return (-1);

	/* find "mean" of surface defined by p0[] */
	mean = 0;
	for (i = 0; i < nimmed; i++) {
	    ImMed *imp = &immed[i];
	    mean += surface_z (imp->x, imp->y, p0);
	}
	mean /= nimmed;

	/* use surface against input to flat output */
	for (y = 0; y < h; y++)
	    for (x = 0; x < w; x++)
		*tip++ = *fip++ * mean/surface_z (x, y, p0);

	free ((void *)p0);
	free ((void *)p1);
	free ((void *)immed);

	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: filters.c,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $"};
