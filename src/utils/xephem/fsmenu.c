/* handle setup of field star options */

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <errno.h>

#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
typedef const void *qsort_arg; 
#else
extern char *getenv();
extern void *memset();
typedef void *qsort_arg;
#endif

#if defined(_POSIX_SOURCE)
#include <unistd.h>
#else
extern int close();
#endif

#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/MessageB.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "db.h"
#include "net.h"

extern Widget toplevel_w;
#define XtD     XtDisplay(toplevel_w)
extern XtAppContext xe_app;
extern Colormap xe_cm;

#define	DUPSEP		degrad(4/3600.)	/* dup if < this close, rads */
#define	DUPMAG		(3.0)		/* dup if mags differ < this much */
#define	FOVLIM		degrad(15.0)	/* max fov we'll fetch */

extern Now *mm_get_now P_((void));
extern char *getShareDir P_((void));
extern double delra P_((double dra));
extern int GSCFetch P_((double ra0, double dec0, double fov, double fmag,
    ObjF **opp, int nopp, char msg[]));
extern int GSCSetup P_((char *cdp, char *chp, char msg[]));
extern int USNOFetch P_((double r0, double d0, double fov, double fmag,
    ObjF **opp, int nopp, char msg[]));
extern int USNOSetup P_((char *cdp, int wantgsc, char msg[]));
extern int existsh P_((char *name));
extern int gscesofetch P_((Now *np, double ra, double dec, double fov,
    double mag, ObjF **opp, int nop, char msg[]));
extern int gscnetfetch P_((char *host, Now *np, double ra, double dec,
    double fov, double mag, ObjF **opp, int nop, char msg[]));
extern int isUp P_((Widget w));
extern int obj_cir P_((Now *np, Obj *op));
extern int xe2chkfile P_((char *file, char *msg));
extern int xe2fetch P_((char *file, Now *np, double ra, double dec,
    double fov, double mag, ObjF **opp, char *msg));
extern Obj *db_scan P_((DBScan *sp));  
extern void XCheck P_((XtAppContext app));
extern void defaultTextFN P_((Widget w, int setcols, char *x, char *y));
extern void db_scaninit P_((DBScan *sp, int mask, ObjF *op, int nop));
extern void db_update P_((Obj *op));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void sv_loadfs P_((int force));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *msg, int app_modal));

static int objfd_cmp P_((qsort_arg v1, qsort_arg v2));
static int fs_nodbdups P_((ObjF *fop, int nfop, double ra, double dec,
    double fov));
static int scanchk P_((Obj *mop, Obj *op, double cdec));
static void fs_create_fsshell P_((void));
static int fs_save P_((void));
static void fs_setup P_((void));
static void apply_cb P_((Widget w, XtPointer client, XtPointer call));
static void ok_cb P_((Widget w, XtPointer client, XtPointer call));
static void gsctb_cb P_((Widget w, XtPointer client, XtPointer call));
static void pmtb_cb P_((Widget w, XtPointer client, XtPointer call));
static void cancel_cb P_((Widget w, XtPointer client, XtPointer call));
static void help_cb P_((Widget w, XtPointer client, XtPointer call));

static Widget fsshell_w;	/* the main shell */
static Widget cdtf_w, cdtb_w;	/* GSC CD-ROM text field and toggle button */
static Widget nettf_w, nettb_w;	/* GSC network text field and toggle button */
static Widget chtf_w, chtb_w;	/* GSC Cache text field and toggle button */
static Widget notf_w, notb_w;	/* USNO text field and toggle button */
static Widget etb_w;		/* ESO toggle button */
static Widget ppmtf_w, ppmtb_w;	/* PMM text field and toggle button */
static Widget tyctf_w, tyctb_w;	/* Tycho text field and toggle button */
static Widget nodupt_w;		/* no dups toggle button */

/* TB codes */
typedef enum {
    CDROMTB, CACHETB, NETTB, ESOTB
} TB;

