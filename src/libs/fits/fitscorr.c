/* functions to apply and create ccd bias, thermal and flat files.
 */

/* STO20010405	Modified by adding removeBadColumns() and findMapFN() functions.
   See calimage.c for more information
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "P_.h"
#include "astro.h"
#include "strops.h"
#include "telenv.h"
#include "fits.h"
#include "configfile.h"
#include "fitscorr.h"
static char def_caldir[] = "archive/calib";

static int findNextFITS (char *dirname, char *prefix, int gap, char file[],
    char errmsg[]);
static void incFile (char *file);
static int openCorrFile (FImage *matchfip, char fn[],
    int (*qualfp)(FImage *matchfip, FImage *fip, char errmsg[]),
    FImage *fip, char errmsg[]);
static int findLastFITS (char *dirname, char *prefix,
			 int (*qualfp)(FImage *matchfip, FImage *fip, char errmsg[]),
			 FImage *matchfip, int gap, char file[], char errmsg[]);
static int findLastSuffix (char *dirname, char *prefix,
			   int (*qualfp)(FImage *matchfip, FImage *fip, char errmsg[]),
			   FImage *matchfip, int gap, char file[], char errmsg[], char suffix[]);
static int chkDim (FImage *fip1, FImage *fip2, char errmsg[]);
static int subimage (FImage *fip1, FImage *fip2, int *x0p, int *y0p, int *wp,  char errmsg[]);

/* STO20010405 */
static int isMapTerm(BADCOL *p);
static int findNextSuffix (char *dirname, char *prefix, int gap, char file[], char errmsg[], char suffix[]);
static int nextbadcol(FILE *fp, BADCOL *bc);

/* STO 2002-10-07: These are now values (optionally) found in camera.cfg
 * If they are NOT defined in the config, they take the values of the previous defines
 * These values are updated when config is read by camera, xobs/Autofocus, or calimage
 */

#define	DEF_MAXMAXNEG					10000	/* largest PIXDC0 we will permit */
#define	DEF_DCBOR						32		/* border in which PIXDC0 is not performed */
#define DEF_ALLOW_SUBIMAGE_CALIBRATORS	1 		/* use pieces of whole images */
static int MAXMAXNEG					= DEF_MAXMAXNEG;
static int DCBOR						= DEF_DCBOR;
static int ALLOW_SUBIMAGE_CALIBRATORS	= DEF_ALLOW_SUBIMAGE_CALIBRATORS;

// available externally as globals
int useMeanBias = 0;
int useMeanTherm = 0;
int useMeanFlat = 0;

// Call this from the "initCfg" section of camera,autofocus,calimage
void readCorrectionCfg(int trace, char *cfgFile)
{
	static CfgEntry ccfg[] = {
	    {"MAXMAXNEG",	CFG_INT, &MAXMAXNEG},
	    {"DCBOR",		CFG_INT, &DCBOR},
	    {"ALLOWSUB",	CFG_INT, &ALLOW_SUBIMAGE_CALIBRATORS},
	    {"MEANBIAS",    CFG_INT, &useMeanBias},
        {"MEANTHERM",   CFG_INT, &useMeanTherm},
        {"MEANFLAT",    CFG_INT, &useMeanFlat}
	};

	readCfgFile (trace, cfgFile, ccfg, sizeof(ccfg)/sizeof(ccfg[0]));
}

/* apply bias/thermal/flat corrections to the given FITS file.
 * if any correction file names are NULL, try the standard places.
 * return 0 if ok else put a reason in errmsg and return -1.
 */
