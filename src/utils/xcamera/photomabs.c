/* code handle absolute photometry (the main stuff is in photom.c) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/ToggleB.h>
#include <Xm/TextF.h>
#include <Xm/Separator.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "xtools.h"
#include "fits.h"
#include "wcs.h"
#include "telenv.h"
#include "fieldstar.h"
#include "camera.h"
#include "strops.h"
#include "photstd.h"


#define	MINSEP	degrad(10.0/3600.0)	/* min sep in star list, rads */
#define	JDWINDOW	1.0		/* max error in photcal.out, days */
#define	DEFSNR		100.0		/* SNR when using photcal.def */

/* indeces into various data structures for the 4 filters */
enum {BF, VF, RF, IF, NF};

static void cancelCB (Widget w, XtPointer client, XtPointer call);
static void defRefCB (Widget w, XtPointer client, XtPointer call);
static void create_absref_w(void);
static void setTestStar_w(void);
static void setAbsRef_w(void);
static void getAbsRef_w(void);
static void absOkCB (Widget w, XtPointer client, XtPointer call);
static void absApplyCB (Widget w, XtPointer client, XtPointer call);
static void absCloseCB (Widget w, XtPointer client, XtPointer call);
static void overrideCB (Widget w, XtPointer client, XtPointer call);
static void create_k_w(void);
static void setK_w(void);
static void getK_w(void);
static void resetStarListCB (Widget w, XtPointer client, XtPointer call);
static void resetTestStar(void);
static void readPhotCalFileCB (Widget w, XtPointer client, XtPointer call);
static void readPhotCalFile ();
static int readPhotCalOut ();
static void readPhotCalDef (int all);
static void setPhotcalDate(void);
static void kOkCB (Widget w, XtPointer client, XtPointer call);
static int getFilter (FImage *fip, int *code, char *name);
static int getAirmass (FImage *fip, double *amp);
static int getExptime (FImage *fip, double *expp);
static double normalizedVobs (StarStats *sp, double exp);

static char duh[] = "  ???? ";

static Widget abs_w;		/* toplevel absolute manager */
static Widget refname_w;	/* label for reference object name */
static Widget absref_w;		/* toplevel reference star dialog */
static Widget k_w;		/* toplevel kp/kpp dialog */
static Widget ra_w;		/* label for test star ra */
static Widget dec_w;		/* label for test star dec */
static Widget setref_w;		/* tb -- true when want to set ref inst cor */
static Widget photcaldeffn_w;	/* TF to hold default photcal file name */
static Widget photcalfn_w;	/* TF to hold photcal file name */
static Widget photcaldate_w;	/* TF to hold photcal date */
static Widget autoread_w;	/* TB to auto read photcal image on each file */

/* stuff needed to compute and display info about one filter */
typedef struct {
    /* stuff for the main portion -- the test star */
    double V, Verr;		/* true mag, and err */
    double Vsnr;		/* snr for this star */
    int Vok;			/* set when true is known */
    Widget V_w, Verr_w;		/* labels for true and error value */
    int cor;			/* degree of correction: one of COR_ values */
    Widget cor_w;		/* label to say how corrected */

    /* stuff for the reference star definition */
    double rV;			/* true mag */
    double rVsnr;		/* snr for ref star */
    int rVok;			/* true when true mag is known */
    Widget rV_w;		/* text for true mag */
    double rV0;			/* instrument correction */
    int rV0ok;			/* true when instrument correction is known */
    Widget rV0_w;		/* label for instrument correction */

    /* stuff for the photometric corrections */
    double kp, kpp;		/* photometric corrections */
    Widget kp_w, kpp_w;		/* their text widgets */
} AbsPhot;

/* one set per filter */
static AbsPhot absphot[NF];

/* value for AbsPhot.cor */
enum {COR_NONE, COR_JUSTINST, COR_INSTCOLOR};


/* list of test stars.
 * these are copies of the values of these fields when they were last
 * seen in the absphot structure. we swap them in and out as they are
 * needed so all the other code can always just use absphot.
 */
typedef struct {
    double ra, dec;	/* star location */
    double V[NF];	/* just like in AbsPhot */
    double Verr[NF];	/* just like in AbsPhot */
    double Vsnr[NF];	/* just like in AbsPhot */
    int Vok[NF];	/* just like in AbsPhot */
    int cor[NF];	/* just like in AbsPhot */
} ListStar;

static int growStarList (double ra, double dec);
static void starListToAbsPhot (int i);
static void absPhotToStarList (int i);
static int establishTestStar(FImage *fip, StarStats *sp);

static ListStar *starlist;	/* list of all test stars we are working on */
static int nstarlist;		/* number of stars in list */
static int starlistn;		/* current one in use */

/* a new image has just been loaded.
 * if abs photometry is currently active and autoread_w is true, search and
 * load calibration constants for the date of this image from photcol.out.
 */
void
readAbsPhotom()
{
	if (!state.fimage.image || !abs_w || !XtIsManaged(abs_w)
			|| !autoread_w || !XmToggleButtonGetState (autoread_w))
	    return;

	setPhotcalDate();
	readPhotCalFile();

	setAp (ABSAP);
}

/* create the absolute dialog chunk of the main photom dialog.
 * it is a child of the main photom_w form widget.
 * start attaching at top_w.
 * don't connect anything to the bottom of parent_w but return the lowest
 *   widget made in the form.
 */
