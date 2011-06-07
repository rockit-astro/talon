/* look up which star atlas an ra/dec is in.
 * contributed by Atsuo Ohki <ohki@gssm.otsuka.tsukuba.ac.jp>
 */
 
#include <stdio.h>

#include "P_.h"
#include "astro.h"

/* for Millennium Star Atlas */
static int msa_charts[] = {
    /* 90*/  2, /* 84*/  4, /* 78*/  8, /* 72*/ 10, /* 66*/ 12,
    /* 60*/ 14, /* 54*/ 16, /* 48*/ 20, /* 42*/ 20, /* 36*/ 22,
    /* 30*/ 22, /* 24*/ 24, /* 18*/ 24, /* 12*/ 24, /*  6*/ 24,
    /*  0*/ 24,
    /* -6*/ 24, /*-12*/ 24, /*-18*/ 24, /*-24*/ 24, /*-30*/ 22,
    /*-36*/ 22, /*-42*/ 20, /*-48*/ 20, /*-54*/ 16, /*-60*/ 14,
    /*-66*/ 12, /*-72*/ 10, /*-78*/  8, /*-84*/  4, /*-90*/  2
};

/*
 * find the chart number of Millennium Star Atlas and return pointer to static
 * string describing location.
 * 0 <= ra < 24;  -90 <= dec <= 90
 */
char *
msa_atlas(ra, dec)
double ra, dec;
{
	static char buf[512];
	int zone, band;
	int i, p;

	ra = radhr(ra);
	dec = raddeg(dec);
	buf[0] = 0;
	if (ra < 0.0 || 24.0 <= ra || dec < -90.0 || 90.0 < dec)
	    return (buf);
	zone = (int)(ra/8.0);
	band = -((int)(dec+((dec>=0)?3:-3))/6 - 15);
	for (p=0, i=0; i <= band; i++)
	    p += msa_charts[i];
	i = (int)((ra - 8.0*zone) / (8.0/msa_charts[band]));
	sprintf(buf, "V%d - P%3d", zone+1, p-i+zone*516);
	return (buf);
}

/* for Uranometria 2000.0 */
static struct {
    double l;
    int n;
} um_zones[] = {
    /* 84 - 90 */ { 84.5,  2},
    /* 72 - 85 */ { 72.5, 12},
    /* 60 - 73 */ { 61.0, 20},
    /* 49 - 62 */ { 50.0, 24},
    /* 38 - 51 */ { 39.0, 30},
    /* 27 - 40 */ { 28.0, 36},
    /* 16 - 29 */ { 17.0, 45},
    /*  5 - 18 */ {  5.5, 45},
    /*  0 -  6 */ {  0.0, 45},
		  {  0.0,  0}
};

/*
 * find the chart number of Uranometria 2000.0 and return pointer to static
 * string describing location.
 * 0 <= ra < 24;  -90 <= dec <= 90
 */
char *
um_atlas(ra, dec)
double ra, dec;
{
	static char buf[512];
	int band, south;
	int p;
	double w;

	ra = radhr(ra);
	dec = raddeg(dec);
	buf[0] = 0;
	if (ra < 0.0 || 24.0 <= ra || dec < -90.0 || 90.0 < dec)
	    return (buf);
	p = 0;
	if (dec < 0.0) {
	    dec = -dec;
	    south = 1;
	} else
	    south = 0;
	p = 1;
	for (band=0; um_zones[band].n; band++) {
	    if (um_zones[band].l <= dec)
		break; 
	    p += um_zones[band].n;
	}
	if (!um_zones[band].n)
	    return (buf);
	w = 24.0 / um_zones[band].n;
	if (band) {
	    ra += w/2.0;
	    if (ra >= 24.0)
		ra -= 24.0;
	}
	if (south && um_zones[band+1].n)
	    p = 475 - p - um_zones[band].n;
	if (south && band == 0) {
	    /* south pole part is mis-ordered! */
	    ra = 24.0 - ra;
	}
	sprintf(buf, "V%d - P%3d", south+1, p+(int)(ra/w));
	return (buf);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: atlas.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
