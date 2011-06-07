/* code handle photometry reports
 * we do the main menu and the differential stuff here.
 * the absolute photometry is done in photomasb.c
 */

#include <stdio.h>
#include <stdlib.h>
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
#include "xtools.h"
#include "fits.h"
#include "wcs.h"
#include "fieldstar.h"
#include "camera.h"

static void apReport (int newap);
static void create_photom_w (void);
static Widget create_diff_w (Widget top);
static void modeCB (Widget w, XtPointer client, XtPointer call);
static void unmapCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);
static void apCB (Widget w, XtPointer client, XtPointer call);
static void lockapCB (Widget w, XtPointer client, XtPointer call);
static void makeGC(void);
static void diffReport(void);

static int wantPhotom;		/* !0 when want any photom stuff */
static int wantAbs;		/* 1 for abs, 0 for differential. */

static Widget photom_w;		/* toplevel photom dialog */
static Widget rap_w;		/* text widget to hold StarDfn.rAp */
static Widget rsrch_w;		/* text widget to hold StarDfn.rsrch */
static Widget lockap_w;		/* the lock aperature TB */
static Widget ix_w, iy_w;	/* labels to report star x/y */
static Widget ra_w, dec_w;	/* labels to report star RA/Dec */
static Widget p_w;		/* brightest pixel in star */
static Widget fwhm_w;		/* star full width half max */

static GC photGC;		/* GC to use to mark photometric stars */

static char duh[] = "????";

/* stuff for differential photometry mode */
static Widget diff_w;		/* toplevel differential manager */
static void setRefCB (Widget w, XtPointer client, XtPointer call);
static StarStats newStar;	/* last star computed */
static StarStats refStar;	/* reference star, unless .p == 0 */
static Widget mag_w, magerr_w;	/* labels to report star magnitude and err */
static Widget magref_w;		/* text widget with desired reference mag */


/* called from the overall photometry feature PB */
/* ARGSUSED */
void
photomCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!photom_w)
	    create_photom_w();

	if (XtIsManaged(photom_w))
	    raiseShell (photom_w);
	else 
	    XtManageChild (photom_w);
	wantPhotom = 1;

	manageAbsPhot(wantAbs);
}

/* used by the abs photom to set and lock the aperture found in the
 * photcal file.
 */
void
setAp (newap)
int newap;
{
	/* set the value */
	apReport (newap);

	/* and lock it in */
	XmToggleButtonSetState (lockap_w, True, False);
}

/* called when mouse button is pressed to report photometry.
 * may be called while we are not up.
 */
void
doPhotom (ix, iy)
int ix, iy;	/* image coordinates of mouse */
{
	StarDfn sd;
	double pixsz;
	char *txt;
	double ra, dec;
	char str[1024];
	int r, wx, wy;

	/* make sure it's ok to proceed */
	if (!wantPhotom || !state.fimage.image)
	    return;

	if (!photom_w)
	    create_photom_w();
	XtManageChild (photom_w);
	if (!photGC)
	    makeGC();

	/* gather star definition info */
	sd.how = SSHOW_MAXINAREA;
	txt = XmTextFieldGetString (rsrch_w);
	sd.rsrch = atoi (txt);
	XtFree (txt);
	if (XmToggleButtonGetState(lockap_w)) {
	    /* locked, so force value in text field */
	    txt = XmTextFieldGetString (rap_w);
	    sd.rAp = atoi (txt);
	    XtFree (txt);
	} else {
	    /* not locked, so allow searching for best aperture */
	    sd.rAp = 0;		/* allow searching for a new one */
	    refStar.p = 0;	/* turns off mag compuations */
	}
	
	/* compute the star stats */

	if (starStats ((CamPixel *)state.fimage.image, state.fimage.sw,
			    state.fimage.sh, &sd, ix, iy, &newStar, str) < 0) {
	    msg ("%s", str);
	    return;
	}

	wx = (int)floor(newStar.x+0.5);
	wy = (int)floor(newStar.y+0.5);
	image2window (&wx, &wy);

	/* report star stats */

	if (getRealFITS (&state.fimage, "CDELT1", &pixsz) < 0) {
	    msg ("No CDELT1 -- reporting FWMH in pixels");
	    pixsz = 1;
	} else
	    pixsz = 3600.0*fabs(pixsz);
	(void) sprintf (str, "%7.2f", newStar.x);
	set_xmstring (ix_w, XmNlabelString, str);
	(void) sprintf (str, "%7.2f", newStar.y);
	set_xmstring (iy_w, XmNlabelString, str);
	if (xy2rd (&state.fimage, newStar.x, newStar.y, &ra, &dec) < 0) {
	    msg ("No WCS for RA/DEC");
	    set_xmstring (ra_w, XmNlabelString, duh);
	    set_xmstring (dec_w, XmNlabelString, duh);
	} else {
	    fs_sexa (str, radhr(ra), 3, 360000);
	    set_xmstring (ra_w, XmNlabelString, str);
	    fs_sexa (str, raddeg(dec), 3, 36000);
	    set_xmstring (dec_w, XmNlabelString, str);
	}
	(void) sprintf (str, "%7.1f", sqrt(newStar.xmax*newStar.ymax));
	set_xmstring (p_w, XmNlabelString, str);
	(void) sprintf (str, "%5.2f\"",pixsz*sqrt(newStar.xfwhm*newStar.yfwhm));
	set_xmstring (fwhm_w, XmNlabelString, str);

	/* mark location if background exists.
	 * radius based just on star brightness wrt sky background.
	 */
	if (newStar.Sky > 0) {
	    r = 1.5*log10((double)newStar.Src);
	    if (r < 1)
		r = 1;
	    r = r*state.mag/MAGDENOM;
	    XDrawArc (XtDisplay(state.imageDA), XtWindow(state.imageDA),
				    photGC, wx-r, wy-r, 2*r, 2*r, 0, 360*64);
	}

	/* report the aperature used */
	apReport (newStar.rAp);

	/* now do the work that depends on abs or differential mode */
	if (wantAbs)
	    absPhotWork(&newStar);
	else
	    diffReport();
}