Widget
createAbsPhot_w (parent_w, top_w)
Widget parent_w;
Widget top_w;
{
	typedef struct {
	    char *name;
	    char *label;
	    Widget *wp;
	} Controls;
	static Controls controls[5*4] = {
	    {"Filt", "Filter", NULL},
	    {"BVRIMag", "Mag", NULL},
	    {"MagErr", "Err", NULL},
	    {"ColCrted", "BVCor?", NULL},

	    {"BLbl", "B", NULL},
	    {"BVal", duh, &absphot[BF].V_w},
	    {"BErr", duh, &absphot[BF].Verr_w},
	    {"Bcc",  duh, &absphot[BF].cor_w},

	    {"VLbl", "V", NULL},
	    {"VVal", duh, &absphot[VF].V_w},
	    {"VErr", duh, &absphot[VF].Verr_w},
	    {"Vcc",  duh, &absphot[VF].cor_w},

	    {"RLbl", "R", NULL},
	    {"RVal", duh, &absphot[RF].V_w},
	    {"RErr", duh, &absphot[RF].Verr_w},
	    {"Rcc",  duh, &absphot[RF].cor_w},

	    {"ILbl", "I", NULL},
	    {"IVal", duh, &absphot[IF].V_w},
	    {"IErr", duh, &absphot[IF].Verr_w},
	    {"Icc",  duh, &absphot[IF].cor_w},
	};
	Widget w;
	Widget bvrirc_w;
	Arg args[20];
	int n;
	int i;

	/* make an overall rowcolumn to be managed when want to display the
	 * absolute mode fields.
	 * N.B. don't manage it here 
	 */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	abs_w = XmCreateRowColumn (parent_w, "DiffRC", args, n);

	/* make the label for the reference object */

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	refname_w = XmCreateLabel (abs_w, "RefName", args, n);
	set_xmstring (refname_w, XmNlabelString, duh);
	XtManageChild (refname_w);

	/* make the button to allow (re)defining the reference object */

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	w = XmCreatePushButton (abs_w, "DefRefPB", args, n);
	XtAddCallback (w, XmNactivateCallback, defRefCB, NULL);
	set_xmstring (w, XmNlabelString, "(Re)Define Reference ...");
	XtManageChild (w);

	/* sep */
	n = 0;
	w = XmCreateSeparator (abs_w, "Sep1", args, n);
	XtManageChild (w);

	/* labels for RA and Dec */
	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	ra_w = XmCreateLabel (abs_w, "RA", args, n);
	XtManageChild (ra_w);
	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	dec_w = XmCreateLabel (abs_w, "Dec", args, n);
	XtManageChild (dec_w);

	/* make a 5-row rowcolumn for the BVRI table */

	n = 0; 
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNnumColumns, 5); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	bvrirc_w = XmCreateRowColumn (abs_w, "BVRIRC", args, n);
	XtManageChild (bvrirc_w);

	    for (i = 0; i < XtNumber(controls); i++) {
		Controls *ctp = &controls[i];

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
		w = XmCreateLabel (bvrirc_w, ctp->name, args, n);
		set_xmstring (w, XmNlabelString, ctp->label);
		XtManageChild (w);
		if (ctp->wp)
		    *ctp->wp = w;
	    }

	/* sep */
	n = 0;
	w = XmCreateSeparator (abs_w, "Sep2", args, n);
	XtManageChild (w);

	/* add a control to reset the star */

        n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	w = XmCreatePushButton (abs_w, "RestStar", args, n);
	XtAddCallback (w, XmNactivateCallback, resetStarListCB, NULL);
	set_xmstring (w, XmNlabelString, "Reset Star List");
	XtManageChild (w);

	/* make the other dialogs too, but don't manage them yet. */
	create_absref_w();
	create_k_w();
	getK_w();	/* pick up the initial defaults */

	return (abs_w);
}

/* called to turn abs menus on or off */
void
manageAbsPhot (whether)
int whether;
{
	if (whether) {
	    XtManageChild (abs_w);
	    if (absref_w && XtIsManaged(absref_w))
		raiseShell (absref_w);
	    if (k_w && XtIsManaged(k_w))
		raiseShell (k_w);
	} else {
	    XtUnmanageChild (abs_w);
	    XtUnmanageChild (absref_w);
	    XmToggleButtonSetState (setref_w, False, False);
	    XtUnmanageChild (k_w);
	}
}

/* do whatever we need to when the the right mouse button is hit and we are
 * in absolute photometry mode.
 * the info about the star we just found is in newStar.
 */
