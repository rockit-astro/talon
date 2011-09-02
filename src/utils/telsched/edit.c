/* provide the editing facility for each run.
 * we allow any number of runs to be being edited at a time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>
#include <Xm/TextF.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "xtools.h"
#include "configfile.h"
#include "strops.h"
#include "scan.h"

#include "telsched.h"

extern int getExtVal(char* bp, Scan* sp); // hmm. should put in header?  Really, it's a local function, but we use it here.

typedef struct {
    Widget schedfn_w;
    Widget source_w;
    Widget comment_w;
    Widget title_w;
    Widget observer_w;
    Widget imagedn_w;
    Widget yoff_w;

    Widget rao_w;
    Widget deco_w;
    Widget sx_w, sy_w, sw_w, sh_w;
    Widget binx_w, biny_w;
    Widget ccdcalib_w;

    Widget imagefn_w;
    Widget shutter_w;
    Widget compress_w;
    Widget filter_w;
    Widget dur_w;
    Widget priority_w;
    Widget lststart_w;
    Widget lstdelta_w;
    Widget date_w;

    // STO: Including editable fields for extra items added
    Widget extAct_w;
    Widget extVal_w;
    Widget targPixX_w;
    Widget targPixY_w;

    /* if editing: op points back into the workop[] array so we can modify it
     *             and the newobs is unused;
     *      else : op points to newobs so we can create from it.
     * N.B. this rule is encapsulated in the EICREAT macro: use it!
     */
    Obs newobs;
    Obs *op;

} EditInfo;

#define	EICREAT(eip)	((eip)->op == &(eip)->newobs)

static Widget createMenu (Obs *op);
static Widget makeEditRow (char *label, Widget sf_w, int txtcols, int editable);
static void fillMenu (EditInfo *eip);
static void initNew (Obs *op);
static int readObs (EditInfo *eip);
static void ok_cb (Widget w, XtPointer client, XtPointer data);
static void cancel_cb (Widget w, XtPointer client, XtPointer data);
static void destroy_cb (Widget w, XtPointer client, XtPointer data);
static void set_utcstart (Now *np, Obs *op, int nop);

/* client data codes for the ctrl_cb callback. */
enum {OK_CTRL, CANCEL_CTRL};

/* used to display fields for which there is nothing interesting */
static char dont_care[] = "Any";

/* used when actually creating a new scan, not editing an existing one. */

/* create, manage and save widget id of an editing shell for the given Obs
 * which is already part of the working list being maintained.
 * shell is destroyed if observing list is changed.
 * we destroy ourselves when we close.
 * N.B. op is known to the caller so we can not move it.
 */
void
editRun (op)
Obs *op;
{
	XtManageChild (createMenu(op));
}

/* create a new edit dialog but for the purposes of creating a new Obs.
 */
void
newRun ()
{
	XtManageChild (createMenu(NULL));
}

/* if op: make a dialog to edit an Obs already on the main workop list.
 * else : make a dialog to create a new Obs entirely, using eip->newobs.
 */
