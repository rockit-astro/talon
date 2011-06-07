/*
 * Updated version: Merges DJA version "Distance" WCS/Astrometry work
 * with standard Talon libraries.
 *
 * Control Compilation via wcs.h header #define USE_DISTANCE_METHOD
 * Set to 1 (to enable) or 0 (to disable, and use former triangle method only)
 *
 * Last update DJA 020917
 * Merged STO 021001
 * 021220 (DJA):  add MINPAIR, MAXROT config params
 * 030103 (STO):  merged w/new ip.cfg system
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>

#include "P_.h"
#include "astro.h"
#include "configfile.h"
#include "telenv.h"
#include "fits.h"
#include "wcs.h"
#include "fieldstar.h"

#if USE_DISTANCE_METHOD
#include "setwcsfitsd.h"  // diagnostic tracing options defined here
#endif

#define	HUNTFRAC	0.33	/* frac of image to move each step during hunt*/
#define	GSCLIM		18.0	/* deepest GSC search, mag */
#define	USNOLIM		20.0	/* deepest USNO search, mag */

#if USE_DISTANCE_METHOD
extern int findRegistrationD
    (FImage *fip, double ra0, double dec0, double rot0,
    double psx0, double psy0, double sx[], double sy[], int ns, double gr[],
    double gd[], int ng,
    int nparam, int MINPAIR, int TRYSTARS, double MAXROT, double MATCHDIST,
    double *rhp, double *residp);

#else
#define MAXNSTARS	25	/* max image star pairs we try when nailing */

extern int findRegistration (FImage *fip, double ra0, double dec0, double rot0,
    double psx0, double psy0, double sx[], double sy[], int ns, double gr[],
    double gd[], int ng, double *residp);
#endif

static int spiralToFit (FImage *fip, int wantusno, double sprad, double sx[],
    double sy[], int ns, int verbose, char msg[]);
static int tryOneLoc (FImage *fip, int wantusno, double sx[], double sy[],
    int ns, double ra0, double dec0, double rot0, double fov, double psx0,
    double psy0, int verbose, char msg[]);
static void nailIt (FImage *fip, int wantusno, double sx[], double sy[],int ns, int verbose);
static int getNominal (FImage *fip, int verbose, double *rap, double *decp,
    double *fovp, double *psxp, double *psyp, char msg[]);
static void sortStars (double *sx, double *sy, double *sb, int ns);

static int (*bail_fp)(void);	/* call to see if user wants to bail out */

#ifdef TIME_TRACE
#include <sys/time.h>
#include <unistd.h>
static void traceTime (char string[]);
#endif


/* set the C* WCS fields in fip. do it by finding stars in fip and in GSC and
 *   finding the rotation, scale and offsets which result in a best-fit.
 * be willing to search around the nominal center by as much as hunt rads.
 * verbose generates extra info on stdout.
 * return 0 if all ok, else -1.
 * N.B. we initially set msg[0] to '\0'; but even if we return 0 it might have
 *   a message in it so caller should always print msg if it doesn't start with
 *   '\0';
 */
