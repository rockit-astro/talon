/* stuff for the eyepiece size dialog */

#include <stdio.h>
#include <math.h>

#if defined(__STDC__)
#include <stdlib.h>
#endif


#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/Scale.h>
#include <Xm/SelectioB.h>
#include <Xm/ToggleB.h>
#include <Xm/PushB.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "preferences.h"
#include "skyeyep.h"

extern Widget toplevel_w;
extern Colormap xe_cm;

extern Now *mm_get_now P_((void));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void prompt_map_cb P_((Widget w, XtPointer client, XtPointer call));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txtp));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void wtip P_((Widget w, char *tip));

/* a list of EyePieces */
static EyePiece *eyep;		/* malloced list of eyepieces */
static int neyep;		/* number of entries off eyep */

static Widget eyepd_w;		/* overall eyepiece dialog */
static Widget eyepws_w;		/* eyepiece width scale */
static Widget eyephs_w;		/* eyepiece height scale */
static Widget eyepwl_w;		/* eyepiece width label */
static Widget eyephl_w;		/* eyepiece height label */
static Widget eyer_w;		/* eyepiece Round TB */
static Widget eyes_w;		/* eyepiece Square TB */
static Widget eyef_w;		/* eyepiece filled TB */
static Widget eyeb_w;		/* eyepiece border TB */
static Widget telrad_w;		/* telrad on/off TB */
static Widget delep_w;		/* the delete all PB */
static Widget lock_w;		/* lock scales TB */

static void se_create_eyepd_w P_((void));
static void se_eyepsz P_((double *wp, double *hp, int *rp, int *fp));
static int se_scale_fmt P_((Widget s_w, Widget l_w));
static void se_telrad_cb P_((Widget w, XtPointer client, XtPointer call));
static void se_wscale_cb P_((Widget w, XtPointer client, XtPointer call));
static void se_hscale_cb P_((Widget w, XtPointer client, XtPointer call));
static void se_delall_cb P_((Widget w, XtPointer client, XtPointer call));
static void se_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void se_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void se_rmall P_((void));

/* telrad circle diameters, degrees */
static double telrad_sz[] = {.5, 2., 4.};

static char skyeyecategory[] = "Sky View -- Eyepieces";

void 
se_manage()
{
	if (!eyepd_w)
	    se_create_eyepd_w();
	XtManageChild (eyepd_w);
}

void 
se_unmanage()
{
	if (eyepd_w)
	    XtUnmanageChild (eyepd_w);
}

/* add one eyepiece for the given location */
void
se_add (ra, dec, alt, az)
double ra, dec, alt, az;
{
	EyePiece *new;
	int telrad;
	int nnew;

	/* check for first time */
	if (!eyepd_w)
	    se_create_eyepd_w();

	/* increase allocation at eyep by 1 or 3 if adding a telrad */
	telrad = XmToggleButtonGetState (telrad_w);
	nnew = telrad ? 3 : 1;
	eyep = (EyePiece*)XtRealloc((void *)eyep,(neyep+nnew)*sizeof(EyePiece));

	/* new points to the one(s) we're adding */
	new = eyep+neyep;
	neyep += nnew;

	/* fill in the details */
	while (--nnew >= 0) {
	    new->ra = ra;
	    new->dec = dec;
	    new->alt = alt;
	    new->az = az;
	    if (telrad) {
		new->eyepw = degrad(telrad_sz[nnew]);
		new->eyeph = degrad(telrad_sz[nnew]);
		new->round = 1;
		new->solid = 0;
	    } else
		se_eyepsz (&new->eyepw, &new->eyeph, &new->round, &new->solid);
	    new++;
	}

	/* at least one to delete now */
	XtSetSensitive (delep_w, True);
}

/* return the list of current eyepieces, if interested, and the count.
 */
int
se_getlist (ep)
EyePiece **ep;
{
	if (ep)
	    *ep = eyep;
	return (neyep);
}

/* fetch the current eyepiece diameter, in rads, whether it is round, and
 * whether it is filled from the dialog.
 */
static void
se_eyepsz(wp, hp, rp, fp)
double *wp, *hp;
int *rp;
int *fp;
{
	int eyepw, eyeph;

	if (!eyepd_w)
	    se_create_eyepd_w();

	XmScaleGetValue (eyepws_w, &eyepw);
	*wp = degrad(eyepw/60.0);

	XmScaleGetValue (eyephs_w, &eyeph);
	*hp = degrad(eyeph/60.0);

	*rp = XmToggleButtonGetState (eyer_w);
	*fp = XmToggleButtonGetState (eyef_w);
}

