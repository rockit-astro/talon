/* New version of libwcs/findreg.c
 *
 * Ideas are:
 * (a) try matching by
 *      (i) using distances between pairs of stars rather than by comparing
 *          similar triangles
 *     (ii) when an approximate fit has been found, match using absolute
 *          positions, which is quicker when it works
 * (b) try improving accuracy of astrometry by
 *      (i) using more parameters in astrometric fit
 *     (ii) allowing projection centre to be read from FITS header rather
 *          than assuming it to be at centre of FITS image (for individual
 *          chips in mosaic CCD cameras, or cropped images, projection centre
 *          will generally not equal image centre; this can matter for wide
 *          fields)
 *
 * Name of function findRegistration is changed to findRegistrationD (D for
 * "distance" matching) and this new function is called with six extra
 * arguments, five of which correspond to config params MINPAIR, TRYSTARS,
 * MAXROT, MATCHDIST, REJECTDIST.  The other, nparam, is the number of
 * parameters in the astrometric fit.  If nparam = 5, distance matching is
 * attempted and if successful a 5-parameter WCS fit is determined, as
 * previously (except the previous version attempted "triangle" based
 * matching).  If nparam = 12, 20 or 26, a 5-parameter WCS fit is assumed to
 * exist already in the input fits header, and findRegistrationD uses
 * position matching to (try to) firstly get an updated WCS fit and then do a
 * 12-parameter quadratic fit, 20-parameter cubic fit, or fit using 26
 * parameters corresponding to those used in the Digitized Sky Survey.
 *
 * Projection centre is input ra0, dec0 for WCS fit.  For higher order fits,
 * default is ra0, dec0, but it can be chosen explicitly by having fields
 * PROJCEN1, PROJCEN2 in input FITS header, specifying x,y pixel where
 * projection centre is expected to be (needn't be exact; anything roughly ok
 * should allow reasonable correction to distortion).
 *
 * Previous version 020917
 * 021219:  improve higher order fit finding (e.g. remember any fit we get,
 *             since sometimes we'd been discarding fits and continuing
 *             unsuccessfully to search for better ones)
 * 021220:  untangle gnuplot spaghetti by creating separate function plotGNU
 *             so we don't loop back to middle of code
 *          use MINPAIR, MAXROT from ip.cfg
 * Last update DJA 021220
 */

/* find best WCS C* registration of two sets of stars.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <string.h>

#include "P_.h"
#include "astro.h"
#include "fits.h"
#include "wcs.h"
#include "lstsqr.h"


#define THRESH          1.5	/* discard resids > THRESH*median */
                                /* or > max/THRESH, in higher order fits */
                                /* (in both cases, only if above limit) */
#define FTOL     0.0005  /* frac change in chisqr() we call a good fit */
#define NOUTLL      6    /* max number of loops to discard outliers */
#define NENLARGE    6    /* when doing higher order fits, max number of loops
                          * to try to bring in extra stars (i.e. a series of
                          * progressively better fits may each bring more
                          * stars within range in a new position match);
                          * NOUTLL loops are done within NENLARGE loops */

/* Move above #define's to ip.cfg if you find you're often changing them */

/* Edit findregd.h if want tracing information */
#include "findregd.h"

/* these values are made file-global for use by the chisqr evaluator */
static double resid_max, resid_sum, resid_sum2;
static double *resid_g;
static double *sx_g;
static double *sy_g;
static double *gr_g;
static double *gd_g;
static double *gx_g;
static double *gy_g;
static double psx0_g, psy0_g;
static FImage fim_g;
static int npair_g;

static void init_fim (FImage *fip);
static int call_lstsqr (double *t_ra, double *t_dc, double *t_th, double *t_sx,
    double *t_sy);
static int call_lstsqr2 (double a[], double b[]);
static int call_lstsqr3 (double a[], double b[]);
static int call_lstsqrDSS (double a[], double b[]);
static void setFITSWCS (FImage *fip, double ra, double dec, double rot,
    double pixszw, double pixszh);
static void setFITSastrom (FImage *fip, double a[], double b[]);
/* N.B. double a[] in dmedian different from other double a[]'s */
static void dmedian (double a[], int na, double *mp);
#define GNUPLOT_TRACE
#ifdef GNUPLOT_TRACE
static void plotGNU (FImage *fip, double t_sx, double t_sy,
    double sx[], double sy[], int ns,
    double gr[], double gd[], double gx[], double gy[], int ng,
    double smx[], double smy[], double gmr[], double gmd[],
    double gmx[], double gmy[], int npair, int npmax,
    double a[], double b[], int nparam, int hiordok);
#endif
static void matchByDist (double sx[], double sy[], int ns,
    double gr[], double gd[], double gx[], double gy[], int ng,
    double x2as, double y2as, double MATCHDIST, int nenough, int npmax,
    int mats[], int matg[], int *np);
static void matchByPos (double sx[], double sy[], int ns,
    double gr[], double gd[], double gx[], double gy[], int ng,
    double xsc, double ysc, double MATCHDIST, int npmax,
    int mats[], int matg[], int *np);
static void xy2xieta
    (double a[], double b[], double x, double y, double *xi, double *eta);
static void RADec2xieta (double rc, double dc, int n, double *r, double *d,
    double *xi, double *eta);

#ifdef FITS_SOLUTION_TRACE
static int getAMDFITS (FImage *fip, double a[], double b[]);
#endif

#ifdef THRESH_TRACE
static void xieta2RADec (double rc, double dc, int n, double *xi, double *eta,
    double *r, double *d);
static void traceThresh (double a[], double b[], double residsort[],
    double rmed, double threshresid);
#endif

#ifdef TIME_TRACE
#include <sys/time.h>
#include <unistd.h>
static void traceTime (char string[]);
#endif


/* find best C* in fip such that s's best-match the g's, which are centered
 *   on the given nominal position with given field of view.
 * if find a match with a maximum residual <= *rp, update fip and return 0,
 *   else leave fip unchanged and return -1.
 * if higher order fit requested (nparam = 12, 20 or 26) and not found, set
 *   *rhp to -1; ignore rhp if higher order fit not requested.
 * so return value of findRegistration itself being -1 tells you WCS failed,
 *   *rhp being -1 tells you higher order fit failed.
 */
int
findRegistrationD (fip, ra0, dec0, rot0, psx0, psy0, sx, sy, ns, gr, gd, ng,
                   nparam, MINPAIR, TRYSTARS, MAXROT, MATCHDIST, rhp, rp)
FImage *fip;		/* image header to modify IFF we find a match */
double ra0, dec0;	/* initial center position */
double rot0;		/* initial rotation */
double psx0, psy0;	/* initial pixel scales, rads/pixel right and down */
double sx[], sy[];	/* test stars, image locations, pixels */
int ns;			/* number of entries in sx[] and sy[] */
double gr[], gd[];	/* reference stars, ra/dec, rads */
int ng;			/* number of entries in gr[] and gd[] */
int nparam;       /* no. of params in astrometric fit */
int MINPAIR;      /* min pairs for WCS fit */
int TRYSTARS;     /* try fit if find this no. of pairs, don't look for more */
double MAXROT;    /* max rotation in degrees, for WCS */
double MATCHDIST; /* limit (arcsec) within which distances are considered
                     to be potentially the same in catalogue & image */
double *rhp;      /* IN: max acceptable higher order fit residual (arcsec)
                     OUT: actual rms residual (arcsec) */