int
setWCSFITS (FImage *fip, int wantusno, double hunt, int (*bfp)(),
int verbose, char msg[])
{
	int *sx=0, *sy=0;	/* malloced star coords */
	CamPixel *sb=0;		/* malloced star brightnest pixel */
	double *sxd=0, *syd=0;	/* same as sx and sy but as doubles */
	double *sbd=0;		/* same as sb but as doubles */
	StarDfn sd;		/* used to refine star locs */
	int ns;			/* n image stars */
	int nbs;		/* n brightest image stars we actually use */
	int ret = 0;
	int i;

#ifdef TIME_TRACE
	traceTime ("Reset clock");
#endif

	/* get params */
	loadIpCfg();
	
#if USE_DISTANCE_METHOD
	/* write values of config params (for definition of which pointer
	 * type valp is, see readCfgFile in libmisc/configfile.c)
	 */
	if (verbose) {
/*	
	  printf ("Values from ip.cfg: ");
	  for (i = 0; i < NIPCFG; i++) {
	    if (ipcfg[i].type == CFG_INT)
	      printf (" %s=%i", ipcfg[i].name, *((int *)ipcfg[i].valp));
	    else if (ipcfg[i].type == CFG_DBL)
	      printf (" %s=%.1f", ipcfg[i].name, *((double *)ipcfg[i].valp));
	    if (i == NIPCFG-1) printf ("\n");
	    else if (i%4 == 3) printf ("\n                    ");
	  }
*/
		printf("ip.cfg path: %s\n",getCurrentIpCfgPath());
		printf("    MAXRESID  = %d\n"
		       "    MAXISTARS = %d\n"
		       "    MAXCSTARS = %d\n"
		       "    BRCSTAR   = %d\n"
		       "    MAXPAIR   = %d\n"
		       "    TRYSTARS  = %d\n"
		       "    MATCHDIST = %.2g\n"
		       "    REJECTDIST= %.2g\n"
		       "    ORDER     = %d\n",
		       MAXRESID,MAXISTARS,MAXCSTARS,BRCSTAR,
		       MAXPAIR,TRYSTARS,MATCHDIST,REJECTDIST,ORDER);		
	}
#endif
	
	/* reset initial message */
	msg[0] = '\0';

	/* save bale function */
	bail_fp = bfp;
	
#ifdef TIME_TRACE
	traceTime ("Loaded ip.cfg");
#endif
	
	/* discover star-like things in the image */
	ns = findStars (fip->image, fip->sw, fip->sh, &sx, &sy, &sb);
	if (ns < MINPAIR) {
	    sprintf (msg, "Need at least %d image stars but only found %d",
								MINPAIR, ns);
	    ret = -1;
	    goto out;
	}

#ifdef STARS_TRACE
	for (i = 0; i < ns; i++)
	    printf ("%3d: [%4d,%4d]\n", i, sx[i], sy[i]);
#endif

	if (verbose)
	    printf ("found %d image stars\n", ns);

#ifdef TIME_TRACE
	traceTime ("Found brightest pixels of stars");
#endif

	/* sort stars by brightness.
	 * (need doubles for spiralToFit anyway)
	 */
	sxd = (double *)malloc (ns*sizeof(double));
	syd = (double *)malloc (ns*sizeof(double));
	sbd = (double *)malloc (ns*sizeof(double));
	if (!sbd || !sxd || !syd) {
	    sprintf (msg, "Mallocs failed for %d stars", ns);
	    ret = -1;
	    goto out;
	}
	for (i = 0; i < ns; i++) {
	    sxd[i] = sx[i];
	    syd[i] = sy[i];
	    sbd[i] = sb[i];
	}
	sortStars (sxd, syd, sbd, ns);

#ifdef TIME_TRACE
	traceTime ("Sorted by brightest pixels");
#endif

#ifdef SSTARS_TRACE
	for (i = 0; i < ns; i++)
	    printf ("%3d: [%4.0f,%4.0f]\n", i, sxd[i], syd[i]);
#endif

#if USE_DISTANCE_METHOD	
	/* clamp to MAXPAIR or MAXISTARS brightest */
	nbs = MAXPAIR > MAXISTARS ? MAXPAIR : MAXISTARS;
	nbs = ns > nbs ? nbs : ns;
#else
	/* clamp to MAXNSTARS brightest */
	nbs = ns > MAXNSTARS ? MAXNSTARS : ns;
#endif
	/* finished with brightness and integer info */
	free ((void *)sbd); sbd = 0;
	free ((void *)sb); sb = 0;
	free ((void *)sx); sx = 0;
	free ((void *)sy); sy = 0;

	/* get better positions */
	sd.rsrch = 0;
	sd.rAp = 0; 
	sd.how = SSHOW_HERE;
	for (i = 0; i < nbs; i++) {
	    StarStats ss;
	    starStats ((CamPixel *)(fip->image), fip->sw, fip->sh, &sd,
				(int)(sxd[i]+.5), (int)(syd[i]+.5), &ss, msg);
	    sxd[i] = ss.x;
	    syd[i] = ss.y;
#ifdef BSTARSTATS_TRACE
	    /* trace bright star statistics */
	    if (i == 0) printf ("%s\n%s\n",
	      "         Centroid       FWHM(pix)  Gauss.peak incl.sky",
	      "         x       y       x    y       x       y        sky");
	    printf ("%3i:%9.2f%8.2f%7.2f%5.2f%10.1f%8.1f%8i\n",
	     i, ss.x, ss.y, ss.xfwhm, ss.yfwhm, ss.xmax, ss.ymax, ss.Sky);
#endif
	}

#ifdef TIME_TRACE
	printf ("centroided %d image stars\n", nbs);
	traceTime ("Centroided");
#endif	

	/* hunt with this set */
	ret = spiralToFit (fip, wantusno, hunt, sxd, syd, nbs, verbose, msg);

    out:
	if (sx)  free ((void *)sx);
	if (sy)  free ((void *)sy);
	if (sb)  free ((void *)sb);
	if (sxd) free ((void *)sxd);
	if (syd) free ((void *)syd);
	if (sbd) free ((void *)sbd);

	return (ret);
}

