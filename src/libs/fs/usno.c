/* USNOSetup(): call to change options and base directories.
 * USNOFetch(): return an array of FieldStars matching the given criteria.
 * based on sample code in demo.tar on SA1.0 CDROM and info in read.use.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "strops.h"
#include "fieldstar.h"

/* 
 * 1.1  2/23/98 Switch to FieldStar
 * 1.0  2/15/98 Works great.
 * 0.1	2/11/98	begin work
 */


#define	CATBPR	12	/* bytes per star record in .cat file */
#define	ACCBPR	30	/* bytes per record in .acc file */

typedef unsigned int UI;
typedef unsigned char UC;

/* an array of FieldStar which can be grown efficiently in mults of NINC */
typedef struct {
    FieldStar *mem;	/* malloced array */
    int used;		/* number actually in use */
    int max;		/* cells in mem[] */
} StarArray;

#define	NINC	16	/* grow ObjFArray array this many at a time */

static int corner (double r0, double d0, double rov, int *nr, double fr[2],
    double lr[2], int *nd, double fd[2], double ld[2], int zone[2], char msg[]);
static int fetchSwath (int zone, double maxmag, double fr, double lr,
    double fd, double ld, StarArray *stara, char msg[]);
static int crackCatBuf (UC buf[CATBPR], FieldStar *sp);
static int addGS (StarArray *stara, FieldStar *sp);

static char *cdpath;		/* where CD rom is mounted */
static int nogsc;		/* set to 1 to exclude GSC stars */

/* save the path to the base of the cdrom.
 * test for some reasonable entries.
 * return 0 if looks ok, else -1 and reason in msg[].
 */
int
USNOSetup (char *cdp, int wantgsc, char msg[])
{
	char tstname[1024];
	FILE *fp;

	/* try a typical name */
	sprintf (tstname, "%s/zone1275.acc", cdp);
	fp = fopen (tstname, "r");
	if (fp == NULL) {
	    sprintf (msg, "%s: %s", tstname, strerror(errno));
	    return (-1);
	}
	fclose (fp);

	/* store our own copy */
	if (cdpath)
	    free (cdpath);
	cdpath = malloc (strlen(cdp) + 1);
	strcpy (cdpath, cdp);

	/* store GSC flag */
	nogsc = !wantgsc;

	/* probably ok */
	return (0);

}

/* build a malloced array of FieldStars at *spp and return count.
 * N.B. caller must free *spp iff return count is > 0.
 * if trouble fill msg[] with a short diagnostic and return -1.
 */
int
USNOFetch (
double r0,	/* center RA, rads */
double d0,	/* center Dec, rads */
double fov,	/* field of view, rads */
double fmag,	/* faintest mag */
FieldStar **spp,/* *spp will be a malloced array of FieldStar in region */
char msg[])	/* filled with error message if return -1 */
{
	double fr[2], lr[2];	/* first and last ra in each region, up to 2 */
	double fd[2], ld[2];	/* first and last dec in each region, up to 2 */
	int nr, nd;		/* number of ra and dec regions, max 2 each */
	int zone[2];		/* zone for filename, up to 2 */
	double rov;		/* radius of view, degrees */
	StarArray stara;	/* malloc accumulator */
	int i, j, s;

	/* insure there is a cdpath set up */
	if (!cdpath) {
	    strcpy (msg, "USNOFetch() called before USNOSetup()");
	    return (-1);
	}

	/* convert to cdrom units */
	r0 = raddeg(r0);
	d0 = raddeg(d0);
	rov = raddeg (fov)/2;
	if (rov >= 7.5) {
	    strcpy (msg, "Radius must be less than 7.5 degrees");
	    return (-1);
	}

	/* find the files to use and ranges in each */
	i = corner (r0, d0, rov, &nr, fr, lr, &nd, fd, ld, zone, msg);
	if (i < 0)
	    return (-1);

	/* init the memory array */
	stara.mem = NULL;
	stara.used = 0;
	stara.max = 0;

	/* fetch each chunk, adding to stara */
	for (i = 0; i < nd; i++) {
	    for (j = 0; j < nr; j++) {
		s = fetchSwath(zone[i],fmag,fr[j],lr[j],fd[i],ld[i],&stara,msg);
		if (s < 0) {
		    if (stara.mem)
			free ((void *)stara.mem);
		    return (-1);
		}
	    }
	}

	/* caller told not to free if return 0 */
	if (stara.used == 0 && stara.mem != NULL)
	    free ((void *)stara.mem);

	/* ok */
	*spp = stara.mem;
	return (stara.used);
}