double *rp;       /* IN: max acceptable WCS residual (pixels)
                     OUT: actual mean residual (pixels) */
{
	int npair1, minpair, ienlarge, hiordok=0, thistimeok, thisbetter;
	double rh1 = 0;
	int npmax;              /* max. number of star pairs */
	int *mats, *matg;       /* malloced indices of matched pairs */
	double *smx, *smy;      /* malloced list of matched s stars */
	double *gmr, *gmd;      /* malloced list of matched g stars */
	double *gmx, *gmy;      /* malloced nominal x/y of matched g stars */
	double *resid;          /* malloced residual for each pair */
	double *residsort;      /* malloced sorted residuals */
	double a[14], b[14];    /* params for higher order astrometric fit */
	double *sxi, *seta;     /* malloced xi/eta of s stars */
	double *gx, *gy;	/* malloced nominal x/y of gr/gd */
	double t_ra, t_dc;	/* trial values of image center */
	double t_th, t_sx, t_sy;/* trial values of rotation and pixel scales */
	int npair;		/* final number of star pairs */
	double rmed = 0;	/* median residual */
	int ok = 1;		/* success */
	int i, j;
	int n;

	if (nparam != 5 && nparam != 12 && nparam != 20 && nparam != 26)
	  return (-1);

	/* init trial values */
	t_ra = ra0;
	t_dc = dec0;
	t_th = rot0;
	t_sx = psx0;
	t_sy = psy0;

#ifdef TIME_TRACE
	traceTime ("Reset clock");
#endif

	/* find initial projection of reference stars */
	gx = (double *) malloc (ng * sizeof(double));
	gy = (double *) malloc (ng * sizeof(double));
	init_fim (fip);
	setFITSWCS (&fim_g, t_ra, t_dc, t_th, t_sx, t_sy);
	for (i = 0; i < ng; i++)
	    RADec2xy (&fim_g, gr[i], gd[i], &gx[i], &gy[i]);

#ifdef IN_TRACE
	printf ("IN:\n");
	printf ("ra=%6.3f dc=%8.3f th=%8.3fsx=%7.4f sy=%7.4f\n", radhr(ra0),
			raddeg(dec0), raddeg(rot0), 3600*raddeg(psx0),
			3600*raddeg(psy0));
	for (i = j = 0; i < ns && j < ng; i++, j++)
	    printf ("%2d: s[%4.0f,%4.0f] g[%4.0f,%4.0f]\n",
						i, sx[i], sy[i], gx[j], gy[j]);
	for (; i < ns; i++)
	    printf ("%2d: s[%4.0f,%4.0f]\n", i, sx[i], sy[i]);
	for (; j < ng; j++)
	    printf ("%2d:              g[%4.0f,%4.0f]\n", j, gx[j], gy[j]);
#endif

#ifdef TIME_TRACE
	traceTime ("Converted catalogue RA,dec to nominal x,y");
#endif

	npmax = ng < ns ? ng : ns;
	mats = (int *) malloc (npmax * sizeof(int));
	matg = (int *) malloc (npmax * sizeof(int));
	smx = (double *) malloc (npmax * sizeof(double));
	smy = (double *) malloc (npmax * sizeof(double));
	gmr = (double *) malloc (npmax * sizeof(double));
	gmd = (double *) malloc (npmax * sizeof(double));
	gmx = (double *) malloc (npmax * sizeof(double));
	gmy = (double *) malloc (npmax * sizeof(double));
	resid = (double *) malloc (npmax * sizeof(double));
	residsort = (double *) malloc (npmax * sizeof(double));

	/* if findRegistrationD is being called to request 5-parameter WCS
	 * fit, try distance match
	 * else assume input WCS fit is already essentially correct and so
	 * it should be possible to match using absolute positions
	 */
	if (nparam == 5)
	  matchByDist (sx, sy, ns, gr, gd, gx, gy, ng,
		       raddeg(t_sx)*3600, raddeg(t_sy)*3600, MATCHDIST,
		       TRYSTARS, npmax,  mats, matg, &npair);
	else
	  matchByPos (sx, sy, ns, gr, gd, gx, gy, ng,
		      raddeg(t_sx)*3600, raddeg(t_sy)*3600, MATCHDIST,
		      npmax,  mats, matg, &npair);
	npair1 = npair;

	/* put surviving coords into the candidate pair arrays */
#ifdef TOPVOTES_TRACE
	printf ("TOPVOTES_TRACE: %d pairs\n", npair);
	printf ("       s   g        %s      pixels\n",
		"matched pairs, before least squares fit");
#endif
	resid_sum2 = 0;
	n = npair;
	npair = 0;
	for (i = 0; i < n; i++) {
	  int sidx = mats[i];
	  int gidx = matg[i];

#ifdef TOPVOTES_TRACE
	    {
	    double r2;
	    char ras[32], decs[32];
	    fs_sexa (ras, radhr(gr[gidx]), 2, 36000);
	    fs_sexa (decs, raddeg(gd[gidx]), 3, 3600);
	    resid_sum2+=r2=pow(sx[sidx]-gx[gidx],2)+pow(sy[sidx]-gy[gidx],2);
	    printf (
	     "%3d: (%3d,%3d) s:[%4.0f,%4.0f] g:[%s,%s][%4.0f,%4.0f] %6.2f\n",
			    i, sidx, gidx, sx[sidx], sy[sidx],
			    ras, decs, gx[gidx], gy[gidx],
			    sqrt(r2));
	    }
#endif

	    smx[i] = sx[sidx];
	    smy[i] = sy[sidx];
	    gmr[i] = gr[gidx];
	    gmd[i] = gd[gidx];
	    npair++;
	}
#ifdef TOPVOTES_TRACE
	printf ("Rms%8.3f pixels\n", sqrt(resid_sum2/npair));
#endif

#ifdef TIME_TRACE
	if (nparam == 5) traceTime ("Searched for distance match");
	else traceTime ("Searched for position match");
#endif

	if (npair < MINPAIR) {
	    ok = 0;
	    goto out;
	}

	/* provide global access to the star pair lists for chisqr() model */
	/* smx,smy,gmr,gmd,gmx,gmy,resid are pointers and psx0,psy0 don't get
	 * updated anywhere in this function; so only need assign this global
	 * access once; npair_g, however, needs to be reset from npair each
	 * time least squares is run
	 */
	gx_g = gmx;
	gy_g = gmy;
	sx_g = smx;
	sy_g = smy;
	gr_g = gmr;
	gd_g = gmd;
	resid_g = resid;
	psx0_g = psx0;
	psy0_g = psy0;

	/* find best fit transformation of gmr/d[] to smx/y[].
	 * may retry a few times to discard outlyers.
	 */
	for (j = 0; j < NOUTLL; j++) {
	    double threshresid;	/* threshold residual for next trial solution */

	    /* find best fit */
	    npair_g = npair;
	    if (call_lstsqr (&t_ra, &t_dc, &t_th, &t_sx, &t_sy) < 0) {
		ok = 0;
		goto out;
	    }

#ifdef FIT_TRACE
	    printf("FIT_TRACE: ");
	    printf(" npair=%i, best fit is:\n", npair);
	    printf("ra=%7.4f dc=%8.4f rot=%9.4f sx=%8.4f sy=%8.4f -> rmax=%9.4f\n",
		    radhr(t_ra), raddeg(t_dc), raddeg(t_th), 3600*raddeg(t_sx),
		    3600*raddeg(t_sy), resid_max);
#endif
	    
	    for (i = 0; i < npair; i++) residsort[i]=resid_g[i];
	    dmedian (residsort, npair, &rmed);
	    threshresid = THRESH*rmed;
	    if (threshresid < *rp)
		threshresid = *rp;

#ifdef THRESH_TRACE
	    printf ("THRESH_TRACE:\n");
  printf ("npair=%3d rmed=%8.3f thresh=%6.3f\n", npair, rmed, threshresid);
  printf ("        pixel    Image %s Catalogue     RA   dec pixels sorted\n",
          "                   ");
	    for (i = 0; i < npair; i++) {
		char ras[32], decs[32], sras[32], sdecs[32];
		double smr, smd, sxi, seta, gxi, geta;
		double r2as = raddeg(1)*3600;
		/* sloppy programming to use file global variable fim_g
		 * but easier than bothering to initialise another FImage
		 * structure; function chisqr always sets fim_g as required
		 * so this won't interfere
		 */
		setFITSWCS (&fim_g, t_ra, t_dc, t_th, t_sx, t_sy);
		xy2RADec (&fim_g, smx[i], smy[i], &smr, &smd);
		RADec2xieta (t_ra, t_dc, 1, &smr, &smd, &sxi, &seta);
		RADec2xieta (t_ra, t_dc, 1, &gmr[i], &gmd[i], &gxi, &geta);
		fs_sexa (sras, radhr(smr), 2, 360000);
		fs_sexa (sdecs, raddeg(smd), 3, 36000);
		fs_sexa (ras, radhr(gmr[i]), 2, 360000);
		fs_sexa (decs, raddeg(gmd[i]), 3, 36000);
		printf
	      ("%3d: [%4.0f,%4.0f][%s,%s] [%s,%s] %5.2f %5.2f %5.2f %5.2f%c\n",
		   i, smx[i], smy[i], sras, sdecs, ras+6, decs+7,
		   (sxi-gxi)*r2as, (seta-geta)*r2as, 
		   resid_g[i], residsort[i],
		   residsort[i] < threshresid ? ' ' : '*');
	    }
	    printf ("Max%8.3f   Mean%8.3f   Rms%8.3f pixels\n",
		    resid_max, resid_sum/npair_g, sqrt(resid_sum2/npair_g));
#endif

	    /* no point in culling large resids if last time through loop */
	    if (j == NOUTLL - 1) goto out;

	    /* discard those pairs with large residuals */
	    n = npair;
	    npair = 0;
	    for (i = 0; i < n; i++) {
		if (resid_g[i] < threshresid) {
		    if (i != npair) { /* just to avoid a[n] = a[n] */
			smx[npair] = smx[i];
			smy[npair] = smy[i];
			gmr[npair] = gmr[i];
			gmd[npair] = gmd[i];
		    }
		    npair++;
		}
	    }

	    /* stop if no more culled (good) or now too few left (bad) */
	    if (npair == n)
		goto out;
	    if (npair < MINPAIR) {
		ok = 0;
		goto out;
	    }
	}

    out:

#ifdef TIME_TRACE
	traceTime ("Searched for WCS least squares fit");
#endif

#ifdef PRMAXRES_TRACE
	printf ("PRMAXRES_TRACE: %8.2f with %2d pairs @ %6.2f %6.2f\n",
				resid_max, npair, radhr(ra0), raddeg(dec0));
#endif

	/* got a solution if ok so far, within goal and reasonable rotation
	 * and scaling
	 */
	if (ok && resid_max < *rp  && acos(cos(t_th)) < degrad(MAXROT)
				   && fabs(t_sx) < 2*fabs(psx0)
				   && fabs(t_sx) > fabs(psx0)/2
				   && fabs(t_sy) < 2*fabs(psy0)
				   && fabs(t_sy) > fabs(psy0)/2) {
#ifdef FITS_SOLUTION_TRACE
	    printf ("FITS_SOLUTION:\n");
	    printf ("CDELT1  = %20.10g\n", raddeg(t_sx));
	    printf ("CDELT2  = %20.10g\n", raddeg(t_sy));
	    printf ("RA      = %20.10g\n", raddeg(t_ra));
	    printf ("DEC     = %20.10g\n", raddeg(t_dc));
	    printf ("CROTA2  = %20.10g\n", raddeg(t_th));
#endif

	    /* yes! */
	    setFITSWCS (fip, t_ra, t_dc, t_th, t_sx, t_sy);
	    *rp = resid_sum/npair_g;
	} else
	    ok = 0;

#ifdef GNUPLOT_TRACE
	if (ok) plotGNU (fip, t_sx, t_sy, sx, sy, ns, gr, gd, gx, gy, ng,
	  smx, smy, gmr, gmd, gmx, gmy, npair, npmax, a, b, nparam, hiordok);
#endif

	/* So far, function has operated similarly to old version except for
	 * different matching algorithm - have looked for 5-parameter WCS
	 * fit, not higher order fit, whatever input value of nparam
	 * (Either ok = 1 having found WCS solution, updated *fip and *rp,
	 * and written gnuplot files; or ok = 0)
	 *
	 * If nparam = 5, not seeking higher order fit; so tidy up and
	 * finish
	 * But if nparam > 5, search for higher order fit
	 *
	 * N.B. if you want gnuplot files for WCS fit, e.g. for testing
	 * purposes, you could set REJECTDIST to 0.0 in ip.cfg to force the
	 * higher order fit to fail, then higher order gnuplot files won't
	 * overwrite WCS gnuplot files
	 */

	if (nparam == 5) goto tidy;

	minpair = nparam/2+1;

	/* At this stage we have WCS fit given by t_ra, t_dc, t_th, t_sx,
	 * t_sy, which are probably values just determined, although if a
	 * WCS solution wasn't found this time, then they'll be the values
	 * determined the previous time this function was called.
	 * (We assume this function is only called with nparam > 5 if a WCS
	 * solution has been found.)
	 *
	 * Next, want to find higher order astrometric fit.
	 * Will do this by comparing conventional xi,eta "standard
	 * coordinates" in image & catalogue.
	 *
	 * Use notation of Digitized Sky Survey booklet a1,a2,...,b1,b2,...
	 * for astrometric parameters, which we store in a[i], b[i],
	 * i=1,2,...   Store RA, dec of projection centre in a[0], b[0].
	 */

#ifdef TIME_TRACE
	traceTime ("Reset clock");
#endif

	{
	  /* Recalculate gx,gy as xi,eta (previously nominal x/y image
	   * locations)
	   */
	  double xc, yc, rc, dc, ct, st;
	  /* If approx. telescope projection centre is available in FITS
	   * header (expressed as x,y pixel - but we can use WCS solution
	   * to convert x,y to absolute RA,dec of projection centre), use
	   * it; otherwise use WCS field centre.
	   */
	  if (getRealFITS (fip, "PROJCEN1", &xc) == 0 &&
	      getRealFITS (fip, "PROJCEN2", &yc) == 0)
	    xy2RADec (fip, xc, yc, &rc, &dc);
	  else {rc=t_ra; dc=t_dc; RADec2xy (fip, rc, dc, &xc, &yc);}
	  a[0]=rc; b[0]=dc;
	  /* shift to xc,yc & rescaling by t_sx,t_sy gives
	   * xi  = a1*x + a2*y + a3
	   * eta = b1*y + b2*x + b3
	   * where a1=t_sx, a2=0, a3=-t_sx*xc, b1=t_sy, b2=0, b3=-t_sy*yc;
	   * rotation by theta in addition gives following
	   */
	  ct=cos(t_th); st=sin(t_th);
	  a[1] = ct*t_sx; a[2] = -st*t_sy; a[3] = -ct*t_sx*xc+st*t_sy*yc;
	  b[1] = ct*t_sy; b[2] =  st*t_sx; b[3] = -ct*t_sy*yc-st*t_sx*xc;
	  /* will use these a1,a2,a3,b1,b2,b3 as initial guess (input to
	   * least squares routine which will evaluate them more accurately
	   * as well as determining further parameters a4 etc.)
	   */
	  RADec2xieta (rc, dc, ng, gr, gd,  gx, gy);
	}

#ifdef TIME_TRACE
	traceTime ("Converted catalogue RA,dec to xi,eta");
#endif

	sxi = (double *) malloc (ns * sizeof(double));
	seta = (double *) malloc (ns * sizeof(double));

	/* From here will try to work in arcsec rather than pixels (for
	 * resids etc.), seems reasonable since working in tangent plane
	 * rather than nominal image x/y
	 */

	for (ienlarge = 0; ienlarge < NENLARGE; ienlarge++) {

	  /* 1st time around, use position match found above, to determine
	   * preliminary higher order astrometric fit
	   * values in mats[], matg[] are still available
	   */
	  if (ienlarge == 0) {npair = npair1; rh1 = *rhp;}

	  else {
	    double r2as = raddeg(1)*3600;
	    for (i = 0; i < ns; i++)
	      xy2xieta (a, b, sx[i], sy[i], &sxi[i], &seta[i]);
	    matchByPos (sxi, seta, ns, gr, gd, gx, gy, ng,
	                r2as, r2as, MATCHDIST, npmax, mats, matg, &npair);
#ifdef TOPVOTES_TRACE
	    printf ("TOPVOTES_TRACE: %d pairs\n", npair);
	    printf ("       s   g  %s  arcsec\n",
	            "matched pairs, before least squares fit");
	    resid_sum2 = 0;
	    for (i = 0; i < npair; i++) {
	      int sidx = mats[i];
	      int gidx = matg[i];
	      {
		double r2;
		char ras[32], decs[32];
		fs_sexa (ras, radhr(gr[gidx]), 2, 36000);
		fs_sexa (decs, raddeg(gd[gidx]), 3, 3600);
		resid_sum2 += r2 = pow((sxi[sidx]-gx[gidx])*r2as,2) +
		                   pow((seta[sidx]-gy[gidx])*r2as,2);
		printf (
		 "%3d: (%3d,%3d) s:[%4.0f,%4.0f] g:[%s,%s] %6.2f\n",
		 i, sidx, gidx, sx[sidx], sy[sidx], ras, decs, sqrt(r2));
	      }
	    }
	    printf ("Rms%8.3f arcsec\n", sqrt(resid_sum2/npair));
#endif
#ifdef TIME_TRACE
	  traceTime ("Searched for position match");
#endif
	  }

	  thistimeok = 1;
	  if (npair < minpair) {thistimeok = 0; goto out2;}

	  /* set smx,smy,gmx,gmy (gmr,gmd used for tracing) */
	  for (i = 0; i < npair; i++) {
	    int sidx = mats[i];
	    int gidx = matg[i];
	    smx[i] = sx[sidx];
	    smy[i] = sy[sidx];
	    gmr[i] = gr[gidx];
	    gmd[i] = gd[gidx];
	    gmx[i] = gx[gidx];
	    gmy[i] = gy[gidx];
	  }

	  /* find best fit transformation of gmx/y[] to smx/y[] */
	  for (j = 0; j < NOUTLL; j++) {
	    double threshresid;
	    int converged;
	    npair_g = npair;
	    /* note that it doesn't matter if least squares routine fails,
	     * PROVIDED the residual (which we check below) is small enough
	     * (i.e. even if we haven't found the BEST solution with the
	     * given set of stars, we've found a solution with reasonable
	     * resids - after all, we aren't exhaustively checking all
	     * possible subsets of stars just in case we might find a
	     * "better" solution)
	     * therefore the line that was in a previous version of the
	     * program, which makes the astrometric fit fail if converged<0,
	     * is deleted
	     * earlier in the program, we still have WCS failure if
	     * call_lstsqr<0, but in practice convergence of least squares
	     * routine only seems an occasional problem for the higher order
	     * fits, not WCS
	     */
	    converged = (nparam == 12) ? call_lstsqr2(a,b) :
	      ((nparam == 20) ? call_lstsqr3(a,b) : call_lstsqrDSS (a,b));
#ifdef FIT_TRACE
	    printf("FIT_TRACE: ");
	    printf(" npair=%i, best fit gives", npair);
	    printf(" rmax=%9.4f arcsec\n", resid_max);
	    if (converged < 0)
	      printf("            but least squares has not converged \n");
#endif
	    for (i = 0; i < npair; i++) residsort[i]=resid_g[i];
	    dmedian (residsort, npair, &rmed);
	    threshresid = resid_max > *rhp*THRESH ? resid_max/THRESH : *rhp;
#ifdef THRESH_TRACE
	    traceThresh (a, b, residsort, rmed, threshresid);
#endif
	    if (j == NOUTLL - 1) goto out2;
	    n = npair;
	    npair = 0;
	    for (i = 0; i < n; i++) {
                if (resid_g[i] < threshresid) {
		    if (i != npair) { /* just to avoid a[n] = a[n] */
			smx[npair] = smx[i];
			smy[npair] = smy[i];
			gmr[npair] = gmr[i];
			gmd[npair] = gmd[i];
			gmx[npair] = gmx[i];
			gmy[npair] = gmy[i];
		    }
		    npair++;
		}
	    }
	    if (npair == n) goto out2;
	    if (npair < minpair) {thistimeok = 0; goto out2;}
	  } /**************** end of NOUTLL loop ****************/

    out2:

#ifdef TIME_TRACE
	  if (nparam == 12)
	    traceTime ("Searched for quadratic least squares fit");
	  else if (nparam == 20)
	    traceTime ("Searched for cubic least squares fit");
	  else
	    traceTime ("Searched for DSS model least squares fit");
#endif
#ifdef PRMAXRES_TRACE
	  printf
	    ("PRMAXRES_TRACE: %8.2f\" with %2d pairs in %2d parameter fit\n",
	     resid_max, npair, nparam);
#endif

	  if (resid_max > *rhp) thistimeok = 0;

	  /* can't find solution first time - immediate fail */
	  if (ienlarge == 0 && thistimeok == 0) break;

	  hiordok = 1;

	  /* first time, no previous solution to compare */
	  if (ienlarge == 0) thisbetter = 1;
	  /* can't find solution this time - previous was better */
	  else if (thistimeok == 0) thisbetter = 0;
	  /* ended up with fewer pairs than before - previous better */
	  else if (npair < npair1) thisbetter = 0;
	  else if (npair > npair1) thisbetter = 1;
	  else if (sqrt(resid_sum2/npair_g) < rh1) thisbetter = 1;
	  else thisbetter = 0;

	  if (thisbetter) {
	    setFITSastrom (fip, a, b);
	    npair1 = npair;
	    rh1 = sqrt(resid_sum2/npair_g);
#ifdef GNUPLOT_TRACE
	    plotGNU (fip, t_sx, t_sy, sx, sy, ns, gr, gd, gx, gy, ng,
	    smx, smy, gmr, gmd, gmx, gmy, npair, npmax, a, b, nparam, hiordok);
#endif
	  }
	  else {
#ifdef FITS_SOLUTION_TRACE
	    getAMDFITS (fip, a, b);
#endif
	    npair = npair1;
	    break;
	  }
	} /**************** end of NENLARGE loop ****************/

	free ((void *)sxi);
	free ((void *)seta);

	if (hiordok) {
#ifdef FITS_SOLUTION_TRACE
	  printf ("FITS_SOLUTION:\n");
	  for (i = 0; i < 14; i++) {
	    printf ("AMDX%i = %20.10G\n", i,a[i]);
	    if (i == 0) printf ("      = %16.10G deg\n", raddeg(a[i]));
	    printf ("AMDY%i = %20.10G\n", i,b[i]);
	    if (i == 0) printf ("      = %16.10G deg\n", raddeg(b[i]));
	  }
#endif
	  *rhp = rh1;
	}
	else
	  *rhp = -1;

    tidy:

	/* finished with the voting and other temp arrays */
	free ((void *)mats);
	free ((void *)matg);
	free ((void *)smx);
	free ((void *)smy);
	free ((void *)gmr);
	free ((void *)gmd);
	free ((void *)gmx);
	free ((void *)gmy);
	free ((void *)resid);
	free ((void *)residsort);
	free ((void *)gx);
	free ((void *)gy);
	resetFImage (&fim_g);

	return (ok ? 0 : -1);
}