/* hunt around in a spiral out to sprad looking for a fit.
 * if find set C* in fip and return 0, else -1.
 */
static int
spiralToFit (FImage *fip, int wantusno, double sprad, double sx[],
double sy[], int ns, int verbose, char msg[])
{
	double ra0, dec0;	/* starting location, rads */
	double psx0, psy0;	/* starting pixel scales, rads/pixel */
	double fov0;		/* nominal field of view, rads */
	double dra, ddec;	/* spiral step sizes */
	int nhunt;		/* number of steps in hunt pattern */
	int ns0;		/* n stars to use during initial hunt */
	int i, j;
	int s;
	int r;

	/* get initial nominal position and scale */
	if (getNominal (fip, verbose, &ra0, &dec0, &fov0, &psx0, &psy0, msg)< 0)
	    return (-1);

	/* just use MAXISTARS until have a very good candidate */
	ns0 = ns > MAXISTARS ? MAXISTARS : ns;

	/* find spiral steps */
	ddec = fip->sh*fabs(psy0)*HUNTFRAC;
	dra = fip->sw*fabs(psx0)*HUNTFRAC/cos(dec0);
	nhunt = (int)floor(sprad/ddec);

	/* go hunting in a spiral */
	for (r = 0; r <= nhunt; r++) {
	    for (i = -r; i <= r; i++) {
		for (j = -r; j <= r; j++) {
		    double hra, hdec;

		    /* just want a hollow square */
		    if (i > -r && i < r && j > -r && j < r)
			continue;

		    /* compute hunting centers -- allow for over-the-poles */
		    hdec = dec0 + j*ddec;
		    hra = ra0 + i*dra;
		    if (hdec > PI/2) {
			hra += PI;
			hdec = PI-hdec;
		    } else if (hdec < -PI/2) {
			hra += PI;
			hdec = -PI-hdec;
		    }
		    range (&hra, 2*PI);

		    /* try this center */
		    s = tryOneLoc (fip, wantusno, sx, sy, ns0, hra, hdec,
					0.0, fov0, psx0, psy0, verbose, msg);

		    /* see how it went */
		    switch (s) {
		    case 0:
			nailIt (fip, wantusno, sx, sy, ns, verbose);
			return (0);		/* found fit! */
		    case -2: return (-1);	/* fatal trouble */
		    case -1: break;		/* didn't fit -- keep hunting */
		    default: printf ("Bad tryOneLoc: %d\n", s); exit(1);
		    }
		}
	    }
	}

	/* 'fraid not */
	sprintf(msg,"No solutions in %.2f degree search", raddeg(sprad));
					
	return (-1);
}

