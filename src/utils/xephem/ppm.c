/* fetch the desired region of ppm (or similarly-formatted catalog) stars.
 * do simple linear position correction for now.
 */

#include <stdio.h>
#include <math.h>

#if defined(__STDC__)
#include <stdlib.h>
#include <string.h>
#else
extern void *malloc(), *realloc();
#endif

#ifndef SEEK_SET
#define	SEEK_SET 0
#define	SEEK_END 2
#endif

#include "P_.h"
#include "astro.h"
#include "circum.h"

extern FILE *fopenh P_((char *name, char *how));
extern Now *mm_get_now P_((void));
extern char *syserrstr P_((void));
extern void pm_set P_((int percentage));
extern void zero_mem P_((void *loc, unsigned len));

#define	PPMPKTSZ	19		/* packet size - DO NOT CHANGE */

#define	MAXFOV		degrad(30.0)	/* max FOV we will return */

typedef unsigned char UC;
typedef unsigned long UL;
typedef UC PKT[PPMPKTSZ];


/* an array of ObjF which can be grown efficiently in mults of NINC */
typedef struct {
    ObjF *mem;          /* malloced array */
    int max;            /* max number of cells in mem[] or -1 to just count */
    int used;           /* number actually in use */
} ObjFArray;

#define NINC    32      /* grow mem mem this many at a time */

static FILE *ppmopen P_((char *file, char msg[]));
static void unpackObj P_((PKT pkt, Now *np, int saohd, Obj *op));
static int addOneObjF P_((ObjFArray *ap, ObjF *fop));
static void unpack P_((PKT buf, double *ra, double *dec, double *pma,
    double *pmd, double *mag, UL *ppm, UL *sao, UL *hd, char *spect,
    char *class, int *type));

/* return 0 if file looks like a ppm, else -1
 */
int
ppmchkfile (file, msg)
char *file;
char *msg;
{
	Now *np = mm_get_now();
	double prevdec = -PI / 2;
	int ok = 1;
	PKT buf;
	FILE *fp;
	int n, max;
	Obj o;

	fp = ppmopen (file, msg);
	if (!fp)
	    return (-1);

	/* do some basic checks to gain confidence in the file.
	 * N.B. VMS pads binary files to next block size!
	 */
	fseek (fp, 0, SEEK_END);
	max = ftell(fp) / PPMPKTSZ - 100;
	for(n = 0; n < max; n += max / 5) {
	    fseek (fp, n*PPMPKTSZ, SEEK_SET);
	    if (fread (buf, 1, sizeof(buf), fp) != sizeof(buf)) {
		ok = 0;
		break;
	    }
	    unpackObj (buf, np, 0, &o);
	    if (o.f_dec < prevdec) {
		ok = 0;
		break;
	    }
	    prevdec = o.f_dec;
	}
	if (!ok) {
	    (void) sprintf (msg, "%s: not a PPM-format catalog", file);
	    fclose (fp);
	    return (-1);
	}

	fclose (fp);
	return (0);
}

/* malloc a collection of PPM stars around the given location at *opp.
 * if trouble fill msg[] and return -1, else return count.
 * N.B. memory is only actually malloced at *opp if we return > 0.
 * N.B. we very much rely on file being sorted by increasing dec.
 * N.B. we silently limit the fov to MAXFOV.
 */