void
absPhotWork(newStar)
StarStats *newStar;
{
	FImage *fip = &state.fimage;
	double Z;
	int fcode;
	char fname;
	AbsPhot *ap;
	double Vobs;
	double exp;
	double snr;


	if (getFilter (fip, &fcode, &fname) < 0)
	    return;	/* getFilter() already issued a msg() */
	if (getAirmass (fip, &Z) < 0)
	    return;	/* getAirmass() already issued a msg() */
	if (getExptime (fip, &exp) < 0)
	    return;	/* getExptime() already issued a msg() */

	ap = &absphot[fcode];
	Vobs = normalizedVobs (newStar, exp);
	snr = newStar->rmsSrc;

	if (XmToggleButtonGetState(setref_w)) {
	    /* compute instrument correction for reference star */

	    if (!absphot[BF].rVok || !absphot[VF].rVok) {
		msg ("Reference True B and V must be defined first.");
		return;
	    }

	    ap->rV0 = ap->rV - Vobs - ap->kp*Z
				- ap->kpp*(absphot[BF].rV - absphot[VF].rV);
	    ap->rVsnr = snr;
	    ap->rV0ok = 1;
#ifdef TRACE
	    printf ("%c Ref: rV=%5.2f Vobs=%5.2f kp=%5.2f Z=%5.3f kpp=%5.2f BrV=%5.2f VrV=%5.2f\n",
		    fname, ap->rV, Vobs, ap->kp, Z, ap->kpp, absphot[BF].rV,
							    absphot[VF].rV);
#endif

	    setAbsRef_w();
	} else {
	    /* compute what we can about the test star */

	    /* find absphot from star */
	    if (establishTestStar (fip, newStar) < 0)
		return;

	    /* compute new values */

	    if (!ap->rV0ok) {
		msg ("No instrument correction for %c filter.", fname);
		ap->cor = COR_NONE;
	    } else {
		double dm0, dm1;

		/* can at least apply an instrument correction */
		ap->V = Vobs + ap->rV0 + ap->kp*Z;
		ap->Vok = 1;
		ap->cor = COR_JUSTINST;
#ifdef TRACE
		printf ("%c Tst: Vobs=%5.2f rV0=%5.2f kp=%5.2f Z=%5.3f\n",
					    fname, Vobs, ap->rV0, ap->kp, Z);

#endif
		/* if know B and V for this star we can also color correct it */
		if (absphot[BF].Vok && absphot[VF].Vok) {
		    ap->V += ap->kpp*(absphot[BF].V - absphot[VF].V);
		    ap->cor = COR_INSTCOLOR;
#ifdef TRACE
		printf ("        kpp=%5.2f BrV=%5.2f VrV=%5.2f\n",
				fname, ap->kpp, absphot[BF].rV, absphot[VF].rV);
#endif
		}

		ap->Vsnr = snr;

		dm0 = 2.2/ap->Vsnr;
		dm1 = 2.2/ap->rVsnr;
		ap->Verr = sqrt (dm0*dm0 + dm1*dm1);
	    }

	    /* display new stuff */
	    setTestStar_w();

	    /* save it in star list */
	    absPhotToStarList (starlistn);
	}
}

/* handy callback to unmanage the widget named by client */
/* ARGSUSED */
static void
cancelCB (Widget w, XtPointer client, XtPointer call)
{
	XtUnmanageChild ((Widget)client);
}

/* called when we want to allow defining a new abs mode ref star */
/* ARGSUSED */
static void
defRefCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (XtIsManaged(absref_w)) {
	    XtUnmanageChild (absref_w);
	    XmToggleButtonSetState (setref_w, False, False);
	    XtUnmanageChild (k_w);
	} else {
	    setAbsRef_w();
	    XtManageChild (absref_w);
	}
}

/* create the dialog to allow entering True BVRI for the star that will serve
 * as the reference for absolute photometry.
 */
static void
create_absref_w()
{
	typedef struct {
	    int istext;		/* 1 for text, 0 for label */
	    char *name;		/* name of widget */
	    char *label;	/* initial value or label string */
	    Widget *wp;		/* widget id, if not NULL */
	} Field;
	static Field fields[] = {
	    {0, "Filt", "Filter", NULL},
	    {0, "Mag", "True Mag", NULL},
	    {0, "InstrCor", "Instr Cor", NULL},

	    {0, "BL", "B", NULL},
	    {1, "BMag", duh, &absphot[BF].rV_w},
	    {0, "BInst", duh, &absphot[BF].rV0_w},

	    {0, "VL", "V", NULL},
	    {1, "VMag", duh, &absphot[VF].rV_w},
	    {0, "VInst", duh, &absphot[VF].rV0_w},

	    {0, "RL", "R", NULL},
	    {1, "RMag", duh, &absphot[RF].rV_w},
	    {0, "RInst", duh, &absphot[RF].rV0_w},

	    {0, "IL", "I", NULL},
	    {1, "IMag", duh, &absphot[IF].rV_w},
	    {0, "IInst", duh, &absphot[IF].rV0_w},
	};
	Widget rc_w, w;
	Widget sep_w;
	Widget pb_w;
	Arg args[20];
	int n;
	int i;

	/* create master form */
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNmarginHeight, 5); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 5); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	XtSetArg (args[n], XmNfractionBase, 13); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	absref_w = XmCreateFormDialog (toplevel_w, "DefRefFD", args, n);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Abs Phot Ref Star"); n++;
	XtSetValues (XtParent(absref_w), args, n);
	XtVaSetValues (absref_w, XmNcolormap, camcm, NULL);

	/* make a toggle to determine whether we are to set ref star */
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	setref_w = XmCreateToggleButton (absref_w, "SetRefTB", args, n);
	set_xmstring (setref_w, XmNlabelString, "Define Instr Cor");
	XtManageChild (setref_w);

	/* make a r/c for the main table */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, setref_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNnumColumns, XtNumber(fields)/3); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	rc_w = XmCreateRowColumn (absref_w, "MainRC", args, n);
	XtManageChild (rc_w);

	    for (i = 0; i < XtNumber(fields); i++) {
		Field *fp = &fields[i];

		n = 0;
		if (fp->istext) {
		    XtSetArg (args[n], XmNcolumns, 5); n++;
		    w = XmCreateTextField (rc_w, fp->name, args, n);
		    if (fp->label)
			XmTextFieldSetString (w, fp->label);
		    XtManageChild (w);
		} else {
		    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
		    w = XmCreateLabel (rc_w, fp->name, args, n);
		    if (fp->label)
			set_xmstring (w, XmNlabelString, fp->label);
		    XtManageChild (w);
		}

		if (fp->wp)
		    *fp->wp = w;
	    }

	/* separator */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (absref_w, "Sep1", args, n);
	XtManageChild (sep_w);

	/* Override button */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	pb_w = XmCreatePushButton (absref_w, "OverridePB", args, n);
	set_xmstring(pb_w,XmNlabelString,"Override\nPhotometric Constants ...");
	XtAddCallback (pb_w, XmNactivateCallback, overrideCB, NULL);
	XtManageChild (pb_w);

	/* separator */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, pb_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (absref_w, "Sep2", args, n);
	XtManageChild (sep_w);

	/* Ok button */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 1); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 4); n++;
	w = XmCreatePushButton (absref_w, "Ok", args, n);
	XtAddCallback (w, XmNactivateCallback, absOkCB, NULL);
	XtManageChild (w);

	/* Apply button */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 5); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 8); n++;
	w = XmCreatePushButton (absref_w, "Apply", args, n);
	XtAddCallback (w, XmNactivateCallback, absApplyCB, NULL);
	XtManageChild (w);

	/* Close button */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 9); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 12); n++;
	w = XmCreatePushButton (absref_w, "Close", args, n);
	XtAddCallback (w, XmNactivateCallback, absCloseCB, absref_w);
	XtManageChild (w);
}