static int cd_on;		/* whether GSC CDROM is currently on */
static char *cd_dn;		/* GSC CDROM dir */
static int ch_on;		/* whether GSC Cache is currently on */
static char *ch_dn;		/* GSC Cache dir */
static int net_on;		/* whether GSC network connection is on */
static char *net_host;		/* GSC network host name */
static int e_on;		/* whether ESO network connection is on */
static int usno_on;		/* whether USNO is currently on */
static char *usno_fn;		/* USNO filename */
static int ppm_on;		/* whether ppm is currently on */
static char *ppm_fn;		/* ppm filename */
static int tyc_on;		/* whether tycho is currently on */
static char *tyc_fn;		/* tycho filename */
static int nodup_on;		/* whether dup checking is on */

/* call to set up without actually bringing up the menus.
 */
void
fs_create()
{
	if (!fsshell_w) {
	    fs_create_fsshell();
	    (void) fs_save();	/* confirming here is just annoying */
	}
}

void
fs_manage()
{
	fs_create();
	fs_setup();

	XtPopup (fsshell_w, XtGrabNone);
	set_something (fsshell_w, XmNiconic, (XtArgVal)False);
}

/* call to fetch a set of ObjF from the field star sources.
 * don't complain just if none are turned on at the moment.
 * we may eliminate any that seem to be dups of existing FIXED db objects.
 * we turn off any option that appears to not be working.
 * each o_flag in each resulting star is marked with FLDSTAR.
 * return -1 if something looks busted else count.
 * N.B. we only malloc *opp if we return > 0.
 */
int
fs_fetch (np, ra, dec, fov, mag, opp)
Now *np;	/* now */
double ra, dec;	/* at np->n_epoch */
double fov;	/* field of view, rads */
double mag;	/* limiting mag */
ObjF **opp;	/* we set *opp to a malloced list of ObjF */
{
	ObjF *op = NULL;
	int nop = 0;
	int s = 0;
	char msg[256];
	int i;

	if(!cd_on &&!ch_on &&!net_on &&!e_on &&!usno_on &&!ppm_on &&!tyc_on)
	    return (-1);

	watch_cursor (1);

	/* clamp fov */
	if (fov > FOVLIM)
	    fov = FOVLIM;

	/* the fetch tools all want and return J2000 values */
	if (epoch == EOD)
	    ap_as (np, J2000, &ra, &dec);
	else
	    precess (epoch, J2000, &ra, &dec);

	/* get the PM catalogs, if desired */
	if (ppm_on) {
	    nop = xe2fetch (ppm_fn, np, ra, dec, fov, mag, &op, msg);
	    if (nop < 0)
		ppm_on = 0;
	}
	if (tyc_on) {
	    nop = xe2fetch (tyc_fn, np, ra, dec, fov, mag, &op, msg);
	    if (nop < 0)
		tyc_on = 0;
	}

	/* now add the GSC, if desired -- enforce max fov */
	if (net_on) {
	    nop = gscnetfetch (net_host, np, ra, dec, fov, mag, &op,nop,msg);
	    if (nop < 0)
		net_on = 0;
	}
	if (e_on) {
	    nop = gscesofetch (np, ra, dec, fov, mag, &op, nop, msg);
	    if (nop < 0)
		e_on = 0;
	}
	if (cd_on || ch_on) {
	    nop = GSCFetch (ra, dec, fov, mag, &op, nop, msg);
	    if (nop < 0)
		cd_on = 0;
	}
	if (nop < 0)
	    xe_msg (msg, 1);

	/* now add USNO if desired -- enforce max fov */
	if (usno_on) {
	    nop = USNOFetch (ra, dec, fov, mag, &op, nop, msg);
	    if (nop < 0)
		usno_on = 0;
	}
	if (nop < 0)
	    xe_msg (msg, 1);

	/* set the s_fields to np */
	for (i = 0; i < nop; i++)
	    obj_cir (np, (Obj *)&op[i]);

	/* squeeze out stars which duplicate each other or the main database.
	 */
	if (nodup_on && nop > 0) {
	    int newnop = fs_nodbdups (op, nop, ra, dec, fov);
	    if (newnop < nop) {
		/* shrink down to just newnop entries now */
		nop = newnop;
		if (nop > 0)
		    op = (ObjF *) realloc ((void *)op, nop*sizeof(ObjF));
		else {
		    free ((void *)op);	/* *all* are dups! */
		    op = NULL;
		    nop = 0;
		}
	    }
	} else {
	    /* still set the FLDSTAR flag on all entries */
	    for (i = 0; i < nop; i++)
		((Obj *)(&op[i]))->o_flags |= FLDSTAR;
	}

	/* pass back the result, if there's something left */
	if (nop > 0) {
	    *opp = op;
	    s = nop;
	} else if (nop < 0) {
	    fs_setup();
	    s = -1;
	}

	watch_cursor (0);

	return (s);
}