int
correctFITS (fip, biasfn, thermfn, flatfn, errmsg)
FImage *fip;
char biasfn[];
char thermfn[];
char flatfn[];
char errmsg[];
{
	FImage bias, therm, flat;	/* correction files */
	char bfn[512];			/* bias filename if none supplied */
	char tfn[512];			/* thermal filename if none supplied */
	char ffn[512];			/* flat filename if none supplied */
	double iexp;			/* EXPTIME of fip */
	double thermexp;		/* EXPTIME of therm */
	double thermfactor;		/* iexp/thermexp */
	double meanflat;		/* FLATMEAN of flat */
	CamPixel *fipp;			/* fast index into fip pixels */
	CamPixel *biasp;		/* fast index into bias pixels */
	CamPixel *thermp;		/* fast index into therm pixels */
	CamPixel *flatp;		/* fast index into flat pixels */
	double di, db, dt, df, dr;	/* fip, bias, therm, flat, result pix */
	double maxneg;			/* largest (smallest?) neg pixel value*/
	float *tmpim, *tmpimp;		/* temp image, and pointer */
	int x0, y0;			/* upper left within cal image */
	int bw, tw, fw;			/* widths of cal images */
	int iw, ih;			/* width and height of fip */
	int npixels;			/* total pixels in fip */
	char buf[80];
	int el;
	int r, c;

	/* make sure fip hasn't already been munged some way */
	if (getStringFITS (fip, "BIASCOR", buf) == 0
			    || getStringFITS (fip, "THERMCOR", buf) == 0
			    || getStringFITS (fip, "FLATCOR", buf) == 0) {
	    sprintf (errmsg, "Image has aleady had some corrections applied");
	    return (-1);
	}

	/* get the exposure time of the target image */
	if (getRealFITS (fip, "EXPTIME", &iexp) < 0) {
	    sprintf (errmsg, "No reference EXPTIME field");
	    return (-1);
	}

	/* get dimensions and compute size */
	if (getNAXIS (fip, &iw, &ih, errmsg) < 0)
	    return (-1);
	npixels = iw * ih;

	/* malloc and zero temp float array */
	tmpim = (float *) calloc (npixels, sizeof(float));
	if (!tmpim) {
	    free ((void *)tmpim);
	    sprintf (errmsg, "No room for float array");
	    return (-1);
	}

	/* gather up the correction files, trying defaults as necessary */
	if (!biasfn) {
	    if (findBiasFN (fip, NULL, bfn, errmsg) < 0) {
		free ((void *)tmpim);
		return (-1);
	    }
	    biasfn = bfn;
	}
	el = sprintf (errmsg, "%s: ", biasfn);
	if (openCorrFile (fip, biasfn, biasQual, &bias, errmsg+el) < 0) {
	    free ((void *)tmpim);
	    return (-1);
	}
	if (!thermfn) {
	    if (findThermFN (fip, NULL, tfn, errmsg) < 0) {
		resetFImage (&bias);
		free ((void *)tmpim);
		return (-1);
	    }
	    thermfn = tfn;
	}
	el = sprintf (errmsg, "%s: ", thermfn);
	if (openCorrFile(fip, thermfn, thermQual, &therm, errmsg+el) < 0) {
	    resetFImage (&bias);
	    free ((void *)tmpim);
	    return (-1);
	}
	if (!flatfn) {
	    if (findFlatFN (fip, 0, NULL, ffn, errmsg) < 0) {
		resetFImage (&therm);
		resetFImage (&bias);
		free ((void *)tmpim);
		return (-1);
	    }
	    flatfn = ffn;
	}
	el = sprintf (errmsg, "%s: ", flatfn);
	if (openCorrFile (fip, flatfn, flatQual, &flat, errmsg+el) < 0) {
	    resetFImage (&therm);
	    resetFImage (&bias);
	    free ((void *)tmpim);
	    return (-1);
	}

	/* get the thermal exposure time.
	 * and compute the thermal proportion
	 */
	if (getRealFITS (&therm, "EXPTIME", &thermexp) < 0) {
	    resetFImage (&therm);
	    resetFImage (&bias);
	    resetFImage (&flat);
	    free ((void *)tmpim);
	    sprintf (errmsg, "%s: No EXPTIME field", thermfn);
	    return (-1);
	}
	if (thermexp <= 0.0) {
	    resetFImage (&therm);
	    resetFImage (&bias);
	    resetFImage (&flat);
	    free ((void *)tmpim);
	    sprintf (errmsg, "%s: Bad EXPTIME field: %g", thermfn, thermexp);
	    return (-1);
	}
	thermfactor = iexp/thermexp;

	/* get, or compute if have to, the mean value of the flat */
	if (getRealFITS (&flat, "FLATMEAN", &meanflat) < 0)
	    computeMeanFITS (&flat, &meanflat);
	if (meanflat <= 0.0) {
	    resetFImage (&therm);
	    resetFImage (&bias);
	    resetFImage (&flat);
	    free ((void *)tmpim);
	    sprintf (errmsg, "%s: Bad Flat mean: %g", flatfn, meanflat);
	    return (-1);
	}

	/* init pointer into mem for correct scanning into each image.
	 * this allows for subimaged cal files, which is overkill unless
	 * ALLOW_SUBIMAGE_CALIBRATORS is defined, below.
	 */

	subimage (fip, &bias, &x0, &y0, &bw, errmsg);
	biasp = (CamPixel *)bias.image;
	biasp = &biasp[y0*bw + x0];

	subimage (fip, &therm, &x0, &y0, &tw, errmsg);
	thermp = (CamPixel *)therm.image;
	thermp = &thermp[y0*tw + x0];

	subimage (fip, &flat, &x0, &y0, &fw, errmsg);
	flatp = (CamPixel *)flat.image;
	flatp = &flatp[y0*fw + x0];

	tmpimp = tmpim;
	fipp = (CamPixel *) fip->image;


	/* do it !!
	 * also, watch for largest neg offset and shift back up when finished.
	 * ignore such offsets in a small border.
	 */
	maxneg = 0.0;
	for (r = 0; r < ih; r++) {
	    for (c = 0; c < iw; c++) {
		/* get the next set of pixels */
		di = (double)*fipp++;
		db = (double)*biasp++;
		dt = (double)*thermp++;
		df = (double)*flatp++;

		/* beware dead pixel in flat */
		if (df == 0.0)
		    df = 1.0;

		/* do the correction */
		dr = (di - db - dt*thermfactor)*meanflat/df;

		/* check for greater neg undershoot (up to -MAXMAXNEG)
		 * within DCBOR
		 */
		if (dr < maxneg && dr >= -MAXMAXNEG && c > DCBOR && r > DCBOR
						&& c < iw-DCBOR && r < ih-DCBOR)
		    maxneg = dr;

		/* assign and advance tmpimp */
		*tmpimp++ = (float)dr;
	    }

	    /* wrap calibrators when reach edge of fip */
	    biasp += bw - iw;
	    thermp += tw - iw;
	    flatp += fw - iw;
	}

	/* if any pixel went negative, add back so smallest becomes 0 */
	if (maxneg < 0.0) {
	    tmpimp = tmpim;
	    for (r = 0; r < npixels; r++)
		*tmpimp++ -= maxneg;
	    setRealFITS (fip, "PIXDC0", -maxneg, 6, "Residual bias");
	}

	/* now set image pixels -- beware of under and overflow */
	tmpimp = tmpim;
	fipp = (CamPixel *) fip->image;
	for (r = 0; r < npixels; r++) {
	    dr = *tmpimp++;
	    if (dr > MAXCAMPIX)
		dr = MAXCAMPIX;
	    else if (dr < 0.0)
		dr = 0.0;
	    *fipp++ = (CamPixel) dr;
	}

	/* finished with correction images */
	resetFImage (&flat);
	resetFImage (&therm);
	resetFImage (&bias);

	/* finished with temp float array */
	free ((void *)tmpim);

	/* add keywords to fip to mark as having been corrected */
	setStringFITS (fip, "BIASCOR", basenm(biasfn), "Bias file used");
	setStringFITS (fip, "THERMCOR", basenm(thermfn), "Thermal file used");
	setStringFITS (fip, "FLATCOR", basenm(flatfn),"Flat field file used");

	/* ok -- phew! */
	return (0);
}