/* find the new star in the starlist, or add it if not found.
 * set starlistn to its index.
 */
static int
establishTestStar (fip, sp)
FImage *fip;
StarStats *sp;
{
	double ra, dec;
	int i;

	if (xy2rd (fip, sp->x, sp->y, &ra, &dec) < 0) {
	    msg ("No WCS fields");
	    return (-1);
	}

	for (i = 0; i < nstarlist; i++) {
	    ListStar *tp = &starlist[i];
	    double ca, B;

	    solve_sphere(ra-tp->ra, PI/2-dec, sin(tp->dec),cos(tp->dec),&ca,&B);
	    if (acos(ca) <= MINSEP)
		break;
	}

	if (i == nstarlist) {
	    /* not in list so add it */
	    if (growStarList(ra, dec) < 0)
		return (-1);
	    starlistn = nstarlist - 1;
	} else {
	    /* found it at location i */
	    starlistn = i;
	}

	starListToAbsPhot (starlistn);

	return (0);
}

/* set up the widgets displaying what we know of the test star in absphot.
 * also set the ra_w/dec_w widgets.
 */
static void
setTestStar_w(void)
{
	char buf[128], buf2[128];
	int f;

	for (f = BF; f <= IF; f++) {
	    AbsPhot *ap = &absphot[f];

	    if (ap->Vok) {
		sprintf (buf, "%6.3f", ap->V);
		sprintf (buf2, "%6.3f", ap->Verr);
	    } else {
		strcpy (buf, duh);
		strcpy (buf2, duh);
	    }
	    set_xmstring (ap->V_w, XmNlabelString, buf);
	    set_xmstring (ap->Verr_w, XmNlabelString, buf2);

	    switch (ap->cor) {
	    case COR_NONE: strcpy (buf, duh); break;
	    case COR_JUSTINST:  strcpy (buf, "   N  "); break;
	    case COR_INSTCOLOR: strcpy (buf, "   Y  "); break;
	    default:
		fprintf (stderr, "Bad %d cor: %d\n", f, ap->cor);
		abort();
	    }
	    set_xmstring (ap->cor_w, XmNlabelString, buf);
	}

	if (nstarlist == 0) {
	    strcpy (buf, duh);
	    wlprintf (ra_w,  " RA: %s", buf);
	    wlprintf (dec_w, "Dec: %s", buf);
	} else {
	    double tmpra, tmpdec;

	    tmpra = starlist[starlistn].ra;
	    tmpdec = starlist[starlistn].dec;
	    /* star list is already the user's epoch choice */
	    fs_sexa (buf, radhr(tmpra), 3, 360000);
	    wlprintf (ra_w,  " RA: %s", buf);
	    fs_sexa (buf, raddeg(tmpdec), 3, 36000);
	    wlprintf (dec_w, "Dec: %s", buf);
	}

}

/* copy the info about the reference star to their display widgets */
static void
setAbsRef_w()
{
	char buf[128];
	int f;

	for (f = BF; f <= IF; f++) {
	    AbsPhot *ap = &absphot[f];

	    if (ap->rVok)
		sprintf (buf, "%5.2f", ap->rV);
	    else
		strcpy (buf, duh);
	    XmTextFieldSetString (ap->rV_w, buf);

	    if (ap->rV0ok)
		sprintf (buf, "%5.2f", ap->rV0);
	    else
		strcpy (buf, duh);
	    set_xmstring (ap->rV0_w, XmNlabelString, buf);
	}
}