static Widget
createMenu (op)
Obs *op;
{
	typedef struct {
	    char *name;
	    void (*cb)();
	} Ctrl;
	static Ctrl ctrls[] = {
	    {"Apply", ok_cb},
	    {"Close", cancel_cb}
	};
	Arg args[20];
	char title[256];
	Widget sf_w;
	Widget rc_w;
	Widget sep_w;
	EditInfo *eip;
	int n;
	int i;

	/* malloc an EditInfo and save (or set) op */
	eip = (EditInfo *) calloc (1, sizeof(EditInfo));
	if (op)
	    eip->op = op;
	else {
	    eip->op = op = &eip->newobs;
	    initNew(op);
	}

	/* make the parent form dialog.
	 * arrange to be notified if we are ever destroyed so we can
	 * reclaim the eip memory.
	 */
	if (EICREAT(eip))
	    (void) sprintf (title, "telsched Create new scan");
	else
	    (void) sprintf (title, "telsched Edit %.*s", (int)sizeof(title),
							op->scan.obj.o_name);
	n = 0;
	XtSetArg (args[n], XmNtitle, title); n++;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 10); n++;
	XtSetArg (args[n], XmNfractionBase, 7); n++;
	sf_w = XmCreateFormDialog (toplevel_w, "Edit", args, n);

	/* put the shell in op->editsh so caller can destroy us easily */
	for (op->editsh = sf_w; !XtIsShell(op->editsh); )
	    op->editsh = XtParent(op->editsh);
	XtAddCallback (op->editsh,XmNdestroyCallback,destroy_cb,(XtPointer)eip);

	/* make and init the longer label/text widgets in one row/column */

	rc_w = XtVaCreateManagedWidget ("WRC", xmRowColumnWidgetClass, sf_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNspacing, 2,
	    NULL);

	    eip->schedfn_w =  makeEditRow ("Sched file:", rc_w, 0,EICREAT(eip));
	    eip->source_w =   makeEditRow ("    Source:", rc_w, 0, 1);
	    eip->comment_w =  makeEditRow ("   Comment:", rc_w, 0, 1);
	    eip->title_w =    makeEditRow ("     Title:", rc_w, 0, 1);
	    eip->observer_w = makeEditRow ("  Observer:", rc_w, 0, 1);
	    eip->imagedn_w =  makeEditRow (" Image Dir:", rc_w, 0, 1);
	    eip->yoff_w = makeEditRow ("   Why off:", rc_w, 0, 0);
	    if (EICREAT(eip))
		XtUnmanageChild (XtParent(eip->yoff_w));

	/* make and init the shorter label/text widgets in another row/column */

	rc_w = XtVaCreateManagedWidget ("SRC", xmRowColumnWidgetClass, sf_w,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, rc_w,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNpacking, XmPACK_COLUMN,
	    XmNnumColumns, 2,
	    XmNspacing, 2,
	    NULL);

	    eip->rao_w =      makeEditRow (" RA Offset:", rc_w, 12, 1);
	    eip->deco_w =     makeEditRow ("Dec Offset:", rc_w, 12, 1);
	    eip->sx_w =       makeEditRow ("SubImage X:", rc_w, 12, 1);
	    eip->sy_w =       makeEditRow ("SubImage Y:", rc_w, 12, 1);
	    eip->sw_w =       makeEditRow ("SubImage W:", rc_w, 12, 1);
	    eip->sh_w =       makeEditRow ("SubImage H:", rc_w, 12, 1);
	    eip->binx_w =     makeEditRow ("ImageBin X:", rc_w, 12, 1);
	    eip->biny_w =     makeEditRow ("ImageBin Y:", rc_w, 12, 1);
        eip->targPixX_w = makeEditRow ("Targ Pix X:", rc_w, 12, 1);
        eip->targPixY_w = makeEditRow ("Targ Pix Y:", rc_w, 12, 1);
	    eip->ccdcalib_w = makeEditRow (" CCD Calib:", rc_w, 12, 1);
        eip->extAct_w =   makeEditRow ("Ext Action:", rc_w, 12, 1);
        eip->extVal_w =   makeEditRow ("ExtAct Val:", rc_w, 12, 1);

	    eip->imagefn_w =  makeEditRow (" File name:", rc_w, 12, 1);
	    eip->compress_w = makeEditRow ("  Compress:", rc_w, 12, 1);
	    eip->filter_w =   makeEditRow ("    Filter:", rc_w, 12, 1);
	    eip->shutter_w =  makeEditRow ("   Shutter:", rc_w, 12, 1);
	    eip->dur_w =      makeEditRow ("  Duration:", rc_w, 12, 1);
	    eip->priority_w = makeEditRow ("  Priority:", rc_w, 12, 1);
	    eip->lststart_w = makeEditRow (" Start LST:", rc_w, 12, 1);
	    eip->lstdelta_w = makeEditRow (" LST Delta:", rc_w, 12, 1);
	    eip->date_w =     makeEditRow ("   UT Date:", rc_w, 12, 1);

	sep_w = XtVaCreateManagedWidget ("Sep", xmSeparatorWidgetClass, sf_w,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, rc_w,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    NULL);

	for (i = 0; i < XtNumber(ctrls); i++) {
	    Ctrl *cp = &ctrls[i];
	    Widget w;

	    w= XtVaCreateManagedWidget (cp->name, xmPushButtonWidgetClass, sf_w,
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, sep_w,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_POSITION,
		XmNleftPosition, 1 + i*3,
		XmNrightAttachment, XmATTACH_POSITION,
		XmNrightPosition, 3 + i*3,
		NULL);
	    XtAddCallback (w, XmNactivateCallback, cp->cb, (XtPointer)eip);
	}

	fillMenu (eip);

	return (sf_w);
}