/* search the given directory for the most recent .fts file that can serve
 *   as a bias correction frame for the given matchfip file.
 * if caldir is NULL, try the default place.
 * fill fn with full name and return 0, else fill errmsg and return -1.
 */
int
findBiasFN (matchfip, caldir, fn, errmsg)
FImage *matchfip;
char caldir[];
char fn[];
char errmsg[];
{
	return(findLastFITS (caldir, "cbs", biasQual, matchfip, 5, fn, errmsg));
}

/* search the given directory for the most recent .fts file that can serve
 *   as a thermal correction frame for the given matchfip file.
 * if caldir is NULL, try the default place.
 * fill fn with full name and return 0, else fill errmsg and return -1.
 */
int
findThermFN (matchfip, caldir, fn, errmsg)
FImage *matchfip;
char caldir[];
char fn[];
char errmsg[];
{
	return(findLastFITS(caldir, "cth", thermQual, matchfip, 5, fn, errmsg));
}

/* search the given directory for the most recent .fts file that can serve
 *   as a flat correction frame for the given matchfip file.
 * if caldir is NULL, try the default place.
 * if filter is 0, use keyword from matchfip else use filter.
 * fill fn with full name and return 0, else fill errmsg and return -1.
 */
int
findFlatFN (matchfip, filter, caldir, fn, errmsg)
FImage *matchfip;
int filter;		/* really a char but can't make prototype work :-Z */
char caldir[];
char fn[];
char errmsg[];
{
	char prefix[10];

	/* we need a filter code for the filename prefix.
	 * get it from the FILTER keyword of matchfip unless we were passed one.
	 */
	if (!filter) {
	    char filrow[100];
	    if (getStringFITS (matchfip, "FILTER", filrow) < 0) {
		strcpy (errmsg, "No FILTER keyword in input image");
		return (-1);
	    }
	    filter = filrow[0];
	}
	sprintf (prefix, "cf%c", filter);

	return(findLastFITS(caldir, prefix, flatQual, matchfip, 5, fn, errmsg));
}

/* search the given directory for the most recent .map file that can serve
 *   as a bad column map * if caldir is NULL, try the default place.
 * fill fn with full name and return 0, else fill errmsg and return -1.
 */
int findMapFN (caldir, fn, errmsg)char caldir[];char fn[];char errmsg[];
{
  return(findLastSuffix (caldir, "bad", NULL, NULL, 5, fn, errmsg, ".map"));
}

/* search the given directory for the most recent .fts file that can serve
 *   as a bias correction frame for any image and fill fn[] with the name
 *   of a new file that follows in sequence.
 * We do this just based on file names; we don't qualify the contents of any
 *   existing files that happen to match.
 * if caldir is NULL, try the default place.
 * fill fn with full name and return 0, else fill errmsg and return -1.
 */
int
findNewBiasFN (caldir, fn, errmsg)
char caldir[];
char fn[];
char errmsg[];
{
	return (findNextFITS (caldir, "cbs", 5, fn, errmsg));
}

/* search the given directory for the most recent .fts file that can serve
 *   as a thermal correction frame for any image and fill fn[] with the name
 *   of a new file that follows in sequence.
 * We do this just based on file names; we don't qualify the contents of any
 *   existing files that happen to match.
 * if caldir is NULL, try the default place.
 * fill fn with full name and return 0, else fill errmsg and return -1.
 */
int
findNewThermFN (caldir, fn, errmsg)
char caldir[];
char fn[];
char errmsg[];
{
	return (findNextFITS (caldir, "cth", 5, fn, errmsg));
}

/* search the given directory for the most recent .fts file that can serve
 *   as a flat correction frame for any image with the given filter code
 *   and fill fn[] with the name of a new file that follows in sequence.
 * We do this just based on file names; we don't qualify the contents of any
 *   existing files that happen to match.
 * if caldir is NULL, try the default place.
 * fill fn with full name and return 0, else fill errmsg and return -1.
 */
int
findNewFlatFN (filter, caldir, fn, errmsg)
int filter;	/* really a char but can't make a prototype work :-Z */
char *caldir;
char *fn;
char *errmsg;
{
	char prefix[10];

	sprintf (prefix, "cf%c", filter);
	return (findNextFITS (caldir, prefix, 5, fn, errmsg));
}

/* STO20010405
 * find the most recent .map file that can serve as a bad column map
 * and fill fn[] with the name of the new file that follows in sequence.
 * This is done solely on filenames.
 * if caldir is NULL, try the default place.
 * fill fn with full name and return 0, else fill errmsg and return -1.
 */
int findNewMapFN(char caldir[], char fn[], char errmsg[])
{
  return (findNextSuffix(caldir,"bad",5, fn, errmsg,".map"));
}

/* return 0 if fip2 can serve as a bias correction file for fip1.
 * else fill in errmsg and return -1.
 */
int
biasQual (fip1, fip2, errmsg)
FImage *fip1;
FImage *fip2;
char errmsg[];
{
	if (chkDim (fip1, fip2, errmsg) < 0)
	    return (-1);
	if (getStringFITS(fip2, "BIASFR", errmsg) < 0) {
	    sprintf (errmsg, "File must contain BIASFR keyword");
	    return (-1);
	}
	return (0);
}