/* try the given location as a suspected nominal image center.
 * return  0 if find a fit and C* in fip are filled in;
 * return -1 if no good fit is found;
 * return -2 if something goes so wrong we should stop trying altogether.
 * N.B. we assume GSCSetup() and/or USNOSetup() have been called.
 *
 #if USE_DISTANCE_METHOD
 * Previously, tryOneLoc was called during the spiral search with ns0 image
 * stars, and by nailIt with ns image stars, where ns >= ns0.  Either way,
 * tryOneLoc would call findRegistration to try matching AND determine the
 * astrometric (5-parameter WCS) fit.
 *
 * This has been changed.  Now, during the spiral search, findRegistrationD
 * is called to try matching (using distance method) and determine a
 * 5-parameter WCS fit.  But when tryOneLoc is called by nailIt, tryOneLoc
 * now instead assumes we already have a 5-parameter WCS fit that isn't too
 * bad, and calls findRegistrationD not only to get a final WCS fit but also
 * to IMPROVE the astrometric fit by using more parameters.  In order to tell
 * tryOneLoc which we want, nailIt calls tryOneLoc with -ns rather than ns.
 #endif
 */
static int
tryOneLoc (FImage *fip, int wantusno, double sx[], double sy[], int ns,
double ra0, double dec0, double rot0, double fov, double psx0, double psy0,
int verbose, char msg[])
{
	FieldStar *gsc=0;	/* the array of ng GSC stars */
	int ng;				/* n of GSC stars */
	double *gr=0, *gd=0;/* GSC star positions for sorting */
	double *gb=0;		/* GSC star brightnesses for sorting */
	int nbg;			/* n brightest GCS stars we actually use */
	char lmsg[1024];	/* catalog error message */
	double r;			/* fit residual, pixels */
	int ret = 0;		/* assume success */
	int i;
#if USE_DISTANCE_METHOD
	int nparam;         /* no. of astrometric fit params */
	double rh;			/* higher order fit residual, arcsec */
#endif	

	/* first get USNO -- ignore any errors */
	ng = wantusno ? USNOFetch (ra0, dec0, fov, USNOLIM, &gsc, lmsg) : 0;
	if (ng <= 0) {
	    if (verbose && ng < 0)
		printf ("USNO: %s\n", lmsg);
	    if (gsc) {
		free ((void *)gsc);
		gsc = 0;
	    }
	    ng = 0;
	}

	/* then add GCS stars */
	ng = GSCFetch (ra0, dec0, fov, GSCLIM, &gsc, ng, lmsg);
	if (ng < MINPAIR) {
	    if (ng < 0)
		sprintf (msg, "Error getting GSC stars: %s", lmsg);
	    else
		sprintf (msg, "Need at least %d GSC stars but only found %d",
								MINPAIR, ng);
	    ret = -1;
	    goto out;
	}
	if (verbose) {
	    char rstr[64], dstr[64];
	    fs_sexa (rstr, radhr(ra0), 2, 36000);
	    fs_sexa (dstr, raddeg(dec0), 3, 3600);
	    printf ("found %3d GSC stars at %s %s", ng, rstr, dstr);
#if !USE_DISTANCE_METHOD
		printf("\n");
#endif	
	}

	/* break ra/dec and brightness into separate arrays for sorting, etc.
	 * only keep ones dimmer than BRCSTAR
	 */
	gr = (double *) malloc (ng * sizeof(double));
	gd = (double *) malloc (ng * sizeof(double));
	gb = (double *) malloc (ng * sizeof(double));
	if (!gr || !gd || !gb) {
	    sprintf (msg, "Malloc failed for %d GSC stars", ng);
	    ret = -2;
	    goto out;
	}
	nbg = 0;
	for (i = 0; i < ng; i++) {
	    if (gsc[i].mag >= BRCSTAR) {
		gr[nbg] = gsc[i].ra;
		gd[nbg] = gsc[i].dec;
		gb[nbg] = -gsc[i].mag; /* want greater to mean brighter */
		nbg++;
	    }
	}

	/* sort GSC by brightness */
	sortStars (gr, gd, gb, nbg);
	
#if USE_DISTANCE_METHOD	
	if (verbose) printf (", %i fainter than mag %i\n", nbg, BRCSTAR);
#endif

	/* finished with gb now */
	free ((void *)gb); gb = 0;

	/* clamp to MAXCSTARS */
	if (nbg > MAXCSTARS)
	    nbg = MAXCSTARS;

	/* see if user wants to bail */
	if (bail_fp && (*bail_fp)()) {
	    sprintf (msg, "User stopped");
	    ret = -2;
	    goto out;
	}

	/* try to find best fit */
#if USE_DISTANCE_METHOD	
	r = MAXRESID;
	rh = REJECTDIST;
	nparam = ns > 0 ? 5 : (ORDER == 2 ? 12 : (ORDER == 3 ? 20 : 26));
	if (findRegistrationD
	    (fip, ra0, dec0, rot0, psx0, psy0, sx, sy, abs(ns), gr, gd, nbg,
	     nparam, MINPAIR, TRYSTARS, MAXROT, MATCHDIST, &rh, &r)<0) {
	    if (nparam >= 12 && rh > 0 && verbose)
	      printf ("Higher order fit residual (rms): %.2f arcsec\n", rh);
	    if (nparam >= 12 && rh < 0)
	      fprintf (stderr, "Did not find higher order fit\n");
	    sprintf (msg, "Star registration failed.");
	    ret = -1;			/* let caller keep going */
	} else {
	    /* yes! */
	    sprintf (msg, "Fit residual: %.1f pixels", r);
	    if (verbose)
		printf ("Fit residual (mean): %.2f pixels\n", r);
	    if (nparam >= 12 && rh > 0 && verbose)
	      printf ("Higher order fit residual (rms): %.2f arcsec\n", rh);
	    if (nparam >= 12 && rh < 0)
	      fprintf (stderr, "Did not find higher order fit\n");
	    sprintf (msg, "Star registration failed.");
	    ret = 0;
	}
#else
	r = MAXRESID;
	if (findRegistration (fip, ra0, dec0, rot0, psx0, psy0, sx, sy, ns,
							gr, gd, nbg, &r)<0) {
	    sprintf (msg, "Star registration failed.");
	    ret = -1;			/* let caller keep going */
	} else {
	    /* yes! */
	    sprintf (msg, "Fit residual: %.1f pixels", r);
	    if (verbose)
		printf ("Fit residual: %.1f pixels\n", r);
	    ret = 0;
	}
#endif
    out:
	if (gsc) free ((void *)gsc);
	if (gr) free ((void *)gr);
	if (gd) free ((void *)gd);
	if (gb) free ((void *)gb);

	return (ret);
}