/* get the info from the reference star true magnitude widgets */
static void
getAbsRef_w()
{
	char *txt;
	int f;

	for (f = BF; f <= IF; f++) {
	    AbsPhot *ap = &absphot[f];

	    txt = XmTextFieldGetString (ap->rV_w);
	    ap->rV = atof (txt);
	    XtFree (txt);
	    ap->rVok = 1;
	}
}

/* called when we want to copy the true abs widgets to their values.
 * also unmanage absdef_w and turn off the set reference control.
 */
/* ARGSUSED */
static void
absOkCB (Widget w, XtPointer client, XtPointer call)
{
	getAbsRef_w();
	XtUnmanageChild (absref_w);
	XmToggleButtonSetState (setref_w, False, False);
}

/* called when we want to copy the true abs widgets to their values.
 */
/* ARGSUSED */
static void
absApplyCB (Widget w, XtPointer client, XtPointer call)
{
	getAbsRef_w();
}

/* called when we want to drop the Reference dialog.
 * unmanage the dialog and make sure the setref control is off.
 */
/* ARGSUSED */
static void
absCloseCB (Widget w, XtPointer client, XtPointer call)
{
	XtUnmanageChild (absref_w);
	XtUnmanageChild (k_w);
	XmToggleButtonSetState (setref_w, False, False);
}

/* called when we want to allow defining the k photometric constants */
/* ARGSUSED */
static void
overrideCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{

	if (XtIsManaged(k_w)) 
	    raiseShell (k_w);
	else {
	    setK_w();
	    if (state.fimage.image)
		setPhotcalDate();
	    XtManageChild (k_w);
	}
}

/* create the dialog to allow entering new values for the k photometric
 * constants used in absolute photometry.
 */
static void
create_k_w()
{
	typedef struct {
	    int istext;		/* 1 for text, 0 for label */
	    char *name;		/* name of widget */
	    char *label;	/* initial value or label string */
	    Widget *wp;		/* widget id, if not NULL */
	} Field;
	static Field fields[] = {
	    {0, "Filt", "Filter", NULL},
	    {0, "KP", "k'", NULL},
	    {0, "KPP", "k''", NULL},

	    /* don't init the text strings so we can pull out their X defaults*/
	    {0, "BL", "B", NULL},
	    {1, "BKP", NULL, &absphot[BF].kp_w},
	    {1, "BKPP", NULL, &absphot[BF].kpp_w},

	    {0, "VL", "V", NULL},
	    {1, "VKP", NULL, &absphot[VF].kp_w},
	    {1, "VKPP", NULL, &absphot[VF].kpp_w},

	    {0, "RL", "R", NULL},
	    {1, "RKP", NULL, &absphot[RF].kp_w},
	    {1, "RKPP", NULL, &absphot[RF].kpp_w},

	    {0, "IL", "I", NULL},
	    {1, "IKP", NULL, &absphot[IF].kp_w},
	    {1, "IKPP", NULL, &absphot[IF].kpp_w},
	};
	Widget l_w, rc_w, w;
	Widget sep_w;
	char buf[1024];
	Arg args[20];
	int n;
	int i;

	/* create master form */
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNmarginHeight, 5); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 5); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	XtSetArg (args[n], XmNfractionBase, 13); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	k_w = XmCreateFormDialog (toplevel_w, "KFD", args, n);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Photometric Constants"); n++;
	XtSetValues (XtParent(k_w), args, n);
	XtVaSetValues (k_w, XmNcolormap, camcm, NULL);

	/* make top label of instructions */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	l_w = XmCreateLabel (k_w, "INSL", args, n);
	set_xmstring (l_w, XmNlabelString, "Enter new values here:");
	XtManageChild (l_w);

	/* make a r/c for the main table */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, l_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNnumColumns, XtNumber(fields)/3); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	rc_w = XmCreateRowColumn (k_w, "MainRC", args, n);
	XtManageChild (rc_w);

	    for (i = 0; i < XtNumber(fields); i++) {
		Field *fp = &fields[i];

		n = 0;
		if (fp->istext) {
		    XtSetArg (args[n], XmNcolumns, 6); n++;
		    w = XmCreateTextField (rc_w, fp->name, args, n);
		    if (fp->label)
			XmTextFieldSetString (w, fp->label);
		    XtManageChild (w);
		} else {
		    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
		    w = XmCreateLabel (rc_w, fp->name, args, n);
		    if (fp->label)
			set_xmstring (w, XmNlabelString, fp->label);
		    XtManageChild (w);
		}

		if (fp->wp)
		    *fp->wp = w;
	    }

	/* separator */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (k_w, "Sep1", args, n);
	XtManageChild (sep_w);

	/* r/c section to get manage getting values from a file */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	XtSetArg (args[n], XmNspacing, 5); n++;
	rc_w = XmCreateRowColumn (k_w, "AskRC", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    l_w = XmCreateLabel (rc_w, "DIL1", args, n);
	    set_xmstring (l_w, XmNlabelString, "... or Set from Photcal File:\n(Also sets new Instr Cor)");
	    XtManageChild (l_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "L", args, n);
	    set_xmstring (w, XmNlabelString, " ");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    l_w = XmCreateLabel (rc_w, "DL", args, n);
	    set_xmstring (l_w, XmNlabelString, "Date (DD/MM/YY):");
	    XtManageChild (l_w);

	    n = 0;
	    photcaldate_w = XmCreateTextField (rc_w, "DTF", args, n);
	    XtManageChild (photcaldate_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "L", args, n);
	    set_xmstring (w, XmNlabelString, " ");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    l_w = XmCreateLabel (rc_w, "FL", args, n);
	    set_xmstring (l_w, XmNlabelString, "Name of file with dated info:");
	    XtManageChild (l_w);

	    telfixpath (buf, "archive/photcal/photcal.out");
	    n = 0;
	    XtSetArg (args[n], XmNvalue, buf); n++;
	    photcalfn_w = XmCreateTextField (rc_w, "PhotcalFN", args, n);
	    XtManageChild (photcalfn_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "L", args, n);
	    set_xmstring (w, XmNlabelString, " ");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    l_w = XmCreateLabel (rc_w, "FL", args, n);
	    set_xmstring (l_w, XmNlabelString, "Name of default file:");
	    XtManageChild (l_w);

	    telfixpath (buf, "archive/photcal/photcal.def");
	    n = 0;
	    XtSetArg (args[n], XmNvalue, buf); n++;
	    photcaldeffn_w = XmCreateTextField (rc_w, "PhotcalDefFN", args, n);
	    XtManageChild (photcaldeffn_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "L", args, n);
	    set_xmstring (w, XmNlabelString, " ");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (rc_w, "ReadPhotCalPB", args, n);
	    XtAddCallback (w, XmNactivateCallback, readPhotCalFileCB, NULL);
	    set_xmstring (w, XmNlabelString, "Read Photcal File Once");
	    XtManageChild (w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "L", args, n);
	    set_xmstring (w, XmNlabelString, " ");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    autoread_w = XmCreateToggleButton (rc_w, "AutoReadPhotCalTB",
								    args, n);
	    set_xmstring (autoread_w, XmNlabelString,
				    "Auto Read Photcal File\non Each Image");
	    XtManageChild (autoread_w);

	/* another separator */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (k_w, "Sep2", args, n);
	XtManageChild (sep_w);

	/* Ok button */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 1); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 4); n++;
	w = XmCreatePushButton (k_w, "Ok", args, n);
	XtAddCallback (w, XmNactivateCallback, kOkCB, (XtPointer)1);
	XtManageChild (w);

	/* Apply button */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 5); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 8); n++;
	w = XmCreatePushButton (k_w, "Apply", args, n);
	XtAddCallback (w, XmNactivateCallback, kOkCB, (XtPointer)0);
	XtManageChild (w);

	/* Close button */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 9); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 12); n++;
	w = XmCreatePushButton (k_w, "Close", args, n);
	XtAddCallback (w, XmNactivateCallback, cancelCB, k_w);
	XtManageChild (w);
}