int
ppmfetch (file, np, ra, dec, fov, mag, saohd, opp, msg)
char *file;
Now *np;
double ra, dec;
double fov, mag;
int saohd;
ObjF **opp;
char *msg;
{
	double rov;
	double cdec;
	double sdec;
	double crov;
	ObjFArray ofa;
	FILE *fp;
	long l, u, m;
	double ldec, udec;
	Obj o;
	PKT pkt;

	/* initial setup */
	if (fov > MAXFOV)
	    fov = MAXFOV;
	rov = fov/2;
	cdec = cos(dec);
	sdec = sin(dec);
	crov = cos(rov);

	/* bug if not given a place to save new stars */
	if (!opp) {
	    printf ("ppmfetch with opp == NULL\n");
	    exit(1);
	}

	/* open the PPM file */
	fp = ppmopen (file, msg);
	if (!fp)
	    return (-1);

	/* binary search for star nearest lower dec */
	ldec = dec - rov;
	if (ldec < -PI/2)
	    ldec = -PI/2;
	l = 0L;

	/* Determine the true end of the file, in case reported file size
	 * > than the true size (VMS?):  find last entry with nonzero decl.
	 */
	fseek (fp, 0, SEEK_END);
	for(u = ftell(fp) / PPMPKTSZ - 1; ; u--) {
	    if (fseek (fp, u*PPMPKTSZ, SEEK_SET) < 0
			|| fread(pkt, PPMPKTSZ, 1, fp) != 1)
		break;
	    unpackObj (pkt, np, saohd, &o);
	    if(o.f_dec != 0)
		break;
	}
	    
	while (l <= u) {
	    m = (l+u)/2;
	    if (fseek (fp, m*PPMPKTSZ, SEEK_SET) < 0) {
		(void) sprintf (msg, "%s: seek error: %s", file, syserrstr());
		fclose (fp);
		return (-1);
	    }
	    if (fread (pkt, PPMPKTSZ, 1, fp) != 1) {
		(void) sprintf (msg, "%s: read error: %s", file, syserrstr());
		fclose (fp);
		return (-1);
	    }
	    unpackObj (pkt, np, saohd, &o);
	    if (ldec < o.f_dec)
		u = m-1;
	    else
		l = m+1;
	}

	/* add each entry from m up to upper dec */
	udec = dec + rov;
	if (udec > PI/2)
	    udec = PI/2;
	ofa.mem = NULL;
	ofa.max = 0;
	ofa.used = 0;
	while (o.f_dec <= udec) {
	    pm_set ((int)((o.f_dec - ldec)/(udec - ldec)*100));

	    if (sin(o.f_dec)*sdec + cos(o.f_dec)*cdec*cos(ra-o.f_RA) >= crov) {
		if (addOneObjF (&ofa, (ObjF *)&o) < 0) {
		    (void) sprintf (msg, "no more memory");
		    fclose (fp);
		    if (ofa.mem)
			free ((void *)ofa.mem);
		    return (-1);
		}
	    }

	    if (fread (pkt, PPMPKTSZ, 1, fp) != 1) {
		if (feof(fp))
		    break;
		(void) sprintf (msg, "%s: %s", file, syserrstr());
		fclose (fp);
		if (ofa.mem)
		    free ((void *)ofa.mem);
		return (-1);
	    }

	    unpackObj (pkt, np, saohd, &o);
	}

	fclose (fp);
	if (ofa.mem)
	    *opp = ofa.mem;
	return (ofa.used);
}

/* open the ppm file.
 * return the FILE * if ok, else NULL with a reason in msg[].
 */
static FILE *
ppmopen (file, msg)
char *file;
char msg[];
{
	FILE *fp = fopenh (file, "rb");
	if (!fp) {
	    (void) sprintf (msg, "%s: %s", file, syserrstr());
	    return (NULL);
	}
	return (fp);
}

