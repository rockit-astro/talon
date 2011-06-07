/* some misc fits image processing utilities */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "P_.h"
#include "astro.h"
#include "configfile.h"
#include "telenv.h"
#include "fits.h"
#include "wcs.h"

/* image processing config params pulled from ipcfn whenever it changes */

//
// IP.CFG can now be set by caller... all IP.CFG settings are handled here
static char ipcfn[256] = "archive/config/ip.cfg";	/* sans TELHOME */

// values entered here are overwritten by ip.cfg...
// Now that we've consolidated the ip.cfg, there IS no reason for these to no be the default values
// So, they are :-)   Use ip.cfg to override these values, but if you don't have one, you won't crash!
// -- STAR FINDER --
int FSBORD = 32;		// border to ignore when finding stars
int FSNNBOX = 100;		// number of noise boxes to place over image
int FSNBOXSZ = 10;		// pixel width and height of noise box
int FSMINSEP = 5;		// minimum separation between stars, pixels
int FSMINCON = 4;		// minimum number of contiguous connected neighbors
double FSMINSD = 4;		// min SDs of noise above median to qualify
int BURNEDOUT = 60000;	// clamp/ignore pixels brighter than this
// -- STAR STATS --
double TELGAIN = 1.6;	// telescope gain, electrons/adu (magnitude calc)
int DEFSKYRAD = 30;		// default radius to use for sky stats
double MINAPRAD = 2;	// minimum aperture radius around star
double APGAP = 2;		// radius gap between star and sky
double APSKYX = 3;		// this many more pixels in sky than star
double MAXSKYPIX = 200;	// most pix we need for good sky stats
int MINGAUSSR = 7;		// min radius when computing gaussian stats
// -- FWHM STATS --
int NFWHM = 20;			// max stars to use for median FWHM
double FWHMSD = 10;		// min SD to use in finding median FWHM
int FWHMR = 16;			// cross-section radius, pixels
double FWHMRF = 1.3;	// max median fwhm ratio factor
double FWHMSF = 8;		// size factor to qualify in findStatStars()
double FWHMRATIO = 3;	// max ratio in x/yfwhm
// -- STREAK DETECTION --
double STRKDEV = .5;	// percentage (0.00-1.00) difference in fwhm ratio to consider abnormal
int STRKRAD = 8;		// radius to use when computing fwhm for streak analysis
int MINSTRKLEN = 10;	// minimum pixel length for full extent of streak
// -- SMEAR DETECTION --
int SMBOXW = 512;		// width of noise threshold box
int SMBOXH = 512;		// length of noise threshold box
int SAMPWD = 8;			// width of detection sample area
int SAMPHT = 2;			// height of detection sample area
double SMEARSD	= 6.0;	// Factor to multiply SD>M by for value above median to use as detection threshold
double DENSITY	= 0.65;	// Density of bright pixels that indicate a probable smear section
double LOCDENS = 0.20;	// Used when looking for top/bottom from local density values
double DROPOFF = 3.0;	// times dimmer before we consider not valid
int MAXSMEARWIDTH = 10; // Maximum width of a smear
int MINSMEARLEN	= 20;	// Minimum length for any smear
double SMEARLONGERTOL = 0.05;	// Tolerance for a smear to be longer than expected length (ratio of expected length)
double SMEARSHORTERTOL = 0.25;	// Tolerance for a smear to be shorter than expected length (ration of expected length)
double ANOMPEAKMULT	= 5.0;		// Minimum factor of sum of object greater than peak for anomoly detection
double ANOMFWHMDIFF	= 0.30;		// Maximum ratio X FWHM value can exceed Y FWHM value
// -- WCS FITTER
int MAXRESID = 3;		// max allowable residual in WCS fit, pixels
int MAXISTARS = 50;		// max stars to use from image for matching
int MAXCSTARS = 200;	// max stars to use from catalogs
int BRCSTAR = 6;		// brightest catalog star to use, mag
#if USE_DISTANCE_METHOD	
int MINPAIR = 6; 		// min stars to pair for matching
int MAXPAIR = 300;		// max image start to pair with catalogue for astrometry
int TRYSTARS = 24;		// try fit if find this no. of pairs, don't look for more
double MAXROT = 5.0;	// max rotation (degrees) in WCS fit
double MATCHDIST = 6.0;	// limit (arcsec) within which distances are considered
						// to be potentially the same in the catalogue & image
double REJECTDIST = 3.0; // rejection limit (arcsec) for higher order astrometric fit
int ORDER = 2;			// order of astrometric fit (2,3, or 5)
#endif

static CfgEntry ipcfg[] = {
// -- STAR FINDER --
	{"FSBORD",		CFG_INT,	&FSBORD},
	{"FSNNBOX",		CFG_INT,	&FSNNBOX},
	{"FSNBOXSZ",	CFG_INT,	&FSNBOXSZ},
	{"FSMINSEP",	CFG_INT,	&FSMINSEP},
	{"FSMINCON",	CFG_INT,	&FSMINCON},
	{"FSMINSD",		CFG_DBL,	&FSMINSD},
	{"BURNEDOUT",	CFG_INT,	&BURNEDOUT},
// -- STAR STATS --
	{"TELGAIN",		CFG_DBL,	&TELGAIN},
	{"DEFSKYRAD",	CFG_INT,	&DEFSKYRAD},
	{"MINAPRAD",	CFG_DBL,	&MINAPRAD},
	{"APGAP",		CFG_DBL,	&APGAP},
	{"APSKYX",		CFG_DBL,	&APSKYX},
	{"MAXSKYPIX",	CFG_DBL,	&MAXSKYPIX},
	{"MINGAUSSR",	CFG_INT,	&MINGAUSSR},
// -- FWHM STATS --
	{"NFWHM",		CFG_INT,	&NFWHM},
	{"FWHMSD",		CFG_DBL,	&FWHMSD},
	{"FWHMR",		CFG_INT,	&FWHMR},
	{"FWHMRF",		CFG_DBL,	&FWHMRF},
	{"FWHMSF",		CFG_DBL,	&FWHMSF},
	{"FWHMRATIO",	CFG_DBL,	&FWHMRATIO},
// -- STREAK DETECTION --
	{"STRKDEV",		CFG_DBL,	&STRKDEV},
	{"STRKRAD",		CFG_INT,	&STRKRAD},
	{"MINSTRKLEN",	CFG_INT,	&MINSTRKLEN},	
// -- SMEAR DETECTION --
	{"SMBOXW",		CFG_INT,	&SMBOXW},
	{"SMBOXH",		CFG_INT,	&SMBOXH},
	{"SAMPWD",		CFG_INT,	&SAMPWD},
	{"SAMPHT",		CFG_INT,	&SAMPHT},
	{"SMEARSD",		CFG_DBL,	&SMEARSD},
	{"DENSITY",		CFG_DBL,	&DENSITY},
	{"LOCDENS",		CFG_DBL,	&LOCDENS},
	{"DROPOFF",		CFG_DBL,	&DROPOFF},
	{"MINSMEARLEN",	CFG_INT,	&MINSMEARLEN},
	{"SMEARLONGERTOL", CFG_DBL,	&SMEARLONGERTOL},
	{"SMEARSHORTERTOL", CFG_DBL, &SMEARSHORTERTOL},
	{"ANOMPEAKMULT", CFG_DBL,	&ANOMPEAKMULT},
	{"ANOMFWHMDIFF", CFG_DBL,	&ANOMFWHMDIFF},	
// -- WCS FITTER --
	{"MAXRESID",	CFG_INT,	&MAXRESID},
	{"MAXISTARS",	CFG_INT,	&MAXISTARS},
	{"MAXCSTARS",	CFG_INT,	&MAXCSTARS},
	{"BRCSTAR",		CFG_INT,	&BRCSTAR},
#if USE_DISTANCE_METHOD
	{"MINPAIR",		CFG_INT,	&MINPAIR},
	{"MAXPAIR",		CFG_INT,	&MAXPAIR},
	{"TRYSTARS",	CFG_INT,	&TRYSTARS},
	{"MAXROT",		CFG_DBL,	&MAXROT},
	{"MATCHDIST",	CFG_DBL,	&MATCHDIST},
	{"REJECTDIST",	CFG_DBL,	&REJECTDIST},
	{"ORDER",		CFG_INT,	&ORDER},
#endif	
};
#define NIPCFG   (sizeof(ipcfg)/sizeof(ipcfg[0]))

static double getFWHMratio(CamPixel *im0, int w, int h, int x, int y);

extern void gaussfit (int pix[], int n, double *maxp, double *cenp,
    double *fwhmp);

static void starGauss (CamPixel *image, int w, int r, StarStats *ssp);
static void brightSquare (CamPixel *imp, int w, int ix, int iy, int r, int *xp,
    int *yp, CamPixel *bp);
static int brightWalk (CamPixel *imp, int w, int x0, int y0, int maxr,
    int *xp, int *yp, CamPixel *bp);

static void bestRadius (CamPixel *image, int w, int x0, int y0, int rAp,
    int *rp);
static void ringCount (CamPixel *image, int w, int x0, int y0, int r, int *np,
    int *sump);
static void ringStats (CamPixel *image, int w, int x0, int y0, int r, int *Ep,
    double *sigp);
static int skyStats (CamPixel *image, int w, int h, int x0, int y0, int r, 
    int *Ep, double *sigp);
static void circleCount (CamPixel *image, int w, int x0, int y0, int maxr,
    int *np, int *sump);

/* compute stats in the give region of the image of width w pixels.
 * N.B. we do not check bounds.
 */
void
aoiStatsFITS (ip, w, x0, y0, nx, ny, ap)
char *ip;
int w;
int x0, y0, nx, ny;
AOIStats *ap;
{
	CamPixel *image = (CamPixel *)ip;
	CamPixel *row;
	int npix, npix2;
	CamPixel maxp;
	double sd2;
	int x, y;
	int wrap;
	int i, n;

	npix = nx*ny;
	row = &image[w*y0 + x0];
	wrap = w - nx;

	memset ((void *)ap->hist, 0, sizeof(ap->hist));
	ap->sum = ap->sum2 = 0.0;
	maxp = 0;
	for (y = 0; y < ny; y++) {
	    for (x = 0; x < nx; x++) {
		unsigned long p = (unsigned)(*row++);
		ap->hist[p]++;
		ap->sum += (double) (p);
		ap->sum2 += (double)p*(double)p;
		if (p > maxp) {
		    maxp = p;
		    ap->maxx = x;
		    ap->maxy = y;
		}
	    }
	    row += wrap;
	}
	ap->maxx += x0;
	ap->maxy += y0;

	ap->mean = (CamPixel)(ap->sum/npix + 0.5);
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
}
/* copy the rectangular region [x,x+w-1,y,y+h-1] from fip to tip.
 * update header accordingly, including WCS, add CROPX/Y values for the record.
 * return 0 if ok else return -1 with a short explanation in errmsg[].
 * N.B. we assume tip has already been properly reset or inited.
 */
int
cropFITS (tip, fip, x, y, w, h, errmsg)
FImage *tip, *fip;
int x, y, w, h;
char errmsg[];
{
	static char me[] = "cropFITS";
	CamPixel *inp, *outp;
	int nbytes;
	int i, j;

	/* check that the region is wholy within fip */
	if (getNAXIS (fip, &i, &j, errmsg) < 0)
	    return (-1);
	if (x < 0 || x+w-1 > i) {
	    sprintf (errmsg, "%s: Bad AOI: x=%d w=%d sw=%d", me, x, w, i);
	    return (-1);
	}
	if (y < 0 || y+h-1 > j) {
	    sprintf (errmsg, "%s: Bad AOI: y=%d h=%d sh=%d", me, y, h, j);
	    return (-1);
	}

	/* be sure we can even get the pixel memory for tip */
	nbytes = w * h * sizeof(CamPixel);
	tip->image = malloc (nbytes);
	if (!tip->image) {
	    sprintf (errmsg, "%s: Could not malloc %d bytes for pixels", me,
								    nbytes);
	    return (-1);
	}

	/* copy the header then change NAXIS1/2 and add cropping fields. */
	copyFITSHeader (tip, fip);
	setIntFITS (tip, "NAXIS1", w, "Number columns");
	setIntFITS (tip, "NAXIS2", h, "Number rows");
	tip->sw = w;
	tip->sh = h;
	setIntFITS (tip, "CROPX", x, "X of [0,0] in original");
	setIntFITS (tip, "CROPY", y, "Y of [0,0] in original");

	/* fix up WCS too if present */
	if (!getIntFITS(fip, "CRPIX1", &i) && !getIntFITS(fip, "CRPIX2", &j)){
	    setIntFITS (tip, "CRPIX1", i-x, "RA reference pixel index");
	    setIntFITS (tip, "CRPIX2", j-y, "Dec reference pixel index");
	}

	/* copy the pixel region */
	inp = (CamPixel *) fip->image;
	inp += y*fip->sw + x;
	outp = (CamPixel *) tip->image;
	for (j = 0; j < h; j++) {
	    memcpy (outp, inp, w*sizeof(CamPixel));
	    outp += w;
	    inp += fip->sw;
	}

	return (0);
}

/* given an array of CamPixels, flip columns */
void
flipImgCols (CamPixel *img, int w, int h)
{
	int x, y;

	for (y = 0; y < h; y++) {
	    for (x = 0; x < w/2; x++) {
		CamPixel *l = &img[x];
		CamPixel *r = &img[w-x-1];
		CamPixel tmp = *l;
		*l = *r;
		*r = tmp;
	    }
	    img += w;
	}
}

/* given an array of CamPixels, flip rows */
void
flipImgRows (CamPixel *img, int w, int h)
{
	int y;
	CamPixel *tmp;

	/* Allocate the buffer instead of having it a fixed size (sto 7/20/01) */
	tmp = malloc(sizeof(CamPixel) * w);

	if(tmp == NULL) {
		printf("flipImgRows: Unable to allocate %d width buffer", w);
		exit(1);
	}

	for (y = 0; y < h/2; y++) {
	    CamPixel *top = &img[y*w];
	    CamPixel *bot = &img[(h-y-1)*w];

	    (void) memcpy ((void *)tmp, (void *)top, w*sizeof(CamPixel));
	    (void) memcpy ((void *)top, (void *)bot, w*sizeof(CamPixel));
	    (void) memcpy ((void *)bot, (void *)tmp, w*sizeof(CamPixel));
	}
	
	free(tmp);
}

