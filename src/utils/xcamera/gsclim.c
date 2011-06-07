/* dialog to ask the desired GSC limiting magnitude */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/SelectioB.h>
#include <Xm/Form.h>
#include <Xm/ToggleB.h>
#include <Xm/Separator.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/TextF.h>

#include "P_.h"
#include "astro.h"
#include "xtools.h"
#include "fits.h"
#include "telenv.h"
#include "fieldstar.h"
#include "wcs.h"

#include "camera.h"

double gsclimit;
double huntrad;
int usno_ok = -1;

static Widget gsc_w;		/* overall Form Dialog */
static Widget hunt_w;		/* TF holding max hunt radius */
static Widget gsclimit_w;	/* TF holding limiting mag */
static Widget ra_w;		/* TF center RC */
static Widget dec_w;		/* TF center Dec */
static Widget ras_w;		/* TF RC pix size */
static Widget decs_w;		/* TF Dec pix size */
static Widget usnopath_w;	/* TF holding path to sa1.0 image */
static Widget wantusno_w;	/* TB whether to try for usno */
static Widget gsccachepath_w;	/* TF holding path to cache dir */

static void newRow (Widget rc_w, char *name, char *prompt, Widget *new_wp);
static void okCB (Widget w, XtPointer client, XtPointer call);
static void applyCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);
static void apply (void);
static void fssetup (void);
static int newSeed(void);

void
manageGSCLimit ()
{
	if (!gsc_w)
	    createGSCLimit();

	if (XtIsManaged(gsc_w))
	    raiseShell (gsc_w);
	else {
	    gscSetDialog();
	    XtManageChild (gsc_w);
	}
}

/* create the widgets and set up the defaults from them.
 */
void
createGSCLimit()
{
	Widget w;
	Widget rc_w;
	Arg args[20];
	char buf[1024];
	int n;

	/* the main form */

	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 5); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	XtSetArg (args[n], XmNfractionBase, 10); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	gsc_w = XmCreateFormDialog (toplevel_w, "GSCSetup", args, n);

	n = 0;
	XtSetArg (args[n], XmNtitle, "Field Star Setup"); n++;
	XtSetValues (XtParent(gsc_w), args, n);
	XtVaSetValues (gsc_w, XmNcolormap, camcm, NULL);

	/* a RC for the the 1-line prompt/textfield pairs */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNnumColumns, 6); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	rc_w = XmCreateRowColumn (gsc_w, "RC1", args, n);
	XtManageChild (rc_w);

	newRow (rc_w, "MatchRadius", "Hunt radius, degs:", &hunt_w);
	newRow (rc_w, "MagLimit", "Mag limit:", &gsclimit_w);
	newRow (rc_w, "RA", "Nom center RA:", &ra_w);
	newRow (rc_w, "Dec", "Nom center Dec:", &dec_w);
	newRow (rc_w, "Dec", "RA step right, \":", &ras_w);
	newRow (rc_w, "Dec", "Dec step down, \":", &decs_w);

	/* a RC for the the 2-line prompt/textfield pairs */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNnumColumns, 1); n++;
	rc_w = XmCreateRowColumn (gsc_w, "RC2", args, n);
	XtManageChild (rc_w);

	n = 0;
	XtSetArg (args[n], XmNseparatorType, XmNO_LINE); n++;
	w = XmCreateSeparator (rc_w, "Gap", args, n);
	XtManageChild (w);

	/* path to cache */

	newRow (rc_w, "CachePath", "Path to GSC Disk Cache:", &gsccachepath_w);
	telfixpath (buf, "archive/catalogs/gsc");
	XtVaSetValues (gsccachepath_w,
	    XmNvalue, buf,
	    XmNcolumns, strlen(buf),
	    NULL);

	/* path to USNO */

	n = 0;
	wantusno_w = XmCreateToggleButton (rc_w, "WantUSNO", args, n);
	set_xmstring (wantusno_w, XmNlabelString, "Use USNO SA1.0 or 2.0 too");
	XtManageChild (wantusno_w);

	newRow (rc_w, "USNOPath", "Path to SA1.0 or 2.0 image:", &usnopath_w);
	telfixpath (buf, "archive/catalogs/usno");
	XtVaSetValues (usnopath_w,
	    XmNvalue, buf,
	    XmNcolumns, strlen(buf),
	    NULL);

	/* bottom controls */

	n = 0;
	XtSetArg (args[n], XmNseparatorType, XmNO_LINE); n++;
	w = XmCreateSeparator (rc_w, "Gap", args, n);
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 1); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 3); n++;
	w = XmCreatePushButton (gsc_w, "Ok", args, n);
	XtAddCallback (w, XmNactivateCallback, okCB, NULL);
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 4); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 6); n++;
	w = XmCreatePushButton (gsc_w, "Apply", args, n);
	XtAddCallback (w, XmNactivateCallback, applyCB, NULL);
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 7); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 9); n++;
	w = XmCreatePushButton (gsc_w, "Close", args, n);
	XtAddCallback (w, XmNactivateCallback, closeCB, NULL);
	XtManageChild (w);

	/* apply the initial global values */
	fssetup();
}

