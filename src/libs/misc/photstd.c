/* code to support the standard photometric Landolt stars.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"

#include "photstd.h"

static int addStar (PStdStar *sfp, char buf[]);

/* read the given file of standard photometric stars, set *spp to malloced
 * array and return count, or -1 if real trouble.
 * N.B. caller should free *spp with photFree() if we return >= 0.
 *
 * file format is one star per line, columns as follows:
 *   Name          rh rm rs   dd dm ds   V      B-V    V-R    V-I
 * no blanks allowed in Name.
 * beware dd can have leading '-' but value otherwise 0.
 * columns separated by blanks or tabs.
 * ignore all lines not conforming to this pattern.
 */
int
photStdRead (FILE *fp, PStdStar **spp)
{
	char buf[1024];
	PStdStar *sp;
	int nsp;

	/* initial malloc so we can just realloc */
	sp = (PStdStar *) malloc (sizeof(PStdStar));
	if (!sp)
	    return (-1);
	nsp = 0;

	/* read all lines until EOF, adding to list as find good stars */
	while (fgets (buf, sizeof(buf), fp)) {
	    if (addStar (&sp[nsp], buf) == 0) {
		char *new = realloc ((void *)sp, (nsp+2)*sizeof(PStdStar));

		if (!new) {
		    free ((void *)sp);
		    return (-1);
		}
		sp = (PStdStar *) new;
		nsp++;
	    }
	}

	*spp = sp;
	return (nsp);
}

/* free up a PStdStar array and all it contains */
void
photFree (PStdStar *sp, int n)
{
	PStdStar *spsave;

	for (spsave = sp; --n >= 0; sp++) {
	    if (sp->Bp)
		free ((void *)sp->Bp);
	    if (sp->Vp)
		free ((void *)sp->Vp);
	    if (sp->Rp)
		free ((void *)sp->Rp);
	    if (sp->Ip)
		free ((void *)sp->Ip);
	}

	if (spsave)
	    free ((void *)spsave);
}

/* search the given photcal.out file for the closest entry for the given jd but
 * never more than +- jdwin. if find, fill in *cp and return 0; else fill in
 * msg[] and return -1.
 *
 * the file format is one or more of the following examples:
 *
 *   2449941.50000 (M/D/Y: 8/12/1995)
 *   Flt V0      V0err   kp     kperr   kpp    kpperr
 *   B:  18.251  0.054  -0.579  0.034   0.330  0.000
 *   V:  18.501  0.064  -0.452  0.042   0.170  0.000
 *   R:  18.387  0.020  -0.312  0.012   0.030  0.000
 *   I:  17.664  0.017  -0.245  0.010   0.020  0.000
 */
int
photReadCalConst (
FILE *fp,
double jd, 
double jdwin,
PCalConst *cp, 
double *jdp,
char *msg)
{
	long best_where;
	double best_jd;
	char buf[1024];

	best_where = -1;
	best_jd = 0;

	/* find the entry closest to jd -- can't assume any presort */
	while (fgets (buf, sizeof(buf), fp)) {
	    double thisjd;

	    if (sscanf (buf, "%lf (M/D/Y", &thisjd) != 1)
		continue;
	    if (best_where < 0 || fabs(thisjd - jd) < fabs (thisjd - best_jd)){
		best_where = ftell(fp);
		best_jd = thisjd;
	    }
	}

	/* must be no more than jdwin away from jd */
	if (best_where < 0 || fabs(best_jd - jd) > jdwin) {
	    (void) sprintf (msg, "No entries for JD=%14.5f +- %g", jd, jdwin);
	    return (-1);
	}

	/* skip back to best_where and read the entry */
	fseek (fp, best_where, SEEK_SET);

	/* skip col headings */
	(void) fgets (buf, sizeof(buf), fp);

	/* read B line */
	if (fgets (buf, sizeof(buf), fp) == NULL ||
	    sscanf (buf, "B: %lf %lf %lf %lf %lf %lf",
			    &cp->BV0, &cp->BV0e, &cp->Bkp, &cp->Bkpe,
					&cp->Bkpp, &cp->Bkppe) != 6) {
	    (void) sprintf (msg, "Bad line: %s", buf);
	    return (-1);
	}

	/* read V line */
	if (fgets (buf, sizeof(buf), fp) == NULL ||
	    sscanf (buf, "V: %lf %lf %lf %lf %lf %lf",
			&cp->VV0, &cp->VV0e, &cp->Vkp, &cp->Vkpe,
					&cp->Vkpp, &cp->Vkppe) != 6) {
	    (void) sprintf (msg, "Bad line: %s", buf);
	    return (-1);
	}

	/* read R line */
	if (fgets (buf, sizeof(buf), fp) == NULL ||
	    sscanf (buf, "R: %lf %lf %lf %lf %lf %lf",
			&cp->RV0, &cp->RV0e, &cp->Rkp, &cp->Rkpe,
					&cp->Rkpp, &cp->Rkppe) != 6) {
	    (void) sprintf (msg, "Bad line: %s", buf);
	    return (-1);
	}

	/* read I line */
	if (fgets (buf, sizeof(buf), fp) == NULL ||
	    sscanf (buf, "I: %lf %lf %lf %lf %lf %lf",
			&cp->IV0, &cp->IV0e, &cp->Ikp, &cp->Ikpe,
					&cp->Ikpp, &cp->Ikppe) != 6) {
	    (void) sprintf (msg, "Bad line: %s", buf);
	    return (-1);
	}

	/* all ok if we get here */
	*jdp = best_jd;
	return (0);
}

