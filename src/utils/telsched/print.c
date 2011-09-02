#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <Xm/Xm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "strops.h"
#include "misc.h"
#include "configfile.h"
#include "telenv.h"
#include "scan.h"
//#include "db.h"

#include "telsched.h"

static void pr_1summary (Now *np, FILE *fp, Obs *op);
static void display_riset (FILE *fp, int flags, double tm);
static void display_trans (FILE *fp, int flags, double tm);
static void pr_1sls (Now *np, FILE *fp, Obs *op, char *dir, int nametag);
static void createImageFilename (Now *np, Obs *op, char *imdir, char *fullpn,
    int nametag);

/* Used in patch for crossing midnight bug (STO) */
static double utcAtStart;

/* print all current scans, one per line, to filename.
 * return 0 if ok, else -1.
 */
int
print_summary (np, op, nop, filename)
Now *np;
Obs *op;
int nop;
char *filename;
{
	char *bn = basenm(filename);
	char datestr[32];
	char timestr[32];
	FILE *fp;

	if (nop <= 0) {
	    msg ("No scans to print");
	    return(-1);
	}

	fp = fopen (filename, "w");
	if (!fp) {
	    msg ("%s: %s", bn, strerror(errno));
	    return(-1);
	}

	/* header */
	fs_sexa (timestr, utc_now(np), 2, 3600);
	fs_date (datestr, mjd_day(np->n_mjd));
	fprintf (fp, "Schedule summary for %s, dusk at %s UTC.\n", datestr,
								    timestr);
	fprintf (fp, "On C Schedule       Source         RA       Dec   Epoch   HA       El     Az     Dur F Rises  Trans  Sets   Start LST\n");

	for (; --nop >= 0; op++) {
	    pr_1summary (np, fp, op);
	    fprintf (fp, "\n");
	}

	(void) fclose (fp);

	msg ("Saved listing to %s", bn);
	return (0);
}

/* print all desired scans in *.sls format to the given filename.
 * return 0 if ok, else -1.
 */
int
print_sls (
Now *np,		/* time of observations */
Obs *op,		/* list of Obs */
int nop,		/* number of op */
char *imdir,		/* full path to default directory for images */
char *filename)		/* filename of .sls file */
{
	char *bn = basenm(filename);
	FILE *fp;
	int l;

	l = strlen (filename);
	if (l < 5 || strcasecmp (&filename[l-4], ".sls")) {
	    msg ("Filename must end with .sls: %s", bn);
	    return(-1);
	}

	fp = fopen (filename, "w");
	if (!fp) {
	    msg ("%s: %s", bn, strerror(errno));
	    return(-1);
	}

	/* STO: patch for crossing midnight bug */
	utcAtStart = 0; // clear to start

	for (l = 0; --nop >= 0; ) {
	    if (wantinsls (op)) {
	    	// use first valid time as day anchor...
	    	if(!utcAtStart) utcAtStart = op->utcstart;
			pr_1sls (np, fp, op, imdir, l++);
		}
	    op++;
	}

	(void) fclose (fp);

	msg ("Saved scans to %s", bn);
	return (0);
}

/* return True if we want to save this op in the sls file, else 0 */
int
wantinsls (op)
Obs *op;
{
	return (!op->off && op->elig && op->utcstart != NOTIME);
}