/* display the aperture value in rap_w.
 * take care not to actually change the widget unless it is really different
 *   so as to avoid useless valueChanged callbacks.
 */
static void
apReport (int newap)
{
	char *oldstr;
	int oldap;

	oldstr = XmTextFieldGetString (rap_w);
	oldap = atoi (oldstr);
	XtFree (oldstr);

	if (newap != oldap) {
	    char newstr[64];
	    (void) sprintf (newstr, "%d", newap);
	    XmTextFieldSetString (rap_w, newstr);
	}
}


/* create all the dialogs; only manage the main one now though.
 * initially the mode is for differential.
 */
static void
create_photom_w()
{
	typedef struct {
	    char *name;
	    char *label;
	    Widget *wp;
	    XtCallbackProc cb;
	} Field;
	static Field ifields[] = {
	    {"RSearch",   	"Search R:",	&rsrch_w,	NULL},
	    {"RAperture", 	"Aperture R:",	&rap_w,		apCB},
	};
	static Field ofields[] = {
	    {"X",      "X:",       &ix_w},
	    {"Y",      "Y:",       &iy_w},
	    {"RA",     "RA:",      &ra_w},
	    {"Dec",    "Dec:",     &dec_w},
	    {"GPeak",  "Peak:",    &p_w},
	    {"FWHM",   "FWHM:",    &fwhm_w},
	};
	Widget rc_w, w;
	Widget sep_w;
	Widget rb_w;
	Widget close_w;
	Widget bd_w, ba_w;
	XmString str;
	Arg args[20];
	int n;
	int i;

	/* create master form */
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 5); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	photom_w = XmCreateFormDialog (toplevel_w, "Photometry", args, n);
	XtAddCallback (photom_w, XmNunmapCallback, unmapCB, NULL);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Photometry"); n++;
	XtSetValues (XtParent(photom_w), args, n);
	XtVaSetValues (photom_w, XmNcolormap, camcm, NULL);

	/* create the common fields in a 2-column rowcolumn.
	 * ifields, the auto aperture toggle, then the ofields.
	 */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNnumColumns, 
			    XtNumber(ifields)+XtNumber(ofields)+2); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	rc_w = XmCreateRowColumn (photom_w, "RC", args, n);
	XtManageChild (rc_w);

	    /* input fields */

	    for (i = 0; i < XtNumber(ifields); i++) {
		Field *fp = &ifields[i];

		str = XmStringCreate (fp->label, XmSTRING_DEFAULT_CHARSET);
		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
		XtSetArg (args[n], XmNlabelString, str); n++;
		w = XmCreateLabel (rc_w, "ILabel", args, n);
		XtManageChild (w);
		XmStringFree (str);

		n = 0;
		XtSetArg (args[n], XmNcolumns, 5); n++;
		w = XmCreateTextField (rc_w, fp->name, args, n);
		XtManageChild (w);
		*(fp->wp) = w;

		if (fp->cb)
		    XtAddCallback (w, XmNvalueChangedCallback, fp->cb, NULL);
	    }

	    /* the aperature toggle.
	     * initially off, with the Ap TF sensitive
	     * form is just to right-justify the indicator -- ugh
	     */

	    n = 0;
	    w = XmCreateForm (rc_w, "LF", args, n);
	    XtManageChild (w);

		n = 0;
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNset, False); n++;
		lockap_w = XmCreateToggleButton (w, "LockAp", args, n);
		XtAddCallback (lockap_w, XmNvalueChangedCallback, lockapCB, 0);
		XtManageChild (lockap_w);
		set_xmstring (lockap_w, XmNlabelString, "Lock");

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    w = XmCreateLabel (rc_w, "ILabel", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "Aperture");

	    /* title over hte data fields */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	    w = XmCreateLabel (rc_w, "LT", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "Centriod");

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    w = XmCreateLabel (rc_w, "RT", args, n);
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "Positions:");

	    /* output fields */

	    for (i = 0; i < XtNumber(ofields); i++) {
		Field *fp = &ofields[i];

		str = XmStringCreate (fp->label, XmSTRING_DEFAULT_CHARSET);
		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
		XtSetArg (args[n], XmNlabelString, str); n++;
		w = XmCreateLabel (rc_w, "OLabel", args, n);
		XtManageChild (w);
		XmStringFree (str);

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		w = XmCreateLabel (rc_w, fp->name, args, n);
		XtManageChild (w);
		set_xmstring (w, XmNlabelString, "");
		*(fp->wp) = w;
	    }

	/* separator */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (photom_w, "Sep1", args, n);
	XtManageChild (sep_w);

	/* mode radio box */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	rb_w = XmCreateRadioBox (photom_w, "ModeRB", args, n);
	XtManageChild (rb_w);

	    n = 0;
	    XtSetArg (args[n], XmNset, True); n++;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    w = XmCreateToggleButton (rb_w, "DiffMode", args, n);
	    set_xmstring (w, XmNlabelString, "Differential Photometry");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNset, False); n++;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    w = XmCreateToggleButton (rb_w, "AbsMode", args, n);
	    set_xmstring (w, XmNlabelString, "Absolute Photometry");
	    XtAddCallback (w, XmNvalueChangedCallback, modeCB, NULL);
	    XtManageChild (w);

	/* fat separator */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rb_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNseparatorType, XmDOUBLE_LINE); n++;
	sep_w = XmCreateSeparator (photom_w, "Sep2", args, n);
	XtManageChild (sep_w);

	/* add the diff child -- managed, since it's the default */
	bd_w = create_diff_w (sep_w);
	XtManageChild (diff_w);

	/* add the abs child */
	ba_w = createAbsPhot_w (photom_w, sep_w);

	/* put a close box clear at the bottom */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	close_w = XmCreatePushButton (photom_w, "Close", args, n);
	XtManageChild (close_w);
	XtAddCallback (close_w, XmNactivateCallback, closeCB, NULL);

	/* put a separator above the close button */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, close_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (photom_w, "Sep3", args, n);
	XtManageChild (sep_w);

	/* attach the bottom of the diff and abs controls to the sep */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, sep_w); n++;
	XtSetValues (bd_w, args, n);
	XtSetValues (ba_w, args, n);
}

