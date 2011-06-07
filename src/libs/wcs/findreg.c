/* find best WCS C* registration of two sets of stars.
 * use the most likely pairing based on matching similar triangle technique,
 * see "FOCUS Automatic Catalog Matching Algorithms", Frank Valdes, et al,
 *    valdes@nooa.edu. Published in PASP 107, pg 1119, and online at
 *    ftp://iraf.noao.edu/iraf/docs/focas/focasmatch.ps.Z.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>

#include "P_.h"
#include "astro.h"
#include "fits.h"
#include "wcs.h"
#include "lstsqr.h"


#define THRESH          1.5	/* discard resids > THRESH*median */
#define CLOSETRI        0.005	/* max sep between matching tris in tri space */
#define FTOL		0.0002	/* frac change in chisqr() we call a good fit */
#define	MINLEG		25	/* min length of each tri side, pixels */
#define	MINFAT		.90	/* long side must be < this of sum of other 2 */
#define	MAXPAIR		25	/* most pairs we use for final fitting */
#define	MINPAIR		4	/* minimum pairs we may use for solution */
#define	MINVOTES	4	/* min number of votes for a star to be used */
#define	NOUTLL		4	/* max number of loops to discard outlyers */

/* aux structure to find best matching s and g pairs */
typedef struct {
    float x, y;		/* coords in "triangle space": x=b/a, y=c/a, a>b>c */
    short i, j, k;	/* vertices: indexes into point arrays */
} Triangle;

/* these values are made file-global for use by the chisqr evaluator */
static double resid_max, resid_sum, resid_sum2;
static double *resid_g;
static double *sx_g;
static double *sy_g;
static double *gr_g;
static double *gd_g;
static double psx0_g, psy0_g;
static FImage fim_g;
static int npair_g;

static void init_fim (FImage *fip);
static int gentri (Triangle *tri, double x[], double y[], int n);
static int call_lstsqr (double *t_ra, double *t_dc, double *t_th, double *t_sx,
    double *t_sy);
static void setFITSWCS (FImage *fip, double ra, double dec, double rot,
    double pixszw, double pixszh);
static void dmedian (double a[], int na, double *mp);

/* find best C* in fip such that s's best-match the g's, which are centered
 *   on the given nominal position with given field of view.
 * if find a match with a maximum residual <= $rp, update fp and return 0,
 *   else leave fip unchanged and return -1.
 */