static void
pr_1summary (np, fp, op)
Now *np;
FILE *fp;
Obs *op;
{
	Now startnow;
	Obj startobj;
	char buf[128];
	double tmp;
	Obj *objp = &op->scan.obj;

	/* sense is whether it is ON */
	fprintf (fp, " %c", op->off ? 'N' : 'Y');

	/* abbreviate CCDCALIB to 1 chars */
	if (op->scan.ccdcalib.data == CD_RAW)
	    fprintf (fp, " N");
	else if (op->scan.ccdcalib.data == CD_COOKED)
	    fprintf (fp, " C");
	else if (op->scan.ccdcalib.newc == CT_BIAS)
	    fprintf (fp, " B");
	else if (op->scan.ccdcalib.newc == CT_THERMAL)
	    fprintf (fp, " T");
	else if (op->scan.ccdcalib.newc == CT_FLAT)
	    fprintf (fp, " F");
	else
	    fprintf (fp, " ?");

	fprintf (fp, " %-14.14s", op->scan.schedfn);
	fprintf (fp, " %-14.14s", op->scan.obj.o_name);

	/* show fixed obs using EOD they came with, else use EOD */
	if (objp->o_type == FIXED) {
	    fs_sexa (buf, radhr(objp->f_RA), 2, 600);
	    fprintf (fp, " %7.7s", buf);
	    fs_sexa (buf, raddeg(objp->f_dec), 3, 60);
	    fprintf (fp, " %6.6s", buf);
	    mjd_year (objp->f_epoch, &tmp);
	    fprintf (fp, " %6.1f", tmp);
	} else {
	    fs_sexa (buf, radhr(objp->s_ra), 2, 600);
	    fprintf (fp, " %7.7s", buf);
	    fs_sexa (buf, raddeg(objp->s_dec), 3, 60);
	    fprintf (fp, " %6.6s", buf);
	    /* s_* fields are always done to EOD */
	    fprintf (fp, " EOD  ");
	}

	/* if start time has been establushed, build a Now for then and use it
	 * to get the local circumstances at that moment.
	 */
	if (op->utcstart != NOTIME) {
	    double lst;

	    startnow = *np;
	    startobj = *objp;

	    startnow.n_mjd = mjd_day(startnow.n_mjd) + op->utcstart/24.0;
	    obj_cir (&startnow, &startobj);

	    np = &startnow;
	    objp = &startobj;

	    now_lst (np, &lst);
	    tmp = hrrad(lst) - objp->s_ra;
	    haRange (&tmp);
	    fs_sexa (buf, radhr(tmp), 3, 600);
	    fprintf (fp, " %8.8s", buf);

	    fs_sexa (buf, raddeg(objp->s_alt), 3, 60);
	    fprintf (fp, " %6.6s", buf);

	    fs_sexa (buf, raddeg(objp->s_az), 3, 60);
	    fprintf (fp, " %6.6s", buf);
	} else {
	    fprintf (fp, " %8.8s", "");
	    fprintf (fp, " %6.6s", "");
	    fprintf (fp, " %6.6s", "");
	}

	fprintf (fp, " %4g", op->scan.dur);
	fprintf (fp, " %c", op->scan.filter);

	display_riset (fp, op->rs.rs_flags, op->rs.rs_risetm);
	display_trans (fp, op->rs.rs_flags, op->rs.rs_trantm);
	display_riset (fp, op->rs.rs_flags, op->rs.rs_settm);

	if (op->utcstart == NOTIME)
	    strcpy (buf, "  Any");
	else
	    fs_sexa (buf, op->utcstart, 2, 60);
	fprintf (fp, " %5.5s", buf);

	if (op->utcstart == NOTIME)
	    strcpy (buf, "  Any");
	else {
	    double lst;
	    now_lst (np, &lst);
	    fs_sexa (buf, lst, 2, 60);
	}
	fprintf (fp, " %5.5s", buf);
}


static void
display_riset (fp, flags, tm)
FILE *fp;
int flags;
double tm;
{
	if (flags & RS_ERROR)
	    fprintf (fp, " Error ");
	else if (flags & RS_CIRCUMPOLAR)
	    fprintf (fp, " CirPol");
	else if (flags & RS_NEVERUP)
	    fprintf (fp, " NvrUp ");
	else if (flags & RS_NOSET)
	    fprintf (fp, " NoSet ");
	else if (flags & RS_NORISE)
	    fprintf (fp, " NoRise");
	else if (flags & RS_NOTRANS)
	    fprintf (fp, " NoTrns");
	else {
	    char str[32];

	    fs_sexa (str, mjd_hr(tm), 2, 60);	/* 5 chars: "hh:mm" */
	    (void) strcat (str, " ");		/* always add 1 */
	    fprintf (fp, " %s", str);
	}
}

static void
display_trans (fp, flags, tm)
FILE *fp;
int flags;
double tm;
{
	if (flags & RS_ERROR)
	    fprintf (fp, " Error ");
	else if (flags & RS_NEVERUP)
	    fprintf (fp, " NvrUp ");
	else if (flags & RS_NOTRANS)
	    fprintf (fp, " NoTrns");
	else {
	    char str[32];

	    fs_sexa (str, mjd_hr(tm), 2, 60);	/* 5 chars: "hh:mm" */
	    (void) strcat (str, " ");		/* always add 1 */
	    fprintf (fp, " %s", str);
	}
}

/* send op to fp in .sls scans format.
 * (we already know we really want this one).
 */
