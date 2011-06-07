#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "P_.h"
#include "astro.h"
#include "fits.h"

#define	XBORDER 32		/* ignore pixels this close to l/r edge */
#define	YBORDER	110		/* ignore pixels this close to t/b edge */
#define	MAXSIZE	2048		/* largest image dimension we can handle */
#define	MAXSHIFT	50	/* max shift we try either way */

static void findShiftPixelLimits (char *image, int sw, int x0, int y0,
    int nx, int ny, int *llimitp, int *ulimitp);
static void findXYSums (char *image, int w, int x0, int y0, int nx, int ny,
    int llimit, int ulimit, int xsum[], int ysum[]);
static int findShift (int sum1[], int sum2[], int n, int *dp);

/* find the shift dx and dy of image2 so it most closely matches fip1.
 * use only those pixels inside BORDER pixels from the edges.
 * we only search for shifts +/- MAXSHIFT.
 * return 0 if ok, else -1 if peg the meter.
 */
int
align2FITS (fip1, image2, dxp, dyp)
FImage *fip1;
char *image2;
int *dxp, *dyp;
{
	/* xsum[i] is sum of all pixels along y=i */
	/* ysum[i] is sum of all pixels along x=i */
	int xsum1[MAXSIZE], ysum1[MAXSIZE];
	int xsum2[MAXSIZE], ysum2[MAXSIZE];
	int x0, x1, y0, y1;
	int nx, ny;
	int llimit, ulimit;
	int xok, yok;

	/* set boundaries unless already cropped */
	if (getIntFITS (fip1, "CROPX", &xok) < 0) {
	    x0 = XBORDER;
	    x1 = fip1->sw - XBORDER;
	    y0 = YBORDER;
	    y1 = fip1->sh - YBORDER;
	} else {
	    x0 = 0;
	    x1 = fip1->sw;
	    y0 = 0;
	    y1 = fip1->sh;
	}
	nx = x1 - x0;
	ny = y1 - y0;

	findShiftPixelLimits (fip1->image, fip1->sw, x0, y0, nx, ny,
							&llimit, &ulimit);
	findXYSums (fip1->image, fip1->sw, x0, y0, nx, ny,
					    llimit, ulimit, xsum1, ysum1);
	findShiftPixelLimits (image2, fip1->sw, x0, y0, nx, ny,
							&llimit, &ulimit);
	findXYSums (image2, fip1->sw, x0, y0, nx, ny,
					    llimit, ulimit, xsum2, ysum2);

/*
#define PRINTXYSUMS
*/
#ifdef PRINTXYSUMS
	{   int i;
	    printf ("xsum1:\n");
	    for (i = 0; i < ny; i++)
		printf ("%d\n", xsum1[i]);
	    printf ("xsum2:\n");
	    for (i = 0; i < ny; i++)
		printf ("%d\n", xsum2[i]);
	    printf ("ysum1:\n");
	    for (i = 0; i < nx; i++)
		printf ("%d\n", ysum1[i]);
	    printf ("ysum2:\n");
	    for (i = 0; i < nx; i++)
		printf ("%d\n", ysum2[i]);
	}
#endif

	xok = findShift (xsum2, xsum1, ny, dyp);
	yok = 0;
	if (xok == 0)
	    yok = findShift (ysum2, ysum1, nx, dxp);

	return (xok == 0 && yok == 0 ? 0 : -1);
}

/* given an FImage of width sw and a subregion thereof, find the lower and
 * upper pixel values that should be used for detemining alignment shift.
 */
static void
findShiftPixelLimits (ip, sw, x0, y0, nx, ny, llimitp, ulimitp)
char *ip;
int sw;
int x0, y0, nx, ny;
int *llimitp, *ulimitp;
{
	int hist[NCAMPIX];
	CamPixel *image;
	CamPixel *row;
	double sum, sum2, sd2;
	int median = 0, sd;
	int npix, npix2;
	int x, y;
	int wrap;
	int i;

	/* find the sum of the pixels, and the sum of the pixels squared
	 * as well as the histogram over fip1.
	 */
	memset ((void *)hist, 0, sizeof(hist));
	image = (CamPixel *) ip;
	row = &image[sw*y0 + x0];
	wrap = sw - nx;
	sum = sum2 = 0.0;
	for (y = 0; y < ny; y++) {
	    for (x = 0; x < nx; x++) {
		unsigned p = (unsigned) (*row++);
		hist[p]++;
		sum += (double) (p);
		sum2 += (double) (p*p);
	    }
	    row += wrap;
	}

	/* find the standard deviation */
	npix = nx * ny;
	sd2 = (sum2 - sum*sum/npix)/(npix-1);
	sd = sd2 <= 0.0 ? 0 : (int)(sqrt(sd2)+0.5);

	/* median pixel is one with equal counts below and above */
	x = 0;
	npix2 = npix/2;
	for (i = 0; i < NCAMPIX; i++) {
	    x += hist[i];
	    if (x >= npix2) {
		median = i;
		break;
	    }
	}

	/* set limits on pixel values we will use */
	/*
	*llimitp = median + sd;
	*ulimitp = median + 2*sd;
	*/
	*llimitp = median + 4*sd;
	*ulimitp = 50000;
}

/* given an image of width w, a subregion therein and lower and upper pixel
 * values, find the sum of all the pixels of each row and column within the
 * region that fall within these limits and put them in ysum[] and xsum[]
 * respectively.
 * N.B. {x,y}sum[] indices are relative to x0 and y0.
 */
static void
findXYSums (image, w, x0, y0, nx, ny, llimit, ulimit, xsum, ysum)
char *image;
int w;
int x0, y0, nx, ny;
int llimit, ulimit;
int xsum[];
int ysum[];
{
	CamPixel *ip = (CamPixel *)image;
	CamPixel *row;
	int wrap;
	int x, y;

	for (x = 0; x < nx; x++)
	    ysum[x] = 0;
	for (y = 0; y < ny; y++)
	    xsum[y] = 0;

	row = &ip[w*y0 + x0];
	wrap = w - nx;
	for (y = 0; y < ny; y++) {
	    for (x = 0; x < nx; x++) {
		int p = (int) *row++;
		if (p >= llimit && p <= ulimit) {
		    xsum[y] += p;
		    ysum[x] += p;
		}
	    }
	    row += wrap;
	}
}

/* find shift in sum2 that minimizes difference from sum1
 * return 0 if ok else -1 if peg the meter.
 */
static int
findShift (sum1, sum2, n, dp)
int sum1[], sum2[];
int n;
int *dp;
{
	double minerr = -1.0;
	int shift, bestshift = 0;
	double toterr;
	int mini;
	int maxi;
	int i;

	/* check range -MAXSHIFT .. MAXSHIFT */
	for (shift = -MAXSHIFT; shift <= MAXSHIFT; shift++) {
	    toterr = 0.0;
	    mini = shift > 0 ? shift : 0;
	    maxi = shift > 0 ? n : n + shift;
	    for (i = mini; i < maxi; i++)
		toterr += fabs((double)(sum1[i-shift] - sum2[i]));
	    if (minerr < 0 || toterr < minerr) {
		minerr = toterr;
		bestshift = shift;
	    }
	}

	/* return -1 if bestshift is either extreme */
	if (bestshift == -MAXSHIFT || bestshift >= MAXSHIFT)
	    return (-1);

	*dp = bestshift;
	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: align2fits.c,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $"};