/* transpose rows and columns -- effectively rotate 90 deg.  */
/* Note that you must update the header separately though! */
/* (sto 7/20/01) */
/* rotation is is CCW if dir is > 0, CW if dir <= 0 */
void transposeXY(CamPixel *img, int w, int h, int dir)
{
	/* If we were REALLY cool, we'd whip up an in-place recursive rotation algorithm here,
	   but since this has to be working in less than an hour, I'm not going to even bother
	   trying... so we allocate a new temporary bitmap here.. avoid rotating really big pictures...
	*/
	
	CamPixel * rotBuf;
	register CamPixel * ps;
	register CamPixel * pd;
	int dx,di;
	register int x,y;
	
	rotBuf = malloc(w * h * sizeof(CamPixel));
	
	if(dir <= 0) {
		dx = (h-1);
		di = -1;
	}
	else {
		dx = 0;
		di = 1;
	}
	
	for(y = 0; y < h; y++) {
		ps = img + (y*w);			// start at left of destination, from top
		pd = rotBuf + dx;			// start at top of destination, from left
		for(x = 0; x < w; x++) {
			*pd = *ps;
			ps++;					// increment source column
			pd += h;				// while incrementing destination row
		}
		dx += di;					// next across destination while next down in source
	}

	(void) memcpy ((void*)img,(void *)rotBuf, w*h*sizeof(CamPixel));
	free(rotBuf);
}

/* used to sort stars by various criteria */
typedef struct {
    int x, y;
    CamPixel b;
    StarStats ss;
} BrSt;

/* compare two BrSt wrt to brightness and return sorted in decreasing order
 * as per qsort
 */
static int
cmp_brst (const void *p1, const void *p2)
{
	BrSt *s1 = (BrSt *)p1;
	BrSt *s2 = (BrSt *)p2;
	int d = (int)(s2->b) - (int)(s1->b);

	return (d);
}

/* compare two BrSt wrt to ss.xfwhm and return sorted in increasing order
 * as per qsort
 */
static int
cmp_xfwhm (const void *p1, const void *p2)
{
	BrSt *s1 = (BrSt *)p1;
	BrSt *s2 = (BrSt *)p2;
	double d = s1->ss.xfwhm - s2->ss.xfwhm;

	return (d == 0 ? 0 : (d > 0 ? 1 : -1));
}

/* compare two BrSt wrt to ss.yfwhm and return sorted in increasing order
 * as per qsort
 */
static int
cmp_yfwhm (const void *p1, const void *p2)
{
	BrSt *s1 = (BrSt *)p1;
	BrSt *s2 = (BrSt *)p2;
	double d = s1->ss.yfwhm - s2->ss.yfwhm;

	return (d == 0 ? 0 : (d > 0 ? 1 : -1));
}

/* compute the median FWHM and std dev value in each dim of the brightest
 * NFWHM stars with SD/M > FWHMSD.
 * return 0 if ok, else put excuse in msg[] and return -1.
 */
int
fwhmFITS (im, w, h, hp, hsp, vp, vsp, msg)
char *im;		/* CamPixel data */
int w, h;		/* width/heigh of im array */
double *hp, *hsp;	/* hor median FWHM and std dev, pixels */
double *vp, *vsp;	/* vert median FWHM and std dev, pixels */
char msg[];		/* excuse if fail */
{
	int *x, *y;	/* malloced lists of star locations */
	CamPixel *b;	/* malloced list of brightest pixel in each */
	BrSt *bs;	/* malloced copy for sorting */
	int nbs;	/* total number of stars */
	BrSt *goodbs;	/* malloced copies of the good ones for stats */
	int ngoodbs;	/* actual number in goodbs[] to use */
	StarDfn sd;
	int i;

	loadIpCfg();

	/* find all the stars */
	nbs = findStars (im, w, h, &x, &y, &b);
	if (nbs < 0) {
	    sprintf (msg, "Error finding stars");
	    return (-1);
	}

	/* N.B. we are now commited to freeing x/y/b */

	if (nbs == 0) {
	    free ((char *)x);
	    free ((char *)y);
	    free ((char *)b);
	    sprintf (msg, "No stars");
	    return (-1);
	}

	/* sort by brightness */
	bs = (BrSt *) malloc (nbs * sizeof(BrSt));
	if (!bs) {
	    free ((char *)x);
	    free ((char *)y);
	    free ((char *)b);
	    sprintf (msg, "No mem");
	    return (-1);
	}
	for (i = 0; i < nbs; i++) {
	    BrSt *bsp = &bs[i];
	    bsp->x = x[i];
	    bsp->y = y[i];
	    bsp->b = b[i];
	}
	qsort ((void *)bs, nbs, sizeof(BrSt), cmp_brst);

	/* finished with x/y/b */
	free ((char *)x);
	free ((char *)y);
	free ((char *)b);

	/* use up to NFWHM brightest with SD/M > FWHMSD and x/yfwhm > 1*/
	goodbs = (BrSt *) malloc (NFWHM * sizeof(BrSt));
	sd.rsrch = 0;
	sd.rAp = FWHMR;
	sd.how = SSHOW_HERE;
	for (i = ngoodbs = 0; i < nbs && ngoodbs < NFWHM; i++) {
	    BrSt *bsp = &bs[i];
	    StarStats *ssp = &bsp->ss;
	    char buf[1024];

	    if (bsp->b < BURNEDOUT &&
		!starStats((CamPixel*)im, w, h, &sd, bsp->x, bsp->y, ssp, buf)
				    && (ssp->p - ssp->Sky)/ssp->rmsSky > FWHMSD
				    && ssp->xfwhm > 1 && ssp->yfwhm > 1)
		goodbs[ngoodbs++] = *bsp;
	}
	if (ngoodbs <= 0) {
	    sprintf (msg, "No suitable stars");
	    free ((char *)bs);
	    free ((char *)goodbs);
	    return (-1);
	}

	/* find hor median from sort by xfwhm */
	qsort ((void *)goodbs, ngoodbs, sizeof(BrSt), cmp_xfwhm);
	*hp = goodbs[ngoodbs/2].ss.xfwhm;

	/* find hor std dev */
	if (ngoodbs > 1) {
	    double sum, sum2, sd2;

	    sum = sum2 = 0.0;
	    for (i = 0; i < ngoodbs; i++) {
		double f = goodbs[i].ss.xfwhm;
		sum += f;
		sum2 += f*f;
	    }

	    sd2 = (sum2 - sum*sum/ngoodbs)/(ngoodbs-1);
	    *hsp = sd2 <= 0.0 ? 0.0 : sqrt (sd2);
	} else
	    *hsp = 0.0;

	/* find ver median from sort by yfwhm */
	qsort ((void *)goodbs, ngoodbs, sizeof(BrSt), cmp_yfwhm);
	*vp = goodbs[ngoodbs/2].ss.yfwhm;

	/* find ver std dev */
	if (ngoodbs > 1) {
	    double sum, sum2, sd2;

	    sum = sum2 = 0.0;
	    for (i = 0; i < ngoodbs; i++) {
		double f = goodbs[i].ss.yfwhm;
		sum += f;
		sum2 += f*f;
	    }

	    sd2 = (sum2 - sum*sum/ngoodbs)/(ngoodbs-1);
	    *vsp = sd2 <= 0.0 ? 0.0 : sqrt (sd2);
	} else
	    *vsp = 0.0;

#ifdef FWHM_TRACE
	printf ("nbs=%d ngoodbs=%d", nbs, ngoodbs);
	printf ("H=%4.1f %4.1f ", *hp, *hsp);
	printf ("V=%4.1f %4.1f\n", *vp, *vsp);
#endif

	free ((char *)bs);
	free ((char *)goodbs);
	return (0);
}

/* add image2 to fip1 after shifting image2 by dx and dy pixels.
 */
void
alignAdd (fip1, image2, dx, dy)
FImage *fip1;
char *image2;
int dx, dy;
{
	CamPixel *p1 = (CamPixel *) fip1->image;
	CamPixel *p2 = (CamPixel *) image2;
	CamPixel *row1, *row2;
	int x10, y10;	/* starting coords in p1 */
	int x20, y20;	/* starting coords in p2 */
	int nx, ny;	/* size of overlap area */
	int wrap;
	int x, y;

	if (dx > 0) {
	    x10 = dx;
	    x20 = 0;
	    nx = fip1->sw - dx;
	} else {
	    x10 = 0;
	    x20 = -dx;
	    nx = fip1->sw + dx;
	}
	wrap = fip1->sw - nx;

	if (dy > 0) {
	    y10 = dy;
	    y20 = 0;
	    ny = fip1->sh - dy;
	} else {
	    y10 = 0;
	    y20 = -dy;
	    ny = fip1->sh + dy;
	}

#ifdef ADDTRACE
	printf ("x10=%d y10=%d  nx=%d x20=%d y20=%d  ny=%d\n", x10, y10,
							    nx, x20, y20, ny);
#endif

	row1 = &p1[fip1->sw*y10 + x10];
	row2 = &p2[fip1->sw*y20 + x20];
	for (y = 0; y < ny; y++) {
	    for (x = 0; x < nx; x++) {
		int sum = (int)(*row1) + (int)(*row2++);
		*row1++ = sum > MAXCAMPIX ? MAXCAMPIX : sum;
	    }
	    row1 += wrap;
	    row2 += wrap;
	}
}


/* given a CamPixel array of size wXh, a StarDfn and an initial location ix/iy,
 *   find stats of star and store in StarStats.
 * return 0 if ssp filled in ok, else -1 and errmsg[] if trouble.
 */
int
starStats (image, w, h, sdp, ix, iy, ssp, errmsg)
CamPixel *image;		/* array of pixels */
int w, h;			/* width and height of image */
StarDfn *sdp;			/* star search parameters definition */
int ix, iy;			/* initial guess of loc of star */
StarStats *ssp;			/* what we found */
char errmsg[];			/* disgnostic message if return -1 */
{
	int maxr;		/* max radius we ever touch */
	CamPixel bp;		/* brightest pixel */
	int bx, by;		/* location of " */
	int N;			/* total pixels in circle */
	int C;			/* total count of pixels in circle */
	int E;			/* median pixel in sky annulus */
	double rmsS;		/* rms noise estimate of sky annulus */
	int rAp;
	int ok;

	loadIpCfg();

	/* 1: confirm that we are wholly within the image */
	maxr = sdp->rAp;
	switch (sdp->how) {
	case SSHOW_BRIGHTWALK:
	case SSHOW_MAXINAREA:
	    maxr += sdp->rsrch;
	    break;
	default:
	    break;
	}
	if (ix - maxr < 0 || ix + maxr >= w || iy - maxr < 0 || iy + maxr >= h){
	    sprintf (errmsg,
	    	"Coordinates [%d,%d] + search sizes lie outside image", ix, iy);
	    return (-1);
	}

	/* 2: find the brightest pixel, in one of several ways */
	switch (sdp->how) {
	case SSHOW_BRIGHTWALK:
	    /* walk the gradient starting at ix/iy to find the brightest
	     * pixel. we never go further than sdp->rb away.
	     */
	    ok = brightWalk (image, w, ix, iy, sdp->rsrch, &bx, &by, &bp) == 0;
	    break;

	case SSHOW_MAXINAREA:
	    /* centered at ix/iy search the entire square of radius sdp->rb
	     * for the brightest pixel
	     */
	    brightSquare (image, w, ix, iy, sdp->rsrch, &bx, &by, &bp);
	    ok = 1;
	    break;

	case SSHOW_HERE:
	    /* just use ix and iy directly */
	    bx = ix;
	    by = iy;
	    bp = image[iy*w + ix];
	    ok = 1;
	    break;

	default:
	    printf ("Bug! Bogus SSHow code: %d\n", sdp->how);
	    exit (1);
	}

	if (!ok) {
	    sprintf (errmsg, "No brightest pixel found");
	    return (-1);
	}

	ssp->bx = bx;
	ssp->by = by;
	ssp->p = bp;

#ifdef STATS_TRACE
	printf ("Brightest pixel is %d at [%d,%d]\n", ssp->p, bx, by);
#endif

	/* 3: if not handed an aperture radius, find one.
	 * in any case, enforce MINAPRAD.
	 */
	if ((rAp = sdp->rAp) == 0) {
	    int r = maxr < DEFSKYRAD ? maxr : DEFSKYRAD;
	    bestRadius (image, w, bx, by, r, &rAp);
#ifdef STATS_TRACE
	    printf ("  Best Aperture radius = %d\n", rAp);
	} else {
	    printf ("  Handed Aperture radius = %d\n", rAp);
#endif
	}
	if (rAp < MINAPRAD)
	    rAp = MINAPRAD;

	/* 4: find noise in thick annulus from radius rAp+APGAP out until
	 * use PI*rAp*rAp*APSKYX pixels.
	 */
	if (skyStats (image, w, h, bx, by, rAp, &E, &rmsS) < 0) {
	    sprintf (errmsg, "bad skyStats");
	    return (-1);
	}
	ssp->Sky = E;
	ssp->rmsSky = rmsS;
	ssp->rAp = rAp;
#ifdef STATS_TRACE
	printf ("  Sky=%d rmsSky=%g rAp=%d\n", ssp->Sky, ssp->rmsSky, ssp->rAp);
#endif

	/* 5: find pixels in annuli out through rAp */
	circleCount (image, w, bx, by, rAp, &N, &C);
	ssp->Src = C - N*E;
	ssp->rmsSrc = sqrt(N*rmsS + ssp->Src/TELGAIN);
#ifdef STATS_TRACE
	printf ("  Src=%d rmsSrc=%g\n", ssp->Src, ssp->rmsSrc);
#endif

	/* 6: finally, find the gaussian params too */
	starGauss (image, w, ssp->rAp, ssp);

	/* ok */
	return (0);
}

/* find relative mag (and error estimate) of target, t, wrt reference, r.
 * return 0 if ok, -1 if either source was actually below its noise, in which
 * case *mp is just the brightest possible star, and *dmp is meaningless.
 * 
 * Based on Larry Molnar notes of 6 Dec 1996
 */
int
starMag (r, t, mp, dmp)
StarStats *r, *t;
double *mp, *dmp;
{
	if (t->Src <= 0 || t->Src <= t->rmsSrc || r->Src <= r->rmsSrc) {
	    /* can happen when doing stats from pure noise */
	    *mp = 2.5*log10((double)r->Src / (double)t->rmsSrc);
	    *dmp = 99.99;
	    return (-1);
	} else {
	    double er = r->rmsSrc / r->Src;
	    double et = t->rmsSrc / t->Src;

	    *mp = 2.5*log10((double)r->Src / (double)t->Src);
	    *dmp = 1.0857*sqrt(er*er + et*et);
	    return (0);
	}
}