/* init fim_g from fip */
static void
init_fim (FImage *fip)
{
	initFImage (&fim_g);

	setLogicalFITS (&fim_g, "SIMPLE", 1, NULL);
	setIntFITS (&fim_g, "BITPIX", 16, NULL);
	setIntFITS (&fim_g, "NAXIS", 2, NULL);
	setIntFITS (&fim_g, "NAXIS1", fip->sw, NULL);
	fim_g.sw = fip->sw;
	setIntFITS (&fim_g, "NAXIS2", fip->sh, NULL);
	fim_g.sh = fip->sh;
}


/* compute the chisqr of the vector v with respect to fim_g.
 * update resid/_max/_sum/_sum2/_g[i].  (*** UNITS OF PIXELS ***)
 *
 * chisqr is version for 5 parameter WCS fit.
 * chisqr needs to call RADec2xy (cf. chisqr2).
 */
static double
chisqr(double v[5])
{
	FImage *fip = &fim_g;
	double *mx, *my;
	double ra = v[0];
	double dc = v[1];
	double th = v[2];
	double sx = v[3];
	double sy = v[4];
	double c2;
	int i;

	mx = (double *) malloc (npair_g * sizeof(double));
	my = (double *) malloc (npair_g * sizeof(double));

	/* init residual stats */
	resid_max = 0;
	resid_sum = 0;
	resid_sum2 = 0;

	/* install trial values */
	setFITSWCS (fip, ra, dc, th, sx, sy);

	/* find errors compared with star list */
	c2 = 0.0;
	for (i = 0; i < npair_g; i++) {
	    double ex, ey;
	    double r, r2;

	    RADec2xy (fip, gr_g[i], gd_g[i], &mx[i], &my[i]);

	    /* credit for small distance */
	    ex = mx[i] - sx_g[i];
	    ey = my[i] - sy_g[i];
	    r2 = ex*ex + ey*ey;

	    /* credit for similar angle */
	    if (i > 0) {
		double a1 = atan2 (sy_g[i-1]-sy_g[i], sx_g[i-1]-sx_g[i]);
		double a2 = atan2 (my[i-1]-my[i], mx[i-1]-mx[i]);
		r2 *= 1+fabs(a1-a2);
	    }

	    c2 += r2;
	    resid_sum2 += r2;
	    r = sqrt(r2);
	    if (r > resid_max)
		resid_max = r;
	    resid_sum += r;
	    resid_g[i] = r;
#ifdef RESID_TRACE
	    printf ("RESID: %2d: %8.4f\n", i, r);
#endif
	}

	free ((void *)mx);
	free ((void *)my);

#ifdef CHSQR_TRACE
	printf("ra=%7.4f dc=%8.4f rot=%9.4f sx=%8.4f sy=%8.4f -> rmax=%9.4f\n",
			    radhr(ra), raddeg(dc), raddeg(th), 3600*raddeg(sx),
			    3600*raddeg(sy), resid_max);
#endif
	return (c2);
}