/* return 1 if any proper motion catalog is on enabled, else 0 */
int
fs_pmon()
{
	return (ppm_on || tyc_on);
}

/* called to put up or remove the watch cursor.  */
void
fs_cursor (c)
Cursor c;
{
	Window win;

	if (fsshell_w && (win = XtWindow(fsshell_w)) != 0) {
	    if (c)
		XDefineCursor (XtDisplay(toplevel_w), win, c);
	    else
		XUndefineCursor (XtDisplay(toplevel_w), win);
	}
}

/* qsort-style comparison of two ObjF's by dec */
static int
objfd_cmp (v1, v2)
qsort_arg v1, v2;
{
	Obj *op1 = (Obj *)v1;
	Obj *op2 = (Obj *)v2;
	double d = op1->s_dec - op2->s_dec;

	if (d < 0.0)
	    return (-1);
	if (d > 0.0)
	    return (1);
	return (0);
}

/* squeeze out all entries in fop[] which are located within DUPSEP and have a
 *   magnitude within DUPMAG of any FIXED object currently in the database. We
 *   also squeeze out any GSC or USNO stars which meet those same criteria
 *   for any PM star.
 * when finished, all remaining entries will be contiguous at the front of the
 *   fop array and each will have FLDSTAR set in o_flag.
 * return the number of objects which remain.
 * N.B. we assume FLDSTAR bit is initially off in each o_flag.
 * N.B. we assume all fop[] have already been obj_cir()'d to mm_get_now().
 * N.B. we assume all entries in fop[] are within fov of ra/dec.
 */
static int
fs_nodbdups (fop, nfop, ra, dec, fov)
ObjF *fop;
int nfop;
double ra, dec, fov;
{
	double rov = fov/2;
	Obj *op;
	DBScan dbs;
	int l, u, m;
	Obj *mop;
	double diff;
	double cdec;

	/* sort fop by increasing dec */
	qsort ((void *)fop, nfop, sizeof(ObjF), objfd_cmp);

	/* mark all GSC stars with FLDSTAR flag which are close to any PM.
	 * don't compare GSC and PM stars with themselves.
	 */
	for (m = 0; m < nfop; m++) {
	    mop = (Obj *)&fop[m];

	    /* only check for GSC stars close to PM stars, not v.v. */
	    if (mop->o_name[0] == 'G')
		continue;

	    /* scan each way from mop and mark close GSC stars */
	    cdec = cos(mop->s_dec);
	    for (u = m+1; u < nfop; u++) {
		op = (Obj *)&fop[u];
		if (fabs(mop->s_dec - op->s_dec) >= DUPSEP)
		    break;
		if (op->o_name[0] == 'G'
			&& cdec*delra(mop->s_ra - op->s_ra) < DUPSEP
			&& fabs(get_mag(mop) - get_mag(op)) < DUPMAG)
		    op->o_flags |= FLDSTAR;
	    }
	    for (l = m-1; l >= 0; l--) {
		op = (Obj *)&fop[l];
		if (fabs(mop->s_dec - op->s_dec) >= DUPSEP)
		    break;
		if (op->o_name[0] == 'G'
			&& cdec*delra(mop->s_ra - op->s_ra) < DUPSEP
			&& fabs(get_mag(mop) - get_mag(op)) < DUPMAG)
		    op->o_flags |= FLDSTAR;
	    }
	}

	/* scan all FIXED objects and mark all field stars with FLDSTAR
	 * which appears to be the same any one.
	 */
	for (db_scaninit(&dbs, FIXEDM, NULL, 0); (op = db_scan(&dbs)) != 0; ) {
	    /* update the s_ra/dec fields */
	    db_update (op);

	    /* skip ops outside the given field */
	    cdec = cos(op->s_dec);
	    if (fabs(op->s_dec - dec) > rov || cdec*delra(op->s_ra - ra) > rov)
		continue;

	    /* binary search to find the fop closest to op */
	    l = 0;
	    u = nfop - 1;
	    while (l <= u) {
		m = (l+u)/2;
		mop = (Obj *)(&fop[m]);
		diff = mop->s_dec - op->s_dec;
		if (diff < 0.0)
		    l = m+1;
		else
		    u = m-1;
	    }

	    /* scan each way from m and mark all that are dups with FLDSTAR.
	     * N.B. here, FLDSTAR marks *dups* (not the entries to keep)
	     * N.B. remember, u and l have crossed each other by now.
	     */
	    for (; l < nfop; l++)
		if (scanchk ((Obj *)(&fop[l]), op, cdec) < 0)
		    break;
	    for (; u >= 0; --u)
		if (scanchk ((Obj *)(&fop[u]), op, cdec) < 0)
		    break;
	}

	/* squeeze all entries marked with FLDSTAR to the front of fop[]
	 * and mark those that remain with FLDSTAR (yes, the bit now finally
	 * means what it says).
	 */
	for (u = l = 0; u < nfop; u++) {
	    mop = (Obj *)(&fop[u]);
	    if (!(mop->o_flags & FLDSTAR)) {
		mop->o_flags |= FLDSTAR; /* now the mark means a *keeper* */
		if (u > l)
		    memcpy ((void *)(&fop[l]), (void *)mop, sizeof(ObjF));
		l++;
	    }
	}

	return (l);
}