/* support for bWalk */
#define	BW_FANR	2
#define	BW_NFAN	((2*BW_FANR+1)*(2*BW_FANR+1)-1)
static int bW_w, bW_h;
static int *bW_fan;
static int bW_thresh;
static CamPixel *bW_im;
static CamPixel *bW_bp;

/* scanning around bp, set bW_bp to the brightest member of bW_fan.
 */
static int
bWalk (CamPixel *bp)
{
	int x = (bp-bW_im)%bW_w;
	int y = (bp-bW_im)/bW_w;
	int i, bf;

	if (*bp > BURNEDOUT)
	    return(-1);

	if (x < FSBORD || x > bW_w-FSBORD || y < FSBORD || y > bW_h-FSBORD)
	    return(-1);

	for (bf = i = 0; i < BW_NFAN; i++)
	    if (bp[bW_fan[i]] > bp[bf])
		bf = bW_fan[i];

	if (bf == 0) {
	    bW_bp = bp;
	    return (0);
	} else
	    return (bWalk (bp + bf));
}

/* given an array of n y-values for x-values starting at xbase and incremented
 * by step, return the interpolated value of y at x.
 */
static int
linInterp(int xbase, int step, int yarr[], int n, int x)
{
	int idx, x0, y0, y1;

	idx = (x-xbase)/step;
	if (idx > n-2)
	    idx = n-2;
	x0 = xbase + idx*step;
	y0 = yarr[idx];
	y1 = yarr[idx+1];
	
	return (((double)(x)-x0)*(y1-y0)/step + y0);
}

/* find signal threshold in given box of given image */
static void
findThresh (CamPixel *im0, int imw, int boxw, int boxh, int *tp)
{
	int halfnpix = boxw*boxh/2;
	int wrap = imw - boxw;
	double sum, sum2, sd, sd2;
	double thresh;
	CamPixel *p;
	int median;
	int i, j, n;
	int t, b;

	/* find median using binary search.
	 * much faster than hist method for small npix.
	 */
	t = MAXCAMPIX;
	b = 0;
	while (b <= t) {
	    median = (t+b)/2;

	    p = im0;
	    n = 0;
	    for (i = 0; i < boxh; i++) {
		for (j = 0; j < boxw; j++) {
		    if (*p++ > median) {
			if (++n > halfnpix) {
			    b = median+1;
			    goto toolow;
			}
		    }
		}
		p += wrap;
	    }

	    if (n == halfnpix)
		break;
	    t = median-1;
	toolow: ;
	}

	/* find SD of non-0 pixels below the median */
	sum = sum2 = n = 0;
	p = im0;
	for (i = 0; i < boxh; i++) {
	    for (j = 0; j < boxw; j++) {
		int pj = (int)(*p++);
		b = median - pj;
		if (pj && b >= 0) {
		    sum += b;
		    sum2 += (double)b*b;
		    n++;
		}
	    }
	    p += wrap;
	}
	sd2 = n > 1 ? (sum2 - sum * sum/n)/(n-1) : 0;
	sd = sd2 <= 0.0 ? 0.0 : sqrt(sd2);

	thresh = median + FSMINSD*sd;
	*tp = thresh > MAXCAMPIX ? MAXCAMPIX : thresh;
}

/* scan around peak and count the number of contiguous neighbors above thresh.
 */
static int
connected (CamPixel *peak, int w, int thresh)
{
	static int lastw = -1;
	static int fan[8];
	int i, n;

	if (lastw != w) {
	    fan[0] = -1;
	    fan[1] = -w-1;
	    fan[2] = -w;
	    fan[3] = -w+1;
	    fan[4] = 1;
	    fan[5] = w+1;
	    fan[6] = w;
	    fan[7] = w-1;
	    lastw = w;
	}

	for (n = i = 0; i < 8+FSMINCON; i++) {
	    if (peak[fan[i%8]] > thresh) {
			if (++n >= FSMINCON) {
			    return (0);
			}
	    } else {
			n = 0;
		}
	}
	return (-1);
}

/* scan around peak return the average pixel value
 */

static int
ringAvg (CamPixel *peak)
{
	static int lastw = -1;
	static int fan[8];
	int i, n;

	if (lastw != bW_w) {
	    fan[0] = -1;
	    fan[1] = -bW_w-1;
	    fan[2] = -bW_w;
	    fan[3] = -bW_w+1;
	    fan[4] = 1;
	    fan[5] = bW_w+1;
	    fan[6] = bW_w;
	    fan[7] = bW_w-1;
	    lastw = bW_w;
	}

	for (n = i = 0; i < 8; i++) {
		n +=peak[fan[i]];
	}
	return (n/8);
}

////////////////////////////////////////////////////////////////////////////////////////////

// size of block (both width and height)
#define BLOCK_WH 5
#define BLOCKSIZE (BLOCK_WH * BLOCK_WH)

// NOTE: Block code uses bW_ (bWalk) static constants also.
// Assumed to be called when star finder gives us a qualified peak after doing bWalk.

#define pixelX(addr) ((addr-bW_im)%bW_w)
#define pixelY(addr) ((addr-bW_im)/bW_w)

// Calculate the threshold of the pixels within this block
static int blockThresh(CamPixel *addr)
{
	int thresh;
	findThresh(addr,bW_w,BLOCK_WH,BLOCK_WH,&thresh);
	return thresh;		
}

// Walk in current direction selecting the brightest neighbor
// that is in the fan of direction, preferring the one directly ahead
// but only those surrounded by neighboring pixels above threshold
// allow backwards directions (4-7) also
/* Directions are:
		5 6 7
		4   0
		3 2 1
*/	
static void blockWalk(CamPixel **pAddr, int dir, int dump)
{
	CamPixel *addr = *pAddr;
	// check for values above noise level that qualified us for starting
	int thresh = bW_thresh;
	
	// only need to set this up once
	static int blockmap[BLOCKSIZE];
	static int oldW;
	if(oldW != bW_w) {
		int i;
		for(i=0; i<BLOCKSIZE; i++) {
			blockmap[i] =  ((i/BLOCK_WH)-(BLOCK_WH/2))*bW_w + ((i%BLOCK_WH)-(BLOCK_WH/2));		
		}
		oldW = bW_w;
		if(dump) printf("Made block map\n");
	}
	
	while(1) {

		// Check for brightest pixel with qualifying neighbors in this direction
		int brightest = 0;
		int brightIdx = -1;
		int i;
						
		for(i=0; i<BLOCKSIZE; i++) {
			int x = i%BLOCK_WH - BLOCK_WH/2;
			int y = i/BLOCK_WH - BLOCK_WH/2;
			int t;
			if(!x && !y) continue;
			switch(dir) {		
				case 0:
					t = (x > 0 && abs(y) < 2);
					break;
				case 1:
					t = (x >=0 && y>=0 && abs(x-y) < 2);
					break;
				case 2:
					t = (y > 0 && abs(x) < 2);
					break;
				case 3:
					t = (x <=0 && y>=0 && abs(x-y) < 2);
					break;
				case 4:
					t = (x < 0 && abs(y) < 2);
					break;
				case 5:
					t = (x <=0 && y<=0 && abs(x-y) < 2);
					break;
				case 6:
					t = (y < 0 && abs(x) < 2);
					break;
				case 7:
					t = (x >=0 && y<=0 && abs(x-y) < 2);
					break;
				default:
					t = 0;
					break;
			}
			if(t) {	
				CamPixel *t = &addr[blockmap[i]];
				// make sure we're bright enough ourselves!
				if(*t > thresh) {
					// make sure we're connected to qualified neighbors
					if(connected(t,bW_w,thresh) >= 0) {
						int v = *t + ringAvg(t);
						if(v > brightest) {
							brightest = v;
							brightIdx = i;
							if(dump) printf("(dir %d): bright at %d,%d :%d\n",dir,
											pixelX(addr),pixelY(addr),*addr);
						}
					}
				}
			}
		}
		if(brightIdx >= 0) {
		    // move block to brightest found
			addr = &addr[blockmap[brightIdx]];			
			
			// check the border
			{
			    int x = pixelX(addr);
			    int y = pixelY(addr);

				if((x < FSBORD || x >= bW_w-FSBORD)
				|| (y < FSBORD || y >= bW_h-FSBORD)) {
					break; // done walking if we hit edge
				}
			}
						
		}
		else break; // done walking
	}
	
	*pAddr = addr;
}	
			
// Return the pixel distance between two blocks
static int pixelDist(CamPixel *addr1, CamPixel *addr2)
{
	int dx, dy;
	dx = pixelX(addr2) - pixelX(addr1);
	dy = pixelY(addr2) - pixelY(addr1);
	return (int) (0.5 + sqrt(dx*dx+dy*dy));
}

// Return >0 if the given point can be found in the streak list
// If start point collides, return 1, otherwise return 2.
// If no collision, return 0.
// Check for the same start pixel first, then check for an intercept
// with an existing streak
int IsPointWithinStreakList(int x, int y, StreakData *streakList, int nstreaks)
{
	int i;
	int l,t,r,b;
	
	l = x - FSMINSEP;
	t = y - FSMINSEP;
	r = x + FSMINSEP;
	b = y + FSMINSEP;
	
	for (i = nstreaks;  --i >= 0 /*&& streakList[i].endY >= t*/; ) { // must read whole list!
			
	    if (abs(streakList[i].startX-x)<=FSMINSEP
	    && abs(streakList[i].startY-y)<=FSMINSEP) {
	    	return 1; // start is already on list
	    }
	    if(streakList[i].length && streakList[i].endY >= t && streakList[i].startY <= b) {
	    	int sl,sr;
	    	int st = streakList[i].startY;
	    	int sb = streakList[i].endY;
	    	if(streakList[i].startX < streakList[i].endX) {
	    		sl = streakList[i].startX;
	    		sr = streakList[i].endX;
	    	} else {
	    		sr = streakList[i].startX;
	    		sl = streakList[i].endX;
	    	}
	    	if(r >= sl && l <=sr && b >= st && t <= sb) {
	    		int yint;	    		
	    		if(sr - sl <= FSMINSEP) {
	    			return 1; // vertical (or at least narrow enough!)
	    		}
	    		if(sb - st <= FSMINSEP) {
	    			return 1; // horizontal (or at least narrow enough!)
	    		}
	    		// we now have to do an intercept to see if we collide
				yint = st + ((x - streakList[i].startX) * streakList[i].slope);
				if(yint >=t && yint <=b) {
					return 1;
				}
			}
		}
	    			    	
	}
	return(0);
	
}

/*
 * This is the entry point that is called by the star finder for finding streaks.
 * We assume that bWalk has been called prior to this and that the bW_ static variables
 * contain the current peak and threshold data.
 *
 * We will call blockWalk and analyze pixels surrounding contiguous peaks and
 * break when we exhaust the trail in each of 5 directions.  Since we know the
 * star finder is scanning left-to-right and top-down, we only need to look
 * Right, Down-right, Down, and Down-left to trace any point given us.
 * We also go Left (direction 4), because a long low-slope streak may be too shallow to trace
 * with direction 3.
 *
 * We pick the run with the longest length as the direction for the streak.
 *
 * Further qualification of whether or not this is really a streak comes later.
 *
 * Return value is length of streak, or 0 if a star is found, or -1 if there was an error or rejection
 * Endpoints are returned via startx, starty, and endx, endy (if not null)
 *
 * Passing 1 for 'dump' will output debug trace information
 */
int walkStreak(int *startx, int *starty, int *endx, int *endy, int dump)
{
	CamPixel * startAddr;
	CamPixel * endAddr[5];
	CamPixel * streakStartAddr;
	int baseThresh;
	int dir;
	int length[5];
	int longest,longdir;
			
	// start address is the peak found by last bWalk
	startAddr = streakStartAddr = bW_bp;
				
	// reject if values are too low
	baseThresh = blockThresh(startAddr);	
	// bW_thresh is the dark threshold at this location
	if(baseThresh < bW_thresh) {
		if(dump) printf("threshold rejection:%d < %d\n",	baseThresh,bW_thresh);
		return -1;
	}
	
	// reject if starting peak is not connected to anything
	if(connected(startAddr,bW_w,bW_thresh) < 0) {
		if(dump) printf("connection rejection\n");
		return -1;
	}
		
	// Now walk in each direction and record the results
	for(dir=0; dir<5; dir++) {
		endAddr[dir] = startAddr;
		blockWalk(&endAddr[dir],dir,dump);
	}		
	
	// Find the longest
	longest = longdir = 0;
	if(dump) printf("<");
	for(dir=0; dir<5; dir++) {
		if(pixelY(endAddr[dir]) < pixelY(startAddr) ) {
			length[dir] = 0; // reject any retrograde traces
		} else {
			length[dir] = pixelDist(startAddr,endAddr[dir]);		
			if(length[dir] > longest) {
				longest = length[dir];
				longdir = dir;
			}
			if(dump) printf(" %d",length[dir]);
		}
	}
	if(dump) printf(" >\n");
	
	// trace back from start in opposite direction to find outer edge
	if(dump) printf("tracing back to start\n");
	streakStartAddr = startAddr;
	blockWalk(&streakStartAddr,(longdir+4)%8,dump);
	
	if(dump)
	printf("***** {s} Seeded:%d,%d == Start: %4d, %4d  End: %4d, %4d  Length: %d\n",
			pixelX(startAddr),pixelY(startAddr),
			pixelX(streakStartAddr),pixelY(streakStartAddr),
			pixelX(endAddr[longdir]), pixelY(endAddr[longdir]),
			longest);
			
	if(startx) *startx = pixelX(streakStartAddr);
	if(starty) *starty = pixelY(streakStartAddr);
	if(endx) *endx = pixelX(endAddr[longdir]);
	if(endy) *endy = pixelY(endAddr[longdir]);
	return longest;
}

/*
 * Compute the endpoint of a line with given slope and length from start point
 */
static void slopeLength(int x, int y, int length, double slope, int *outX, int *outY)
{
	double m = fabs(slope);
	int dx,dy;
	if(m < 1.0) {
		dy = length * m;
		dx = length;
	} else {
		dx = length / m;
		dy = length;
	}	
	if(slope < 0) dx = -dx;
	*outX = x+dx;
	*outY = y+dy;
}


/*
 * Walk the width of a streak at the given position.
 * This will measure the distance and average brightness
 * at different cross sections of the streak being qualified.
 */
typedef struct {
	int width;
	int bright;
} SEGINFO;