static void
pr_1sls (Now *np, FILE *fp, Obs *op, char *imdir, int nametag)
{
	Scan *sp = &op->scan;
	char raostr[32];
	char decostr[32];
	char datestr[32];
	char timestr[32];
	char dbline[1024];
	char imagepn[1024];
	double startmjd;
	int i;

	createImageFilename (np, op, imdir, imagepn, nametag);

	startmjd = mjd_day(np->n_mjd) + op->utcstart/24.0;

	/* Patch for crossing midnight bug: STO 7/23/01 */
	if(op->utcstart < utcAtStart)
		startmjd += 1.0;	// increment day

	fs_date (datestr, mjd_day(startmjd));
	fs_sexa (timestr, op->utcstart, 2, 60);
	fs_sexa (raostr, radhr(sp->rao), 2, 36000);
	fs_sexa (decostr, raddeg(sp->deco), 2, 36000);

	db_write_line (&sp->obj, dbline);

	i = 0;
	fprintf (fp, "%2d            status: N\n", i++); /*N(ew)/D(one)/F(ail)*/
	fprintf (fp, "%2d          start JD: %13.5f (%s %s UTC)\n", i++,
					      MJD0+startmjd, datestr, timestr);
	fprintf (fp, "%2d    lstdelta, mins: %-6g\n",  i++, sp->startdt/60.);
	fprintf (fp, "%2d           schedfn: %s\n",    i++, sp->schedfn);
	fprintf (fp, "%2d             title: %s\n",    i++, sp->title);
	fprintf (fp, "%2d          observer: %s\n",    i++, sp->observer);
	fprintf (fp, "%2d           comment: %s\n",    i++, sp->comment);
	fprintf (fp, "%2d               EDB: %s\n",    i++, dbline);
	fprintf (fp, "%2d          RAOffset: %s\n",    i++, raostr);
	fprintf (fp, "%2d         DecOffset: %s\n",    i++, decostr);
	fprintf (fp, "%2d    frame position: %d+%d\n", i++, sp->sx, sp->sy);
	fprintf (fp, "%2d        frame size: %dx%d\n", i++, sp->sw, sp->sh);
	fprintf (fp, "%2d           binning: %dx%d\n", i++, sp->binx, sp->biny);
	fprintf (fp, "%2d    duration, secs: %-6g\n",  i++, sp->dur);
	fprintf (fp, "%2d           shutter: %s\n",    i++,
						    ccdSO2Str(sp->shutter));
	fprintf (fp, "%2d          ccdcalib: %s\n",    i++,
						    ccdCalib2Str(sp->ccdcalib));
	fprintf (fp, "%2d            filter: %c\n",    i++, sp->filter);
	fprintf (fp, "%2d             hcomp: %d\n",    i++, sp->compress);
		fprintf (fp, "%2d   Extended Action: %s\n",    i++, ccdExtAct2Str(sp->ccdcalib));
        fprintf (fp, "%2d  Ext. Act. Values: %s\n",    i++, extActValueStr(sp));
	fprintf (fp, "%2d          priority: %d\n",    i++, sp->priority);
	fprintf (fp, "%2d          pathname: %s\n",    i++, imagepn);

}

/* create the full path name for this image in fullpn.
 * if op->scan.imagefn contains the right template, fill it in; else if
 *   already has anything, use it as-is; else make up something.
 * if op->scan.imagedn contains something, use it, else use imdir.
 */
static void
createImageFilename (Now *np, Obs *op, char *imdir, char *fullpn, int nametag)
{
	struct tm *tmp;
	time_t t;
	char *dir;

	/* first the directory portion -- use either scan's or the default */
	dir = op->scan.imagedn[0] ? op->scan.imagedn : imdir;
	telfixpath (fullpn, dir);
	if (fullpn[strlen(fullpn)-1] != '/')
	    strcat (fullpn, "/");


	/* UNIX t is seconds since 00:00:00 1/1/1970 UTC on UNIX systems;
	 * mjd was 25567.5 then.
	 */
	t = (time_t)((np->n_mjd - 25567.5)*SPD + 0.5);
	tmp = gmtime (&t);

	/* add name starting at end */
	if (strchr (op->scan.imagefn, '@')) {
	    /* imagefn[] appears created when the sched file was read in to be
	     * of the form SSS@@@NN. replace @@@ with the day-of-year.
	     */
	    char *firstAt;
	    char code[8];

	    strcat (fullpn, op->scan.imagefn);		/* add fn */
	    sprintf (code, "%03d", tmp->tm_yday+1);	/* want 1-based */
	    firstAt = strchr (fullpn, '@');		/* find @ */
	    memcpy (firstAt, code, 3);			/* strcpy() adds '\0' */
	    strcat (fullpn, ".fts");
	} else if (op->scan.ccdcalib.data == CD_NONE) {
	    /* telrun will make up the proper name */
	    strcat (fullpn, "[unused]");
	} else if (op->scan.imagefn[0]) {
	    /* something is there.. try to make it sensible at least */
	    int l = strlen (op->scan.imagefn);
	    strcat (fullpn, op->scan.imagefn);
	    if (l<4 || strcasecmp (op->scan.imagefn+l-4, ".fts"))
		strcat (fullpn, ".fts");
	} else {
	    /* make in form MMDDXXXX. MMDD from np, but
	     * XXXX is nametag just as a way to make it different (can't use
	     * time because this func is called very rapidly).
	     */
	    int mon = tmp->tm_mon+1;
	    int day = tmp->tm_mday;
	    char buf[32];

	    sprintf (buf, "%02d%02d%04d.fts", mon, day, nametag);
	    strcat (fullpn, buf);
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: print.c,v $ $Date: 2002/03/12 16:46:21 $ $Revision: 1.3 $ $Name:  $"};