/* read the given photcal.def file. fill in *cp and the (fake) true values.
 * set all the error values to 0.
 * if ok, return 0; else fill in msg[] and return -1.
 *
 * follows is an example of the file format:
 *    # defaults for true, V0, kp and kpp.
 *
 *    B:  0.00  19.292 -0.679  0.33
 *    V:  0.75  18.900 -0.477  0.17
 *    R:  1.50  18.414 -0.301  0.03
 *    I:  2.25  17.775 -0.265  0.02
 */
int
photReadDefCalConst (
FILE *fp,
PCalConst *cp, 
double *Bp,
double *Vp,
double *Rp,
double *Ip,
char *msg)
{
	char buf[1024];
	int ok;

	/* skip ahead to the B: line */
	ok = 0;
	while (fgets (buf, sizeof(buf), fp)) {
	    if (sscanf(buf,"B: %lf %lf %lf %lf", Bp, &cp->BV0, &cp->Bkp,
							    &cp->Bkpp) == 4) {
		ok = 1;
		break;
	    }
	}

	if (ok)
	    if (!fgets (buf, sizeof(buf), fp) ||
					    sscanf (buf, "V: %lf %lf %lf %lf",
					Vp, &cp->VV0, &cp->Vkp, &cp->Vkpp) != 4)
		ok = 0;

	if (ok) 
	    if (!fgets (buf, sizeof(buf), fp) ||
					    sscanf (buf, "R: %lf %lf %lf %lf",
					Rp, &cp->RV0, &cp->Rkp, &cp->Rkpp) != 4)
		ok = 0;

	if (ok) 
	    if (!fgets (buf, sizeof(buf), fp) ||
					    sscanf (buf, "I: %lf %lf %lf %lf",
					Ip, &cp->IV0, &cp->Ikp, &cp->Ikpp) != 4)
		ok = 0;

	if (ok) {
	    cp->BV0e = cp->Bkpe = cp->Bkppe = 0.0;
	    cp->VV0e = cp->Vkpe = cp->Vkppe = 0.0;
	    cp->RV0e = cp->Rkpe = cp->Rkppe = 0.0;
	    cp->IV0e = cp->Ikpe = cp->Ikppe = 0.0;
	    return (0);
	}

	strcpy (msg, "Bad format");
	return (-1);
}

/* crack buf and fill in *sp.
 * return 0 if ok else -1 if doesn't look right.
 */
static int
addStar (PStdStar *sp, char buf[])
{
	double rh, rm, rs;
	double dd, dm, ds;
	double v, bv, vr, vi;
	char name[64], dsign[64];
	Obj *op;
	int n;

	n = sscanf (buf, "%s %lf %lf %lf%[ -] %lf %lf %lf %lf %lf %lf %lf",
	    name, &rh, &rm, &rs, dsign, &dd, &dm, &ds, &v, &bv, &vr, &vi);
	if (n != 12)
	    return (-1);

	memset (sp, 0, sizeof(*sp));

	op = &sp->o;
	strncpy (op->o_name, name, MAXNM-1);
	op->o_type = FIXED;
	op->f_epoch = J2000;
	op->f_RA = hrrad (rs/3600. + rm/60. + rh);
	op->f_dec = degrad (ds/3600. + dm/60. + dd);
	if (strchr (dsign, '-'))
	    op->f_dec *= -1;

	sp->Bm = bv + v;
	sp->Vm = v;
	sp->Rm = v - vr;
	sp->Im = v - vi;

	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: photstd.c,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $"};