/* return 0 if fip2 can serve as a thermal correction file for fip1.
 * else fill in errmsg and return -1.
 */
int
thermQual (fip1, fip2, errmsg)
FImage *fip1;
FImage *fip2;
char errmsg[];
{
	double dur;

	if (chkDim (fip1, fip2, errmsg) < 0)
	    return (-1);
	if (getStringFITS(fip2, "THERMFR", errmsg) < 0) {
	    sprintf (errmsg, "File must contain THERMFR keyword");
	    return (-1);
	}
	if (getRealFITS (fip2, "EXPTIME", &dur) < 0 || dur <= 0) {
	    sprintf (errmsg, "EXPTIME must be > 0");
	    return (-1);
	}
	return (0);
}

/* return 0 if fip2 can serve as a flat correction file for fip1.
 * else fill in errmsg and return -1.
 */
int
flatQual (fip1, fip2, errmsg)
FImage *fip1;
FImage *fip2;
char errmsg[];
{
	if (chkDim (fip1, fip2, errmsg) < 0)
	    return (-1);
	if (getStringFITS(fip2, "FLATFR", errmsg) < 0) {
	    sprintf (errmsg, "File must contain FLATFR keyword");
	    return (-1);
	}
	return (0);
}

/* set *mp to the mean pixel value within fip.
 */
void
computeMeanFITS (fip, mp)
FImage *fip;
double *mp;
{
	unsigned short *image;
	unsigned short *last;
	char errmsg[1024];
	int iw, ih;
	int npixels;
	double sum;

	(void) getNAXIS (fip, &iw, &ih, errmsg);/* error "can't happen" */
	npixels = iw*ih;
	image = (unsigned short *)fip->image;
	last = &image[npixels];

	for (sum = 0.0; image < last; )
	    sum += (double)(*image++);

	*mp = sum/npixels;
}

/* follows is a family of functions which create new correction files */

/* create a float accumulator for use with correction file creation funcs.
 * return NULL if no mem.
 */
float *
nc_makeFPAccum(int npixels)
{
	float *ap = (float *) calloc (npixels, sizeof(float));
	return (ap);
}

/* add the n pixels in im[] to acc[] */
void
nc_accumulate (int n, float *acc, CamPixel *im)
{
	while (--n >= 0)
	    *acc++ += (float)(*im++);
}

/* subtract the n pixels in im[] from acc[] */
void
nc_subtract (int n, float *acc, CamPixel *im)
{
	while (--n >= 0)
	    *acc++ -= (float)(*im++);
}

/* subtract the n pixels in im[] from acc[] after scaling each by fact */
void
nc_fsubtract (int n, float *acc, CamPixel *im, double fact)
{
	while (--n >= 0)
	    *acc++ -= (float)((double)(*im++)*fact);
}

/* divide each of the n pixels in acc[] by div */
void
nc_accdiv (int n, float *acc, int div)
{
	while (--n >= 0)
	    *acc++ /= div;
}

/* convert the n pixels in acc[] to im[] */
void
nc_acc2im (int n, float *acc, CamPixel *im)
{
	while (--n >= 0) {
	    double v = (double)(*acc++);
	    if (v > MAXCAMPIX)
		v = MAXCAMPIX;
	    if (v < 0.0)
		v = 0.0;
	    *im++ = (CamPixel) floor(v + .5);
	}
}

/* add the standard header info for a bias frame */
void
nc_biaskw (FImage *fip, int ntot)
{
	char buf[1024];

	sprintf (buf, "Bias frame: Averaged=%d", ntot);
	setStringFITS (fip, "BIASFR", buf, NULL);
}

/* add the standard header info for a thermal frame */
void
nc_thermalkw (FImage *fip, int ntot, char biasfn[])
{
	char buf[1024];

	sprintf (buf, "Thermal frame: Averaged=%d Bias=%s",ntot,basenm(biasfn));
	setStringFITS (fip, "THERMFR", buf, NULL);
}

/* add the standard header info for a flat frame */
void
nc_flatkw (FImage *fip, int ntot, char biasfn[], char thermfn[], int filter)
{
	char buf[1024];
	double mean;

	sprintf (buf, "Flat frame: Averaged=%d Bias=%s Thermal=%s",
				    ntot, basenm(biasfn), basenm(thermfn));
	setStringFITS (fip, "FLATFR", buf, NULL);

	computeMeanFITS (fip, &mean);
	setRealFITS (fip, "FLATMEAN", mean, 8, "Mean pixel in flat");

	sprintf (buf, "%c", filter);
	setStringFITS (fip, "FILTER", buf, "Filter code");
}

/* apply to acc[n] the bias named in biasfn[].
 * return 0 if ok, else excuse in msg[] and -1.
 * N.B. we *only* check that biasfn[] has at least n pixels.
 */
int
nc_applyBias(int n, float *acc, char biasfn[], char msg[])
{
	FImage bim;
	int w, h;
	int fd;
	int s;

	/* open and sanity-check */
	fd = open (biasfn, O_RDONLY);
	if (fd < 0) {
	    sprintf (msg, "%s", strerror(errno));
	    return (-1);
	}
	s = readFITS (fd, &bim, msg);
	(void) close (fd);
	if (s < 0)
	    return (-1);
	if (getNAXIS (&bim, &w, &h, msg) < 0) {
	    resetFImage (&bim);
	    return (-1);
	}
	if (n > w*h) {
	    sprintf (msg, "Bias image too small!");
	    resetFImage (&bim);
	    return (-1);
	}

	/* subtract bias */
	nc_subtract (n, acc, (CamPixel *)bim.image);
	resetFImage (&bim);

	return (0);
}

