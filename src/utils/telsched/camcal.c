/* code to add a camera calibration scan.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include <Xm/Xm.h>
#include <Xm/TextF.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/Label.h>
#include <Xm/Form.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "xtools.h"
#include "configfile.h"
#include "telenv.h"
#include "scan.h"

#include "telsched.h"

static void create_camcal_w (void);
static void init_camcal_w(void);
static void style_cb (Widget w, XtPointer client, XtPointer call);
static void close_cb (Widget w, XtPointer client, XtPointer call);
static void ok_cb (Widget w, XtPointer client, XtPointer call);
static char *styleName (CCType style);
static void style2CCDCalib (CCType style, CCDCalib *cp);
static int getFilter (CCType style, char *filter);
static int estimateDur (CCType style, char filter);

static Widget camcal_w;		/* main dialog */
static Widget startt_w;		/* start time TF */
static Widget dir_w;		/* directory TF */
static Widget pri_w;		/* priority TF */
static Widget bin_w;		/* binning TF */
static Widget bias_w;		/* bias TB */
static Widget thermal_w;	/* thermal TB */
static Widget flat_w;		/* flat TB */
static Widget filter_w;		/* filter TF */
static Widget fl_w;		/* filter label */

/* callback to allow inserting a camera calibration scan */
void
camcal_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!camcal_w) {
	    create_camcal_w();
	    init_camcal_w();
	}

	if (XtIsManaged(camcal_w))
	    XtUnmanageChild (camcal_w);
	else {
	    XtManageChild (camcal_w);
	}
}

static void
create_camcal_w()
{
	typedef struct {
	    char *prompt;
	    char *tfname;
	    Widget *tfp;
	} Prompt;
	static Prompt prompts[] = {
	    /* N.B. put the widest one on top */
	    {"Cal Directory:",	"CalDir",	&dir_w},
	    {"Sort PRIORITY:",	"Priority",	&pri_w},
	    {"UTC Start:",	"UTCStart",	&startt_w},
	    {"Binning:",	"Binning",	&bin_w},
	};
	Widget sep_w;
	Widget flw = 0;
	Widget w;
	Arg args[20];
	int n;
	int i;

	n = 0;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 10); n++;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	camcal_w = XmCreateFormDialog (toplevel_w, "CamCal", args, n);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Setup Camera Calibration Scan"); n++;
	XtSetValues (XtParent(camcal_w), args, n);

	/* make the label/prompt pairs */

	for (i = 0; i < XtNumber(prompts); i++) {
	    Prompt *pp = &prompts[i];

	    n = 0;
	    if (i == 0) {
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    } else {
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
		XtSetArg (args[n], XmNtopWidget, *pp[-1].tfp); n++;
	    }
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	    w = XmCreateLabel (camcal_w, "L", args, n);
	    set_xmstring (w, XmNlabelString, pp->prompt);
	    XtManageChild (w);

	    n = 0;
	    if (i == 0) {
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
		flw = w;
	    } else {
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
		XtSetArg (args[n], XmNtopWidget, *pp[-1].tfp); n++;
	    }
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNleftWidget, flw); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    w = XmCreateTextField (camcal_w, pp->tfname, args, n);
	    XtManageChild (w);
	    *pp->tfp = w;
	}

	/* make the style toggle-like buttons */

	w = *prompts[XtNumber(prompts)-1].tfp;

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, w); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 5); n++;
	bias_w = XmCreateToggleButton (camcal_w, "Bias", args, n);
	XtAddCallback (bias_w, XmNvalueChangedCallback, style_cb,
							    (XtPointer)CT_BIAS);
	XtManageChild (bias_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, w); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 27); n++;
	thermal_w = XmCreateToggleButton (camcal_w, "Thermal", args, n);
	XtAddCallback (thermal_w, XmNvalueChangedCallback, style_cb,
							(XtPointer)CT_THERMAL);
	XtManageChild (thermal_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, w); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 55); n++;
	flat_w = XmCreateToggleButton (camcal_w, "Flat", args, n);
	XtAddCallback (flat_w, XmNvalueChangedCallback, style_cb,
							    (XtPointer)CT_FLAT);
	XtManageChild (flat_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, w); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 72); n++;
	fl_w = XmCreateLabel (camcal_w, "Filter", args, n);
	wlprintf (fl_w, "Filter:");
	XtManageChild (fl_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, w); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 95); n++;
	XtSetArg (args[n], XmNcolumns, 1); n++;
	filter_w = XmCreateTextField (camcal_w, "FilterTF", args, n);
	XtManageChild (filter_w);

	/* sep */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNtopWidget, filter_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (camcal_w, "Sep", args, n);
	XtManageChild (sep_w);

	/* controls at the bottom */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 20); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 40); n++;
	w = XmCreatePushButton (camcal_w, "Apply", args, n);
	XtManageChild (w);
	XtAddCallback (w, XmNactivateCallback, ok_cb, NULL);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 60); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 80); n++;
	w = XmCreatePushButton (camcal_w, "Close", args, n);
	XtManageChild (w);
	XtAddCallback (w, XmNactivateCallback, close_cb, NULL);
}