/* fip has been found during the hunt so we know where it is quite well.
 * repeat one more time just to really nail it using a good inclusive field.
 */
static void
nailIt (FImage *fip, int wantusno, double sx[], double sy[], int ns, int verbose)
{
	double ra, dec;
	double cd1, cd2;
	double rot;
	char msg[1024];
	double fov;
	int n;

	getRealFITS (fip, "CRVAL1", &ra);
	getRealFITS (fip, "CRVAL2", &dec);
	getRealFITS (fip, "CDELT1", &cd1);
	getRealFITS (fip, "CDELT2", &cd2);
	getRealFITS (fip, "CROTA2", &rot);
	fov = sqrt(cd1*cd1*fip->sw*fip->sw + cd2*cd2*fip->sh*fip->sh);

#if USE_DISTANCE_METHOD
	n = tryOneLoc (fip, wantusno, sx, sy, -ns, degrad(ra), degrad(dec),
	   degrad(rot), degrad(fov), degrad(cd1), degrad(cd2), verbose, msg);
#else	
	n = tryOneLoc (fip, wantusno, sx, sy, ns, degrad(ra), degrad(dec),
		degrad(rot), degrad(fov), degrad(cd1), degrad(cd2), 0, msg);
	(void) verbose; // unused		
#endif		
}