/* apply to acc[n] the thermal named in thermfn[].
 * acc was taken with a duration of dur seconds.
 * return 0 if ok, else excuse in msg[] and -1.
 * N.B. we *only* check that thermfn[] has at least n pixels.
 */
int
nc_applyThermal(int n, float *acc, double dur, char thermfn[], char msg[])
{
	double tdur;
	FImage tim;
	int w, h;
	int fd;
	int s;

	/* open and sanity-check */
	fd = open (thermfn, O_RDONLY);
	if (fd < 0) {
	    sprintf (msg, "%s", strerror(errno));
	    return (-1);
	}
	s = readFITS (fd, &tim, msg);
	(void) close (fd);
	if (s < 0)
	    return (-1);
	if (getNAXIS (&tim, &w, &h, msg) < 0) {
	    resetFImage (&tim);
	    return (-1);
	}
	if (n > w*h) {
	    sprintf (msg, "Thermal image too small!");
	    resetFImage (&tim);
	    return (-1);
	}

	/* fetch the thermal's duration */
	if (getRealFITS (&tim, "EXPTIME", &tdur) < 0) {
	    sprintf (msg, "no EXPTIME in supposed thermal");
	    resetFImage (&tim);
	    return (-1);
	}

	/* subtract the thermal, scaled by duration */
	nc_fsubtract (n, acc, (CamPixel *)tim.image, dur/tdur);
	resetFImage (&tim);

	return (0);
}

/* STO20010405: Modified to allow different suffix, made findNextFITS use it with
   a ".fts" suffix for compatability.  Used by findNextMapFN.
*/
/* look through a directory and find the .fts file with the given prefix that
 *   sorts last according to the gap chars between the prefix and the .fts
 *   suffix. Then increment the chars in the gap so it would be the "next"
 *   such file. * if dirname is NULL, use the default place.
 * fill file with full name and return 0, else fill errmsg and return -1.
 */
static int findNextFITS(char * dirname, char * prefix, int gap, char file[], char errmsg[])
{
  return findNextSuffix(dirname, prefix, gap, file, errmsg, ".fts");
}

/* look through a directory and find the (suffix) file with the given prefix that
 *   sorts last according to the gap chars between the prefix and the (suffix)
 *   suffix. Then increment the chars in the gap so it would be the "next"
 *   such file.
 * if dirname is NULL, use the default place.
 * fill file with full name and return 0, else fill errmsg and return -1.
 */
static int
findNextSuffix (dirname, prefix, gap, file, errmsg, suffix)
char *dirname;
char *prefix;
int gap;
char file[];
char errmsg[];char suffix[];
{
/*	static char suffix[] = ".fts"; */
	char teldirname[1024];
	struct dirent *ep;
	char fn[128];
	DIR *dp;
	int prefl;	/* length of prefix */
	int suffl;	/* length of suffix */
	int cmpl;	/* length of prefix plus gap */
	int totl;	/* total length of filename we are considering */
	int found;

	if (!dirname)
	    dirname = def_caldir;
	telfixpath (teldirname, dirname);
	dp = opendir (teldirname);
	if (!dp) {
	    sprintf (errmsg, "%s: %s", teldirname, strerror(errno));
	    return (-1);
	}

	prefl = strlen (prefix);
	suffl = strlen (suffix);
	cmpl = prefl + gap;
	totl = prefl + gap + suffl;
	found = 0;
	fn[0] = '\0';	/* this will sort before anything */
	while ((ep = readdir (dp)) != NULL) {
	    char *name = ep->d_name;

	    /* check for the name being later */
	    if (strlen (name) == totl
				    && strcasecmp (name+cmpl, suffix) == 0
				    && strncasecmp (name, prefix, prefl) == 0
				    && strncasecmp (name, fn, cmpl) > 0) {
		/* ok, this is a good one */
		strcpy (fn, name);
		found = 1;
	    }
	}

	(void) closedir (dp);
	if (!found) {
	    /* no files found at all, so set to a default initial value.
	     * incFile() will actually fill in today's date
	     */
	    sprintf (fn, "%s00000%s", prefix, suffix);
	}

	/* increment name */
	incFile (fn);

	/* caller wants full path */
	sprintf (file, "%s/%s", teldirname, fn);
	return (0);
}

/* given a filename of the form XXXMDDNN.fts, where:
 *   XXX is any three letter prefix, which we leave unchanged;
 *   MDD is a date code where M is the month in hex and DD is the decimal date;
 *   NN is a hex sequence number;
 * set file to the next name that would follow in sequence. if today is MDD
 *   then we increment NN, else we set MDD to today and NN to 00.
 */
static void
incFile (file)
char *file;
{
	struct tm *tmp;
	char today[4];
	time_t t;

	(void) time (&t);
	tmp = gmtime (&t);
	sprintf (today, "%x%02d", tmp->tm_mon+1, tmp->tm_mday);
	if (strncasecmp (file+3, today, 3) == 0) {
	    int nn;
	    sscanf (file+6, "%x", &nn);
	    sprintf (file+6, "%02X", nn+1);
	} else
	    sprintf (file+3, "%s00", today);

	file[8] = '.';	/* put back what sprintf clobbered with it's \0 */
}

/* see if the given fn is a FITS file that can be used as a correction file
 *   for matchfip, based on the requirements imposed by qualfp.
 * if it can, fill in fip and return 0, else fill errmsg with a clue why not
 *   and return -1.
 */