/* set up whatever is needed first time the dialog appears */
static void
init_camcal_w()
{
	double duskmjd, dawnmjd;
	char buf[128];

	/* init time to a while before dusk */
	dawnduskToday (&dawnmjd, &duskmjd);
	fs_sexa (buf, mjd_hr(duskmjd-1./24.), 2, 60);
	wtprintf (startt_w, "%s", buf[0] == ' ' ? buf+1 : buf);

	/* init directory */
	telfixpath (buf, "archive/calib");
	wtprintf (dir_w, "%s", buf);
	XtVaSetValues (dir_w, XmNcolumns, strlen(buf), NULL);

	/* start off with Bias selected */
	XmToggleButtonSetState (bias_w, True, False);
	XmTextFieldSetString (filter_w, "C");
	XtSetSensitive (filter_w, False);
	XtSetSensitive (fl_w, False);

	/* init priority */
	wtprintf (pri_w, "0");

	/* init binning */
	wtprintf (bin_w, "1");
}

/* callback from the three style TB's.
 * client is one of CCType
 */
static void
style_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	CCType style = (CCType)client;
	int set = XmToggleButtonGetState (w);

	switch (style) {
	case CT_BIAS: 
	    if (!set) {
		XmToggleButtonSetState (thermal_w, False, False);
		XmToggleButtonSetState (flat_w, False, False);
		XtSetSensitive (filter_w, False);
		XtSetSensitive (fl_w, False);
	    }
	    break;

	case CT_THERMAL:
	    if (set) {
		XmToggleButtonSetState (bias_w, True, False);
	    } else {
		XmToggleButtonSetState (flat_w, False, False);
		XtSetSensitive (filter_w, False);
		XtSetSensitive (fl_w, False);
	    }
	    break;

	case CT_FLAT:
	    if (set) {
		XmToggleButtonSetState (bias_w, True, False);
		XmToggleButtonSetState (thermal_w, True, False);
		XtSetSensitive (filter_w, True);
		XtSetSensitive (fl_w, True);
	    } else {
		XtSetSensitive (filter_w, False);
		XtSetSensitive (fl_w, False);
	    }
	    break;

	default:
	    fprintf (stderr, "Bug! style=%d\n", style);
	    exit (1);
	}
}


static void
close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (camcal_w);
}