int dbon = 0;
static void widthWalk(CamPixel *addr, double slope, int thresh, SEGINFO *pSegment)
{
	int x,y,i,val,sumval;
	int w1,w2, b1,b2;
	int x1 = pixelX(addr);
	int y1 = pixelY(addr);
	i = 0;
	sumval = *addr;
	if(dbon) printf("first walk @ %d,%d\n",x1,y1);
	while(1) {
		i++;
		// walk the perpendicular of the slope
		if(fabs(slope) < 1) {
			y = -i;
			x = i*slope;
		} else {
			x = -i;
			y = i/fabs(slope);
			if(slope < 0) x = -x;
		}				
		
		if (x1+x < FSBORD || x1+x > bW_w-FSBORD || y1+y < FSBORD || y1+y > bW_h-FSBORD)
			break;
		
		val = addr[y*bW_w+x];
		if(dbon)	printf("%d,%d = %d\n",x1+x,y1+y,val);
		if(val < thresh) break;
		sumval += val;
	}
	w1 = i-1;
	b1 = sumval / i;
	
	i = 0;
	sumval = *addr;
	if(dbon)	printf("second walk\n");
	while(1) {
		i++;
		if(fabs(slope) < 1) {
			y = i;
			x = -i*slope;
		} else {
			x = i;
			y = -i/fabs(slope);
			if(slope < 0) x = -x;
		}				
		if (x1+x < FSBORD || x1+x > bW_w-FSBORD || y1+y < FSBORD || y1+y > bW_h-FSBORD)
			break;
		val = addr[y*bW_w+x];
		if(dbon) 	printf("%d,%d = %d\n",x1+x,y1+y,val);
		if(val < thresh) break;
		sumval += val;
	}
	w2 = i-1;
	b2 = sumval / i ;
	
	// combine these values and return them
	if(pSegment) {
		pSegment->width = w1+w2+1;
		pSegment->bright = (b1+b2)/2;
	}
}

/*
 * Perform a crude shape-and-brightness analysis of the
 * streak and record this profile in the "flags" field of the streak data
 */