/* find nominal center and scale of given image from header info.
 * return 0 if ok else return -1 with excuse in msg[].
 */
static int
getNominal (FImage *fip, int verbose, double *rap, double *decp, double *fovp,
double *psxp, double *psyp, char msg[])
{
	double cdelt1, cdelt2;
	char str[128];
	double x, y, v;

	/* accept several fields for getting nominal center */
	if (getStringFITS (fip, "RA", str) == 0
				|| getStringFITS (fip, "OBJRA", str) == 0
				|| getStringFITS (fip, "RAEOD", str) == 0) {
	    scansex (str, &v);
	    *rap = hrrad(v);
	} else if (getStringFITS (fip, "CTYPE1", str) == 0
				&& strcmp (str, "RA---TAN") == 0
				&& getRealFITS (fip, "CRVAL1", &v) == 0) {
	    *rap = degrad(v);
	} else {
	    sprintf (msg, "Can not find nominal RA");
	    return (-1);
	}
	if (getStringFITS (fip, "DEC", str) == 0
				|| getStringFITS (fip, "OBJDEC", str) == 0
				|| getStringFITS (fip, "DECEOD", str) == 0) {
	    scansex (str, &v);
	    *decp = degrad(v);
	} else if (getStringFITS (fip, "CTYPE2", str) == 0
				&& strcmp (str, "DEC--TAN") == 0
				&& getRealFITS (fip, "CRVAL2", &v) == 0) {
	    *decp = degrad(v);
	} else {
	    sprintf (msg, "Can not find nominal DEC");
	    return (-1);
	}

	/* find pixel scales and fov */
	if (getRealFITS (fip, "CDELT1", &cdelt1) < 0 ||
				getRealFITS (fip, "CDELT2", &cdelt2) < 0) {
	    sprintf (msg, "CDELT1 and CDELT2 are required");
	    return (-1);
	}
	*psxp = degrad(cdelt1);
	*psyp = degrad(cdelt2);
	x = fip->sw * *psxp;
	y = fip->sh * *psyp;
	*fovp = sqrt(x*x + y*y);

	if (verbose) {
	    char rstr[64], dstr[64];
	    fs_sexa (rstr, radhr(*rap), 2, 36000);
	    fs_sexa (dstr, raddeg(*decp), 3, 3600);
	    printf ("RA=%s DEC=%s FOV=%g\n", rstr, dstr, raddeg(*fovp));
	    printf ("\n");
	}

	return (0);
}

/* structure we use just for building the global star lists needed for sorting
 */
typedef struct {
    double x, y, b;
} _StarInfo;

/* sort in *decreasing* order, as per qsort */
static int
starstatSortF (ssp1, ssp2)
const void *ssp1, *ssp2;
{
	double d = ((_StarInfo *)ssp2)->b - ((_StarInfo *)ssp1)->b;

	if (d < 0)
	    return (-1);
	if (d > 0)
	    return (1);
	return (0);
}

/* sort stars by decreasing *sb, aligning sx and sy to match */
static void
sortStars (double *sx, double *sy, double *sb, int ns)
{
	_StarInfo *sip;
	int i;

	sip = (_StarInfo *) malloc (ns * sizeof(_StarInfo));

	for (i = 0; i < ns; i++) {
	    sip[i].x = sx[i];
	    sip[i].y = sy[i];
	    sip[i].b = sb[i];
	}

	qsort ((void *)sip, ns, sizeof(_StarInfo), starstatSortF);

	for (i = 0; i < ns; i++) {
	    sx[i] = sip[i].x;
	    sy[i] = sip[i].y;
	    sb[i] = sip[i].b;
	}

	free ((void *)sip);
}


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