/* if mop is further than DUPSEP from op in dec, return -1.
 * then if mop is within DUPSEP in ra and within DUPMAG in mag too set FLDSTAR
 *   in mop->o_flags.
 * return 0.
 */
static int
scanchk (mop, op, cdec)
Obj *mop, *op;
double cdec;
{
	if (fabs(mop->s_dec - op->s_dec) >= DUPSEP)
	    return (-1);
	if (cdec*delra(mop->s_ra - op->s_ra) < DUPSEP
		&& fabs(get_mag(mop) - get_mag(op)) < DUPMAG)
	    mop->o_flags |= FLDSTAR;
	return (0);
}

static void
fs_create_fsshell()
{
	Widget fsform_w, f, w;
	Arg args[20];
	int n;

	/* create outtern shell and form */

	n = 0;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle, "xephem Field Stars setup"); n++;
	XtSetArg (args[n], XmNiconName, "FStars"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	fsshell_w = XtCreatePopupShell ("FieldStars", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (fsshell_w, XmNcolormap, (XtArgVal)xe_cm);
	sr_reg (fsshell_w, "XEphem*FieldStars.x", dbcategory, 0);
	sr_reg (fsshell_w, "XEphem*FieldStars.y", dbcategory, 0);

	n = 0;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	fsform_w = XmCreateForm (fsshell_w, "FSForm", args, n);
	XtAddCallback (fsform_w, XmNhelpCallback, help_cb, NULL);
	XtManageChild (fsform_w);

	/* make the GSC section title */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	w = XmCreateLabel (fsform_w, "GSCT", args, n);
	set_xmstring (w, XmNlabelString, "Hubble Guide Star Catalog, GSC:");
	XtManageChild (w);

	    /* make the GSC CD-ROM toggle/text */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, w); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNcolumns, 60); n++;
	    cdtf_w = XmCreateTextField (fsform_w, "GSCCDDirectory", args, n);
	    wtip (cdtf_w, "Pathname to the root of the CDROM when mounted");
	    XtManageChild (cdtf_w);
	    sr_reg (cdtf_w, NULL, dbcategory, 1);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftOffset, 15); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNrightWidget, cdtf_w); n++;
	    XtSetArg (args[n], XmNrightOffset, 10); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    cdtb_w = XmCreateToggleButton (fsform_w, "GSCCD", args, n);
	    XtAddCallback (cdtb_w, XmNvalueChangedCallback, gsctb_cb,
							    (XtPointer)CDROMTB);
	    set_xmstring (cdtb_w, XmNlabelString, "ASP CDROM Directory:");
	    wtip (cdtb_w, "Whether to use the ASP CDROM for GSC stars");
	    XtManageChild (cdtb_w);
	    sr_reg (cdtb_w, NULL, dbcategory, 1);

	    /* make the GSC Cache toggle/text */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, cdtf_w); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNcolumns, 60); n++;
	    chtf_w = XmCreateTextField (fsform_w, "GSCCacheDirectory", args, n);
	    defaultTextFN (chtf_w, 0, getShareDir(), "catalogs/gsc");
	    wtip (chtf_w, "Pathname to the GSC subdirectory");
	    XtManageChild (chtf_w);
	    sr_reg (chtf_w, NULL, dbcategory, 1);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, cdtf_w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftOffset, 15); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNrightWidget, chtf_w); n++;
	    XtSetArg (args[n], XmNrightOffset, 10); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    chtb_w = XmCreateToggleButton (fsform_w, "GSCCache", args, n);
	    XtAddCallback (chtb_w, XmNvalueChangedCallback, gsctb_cb,
							    (XtPointer)CACHETB);
	    set_xmstring (chtb_w, XmNlabelString,  "Local Cache Directory:");
	    wtip (chtb_w, "Whether to use a local disk for GSC stars");
	    XtManageChild (chtb_w);
	    sr_reg (chtb_w, NULL, dbcategory, 1);

	    /* make the GSC network toggle/text */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, chtf_w); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNcolumns, 60); n++;
	    nettf_w = XmCreateTextField (fsform_w, "XEphemdbdURL", args, n);
	    wtip (nettf_w, "URL (/<host>/.../xephemdbd.pl) from which to fetch GSC stars");
	    sr_reg (nettf_w, NULL, dbcategory, 1);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, chtf_w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftOffset, 15); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNrightWidget, nettf_w); n++;
	    XtSetArg (args[n], XmNrightOffset, 10); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    nettb_w = XmCreateToggleButton (fsform_w, "XEphemdbdNet", args, n);
	    XtAddCallback (nettb_w, XmNvalueChangedCallback, gsctb_cb,
							    (XtPointer)NETTB);
	    set_xmstring (nettb_w, XmNlabelString, "Internet to xephemdbd:");
	    wtip (nettb_w,"Whether to use an Internet XEphem server for stars");
	    sr_reg (nettb_w, NULL, dbcategory, 1);
	    XtManageChild (nettb_w);
	    XtManageChild (nettf_w);

	    /* make the ESO network toggle/text */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, nettf_w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftOffset, 15); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    etb_w = XmCreateToggleButton (fsform_w, "ESONet", args, n);
	    XtAddCallback (etb_w, XmNvalueChangedCallback, gsctb_cb,
							    (XtPointer)ESOTB);
	    set_xmstring (etb_w, XmNlabelString, "Internet to ESO");
	    wtip(etb_w,"Whether to use the Internet to ESO to fetch GSC stars");
	    XtManageChild (etb_w);
	    sr_reg (etb_w, NULL, dbcategory, 1);

	/* make the USNO section title */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, etb_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	w = XmCreateLabel (fsform_w, "NOMT", args, n);
	set_xmstring (w, XmNlabelString, "USNO A or SA catalogs:");
	XtManageChild (w);

	    /* make the USNO toggle/text */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, w); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNcolumns, 60); n++;
	    notf_w = XmCreateTextField (fsform_w, "USNODirectory", args, n);
	    wtip (notf_w,
		"Pathname of A1.0, SA1.0 etc catalog series root directory");
	    defaultTextFN (notf_w, 0, getShareDir(), "catalogs/usno");
	    XtManageChild (notf_w);
	    sr_reg (notf_w, NULL, dbcategory, 1);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftOffset, 15); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNrightWidget, notf_w); n++;
	    XtSetArg (args[n], XmNrightOffset, 10); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    notb_w = XmCreateToggleButton (fsform_w, "USNO", args, n);
	    set_xmstring (notb_w, XmNlabelString, "Root directory: ");
	    wtip (notb_w, "Whether to access a USNO catalog");
	    XtManageChild (notb_w);
	    sr_reg (notb_w, NULL, dbcategory, 1);

	/* make the PM section title */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, notb_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	w = XmCreateLabel (fsform_w, "PMT", args, n);
	set_xmstring (w, XmNlabelString, "Proper Motion catalogs:");
	XtManageChild (w);

	    /* make the PM toggle/text */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, w); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNcolumns, 60); n++;
	    ppmtf_w = XmCreateTextField (fsform_w, "PPMFilename", args, n);
	    wtip (ppmtf_w, "Full pathname of ppm.xe2 file");
	    defaultTextFN (ppmtf_w, 0, getShareDir(), "catalogs/ppm.xe2");
	    XtManageChild (ppmtf_w);
	    sr_reg (ppmtf_w, NULL, dbcategory, 1);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftOffset, 15); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNrightWidget, ppmtf_w); n++;
	    XtSetArg (args[n], XmNrightOffset, 10); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    ppmtb_w = XmCreateToggleButton (fsform_w, "PPM", args, n);
	    set_xmstring (ppmtb_w, XmNlabelString, "PPM catalog: ");
	    wtip (ppmtb_w, "Whether to access the PPM catalog, ppm.xe2");
	    XtManageChild (ppmtb_w);
	    sr_reg (ppmtb_w, NULL, dbcategory, 1);

	    /* make the Tycho toggle/text */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, ppmtf_w); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNcolumns, 60); n++;
	    tyctf_w = XmCreateTextField (fsform_w, "TychoFilename", args, n);
	    wtip (tyctf_w, "Full pathname of hiptyc2.xe2 file");
	    defaultTextFN (tyctf_w, 0, getShareDir(), "catalogs/hiptyc2.xe2");
	    XtManageChild (tyctf_w);
	    sr_reg (tyctf_w, NULL, dbcategory, 1);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, ppmtf_w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftOffset, 15); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNrightWidget, tyctf_w); n++;
	    XtSetArg (args[n], XmNrightOffset, 10); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    tyctb_w = XmCreateToggleButton (fsform_w, "Tycho", args, n);
	    set_xmstring (tyctb_w, XmNlabelString, "Hipparcos + Tycho2: ");
	    wtip (tyctb_w, "Whether to access the combined Hipparcos and Tycho2 catalog");
	    XtManageChild (tyctb_w);
	    sr_reg (tyctb_w, NULL, dbcategory, 1);

	    /* enforce and tie together as 1-of-2 pair */
	    if(XmToggleButtonGetState(ppmtb_w) &&
	    				XmToggleButtonGetState(tyctb_w)) {
		xe_msg ("PPM and Tycho are both on -- turning off Tycho", 0);
		XmToggleButtonSetState (tyctb_w, False, False);
	    }
	    XtAddCallback (ppmtb_w, XmNvalueChangedCallback, pmtb_cb,
							    (XtPointer)tyctb_w);
	    XtAddCallback (tyctb_w, XmNvalueChangedCallback, pmtb_cb,
							    (XtPointer)ppmtb_w);

	    /* make the No dups toggle */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, tyctf_w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 40); n++;
	    nodupt_w = XmCreateToggleButton (fsform_w, "NoDups", args, n);
	    set_xmstring (nodupt_w,XmNlabelString,"Skip likely duplicates");
	    wtip (nodupt_w,
	      "When loading, skip objects which are very similar to ones already in memory");
	    XtManageChild (nodupt_w);
	    sr_reg (nodupt_w, NULL, dbcategory, 1);

	/* make the controls across the bottom under a separator */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, nodupt_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	w = XmCreateSeparator (fsform_w, "Sep", args, n);
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNfractionBase, 13); n++;
	f = XmCreateForm (fsform_w, "CF", args, n);
	XtManageChild (f);

	    n = 0;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 1); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 3); n++;
	    w = XmCreatePushButton (f, "Ok", args, n);
	    wtip (w, "Engage the settings and close if ok");
	    XtAddCallback (w, XmNactivateCallback, ok_cb, NULL);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 4); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 6); n++;
	    w = XmCreatePushButton (f, "Apply", args, n);
	    wtip (w, "Engage the settings and remain up");
	    XtAddCallback (w, XmNactivateCallback, apply_cb, NULL);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 7); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 9); n++;
	    w = XmCreatePushButton (f, "Close", args, n);
	    wtip (w, "Close this window without doing anything");
	    XtAddCallback (w, XmNactivateCallback, cancel_cb, NULL);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 10); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 12); n++;
	    w = XmCreatePushButton (f, "Help", args, n);
	    wtip (w, "More detailed descriptions");
	    XtAddCallback (w, XmNactivateCallback, help_cb, NULL);
	    XtManageChild (w);
}