/* compute the chisqr of the vector v.
 * update resid/_max/_sum/_sum2/_g[i].  (*** UNITS OF ARCSEC ***)
 *
 * chisqr2 is version for 12 parameter quadratic fit.
 * chisqr2 much faster than function chisqr because chisqr2 doesn't need to
 * call RADec2xy; chisqr2 instead uses file global gx_g, gy_g.
 * Other file global references:  sx_g, sy_g, npair_g.
 */
static double
chisqr2(double v[12])
{
	double c2;
	int i;

	/* init residual stats */
	resid_max = 0;
	resid_sum = 0;
	resid_sum2 = 0;

	/* find errors compared with star list */
	c2 = 0.0;
	for (i = 0; i < npair_g; i++) {
	    double r2as = raddeg(1)*3600;
	    double ex, ey;
	    double x, y, xi, eta;
	    double r, r2;

	    /* xi,eta in radians, x,y in pixels, ex,ey in arcsec */
	    x = sx_g[i];
	    y = sy_g[i];
            xi = v[0]*x + v[1]*y + v[2] + v[3]*x*x + v[4]*x*y + v[5]*y*y;
            eta = v[6]*y + v[7]*x + v[8] + v[9]*y*y + v[10]*x*y + v[11]*x*x;
	    ex = (gx_g[i] - xi)  * r2as;
	    ey = (gy_g[i] - eta) * r2as;
	    r2 = ex*ex + ey*ey;

	    c2 += r2;
	    resid_sum2 += r2;
	    r = sqrt(r2);
	    if (r > resid_max)
		resid_max = r;
	    resid_sum += r;
	    resid_g[i] = r;
#ifdef RESID_TRACE
	    printf ("RESID: %2d: %8.4f\n", i, r);
#endif
	}

#ifdef CHSQR_TRACE
	for (i = 0; i < 12; i++) {
	  if (i == 6) printf ("\n");
	  printf ("%10.2E", v[i]);
	}
	printf (" -> rmax=%8.2f\n chi squared =%14.2f\n", resid_max, c2);
#endif
	return (c2);
}


/* compute the chisqr of the vector v.  (*** UNITS OF ARCSEC ***)
 *
 * chisqr3 is version for 20 parameter cubic fit.
 * works in same way as chisqr2.
 */