/* make the label/text field row.
 * return the text widget.
 */
static Widget
makeEditRow (label, rc_w, txtcols, editable)
char *label;
Widget rc_w;
int txtcols;
int editable;
{
	Widget f_w;
	Arg args[20];
	Widget l_w, t_w;
	int n;

	n = 0;
	f_w = XtCreateManagedWidget ("ROW", xmFormWidgetClass, rc_w, args, n);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	l_w = XtCreateManagedWidget ("ELBL", xmLabelWidgetClass, f_w, args, n);
	wlprintf (l_w, "%s", label);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, l_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNeditable, editable); n++;
	if (txtcols > 0) {
	    XtSetArg (args[n], XmNcolumns, txtcols); n++;
	}
	t_w = XtCreateManagedWidget ("ETF", xmTextFieldWidgetClass, f_w,args,n);

	return (t_w);
}

/* set up op to be a reasonable starting point for a new record */
static void
initNew (Obs *op)
{
	Scan *sp = &op->scan;

	initObs (op);

	ACPYZ (sp->schedfn, "HandMade.sch");
	ACPYZ (sp->imagedn, imdir);
	ACPYZ (sp->obj.o_name, "<Your source>");
	ACPYZ (sp->comment, "<Your comment>");
	ACPYZ (sp->title, "<Your title>");
	ACPYZ (sp->observer, "<Your name>");

	(void) ccdStr2Calib ("CATALOG", &sp->ccdcalib);
	(void) ccdStr2SO ("Open", &sp->shutter);
	sp->compress = 0;
	sp->sx = 0;
	sp->sy = 0;
	sp->sw = DEFIMW;
	sp->sh = DEFIMH;
	sp->binx = DEFBIN;
	sp->biny = DEFBIN;
	sp->filter = FDEFLT;
	sp->dur = 30;
	sp->startdt = (int)floor(LSTDELTADEF*60+.5);
	sp->priority = 100;
}

