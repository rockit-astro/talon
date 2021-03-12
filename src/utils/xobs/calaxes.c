/* this dialog allows the operator to calibrate the telescope axes using
 * several known stars at known times and encoder positions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/Form.h>
#include <Xm/TextF.h>
#include <Xm/Form.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "misc.h"
#include "xtools.h"
#include "configfile.h"
#include "telenv.h"
#include "telstatshm.h"
#include "cliserv.h"
#include "strops.h"

#include "xobs.h"

static void mkCalGUI (void);
static int tooClose (int i, int j);
static void resetCB (Widget w, XtPointer client, XtPointer call);
static void solveCB (Widget w, XtPointer client, XtPointer call);
static void installCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);
static void undoCB (Widget w, XtPointer client, XtPointer call);
static void markCB (Widget w, XtPointer client, XtPointer call);
static void mkTable (Widget p_w);
static void mkRow (Widget p_w, int i);
static void showSolution(int empty);
static int installNew();
static void solveNew(void);
static void findHD (int i);

#define MAXOBJS		8	/* max number of objects to use */
#define	MINDHA	degrad(1)	/* min HA sep between any pair */
#define	MINDDEC	degrad(1)	/* min Dec sep between any pair */
#define	CHCINDENT	90	/* pixels to indent lights */

static Widget cal_w;		/* main form shell */
static Widget msg_w;		/* top label for messages */
static Widget new0_w;		/* label to display HT DT of model */
static Widget new1_w;		/* label to display Axes of model */
static Widget new2_w;		/* label to display polar misalignment */

/* names of and stuff from the config files */
static char hcfgname[] = "archive/config/home.cfg";
static TelAxes tax;		/* axes reference info */
static int taxok;		/* set once a solution has been found */

static char blank[] = " ";
static char intro_msg[] =
		    "Select a star with Sky View, center with Paddle, Mark.";

/* info needed for each object, such as display widgets and especially the
 * reference stars and scope locations from which we finally compute the
 * transform.
 */
static Widget resid_w[MAXOBJS];	/* Label for showing residual */
static Widget lite_w[MAXOBJS];	/* indicator to show when marked */
static Widget lbl_w[MAXOBJS];	/* Label to show obj name */
typedef struct {
    Obj obj;			/* obj definition */
    double obsmjd;		/* mjd of observation */
    double henc, denc;		/* raw encoder angle */
    double ha, dec;		/* refracted sky loc */
    double resid;		/* residual to model */
    int marked;			/* set when henc/denc have been set */
} ObjInfo;
static ObjInfo objs[MAXOBJS];	/* one for each possible object */
static int nobjs;		/* number of objs[] in use */

/* toggle whether the axis dialog is up */
void
axes_manage ()
{
	if (!cal_w)
	    mkCalGUI();

	if (XtIsManaged(cal_w)) {
	    XtUnmanageChild (cal_w);
	} else
	    XtManageChild (cal_w);
}

/* message has arrived from xephem.
 * keep it if we are interested.
 * return 0 if we were not interested, -1 if we took it.
 */
int
axes_xephemSet (char *buf)
{
	Now *np = &telstatshmp->now;
	ObjInfo *oip;
	int newi, i;

	/* care only if we can use it now */
	if (!cal_w || !XtIsManaged(cal_w))
	    return (0);		/* no, we don't care */

	/* candidate is next unmarked slot */
	newi = (nobjs == 0 || objs[nobjs-1].marked) ? nobjs : nobjs-1;

	/* use objs[newi], unless no more room */
	if (newi == MAXOBJS) {
	    wlprintf (msg_w, "No more room.");
	    return (-1);
	}
	oip = &objs[newi];

	/* check the candidate */
	if (db_crack_line (buf, &oip->obj, NULL) < 0) {
	    wlprintf (msg_w, "Bad object: %s", buf);
	    return (-1);
	}
	if (strcmp (oip->obj.o_name, "TelAnon") == 0) {
	    wlprintf (msg_w, "Please choose a catalog object.");
	    return (-1);
	}

	/* sanity-check proximity with others now */
	oip->obsmjd = mjd;
	findHD (newi); 
	for (i = 0; i < newi; i++)
	    if (tooClose (newi, i) < 0) {
		XBell (XtDisplay(toplevel_w), 100);
		return (-1);
	    }

	/* guess flip and reject if near meridian on Germ Eq. */
	if (telstatshmp->tax.GERMEQ) {
	    if (fabs(oip->ha) < hrrad(0.5)) {
		XBell (XtDisplay(toplevel_w), 100);
		wlprintf (msg_w, "Choose an object farther from the meridian.");
		return (-1);
	    }
	    telstatshmp->tax.GERMEQ_FLIP = oip->ha > 0.0;
	}

	/* ok, go with newi */
	oip->marked = 0;
	wlprintf (lbl_w[newi], "%s", oip->obj.o_name);
	wlprintf (resid_w[newi], blank);
	wlprintf (msg_w,"Center %s using Paddle, then Mark; or reselect.",
							    oip->obj.o_name);
	nobjs = newi+1;
	return (-1);
}