static double
chisqr3(double v[20])
{
	double c2;
	int i;

	/* init residual stats */
	resid_max = 0;
	resid_sum = 0;
	resid_sum2 = 0;

	/* find errors compared with star list */
	c2 = 0.0;
	for (i = 0; i < npair_g; i++) {
	    double r2as = raddeg(1)*3600;
	    double ex, ey;
	    double x, y, xi, eta;
	    double r, r2;

	    x = sx_g[i];
	    y = sy_g[i];
	    xi = v[0]*x + v[1]*y + v[2] + v[3]*x*x + v[4]*x*y + v[5]*y*y +
	         v[6]*x*x*x + v[7]*x*x*y + v[8]*x*y*y + v[9]*y*y*y;
	    eta = v[10]*y + v[11]*x + v[12] + v[13]*y*y + v[14]*x*y + v[15]*x*x
	        + v[16]*y*y*y + v[17]*x*y*y + v[18]*x*x*y + v[19]*x*x*x;
	    ex = (gx_g[i] - xi)  * r2as;
	    ey = (gy_g[i] - eta) * r2as;
	    r2 = ex*ex + ey*ey;

	    c2 += r2;
	    resid_sum2 += r2;
	    r = sqrt(r2);
	    if (r > resid_max)
		resid_max = r;
	    resid_sum += r;
	    resid_g[i] = r;
#ifdef RESID_TRACE
	    printf ("RESID: %2d: %8.4f\n", i, r);
#endif
	}

#ifdef CHSQR_TRACE
	for (i = 0; i < 20; i++) {
	  if (i == 6 || i == 16) printf ("\n                    ");
	  if (i == 10) printf ("\n");
	  printf ("%10.2E", v[i]);
	}
	printf (" -> rmax=%8.2f\n chi squared =%14.2f\n", resid_max, c2);
#endif
	return (c2);
}


/* compute the chisqr of the vector v.  (*** UNITS OF ARCSEC ***)
 *
 * chisqrDSS is version for Digitized Sky Survey 26 parameter fit.
 * works in same way as chisqr2.
 */
static double
chisqrDSS(double v[26])
{
	double c2;
	int i;

	/* init residual stats */
	resid_max = 0;
	resid_sum = 0;
	resid_sum2 = 0;

	/* find errors compared with star list */
	c2 = 0.0;
	for (i = 0; i < npair_g; i++) {
	    double r2as = raddeg(1)*3600;
	    double ex, ey;
	    double x, y, x2y2, xi, eta;
	    double r, r2;

	    x = sx_g[i];
	    y = sy_g[i];
	    x2y2 = x*x + y*y;
	    xi = v[0]*x + v[1]*y + v[2] + v[3]*x*x + v[4]*x*y + v[5]*y*y +
	         v[7]*x*x*x + v[8]*x*x*y + v[9]*x*y*y + v[10]*y*y*y +
	         v[6]*x2y2 + v[11]*x*x2y2 + v[12]*x*x2y2*x2y2;
	    eta = v[13]*y + v[14]*x + v[15] + v[16]*y*y + v[17]*x*y + v[18]*x*x
	        + v[20]*y*y*y + v[21]*x*y*y + v[22]*x*x*y + v[23]*x*x*x +
	          v[19]*x2y2 + v[24]*y*x2y2 + v[25]*y*x2y2*x2y2;
	    ex = (gx_g[i] - xi)  * r2as;
	    ey = (gy_g[i] - eta) * r2as;
	    r2 = ex*ex + ey*ey;

	    c2 += r2;
	    resid_sum2 += r2;
	    r = sqrt(r2);
	    if (r > resid_max)
		resid_max = r;
	    resid_sum += r;
	    resid_g[i] = r;
#ifdef RESID_TRACE
	    printf ("RESID: %2d: %8.4f\n", i, r);
#endif
	}

#ifdef CHSQR_TRACE
	for (i = 0; i < 26; i++) {
	  if (i == 7 || i == 13 || i == 20) printf ("\n");
	  printf ("%10.2E", v[i]);
	}
	printf (" -> rmax=%8.2f\n chi squared =%14.2f\n", resid_max, c2);
#endif
	return (c2);
}


/* set up the necessary temp and global arrays and call the multivariat solver.
 * return 0 if ok, else -1.
 */
static int
call_lstsqr (double *t_ra, double *t_dc, double *t_th, double *t_sx,
double *t_sy)
{
	double p0[5], p1[5];

	p0[0] = *t_ra;
	p0[1] = *t_dc;
	p0[2] = *t_th;
	p0[3] = *t_sx;
	p0[4] = *t_sy;
	p1[0] = *t_ra + degrad(.25);
	p1[1] = *t_dc + degrad(.25);
	p1[2] = *t_th + degrad(30);
	p1[3] = *t_sx * 1.1;
	p1[4] = *t_sy * 1.1;

#ifdef CHSQR_TRACE
	    printf ("CHSQR_TRACE:\n");
#endif
	if (lstsqr (chisqr, p0, p1, 5, FTOL) < 0)
	    return (-1);

	*t_ra = p0[0];
	*t_dc = p0[1];
	*t_th = p0[2];
	*t_sx = p0[3];
	*t_sy = p0[4];

	return (0);
}


/* set up the necessary temp & global arrays and call the multivariate solver.
 * return 0 if ok, else -1.
 * Version for 12 parameter quadratic fit.
 * a[1-3],b[1-3]  IN: initial guess
 * initial guess for quadratic coefficients taken as zero
 * a[1-13],b[1-13]  OUT: least squares solution, nonzero terms 1-6
 *
 * offsets for second guess taken as 1E-3 for constant terms a3,b3 since
 * expect residuals from initial guess to be closer than 1E-3 radians;
 * offsets taken as 1E-6, 1E-9 for linear, quadratic terms since x,y of order
 * 1E3
 */
static int
call_lstsqr2 (double a[], double b[])
{
	double p0[12], p1[12];
	int i;

	/* p0[0-5] are a1-a6 in Digitized Sky Survey notation
	 * p0[6-11] are b1-b6
	 */

	for (i = 0; i < 12; i++) {p0[i] = 0; p1[i] = 1E-9;}
	for (i = 0; i < 3; i++) {
	  p0[i] = a[i+1]; p0[i+6] = b[i+1]; 
	  p1[i] = a[i+1]+1E-6; p1[i+6] = b[i+1]+1E-6;
	}
	p1[2] = a[3]+1E-3; p1[8] = b[3]+1E-3;

#ifdef CHSQR_TRACE
	    printf ("CHSQR_TRACE:\n");
#endif
	if (lstsqr (chisqr2, p0, p1, 12, FTOL) < 0)
	    return (-1);

	for (i = 1; i < 14; i++) {a[i] = b[i] = 0;}
	for (i = 1; i < 7; i++) {a[i] = p0[i-1]; b[i] = p0[i+5];}

	return (0);
}


/* set up the necessary temp & global arrays and call the multivariate solver.
 * return 0 if ok, else -1.
 * Version for 20 parameter cubic fit.
 * a[1-3],b[1-3]  IN: initial guess
 * initial guess for quadratic, cubic coefficients taken as zero
 * a[1-13],b[1-13]  OUT: least squares solution, nonzero terms 1-6,8-11
 */
static int
call_lstsqr3 (double a[], double b[])
{
	double p0[20], p1[20];
	int i;

	/* p0[0-9] are a1-a6, a8-a11 in Digitized Sky Survey notation
	 * p0[10-19] are b1-b6, b8-b11
	 */

	for (i = 0; i < 20; i++) {p0[i] = 0; p1[i] = 1E-12;}
	for (i = 3; i < 6; i++) {p1[i] = 1E-9; p1[i+10] = 1E-9;}
	for (i = 0; i < 3; i++) {
	  p0[i] = a[i+1]; p0[i+10] = b[i+1]; 
	  p1[i] = a[i+1]+1E-6; p1[i+10] = b[i+1]+1E-6;
	}
	p1[2] = a[3]+1E-3; p1[12] = b[3]+1E-3;

#ifdef CHSQR_TRACE
	    printf ("CHSQR_TRACE:\n");
#endif
	if (lstsqr (chisqr3, p0, p1, 20, FTOL) < 0)
	    return (-1);

	for (i = 1; i < 14; i++) {a[i] = b[i] = 0;}
	for (i = 1; i < 7; i++)  {a[i] = p0[i-1]; b[i] = p0[i+9];}
	for (i = 8; i < 12; i++) {a[i] = p0[i-2]; b[i] = p0[i+8];}

	return (0);
}


/* set up the necessary temp & global arrays and call the multivariate solver.
 * return 0 if ok, else -1.
 * Version for 26 parameter 5th order fit.
 * It is the 5th order fit defined in the Digitized Sky Survey, not a general
 * 5th order fit.  In fact, only 1 term in xi and 1 term in eta is 5th order.
 *
 * a[1-3],b[1-3]  IN: initial guess
 * initial guess for quadratic, cubic, quintic coefficients taken as zero
 * a[1-13],b[1-13]  OUT: least squares solution
 */