static int
corner (double r0, double d0, double rov, int *nr, double fr[2], double lr[2],
int *nd, double fd[2], double ld[2], int zone[2], char msg[])
{
	double cd = cos(degrad(d0));
	double x1, x2;
	int z1, z2;
	int z, j;

	/* find limits on ra, taking care if span 24h */
	x1 = r0 - rov/cd;
	x2 = r0 + rov/cd;
	if (x1 < 0.0) {
	    *nr = 2;
	    fr[0] = 0.0;
	    lr[0] = x2;
	    fr[1] = 360.0 + x1;
	    lr[1] = 360.0;
	} else if (x2 >= 360.0) {
	    *nr = 2;
	    fr[0] = 0.0;
	    lr[0] = x2 - 360.0;
	    fr[1] = x1;
	    lr[1] = 360.0;
	} else {
	    *nr = 1;
	    fr[0] = x1;
	    lr[0] = x2;
	}

	/* find dec limits and zones */
	x1 = d0 - rov;
	x2 = d0 + rov;
	z1 = (int)floor((x1 + 90.0)/7.5);
	z2 = (int)floor((x2 + 90.0)/7.5);
	*nd = z2 - z1 + 1;
	if (*nd > 2) {
	    *nd = 2;
	    z2 = z1 + 1;
	    /*
	    sprintf (msg, "No! ndec = %d", *nd);
	    return (-1);
	    */
	}
	j = 0;
	for (z = z1; z <= z2; z++) {
	    double dmin = z*7.5 - 90.0;
	    double dmax = dmin+7.5;
	    zone[j] = z*75;
	    fd[j] = x1 > dmin ? x1 : dmin;
	    ld[j] = x2 < dmax ? x2 : dmax;
	    j++;
	}

	return (0);
}

static int
fetchSwath (int zone, double maxmag, double fr, double lr, double fd,
double ld, StarArray *stara, char msg[])
{
	char fn[1024], *bfn;
	char buf[ACCBPR];
	UC catbuf[CATBPR];
	FieldStar fs;
	long frec;
	off_t os;
	FILE *fp;

#ifdef TRACE_FETCH
	fprintf (stderr, "Zone %4d RA:%6.2f..%6.2f Dec:%6.2f..%6.2f\n",
							zone, fr, lr, fd, ld);
#endif

	/* read access file for position in catalog file */
	sprintf (fn, "%s/zone%04d.acc", cdpath, zone);
	bfn = basenm(fn);
	fp = fopen (fn, "r");
	if (fp == NULL) {
	    sprintf (msg, "%s: %s", bfn, strerror(errno));
	    return (-1);
	}
	os = ACCBPR*(off_t)floor(fr/3.75);
	if (fseek (fp, os, SEEK_SET) < 0) {
	    sprintf (msg, "%s: fseek(%ld): %s", bfn, (long)os, strerror(errno));
	    fclose (fp);
	    return (-1);
	}
	if (!fread (buf, ACCBPR, 1, fp)) {
	    if (ferror(fp))
		sprintf (msg, "%s: fread(@%ld): %s", bfn, (long)os,
							strerror(errno));
	    else
		sprintf (msg, "%s: unexpected EOF @ %ld", bfn, (long)os);
	    fclose (fp);
	    return (-1);
	}
	fclose (fp);
	if (sscanf (buf, "%*f %ld", &frec) != 1) {
	    sprintf (msg, "%s: sscanf(%s)", bfn, buf);
	    return (-1);
	}

#ifdef TRACE_FETCH
	fprintf (stderr, "    frec=%6ld\n", frec);
#endif

	/* open and position the catalog file */
	sprintf (fn, "%s/zone%04d.cat", cdpath, zone);
	bfn = basenm(fn);
	fp = fopen (fn, "r");
	if (fp == NULL) {
	    sprintf (msg, "%s: %s", bfn, strerror(errno));
	    return (-1);
	}
	os = (off_t)(frec-1)*CATBPR;
	if (fseek (fp, os, SEEK_SET) < 0) {
	    sprintf (msg, "%s: fseek(%ld): %s", bfn, (long)os, strerror(errno));
	    fclose (fp);
	    return (-1);
	}

	/* now want in rads */
	fr = degrad(fr);
	lr = degrad(lr);
	fd = degrad(fd);
	ld = degrad(ld);

	/* read until find star with RA larger than lr */
	while (fread (catbuf, CATBPR, 1, fp)) {
	    int cstat = crackCatBuf (catbuf, &fs);
	    int fstat = fs.mag<=maxmag && fs.ra>=fr && fs.ra<=lr
			&& fs.dec>=fd && fs.dec<=ld;
	    os += CATBPR;
	    if (cstat==0 && fstat) {
		if (addGS (stara, &fs) < 0) {
		    sprintf (msg, "No more memory");
		    fclose (fp);
		    return (-1);
		}
	    }

	    /* sorted by ra so finished when hit upper limit */
	    if (fs.ra > lr)
		break;
	}

	if (ferror(fp)) {
	    sprintf(msg,"%s: fread(@%ld): %s",bfn,(long)os,strerror(errno));
	    fclose (fp);
	    return (-1);
	}

	/* finished */
	fclose (fp);

	/* ok*/
	return (0);
}