/* set the widgets to all their current values */
void
gscSetDialog()
{
	static char blank[] = " ";
	FImage *fip = &state.fimage;
	char buf[1024];
	double tmp;

	if (!fip->image)
	    return;
	if (!gsc_w)
	    createGSCLimit();

	if (getStringFITS (fip, "RA", buf) < 0) {
	    double r, d;
	    if (!xy2RADec (fip, fip->sw/2., fip->sh/2., &r, &d))
		fs_sexa (buf, radhr(r), 2, 36000);
	    else
		strcpy (buf, blank);
	}
	XmTextFieldSetString (ra_w, buf);

	if (getStringFITS (fip, "DEC", buf) < 0) {
	    double r, d;
	    if (!xy2RADec (fip, fip->sw/2., fip->sh/2., &r, &d))
		fs_sexa (buf, raddeg(d), 2, 3600);
	    else
		strcpy (buf, blank);
	}
	XmTextFieldSetString (dec_w, buf);

	if (!getRealFITS (fip, "CDELT1", &tmp)) {
	    sprintf (buf, "%g", tmp*3600);
	    XmTextFieldSetString (ras_w, buf);
	} else
	    XmTextFieldSetString (ras_w, blank);

	if (!getRealFITS (fip, "CDELT2", &tmp)) {
	    sprintf (buf, "%g", tmp*3600);
	    XmTextFieldSetString (decs_w, buf);
	} else
	    XmTextFieldSetString (decs_w, blank);

	sprintf (buf, "%g", raddeg(huntrad));
	XmTextFieldSetString (hunt_w, buf);

	sprintf (buf, "%g", gsclimit);
	XmTextFieldSetString (gsclimit_w, buf);
}

/* add one prompt/text to rc_w */
static void
newRow (Widget rc_w, char *name, char *prompt, Widget *new_wp)
{
	Arg args[20];
	Widget w;
	int n;

	n = 0;
	w = XmCreateLabel (rc_w, "FSL", args, n);
	set_xmstring (w, XmNlabelString, prompt);
	XtManageChild (w);

	n = 0;
	w = XmCreateTextField (rc_w, name, args, n);
	XtManageChild (w);

	*new_wp = w;
}

static void
okCB (Widget w, XtPointer client, XtPointer call)
{
	apply();
	XtUnmanageChild (gsc_w);
}

static void
applyCB (Widget w, XtPointer client, XtPointer call)
{
	apply();
}

static void
closeCB (Widget w, XtPointer client, XtPointer call)
{
	XtUnmanageChild (gsc_w);
}

/* read all the widgets and "make them so" */
static void
apply()
{
	/* set the new seed values */
	if (newSeed() < 0)
	    return;

	/* apply the new values */
	fssetup();

	/* if showing GSC stars, show new level */
	if (state.showgsc) {
	    resetGSC();
	    refreshScene (0, 0, state.fimage.sw, state.fimage.sh);
						/* refresh in case fewer */
	}
}

/* read the widgets to set the globals and call GSCSetup()
 * and USNOSetup() with the directories.
 */
static void
fssetup()
{
	static String chp;
	char buf[1024];
	String tf;

	tf = XmTextFieldGetString (hunt_w);
	huntrad = degrad(atof(tf));
	XtFree (tf);

	tf = XmTextFieldGetString (gsclimit_w);
	gsclimit = atof (tf);
	XtFree (tf);

	/* GSC requires persistent string */
	if (chp)
	    XtFree(chp);
	chp = XmTextFieldGetString (gsccachepath_w);
	if (GSCSetup (NULL, chp, buf) < 0)
	    msg ("GSC error: %s", buf);

	if (XmToggleButtonGetState (wantusno_w)) {
	    /* USNO does not require persistent string */
	    tf = XmTextFieldGetString (usnopath_w);
	    if ((usno_ok = USNOSetup (tf, 0, buf)) < 0) {
		msg ("USNO error: %s", buf);
		XmToggleButtonSetState (wantusno_w, False, True);
	    }
	    XtFree (tf);
	} else
	    usno_ok = -1;
}

/* read the widgets and apply new values for RA/DEC/CDELT1/CDELT2.
 * return 0 if ok else -1.
 */
static int
newSeed()
{
	FImage *fip = &state.fimage;
	double ra, dec, c1, c2;
	char buf[1024];
	String tf;
	int s;

	/* check all first */

	tf = XmTextFieldGetString (ra_w);
	s = scansex (tf, &ra);
	XtFree (tf);
	if (s < 0) {
	    msg ("Missing or Bad RA format");
	    return (-1);
	}

	tf = XmTextFieldGetString (dec_w);
	s = scansex (tf, &dec);
	XtFree (tf);
	if (s < 0) {
	    msg ("Missing or Bad Dec format");
	    return (-1);
	}

	tf = XmTextFieldGetString (ras_w);
	c1 = atof(tf);
	XtFree (tf);
	if (c1 == 0.0) {
	    msg ("CDELT1 can not be 0");
	    return (-1);
	}

	tf = XmTextFieldGetString (decs_w);
	c2 = atof(tf);
	XtFree (tf);
	if (c2 == 0.0) {
	    msg ("CDELT2 can not be 0");
	    return (-1);
	}


	/* now apply */

	fs_sexa (buf, ra, 3, 36000);
	setStringFITS (fip, "RA", buf, "Nominal center J2000 RA");

	fs_sexa (buf, dec, 3, 36000);
	setStringFITS (fip, "DEC", buf, "Nominal center J2000 Dec");

	setRealFITS (fip, "CDELT1", c1/3600.0, 10,
					    "RA step right, degrees/pixel");

	setRealFITS (fip, "CDELT2", c2/3600.0, 10,
					    "Dec step down, degrees/pixel");

	updateFITS();

	return (0);

}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: gsclim.c,v $ $Date: 2001/05/23 19:26:27 $ $Revision: 1.3 $ $N"};