static void
unpackObj (pkt, np, saohd, op)
PKT pkt;
Now *np;
int saohd;
Obj *op;
{
	double ra;
	double dec;
	double pma;
	double pmd;
	double mag;
	UL ppm;
	UL sao;
	UL hd;
	char spect;
	char class;
	int type;

	unpack (pkt, &ra, &dec, &pma, &pmd, &mag, &ppm, &sao, &hd, &spect,
								&class, &type);

	zero_mem ((void *)op, sizeof (*op));

	if (saohd && hd != 0L)
	    (void) sprintf (op->o_name, "HD %ld", hd);
	else if (saohd && sao != 0L)
	    (void) sprintf (op->o_name, "SAO %ld", sao);
	else if (ppm != 0L)
	    (void) sprintf (op->o_name, "PPM %ld", ppm);
	else
	    (void) strcpy (op->o_name, "Tycho"); /* for now anyway :-) */

	op->o_type = FIXED;
	op->f_class = type == 2 ? 'D' : type == 1 ? 'T' : 'S';
	op->f_spect[0] = spect == ' ' ? '\0' : spect;
	op->f_spect[1] = class == ' ' ? '\0' : class;
	op->f_RA = (float)(ra + hrrad(pma/3600.0)*(mjd - J2000)/365.24);
	op->f_dec = (float)(dec + degrad(pmd/3600.0)*(mjd - J2000)/365.24);
	op->f_epoch = (float)J2000;
	set_fmag (op, mag);
}

/* add one ObjF entry to ap[], growing if necessary.
 * return 0 if ok else return -1
 */
static int
addOneObjF (ap, fop)
ObjFArray *ap;
ObjF *fop;
{
	ObjF *newf;

	if (ap->used >= ap->max) {
	    /* add room for NINC more */
	    char *newmem = ap->mem ? realloc ((void *)ap->mem,
						(ap->max+NINC)*sizeof(ObjF))
				   : malloc (NINC*sizeof(ObjF));
	    if (!newmem)
		return (-1);
	    ap->mem = (ObjF *)newmem;
	    ap->max += NINC;
	}

	newf = &ap->mem[ap->used++];
	(void) memcpy ((void *)newf, (void *)fop, sizeof(ObjF));

	return (0);
}

static void
unpack (buf, ra, dec, pma, pmd, mag, ppm, sao, hd, spect, class, type)
PKT buf;
double *ra;
double *dec;
double *pma;
double *pmd;
double *mag;
UL *ppm;
UL *sao;
UL *hd;
char *spect;
char *class;
int *type;
{
	UL t;

 	/* 0-2 PPM number; byte[0] & 0xc0 is type: 0=star 1=star-like 2=double*/
	t = buf[0];
	*type = t >> 6;
	t = ((UL)buf[0] << 16) | ((UL)buf[1] << 8) | (UL)buf[2];
	*ppm = t & 0x3fffff;

	/* 3-5 SAO (if byte[3]&0x80) or HD number; 0 for undefined */
	t = ((UL)buf[3] << 16) | ((UL)buf[4] << 8) | (UL)buf[5];
	if (t & 0x800000) {
	    *sao = t & 0x7fffff;
	    *hd = 0L;
	} else {
	    *sao = 0L;
	    *hd = t;
	}

	/* 6-8	RA, rads, 0 .. 2*PI mapped to 0 .. 1<<24 */
	t = ((UL)buf[6] << 16) | ((UL)buf[7] << 8) | (UL)buf[8];
	*ra = t*(2*PI)/(1L<<24);

	/* 9-11	Dec, rads, -PI/2 .. PI/2 mapped to 0 .. (1<<24)-1 */
	t = ((UL)buf[9] << 16) | ((UL)buf[10] << 8) | (UL)buf[11];
	*dec = t*PI/((1L<<24)-1) - PI/2;

	/* 12	Magnitude, -2 .. 14 mapped as (Mag+2)*10 */
	t = buf[12];
	*mag = t/10.0 - 2.0;

	/* 13-14 Spectrum characters */
	*spect = (char)buf[13];
	*class = (char)buf[14];

	/* 15-16 PM RA, coded as (PMA*10000)+5000 */
	t = ((UL)buf[15] << 8) | (UL)buf[16];
	*pma = (t - 5000.0)/10000.0;

	/* 17-18 PM Dec, coded as (PMD*1000)+10000 */
	t = ((UL)buf[17] << 8) | (UL)buf[18];
	*pmd = (t - 10000.0)/1000.0;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: ppm.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