int
findRegistration (fip, ra0, dec0, rot0, psx0, psy0, sx, sy, ns, gr, gd, ng, rp)
FImage *fip;		/* image header to modify IFF we find a match */
double ra0, dec0;	/* initial center position */
double rot0;		/* initial rotation */
double psx0, psy0;	/* initial pixel scales, rads/pixel right and down */
double sx[], sy[];	/* test stars, image locations, pixels */
int ns;			/* number of entries in sx[] and sy[] */
double gr[], gd[];	/* reference stars, ra/dec, rads */
int ng;			/* number of entries in gr[] and gd[] */
double *rp;		/* IN: max acceptable residual OUT: actual residual */
{
#define	VIDX(s,g)	((s)*ng + (g))	/* vote array index from star,GSC */
#define	SIDX(i)		((i)/ng)	/* star index from vote array index */
#define	GIDX(i)		((i)%ng)	/* GSC index from vote array index */
	double smx[MAXPAIR], smy[MAXPAIR];	/* list of matched s stars */
	double gmr[MAXPAIR], gmd[MAXPAIR];	/* list of matched g stars */
	double resid[MAXPAIR];	/* residual for each pair */
	double *gx, *gy;	/* malloced nominal x/y of gr/gd */
	Triangle *stri;		/* malloced list of all s triangles */
	Triangle *gtri;		/* malloced list of all g triangles */
	double t_ra, t_dc;	/* trial values of image center */
	double t_th, t_sx, t_sy;/* trial values of rotation and pixel scales */
	int nstri, ngtri;	/* number of s and g triangles */
	int *votes;		/* malloced votes for common coords */
	int **topvotes;		/* ptrs into votes for the highest entries */
	int npair;		/* final number of star pairs */
	double rmed = 0;	/* median residual */
	int ok = 1;		/* success */
	int i, j;
	int n;

	/* init trial values */
	t_ra = ra0;
	t_dc = dec0;
	t_th = rot0;
	t_sx = psx0;
	t_sy = psy0;

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

	/* generate the triangle lists */
	stri = (Triangle *) malloc ((ns*(ns-1)*(ns-2)/6+1) * sizeof(Triangle));
	nstri = gentri (stri, sx, sy, ns);
	gtri = (Triangle *) malloc ((ng*(ng-1)*(ng-2)/6+1) * sizeof(Triangle));
	ngtri = gentri (gtri, gx, gy, ng);

	/* make a voting array.
	 * assign rows to s stars, columns to g stars 
	 */
	votes = (int *) calloc (ns*ng, sizeof(int));

	/* scan for matching triangles and vote for each of its vertices */
	for (i = 0; i < nstri; i++) {
	    Triangle *ts = &stri[i];
	    for (j = 0; j < ngtri; j++) {
		Triangle *tg = &gtri[j];
		double d = fabs(ts->x - tg->x) + fabs(ts->y - tg->y);

		if (d <= CLOSETRI) {
		    votes[VIDX(ts->i,tg->i)]++;
		    votes[VIDX(ts->j,tg->j)]++;
		    votes[VIDX(ts->k,tg->k)]++;

#ifdef TRIANGLE_TRACE

		    printf ("T:s:[%3.0f,%3.0f:%3.0f,%3.0f:%3.0f,%3.0f;%5.3f,%5.3f]g:[%3.0f,%3.0f:%3.0f,%3.0f:%3.0f,%3.0f;%5.3f,%5.3f]\n",
			    sx[ts->i],sy[ts->i], sx[ts->j],sy[ts->j],
			    sx[ts->k],sy[ts->k], ts->x, ts->y,
			    gx[tg->i],gy[tg->i], gx[tg->j],gy[tg->j],
			    gx[tg->k],gy[tg->k], tg->x, tg->y);
#endif

		}
	    }
	}

#ifdef VOTEARRAY_TRACE
	printf ("VOTEARRAY:\ns/g ");
	for (j = 0; j < ng; j++)
	    printf (" %2d", j);
	printf ("\n");
	for (i = 0; i < ns; i++) {
	    printf ("%2d: ", i);
	    for (j = 0; j < ng; j++)
		printf (" %2d", votes[VIDX(i,j)]);
	    printf ("\n");
	}
#endif

	/* finished with the triangles */
	free ((void *)gtri);
	free ((void *)stri);

	/* scan for the stars with the most votes and put addresses of votes[]
	 * in topvotes in descending order. never add one with < MINVOTES.
	 */
	n = ns < ng ? ns : ng;
	topvotes = (int **) malloc (n * sizeof(int *));
	npair = 0;
	for (i = 0; i < ns; i++) {
	    for (j = 0; j < ng; j++) {
		int v = votes[VIDX(i,j)];
		int k;

		for (k = npair; --k >= 0 && v > *topvotes[k]; )
		    if (k < n-1)
			topvotes[k+1] = topvotes[k];
		if (++k < n && v >= MINVOTES) {
		    topvotes[k] = &votes[VIDX(i,j)];
		    if (npair < n)
			npair++;
		}
	    }
	}

	/* use just the cream of the crop.
	 * N.B. this also allows the temp arrays to be sized at MAXPAIR.
	 */
	if (npair > MAXPAIR)
	    npair = MAXPAIR;

	/* put surviving coords into the candidate pair arrays */
#ifdef TOPVOTES_TRACE
	printf ("TOPVOTES_TRACE: %d pairs\n", npair);
#endif
	n = npair;
	npair = 0;
	for (i = 0; i < n; i++) {
	    int sidx = SIDX(topvotes[i] - votes);
	    int gidx = GIDX(topvotes[i] - votes);

#ifdef TOPVOTES_TRACE
	    {
	    char ras[32], decs[32];
	    fs_sexa (ras, radhr(gr[gidx]), 2, 36000);
	    fs_sexa (decs, raddeg(gd[gidx]), 3, 3600);
	    printf ("%2d: [%2d,%2d]: s:[%4.0f,%4.0f] g:[%s,%s]=[%4.0f,%4.0f] dx,y=[%4.0f,%4.0f]\n",
			    *topvotes[i], sidx, gidx, sx[sidx], sy[sidx],
			    ras, decs, gx[gidx], gy[gidx],
			    sx[sidx]-gx[gidx], sy[sidx]-gy[gidx]);
	    }
#endif

	    smx[i] = sx[sidx];
	    smy[i] = sy[sidx];
	    gmr[i] = gr[gidx];
	    gmd[i] = gd[gidx];
	    npair++;
	}

	if (npair < MINPAIR) {
	    ok = 0;
	    goto out;
	}

	/* provide global access to the star pair lists for chisqr() model */
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
	    printf("ra=%7.4f dc=%8.4f rot=%9.4f sx=%8.4f sy=%8.4f -> rmax=%9.4f\n",
		    radhr(t_ra), raddeg(t_dc), raddeg(t_th), 3600*raddeg(t_sx),
		    3600*raddeg(t_sy), resid_max);
#endif
	    
	    dmedian (resid_g, npair, &rmed);
	    threshresid = THRESH*rmed;
	    if (threshresid < *rp)
		threshresid = *rp;

#ifdef THRESH_TRACE
	    printf ("THRESH_TRACE:\n");
	    printf ("npair=%3d rmed=%8.3f thresh=%6.3f\n", npair, rmed,
								threshresid);
	    for (i = 0; i < npair; i++) {
		char ras[32], decs[32];
		fs_sexa (ras, radhr(gmr[i]), 2, 360000);
		fs_sexa (decs, raddeg(gmd[i]), 3, 36000);
		printf (" %c%3d: s:[%4.0f,%4.0f] g:[%s,%s] %6.3f\n",
					resid_g[i] < threshresid ? ' ' : '*',
				    i, smx[i], smy[i], ras, decs, resid_g[i]);
	    }
#endif
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

#ifdef PRMAXRES_TRACE
	printf ("PRMAXRES_TRACE: %8.2f with %2d pairs @ %6.2f %6.2f\n",
				resid_max, npair, radhr(ra0), raddeg(dec0));
#endif

	/* got a solution if ok so far, within goal and reasonable scaling */
	if (ok && rmed < *rp  && fabs(t_sx) < 2*fabs(psx0)
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

#define GNUPLOT_TRACE
#ifdef GNUPLOT_TRACE
	    {
	    /* plot all stars and mark those used for the final fit.
	     * N.B. gnuplot wants +y upward, FITS uses +y downward
	     */

	    double xem[MAXPAIR], yem[MAXPAIR];
	    double mxerr, myerr;
	    FILE *fp;

	    /* find final catalog star positions */
	    for (i = 0; i < ng; i++)
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
	    for (i = 0; i < ng; i++)
		fprintf (fp, "%7.1f %7.1f\n", gx[i], gy[i]);
	    if (fp != stdout) fclose (fp);

	    /* fit stars */
	    fp = fopen ("/tmp/wcs.fit", "w");
	    if (!fp) fp = stdout;
	    for (i = 0; i < npair; i++) {
		int sidx = SIDX(topvotes[i] - votes);
		int gidx = GIDX(topvotes[i] - votes);
		xem[i] = fabs(sx[sidx]-gx[gidx]);
		yem[i] = fabs(sy[sidx]-gy[gidx]);
		fprintf (fp, "%g %g %g %g\n", sx[sidx], sy[sidx], 50*xem[i],
								    50*yem[i]);
	    }
	    if (fp != stdout) fclose (fp);
	    dmedian (xem, npair, &mxerr);
	    dmedian (yem, npair, &myerr);

	    /* file of gnuplot commands to display nicely */
	    fp = fopen ("/tmp/wcs.gnp", "w");
	    if (!fp) fp = stdout;
	    fprintf (fp, "# 'load' this file into gnuplot to show stars in fit\n");
	    fprintf (fp, "set pointsize 3\n");
	    fprintf (fp, "set grid\n");
	    fprintf (fp, "set key below reverse\n");
	    fprintf (fp, "set xrange [0:%d]\n", fip->sw);
	    fprintf (fp, "set yrange [%d:0]\n", fip->sh);
	    fprintf (fp, "set xlabel 'x'\n");
	    fprintf (fp, "set ylabel 'y'\n");
	    fprintf (fp, "set title \"WCS Fit Map\\nMedian xerr %.2f = %.2f'', yerr %.2f = %.2f''; drawn @ scale/100\"\n",
			mxerr, mxerr*3600*fabs(raddeg(t_sx)),
			myerr, myerr*3600*fabs(raddeg(t_sy)));
	    fprintf (fp,
	"plot '/tmp/wcs.s' ti '%d Image stars', '/tmp/wcs.c' ti '%d Catalog stars', '/tmp/wcs.fit' ti '%d used in fit' with xyerrorbars ps 0\n", ns, ng, npair);
	    fprintf (fp, "pause -1\n");
	    if (fp != stdout) fclose (fp);
	    }
#endif
	} else
	    ok = 0;

	/* finished with the voting and other temp arrays */
	free ((void *)votes);
	free ((void *)topvotes);
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

/* given n x/y pairs, generate the (n,3) = n!/(n-3)!/3! = n*(n-1)*(n-2)/6
 *   possible triangles.
 * sort each Triangle's vertices such that a>b>c and
 *   i is the vertex between the shortest two edges (c and b),
 *   j is the vertex between the longest and the shortest edge (a and c), and
 *   k is the vertex between the longest two edges (a and b).
 * only include those with a < MINFAT(b+c)
 * return count.
 */
static int
gentri (Triangle *tri, double x[], double y[], int n)
{
#define	EIDX(i,j)	((i)*n + (j))
	float *edges;
	int i, j, k;
	int ntri;

	/* compute all edges once, for efficiency */
	edges = (float *) calloc (n*n, sizeof(float));
	for (i = 0; i < n; i++)
	    for (j = 0; j < n; j++) {
		if (i < j) {
		    double dx = x[i] - x[j];
		    double dy = y[i] - y[j];
		    edges[EIDX(i,j)] = sqrt (dx*dx + dy*dy);
		} else if (i > j)
		    edges[EIDX(i,j)] = edges[EIDX(j,i)];
		else
		    edges[EIDX(i,j)] = 0.0;
#ifdef EDGE_TRACE
                printf ("EDGE: l=%10.5f i=%2d:[%10.5f,%10.5f] j=%2d:[%10.5f,%10.5f]\n",
			    edges[EIDX(i,j)], i, x[i], y[i], j, x[j], y[j]);
#endif
	    }

	/* find the triangles */
	ntri = 0;
	for (i = 0; i < n; i++) {
	    for (j = i+1; j < n; j++) {
		for (k = j+1; k < n; k++) {
		    int si, sj, sk;
		    float a, b, c; /* edges | a>b>c */
		    Triangle *tp;
		    float tf;

		    /* find the three edge lengths and their vertices */
		    a = edges[EIDX(j,k)];
		    b = edges[EIDX(i,k)];
		    c = edges[EIDX(i,j)];
		    si = i;
		    sj = j;
		    sk = k;

		    /* use only "large" triangles */
		    if (a < MINLEG || b < MINLEG || c < MINLEG)
			continue;

		    /* sort so a>b>c */
		    /* first get a>b */
		    if (b > a) {
			int t = si; si = sj; sj = t;
			tf = a; a = b; b = tf;
		    }
		    /* then see where c fits in */
		    if (c > a) {
			int t = si; si = sk; sk = sj; sj = t;
			tf = c; c = b; b = a; a = tf;
		    } else if (c > b) {
			int t = sj; sj = sk; sk = t;
			tf = c; c = b; b = tf;
		    }

		    /* use only "fat" triangles */
		    if (a > MINFAT*(b+c))
			continue;

		    /* compute location in triangle space */
		    tp = &tri[ntri];
		    tp->x = b/a;
		    tp->y = c/a;
		    ntri++;

		    /* store the vertices in sorted order */
		    tp->i = si;
		    tp->j = sj;
		    tp->k = sk;

		    /* check */
		    a = edges[EIDX(sj,sk)];
		    b = edges[EIDX(si,sk)];
		    c = edges[EIDX(si,sj)];
		    if (!(a>=b && b>=c)) {
			printf ("Bug! Vertices not sorted\n");
			exit (1);
		    }

#ifdef VERTEX_TRACE
		    printf ("V: %6.1f > %6.1f > %6.1f\n", a, b, c);
#endif
		}
	    }
	}

	/* may be fewer than max possible due to MINFAT culling */
	if (ntri > n*(n-1)*(n-2)/6) {
	    fprintf (stderr, "genTri assertion failed: n=%d ntri=%d\n",n,ntri);
	    exit(1);
	}

	free ((void *)edges);

	return (ntri);
	    
#undef EIDX
}

/* compute the chisqr of the vector v with respect to fim_g.
 * update resid/_max/_sum/_sum2.
 */
static double
chisqr(double v[5])
{
	FImage *fip = &fim_g;
	double mx[MAXPAIR], my[MAXPAIR];
	double ra = v[0];
	double dc = v[1];
	double th = v[2];
	double sx = v[3];
	double sy = v[4];
	double c2;
	int i;

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

#ifdef CHSQR_TRACE
	printf("ra=%7.4f dc=%8.4f rot=%9.4f sx=%8.4f sy=%8.4f -> rmax=%9.4f\n",
			    radhr(ra), raddeg(dc), raddeg(th), 3600*raddeg(sx),
			    3600*raddeg(sy), resid_max);
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

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: findreg.c,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $"};