/* create the differential dialog chunk.
 * it is a child of the main photom_w form widget.
 * start attaching at top_w.
 * don't connect anything to the bottom of photo_w but return the lowest widget
 *   made in the form.
 */
static Widget
create_diff_w (top_w)
Widget top_w;
{
	Widget w, rc_w;
	Arg args[20];
	int n;

	/* make an overall rowcolumn to be managed when want to display the
	 * differential mode fields.
	 * N.B. don't manage it here 
	 */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	diff_w = XmCreateRowColumn (photom_w, "DiffRC", args, n);

	/* make a 3-row row/col for the controls */
	n = 0; 
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNnumColumns, 3); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	rc_w = XmCreateRowColumn (diff_w, "DiffRC", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	    w = XmCreateLabel (rc_w, "DiffRefL", args, n);
	    set_xmstring (w, XmNlabelString, "Base Mag:");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNcolumns, 5); n++;
	    magref_w = XmCreateTextField (rc_w, "DiffRefT", args, n);
	    XmTextFieldSetString (magref_w, "0");
	    XtManageChild (magref_w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	    w = XmCreateLabel (rc_w, "MagL", args, n);
	    set_xmstring (w, XmNlabelString, "Mag:");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    mag_w = XmCreateLabel (rc_w, "DiffMagT", args, n);
	    set_xmstring (mag_w, XmNlabelString, duh);
	    XtManageChild (mag_w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	    w = XmCreateLabel (rc_w, "Err", args, n);
	    set_xmstring (w, XmNlabelString, "Mag Err:");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    magerr_w = XmCreateLabel (rc_w, "DiffErrT", args, n);
	    set_xmstring (magerr_w, XmNlabelString, duh);
	    XtManageChild (magerr_w);


	/* add a control to set the reference star */

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	w = XmCreatePushButton (diff_w, "SetRef", args, n);
	XtAddCallback (w, XmNactivateCallback, setRefCB, NULL);
	set_xmstring (w, XmNlabelString, "Use as Reference");
	XtManageChild (w);

	return (diff_w);
}

/* called to change between diff and abs mode.
 * we are connected to the AbsMode toggle button.
 */
/* ARGSUSED */
static void
modeCB (Widget w, XtPointer client, XtPointer call)
{
	wantAbs = XmToggleButtonGetState (w);

	if (wantAbs) {
	    XtUnmanageChild (diff_w);
	    manageAbsPhot (1);
	} else {
	    XtManageChild (diff_w);
	    manageAbsPhot (0);
	}
}

/* called whenever we are unmanaged by any means */
/* ARGSUSED */
static void
unmapCB (Widget w, XtPointer client, XtPointer call)
{
	manageAbsPhot (0);
	wantPhotom = 0;
}

/* called from the Close pushbutton */
/* ARGSUSED */
static void
closeCB (Widget w, XtPointer client, XtPointer call)
{
	/* unmanage -- it does all the rest */
	XtUnmanageChild (photom_w);
}

/* called when anything is changed in the manual aperature TF */
/* ARGSUSED */
static void
apCB (Widget w, XtPointer client, XtPointer call)
{
	/* disable the reference star */
	refStar.p = 0;
	diffReport();
}

/* called when the Lock Aperture TB changes state */
/* ARGSUSED */
static void
lockapCB (Widget w, XtPointer client, XtPointer call)
{
	int on = XmToggleButtonGetState (w);

	/* invalidate the reference if user has unlocked the radius */
	if (on) {
	    refStar.p = 0;
	    diffReport();
	}
}

/* report differential magnitude -- newStar wrt refStar */
static void
diffReport()
{
	if (refStar.p == 0) {
	    msg ("No reference star");
	    set_xmstring (mag_w, XmNlabelString, duh);
	    set_xmstring (magerr_w, XmNlabelString, duh);
	} else {
	    double m, dm;
	    double magref;
	    char str[64];
	    char *txt;

	    txt = XmTextFieldGetString (magref_w);
	    magref = atof (txt);
	    XtFree (txt);
	    if (starMag (&refStar, &newStar, &m, &dm) < 0) {
		(void) sprintf (str, ">%.3f", m + magref);
		set_xmstring (mag_w, XmNlabelString, str);
		set_xmstring (magerr_w, XmNlabelString, duh);
	    } else {
		(void) sprintf (str, "%.3f", m + magref);
		set_xmstring (mag_w, XmNlabelString, str);
		(void) sprintf (str, "%.3f", dm);
		set_xmstring (magerr_w, XmNlabelString, str);
	    }
	}
}

/* called when the current star is to be made the reference differential star */
/* ARGSUSED */
static void
setRefCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* save the new star as the reference.
	 * display it's aperature
	 */
	refStar = newStar;
	apReport (refStar.rAp);

	/* lock the current aperature.
	 */
	XmToggleButtonSetState (lockap_w, True, False);

	/* report the first maginitude */
	diffReport();
}

static void
makeGC()
{
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);
	XGCValues gcv;
	unsigned int gcm;
	Pixel p;

	if (get_color_resource (dsp, myclass, "PhotomColor", &p) < 0) {
	    msg ("Can't get PhotomColor -- using White");
	    p = WhitePixel (dsp, DefaultScreen(dsp));
	}
	gcm = GCForeground;
	gcv.foreground = p;
	photGC = XCreateGC (dsp, win, gcm, &gcv);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: photom.c,v $ $Date: 2001/04/19 21:12:00 $ $Revision: 1.1.1.1 $ $Name:  $"};