static int
openCorrFile (matchfip, fn, qualfp, fip, errmsg)
FImage *matchfip;
char fn[];
int (*qualfp)(FImage *matchfip, FImage *fip, char errmsg[]);
FImage *fip;
char errmsg[];
{
	int fd;

	fd = telopen (fn, O_RDONLY);
	if (fd < 0) {
	    sprintf (errmsg, "Error opening %s: %s", fn, strerror(errno));
	    return (-1);
	}
	initFImage (fip);
	if (readFITS (fd, fip, errmsg) < 0) {
	    (void) close (fd);
	    return (-1);
	}
	if ((*qualfp) (matchfip, fip, errmsg) < 0) {
	    resetFImage (fip);
	    (void) close (fd);
	    return (-1);
	}
	(void) close (fd);
	return (0);
}
/* STO20010405
   Modified to accept a different suffix, and made findLastFITS compatible
   Used by findMapFN
*/
static int findLastFITS (dirname, prefix, qualfp, matchfip, gap, file, errmsg)
char *dirname;
char *prefix;
int (*qualfp)(FImage *matchfip, FImage *fip, char errmsg[]);
FImage *matchfip;
int gap;
char file[];
char errmsg[];
{
  return findLastSuffix(dirname, prefix, qualfp, matchfip, gap, file, errmsg, ".fts");
}

/* look through a directory and find the (suffix) file with the given prefix and
 *   qualifications with respect to matchfip that sorts last according to the
 *   gap chars between the prefix and the (suffix) suffix..
 * if dirname is NULL, use the default place.
 * return 0 and fill in fn if find one, else -1 and fill in errmsg[] if trouble.
 * N.B. file[] will contain the full name, including dirname.
 */
static int
findLastSuffix (dirname, prefix, qualfp, matchfip, gap, file, errmsg, suffix)
char *dirname;
char *prefix;
int (*qualfp)(FImage *matchfip, FImage *fip, char errmsg[]);
FImage *matchfip;
int gap;
char file[];
char errmsg[];char suffix[];
{
	/* static char suffix[] = ".fts"; */
	char teldirname[1024];
	struct dirent *ep;
	DIR *dp;
	FImage fimage;
	int prefl;	/* length of prefix */
	int suffl;	/* length of suffix */
	int cmpl;	/* length of prefix plus gap */
	int totl;	/* total length of filename we are considering */
	int found;

	if (!dirname)
	    dirname = def_caldir;
	telfixpath (teldirname, dirname);
	dp = opendir (teldirname);
	if (!dp) {
	    sprintf (errmsg, "%s: %s", teldirname, strerror(errno));
	    return (-1);
	}

	initFImage (&fimage);

	prefl = strlen (prefix);
	suffl = strlen (suffix);
	cmpl = prefl + gap;
	totl = prefl + gap + suffl;
	found = 0;
	file[0] = '\0';	/* this will sort before anything */
	while ((ep = readdir (dp)) != NULL) {
	    char *name = ep->d_name;
	    char fn[2048];
	    int qual = -666;
	    int fd;

	    /* check for the name being ok */
	    if (!(strlen (name) == totl
				    && strcasecmp (name+cmpl, suffix) == 0
				    && strncasecmp (name, prefix, prefl) == 0
				    && strncasecmp (name, file, cmpl) > 0))
		continue;

	    /* name qualifies; now check whatever.
	     * just keep going if file is bad some how.
	     */
	    sprintf (fn, "%s/%s", teldirname, name);
	    fd = open (fn, O_RDONLY);
	    if (fd < 0)
		continue;

	    if(matchfip && qualfp)
	    {
	      if (readFITSHeader (fd, &fimage, fn) == 0
	      && (qual = (*qualfp)(matchfip, &fimage, errmsg)) == 0)
	      {
	       	strcpy (file, fn);	/* ok, this is a good one */
	       	found = 1;
	      }
	    }
	    else
	    {
	      strcpy (file, fn);	/* ok, this is a good one */
	      found = 1;
	    }

#ifdef FINDLAST_TRACE
printf ("%s:%s found=%d\n", fn, qual < 0 ? errmsg : " ", found);
#endif

	    resetFImage (&fimage);
	    (void) close (fd);
	}

	(void) closedir (dp);
	if (!found) {
	    sprintf (errmsg, "No suitable %s*%s files found in %s",
							    prefix, suffix, teldirname);
	    return (-1);
	}
	return (0);
}

/* return 0 if both images are such that the latter can server as a corrector
 * for the former. Only binning must match because we can cut out subparts.
 * else, fill errmsg[] and return -1.
 */