static int
call_lstsqrDSS (double a[], double b[])
{
	double p0[26], p1[26];
	int i;

	/* p0[0-12] are a1-a13 in Digitized Sky Survey notation
	 * p0[13-25] are b1-b13
	 */

	for (i = 0; i < 26; i++) {p0[i] = 0; p1[i] = 1E-12;}
	for (i = 3; i < 7; i++) {p1[i] = 1E-9; p1[i+13] = 1E-9;}
	for (i = 0; i < 3; i++) {
	  p0[i] = a[i+1]; p0[i+13] = b[i+1]; 
	  p1[i] = a[i+1]+1E-6; p1[i+13] = b[i+1]+1E-6;
	}
	p1[2] = a[3]+1E-3; p1[15] = b[3]+1E-3;
	p1[12] = 1E-19; p1[25] = 1E-19;

#ifdef CHSQR_TRACE
	    printf ("CHSQR_TRACE:\n");
#endif
	if (lstsqr (chisqrDSS, p0, p1, 26, FTOL) < 0)
	    return (-1);

	for (i = 1; i < 14; i++) {a[i] = p0[i-1]; b[i] = p0[i+12];}

	return (0);
}


static int
d_cmp (const void *p1, const void *p2)
{
	double dif = (*(double*)p1) - (*(double*)p2);
	if (dif < 0)
	    return (-1);
	if (dif > 0)
	    return (1);
	return (0);
}

static void
dmedian (double a[], int na, double *mp)
{
	qsort ((void *)a, na, sizeof(double), d_cmp);
	*mp = a[na/2];
}

/* set WCS fields to the given values.
 * N.B. this assumes ra/dec refers to the image center.
 */
static void
setFITSWCS (FImage *fip, double ra, double dec, double rot, double pixszw,
double pixszh)
{
	setStringFITS (fip, "CTYPE1", "RA---TAN", "Columns are RA");
	setRealFITS (fip,   "CRVAL1", raddeg(ra), 10, "RA at CRPIX1, degrees");
	setRealFITS (fip,   "CDELT1", raddeg(pixszw), 10,
						"RA step right, degrees/pixel");
	setRealFITS (fip,   "CRPIX1", fip->sw/2.0, 10,
					"RA reference pixel index, 1-based");
	setRealFITS (fip,   "CROTA1", 0.0, 10, NULL);

	setStringFITS (fip, "CTYPE2", "DEC--TAN", "Rows are Dec");
	setRealFITS (fip,   "CRVAL2", raddeg(dec), 10,"Dec at CRPIX2, degrees");
	setRealFITS (fip,   "CDELT2", raddeg(pixszh), 10, 
						"Dec step down, degrees/pixel");
	setRealFITS (fip,   "CRPIX2", fip->sh/2.0, 10,
					"Dec reference pixel index, 1-based");

	setRealFITS (fip,   "CROTA2", raddeg(rot), 10,
					    "Rotation N through E, degrees");
}


/* set FITS header astrometric fields to the given values.
 */
static void
setFITSastrom (FImage *fip, double a[], double b[])
{
  int i;
  char name[7], comment[25];
  setRealFITS
    (fip, "AMDX0", a[0], 10, "astrometric parameter alpha_c (radians)");
  setRealFITS
    (fip, "AMDY0", b[0], 10, "astrometric parameter delta_c (radians)");
  for (i = 1; i < 14; i++) {
    sprintf (name, "AMDX%i", i);
    sprintf (comment, "astrometric parameter a%i", i);
    setRealFITS (fip, name, a[i], 10, comment);
    sprintf (name, "AMDY%i", i);
    sprintf (comment, "astrometric parameter b%i", i);
    setRealFITS (fip, name, b[i], 10, comment);
  }
}


/* Star with index DistInfo.i has distance DistInfo.d from base star */
typedef struct {
  int i;
  double d;
} DistInfo;


/* Sort by increasing distance; cf. starstatSortF in setwcsfits.c */
static int
compareDist (const void* ptr1, const void* ptr2)
{
  double d = ((DistInfo*)ptr1)->d - ((DistInfo*)ptr2)->d;
  if (d < 0)
    return (-1);
  if (d > 0)
    return (1);
  return (0);
}


#ifdef GNUPLOT_TRACE
/* write files that gnuplot can use
 * call this whenever an acceptable solution is found in case later fit
 * fit finding attempts fail, overwriting smx, smy, gmr, gmd
 */
static void plotGNU (FImage *fip, double t_sx, double t_sy,
    double sx[], double sy[], int ns,
    double gr[], double gd[], double gx[], double gy[], int ng,
    double smx[], double smy[], double gmr[], double gmd[],
    double gmx[], double gmy[], int npair, int npmax,
    double a[], double b[], int nparam, int hiordok)
{
	    /* plot all stars and mark those used for the final fit.
	     * N.B. gnuplot wants +y upward, FITS uses +y downward
	     */

	    double *xem, *yem;
	    double mxerr, myerr;
	    FILE *fp;
	    int i;

	    /* find final catalog star positions */
	    if (nparam == 5) for (i = 0; i < ng; i++)
		RADec2xy (fip, gr[i], gd[i], &gx[i], &gy[i]);

	    /* image stars */
	    fp = fopen ("/tmp/wcs.s", "w");
	    if (!fp) fp = stdout;
	    for (i = 0; i < ns; i++)
		fprintf (fp, "%7.1f %7.1f\n", sx[i], sy[i]);
	    if (fp != stdout) fclose (fp);

	    /* catalog stars */
	    fp = fopen ("/tmp/wcs.c", "w");
	    if (!fp) fp = stdout;
	    for (i = 0; i < ng; i++) {
	      if (nparam == 5) fprintf (fp, "%7.1f %7.1f\n", gx[i], gy[i]);
	      else {
		double gxtmp, gytmp;
		RADec2xy (fip, gr[i], gd[i], &gxtmp, &gytmp);
		fprintf (fp, "%7.1f %7.1f\n", gxtmp, gytmp);
	      }
	    }
	    if (fp != stdout) fclose (fp);

	    /* fit stars */
	    fp = fopen ("/tmp/wcs.fit", "w");
	    if (!fp) fp = stdout;
	    xem = (double *) malloc (npmax * sizeof(double));
	    yem = (double *) malloc (npmax * sizeof(double));
	    for (i = 0; i < npair; i++) {
		double xi, eta;
		if (hiordok) {
		  xy2xieta (a, b, smx[i], smy[i], &xi, &eta);
		  /* (convert from rad to pix) */
		  xem[i] = fabs((xi-gmx[i])/psx0_g);
		  yem[i] = fabs((eta-gmy[i])/psy0_g);
		}
		else {
		  RADec2xy (fip, gmr[i], gmd[i], &xi, &eta);
		  xem[i] = fabs(smx[i]-xi);
		  yem[i] = fabs(smy[i]-eta);
		}
		fprintf (fp, "%g %g %g %g\n", smx[i], smy[i], 100*xem[i],
							      100*yem[i]);
	    }
	    if (fp != stdout) fclose (fp);
	    dmedian (xem, npair, &mxerr);
	    dmedian (yem, npair, &myerr);
	    free ((void *)xem);
	    free ((void *)yem);

	    /* file of gnuplot commands to display nicely */
	    fp = fopen ("/tmp/wcs.gnp", "w");
	    if (!fp) fp = stdout;
	    fprintf (fp, "# 'load' this file into gnuplot to show stars in fit\n");
	    fprintf (fp, "set size ratio %f\n", (float)fip->sh/(float)fip->sw);
	    fprintf (fp, "set pointsize 2\n");
	    fprintf (fp, "set grid\n");
	    fprintf (fp, "set key below reverse\n");
	    fprintf (fp, "set xrange [0:%d]\n", fip->sw);
	    fprintf (fp, "set yrange [%d:0]\n", fip->sh);
	    fprintf (fp, "set xlabel 'x'\n");
	    fprintf (fp, "set ylabel 'y'\n");
	    fprintf (fp, "set title \"");
	    if (! hiordok) fprintf (fp, "WCS");
	    if (hiordok && nparam == 12) fprintf (fp, "Quadratic");
	    if (hiordok && nparam == 20) fprintf (fp, "Cubic");
	    if (hiordok && nparam == 26) fprintf (fp, "DSS");
	    fprintf (fp, " Fit Map\\nMedian xerr %.2f = %.2f'', yerr %.2f = %.2f''; drawn @ scale/200\"\n",
			mxerr, mxerr*3600*fabs(raddeg(t_sx)),
			myerr, myerr*3600*fabs(raddeg(t_sy)));
	    fprintf (fp,
	"plot '/tmp/wcs.s' ti '%d Image stars', '/tmp/wcs.c' ti '%d Catalog stars', '/tmp/wcs.fit' ti '%d used in fit' with xyerrorbars ps 0\n", ns, ng, npair);
	    fprintf (fp, "pause -1\n");
	    if (fp != stdout) fclose (fp);
}
#endif