/* build the user interface */
static void
mkCalGUI()
{
	typedef struct {
	    char *name;
	    XtCallbackProc cb;
	    char *tip;
	} Ctrl;
	static Ctrl ctrls[] = {
	    {"Reset",  resetCB,  "Erase all progress and start over"},
	    {"Undo",   undoCB,   "Undo last mark or remove last star"},
	    {"Solve",  solveCB,  "Use all marked stars and attempt a solution"},
	    {"Install",installCB,"Save the curret solution and make it active"},
	    {"Close",  closeCB,  "Close this dialog and do nothing"},
	};
	Widget rc_w;
	Widget sep_w, w;
	Arg args[20];
	int n;
	int i;

	n = 0;
	XtSetArg (args[n], XmNhorizontalSpacing, 10); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_GROW); n++;
	XtSetArg (args[n], XmNfractionBase, 3*XtNumber(ctrls)+1); n++;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNtitle, "Axis Calibration"); n++;
	cal_w = XmCreateFormDialog (toplevel_w, "F", args, n);

	/* stack the majority of stuff in a RC */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	rc_w = XmCreateRowColumn (cal_w, "RC", args, n);
	XtManageChild (rc_w);

	/* messages label */

	n = 0;
	msg_w = XmCreateLabel (rc_w, "IL", args, n);
	wlprintf (msg_w, intro_msg);
	XtManageChild (msg_w);

	/* create the main table */

	mkTable (rc_w);

	n = 0;
	sep_w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (sep_w);

	n = 0;
	w = XmCreateLabel (rc_w, "SL", args, n);
	wlprintf (w, "Solution:");
	wtip (w,
	    "This area shows the current candidate telescope axis solution");
	XtManageChild (w);

	n = 0;
	new0_w = XmCreateLabel (rc_w, "SL", args, n);
	XtManageChild (new0_w);
	new1_w = XmCreateLabel (rc_w, "SL", args, n);
	XtManageChild (new1_w);
	new2_w = XmCreateLabel (rc_w, "SL", args, n);
	XtManageChild (new2_w);

	n = 0;
	sep_w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (sep_w);

	/* put the buttons across the bottom */

	for (i = 0; i < XtNumber(ctrls); i++) {

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 1+i*3); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 3+i*3); n++;
	    w = XmCreatePushButton (cal_w, ctrls[i].name, args, n);
	    XtAddCallback (w, XmNactivateCallback, ctrls[i].cb, 0);
	    wtip (w, ctrls[i].tip);
	    XtManageChild (w);
	}

	/* clear solution */
	showSolution(1);
}

/* make the list of controls/lights for each object off rc_w */
static void
mkTable (Widget rc_w)
{
	Arg args[20];
	Widget f_w;
	Widget w;
	int n;
	int i;

	n = 0;
	f_w = XmCreateForm (rc_w, "EF", args, n);
	XtManageChild (f_w);

	/* Mark button */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, CHCINDENT); n++;
	w = XmCreatePushButton (f_w, " Mark ", args, n);
	XtAddCallback (w, XmNactivateCallback, markCB, NULL);
	wtip (w, "Press when new star in list is centered in finder");
	XtManageChild (w);

	/* add one row per potential object */

	for (i = 0; i < MAXOBJS; i++)
	    mkRow (rc_w, i);
}