/* set up the dialog according to our static state */
static void
fs_setup ()
{
	/* GSC */
	XmToggleButtonSetState (cdtb_w, cd_on, False);
	if (cd_dn)
	    XmTextFieldSetString (cdtf_w, cd_dn);

	XmToggleButtonSetState (chtb_w, ch_on, False);
	if (ch_dn)
	    XmTextFieldSetString (chtf_w, ch_dn);

	XmToggleButtonSetState (nettb_w, net_on, False);
	if (net_host)
	    XmTextFieldSetString (nettf_w, net_host);

	XmToggleButtonSetState (etb_w, e_on, False);

	/* USNO */
	XmToggleButtonSetState (notb_w, usno_on, False);
	if (usno_fn)
	    XmTextFieldSetString (notf_w, usno_fn);

	/* PM */
	XmToggleButtonSetState (ppmtb_w, ppm_on, False);
	if (ppm_fn)
	    XmTextFieldSetString (ppmtf_w, ppm_fn);
	XmToggleButtonSetState (tyctb_w, tyc_on, False);
	if (tyc_fn)
	    XmTextFieldSetString (tyctf_w, tyc_fn);
	XmToggleButtonSetState (nodupt_w, nodup_on, False);
}

/* save the dialog as our static state.
 * we test the new settings and might change them if they seem to be
 *   unreasonable.
 * if any major trouble, issue xe_msg and return -1, else return 0.
 */