static void
fillMenu (eip)
EditInfo *eip;
{
	Obs *op = eip->op;
	Scan *sp = &op->scan;
	char buf[32];

	wtprintf (eip->schedfn_w, "%s", sp->schedfn);
	wtprintf (eip->source_w, "%s", sp->obj.o_name);
	wtprintf (eip->comment_w, "%s", sp->comment);
	wtprintf (eip->title_w, "%s", sp->title);
	wtprintf (eip->observer_w, "%s", sp->observer);
	wtprintf (eip->imagedn_w, "%s", sp->imagedn);
	wtprintf (eip->imagefn_w, "%s", sp->imagefn);
	wtprintf (eip->yoff_w, "%s", op->yoff);

	wtprintf (eip->ccdcalib_w, "%s", ccdCalib2Str(sp->ccdcalib));
	wtprintf (eip->compress_w, "%d", sp->compress);
	wtprintf (eip->shutter_w, "%s", ccdSO2Str(sp->shutter));
	wtprintf (eip->sx_w, "%d", sp->sx);
	wtprintf (eip->sy_w, "%d", sp->sy);
	wtprintf (eip->sw_w, "%d", sp->sw);
	wtprintf (eip->sh_w, "%d", sp->sh);
	wtprintf (eip->binx_w, "%d", sp->binx);
	wtprintf (eip->biny_w, "%d", sp->biny);
	wtprintf (eip->filter_w, "%c", sp->filter);
	wtprintf (eip->dur_w, "%g", sp->dur);
	wtprintf (eip->priority_w, "%d", sp->priority);

	fs_sexa (buf, radhr(sp->rao), 2, 36000);
	wtprintf (eip->rao_w, "%s", buf);

	fs_sexa (buf, raddeg(sp->deco), 2, 36000);
	wtprintf (eip->deco_w, "%s", buf);

	if (op->lststart == NOTIME)
	    wtprintf (eip->lststart_w, "%s", dont_care);
	else {
	    fs_sexa (buf, op->lststart, 2, 3600);
	    wtprintf (eip->lststart_w, "%s", buf);
	}

	/* user wants to see minutes */
	wtprintf (eip->lstdelta_w, "%g", sp->startdt/60.);

	if (op->date[0] == '\0')
	    wtprintf (eip->date_w, "%s", dont_care);
	else
	    wtprintf (eip->date_w, "%s", op->date);

    // STO: New extras
    wtprintf(eip->extAct_w, "%s", ccdExtAct2Str(sp->ccdcalib));
    wtprintf(eip->extVal_w, "%s", extActValueStr(sp));
}

/* given the widgets in eip, fill in eip->op.
 * return 0 if ok, else write msg() and return -1.
 */