/* make a horizontal label/light/label row for object i */
static void
mkRow (Widget p_w, int i)
{
	Widget f_w, l_w, w;
	Arg args[20];
	int n;

	n = 0;
	f_w = XmCreateForm (p_w, "LP", args, n);
	XtManageChild (f_w);

	l_w = mkLight (f_w);
	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, CHCINDENT); n++;
	XtSetValues (l_w, args, n);
	setLt (l_w, LTIDLE);
	XtManageChild (l_w);
	lite_w[i] = l_w;

	n = 0;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, l_w); n++;
	XtSetArg (args[n], XmNrightOffset, 6); n++;
	w = XmCreateLabel (f_w, "RL", args, n);
	wlprintf (w, blank);
	XtManageChild (w);
	resid_w[i] = w;

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, l_w); n++;
	XtSetArg (args[n], XmNleftOffset, 6); n++;
	w = XmCreateLabel (f_w, "EL", args, n);
	wlprintf (w, blank);
	XtManageChild (w);
	lbl_w[i] = w;
}

/* callback when the undo PB is pushed */
static void
undoCB (Widget w, XtPointer client, XtPointer call)
{
	if (nobjs > 0) {
	    int i = nobjs-1;
	    ObjInfo *oip = &objs[i];

	    if (oip->marked) {
		oip->marked = 0;
		setLt (lite_w[i], LTIDLE);
		wlprintf (msg_w, "Redo %s, reselect, or Undo more.",
							    oip->obj.o_name);
	    } else {
		wlprintf (lbl_w[i], blank);
		wlprintf (resid_w[i], blank);
		nobjs--;

		switch (nobjs) {
		case 0:
		    wlprintf (msg_w, intro_msg);
		    break;;
		case 1:
		    wlprintf (msg_w, "1 star is marked. Please do another.");
		    break;;
		case 2:
		    wlprintf (msg_w,
		    "2 stars are marked. More is better, but can Solve now.");
		    break;;
		default:
		    wlprintf (msg_w,
			    "%d stars are marked. Do another or Solve.", nobjs);
		    break;;
		}
	    }
	} else
	    wlprintf (msg_w, intro_msg);
}

/* callback when the mark PB is pushed */
static void
markCB (Widget w, XtPointer client, XtPointer call)
{
	Now *np = &telstatshmp->now;
	ObjInfo *oip;
	char *oname;
	int newi;

	/* object must already be defined at least */
	if (nobjs == 0) {
	    wlprintf (msg_w,
			"Nothing to mark.. first select and center a star.");
	    return;
	}

	/* save time and encoders, mark */
	newi = nobjs-1;
	oip = &objs[newi];
	oip->obsmjd = mjd;
	oip->henc = telstatshmp->minfo[TEL_HM].cpos;
	oip->denc = telstatshmp->minfo[TEL_DM].cpos;
	findHD (newi);
	if (telstatshmp->tax.GERMEQ && oip->ha > 0) {
	    /* simulate flipped if in west, just as in tel_realxy2ideal() */
	    double X = oip->henc;
	    double Y = oip->denc;

	    X += PI;
	    Y = PI - 2*telstatshmp->tax.YC - Y;
	    while (X > 2*PI)
		X -= 2*PI;
	    while (X < -2*PI)
		X += 2*PI;
	    while (Y > 2*PI)
		Y -= 2*PI;
	    while (Y < -2*PI)
		Y += 2*PI;

	    oip->henc = X;
	    oip->denc = Y;
	}
	oip->marked = 1;
	setLt (lite_w[newi], LTOK);
	oname = oip->obj.o_name;

	/* new prompt */
	if (nobjs < 2)
	    wlprintf (msg_w, "%s is marked. Now do another.", oname);
	else if (nobjs < 3)
	    wlprintf (msg_w, "%s is marked. More is better, but can Solve now.",
								oname);
	else if (nobjs < MAXOBJS)
	    wlprintf (msg_w, "%s is marked. Do another or Solve.", oname);
	else {
	    wlprintf(msg_w, "%s is marked. Reached max.. Solving..", oname);
	    solveNew ();
	}
}

/* called from Restart button */
static void
resetCB (Widget w, XtPointer client, XtPointer call)
{
	int i;

	for (i = 0; i < MAXOBJS; i++) {
	    wlprintf (resid_w[i], blank);
	    setLt (lite_w[i], LTIDLE);
	    wlprintf (lbl_w[i], blank);
	    objs[i].marked = 0;
	}
	nobjs = 0;
	showSolution(1);
	wlprintf (msg_w, intro_msg);
}

/* called from Solve button */
static void
solveCB (Widget w, XtPointer client, XtPointer call)
{
	solveNew ();
}

