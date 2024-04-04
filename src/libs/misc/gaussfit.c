/* given an array of pixels find the best-fit gaussian.
 * this is not really for external use -- just by starStats().
 * N.B. this is NOT reentrant.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>

#include "lstsqr.h"

#define	FRACERR		.0001		/* fractional error */

/* storage so we can get at the pixels from g_chisqr() */
static int g_npix;
static int *g_pix;

/* evaluate the chisqr of these parameters */
static double
g_chisqr (p)
double p[];
{
	double max = p[0];
	double cen = p[1];
	double sig = p[2];
	double cs;
	int i;

	cs = 0.0;
	for (i = 0; i < g_npix; i++) {
	    double dx = i - cen;
	    double e = max*exp(-(dx*dx)/(2*sig*sig)) - g_pix[i];
	    cs += e*e;
	}

	return (cs);
}

void
gaussfit (pix, n, gmaxp, cenp, fwhmp)
int pix[];	/* pixels along a line */
int n;		/* n pixels */
double *gmaxp;	/* return gaussian max value */
double *cenp;	/* return gaussian center pixel location */
double *fwhmp;	/* return full width half max */
{
	int min, max, avg, maxi, halfw;
	double p0[3], p1[3];
	double sigma;
	int i;

	/* make initial guesses */
	maxi = 0;
	min = max = pix[maxi];
	for (i = 1; i < n; i++) {
	    int p = pix[i];
	    if (p > max) {
		max = p;
		maxi = i;
	    }
	    if (p < min)
		min = p;
	}

	/* base sigma on fattest side */
	halfw = 1;
	avg = (max+min)/2;
	for (i = maxi+1; i < n; i++) {
	    if (pix[i] < avg) {
		halfw = i-maxi;
		break;
	    }
	}
	for (i = maxi-1; i >= 0; --i) {
	    if (pix[i] < avg) {
		if (maxi-i > halfw)
		    halfw = maxi-i;
		break;
	    }
	}
	sigma = 0.85*sqrt((double)halfw);

	/* solve */
	p0[0] = (double)max;
	p0[1] = (double)maxi;
	p0[2] = sigma;
	p1[0] = (double)max*1.1;
	p1[1] = (double)maxi+2;
	p1[2] = sigma+1.0;
	g_pix = pix;
	g_npix = n;
	if (lstsqr (g_chisqr, p0, p1, 3, FRACERR) < 0) {
	    *gmaxp = (double)max;
	    *cenp = (double)maxi;
	    *fwhmp = sigma/2;
	    fprintf (stderr, "Gaussfit fails\n");
	} else {
	    *gmaxp = p0[0];
	    *cenp = p0[1];
	    *fwhmp = 2.354 * p0[2];
	}
}

#ifdef TESTIT

int
main (int ac, char *av[])
{
	static int pix[] = {4, 30, 90, 180, 330, 200, 100, 50, 0};
	int n = sizeof(pix)/sizeof(pix[0]);
	double gmax;	/* return gaussian max value */
	double cen;	/* return gaussian center pixel location */
	double fwhm;	/* return full width half max */
	int i;

	gaussfit (pix, n, &gmax, &cen, &fwhm);

	printf ("gmax = %g\n", gmax);
	printf ("cen = %g\n", cen);
	printf ("fwhm = %g\n", fwhm);

	for (i = 0; i < n; i++) {
	    double dx = i - cen;
	    double sig = fwhm/2.354;

	    printf ("x = %6.2f y = %6.2f f() = %6.2f\n", (double)i,
		(double)pix[i], gmax * exp(-(dx*dx)/(2*sig*sig)));
	}

	return (0);
}

#endif