static int
fs_save ()
{
	char msg[1024];
	int allok = 1;

	watch_cursor (1);

	/* GSC CDROM and Cache */
	cd_on = XmToggleButtonGetState (cdtb_w);
	if (cd_dn)
	    XtFree (cd_dn);
	cd_dn = XmTextFieldGetString (cdtf_w);
	ch_on = XmToggleButtonGetState (chtb_w);
	if (ch_dn)
	    XtFree (ch_dn);
	ch_dn = XmTextFieldGetString (chtf_w);
	if ((cd_on || ch_on) && GSCSetup (cd_on?cd_dn:NULL, ch_on?ch_dn:NULL,
								    msg) < 0) {
	    xe_msg (msg, 1);
	    cd_on = 0;
	    ch_on = 0;
	    fs_setup ();
	    allok = 0;
	}

	/* xephemdbd network connection */
	net_on = XmToggleButtonGetState (nettb_w);
	if (net_host)
	    XtFree (net_host);
	net_host = XmTextFieldGetString (nettf_w);
	if (net_on) {
	    /* test by actually fetching a small patch */
	    Now *np = mm_get_now();
	    double fov = degrad(1);
	    double mag = 13.0;
	    int nop;
	    ObjF *op;

	    op = NULL;
	    nop = gscnetfetch (net_host, np, 0.0, 0.0, fov, mag, &op, 0, msg);
	    if (nop <= 0) {
		if (nop == 0)
		    (void)strcpy(msg,
		    		"xephemdbd connected but returned no objects");
		xe_msg (msg, 1);
		net_on = 0;
		fs_setup ();
		allok = 0;
	    } else
		free ((void *)op);
	}

	/* ESO connection */
	e_on = XmToggleButtonGetState (etb_w);
	if (e_on) {
	    /* test by actually fetching a small patch */
	    Now *np = mm_get_now();
	    double fov = degrad(1);
	    double mag = 13.0;
	    int nop;
	    ObjF *op;

	    op = NULL;
	    nop = gscesofetch (np, 0.0, 0.0, fov, mag, &op, 0, msg);
	    if (nop <= 0) {
		if (nop == 0)
		    (void) strcpy(msg,"ESO Connected but returned no objects");
		xe_msg (msg, 1);
		e_on = 0;
		fs_setup ();
		allok = 0;
	    } else if (nop > 0)
		free ((void *)op);
	}

	/* USNO */
	usno_on = XmToggleButtonGetState (notb_w);
	if (usno_fn)
	    XtFree (usno_fn);
	usno_fn = XmTextFieldGetString (notf_w);
	if (usno_on && USNOSetup (usno_fn, 1, msg) < 0) {
	    xe_msg (msg, 1);
	    usno_on = 0;
	    fs_setup();
	    allok = 0;
	}

	/* PPM */
	ppm_on = XmToggleButtonGetState (ppmtb_w);
	if (ppm_fn)
	    XtFree (ppm_fn);
	ppm_fn = XmTextFieldGetString (ppmtf_w);
	if (ppm_on && xe2chkfile (ppm_fn, msg) < 0) {
	    xe_msg (msg, 1);
	    ppm_on = 0;
	    fs_setup();
	    allok = 0;
	}

	/* Tycho */
	tyc_on = XmToggleButtonGetState (tyctb_w);
	if (tyc_fn)
	    XtFree (tyc_fn);
	tyc_fn = XmTextFieldGetString (tyctf_w);
	if (tyc_on && xe2chkfile (tyc_fn, msg) < 0) {
	    xe_msg (msg, 1);
	    tyc_on = 0;
	    fs_setup();
	    allok = 0;
	}

	/* options */
	nodup_on = XmToggleButtonGetState (nodupt_w);

	watch_cursor (0);

	return (allok ? 0 : -1);
}