static int
readObs (eip)
EditInfo *eip;
{
	Obs *op = eip->op;
	Scan *sp = &op->scan;
	char *s;

	get_something (eip->schedfn_w, XmNvalue, (char *)&s);
	ACPYZ (sp->schedfn, s);
	XtFree (s);

	get_something (eip->source_w, XmNvalue, (char *)&s);
	ACPYZ (sp->obj.o_name, s);
	XtFree (s);

	get_something (eip->comment_w, XmNvalue, (char *)&s);
	ACPYZ (sp->comment, s);
	XtFree (s);

	get_something (eip->title_w, XmNvalue, (char *)&s);
	ACPYZ (sp->title, s);
	XtFree (s);

	get_something (eip->observer_w, XmNvalue, (char *)&s);
	ACPYZ (sp->observer, s);
	XtFree (s);

	get_something (eip->imagedn_w, XmNvalue, (char *)&s);
	ACPYZ (sp->imagedn, s);
	XtFree (s);

	get_something (eip->imagefn_w, XmNvalue, (char *)&s);
	ACPYZ (sp->imagefn, s);
	XtFree (s);

	get_something (eip->shutter_w, XmNvalue, (char *)&s);
	if (ccdStr2SO (s, &sp->shutter) < 0) {
	    illegalCCDSO (s);
	    XtFree (s);
	    return (-1);
	}
	XtFree (s);

	get_something (eip->ccdcalib_w, XmNvalue, (char *)&s);
	if (ccdStr2Calib (s, &sp->ccdcalib) < 0) {
	    illegalCCDCalib (s);
	    XtFree (s);
	    return (-1);
	}
	XtFree (s);

	get_something (eip->compress_w, XmNvalue, (char *)&s);
	sp->compress = (s[0]=='y' || s[0]=='Y') ? 1 : atoi (s);
	XtFree (s);

	get_something (eip->sx_w, XmNvalue, (char *)&s);
	sp->sx = atoi (s);
	XtFree (s);

	get_something (eip->sy_w, XmNvalue, (char *)&s);
	sp->sy = atoi (s);
	XtFree (s);

	get_something (eip->sw_w, XmNvalue, (char *)&s);
	sp->sw = atoi (s);
	XtFree (s);

	get_something (eip->sh_w, XmNvalue, (char *)&s);
	sp->sh = atoi (s);
	XtFree (s);

	get_something (eip->binx_w, XmNvalue, (char *)&s);
	sp->binx = atoi (s);
	XtFree (s);

	get_something (eip->biny_w, XmNvalue, (char *)&s);
	sp->biny = atoi (s);
	XtFree (s);

	get_something (eip->filter_w, XmNvalue, (char *)&s);
	if (legalFilters (1, &s) == 0) {
	    sp->filter = s[0];
	} else {
	    XtFree (s);
	    return (-1);
	}
	XtFree (s);

	get_something (eip->dur_w, XmNvalue, (char *)&s);
	sp->dur = atof (s);
	XtFree (s);

	get_something (eip->priority_w, XmNvalue, (char *)&s);
	sp->priority = atoi (s);
	XtFree (s);

	get_something (eip->rao_w, XmNvalue, (char *)&s);
	if (scansex (s, &sp->rao) < 0) {
	    msg ("Bogus RAOFFSET format: %s", s);
	    XtFree (s);
	    return (-1);
	}
	sp->rao = hrrad(sp->rao);
	XtFree (s);

	get_something (eip->deco_w, XmNvalue, (char *)&s);
	if (scansex (s, &sp->deco) < 0) {
	    msg ("Bogus DECOFFSET format: %s", s);
	    XtFree (s);
	    return (-1);
	}
	sp->deco = degrad(sp->deco);
	XtFree (s);

	get_something (eip->lststart_w, XmNvalue, (char *)&s);
	if (strncasecmp (s, dont_care, strlen(dont_care)) == 0) {
	    op->lststart = NOTIME;
	} else if (scansex (s, &op->lststart) < 0) {
	    msg ("Bogus LSTSTART format: %s", s);
	    XtFree (s);
	    return (-1);
	}
	XtFree (s);

	get_something (eip->lstdelta_w, XmNvalue, (char *)&s);
	sp->startdt = (int)floor(atof(s)*60+.5); /* user entered minutes */
	XtFree (s);

	get_something (eip->date_w, XmNvalue, (char *)&s);
	if (strncasecmp (s, dont_care, strlen(dont_care)) == 0)
	    op->date[0] = '\0';
	else if (dateformatok (s) < 0) {
	    msg (" Bad UTSTART format (want MM/DD/YYYY): %s", s);
	    XtFree (s);
	    return (-1);
	} else
	    ACPYZ (op->date, s);
	XtFree (s);

	/* pure camera cal scans must have a time set, and a fake obj */
	if (sp->ccdcalib.data == CD_NONE) {
	    if (op->lststart == NOTIME) {
		msg ("Pure camera calibration scan must have a start time.");
		return (-1);
	    }
	    sp->obj.o_type = FIXED;
	    sp->obj.f_RA = 0.0;
	    sp->obj.f_dec = 0.0;
	    sp->obj.f_epoch = J2000;
	}

    // STO: Extras
        get_something(eip->extAct_w, XmNvalue, (char *)&s);
        int rt = ccdStr2ExtAct(s, &sp->ccdcalib);
        XtFree (s);
        if(rt == 0)
        {
            get_something(eip->extVal_w, XmNvalue, (char *)&s);
            getExtVal(s, sp);
            XtFree(s);
        }

	return (0);
}

/* called when the Ok control button is activated.
 * client contains the EditInfo pointer.
 */