/* create the eyepiece size dialog */
static void
se_create_eyepd_w()
{
	Widget w, sep_w;
	Widget l_w, rb_w;
	Arg args[20];
	int n;

	/* create form */

	n = 0;
	XtSetArg(args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	eyepd_w = XmCreateFormDialog (svshell_w, "SkyEyep", args, n);
	set_something (eyepd_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (eyepd_w, XmNhelpCallback, se_help_cb, 0);
	XtAddCallback (eyepd_w, XmNmapCallback, prompt_map_cb, NULL);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Eyepiece Setup"); n++;
	XtSetValues (XtParent(eyepd_w), args, n);

	/* title label */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	w = XmCreateLabel (eyepd_w, "L", args, n);
	set_xmstring (w, XmNlabelString, "Eyepiece width and height, degrees:");
	XtManageChild (w);

	/* w scale and its label */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	eyepwl_w = XmCreateLabel (eyepd_w, "EyepWL", args, n);
	XtManageChild (eyepwl_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, eyepwl_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNshowValue, False); n++;
	XtSetArg (args[n], XmNscaleMultiple, 1); n++;
	eyepws_w = XmCreateScale (eyepd_w, "EyepW", args, n);
	XtAddCallback (eyepws_w, XmNdragCallback, se_wscale_cb, 0);
	XtAddCallback (eyepws_w, XmNvalueChangedCallback, se_wscale_cb, 0);
	wtip (eyepws_w, "Set to desired width of eyepiece");
	XtManageChild (eyepws_w);
	sr_reg (eyepws_w, NULL, skyeyecategory, 1);

	/* init the slave string */
	(void) se_scale_fmt (eyepws_w, eyepwl_w);

	/* h scale and its label */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, eyepws_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	eyephl_w = XmCreateLabel (eyepd_w, "EyepHL", args, n);
	XtManageChild (eyephl_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, eyephl_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNshowValue, False); n++;
	XtSetArg (args[n], XmNscaleMultiple, 1); n++;
	eyephs_w = XmCreateScale (eyepd_w, "EyepH", args, n);
	XtAddCallback (eyephs_w, XmNdragCallback, se_hscale_cb, 0);
	XtAddCallback (eyephs_w, XmNvalueChangedCallback, se_hscale_cb, 0);
	wtip (eyephs_w, "Set to desired height of eyepiece");
	XtManageChild (eyephs_w);
	sr_reg (eyephs_w, NULL, skyeyecategory, 1);

	/* init the slave string */
	(void) se_scale_fmt (eyephs_w, eyephl_w);

	/* lock TB */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, eyephs_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 10); n++;
	lock_w = XmCreateToggleButton (eyepd_w, "Lock", args, n);
	set_xmstring (lock_w, XmNlabelString, "Lock scales together");
	wtip (lock_w, "When on, width and height scales move as one");
	XtManageChild (lock_w);
	sr_reg (lock_w, NULL, skyeyecategory, 1);

	/* telrad TB */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, lock_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 10); n++;
	telrad_w = XmCreateToggleButton (eyepd_w, "Telrad", args, n);
	XtAddCallback (telrad_w, XmNvalueChangedCallback, se_telrad_cb, NULL);
	set_xmstring (telrad_w, XmNlabelString, "Create a Telrad pattern");
	wtip (telrad_w, "When on, creates 3 open circles matching the Telrad.");
	XtManageChild (telrad_w);
	sr_reg (telrad_w, NULL, skyeyecategory, 1);

	/* shape label */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, telrad_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	l_w = XmCreateLabel (eyepd_w, "S", args, n);
	set_xmstring (l_w, XmNlabelString, "Shape:");
	XtManageChild (l_w);

	/* round or square Radio box */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, l_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 10); n++;
	rb_w = XmCreateRadioBox (eyepd_w, "RSRB", args, n);
	XtManageChild (rb_w);

	    n = 0;
	    eyer_w = XmCreateToggleButton (rb_w, "Elliptical", args, n);
	    wtip (eyer_w, "When on, next eyepiece will be elliptical");
	    XtManageChild (eyer_w);
	    sr_reg (eyer_w, NULL, skyeyecategory, 1);

	    n = 0;
	    eyes_w = XmCreateToggleButton (rb_w, "Rectangular", args, n);
	    wtip (eyes_w, "When on, next eyepiece will be rectangular");
	    XtManageChild (eyes_w);

	    /* "Elliptical" establishes truth setting */
	    XmToggleButtonSetState (eyes_w, !XmToggleButtonGetState(eyer_w), 0);

	/* style label */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, telrad_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 50); n++;
	l_w = XmCreateLabel (eyepd_w, "St", args, n);
	set_xmstring (l_w, XmNlabelString, "Style:");
	XtManageChild (l_w);

	/* style Radio box */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, l_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 50); n++;
	rb_w = XmCreateRadioBox (eyepd_w, "FBRB", args, n);
	XtManageChild (rb_w);

	    n = 0;
	    eyef_w = XmCreateToggleButton (rb_w, "Solid", args, n);
	    wtip (eyef_w, "When on, next eyepiece will be solid");
	    XtManageChild (eyef_w);
	    sr_reg (eyef_w, NULL, skyeyecategory, 1);

	    n = 0;
	    eyeb_w = XmCreateToggleButton (rb_w, "Outline", args, n);
	    wtip (eyeb_w, "When on, next eyepiece will be just a border");
	    XtManageChild (eyeb_w);

	    /* "Solid" establishes truth setting */
	    XmToggleButtonSetState (eyeb_w, !XmToggleButtonGetState(eyef_w), 0);

	/* delete PB */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rb_w); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 20); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 80); n++;
	delep_w = XmCreatePushButton (eyepd_w, "DelE", args, n);
	XtAddCallback (delep_w, XmNactivateCallback, se_delall_cb, NULL);
	wtip (delep_w, "Delete all eyepieces");
	set_xmstring (delep_w, XmNlabelString, "Delete all Eyepieces");
	XtSetSensitive (delep_w, False);	/* works when there are some */
	XtManageChild (delep_w);

	/* separator */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, delep_w); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (eyepd_w, "Sep", args, n);
	XtManageChild (sep_w);

	/* a close button */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 20); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 40); n++;
	w = XmCreatePushButton (eyepd_w, "Close", args, n);
	XtAddCallback (w, XmNactivateCallback, se_close_cb, NULL);
	wtip (w, "Close this dialog");
	XtManageChild (w);

	/* a help button */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 60); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 80); n++;
	w = XmCreatePushButton (eyepd_w, "Help", args, n);
	XtAddCallback (w, XmNactivateCallback, se_help_cb, NULL);
	wtip (w, "More info about this dialog");
	XtManageChild (w);

	/* engage side effects if telrad on initially */
	if (XmToggleButtonGetState(telrad_w)) {
	    XmToggleButtonSetState(telrad_w, False, True);
	    XmToggleButtonSetState(telrad_w, True, True);
	}
}