/* called from Install button */
static void
installCB (Widget w, XtPointer client, XtPointer call)
{
	if (!taxok) {
	    wlprintf (msg_w, "Can not install until a solution is found.");
	    return;
	}

	if (!rusure (toplevel_w, "Install this new calibration solution now"))
	    return;

	if (installNew() == 0) {
	    wlprintf (msg_w, "New calibration solution installed and active.");
	    msg ("Installed new calibration model: %g %g %g %g %g",
				    tax.HT, tax.DT, tax.XP, tax.YC, tax.NP);
	}
}

/* called from Close button */
static void
closeCB (Widget w, XtPointer client, XtPointer call)
{
	XtUnmanageChild (cal_w);
}

/* return -1 if stars i and j are too close to be used, else 0 if ok.
 * N.B. we assume findHD() has already been called for object i.
 */
static int
tooClose (int i, int j)
{
	char *ni = objs[i].obj.o_name;
	char *nj = objs[j].obj.o_name;

	/* insure emphemeris is up to date */
	findHD (j); 

	/* check each dimension separately */
	if (delra(objs[i].ha - objs[j].ha) < MINDHA) {
	    wlprintf (msg_w, "%s and %s are too close in Hour angle.", ni, nj);
	    return (-1);
	}
	if (fabs(objs[i].dec - objs[j].dec) < MINDDEC) {
	    wlprintf (msg_w, "%s and %s are too close in Dec.", ni, nj);
	    return (-1);
	}

	return (0);
}

/* show the new values, or just the labels if empty */
static void
showSolution(int empty)
{
	char htbuf[32];
	char dtbuf[32];
	char xpbuf[32];
	char ycbuf[32];
	char npbuf[32];
	char dAltbuf[32];
	char dAzbuf[32];

	if (empty) {
	    (void) sprintf (htbuf, "%*s",  12, blank);
	    (void) sprintf (dtbuf, "%*s",  10, blank);
	    (void) sprintf (npbuf, "%*s",  10, blank);

	    (void) sprintf (xpbuf, "%*s",  10, blank);
	    (void) sprintf (ycbuf, "%*s",  10, blank);

	    (void) sprintf (dAltbuf,"%*s", 10, blank);
	    (void) sprintf (dAzbuf, "%*s", 10, blank);
	} else {
	    fs_sexa (htbuf, radhr(tax.HT),  4, 36000);
	    fs_sexa (dtbuf, raddeg(tax.DT), 4, 3600);
	    fs_sexa (npbuf, raddeg(tax.NP), 4, 3600);

	    fs_sexa (xpbuf, raddeg(tax.XP), 4, 3600);
	    fs_sexa (ycbuf, raddeg(tax.YC), 4, 3600);

	    fs_sexa (dAltbuf, raddeg((PI/2-tax.DT)*cos(tax.HT)), 4, 3600);
	    fs_sexa (dAzbuf, -raddeg((PI/2-tax.DT)*sin(tax.HT)), 4, 3600);
	}

	wlprintf (new0_w, "  T Pole :     HA =%s  Dec = %s  NonP = %s", htbuf,
								dtbuf, npbuf);
	wlprintf (new1_w, "  Ax Ref :   XPol =%s   YHom = %s", xpbuf, ycbuf);
	wlprintf (new2_w, "Pole Off :   dAlt =%s    dAz = %s", dAltbuf, dAzbuf);

	/* save whether computed */
	taxok = !empty;
}

/* install the new values.
 * return 0 if ok, else -1
 */
static int
installNew()
{
	char buf[1024];
	int i;

	/* update home.cfg */
	i = 0;
	(void) sprintf (buf, "%10.7f", tax.HT);
	i += writeCfgFile (hcfgname, "HT", buf, NULL);
	(void) sprintf (buf, "%10.7f", tax.DT);
	i += writeCfgFile (hcfgname, "DT", buf, NULL);
	(void) sprintf (buf, "%10.7f", tax.XP);
	i += writeCfgFile (hcfgname, "XP", buf, NULL);
	(void) sprintf (buf, "%10.7f", tax.YC);
	i += writeCfgFile (hcfgname, "YC", buf, NULL);
	(void) sprintf (buf, "%10.7f", tax.NP);
	i += writeCfgFile (hcfgname, "NP", buf, NULL);
	if (i != 0) {
	    wlprintf (msg_w, "%s: %s", hcfgname, strerror(errno));
	    return (-1);
	}

	/* inform the Tel fifo to reread the config file */
	fifoMsg (Tel_Id, "%s", "Reset");

	/* done */
	return (0);
}

/* qsort-style function to compare residuals of two objects in ascending
 * order
 */