/* Match image and catalogue stars using distances - depends critically on
 * pixel scale being accurate.
 * In each of image and catalogue, choose a base star to measure distances
 * from.  If the two base stars correspond, then there should be several
 * other stars at the same distance in the image as in the catalogue.  If
 * not, then choose a different pair of base stars.  Failure should take
 * time of order n^2 (all possible pairs as base stars), success should be
 * quicker.
 */
static void
matchByDist (double sx[], double sy[], int ns,
 double gr[], double gd[], double gx[], double gy[], int ng,
 double x2as, double y2as, double MATCHDIST, int nenough, int npmax,
 int mats[], int matg[], int *np)
     /*
      *  sx, sy     test stars, image locations, pixels
      *  ns         number of entries in sx[] and sy[]
      *  gr, gd     reference stars, ra/dec, rads
      *  gx, gy     reference stars, nominal x/y image locations, pixels
      *  ng         number of entries in gr[] and gd[] and gx[] and gy[]
      *  x2as, y2as factors converting x/y image pixels to arcsec, assuming
      *             MATCHDIST in arcsec
      *  MATCHDIST  dist. within which image/catalogue star considered same
      *  nenough    don't continue to next base pair if found at least this
      *             no. of pairs with current base pair
      *  npmax      maximum number of pairs (= lower of ns, ng)
      *
      *    If a match is found, indices of matching stars are written to
      *    mats, matg.  If no match is found, *np = 0.
      *
      *  mats, matg indices of matched pairs, at most npmax
      *  *np        number of star pairs, at most npmax
      */
{
  DistInfo *sdi, *gdi; /* malloced distances */
  int ibs, ibg;        /* index of base star, image and catalogue */
  int nm = 0;          /* number of matches with best base pair so far */
  int nmc = 0;         /* number of provisional matches, current base pair */
  int *matsc, *matgc;  /* malloced indices of provisionally matched pairs
                          with current base pair */
#ifdef MATCH_TRACE
#define CHARS 3000
  char infostr[CHARS];
  char* cptr;
#endif
  int enough = 0;      /* satisfied that enough pairs of stars are matched */
  int i, j, im;

#ifdef MATCH_TRACE
  {
    double w;
    printf ("MATCH_TRACE:\n");
    printf ("Input (unmatched) to matchByDist:\n");
    if (! getRealFITS (&fim_g,"CRVAL1", &w)) printf ("ra=%f  ", w/15);
    if (! getRealFITS (&fim_g,"CRVAL2", &w)) printf ("dec=%f  ", w);
    if (! getRealFITS (&fim_g,"CROTA2", &w)) printf ("rot=%f  ", w);
    printf ("sx=%f  ", x2as);
    printf ("sy=%f\n", y2as);
  }
  for (i = j = 0; i < ns && j < ng; i++, j++)
    printf ("(%3d) s[%7.2f,%7.2f] g[%7.2f,%7.2f] g[%9.5f,%9.5f]\n",
	     i, sx[i], sy[i], gx[j], gy[j], raddeg(gr[j]), raddeg(gd[j]));
  for (; i < ns; i++)
    printf ("(%3d) s[%7.2f,%7.2f]\n", i, sx[i], sy[i]);
  for (; j < ng; j++)
    printf ("(%3d)                    g[%7.2f,%7.2f] g[%9.5f,%9.5f]\n",
	     j, gx[j], gy[j], raddeg(gr[j]), raddeg(gd[j]));
#endif

  matsc = (int *) malloc (npmax * sizeof(int));
  matgc = (int *) malloc (npmax * sizeof(int));

  sdi = (DistInfo *) malloc (ns * sizeof(DistInfo));
  gdi = (DistInfo *) malloc (ng * sizeof(DistInfo));

  /* keep trying pairs of base stars till enough matches */
  for (ibg = 0; ibg < ng && ! enough; ibg++) {
    /* Pythagorean distances */
    for (j = 0; j < ng; j++) {
      gdi[j].i = j;
      gdi[j].d =
	sqrt( pow((gx[j]-gx[ibg])*x2as,2) + pow((gy[j]-gy[ibg])*y2as,2) );
    }
    /* sorting dists will make it easier to find pairs that are the same */
    qsort (gdi, ng, sizeof(DistInfo), compareDist);
    for (ibs = 0; ibs < ns && ! enough; ibs++) {
      for (i = 0; i < ns; i++) {
	sdi[i].i = i;
	sdi[i].d =
	  sqrt( pow((sx[i]-sx[ibs])*x2as,2) + pow((sy[i]-sy[ibs])*y2as,2) );
      }
      qsort (sdi, ns, sizeof(DistInfo), compareDist);

      /* find pairs of distances that are the same, within MATCHDIST,
         and set nmc to the number of pairs found */
#ifdef MATCH_TRACE
      infostr[0]='\0';
#endif
      for (nmc = i = j = 0; i < ns && j < ng;  ) {
	/* if distances agree then i (image) & j (catalogue) is a new
	   candidate match */
        if (fabs(sdi[i].d-gdi[j].d) < MATCHDIST) {
	  /* im will cycle through each already matched pair */
#ifdef MATCH_TRACE
	  if ( (cptr=strchr(infostr,'\0')) - infostr < CHARS - 99 )
	    sprintf(cptr, "candidate %i:%i: ", i,j);
	  else sprintf(cptr-9, "........\n");
#endif
          for (im = 0; im < nmc; im++) {
	    double dx,dy,ds,dg;
	    dx=sx[matsc[im]]-sx[sdi[i].i];
	    dy=sy[matsc[im]]-sy[sdi[i].i];
	    ds=sqrt( pow(dx*x2as,2) + pow(dy*y2as,2) );
	    dx=gx[matgc[im]]-gx[gdi[j].i];
	    dy=gy[matgc[im]]-gy[gdi[j].i];
	    dg=sqrt( pow(dx*x2as,2) + pow(dy*y2as,2) );
	    /* break if new candidate gives mismatched distance from any of
	       the already matched ones */
	    if (fabs(ds-dg) > MATCHDIST) break;
#ifdef MATCH_TRACE
	  if ( (cptr=strchr(infostr,'\0')) - infostr < CHARS - 99 )
	    sprintf(cptr, "%iok ", im);
	  else sprintf(cptr-9, "........\n");
#endif
	  }
	  /* only accept new candidate match if we didn't break out of the
	     for loop */
	  if (im == nmc) {
	    matsc[im] = sdi[i].i;
	    matgc[im] = gdi[j].i;
	    nmc++;
#ifdef MATCH_TRACE
	  if ( (cptr=strchr(infostr,'\0')) - infostr < CHARS - 99 )
	    sprintf(cptr, "(%i,%i) accepted",sdi[i].i,gdi[j].i);
	  else sprintf(cptr-9, "........\n");
#endif
	  }
	  i++; j++;
#ifdef MATCH_TRACE
	  sprintf(strchr(infostr,'\0'), "\n");
#endif
	}
        else if (sdi[i].d > gdi[j].d)
	  j++;
        else
	  i++;
      }
      /**************** Problem with above procedure is once a candidate
			match is accepted, it's accepted, can't go back and
			reject it.  In particular, the 2nd matched pair may
			quite often be wrong.  Either we can amend the above,
			or we can hope that even if you fail with one correct
			pair of base stars matched, you'll succeed with
			a different pair of base stars.
			Another imperfection is that if a pair is close in
			dist, then is tested and found not to be a pair,
			it isn't checked if either member of the pair matches
			any other star in dist. ****************/

      if (nmc > nm) {
#ifdef MATCH_TRACE
	/* Printing distances for every pair of base stars would produce a
	 * large amount of output, even if tracing; so only print each time
	 * we improve the no. of matches.  Can move these lines if really
	 * want to check every detail every time.
	 */
	printf(infostr);
	printf("best base pair so far  s(%3i) g(%3i)   %3i pairs\n",
		ibs,ibg,nmc);
	printf (
	 "Distances (arcsec) were:\n           Image          Catalogue\n");
	for (i = j = 0; i < ns && j < ng; i++, j++)
	  printf ("%3i: %9.2f (%3i) %9.2f (%3i)\n",
		   i, sdi[i].d, sdi[i].i, gdi[j].d, gdi[j].i);
	for (; i < ns; i++)
	  printf ("%3i: %9.2f (%3i)\n", i, sdi[i].d, sdi[i].i);
	for (; j < ng; j++)
	  printf ("%3i:                 %9.2f (%3i)\n",
		   j, gdi[j].d, gdi[j].i);
#endif
	nm = nmc;
	for (im = 0; im < nm; im++) {mats[im]=matsc[im]; matg[im]=matgc[im];}
      }
      if (nm == npmax || nm >= nenough) enough = 1;
    }
  }

  free ((void *)matsc);
  free ((void *)matgc);
  free ((void *)sdi);
  free ((void *)gdi);

  *np = nm;
}