/* read the given scale and write it's value in the given label.
 * return the scale value.
 */
static int
se_scale_fmt (s_w, l_w)
Widget s_w, l_w;
{
	char buf[64];
	int v;

	/* format from value, in arc minutes */
	XmScaleGetValue (s_w, &v);
	fs_sexa (buf, v/60.0, 2, 60);
	set_xmstring (l_w, XmNlabelString, buf);

	return (v);
}

/* called when the telrad TB is activated */
static void
se_telrad_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int set = XmToggleButtonGetState (w);

	if (set) {
	    XtSetSensitive (eyef_w, False);
	    XtSetSensitive (eyeb_w, False);
	    XtSetSensitive (eyer_w, False);
	    XtSetSensitive (eyes_w, False);
	    XtSetSensitive (lock_w, False);
	    XtSetSensitive (eyephs_w, False);
	    XtSetSensitive (eyepws_w, False);
	} else {
	    XtSetSensitive (eyef_w, True);
	    XtSetSensitive (eyeb_w, True);
	    XtSetSensitive (eyer_w, True);
	    XtSetSensitive (eyes_w, True);
	    XtSetSensitive (lock_w, True);
	    XtSetSensitive (eyephs_w, True);
	    XtSetSensitive (eyepws_w, True);
	}
}

/* drag callback from the height scale */
static void
se_hscale_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int v = se_scale_fmt (eyephs_w, eyephl_w);

	/* slave the w scale to the h scale */
	if (XmToggleButtonGetState(lock_w)) {
	    XmScaleSetValue (eyepws_w, v);
	    se_scale_fmt (eyepws_w, eyepwl_w);
	    XmToggleButtonSetState (telrad_w, False, True);
	}
}

/* drag callback from the width scale */
static void
se_wscale_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int v = se_scale_fmt (eyepws_w, eyepwl_w);

	/* slave the w scale to the h scale */
	if (XmToggleButtonGetState(lock_w)) {
	    XmScaleSetValue (eyephs_w, v);
	    se_scale_fmt (eyephs_w, eyephl_w);
	    XmToggleButtonSetState (telrad_w, False, True);
	}
}

/* callback from the delete eyepieces control.
 */
/* ARGSUSED */
static void
se_delall_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (eyep) {
	    se_rmall();
	    sv_all(mm_get_now());
	    XtSetSensitive (delep_w, False);
	}
}

/* callback from the close PB.
 */
/* ARGSUSED */
static void
se_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (eyepd_w);
}

/* called when the help button is hit in the eyepiece dialog */
/* ARGSUSED */
static void
se_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg[] = {
	    "Define eyepiece shapes and sizes."
	};

	hlp_dialog ("Sky View -- Eyepieces", msg, sizeof(msg)/sizeof(msg[0]));

}

/* delete the entire list of eyepieces */
static void
se_rmall()
{
	if (eyep) {
	    free ((void *)eyep);
	    eyep = NULL;
	}
	neyep = 0;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: skyeyep.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