static int
chkDim (fip1, fip2, errmsg)
FImage *fip1;
FImage *fip2;
char errmsg[];
{
	int x1, y1, x2, y2;

	/* check binning */
	if (getIntFITS (fip1, "XFACTOR", &x1) < 0)
	    x1 = 1;	/* assume 1:1 if absent */
	if (getIntFITS (fip1, "YFACTOR", &y1) < 0)
	    y1 = 1;	/* assume 1:1 if absent */
	if (getIntFITS (fip2, "XFACTOR", &x2) < 0)
	    x2 = 1;	/* assume 1:1 if absent */
	if (getIntFITS (fip2, "YFACTOR", &y2) < 0)
	    y2 = 1;	/* assume 1:1 if absent */
	if (x1 != x2 || x2 != y2) {
	    sprintf (errmsg, "Binning must match: %dx%d vs %dx%d",
							x1, y1, x2, y2);
	    return (-1);
	}

/* define the following if we want to allow using larger calibration frames
 * and dig out a subframe .. if don't trust the camera to make artifacts
 * which depend on subimages then leave this undefined and we will insist on
 * calibration frames exactly matching the test image.
 */
#ifdef ALLOW_SUBIMAGE_CALIBRATORS
	/* check (sub)dimensions */
	if (subimage (fip1, fip2, NULL, NULL, NULL, errmsg) < 0)
	    return (-1);
#else
	/* check dimensions */
	if (getNAXIS (fip1, &x1, &y1, errmsg) < 0)
	    return (-1);
	if (getNAXIS (fip2, &x2, &y2, errmsg) < 0)
	    return (-1);
	if (x1 != x2 || y1 != y2) {
	    sprintf (errmsg, "Dimensions must match: %dx%d vs %dx%d",
							    x1, y1, x2, y2);
	    return (-1);
	}

	/* check offsets */
	if (getIntFITS (fip1, "OFFSET1", &x1) < 0)
	    x1 = 0;	/* assume 0 */
	if (getIntFITS (fip1, "OFFSET2", &y1) < 0)
	    y1 = 0;	/* assume 0 */
	if (getIntFITS (fip2, "OFFSET1", &x2) < 0)
	    x2 = 0;	/* assume 0 */
	if (getIntFITS (fip2, "OFFSET2", &y2) < 0)
	    y2 = 0;	/* assume 0 */
	if (x1 != x2 || y1 != y2) {
	    sprintf (errmsg, "Offsets must match: %dx%d vs %dx%d",
							x1, y1, x2, y2);
	    return (-1);
	}
#endif /* ALLOW_SUBIMAGE_CALIBRATORS */

	/* ok */
	return (0);
}

/* if fip1 is wholy within fip2, find the location of 1 within 2.
 * if ok return 0, else a message and -1.
 * any of x0p/y0p/wp may be NULL if just want to check the potential.
 *
 *   Entire camera frame:
 *   -----------------------------
 *   |                           |
 *   |                           |
 *   |    fip2:                  |
 *   |    -------------------    |
 *   |    |                 |    |
 *   |    |    fip1:        |    |
 *   |    |    -----------  |    |
 *   |    |    |         |  |    |
 *   |    |    |         |  |    |
 *   |    |    |         |  |    |
 *   |    |    -----------  |    |
 *   |    |                 |    |
 *   |    -------------------    |
 *   |                           |
 *   -----------------------------
 */
static int
subimage (fip1, fip2, x0p, y0p, wp, errmsg)
FImage *fip1;	/* raw frame */
FImage *fip2;	/* calibration frame */
int *x0p, *y0p;	/* where fip1[0,0] is in fip2 */
int *wp;	/* width of fip2 */
char errmsg[];
{
	int n1w, n1h, n2w, n2h;		/* width and height */
	int o1h, o1v, o2h, o2v;		/* hor and ver offsets */

	/* get dimensions */
	if (getNAXIS (fip1, &n1w, &n1h, errmsg) < 0)
	    return (-1);
	if (getNAXIS (fip2, &n2w, &n2h, errmsg) < 0)
	    return (-1);

	/* get offsets */
	if (getIntFITS (fip1, "OFFSET1", &o1h) < 0)
	    o1h = 0;	/* assume 0 if absent */
	if (getIntFITS (fip1, "OFFSET2", &o1v) < 0)
	    o1v = 0;	/* assume 0 if absent */
	if (getIntFITS (fip2, "OFFSET1", &o2h) < 0)
	    o2h = 0;	/* assume 0 if absent */
	if (getIntFITS (fip2, "OFFSET2", &o2v) < 0)
	    o2v = 0;	/* assume 0 if absent */

	/* see whether fip1 is wholey contained within fip2 */
	if (		   o1h < o2h 			/* left edge */
			|| o1v < o2v			/* top edge */
			|| o2h + n2w < o1h + n1w	/* right edge */
			|| o2v + n2h < o1v + n1h	/* bottom edge */ ) {
	    (void) sprintf (errmsg,"Image not wholey within calibration frame");
	    return (-1);
	}

	/* ok */
	if (x0p) *x0p = o1h - o2h;
	if (y0p) *y0p = o1v - o2v;
	if (wp)  *wp = n2w;
	return (0);
}
/* STO20010405 */
/* Remove bad columns
   remove bad columns as indicated in map by replacing the
   column pixel with the mean of the adjacent pixels.

   Notes:   Adjacent bad columns would result in a skewed median
   because the value from column to the right will be invalid.
   To mitigate this, the span of adjacent columns is skipped
   so that all the dead pixels in the intervening span represent
   the mean of the two adjacent non-dead pixels.

   returns 0 on success
*/
int removeBadColumns(FImage *fip, BADCOL *bcMap, char *mapnameused, char *errmsg)
{
  CamPixel *	base;
  CamPixel *    pPix;
  BADCOL *	pMap;
  BADCOL *	pMapA;
  int	       	width,height;
  int	       	end;
  CamPixel	avg;
  long		adj0,adj1;	/* long so we can test if we filled it */
  int	       	cnt;		/* test for adjacent mapped columns */
  char		buf[80];

  /* See if we've done this already. If so, reject this */
  if( getStringFITS(fip, "BADPXCOR", buf) == 0 ) {
    sprintf(errmsg, "(warning): Image has already had bad column corrections applied, no further columns removed.");
    // NOT an error!  return(-1);
    return(0);
  }

  pMap = bcMap;

  while(!isMapTerm(pMap)) {
    /* init ladj/radj here. It is the # of columns to the left/right the next valid column is.
       Normally 1, but will be different if we have to skip adjacent bad columns
    */
	pMap->ladj = pMap->radj = 1;
	pMapA = bcMap;
	while(!isMapTerm(pMapA)) {
	  if(pMapA->column == pMap->column-1 || pMapA->column == pMap->column+1) {
	    //fprintf(stderr,"warning: adjacent columns found in bad column map at %d and %d\n",pMap->column, pMapA->column);
	    if(pMapA->column > pMap->column)
		pMap->radj++;
	    else
		pMap->ladj++;
	   }
	   pMapA++;
	 }
	 pMap++;
	}

	base = (CamPixel *) fip->image;	width = fip->sw;
	height = fip->sh;
	/* Process the bad pixels */
	pMap = bcMap;
	cnt = 0;
	while(!isMapTerm(pMap)) {
		pPix = base + (pMap->begin * width) + pMap->column;
		/* test for clip */
		if(pMap->column < width && pMap->begin < height)
		{
			/* constrain to image */
			end = pMap->end;
			if(end > height) end = height;
			/* process column run */
			cnt++; // count the ones we actually use!
			while(end-- >= pMap->begin) {
				adj0 = adj1 = -1; /* fill with a number that can't come from the pix data */
				if(pMap->column > 0) {
					adj0 = *(pPix - pMap->ladj);	/* get column to left */
				}
				if(pMap->column < width-1) {
					adj1 = *(pPix + pMap->radj);	/* get column to right */
				}
				/* pixels at edge are mirrors of inside adjacent column */
				if(adj0 == -1) adj0 = adj1;
				if(adj1 == -1) adj1 = adj0;
				/* compute the mean of the adjacent pixels */
				avg = (CamPixel) ((adj0 + adj1) / 2);
				/* replace the pixel with this */
				*pPix = avg;
				/* next row */
				pPix += width;
			}
		}
		/* next map entry */
		pMap++;
	}
	if(cnt) // if we TOUCHED it, then record this in the FITS header
	{
		if(!mapnameused) mapnameused = "(unspecified)";
		setStringFITS (fip, "BADPXCOR", mapnameused, "Bad Column Map file applied");
	}
	return(0); /* all ok */
}