/* copy the kp and kpp values to their display widgets.
 */
static void
setK_w()
{
	char buf[128];
	int f;

	for (f = BF; f <= IF; f++) {
	    AbsPhot *ap = &absphot[f];

	    sprintf (buf, "%5.2f", ap->kp);
	    XmTextFieldSetString (ap->kp_w, buf);
	    sprintf (buf, "%5.2f", ap->kpp);
	    XmTextFieldSetString (ap->kpp_w, buf);
	}
}

/* get the info from the kp and kpp widgets */
static void
getK_w()
{
	char *txt;
	int f;

	for (f = BF; f <= IF; f++) {
	    AbsPhot *ap = &absphot[f];

	    txt = XmTextFieldGetString(ap->kp_w);
	    ap->kp = atof(txt);
	    XtFree(txt);
	    txt = XmTextFieldGetString(ap->kpp_w);
	    ap->kpp = atof(txt);
	    XtFree(txt);
	}
}

/* called when we want to use new values from the kp and kpp widgets.
 * also unmanage k_w and reset the Instrument corrections.
 * client is 1 if we also want to close the dialog.
 */
/* ARGSUSED */
static void
kOkCB (Widget w, XtPointer client, XtPointer call)
{
	int f;

	for (f = BF; f <= IF; f++)
	    absphot[f].rV0ok = 0;
	setAbsRef_w();

	getK_w();

	if (client)
	    XtUnmanageChild (k_w);
}

/* called to reset the list of known test stars */
/* ARGSUSED */
static void
resetStarListCB (Widget w, XtPointer client, XtPointer call)
{
	/* discard present star list */
	if (starlist)
	    free ((char *)starlist);
	starlist = NULL;
	nstarlist = 0;

	resetTestStar();
	setTestStar_w();
}

/* called to read photcal file */
/* ARGSUSED */
static void
readPhotCalFileCB (Widget w, XtPointer client, XtPointer call)
{
	readPhotCalFile();
}

/* read the photcal.out filename named in photcalfn_w and look for an entry
 * within JDWINDOW of the date in photcaldate_w.
 * if nothing appropriate, then try file named in photcaldeffn_w.
 * if found anything from either place, fill in all of absphot.
 */