static int qualifyStreakData(CamPixel *im0, int w, StreakData *pStr)
{
	// walk the streak from end to end
	// gather up the pixels along this nominal spine	
	int i,j,k;
	int nval = 0;
	int x,y,val;
	int min,max;
	double median,mean,stdDev;
	int thresh,seglen;
	int wfloor,bfloor,rfloor;
	int wbits,bbits,rbits,qual;
	SEGINFO segInfo[NSEG];
	CamPixel *addr;
	double *pval;
	
	int *psegval[NSEG];
	int nsegval[NSEG];
	int midval[NSEG];
	int seg;
	
	int summary = STREAK_YES; // naively optimistic
		
/*	
	int targx = 1492;
	int targy = 125;
	int rad = 10;
	if(pStr->walkStartX >= targx-rad && pStr->walkStartX <= targx+rad
	&& pStr->walkStartY >= targy-rad && pStr->walkStartY <= targy+rad) {
		dbon = 1;
	} else {
		dbon = 0;
	}
*/

	if(dbon)	printf("in qualify streak\n");
	
	if(pStr->length < MINSTRKLEN) {
		rbits = wbits = bbits = 0;
		summary = STREAK_NO;	// TOO SHORT
		goto setFlags;		// uck a goto! yeah yeah...
	}
		
	// allocations	
	pval = (double *) malloc(pStr->length * sizeof(double));			
	
	seglen = pStr->length/NSEG;
	for(i=0; i<NSEG; i++) {
		psegval[i] = (int *) malloc((seglen+1) * sizeof(int));
		nsegval[i] = 0;
	}
	
	max = 0;
	min = MAXCAMPIX;
	for(i=0; i<pStr->length; i++) {
		if(fabs(pStr->slope) < 1) {
			y = i*fabs(pStr->slope);
			x = i;
			if(pStr->slope < 0) x = -x;
		} else {
			y = i;
			x = i/pStr->slope;
		}
	
		x += pStr->walkStartX;
		y += pStr->walkStartY;
		val = im0[y*w+x];
		if(dbon)	printf("spinewalk: %d, %d = %d\n",x,y,val);
		
		if(val < min) min = val;
		if(val > max) max = val;
				
		// insert into value list, sorted
		for(j=0; j<nval && pval[j] > val; j++);
		for(k=nval; k>j; k--) {
			pval[k] = pval[k-1];
		}
		pval[j] = val;
		nval++;
		
		// add to segment lists
		seg = i/(seglen+1);
		for(j=0; j<nsegval[seg] && psegval[seg][j] > val; j++);
		for(k=nsegval[seg]; k>j; k--) {
			psegval[seg][k] = psegval[seg][k-1];
		}
		psegval[seg][j] = val;
		nsegval[seg]++;				
		
	}
	// get medians of segments
	for(seg=0; seg<NSEG; seg++) {
		midval[seg] = psegval[seg][nsegval[seg]/2];
		if(!(nsegval[seg] & 1)) {
			midval[seg] += psegval[seg][(nsegval[seg]/2)+1];
			midval[seg] /=2;
		}
		free(psegval[seg]);
	}
	
	// Get the stats for the brightness profile
	getStats(pval,nval,&mean,&median,&stdDev);
	free(pval);
	
	if(dbon) printf("Stats: min: %d  max: %d  mean: %lf, median: %lf, sd: %lf\n",
			min,max,mean,median,stdDev);
	thresh = median - (median-min)/4;
	if(dbon) printf("Thresh: %d\n",thresh);
	
	// Divide the streak into segments
	// record the measurements and the brightness at each point
	for(i=0; i<NSEG; i++) {
		slopeLength(pStr->walkStartX,pStr->walkStartY,i*seglen,pStr->slope,&x,&y);
		// get the address at this point
		addr = &im0[y*w+x];
		// walk the widths at this segment
		widthWalk(addr, pStr->slope, thresh, &segInfo[i]);
	}
	// reduce this to pass/fail evaluations of "wide" and "bright"
	// -- use two metrics for 'wide' -- this will help qualify round objects better
	rfloor = (pStr->length *0.42);	// 'round'
	wfloor = (pStr->length *0.27);	// 'wide'
	bfloor = median;
	if(dbon) printf("wfloor: %d  bfloor: %d  rfloor: %d\n",wfloor,bfloor,rfloor);
	wbits = bbits = rbits = 0;
	for(i=0; i<NSEG; i++) {
		wbits <<= 1;
		bbits <<= 1;
		if(dbon) printf("%d) w: %d  mv: %d   b: %d\n",i,segInfo[i].width,midval[i],segInfo[i].bright);
		if(segInfo[i].width > rfloor) rbits++;
		if(segInfo[i].width > wfloor)  wbits++;
		if(segInfo[i].width > 1) {
			if(midval[i] > bfloor) bbits++;
			else if(segInfo[i].bright > bfloor) bbits++;
		}
	}

	// now summarize the qualifiers
	if(rbits) {
		summary = STREAK_NO;	// too round
	} else if(wbits) {
		summary = STREAK_MAYBE;	// too thick, but maybe...
	}
	if(summary != STREAK_NO) {
		#if NSEG == 5
		if(bbits == 0x11
		|| bbits == 0x13
		|| bbits == 0x1B
		|| bbits == 0x19
		|| bbits == 0x1D
		|| bbits == 0x0D
		|| bbits == 0x09
		#elif NSEG == 4
		if(bbits == 0x05
		|| bbits == 0x09
		|| bbits == 0x0A
		|| bbits == 0x0B
		|| bbits == 0x0D
		#elif NSEG == 3
		if(bbits == 0x05
	    #endif
   		) {
			summary = STREAK_MAYBE;			
		}		
	}
	if(summary == STREAK_MAYBE) {
		// further qualify for the maybes
		int dx,dy,len;
		dx = pStr->endX - pStr->startX;
		dy = pStr->endY - pStr->startY;
		len = sqrt(dx*dx+dy*dy);
		if(len < MINSTRKLEN) {
			summary = STREAK_NO; // nah.  Not maybe.
		}
	}

setFlags:	
	qual = ((rbits<<(NSEG*3))|(wbits<<(NSEG*2))|(bbits<<(NSEG))|summary);
	if(dbon) printf("% 4d,% 4d ---- %03X\n",pStr->startX,pStr->startY,qual);
	
	return qual;
}

/************************************************************************************/

/*****************\
Smear walking support
\******************/
static int minSmearLength, maxSmearLength; 	// set by calculateSmearLength for use by smear detector
static int smearHijackFindStars; 			// really hacky flag set that will re-route findstars calls (from wcs presumably)
											// to get data from smear table

// Compute the expected length of a smear based on exposure time and pixel size
int calculateSmearLength(FImage *fip)
{
	double cdelt1,expTime;
	double degSmear;
	int	pixSmear;
	
	if(0 != getRealFITS(fip,"CDELT1",&cdelt1)) {
		printf("No CDELT1 keyword found -- cannot compute expected smear length\n");
		return -1;
	}
	if(0 != getRealFITS(fip,"EXPTIME",&expTime)) {
		printf("No EXPTIME keyword found -- cannot compute expected smear length\n");
		return -1;
	}
	
	degSmear = (360.0/SPD)*expTime; // degrees stars moved during the exposure
	pixSmear = (int) fabs(degSmear/cdelt1);
	
	// calculate the range we will accept
	minSmearLength = pixSmear - pixSmear * SMEARSHORTERTOL;
	maxSmearLength = pixSmear + pixSmear * SMEARLONGERTOL;
	
//	printf("Computed an estimated smear length of %d, set min/max to %d - %d\n",pixSmear,minSmearLength,maxSmearLength);
	
	return pixSmear;
}

typedef struct
{
	double 		density;	// density ratio of pixels > thresh
	long		sum;		// sum of pixels > thresh
	CamPixel	mean;		// average value of pixels > thresh
	CamPixel	min;		// min pixel > thresh
	CamPixel	max;		// max pixel > thresh
	
} GridStats;

/* Compute the grid statistics for the given area */
void sampleGrid(CamPixel **pAddr, int thresh, int columns, int rows, GridStats *gs )
{
	int x,y,count = 0;
	int numPix = rows * columns;
	long sum = 0;
	CamPixel min = MAXCAMPIX;
	CamPixel max = 0;
	CamPixel *addr = *pAddr;
	y = rows;
	while(y--) {
		x = columns;
		while(x--) {
			if(*addr > thresh) {
				CamPixel v = *addr - thresh;
				++count;
				sum += v;
				if(v > max) max = v;
				else if(v < min) min = v;
			}
			addr++;
		}	
		addr += bW_w - columns;
	}
	gs->density =  (double) count / (double) numPix;
	gs->sum = sum;
	gs->mean = sum/numPix;
	gs->min = min;
	gs->max = max;
}

#define SMEARINIT 	500	// number of initial table entries
#define SMEARADD	50	// number to add on each expansion
SmearData *smearTable;
int numEntries;
int smearAlloc;

int initSmearTable(void)
{
	int size = SMEARINIT * sizeof(SmearData);
	smearTable = (SmearData *) malloc(size);
	if(!smearTable) return -1;
	memset(smearTable,0,size);
	smearAlloc = SMEARINIT;
	numEntries = 0;
	
	return 0;
}

// Define our rectangle structure
typedef struct
{
	int	left;
	int top;
	int right;
	int bottom;
} Rect, *RectPtr;

void SetRect(RectPtr pRect, int l, int t, int r, int b)
{
	if(pRect) {
		pRect->left = l;
		pRect->top = t;
		pRect->right = r;
		pRect->bottom = b;
	}
}
int IntersectRect(RectPtr pRectOut, RectPtr pRect1, RectPtr pRect2)
{
	register RectPtr r1 = pRect1;
	register RectPtr r2 = pRect2;
	register RectPtr d = pRectOut;
 	return (
         (
          (d->left = ((r1->left > r2->left) ? r1->left : r2->left)) <
          (d->right = ((r1->right < r2->right) ? r1->right : r2->right))
         )
        &
         (
          (d->top = ((r1->top > r2->top) ? r1->top : r2->top)) <
          (d->bottom = ((r1->bottom < r2->bottom) ? r1->bottom : r2->bottom))
         )
    );
}
int RectWidth(RectPtr r)
{
	if(r) {
		return(r->right - r->left);
	}
	return(0);
}
int RectHeight(RectPtr r)
{
	if(r) {
		return(r->bottom - r->top);
	}
	return(0);
}

// Record an object in the smear list.  This will sort by Y.
// A smear has a length; an anomoly is recorded with zero length
int recordSmearObject(int x, int y, int length, int bright)
{		
	int 		i,j;
	SmearData * pEntry;
	
	// didn't find a match
	// Make a new entry for this data
	// do an insertion sort by Y position
	i=0;
	while(i < numEntries) {
		if(y < smearTable[i].starty) {
			break;
		}
		i++;
	}
	j = numEntries;
	while(j > i) {
		smearTable[j] = smearTable[j-1];
		j--;
	}		
	pEntry = &smearTable[i];	
	pEntry->startx = x;
	pEntry->starty = y;
	pEntry->length = length;
	pEntry->bright = bright;
//	printf("adding entry %d (pos %d) at %d, %d length=%d, bright=%d\n", numEntries, i, x,y, length,bright);	
	numEntries++;
	if(numEntries >= smearAlloc) {
//		printf("Growing smearAlloc\n");	
		smearAlloc += SMEARADD;
		smearTable = (SmearData *) realloc(smearTable, smearAlloc * sizeof(SmearData));
		if(!smearTable) return -1;
//		printf("numEntries = %d, next alloc at %d\n",numEntries, smearAlloc);	
	}
	
	return 0;
}	
// record a smear, but only those with qualified lengths
int recordSmear(int x, int y, int length, int bright)
{	
	if(length < minSmearLength || length > maxSmearLength) {
		return 0;
	}
	
	return recordSmearObject(x, y, length, bright);
}
// Record an anomoly as an object with zero length
int recordAnomoly(int x, int y, int bright)
{
	return recordSmearObject(x, y, 0, bright);
}

int isRectInSmearList(int x, int y, int width, int length)
{
	int i = 0;
	Rect newRect,entryRect,resultRect;
	SetRect(&newRect, x, y, x+width, y+length);
		
	while(i < numEntries) {
		if( y+length < smearTable[i].starty) return 0; // nope, none here... remaining entries are all higher
		
		SetRect(&entryRect,smearTable[i].startx,smearTable[i].starty,
						   smearTable[i].startx+smearTable[i].length,
						   smearTable[i].starty+length);
						
		if(IntersectRect(&resultRect,&newRect,&entryRect)) {
			return 1;
		}
		i++;
	}
	return 0;	
}
int isSectionInSmearList(int x, int y)
{
	return isRectInSmearList(x,y,SAMPWD,MAXSMEARWIDTH);
}

/* Dump the results of the table (Debugging) */
void dumpTable(void)
{
	int i;
	SmearData * pEntry;
	for(i=0; i<numEntries; i++) {
		pEntry = &smearTable[i];
		printf("% 3d) %d, %d (%d) [%d]\n",i,pEntry->startx,pEntry->starty, pEntry->length, pEntry->bright);
	}
		
}

// Convert smear data into findstars array triplet, return count
int returnSmearStars(int **xa, int **ya, CamPixel **ba)
{
	int *xp, *yp;
	CamPixel *bp;
	int i;
	SmearData * pEntry;
	
	xp = (int *) malloc(numEntries * sizeof(int));
	yp = (int *) malloc(numEntries * sizeof(int));
	bp = (CamPixel *) malloc(numEntries * sizeof(CamPixel));

	for(i=0; i<numEntries; i++) {
		pEntry = &smearTable[i];
		xp[i] = pEntry->startx;
		yp[i] = pEntry->starty;
		bp[i] = pEntry->bright;
	}

	*xa = xp;
	*ya = yp;
	*ba = bp;		
	
	return numEntries;
}

// Find the local threshold by computing within a very local noise box along supposed smear
// x,y are top-left corner
static void
findThresh2 (CamPixel *im0, int imw, int boxw, int boxh, int *tp)
{
	int halfnpix = boxw*boxh/2;
	int wrap = imw - boxw;
	double sum, sum2, sd, sd2;
	double thresh;
	CamPixel *p;
	int median;
	int i, j, n;
	int t, b;

	/* find median using binary search.
	 * much faster than hist method for small npix.
	 */
	t = MAXCAMPIX;
	b = 0;
	while (b <= t) {
	    median = (t+b)/2;

	    p = im0;
	    n = 0;
	    for (i = 0; i < boxh; i++) {
		for (j = 0; j < boxw; j++) {
		    if (*p++ > median) {
			if (++n > halfnpix) {
			    b = median+1;
			    goto toolow;
			}
		    }
		}
		p += wrap;
	    }

	    if (n == halfnpix)
		break;
	    t = median-1;
	toolow: ;
	}

	/* find SD of non-0 pixels above the median */
	sum = sum2 = n = 0;
	p = im0;
	for (i = 0; i < boxh; i++) {
	    for (j = 0; j < boxw; j++) {
		int pj = (int)(*p++);
		b = pj - median;
		if (pj && b >= 0) {
		    sum += b;
		    sum2 += (double)b*b;
		    n++;
		}
	    }
	    p += wrap;
	}
	sd2 = n > 1 ? (sum2 - sum * sum/n)/(n-1) : 0;
	sd = sd2 <= 0.0 ? 0.0 : sqrt(sd2);

	thresh = median + sd/SAMPWD;
	*tp = thresh > MAXCAMPIX ? MAXCAMPIX : thresh;
}

int localThreshold(int x, int y)
{
	int boxw,boxh;
	int thresh;
	CamPixel *addr = bW_im + y * bW_w + x;
	
	// build a local noise box here
	boxw = MAXSMEARWIDTH * 2;
	boxh = MAXSMEARWIDTH;
	findThresh2 (addr,bW_w, boxw, boxh, &thresh);
	return thresh;
}

// Qualify a section here
// Returns true if we have required density.  If we request a top, bottom, this is also returned.
// If t/b not found, zero is returned via pointer
// This is set to qualify a smear where pAddr is pointing at the bottom.
// The top/bottom return values end up being somewhat superfluous therefore in this version
int qualifySection(CamPixel **pAddr, int thresh, int *top, int *bottom, GridStats *pGS)
{
	CamPixel 	*addr = *pAddr;
	CamPixel 	*saddr = addr;
	int			i;
	int			ltop,lbtm;
	GridStats	gs;
	
	// first see if we have the requisite density
	ltop = lbtm = 0;
//	printf("Qualify section -- y center @ %d\n",pixelY(*pAddr));
	addr -= bW_w*MAXSMEARWIDTH;
	sampleGrid(&addr,thresh,SAMPWD*2,MAXSMEARWIDTH,&gs);
//	printf("qualifying %dx%d for %d at %d, %d: %.3g %ld %d->%d ~%d~\n",SAMPWD*2,MAXSMEARWIDTH,thresh,pixelX(addr),pixelY(addr),
//			gs.density,gs.sum,gs.min,gs.max,gs.mean);
	if(gs.density > DENSITY) {
		// then look for edges, if requested.
		if(top) {
			addr = saddr;
			i = MAXSMEARWIDTH/2;
//			printf("looking for long top...");
			while(i--) {
				addr -= bW_w;
				sampleGrid(&addr, thresh, SAMPWD*3, 1, &gs);
				if(gs.density < LOCDENS && gs.mean < thresh) {
//					printf("found it at %d",ltop);
					break;
				}
				ltop--;
			}
//			printf("\n");
			if(i) *top = pixelY(saddr) + ltop;
			else *top = 0;
		}		
		if(bottom) {
			addr = saddr;
			i = MAXSMEARWIDTH/2;
//			printf("looking for long bottom...");
			while(i--) {
				addr += bW_w;
				sampleGrid(&addr, thresh, SAMPWD*3, 1, &gs);
				if(gs.density < LOCDENS && gs.mean < thresh) {
//					printf("found it at %d",lbtm);
					break;
				}
				lbtm++;
			}
//			printf("\n");
			if(i) *bottom = pixelY(saddr) + lbtm;
			else *bottom = 0;
		}	
		
		if(pGS) *pGS = gs;
		return 1;	
	}
	
	return 0;
}


// Walk along a candidate smear that starts at this point, along centerline
// returns true/false
int walkSmear(CamPixel **pAddr)
{
	CamPixel *addr = *pAddr;
	int x = pixelX(addr);
	int y = pixelY(addr) - MAXSMEARWIDTH/2;
	int left = x;
	int right;
	long rsum;
	int rcount, ravg;
	
	if(isSectionInSmearList(x,y)) {
		return 0;
	}
	
//	printf("Walking with bottom Y of %d\n",y);

	rsum = ravg = rcount = 0;	
	while(x < bW_w-FSBORD) {
		GridStats gs;
		int thresh = localThreshold(x,y);
		if(!qualifySection(&addr, thresh, NULL, NULL, &gs)) {
			break;
		}
		// maintain a running average of brightness, and break if we drop off significantly
//		printf("rcount %d ravg %d gs.mean %d\n",rcount, ravg,gs.mean);
		if(rcount > 4 && ravg > DROPOFF * gs.mean) {
			break;
		}
		rsum += gs.mean;
		rcount++;
		ravg = rsum/rcount;
		
		x += SAMPWD;
		addr += SAMPWD;
	}
	
	right = x;
	
	recordSmear(left,y,right-left,ravg);
	
	return 1;		
}

/*
 * Call this before calling findSmears:
 *
 * This links us to the WCS library without all the
 * other code that uses this libFITS library to need to
 * link in wcs and fs just for the sake of this one call!
 */
SETWCSFUNC theWCSFunc = NULL;
void SetWCSFunction(SETWCSFUNC inWcs)
{
	theWCSFunc = inWcs;
}

/* Find Smears in Image */
// return 0 if all okay, < 0 if error, > 0 if we have detection data, but WCS failed
int findSmears(FImage *fip, SmearData **pSmearData, int *pNumSmears, int findAnomolies, int tusno, double hunt, int (*bail_out)(), char *str)
{
	CamPixel *addr;
	int x,y;
	AOIStats aoiStats;
	StarDfn	sd;
	StarStats ss;
	char buf[256];
	int count;
	int thresh;
	GridStats gs;
	int rowTop;
	char *im0;
	int  imW,imH;
	int  expectedSmearLength;
	int  rt = 0;
	int  verbose =0;

//	printf("beginning findsmears\n");
			
	im0 = fip->image;
	imW = fip->sw;
	imH = fip->sh;
	expectedSmearLength = calculateSmearLength(fip);
	if(expectedSmearLength < 0) {
		return -1;
	}
	if(expectedSmearLength < MINSMEARLEN) {
		printf("Exposure time is too short for smear detection\n");
		return -1;
	}
	
	// Init smear table
	if(0 > initSmearTable()) {
		printf("Failed to init smear data\n");
		return -1;
	}
		
	// Set the odd globals that this file uses
	// (why break a convention just because it sucks?)
	bW_im = (CamPixel *) im0;
	bW_w = imW;
	bW_h = imH;
	
	// start in upper left border
	x = FSBORD;
	y = FSBORD;
	rowTop = FSBORD;
	
	// do for each SMBOX, moving vertically
	while(1) {	
		int boxw,boxh;
		int boxTop, boxEnd, boxLeft, boxRight;
//		printf("findSmears... gathering stats\n");
		// divide area into SMBOXW x SMBOXH segments
		// and get stats on this area
		if(x+SMBOXW > imW-FSBORD) {
			boxw = imW-FSBORD - x;
			if(boxw < 1) break;
		} else {
			boxw = SMBOXW;
		}
		if(y+SMBOXH > imH-FSBORD) {
			boxh = imH-FSBORD - y;
			if(boxh < 1) break;
		} else {
			boxh = SMBOXH;
		}
		boxTop = y;
		boxEnd = y + boxh;
		boxLeft = x;
		boxRight = x + boxw;
		aoiStatsFITS (im0, bW_w, x, y, boxw, boxh, &aoiStats);
//		printf("just got aoiStats...boxw,h of %d,%d at %d,%d... median of %d\n",boxw,boxh,x,y,aoiStats.median);
		sd.rsrch = 10;
		sd.rAp = 0;
		sd.how = SSHOW_HERE;
		if(0 != starStats(bW_im, bW_w, bW_h, &sd, aoiStats.maxx, aoiStats.maxy, &ss, buf)) {
			printf("findSmears error in starStats: %s\n",buf);
			return -1;
		}
//		printf("and now got star stats... computing threshold...");
		thresh = aoiStats.median + SMEARSD * ((ss.p - ss.Sky)/ss.rmsSky);
//		printf("of %d\n",thresh);		
		
		// Now, starting at the top of this grid area and working downward		
		count = 0;
		while(1) {
			if(bail_out && (*bail_out)()) {	// call this to keep dialog refreshed and to poll for abort
				rt = -1;
				goto exit;
			}			
			addr = bW_im + y * bW_w + x;
			// Get grid statistics at this point
			sampleGrid(&addr, thresh, SAMPWD, SAMPHT, &gs );
			// If we are dense enough, we might be a smear
			if(gs.density > DENSITY) {
				int bottom,lthresh;
//				printf("Found a qualifying density (%.2g) at %d, %d\n",gs.density,x,y);
				// Get local threshold from (about) here on down.
				// We'll sync up properly when we are at the center at this point
				lthresh = localThreshold(x,y-2);
//				printf("Local threshold is %d\n",lthresh);
				// Do a qualifying test at this point. Move to bottom of expected smear and test
				addr += bW_w*MAXSMEARWIDTH/2;
				if(qualifySection(&addr, lthresh, NULL, &bottom, NULL)) {
//					printf("Section is qualified .. walking\n");
					// Walk smear along this row
					if(walkSmear(&addr)) {
						// if we were successful, move beyond smear and continue
						y += MAXSMEARWIDTH/2+bottom-1;
					}
					// if we didn't find a smear, we just keep looking
				}					
			}
			
			y++;
			if(y >= boxEnd) {
				y = boxTop;
				x += SAMPWD;
				if(x >= boxRight) {
					break;
				}
			}
		}
				
		// next box
		y = boxTop + boxh;
		x = boxLeft;
		if(y >= imH-FSBORD) {
			y = rowTop;
			x = boxRight;
		
			if(x >= imW-FSBORD) {
				rowTop += SMBOXH;
				y = rowTop;
				x = FSBORD;
				if(y >= imH-FSBORD) {
					break;
				}
			}
		}
	}
	
//	dumpTable(); // debug dump

	// now WCS solve using the gathered data
	smearHijackFindStars = 1; // hijack findstars
	rt = 0;
	verbose = 0;
	if(!theWCSFunc) rt = 1;
	else if((*theWCSFunc)(fip, tusno, hunt, bail_out, verbose, str) < 0) {
//		printf("smear WCS error: %s\n",str);
		rt = 1; // return > 0 if we have detection but no solution
	}
	smearHijackFindStars = 0;

	// Check for anomolies, like geosynchronous satellites that appear as stars and perhaps other things	
	if(findAnomolies) {
		int *fsxa=NULL;
		int *fsya=NULL;
		CamPixel *fsba=NULL;
		int i, fscount;
		
		sd.rsrch = 10;
		sd.rAp = 0;
		sd.how = SSHOW_BRIGHTWALK;
		
		// Now we want to run a regular star search
		fscount = findStars(im0, imW, imH, &fsxa, &fsya, &fsba);
		// go through returned list
		for(i=0; i<fscount; i++) {
			int x = fsxa[i];
			int y = fsya[i];
			// find entries that do not intersect with our smears
			if(!isRectInSmearList(x-MAXSMEARWIDTH/2,y-MAXSMEARWIDTH/2,MAXSMEARWIDTH,MAXSMEARWIDTH)) {	
				int peak = fsba[i];
				// Qualify these as really looking like a star (bright enough, enough radius, whatever)
				if(0 != starStats(bW_im, bW_w, bW_h, &sd, x,y, &ss, buf)) {
					printf("findSmears error in starStats looking for anomoly: %s\n",buf);
					continue;
				}
				if( ((ss.p - ss.Sky) * ANOMPEAKMULT < ss.Src)
				&&  ((ss.xfwhm-ss.yfwhm)/ss.yfwhm < ANOMFWHMDIFF) ) {
					// add anomoly to table
//					printf("Found anomoly at %d, %d (%d): ",x, y, peak);
//					printf("p=%d bx=%d by=%d Src=%d rmsSrc=%.3g rAp=%d Sky=%d rmsSky=%.3g cx=%.3g cy=%.3g xfwhm=%.3g yfwhm=%.3g xmax=%.3g ymax=%.3g\n",
//						ss.p,ss.bx,ss.by,ss.Src,ss.rmsSrc,ss.rAp,ss.Sky,ss.rmsSky,ss.x,ss.y,ss.xfwhm,ss.yfwhm,ss.xmax,ss.ymax);						
					recordAnomoly(x,y,peak);	
				}
			}					
		}
	}	
exit:	
	// Destroy Smear Table
	if(smearTable) {
		if(pSmearData) {
			*pSmearData = smearTable;	// will be freed by caller
			if(pNumSmears) *pNumSmears = numEntries;
		} else {
			free(smearTable);			// we free it ourselves
		}
		smearTable = NULL;
	}
	
	return rt;
}

/************************************************************************************/

/* find the location and brightest pixel for all stars in the given image.
 * pass back malloced arrays of x and y and b.
 * return number of stars (might well be 0 :( ), or -1 if trouble.
 * N.B. caller must free *xa and *ya and *ba even if we return 0 (but not -1).
 * N.B. we ignore pixels outside FSBORD.
 * N.B. includes undocumented ability to dump raw data around a star.
 */
 // STO: create a couple versions of this so we can call for streaks or stars
 // and get the returns out how we want, but still stay backward compatible
// original call; returns star data in xa,ya,ba with count via return
int findStars(char *im0, int w, int h, int **xa, int **ya, CamPixel **ba)
{
	if(smearHijackFindStars) return returnSmearStars(xa,ya,ba);
	return findStarsAndStreaks(im0,w,h,xa,ya,ba,NULL,NULL);
}
// call that will return streak data (which also will contain star data) in sa (if not null)
// and will also return old-style star data in xa,ya,ba (if not null).
// old style star count via return, streak count (which includes stars it found too) via
// the return pointer numStreaks.
int findStarsAndStreaks(char *im0, int w, int h, int **xa, int **ya, CamPixel **ba, StreakData **sa, int *numStreaks)
{
	int dumpx, dumpy, dumpr;
	CamPixel *p0 = (CamPixel *)im0;
	int *xp=NULL, *yp=NULL;
	CamPixel *bp=NULL;
	StreakData *streakList = NULL;
	CamPixel *p;
	int x, y;
	int fan[BW_NFAN];	/* fan around center */
	int nmalloc;		/* total room available for stars */
	int nstars;		/* number nmalloc[] in use */
	int nstreaks; 	// number streaks in list
	FILE *fp;		/* set if tracing */
	int nxbox, nybox;	/* n noise boxes each direction */
	int boxw, boxh;		/* size of each noise box */
	int nboxes;		/* total number of noise boxes */
	int *boxes;		/* malloced list of signal thresh in each box */
	int *ytopr, *ybotr;	/* interpolated top and bottom rows this seg */
	int *ytoprp, *ybotrp;	/* pointers to y rows, allows to flip */
	int ytop = 0;		/* y at top of current interpolation range */
	int i;
	// flags for what mode(s) to use: determined by return pointers passed in
	int	std_findstars = (xa && ya && ba) ? 1 : 0;
	int find_streaks = (sa) ? 1 : 0;
	
	/* get fresh imaging params */
	loadIpCfg();
	
	/* prepare for bWalk */
	i = 0;
	for (y = -BW_FANR; y <= BW_FANR; y++)
	    for (x = -BW_FANR; x <= BW_FANR; x++)
		if (x || y)
		    fan[i++] = y*w + x;
	bW_im = p0;
	bW_fan = fan;
	bW_w = w;
	bW_h = h;
		
	/* start arrays so we can always use realloc */
	nmalloc = 100;
	if(std_findstars) {
		xp = (int *) malloc (nmalloc*sizeof(xp[0]));
		yp = (int *) malloc (nmalloc*sizeof(yp[0]));
		bp = (CamPixel *) malloc (nmalloc*sizeof(bp[0]));
	}
	if(find_streaks) {
		streakList = (StreakData *) calloc(100,sizeof(StreakData));
	}
	
	/* try to read file naming a region to dump. */
	if ((fp = fopen ("x.dumpstar", "r")) != NULL) {
	    if (fscanf(fp,"%d %d %d",&dumpx,&dumpy,&dumpr) != 3) {
		fclose (fp);
		fp = NULL;
	    }
	}

	/* spread FSNNBOX noise boxes around evenly and find their stats */
	nxbox = (int)ceil(sqrt(FSNNBOX*(w-2.*FSBORD)/(h-2.*FSBORD)));
	nybox = FSNNBOX/nxbox;
	boxw = (w-2*FSBORD)/nxbox;
	boxh = (h-2*FSBORD)/nybox;
	nboxes = nxbox*nybox;
	boxes = (int *) malloc (nboxes*sizeof(boxes[0]));
	for (y = 0; y < nybox; y++) {
	    int y0 = FSBORD + y*boxh;
	    for (x = 0; x < nxbox; x++) {
		int x0 = FSBORD + x*boxw;
		int boxi = y*nxbox + x;
		int thresh;

		findThresh (&p0[y0*w + x0], w, boxw, boxh, &thresh);
		boxes[boxi] = thresh;

		if (fp)
		    printf("T %5d %5d : %5d\n", x0+boxw/2, y0+boxh/2,
								boxes[boxi]);
	    }
	}

	/* scan inside FSBORD, get noise by interpolating box stats */
	nstars = 0;
	nstreaks = 0;
	p = &p0[FSBORD*w + FSBORD];
	ytoprp = ytopr = (int *) malloc (w * sizeof(ytopr[0]));
	ybotrp = ybotr = (int *) malloc (w * sizeof(ybotr[0]));
	for (y = FSBORD; y < h-FSBORD; y++) {
	    int ydump = fp && y >= dumpy-dumpr && y <= dumpy+dumpr;
	    int ybase = FSBORD + 3*boxh/2;
	    int yroll = y - ybase;
	    int x0 = FSBORD + boxw/2;

	    /* at each boxh center set ytopr/botp to top/bottom y rows.
	     * each is filled with interpolated values on their rows for all x.
	     */
	    if (y == FSBORD) {
		/* special case to start: cover down to 3/2*boxh */
		ytoprp = ytopr;
		ybotrp = ybotr;
		for (x = FSBORD; x < w-FSBORD; x++) {
		    ytoprp[x] = linInterp (x0, boxw, boxes, nxbox, x);
		    ybotrp[x] = linInterp (x0, boxw, boxes+nxbox, nxbox, x);
		}
		ytop = FSBORD + boxh/2;
	    } else if (yroll>=0 && (yroll%boxh)==0 && yroll/boxh<nybox-2) {
		/* move bottom row to top position, find new bottom row */
		int *swap = ytoprp;
		ytoprp = ybotrp;
		ybotrp = swap;

		for (x = FSBORD; x < w-FSBORD; x++)
		    ybotrp[x]= linInterp(x0, boxw, boxes + nxbox*(yroll/boxh+2),
								    nxbox, x);
		ytop = y;
	    }

	    for (x = FSBORD; x < w-FSBORD; x++) {
		int dump = ydump && x >= dumpx-dumpr && x <= dumpx+dumpr;
		int thresh = ((double)(y)-ytop)*(ybotrp[x]-ytoprp[x])/boxh
								    + ytoprp[x];
		int brx, bry;
		CamPixel *peak;
		
		// turn on debug for streaks too
		dbon = dump;
		
		if (dump)
		    printf ("P %5d %5d = %5d >? %5d : ", x, y, *p, thresh);

		/* below noise floor? */
		if (*p < thresh) {
		    if (dump)
			printf ("< %5d\n", thresh);
		    goto nope;
		}

		/*  burned out? */
		if (*p > BURNEDOUT) {
		    if (dump)
			printf ("> %5d\n", BURNEDOUT);
		    goto nope;
		}

		/* already going up a hill? */
		if (p[-1] > thresh) {
		    if (dump)
			printf ("already going up hill\n");
		    goto nope;
		}

		/* walk trouble? */
		bW_thresh = thresh;
		if (bWalk (p) < 0) {
		    if (dump)
			printf ("bright walk trouble\n");
		    goto nope;
		}
		brx = (bW_bp-p0)%w;
		bry = (bW_bp-p0)/w;;
		peak = &p0[bry*w + brx];

		if (dump)
		    printf ("BW %5d %5d = %5d ", brx, bry, *peak);

		/* already on list? */
		for (i = nstars; --i >= 0 && yp[i] >= bry-FSMINSEP; ) {
		    if (abs(xp[i]-brx)<=FSMINSEP && abs(yp[i]-bry)<=FSMINSEP) {
			if (dump)
			    printf ("already on list\n");
			goto nope;
		    }
		}
				
		/* now use a very local noise value about peak */
		findThresh (peak - (FSNBOXSZ/2)*(w + 1), w, FSNBOXSZ, FSNBOXSZ,
								    &thresh);
								
		if(find_streaks) {
			// At this point, walk the streak.
			// If it returns 0, it means it has qualified a star at this point
			// If it returns > 0 it has qualified a streak of that length
			// If it returns < 0 it means that it did NOT qualify anything for this location
			int streakLength;			
			int startx,starty,endx, endy; // we will get the end point here
			
			// first, check if we've already gotten this
			if(IsPointWithinStreakList(brx,bry,streakList,nstreaks)) {
				if(dump) printf("already on streak list\n");
				if(std_findstars) goto starsearch;	
				else			  goto nope;
			}			
			else if(dump) printf("\n");			
			
			streakLength = walkStreak(&startx,&starty,&endx,&endy,dump);
			
			// reject 0-length finds if they don't pass the star test for connectivity
			if (!streakLength && connected (peak, w, thresh) < 0) {
		    	if (dump)
				printf ("not connected\n");
			    goto nope;
			}		
									
			if(streakLength >= 0) {
				StreakData newStreak, *pStr = &newStreak;
				
				// start recording our new streak				
				pStr->startX = pStr->endX = brx;	// peak bright spot
				pStr->startY = pStr->endY = bry;
				pStr->walkEndX = endx;		// the full extent end
				pStr->walkEndY = endy;
				
				// set our walk starting point... this is the full extent start
				pStr->walkStartX = startx;
				pStr->walkStartY = starty;
				
				// find the peak endpoint by walking back from our walk endpoint
				if(0<=bWalk(&p0[endy*w + endx])) {
					pStr->endX = pixelX(bW_bp);
					pStr->endY = pixelY(bW_bp);
				}
				
				// set slope and length based on full extent
				pStr->slope = (pStr->walkEndX-pStr->walkStartX) ?
								(double) (pStr->walkEndY - pStr->walkStartY) / (double) (pStr->walkEndX - pStr->walkStartX)
								: HUGE_VAL;				
								
				pStr->length = sqrt((pStr->walkEndX - pStr->walkStartX)*(pStr->walkEndX - pStr->walkStartX)+(pStr->walkEndY - pStr->walkStartY)*(pStr->walkEndY - pStr->walkStartY));
													
				
				// Do another check to see if we end with a collision at end point
				if(IsPointWithinStreakList(pStr->endX,pStr->endY,streakList,nstreaks)) {
					if(dump) printf("endpoint already on streak list\n");
					if(std_findstars) goto starsearch;  // note: not doing this would retain consistency between findstars and findstreaks
					else			  goto nope;
				}											

				// get the fwhm ratio for the peak startpoint of this object
				pStr->fwhmRatio = getFWHMratio(p0,w,h,pStr->startX,pStr->startY);
							
				// Now we can add the streak
				++nstreaks;
				if (nstreaks % 100 == 0) {
					int newCount = ((nstreaks/100)+1) * 100;
					streakList = (StreakData *) realloc((void *)streakList, newCount * sizeof(StreakData));
				}

				/* insert by increasing y */				
				for (i = nstreaks-1; --i >= 0 && bry < streakList[i].startY; ) {
					streakList[i+1] = streakList[i];
				}
				streakList[i+1] = newStreak;				
			}							
		}

starsearch:								
		// continue with the rest of this if we are finding stars the old-school way
		if(std_findstars) {

			if (dump)
			    printf ("((( BW %5d %5d = %5d ))) ", brx, bry, *peak);
										
			/* too tall? */
			for (i = 1; i < FSNBOXSZ; i++)
			    if (peak[i*w] < thresh && peak[-i*w] < thresh)
				break;
			if (i == FSNBOXSZ) {
			    if (dump)
				printf ("taller than %5d\n", FSNBOXSZ);
		    	goto nope;
			}				

			/* disconnected? */
			if (connected (peak, w, thresh) < 0) {
	    		if (dump)
				printf ("not connected\n");
			    goto nope;
			}
															
			/* Yes! */

			if (dump)
			    printf ("YES %5d at %5d %5d\n", *peak, brx, bry);
			
			if (nstars == nmalloc) {
			    nmalloc += 100;		/* grow in chunks */
			    xp = (int *) realloc ((void *)xp, nmalloc*sizeof(int));
		    	yp = (int *) realloc ((void *)yp, nmalloc*sizeof(int));
			    bp = (CamPixel*) realloc ((void *)bp, nmalloc*sizeof(CamPixel));
			}

			/* insert by increasing y */
			for (i = nstars; --i >= 0 && bry < yp[i]; ) {
			    xp[i+1] = xp[i];
		    	yp[i+1] = yp[i];
			    bp[i+1] = bp[i];
			}
			xp[i+1] = brx;
			yp[i+1] = bry;
			bp[i+1] = *peak;
			nstars++;
		}
				
		/* come here when finished investigating this [x,y] */
	      nope:
		p++;
	    }

	    p += 2*FSBORD;
	}

	if (fp)
	    fclose (fp);
	free ((char *)boxes);
	free ((char *)ytopr);
	free ((char *)ybotr);

	// now do the second-pass processing of the streak data
	if(find_streaks) {
	
	
		// first, compute the median fwhm ratio
		double *pRatlist = (double *) malloc(nstreaks * sizeof(double));
		int i,j,k;
		int nrat = 0;
		double rat;
		double mean,median,stdDev;
		// create a sorted list of the ratios
		for(i=0; i<nstreaks; i++) {
			if(!streakList[i].length) { // don't count the ones we have already determined are streaks...
				rat = streakList[i].fwhmRatio;
				for(j=0; j<nrat && pRatlist[j] > rat; j++);
				for(k=nrat; k>j; k--) {
					pRatlist[k] = pRatlist[k-1];
				}
				pRatlist[j] = rat;
				nrat++;
			}
		}		
		// get the median stats
		getStats(pRatlist,nrat,&mean,&median,&stdDev);
		free(pRatlist);
//	printf("ratio mean: %lf median: %lf  stdDev: %lf\n",mean,median,stdDev);
//    printf("Looking for ratios < %lf or > %lf\n",median-(median*STRKDEV),median+(median*STRKDEV));
	
	
		for(i=0; i<nstreaks; i++) {
			StreakData *pStr = &streakList[i];
						
			// check for minimum full length
			if(pStr->length < MINSTRKLEN) {
				pStr->flags = STREAK_NO;
				continue;
			} else {
				// check for minimum peak-to-peak length
				int dy = pStr->endY - pStr->startY;
				int dx = pStr->endX - pStr->startX;
				int length;
				dy = (int) (dy * median) + 0.5; // adjust length to match fwhm stats
				length = sqrt(dy*dy+dx*dx);
				if(length < FSMINSEP) {
					// if too short, see if we qualify because of FWHM ratio
					if(fabs(pStr->fwhmRatio - median) < (median*STRKDEV)) {
						// nope.  Reject this one
						pStr->flags = STREAK_NO;
						continue;
					}
				}
			}			
			pStr->flags = qualifyStreakData(p0,w,pStr);				
		}
	}			
						
	if(xa) *xa = xp;
	if(ya) *ya = yp;
	if(ba) *ba = bp;
	if(sa) *sa = streakList;
	if(numStreaks) *numStreaks = nstreaks;
	
	return (nstars);
}

/////////////////////////////////////////////////////////////////////////////////




////////////////////////////////////////////////////////////////////////////////

/* finds linear features in an image by using findStatStars() to identify 
 * bright points in the image and exploring the ratios of FWHM to 
 * identify elongated objects as potentially part of a larger streak.
 */

int 
findLinearFeature (char *im0, int w, int h, StarStats **ssp, \
		   double *xfirst, double *yfirst, \
		   double *xlast, double *ylast) 
{
  int tcount = 0,i,j,ns=0,streaks=0,bad=0,ref=0;
  int nelem[MAXSTREAKS];
  int nelemtemp = 0, count=0;
  double *fr_all, *x_all, *y_all;
  double *fr, *x_val, *y_val;
  double *frtemp, *xtemp, *ytemp;
  
  double frmean, frmedian, frstdev, fr_std;
  
  /* start with findStatStars() to get info on objects in the field */

  ns = findStatStars (im0, w, h, &(*ssp));
  
  fr_all = malloc(ns*sizeof(double));
  x_all = malloc(ns*sizeof(double));
  y_all = malloc(ns*sizeof(double));
  fr = malloc(ns*sizeof(double));
  x_val = malloc(ns*sizeof(double));
  y_val = malloc(ns*sizeof(double));

  /* start by loading the values from findstars */
  
  for (i = 0; i < ns; i++) {
    StarStats *sp = &(*ssp)[i];
    fr_all[i] = sp->yfwhm/sp->xfwhm;
    x_all[i] = sp->x;
    y_all[i] = sp->y;
  }
  threeSort (ns, fr_all, x_all, y_all);
  getStats (fr_all, ns, &frmean, &frmedian, &frstdev);
  if (ns < 10) {
    frmedian = .667;
  }
  /* find those objects with elongated fwhm ratios 
   * based on the median value of fr_all found above
   */
  for (i = 0; i < ns; i++) {
    StarStats *sp = &(*ssp)[i];
    if ((sp->xfwhm/sp->yfwhm) > 3.*frmedian \
	|| (sp->yfwhm/sp->xfwhm) > 3.*frmedian) {
      fr[tcount] = sp->yfwhm/sp->xfwhm;
      x_val[tcount] = sp->x;
      y_val[tcount] = sp->y;
      tcount++;      
    }   
  }

  /* sort the arrays on fr, keeping x,y in line */

  threeSort(tcount, fr, x_val, y_val); 

  /* Find out how many streaks we have, and 
   * how many components are in each.  This is based on 
   * the fwhm ratio.  Objects with similar ratios
   * are presumed to be part of the same streak.  Later
   * we check to make sure adjacent points are really part of 
   * the same streak.
   */

  getStats (fr, tcount, &frmean, &frmedian, &frstdev);
  for (i = 0;i<tcount;i++) {
    fr_std = sqrt((fr[i] - fr[i+1]) \
		  *(fr[i] - fr[i+1]));
    frstdev = 0.3;
    if (fr_std < frstdev) {
      nelemtemp++;
    } else {
      nelemtemp++;
      nelem[streaks] = nelemtemp;
      streaks++;
      nelemtemp = 0;
    }
  }
  /* Sort components into streaks, and find endpoints
   */

  ref = 0;
  for (i=0;i<streaks;i++) {
    // malloc temp arrays for sorting
    frtemp = malloc(nelem[i]*sizeof(double));
    xtemp = malloc(nelem[i]*sizeof(double));
    ytemp = malloc(nelem[i]*sizeof(double));
    // take care of single element objects
    if (nelem[i] == 1) {
      xfirst[i] = -1;
      yfirst[i] = -1;
      xlast[i] = -1;
      ylast[i] = -1;
      ref = ref+nelem[i];
      bad++;
    } else {
      // load temp arrays with all elements of a streak
      for (j=ref;j<ref+nelem[i];j++) {
	frtemp[j-ref] = fr[j];
	xtemp[j-ref] = x_val[j];
	ytemp[j-ref] = y_val[j];
      }
      
      // remove outliers
      threeSort(nelem[i], ytemp, frtemp, xtemp);
      count = removeOutliers(nelem[i], xtemp, ytemp, frtemp);
      // these should be the real endpoints of the streak
      if (count == 0) {
	xfirst[i] = -1;
	yfirst[i] = -1;
	xlast[i] = -1;
	ylast[i] = -1;
	bad++;
      } else {
	xfirst[i] = xtemp[0];
	yfirst[i] = ytemp[0];
	xlast[i] = xtemp[count-1];
	ylast[i] = ytemp[count-1];
      }
      // increment our reference index and zero count
      ref = ref+nelem[i];
      count = 0;
    }
    
    /* clean up for next round */
    
    free ((char *) frtemp);
    free ((char *) xtemp);
    free ((char *) ytemp);
    
  }
  
  /* clean up and go home */
  
  free ((char *) fr);
  free ((char *) x_val);
  free ((char *) y_val);
  free ((char *) fr_all);
  free ((char *) x_all);
  free ((char *) y_all);
  
  /* reorder to get rid of the -1's if we have bad streaks */
  if (bad) {
    count = 0;
    for (i=0;i<streaks;i++) {
      if (xlast[i] > -1) {
	xfirst[count] = xfirst[i];
	yfirst[count] = yfirst[i];
	xlast[count] = xlast[i];
	ylast[count] = ylast[i];
	count++;
      }
    }
    return count;
  } else {
    return streaks;
  }

}

/* threeSort - sorts vector pointed to by s, and carries vectors v1
 * and v2 along for the ride, preserving the associations.  s, v1, and v2
 * must have a 1-to-1 correspondence and lengths equal to n
 */

void 
threeSort (int n, double *s, double *v1, double *v2)
{

  int done,i;
  double hold;
  done = 0;
  while (done == 0) {
    done = 1;
    for (i = 0; i < n-1;i++)
      if (s[i] > s[i+1]) {
	done = 0;
	hold = s[i];
	s[i] = s[i+1];
	s[i+1] = hold;
	hold = v1[i];
	v1[i] = v1[i+1];
	v1[i+1] = hold;
	hold = v2[i];
	v2[i] = v2[i+1];
	v2[i+1] = hold;
      }
    n--;
  }
}

/* linFit - fits a straight line to x,y - reports parameters of fit
 * errors and chisq - only for 
 */
void 
linFit (double *x, double *y, int ndata, double *a, \
	double *b, double *siga, double *sigb, double *chi2)
{
  int i;
  double t, sxoss, sx=0.0, sy=0.0, st2=0.0, ss, sigdat;
  
  *b = 0.0;

  for (i=0;i<ndata;i++) {
    sx += x[i];
    sy += y[i];
  }
  ss = ndata;
  sxoss=sx/ss;
  for (i=0;i<ndata;i++) {
    t=x[i] - sxoss;
    st2 += t*t;
    *b +=t*y[i];
  }
  *b /= st2;
  *a = (sy-sx*(*b))/ss;
  *siga = sqrt((1.0+sx*sx/(ss*st2))/ss);
  *sigb = sqrt(1.0/st2);
  *chi2 = 0.0;
  for (i=0;i<ndata;i++) {
    *chi2 += (y[i] - (*a) - (*b)*x[i])*(y[i] - (*a) - (*b)*x[i]);
  }
  sigdat = sqrt((*chi2)/(ndata-2));
  *siga *= sigdat;
  *sigb *= sigdat;
}

/* given a pointer to a sorted x[i], returns mean, median, and
 * stdev
 */
void  
getStats (double *x, int ndata, double *mean, double *median,\
	  double *stdev) 
{
  int i;
  double m=0.0,md=0.0,s=0.0,sum=0.0,sum2=0.0,sd2;

  for (i=0;i<ndata;i++) {
    //s += (x[i] - (m))*(x[i] - (m));
    sum2 += x[i]*x[i];
    sum += x[i];
  }
  m = sum/ndata;
  sd2 = (sum2 - sum*sum/ndata)/(ndata-1);
  //s = (s)/(ndata-1);
  s = sd2 <= 0.0 ? 0.0 : sqrt(sd2);
  if (ndata%2 == 0) {     
    md = x[ndata/2];
  } else {
    md = ((x[ndata/2] + x[(ndata/2) + 1])/2);
  }
  *mean = m;
  *median = md;
  *stdev = s;
}

/* given a set of vectors x and y, fits a line 
 * to the data and removes outliers.  Returns count
 * of remaining data points.
 */

int 
removeOutliers (int ndata, double *x, double *y, double *fr)
{
  int i,j;
  int count=0;
  double a, b, siga, sigb, chi2;
  double result=0.0;
  double moddiff =0.0;
  

  for (j=0;j<10;j++) {
    linFit(x, y, ndata, &a, &b, &siga, &sigb, &chi2);
    
    for (i=0;i<ndata;i++) {
      moddiff = y[i]-(a+b*x[i]);
      result += fabs(moddiff);
    }
    for (i=0;i<ndata;i++) {
      moddiff = y[i]-(a+b*x[i]);
      if (fabs(moddiff) < 2.*result/ndata) {
	fr[count] = fr[i];
	x[count] = x[i];
	y[count] = y[i];
	count++;
      }
    }
    ndata = count;
    count = 0;
    result = 0.0;
  }

  
  /* if only two data points left, make sure they are adjacent 
   * before returning
   */
  if (ndata == 2) {
    if (fabs(y[0] - y[1]) > 10) {
      ndata = 0;
    }
  }
  return ndata;
}

/* version of findStars() that passes back a malloced array of StarStats.
 * we return number of stars (might well be 0 :-), or -1 if trouble.
 * N.B. caller must free **sspp if we return >= 0
 * N.B. we ignore pixels outside FSBORD.
 */
int
findStatStars (im0, w, h, sspp)
char *im0;
int w, h;
StarStats **sspp;
{
	char buf[1024];	/* some calls need it */
	int *x, *y;	/* malloced lists of star locations */
	CamPixel *b;	/* malloced list of brightest pixel in each */
	StarDfn sd;	/* for getting real star stats */
	int nfs;	/* number of raw stars from findStars() */
	int ngs;	/* number of really good stars */
	int i;

	loadIpCfg();

	/* get list */
	nfs = findStars (im0, w, h, &x, &y, &b);
	if (nfs < 0)
	    return (-1);
	if (nfs == 0) {
	    free ((char *)x);
	    free ((char *)y);
	    free ((char *)b);
	    return (-1);
	}

	/* compute stats and retain only the best */
	*sspp = (StarStats *) malloc (nfs * sizeof(StarStats));
	if (!*sspp) {
	    free ((char *)x);
	    free ((char *)y);
	    free ((char *)b);
	    return (-1);
	}
	sd.rsrch = 0;
	sd.rAp = 0;
	sd.how = SSHOW_HERE;
	ngs = 0;
	for (i = 0; i < nfs; i++) {
	    StarStats *ssp = &(*sspp)[i];
	    if (!starStats((CamPixel*)im0, w, h, &sd, x[i], y[i], ssp, buf)) {
		if (ngs < i)
		    (*sspp)[ngs] = *ssp;
		ngs++;
	    }
	}

	/* ok */
	free ((char *)x);
	free ((char *)y);
	free ((char *)b);
	return (ngs);
}

/* Compute the guassian stats for a star.
 * N.B. we assume all the other portions of ssp are already set.
 */
static void
starGauss (image, w, r, ssp)
CamPixel *image;	/* image array */
int w;		/* width */
int r;		/* how far to go either side of center */
StarStats *ssp;	/* fill in x, y, fwhm and max entries */
{
	int a[1024];	/* "enough" room for row and col buffers */
	CamPixel *imp;
	double max, cen, fwhm;
	int med = ssp->Sky;
	int n;
	int i;

	/* don't use a ridiculously small radius */
	if (r < MINGAUSSR)
	    r = MINGAUSSR;
	n = 2*r + 1;

	imp = &image[w*ssp->by + ssp->bx - r]; /* left end of row */
	for (i = 0; i < n; i++)
	    a[i] = (int)(*imp++) - med;
	gaussfit (a, n, &max, &cen, &fwhm);
	ssp->xmax = max + med;
	ssp->x = ssp->bx + cen - r;
	ssp->xfwhm = fwhm;

	imp = &image[w*(ssp->by-r) + ssp->bx]; /* top end of col */
	for (i = 0; i < n; i++) {
	    a[i] = (int)(*imp) - med;
	    imp += w;
	}
	gaussfit (a, n, &max, &cen, &fwhm);
	ssp->ymax = max + med;
	ssp->y = ssp->by + cen - r;
	ssp->yfwhm = fwhm;
}

// get the ratio of fwhm vertical / fwhm horizontal at the given star center
static double getFWHMratio(CamPixel *im0, int w, int h, int x, int y)
{
	StarStats	ss;
	StarDfn		sdfn;
	char errmsg[256];

	sdfn.rsrch = DEFSKYRAD;
	sdfn.rAp = STRKRAD;
	sdfn.how = SSHOW_HERE;
	if(0 > starStats(im0,w,h,&sdfn,x,y,&ss,errmsg)) {
		printf("getFWHMratio fails starStats: %s\n",errmsg);
	}
//	printf("fwhm ratio @ %d,%d: %lf / %lf = %lf\n", x,y,ss.yfwhm,ss.xfwhm,ss.yfwhm/ss.xfwhm);
	return ss.yfwhm/ss.xfwhm;
}						

/* given an image and a starting point, search a surrounding square for the
 * location and value of the brightest pixel.
 */
static void
brightSquare (image, w, ix, iy, r, xp, yp, bp)
CamPixel *image;	/* image */
int w;			/* width of image */
int ix, iy;		/* location of square center */
int r;			/* half-size of square to search */
int *xp, *yp;		/* location of brightest pixel */
CamPixel *bp;		/* value of brightest pixel */
{
	int x0 = ix - r;		/* left column */
	int y0 = iy - r;		/* top row */
	CamPixel *row = &image[y0*w+x0];/* upper left corner of box */
	int n = 2*r + 1;		/* square size */
	int wrap = w - n;		/* wrap from end of one row to next */
	CamPixel b = 0;			/* candidate brightest pixel value */
	int bx = ix, by = iy;		/* " location */
	register int x, y;		/* coords inside box */

	for (y = 0; y < n; y++) {
	    for (x = 0; x < n; x++) {
		CamPixel p = *row++;
#ifdef BRIGHT_TRACE
		printf ("p[%3dd,%3dd] = %4d\n", x+x0, y+y0, (int)p);
#endif
		if (p > b) {
		    b = p;
		    bx = x0 + x;
		    by = y0 + y;
		}
	    }
	    row += wrap;
	}

	*xp = bx;
	*yp = by;
	*bp = b;
}

/* given an image and a starting point, walk the gradient to the brightest
 * pixel and return its location. we never roam more than maxr away.
 * return 0 if find brightest pixel within maxsteps else -1.
 */
static int
brightWalk (imp, w, x0, y0, maxr, xp, yp, bp)
CamPixel *imp;
int w;
int x0, y0;
int maxr;
int *xp, *yp;
CamPixel *bp;
{
#define	PXY(x,y)	(imp[(y)*w + (x)])
	CamPixel b;
	int x, y;

	/* start by assuming seed point is brightest */
	b = PXY(x0,y0);
	x = x0;
	y = y0;

	/* walk towards any brighter pixel */
	for (;;) {
	    CamPixel tmp, newb;
	    int newx=0, newy=0;

#define	NEWB(x,y)			\
	    tmp = PXY((x),(y));		\
	    if (tmp > newb) {		\
		newx = (x);		\
		newy = (y);		\
		newb = tmp;		\
	    }				\
	    
	    newb = b;
	    NEWB(x+1,y+1);
	    NEWB(x,y+1);
	    NEWB(x-1,y+1);
	    NEWB(x+1,y);
	    NEWB(x-1,y);
	    NEWB(x+1,y-1);
	    NEWB(x,y-1);
	    NEWB(x-1,y-1);

	    if (newb == b)
		break;

	    x = newx;
	    y = newy;
	    b = newb;
	    if (abs(x-x0) > maxr || abs(y-y0) > maxr)
		return (-1);
	}

	*xp = x;
	*yp = y;
	*bp = b;
	return (0);
#undef	PXY
#undef	NEWB
}

/* find the radius of the annulus about [x0,y0] which yields the best SNR.
 * we expect the SNR to increase with r to a max then decrease, so scan out
 * from a radius of 0 and stop when the snr is less than that at the previous r.
 * Based on Larry Molnar notes of 6 Dec 1996
 */
static void
bestRadius (image, w, x0, y0, rAp, rp)
CamPixel *image;                /* array of pixels */
int w;                          /* width of image */
int x0, y0;                     /* center of annulus */
int rAp;			/* initial guess radius which is surely sky */
int *rp;			/* best radius */
{
	double lSNR;		/* "last" snr, ie, at k-1 */
	double Ck;		/* cumulative pixel count through radius k */
	double Nk;		/* cumulative number of pixels through rad k */
	double rmsS2;		/* rms count in ring of sky */
	int E;			/* median count in ring of sky */
	int k;			/* candidate radius */

	/* get stats in annulus far enough out to surely look like sky */
	ringStats (image, w, x0, y0, rAp, &E, &rmsS2);
	rmsS2 *= rmsS2;

#ifdef BEST_TRACE
	printf ("Best: rAp=%d E=%d rmsS2=%g\n", rAp, E, rmsS2);
#endif

	lSNR = -1.0;
	Ck = Nk = 0;
	for (k = 0; k < 2*rAp && k < FSBORD; k++) {
	    int B;		/* sum of pixels in this annulus */
	    int M;		/* number of pixels in this annulus */
	    double Sk;		/* source count out to this radius */
	    double rmsSk;	/* rms out to this radius */
	    double SNR;		/* snr out to this radius */

	    ringCount (image, w, x0, y0, k, &M, &B);
	    Ck += B;
	    Nk += M;

	    Sk = Ck - Nk*E;
	    rmsSk = sqrt(Nk*rmsS2 + Sk/TELGAIN);
	    SNR = Sk/rmsSk;

#ifdef BEST_TRACE
	    printf ("SNR=%5.1f Ck=%8d Nk=%5d B=%6d M=%3d k=%2d\n", SNR, Ck, Nk,
								    B, M, k);
#endif

	    /* quit when SNR decreases */
	    if (SNR < lSNR)
		break;
	    lSNR = SNR;
	}

	/* peak was at prior radius */
	*rp = k-1;
}


/* find number and sum of pixels within an annulus of radius [r..r+1]
 * about [x0,y0]
 */
static void
ringCount (image, w, x0, y0, r, np, sump)
CamPixel *image;                /* array of pixels */
int w;                          /* width of image */
int x0, y0;                     /* center of annulus */
int r;                          /* radius of annulus */
int *np;			/* n pixels in annulus */
int *sump;			/* sum of pixels in annulus */
{
	int x, y;		/* scanning coordinates */
	int inrr = r*r;		/* inner radius, squared */
	int outrr = (r+1)*(r+1);/* outter radius, squared */
	CamPixel *ip;		/* walks down center of box */
	int n;			/* number of pixels encountered */
	int sum;		/* sum of pixels encountered */

	/* scan a box for pixels with radius [r .. r+1] from [x0,y0] */
	ip = &image[w*(y0-r) + x0]; /* start at center of top row */
	n = sum = 0;
	for (y = -r; y <= r; y++) {
	    int yrr = y*y;
	    for (x = -r; x <= r; x++) {
		int xyrr = x*x + yrr;
		if (xyrr >= inrr && xyrr < outrr) {
		    int p = (int)(ip[x]);
		    sum += p;
		    n++;
		}
	    }
	    ip += w;	/* next row, still centered */
	}

	*np = n;
	*sump = sum;
}

/* find median and rms within an annulus of radius [r..r+1] about [x0,y0] */
static void
ringStats (image, w, x0, y0, r, Ep, sigp)
CamPixel *image;		/* array of pixels */
int w;				/* width of image */
int x0, y0;			/* center of annulus */
int r;				/* inner radius of annulus */
int *Ep;			/* median pixel value within annulus */
double *sigp;			/* rms within annulus */
{
	int inrr = r*r;		/* inner radius, squared */
	int outrr = (r+1)*(r+1);/* outter radius, squared */
	int hist[NCAMPIX];	/* histogram (big, but faster than sorting) */
	CamPixel *ip;		/* walks down center of box */
	int x, y;		/* scanning coordinates */
	int p16, p50, p84; 	/* pixel at 16, 50 and 84 percentiles */
	int npix;		/* number of pixels */
	int s16, s50, s84; 	/* 16, 50 and 84% of npix */
	int sum;		/* running sum for stats */

	/* zero the histogram */
	(void) memset ((void *)hist, 0, sizeof(hist));

	/* scan a box for pixels with radius [r .. r+1] from [x0,y0] */
	r++; /* go to outter radius */
	ip = &image[w*(y0-r) + x0]; /* start at center of top row */
	npix = 0;
	for (y = -r; y <= r; y++) {
	    int yrr = y*y;
	    for (x = -r; x <= r; x++) {
		int xyrr = x*x + yrr;
		if (xyrr >= inrr && xyrr < outrr) {
		    int p = (int)(ip[x]);
		    hist[p]++;
		    npix++;
		}
	    }
	    ip += w;	/* next row, still centered */
	}

	/* find the pixels at the 16, 50 and 84 percentiles */
	p16 = p50 = p84 = 0;
	s16 = (int)floor(npix * 0.16 + 0.5);
	s50 = (int)floor(npix * 0.50 + 0.5);
	s84 = (int)floor(npix * 0.84 + 0.5);
	sum = 0;
	for (x = 0; x < NCAMPIX; x++) {
	    sum += hist[x];
	    if (p16 == 0 && sum >= s16)
		p16 = x;
	    if (p50 == 0 && sum >= s50)
		p50 = x;
	    if (p84 == 0 && sum >= s84)
		p84 = x;
	}

#ifdef RING_TRACE
	printf ("Ring: r=%2d npix=%4d p16=%5d p50=%5d p84=%5d\n", r-1, npix,
							    p16, p50, p84);
#endif

	*Ep = p50;
	*sigp = (p84 - p16)/2.0;
}

/* find median and rms of a thick annulus from radius r+APGAP out until
 * use min(PI*r*r*APSKYX,MAXSKYPIX) pixels (but no further than image edge!)
 * return 0 if ok, else -1.
 */
static int
skyStats (image, w, h, x0, y0, r, Ep, sigp)
CamPixel *image;		/* array of pixels */
int w;				/* width of image */
int h;				/* height of image */
int x0, y0;			/* center of annulus */
int r;				/* radius of aperature */
int *Ep;			/* median pixel value within annulus */
double *sigp;			/* rms within annulus */
{
	int hist[NCAMPIX];	/* histogram (big, but faster than sorting) */
	int x, y;		/* scanning coordinates */
	CamPixel *ip;		/* walks down center of box */
	int p16, p50, p84; 	/* pixel at 16, 50 and 84 percentiles */
	int npix;		/* total number of pixels */
	int s16, s50, s84; 	/* 16, 50 and 84% of npix */
	int sum;		/* running sum for stats */
	int minpix;		/* need at least this many pixels */
	int k;			/* walking radius */

	/* zero the histogram */
	(void) memset ((void *)hist, 0, sizeof(hist));

	npix = 0;
	minpix = (int)ceil(PI*r*r*APSKYX);
	if (minpix > MAXSKYPIX)
	    minpix = MAXSKYPIX;
	for (k = r+APGAP; npix < minpix; k++) {
	    int inrr = k*k;		/* inner radius, squared */
	    int outrr = (k+1)*(k+1);	/* outter radius, squared */

	    /* guard the edge */
	    if (x0 - k < 0 || x0 + k >= w || y0 - k < 0 || y0 + k >= h)
		break;

	    /* scan a box for pixels with radius [k .. k+1] from [x0,y0] */
	    ip = &image[w*(y0-k) + x0]; /* start at center of top row */
	    for (y = -k; y <= k; y++) {
		int yy = y*y;
		for (x = -k; x <= k; x++) {
		    int rr = x*x + yy;
		    if (rr >= inrr && rr < outrr) {
			int p = (int)(ip[x]);
			hist[p]++;
			npix++;
		    }
		}
		ip += w;	/* next row, still centered */
	    }
	}

	if (npix < minpix)
	    return (-1);	/* couldn't make it far enough out */

	/* find the pixels at the 16, 50 and 84 percentiles */
	p16 = p50 = p84 = 0;
	s16 = (int)floor(npix * 0.16 + 0.5);
	s50 = (int)floor(npix * 0.50 + 0.5);
	s84 = (int)floor(npix * 0.84 + 0.5);
	sum = 0;
	for (x = 0; x < NCAMPIX; x++) {
	    sum += hist[x];
	    if (p16 == 0 && sum >= s16)
		p16 = x;
	    if (p50 == 0 && sum >= s50)
		p50 = x;
	    if (p84 == 0 && sum >= s84)
		p84 = x;
	}

#ifdef SKY_TRACE
	printf ("Sky: r=%d npix=%d p16=%d p50=%d p84=%d\n", r, npix,
							    p16, p50, p84);
	printf ("Sky: startr=%d finalr=%d minpix=%d\n", r+APGAP, k-1, minpix);
#endif

	*Ep = p50;
	*sigp = (p84 - p16)/2.0;

	return (0);
}

/* find the total number of pixels within a circle about [x0,y0] of radius r,
 * and the sum of those pixels.
 */
static void
circleCount (image, w, x0, y0, maxr, np, sump)
CamPixel *image;        /* array of pixels */
int w;                  /* width of image */
int x0, y0;             /* center of annulus */
int maxr;		/* max radius of circle */
int *np;		/* total n pixels in circle */
int *sump;		/* sum of pixels in circle */
{
	int k;		/* expanding radius */
	int Ck;		/* cumulative pixel count */
	int Nk;		/* cumulative number of pixels */
	int B;		/* sum of pixels at radius k */
	int M;		/* number of pixels at radius k */

	Ck = Nk = 0;
	for (k = 0; k <= maxr; k++) {
	    ringCount (image, w, x0, y0, k, &M, &B);
	    Ck += B;
	    Nk += M;
	}

	*np = Nk;
	*sump = Ck;
}

//
// Loading IP.CFG is now made public
// including the option to change the location of the file
//

/* reload ipcfn if never loaded before or it has been modified since last load.
 * exit if trouble.
 */
void loadIpCfg()
{
	static char telfn[sizeof(ipcfn)+100];
	static time_t lastload;
	struct stat s;

//	if (!lastload)
    telfixpath (telfn, ipcfn);
	
	if (stat (telfn, &s) < 0) {
	    fprintf (stderr, "%s: %s\n", telfn, strerror(errno));
	    exit(1);
	}
	if (s.st_mtime > lastload) {
	    int n = readCfgFile (0, ipcfn, ipcfg, NIPCFG);
	    (void) n; // unused
/*
no longer throw an error if we don't find everything
	
	    if (n != NIPCFG) {
		cfgFileError (ipcfn, n, (CfgPrFp)printf, ipcfg, NIPCFG);
		exit (1);
	    }
*/	
	    lastload = s.st_mtime;
	}
}

//
// Changing the location of the ip.cfg file:
// Caller should get and preserve current setting; change as needed, then
// restore original when done.
// This is done by WCS and Findstars cmdline programs.
// Library code default should remain in effect unless otherwise
// indicated.
//

char * getCurrentIpCfgPath(void)
{
	return ipcfn;
}

void setIpCfgPath(char *pathname)
{
	strncpy(ipcfn, pathname, sizeof(ipcfn));
}

/* For CVS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $Id: fitsip.c,v 1.20 2006/06/05 05:52:07 steve Exp $"};