/* test for bad column map terminator record
 	terminator record is all zeros.*/
static int isMapTerm(BADCOL *p)
{
  if(!p) return 1;

  if(p->column != 0 || p->begin !=0 || p->end != 0)
    return 0;
return 1;

}


/******************************************\
	Bad Column Fix Code
\******************************************/

/****
*
 Column map is in ascii.
 syntax:
 <c> <b> <e>
 where <c> is column number, <b> is beginning row, and <e> is ending row
 separated by one or more spaces
 example:

 32		0	1024		! column 32, rows 0 - 1024
 102	48	328			! column 102, rows 48 - 328
 102	400	403			! column 102, rows 400 - 403

 all characters after a comment (! or #) are ignored to end of line
*
*****/


/* Read all bad columns from map file and return as array in bcOut */
/* bcOut array has a terminator at end where col, begin, end all = 0 */
/* return is count of corrections, which may be zero, or < 0 on error */
/* the bcOut array MUST be freed by the caller if count > 0 */
int readMapFile(char *dirname, char *mapfile, BADCOL **bcOut, char *mapnameused, char *errmsg)
{
	FILE *fp;
	BADCOL	bc;
	int cnt;
	BADCOL *pBadList;
	BADCOL *pBC;
	char catm[1024];

	if (!mapfile) {
		if (findMapFN (dirname, catm, errmsg) < 0) {
			// don't echo an error... this is a "soft" error and we're okay with not having a map file...
			// fprintf (stderr, "%s: %s\n", catm, errmsg);
			return (-1);
		}
		mapfile = catm;
		/*DB fprintf(stderr,"map file found: %s\n",mapfile); */
	}

	/* send back the name of the file we used because it will end up in FITS header */
	strcpy(mapnameused,basenm(mapfile));

	fp = fopen(mapfile, "r");
	if(!fp) {
		sprintf(errmsg, "unable to read bad column map file %s\n",mapfile);
		return(-1);
	}

	cnt = 0;
	while(!nextbadcol(fp,&bc)) {
		cnt++;
	}

	if(cnt > 0)
	{
		fseek(fp, 0, SEEK_SET);
		pBadList = (BADCOL *) malloc((cnt+1) * sizeof(BADCOL)); // add one for terminator
		pBC = pBadList;
		while(!nextbadcol(fp, pBC)) { pBC++; }
		// terminate
		pBC->column = pBC->begin = pBC->end = 0;
		*bcOut = pBadList;
	}
	else
	{
		sprintf(errmsg,"no column map descriptions in file %s\n",mapnameused);
	}

	fclose(fp);

	return(cnt);
}

/* Read next bad column triplet values from map file */
/* return -1 on error, 0 if success */
static int nextbadcol(FILE *fp, BADCOL *bc)
{
	char c;
	char value[3][8];
	int  val,which;

	which = val = 0;
	while(1) {
		c = getc(fp);
		/* skip white space */
		if(c <= 32) {
			while(c != EOF && c <= 32) {
				c = getc(fp);
			}
		}

		/* Read value */
		if(isdigit(c)) {
			while(isdigit(c)) {
				value[which][val++] = c;
				c = getc(fp);
			}
			value[which][val] = '\0';
			val = 0;
			which++;
			if(which >= 3)
			{
				bc->column = atoi(value[0]);
				bc->begin = atoi(value[1]);
				bc->end = atoi(value[2]);

				/*DB
				fprintf(stderr, "COL: %d  %d-%d\n",bc->column, bc->begin, bc->end);
				*/

				return (0);
			}
		}
		else
		{
			/* skip to end of line on comment */
			while(c != EOF && c != '\n' && c != '\r') {
				c = getc(fp);
			}
		}

		if(c == EOF) return(-1);
	}
}


/* For CVS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $Id: fitscorr.c,v 1.6 2002/10/23 21:46:53 steve Exp $"};