static void
readPhotCalFile()
{
	char *dmy;
	int m, y;
	double d;
	char *fn;
	double jd;

	dmy = XmTextFieldGetString (photcaldate_w);
	if (sscanf (dmy, "%lf/%d/%d", &d, &m, &y) == 3)
	    y += 1900;
	else if (sscanf (dmy, "%d-%d-%lf", &y, &m, &d) != 3) {
	    msg ("Bad date string: %s", dmy);
	    XtFree (dmy);
	    return;
	}
	cal_mjd (m, d, y, &jd);
	jd += MJD0;

	fn = XmTextFieldGetString (photcalfn_w);
	if (readPhotCalOut (fn, jd) < 0)
	    readPhotCalDef(1);	/* read everything from photcal.def */
	else
	    readPhotCalDef(0);	/* just read V from photcal.def */

	XtFree (dmy);
	XtFree (fn);
}

/* open and read the given photcal.out file for the entry matching the jd.
 * if found ok set the new values and return 0, else return -1.
 */
static int
readPhotCalOut (char fn[], double jd)
{
	char buf[1024];
	PCalConst pcc;
	double V0e, kpe, kppe;
	double foundjd;
	double d;
	int m, y;
	FILE *fp;

	/* open file */
	fp = fopen (fn, "r");
	if (!fp) {
	    msg ("Can not open %s: %s", fn, strerror(errno));
	    return (-1);
	}

	/* look for entry near jd */
	if (photReadCalConst (fp, jd, JDWINDOW, &pcc, &foundjd, buf) < 0) {
	    msg ("%s: %s", fn, buf);
	    (void) fclose (fp);
	    return (-1);
	}
	(void) fclose (fp);

	/* collect into our data structure */
	absphot[BF].kp = pcc.Bkp;  absphot[BF].kpp = pcc.Bkpp;
	absphot[BF].rV0 = pcc.BV0; absphot[BF].rV0ok = 1;
	absphot[BF].rVsnr = pcc.BV0/pcc.BV0e;
	absphot[VF].kp = pcc.Vkp;  absphot[VF].kpp = pcc.Vkpp;
	absphot[VF].rV0 = pcc.VV0; absphot[VF].rV0ok = 1;
	absphot[VF].rVsnr = pcc.VV0/pcc.VV0e;
	absphot[RF].kp = pcc.Rkp;  absphot[RF].kpp = pcc.Rkpp;
	absphot[RF].rV0 = pcc.RV0; absphot[RF].rV0ok = 1;
	absphot[RF].rVsnr = pcc.RV0/pcc.RV0e;
	absphot[IF].kp = pcc.Ikp;  absphot[IF].kpp = pcc.Ikpp;
	absphot[IF].rV0 = pcc.IV0; absphot[IF].rV0ok = 1;
	absphot[IF].rVsnr = pcc.IV0/pcc.IV0e;
	setK_w();
	setAbsRef_w();

	/* find and report max errors */
	V0e = pcc.BV0e;
	if (pcc.VV0e > V0e) V0e = pcc.VV0e;
	if (pcc.RV0e > V0e) V0e = pcc.RV0e;
	if (pcc.IV0e > V0e) V0e = pcc.IV0e;
	kpe = pcc.Bkpe;
	if (pcc.Vkpe > kpe) kpe = pcc.Vkpe;
	if (pcc.Rkpe > kpe) kpe = pcc.Rkpe;
	if (pcc.Ikpe > kpe) kpe = pcc.Ikpe;
	kppe = pcc.Bkppe;
	if (pcc.Vkppe > kppe) kppe = pcc.Vkppe;
	if (pcc.Rkppe > kppe) kppe = pcc.Rkppe;
	if (pcc.Ikppe > kppe) kppe = pcc.Ikppe;

	mjd_cal (foundjd - MJD0, &m, &d, &y);
	msg ("Read %s at DMY=%g/%d/%d: Max Errs: V0=%g kp=%g kpp=%g",
					basenm(fn), d, m, y, V0e, kpe, kppe);

	return (0);
}

/* read the photcal.def file named by photcaldeffn_w: always set the "true"
 *   values absphot[*].rV.
 * if "all" is set then also set abshot[*].rV0/kp/kpp.
 */
static void
readPhotCalDef(int all)
{
	double rB, rV, rR, rI;
	PCalConst pcc;
	char buf[1024];
	char *fn;
	FILE *fp;

	/* open the default file */
	fn = XmTextFieldGetString (photcaldeffn_w);
	fp = fopen (fn, "r");
	if (!fp) {
	    msg ("%s: %s", fn, strerror(errno));
	    return;
	}

	/* read */
	if (photReadDefCalConst (fp, &pcc, &rB, &rV, &rR, &rI, buf) < 0) {
	    msg ("%s: %s", fn, buf);
	    (void) fclose (fp);
	    XtFree (fn);
	    return;
	}
	(void) fclose (fp);

	/* always fill in the "true" values */
	absphot[BF].rV = rB; absphot[BF].rVok = 1;
	absphot[VF].rV = rV; absphot[VF].rVok = 1;
	absphot[RF].rV = rR; absphot[RF].rVok = 1;
	absphot[IF].rV = rI; absphot[IF].rVok = 1;

	/* and possibly the photometric model values */
	if (all) {
	    absphot[BF].kp = pcc.Bkp;  absphot[BF].kpp = pcc.Bkpp;
	    absphot[BF].rV0 = pcc.BV0; absphot[BF].rV0ok = 1;
	    absphot[BF].rVsnr = DEFSNR;
	    absphot[VF].kp = pcc.Vkp;  absphot[VF].kpp = pcc.Vkpp;
	    absphot[VF].rV0 = pcc.VV0; absphot[VF].rV0ok = 1;
	    absphot[VF].rVsnr = DEFSNR;
	    absphot[RF].kp = pcc.Rkp;  absphot[RF].kpp = pcc.Rkpp;
	    absphot[RF].rV0 = pcc.RV0; absphot[RF].rV0ok = 1;
	    absphot[RF].rVsnr = DEFSNR;
	    absphot[IF].kp = pcc.Ikp;  absphot[IF].kpp = pcc.Ikpp;
	    absphot[IF].rV0 = pcc.IV0; absphot[IF].rV0ok = 1;
	    absphot[IF].rVsnr = DEFSNR;
	    setK_w();

	    msg ("Date not found -- using %s", fn);
	}

	setAbsRef_w();
	XtFree (fn);
}