static void
ok_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char *str;
	char path[1024];
	int pri;
	int bin;
	double utcstart, lststart;
	CCType style;
	Now nowscan;
	Obs obs;
	Obs *op = &obs;
	Obj *objp = &op->scan.obj;
	char *sname;
	char filter;

	/* fetch the starting time, utc */
	get_something (startt_w, XmNvalue, (char *)&str);
	if (scansex (str, &utcstart) < 0) {
	    msg ("Bad start time format: %s", str);
	    XtFree (str);
	    return;
	}
	XtFree (str);

	/* fetch the desired binning */
	get_something (bin_w, XmNvalue, (char *)&str);
	bin = atoi (str);
	XtFree (str);
	if (bin < 1) {
	    msg ("Binning must be at least 1");
	    return;
	}

	/* fetch the desired priority */
	get_something (pri_w, XmNvalue, (char *)&str);
	pri = atoi (str);
	XtFree (str);

	/* fetch directory */
	str = XmTextFieldGetString (dir_w);
	strcpy (path, str);
	XtFree (str);
	if (path[0] != '/') {
	    msg ("Directory must begin with /: %s", path);
	    return;
	}

	/* find the highest form of style set */
	if (XmToggleButtonGetState(flat_w))
	    style = CT_FLAT;
	else if (XmToggleButtonGetState(thermal_w))
	    style = CT_THERMAL;
	else if (XmToggleButtonGetState(bias_w))
	    style = CT_BIAS;
	else {
	    msg ("Please choose a calibration type");
	    return;
	}
	sname = styleName(style);

	/* fetch the desired filter */
	if (getFilter(style, &filter) < 0)
	    return;	/* already printed msg() */

	/* ok */
	msg ("Adding %s scan...", sname);
	watch_cursor(1);

	/* init obs */
	initObs (&obs);

	/* find lst start time */
	nowscan = now;
	nowscan.n_mjd = mjd_day(now.n_mjd) + utcstart/24.0;
	now_lst(&nowscan, &lststart);

	/* set anything for the object */
	objp->o_type = FIXED;
	objp->f_epoch = J2000;
	objp->f_RA = 0;
	objp->f_dec = 0;
	sprintf (objp->o_name, "%s corr", sname);

	ACPYZ (op->scan.schedfn, "CameraCorrections.sch");
	ACPYZ (op->scan.comment, "This entry was generated by telsched");
	sprintf (op->scan.title, "Camera %s Calibration scan", sname);
	ACPYZ (op->scan.observer, "Operator");
	ACPYZ (op->scan.imagedn, path);
	ACPYZ (op->scan.imagefn, "xxx");
	/* N.B. imagefn portion is actually set by telrun */

	style2CCDCalib(style, &op->scan.ccdcalib);
	op->scan.compress = 0;
	op->scan.sx = 0;
	op->scan.sy = 0;
	op->scan.sw = DEFIMW;
	op->scan.sh = DEFIMH;
	op->scan.binx = bin;
	op->scan.biny = bin;
	op->scan.filter = filter;
	op->scan.dur = estimateDur(style, filter);

	op->lststart = lststart;
	op->utcstart = lst2utc(lststart);
	op->scan.startdt = LSTDELTADEF*60;	/* want seconds */
	op->scan.priority = pri;

	/* ok, update the main list */
	addSchedEntries (&obs, 1);
	watch_cursor(0);
}

static char *
styleName (CCType style)
{
	switch (style) {
	case CT_BIAS:    return ("Bias");
	case CT_THERMAL: return ("Thermal");
	case CT_FLAT:    return ("Flat");
	default: fprintf (stderr, "Bogus style: %d\n", style);
	exit (1);
	}
}

static void
style2CCDCalib (CCType style, CCDCalib *cp)
{
	switch (style) {
	case CT_BIAS:    cp->newc = CT_BIAS; cp->data = CD_NONE; break;
	case CT_THERMAL: cp->newc = CT_THERMAL; cp->data = CD_NONE; break;
	case CT_FLAT:    cp->newc = CT_FLAT; cp->data = CD_NONE; break;
	default: fprintf (stderr, "Bogus style: %d\n", style); exit (1);
	}
}

static int
getFilter (CCType style, char *filter)
{
	if (style == CT_FLAT) {
	    static char fakestr[] = "?";
	    String str = XmTextFieldGetString (filter_w);
	    char *fstr = str[0] ? str : fakestr;
	    int r = legalFilters (1, &fstr);

	    if (r == 0)
		*filter = fstr[0];
	    XtFree (str);
	    return (r);
	}

	*filter = 'C';	/* TODO */
	return (0);
}

static int
estimateDur (CCType style, char filter)
{
	int bdur = NBIAS*CAMDIG_MAX;
	int tdur = bdur + NTHERM*(THERMDUR+CAMDIG_MAX);
	int i;

	switch (style) {
	case CT_BIAS:
	    return (bdur);
	case CT_THERMAL:
	    return (tdur);
	case CT_FLAT:
	    /* find duration for this filter */
	    for (i = 0; i < nfilt; i++)
		if (filterp[i].name[0] == filter)
		    return (bdur+tdur+NFLAT*(filterp[i].flatdur+CAMDIG_MAX));
	    msg ("Filter not found: %c", filter);
	    return (0);
	default:
	    fprintf (stderr, "Bogus style: %d\n", style);
	    exit (1);
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: camcal.c,v $ $Date: 2002/03/12 16:46:21 $ $Revision: 1.2 $ $Name:  $"};