static int
resid_qf (const void *p1, const void *p2)
{
	double r1 = ((ObjInfo *)p1)->resid;
	double r2 = ((ObjInfo *)p2)->resid;
	double diff = r1 - r2;

	if (diff < 0)
	    return (-1);
	if (diff > 0)
	    return (1);
	return (0);
}

/* find a best solution */
static void
solveNew()
{
	double H[MAXOBJS], D[MAXOBJS], X[MAXOBJS], Y[MAXOBJS], resid[MAXOBJS];
	double ftol;
	time_t t;
	int i, s;

	/* need >= 2 *marked* objects */
	i = nobjs;
	if (i > 0 && !objs[i-1].marked)
	    i--;
	if (i < 2) {
	    wlprintf (msg_w, "Solver requires at least two marked stars.");
	    return;
	}

	/* discard and erase last if not marked */
	if (!objs[nobjs-1].marked)
	    wlprintf (lbl_w[--nobjs], blank);

	/* compute HA and Dec of all objects and fill the aux arrays */
	for (i = 0; i < nobjs; i++) {
	    ObjInfo *oip = &objs[i];
	    findHD (i);	/* just to be sure */
	    H[i] = oip->ha;
	    D[i] = oip->dec;
	    X[i] = oip->henc;
	    Y[i] = oip->denc;
	}

	/* dump for posterity */
	time(&t);
	printf ("H/D/X/Y, suitable for use with mntmodel, at UT %s",
							asctime(gmtime(&t)));
	for (i = 0; i < nobjs; i++) {
	    ObjInfo *oip = &objs[i];
	    printf ("%10.7f %10.7f %10.7f %10.7f %s\n", oip->ha, oip->dec,
					oip->henc, oip->denc, oip->obj.o_name);
	}

#ifdef TESTGUI
	/* simulate a solution just to test the display */
	tax.HT = hrrad(1.);
	tax.DT = degrad(2.);
	tax.XP = degrad(3.);
	tax.YC = degrad(4.);
	if (nobjs > 2) {
	    tax.NP = degrad(5.);
	    for (i = 0; i < nobjs; i++)
		resid[i] = degrad(1)*rand()/RAND_MAX;
	} else {
	    tax.NP = 0;
	    for (i = 0; i < nobjs; i++)
		resid[i] = 0.0;
	}
	s = 0;
#else
	/* solve.. set initial conditions from currently installed values */
	ftol = 1./(HMOT->estep <= DMOT->estep ? HMOT->estep : DMOT->estep);
	tax = telstatshmp->tax;
	tax.GERMEQ_FLIP = 0;	/* already done by-hand here when marked */
	s = tel_solve_axes (H, D, X, Y, nobjs, ftol, &tax, resid);
#endif

	/* explain if failed */
	if (s < 0) {
	    wlprintf (msg_w,
		    "%d-star solution failed... see xobs.log for raw values.",
				    nobjs);
	    return;
	}

	/* show new solution */
	showSolution(0);

	/* sort by residual */
	for (i = 0; i < nobjs; i++)
	    objs[i].resid = resid[i];
	qsort ((void *)objs, nobjs, sizeof(ObjInfo), resid_qf);

	/* reshow, with residuals */
	for (i = 0; i < nobjs; i++) {
	    ObjInfo *oip = &objs[i];
	    wlprintf (resid_w[i], "%6.1f\"", 3600.0*raddeg(oip->resid));
	    wlprintf (lbl_w[i], "%s", oip->obj.o_name);
	}

	/* suggest next step */
	if (nobjs < MAXOBJS)
	    wlprintf (msg_w,
	      "%d-star solution... add more, Undo, or Install if like.",nobjs);
	else
	    wlprintf (msg_w,
		"%d-star solution... Undo, or Install if like.", nobjs);
}

/* find the _refracted_ HA and Dec of the given object at the given time.
 */
static void
findHD (int i)
{
	/* make a local copy so we can fiddle with the time. */
	Now n = telstatshmp->now;
	Obj *op = &objs[i].obj;

	n.n_mjd = objs[i].obsmjd;
	n.n_epoch = EOD;
	(void) obj_cir (&n, op);

	aa_hadec (n.n_lat, op->s_alt, op->s_az, &objs[i].ha, &objs[i].dec);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: calaxes.c,v $ $Date: 2001/04/19 21:12:06 $ $Revision: 1.1.1.1 $ $Name:  $"};