/* ARGSUSED */
static void
ok_cb (w, client, data)
Widget w;
XtPointer client;
XtPointer data;
{
	EditInfo *eip = (EditInfo *) client;
	Obs newobs;	/* local temp copy in case new stuff is bad */
	Obs *origop;	/* save eip->op */
	int ok;

	/* read user fields into a copy in case we find something wrong */
	origop = eip->op;
	cpyObs (&newobs, origop);
	eip->op = &newobs;
	ok = !readObs(eip);
	eip->op = origop;

	if (ok) {
	    /* fetch new object if source changed or creating unless this is
	     * just a camera calibration scan.
	     */

	    if (newobs.scan.ccdcalib.data != CD_NONE &&
			(EICREAT(eip) || strcasecmp (newobs.scan.obj.o_name,
						    origop->scan.obj.o_name))) {
		if (searchCatEntry (&newobs) < 0) {
		    /* searchCatEntry prints a diagnostic */
		    ok = 0; /* whoops -- new source is unknown */
		} else {
		    msg ("%s found and set", newobs.scan.obj.o_name);
		}
	    } else {
		/* just state things have gone well */
		msg ("`%s' has been %s", newobs.scan.obj.o_name,
					    EICREAT(eip) ? "added" : "edited");
	    }

	    /* check for a new lststart and fill utcstart */
	    if (ok && newobs.lststart != NOTIME)
		set_utcstart (&now, &newobs, 1);

	}

	watch_cursor(1);

	if (ok) {
	    if (EICREAT(eip)) {
		/* add the new entry to the real workop list.
		 * 0 editsh so it doesn't look like it is being edited.
		 */
		newobs.editsh = (Widget)0;
		addSchedEntries (&newobs, 1);
	    } else {
		/* make the changes permanent in calling program's list */
		computeCir (&newobs);
		cpyObs (origop, &newobs);
	    }

	    /* update displayed info */
	    updateScrolledList();
	} else {
	    /* undo */
	    fillMenu (eip);
	}

	watch_cursor(0);
}

/* called when the Cancel control button is activated.
 * client contains the EditInfo pointer.
 */
/* ARGSUSED */
static void
cancel_cb (w, client, data)
Widget w;
XtPointer client;
XtPointer data;
{
	EditInfo *eip = (EditInfo *) client;
	Widget editsh;

	/* destroy this edit menu (and eip) and leave that known to
	 * calling program by virtul of its editsh being 0.
	 */
	editsh = eip->op->editsh;
	eip->op->editsh = (Widget)0;
	if (editsh)
	    XtDestroyWidget (editsh);
}

/* called when the edit form widget is destroyed.
 * client contains the EditInfo pointer.
 */
/* ARGSUSED */
static void
destroy_cb (w, client, data)
Widget w;
XtPointer client;
XtPointer data;
{
	EditInfo *eip = (EditInfo *) client;

	if (eip)
	    free ((char *)eip);
}

/* go through each op and set utcstart to something.
 * use lststart if it's set else work harder.
 */
void
set_utcstart (np, op, nop)
Now *np;
Obs *op;
int nop;
{
	double today = mjd_day (np->n_mjd);
	double lngcor = radhr (np->n_lng);
	double utcnoon = 12.0 - floor(radhr(np->n_lng));
	double mjddawn, mjddusk;

	dawnduskToday (&mjddawn, &mjddusk);
	range (&utcnoon, 24.0);

	for (; --nop >= 0; op++) {
	    if (op->lststart == NOTIME) {
		if (op->rs.rs_flags & (RS_NEVERUP|RS_ERROR|RS_NOTRANS)) {
		    /* never up so set for local noon to get it out of way*/
		    op->utcstart = utcnoon;
		} else if ((op->rs.rs_flags & (RS_CIRCUMPOLAR))
					&& !at_night(mjd_hr(op->rs.rs_trantm))){
		    /* circumpolar but does't transit at night so use dawn or
		     * dusk depending on which is closer to transit time.
		     */
		    if (fabs(op->rs.rs_trantm - mjddusk) <
					    fabs(mjddawn - op->rs.rs_trantm))
			op->utcstart = mjd_hr(mjddusk);
		    else
			op->utcstart = mjd_hr(mjddawn);
		} else {
		    op->utcstart = mjd_hr(op->rs.rs_trantm);
		}
	    } else {
		double olst = op->lststart;
		olst -= lngcor;
		range (&olst, 24.0);
		gst_utc (today, olst, &op->utcstart);
	    }
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: edit.c,v $ $Date: 2006/08/27 20:20:04 $ $Revision: 1.2 $ $Name:  $"};