/* set photcaldate_w to DATE-OBS field of state.fimage */
static void
setPhotcalDate()
{
	char date_obs[FITS_HCOLS];

	if (getStringFITS (&state.fimage, "DATE-OBS", date_obs) < 0) {
	    msg ("No DATE-OBS in image.");
	    return;
	}

	set_something (photcaldate_w, XmNvalue, date_obs);
}

/* reset the entries in absphot relating to the test star */
static void
resetTestStar()
{
	int f;

	for (f = BF; f <= IF; f++) {
	    AbsPhot *ap = &absphot[f];
	    ap->Vok = 0;
	    ap->cor = COR_NONE;
	}
}

/* convert the filter named in FILTER in fip to one of our enum codes */
static int
getFilter (FImage *fip, int *code, char *name)
{
	char filter[80];
	char f;

	if (getStringFITS (fip, "FILTER", filter) < 0) {
	    msg ("Image has no FILTER field.");
	    return (-1);
	}
	f = filter[0];

	if (islower(f))
	    f = (char)toupper(f);

	switch (f) {
	case 'B': *code = BF; break;
	case 'V': *code = VF; break;
	case 'R': *code = RF; break;
	case 'I': *code = IF; break;
	default:
	    msg ("FILTER must be one of BVRI: %c", f);
	    return (-1);
	}

	*name = f;
	return (0);
}

static int
getAirmass (FImage *fip, double *amp)
{
	char elevation[80];
	double alt;

	if (getStringFITS (fip, "ELEVATION", elevation) < 0) {
	    msg ("Image has no ELEVATION field.");
	    return (-1);
	}

	if (scansex (elevation, &alt) < 0) {
	    msg ("Bad ELEVATION format: %s", elevation);
	    return (-1);
	}

	alt = degrad (alt);
	*amp = 1.0/sin(alt);

	return (0);
}

static int
getExptime (FImage *fip, double *expp)
{
	if (getRealFITS (fip, "EXPTIME", expp) < 0) {
	    msg ("Image has no EXPTIME field.");
	    return (-1);
	}

	return (0);
}

/* find the observed brightness of the given star normalized to the given
 * exposure time.
 */
static double
normalizedVobs (sp, exp)
StarStats *sp;	/* observed star */
double exp;	/* exp time, seconds */
{
	double Vobs = -2.511886 * log10(sp->Src/exp);
	return (Vobs);
}

/* add one entry to starlist.
 * set the RA and Dec fields and reset all the others.
 * return 0 if ok else -1 if no more memory.
 */
static int
growStarList (double ra, double dec)
{
	char *mem;

	if (nstarlist == 0)
	    mem = malloc (sizeof(ListStar));
	else
	    mem = realloc ((char *)starlist, (nstarlist+1)*sizeof(ListStar));
	if (!mem) {
	    msg ("No more memory to grow star list");
	    return (-1);
	}
	starlist = (ListStar *) mem;

	memset ((char *)&starlist[nstarlist], 0, sizeof(ListStar));
	starlist[nstarlist].ra = ra;
	starlist[nstarlist].dec = dec;

	nstarlist++;

	return (0);
}

/* copy appropriate absphot fields to starlist[i] */
static void
absPhotToStarList (i)
int i;
{
	int f;

	for (f = BF; f <= IF; f++) {
	    starlist[i].V[f]    = absphot[f].V;
	    starlist[i].Verr[f] = absphot[f].Verr;
	    starlist[i].Vsnr[f] = absphot[f].Vsnr;
	    starlist[i].Vok[f]  = absphot[f].Vok;
	    starlist[i].cor[f]  = absphot[f].cor;
	}
}

/* copy fields from starlist[i] to the appropriate absphot fields
 */
static void
starListToAbsPhot (i)
int i;
{
	int f;

	for (f = BF; f <= IF; f++) {
	    absphot[f].V    = starlist[i].V[f];
	    absphot[f].Verr = starlist[i].Verr[f];
	    absphot[f].Vsnr = starlist[i].Vsnr[f];
	    absphot[f].Vok  = starlist[i].Vok[f];
	    absphot[f].cor  = starlist[i].cor[f];
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: photomabs.c,v $ $Date: 2001/04/19 21:12:00 $ $Revision: 1.1.1.1 $ $Name:  $"};