/* called from Apply */
/* ARGSUSED */
static void
apply_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (fs_save() == 0)
	    sv_loadfs (1);
}

/* called from Ok */
/* ARGSUSED */
static void
ok_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (fs_save() == 0) {
	    sv_loadfs (1);
	    XtPopdown (fsshell_w);
	}
}

/* called from Ok */
/* ARGSUSED */
static void
cancel_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* outta here */
	XtPopdown (fsshell_w);
}

/* called from any of the CDROM, network or Cache toggle buttons.
 * client is one of the TB enums to tell us which.
 */
/* ARGSUSED */
static void
gsctb_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (XmToggleButtonGetState(w))
	    switch ((int)client) {
	    case CDROMTB:	/* FALLTHRU */
	    case CACHETB:
		/* turn off net and ESO if turning on CDROM or Cache */
		XmToggleButtonSetState (nettb_w, False, False);
		XmToggleButtonSetState (etb_w, False, False);
		break;
	    case NETTB:
		/* turn off CDROM and Cache and ESO if turning on network */
		XmToggleButtonSetState (cdtb_w, False, False);
		XmToggleButtonSetState (chtb_w, False, False);
		XmToggleButtonSetState (etb_w, False, False);
		break;
	    case ESOTB:
		/* turn off CDROM and Cache and net if turning on ESO */
		XmToggleButtonSetState (cdtb_w, False, False);
		XmToggleButtonSetState (chtb_w, False, False);
		XmToggleButtonSetState (nettb_w, False, False);
		break;
	    default:
		printf ("FS: bad client: %d\n", (int)client);
		exit(1);
	    }
}

/* used to maintain 1-of-2 ppm vs. tycho databases.
 * client is the other widget.
 */
/* ARGSUSED */
static void
pmtb_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Widget otherw = (Widget)client;

	if (XmToggleButtonGetState(w))
	    XmToggleButtonSetState (otherw, False, False);
}

/* called from Ok */
/* ARGSUSED */
static void
help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        static char *msg[] = {
"Set up which field star sources to use and where they are located."
};

	hlp_dialog ("FieldStars", msg, sizeof(msg)/sizeof(msg[0]));

}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: fsmenu.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