/* crack the star field in buf.
 * return 0 if ok, else -1.
 */
static int
crackCatBuf (UC buf[CATBPR], FieldStar *sp)
{
#define	BEUPACK(b) (((UI)((b)[0])<<24) | ((UI)((b)[1])<<16) | ((UI)((b)[2])<<8)\
							    | ((UI)((b)[3])))
	double ra, dec;
	int red, blu;
	UI mag;

	/* first 4 bytes are packed RA, big-endian */
	ra = BEUPACK(buf)/(100.0*3600.0*15.0);
	sp->ra = hrrad(ra);

	/* next 4 bytes are packed Dec, big-endian */
	dec = BEUPACK(buf+4)/(100.0*3600.0) - 90.0;
	sp->dec = degrad(dec);

	/* last 4 bytes are packed mag info -- can lead to rejection */
	mag = BEUPACK(buf+8);
	if (mag & (1<<31)) {
	    /* negative means corresponding GSC */
	    if (nogsc)
		return (-1);
	    mag &= 0x7fffffff;	/* just chop off sign?? or negate?? */
	}
	if (mag >= 1000000000)
	    return (-1);	/* poor magnitudes */
	red = mag % 1000u;	/* lower 3 digits */
	blu = (mag/1000u)%1000u;/* next 3 digits up */
	if (red > 250) {
	    if (blu > 250)
		return (-1);	/* poor colors */
	    else
		sp->mag = blu/10.0;
	} else
	    sp->mag = red/10.0;

	/* assume it's a star */
	sp->isstar = 1;

	return (0);
}

/* add sp to stara.
 * return -1 if no more memory, else 0.
 */
static int
addGS (StarArray *stara, FieldStar *sp)
{
	/* get next entry, mallocing if fresh out */
	if (stara->used == stara->max) {
	    char *newmem = (char *)stara->mem;

	    stara->max += NINC;
	    newmem = newmem ? realloc (newmem, stara->max*sizeof(ObjF))
			    : malloc (NINC * sizeof(ObjF));
	    if (!newmem)
		return (-1);
	    stara->mem = (FieldStar *)newmem;
	}

	/* make up a name */
	sprintf (sp->name, "SA1.0 %06d", stara->used);

	/* fill and increment count used */
	stara->mem[stara->used++] = *sp;

	/* ok */
	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: usno.c,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $"};