/* Match image and catalogue stars when the catalogue stars have already been
 * converted to virtually correct nominal x/y image locations.
 * The idea is that a 5-parameter WCS fit has already been done, meaning that
 * image and catalogue coordinates should match to within a few arcsec.
 * This function should then be able to match a greater number of pairs of
 * stars, which will then allow a more accurate astrometric fit to be done.
 *
 * The algorithm is currently inefficient because we want a working program
 * as soon as possible.  It can be improved in future if necessary, e.g. by
 * ordering one or more of the input arrays.
 * Currently we simply take each catalogue star in turn, and check it against
 * each image star, seeing if exactly one image star is found within
 * the distance MATCHDIST.
 * At present, it isn't checked whether two catalogue stars are within
 * MATCHDIST of the same image star.  The ip.cfg parameter FSMINSEP actually
 * imposes a minimum separation between image stars.
 */
static void
matchByPos (double sx[], double sy[], int ns,
 double gr[], double gd[], double gx[], double gy[], int ng,
 double xsc, double ysc, double MATCHDIST, int npmax,
 int mats[], int matg[], int *np)
     /*
      *  sx, sy     test stars, image locations, pixels
      *  ns         number of entries in sx[] and sy[]
      *  gr, gd     reference stars, ra/dec, rads (only used for tracing)
      *  gx, gy     reference stars, nominal x/y image locations, pixels
      *  ng         number of entries in gx[] and gy[]
      *  xsc, ysc   x/y pixel scales, value that sx-gx or sy-gy is multiplied
      *             by to make units same as MATCHDIST
      *  MATCHDIST  dist. within which image/catalogue star considered same
      *  npmax      maximum number of pairs (= lower of ns, ng)
      *
      *    If a match is found, indices of matching stars are written to
      *    mats, matg.  If no match is found, *np = 0.
      *
      *  mats, matg indices of matched pairs, at most npmax
      *  *np        number of star pairs, at most npmax
      */
{
  double distsq;
  int nm = 0;     /* number of matches so far */
  int i, j, im;

#ifdef MATCH_TRACE
  printf ("MATCH_TRACE:\n");
  printf ("Input (unmatched) to matchByPos:\n");
  for (i = j = 0; i < ns && j < ng; i++, j++)
    printf ("(%3d) s[%7.2f,%7.2f] g[%7.2f,%7.2f] g[%9.5f,%9.5f]\n",
	     i, sx[i], sy[i], gx[j], gy[j], raddeg(gr[j]), raddeg(gd[j]));
  for (; i < ns; i++)
    printf ("(%3d) s[%7.2f,%7.2f]\n", i, sx[i], sy[i]);
  for (; j < ng; j++)
    printf ("(%3d)                    g[%7.2f,%7.2f] g[%9.5f,%9.5f]\n",
	     j, gx[j], gy[j], raddeg(gr[j]), raddeg(gd[j]));
#endif

  for (distsq = pow(MATCHDIST,2), j = 0; j < ng && nm < npmax; j++) {
    for (im = i = 0; i < ns; i++) {
      if ( pow((sx[i]-gx[j])*xsc,2) + pow((sy[i]-gy[j])*ysc,2) < distsq ) {
        if (im == 0) {
          /* if this is the first image star near this catalogue star,
           * provisionally accept the match (don't increment nm till really
           * accept match)
           */
          im=1; mats[nm]=i; matg[nm]=j;
        }
        else {
          /* if this is the second image star near this catalogue star,
           * reject because ambiguous
           */
          im=2; break;
        }
      }
    }
    /* if all image stars have been checked, and exactly one image star found
     * near this catalogue star, accept the match
     */
#ifdef MATCH_TRACE
    if (im == 1) printf ("(%i,%i) accepted\n", mats[nm],matg[nm]);
#endif
    if (im == 1) nm++;
  }

  *np = nm;
}


#ifdef FITS_SOLUTION_TRACE
/* get astrometric fit parameters AMDX*, AMDY* from FITS header
 * return 0 if all found (all must be present, even if zero), else -1
 */
static int
getAMDFITS (FImage *fip, double a[], double b[])
{
  char name[7];
  int i;

  for (i = 0; i < 14; i++) {
    sprintf (name, "AMDX%i", i);
    if (getRealFITS (fip, name, &a[i]) < 0) return (-1);
    sprintf (name, "AMDY%i", i);
    if (getRealFITS (fip, name, &b[i]) < 0) return (-1);
  }

  return (0);
}
#endif


/* Pixel coordinates to "standard coordinates" in radians,
 * assuming a1-13, b1-13 set correctly
 */
static void
xy2xieta(double a[], double b[], double x, double y, double *xi, double *eta)
{
  double x2y2 = x*x + y*y;
  *xi  = a[1]*x + a[2]*y + a[3] + a[4]*x*x + a[5]*x*y + a[6]*y*y +
         a[8]*x*x*x + a[9]*x*x*y + a[10]*x*y*y + a[11]*y*y*y +
         a[7]*x2y2 + a[12]*x*x2y2 + a[13]*x*x2y2*x2y2;
  *eta = b[1]*y + b[2]*x + b[3] + b[4]*y*y + b[5]*x*y + b[6]*x*x +
         b[8]*y*y*y + b[9]*x*y*y + b[10]*x*x*y + b[11]*x*x*x +
         b[7]*x2y2 + b[12]*y*x2y2 + b[13]*y*x2y2*x2y2;
}


/* RA, dec of projection centre + n (RA,dec)'s to n (xi,eta)'s (all radians)
 * Input (RA,dec)'s given as pointers so that this function can be called for
 * arrays (setting n=1) or single stars
 */
static void
RADec2xieta (double rc, double dc, int n, double *r, double *d,
             double *xi, double *eta)
{
  int i;
  double cr, sr, cd, sd, cdc, sdc, R, *rp, *dp, *xip, *etap;
  cdc=cos(dc); sdc=sin(dc);
  for (i = 0, rp = r, dp = d, xip = xi, etap = eta;  i < n;
       i++,   rp++,   dp++,   xip++,    etap++) {
    cr = cos(*rp-rc);  sr = sin(*rp-rc);  cd = cos(*dp);  sd = sin(*dp);
    R = 1/(cd*cr*cdc+sd*sdc);  *xip = R*cd*sr;  *etap = R*(sd*cdc-cd*cr*sdc);
  }
}


#ifdef THRESH_TRACE
/* RA, dec of projection centre + n (xi,eta)'s to n (RA,dec)'s (all radians)
 * Input (xi,eta)'s given as pointers so that this function can be called for
 * arrays (setting n=1) or single stars
 */
static void
xieta2RADec (double rc, double dc, int n, double *xi, double *eta,
             double *r, double *d)
{
  int i;
  double tdc, *rp, *dp, *xip, *etap;
  tdc = tan(dc);
  for (i = 0, rp = r, dp = d, xip = xi, etap = eta;  i < n;
       i++,   rp++,   dp++,   xip++,    etap++) {
    *rp = atan( (*xip/cos(dc))/(1-*etap*tdc) ) + rc;
    *dp = atan( (*etap+tdc)*cos(*r-rc)/(1-*etap*tdc) );
  }
}
#endif


#ifdef THRESH_TRACE
static void
traceThresh (double a[], double b[], double residsort[], double rmed,
             double threshresid)
{
  char ras[32], decs[32], sras[32], sdecs[32];
  double xi, eta, smr, smd;
  int i;
  printf ("THRESH_TRACE:\n");
  printf ("npair=%3d rmed=%8.3f thresh=%6.3f\n", npair_g, rmed, threshresid);
  printf ("        pixel    Image %s Catalogue     RA   dec arcsec sorted\n",
          "                   ");
  for (i = 0; i < npair_g; i++) {
    double r2as = raddeg(1)*3600;
    xy2xieta (a, b, sx_g[i], sy_g[i], &xi, &eta);
    xieta2RADec (a[0], b[0], 1, &xi, &eta, &smr, &smd);
    fs_sexa (sras, radhr(smr), 2, 360000);
    fs_sexa (sdecs, raddeg(smd), 3, 36000);
    fs_sexa (ras, radhr(gr_g[i]), 2, 360000);
    fs_sexa (decs, raddeg(gd_g[i]), 3, 36000);
    printf ("%3d: [%4.0f,%4.0f][%s,%s] [%s,%s] %5.2f %5.2f %5.2f %5.2f%c\n",
            i, sx_g[i], sy_g[i], sras, sdecs, ras+6, decs+7,
            (xi-gx_g[i])*r2as, (eta-gy_g[i])*r2as, 
            resid_g[i], residsort[i],
            residsort[i] < threshresid ? ' ' : '*');
  }
  printf ("Max%8.3f   Mean%8.3f   Rms%8.3f arcsec\n",
          resid_max, resid_sum/npair_g, sqrt(resid_sum2/npair_g));
}
#endif


#ifdef TIME_TRACE
static void
traceTime (char string[])
{
  static struct timeval  tv0,tv1;
  static struct timezone tz0,tz1;
  if (string == "Reset clock") gettimeofday(&tv0,&tz0);
  gettimeofday(&tv1,&tz1);
  printf("TIME_TRACE:  %s%9.3f sec\n",
         string, (tv1.tv_sec-tv0.tv_sec)+(tv1.tv_usec-tv0.tv_usec)*1E-6);
}
#endif
