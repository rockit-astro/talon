/* code to manage specifying the local horizon */

#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
typedef const void * qsort_arg;
#else
typedef void * qsort_arg;
#endif

#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/FileSB.h>
#include <Xm/ToggleB.h>
#include <Xm/TextF.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "patchlevel.h"

extern Widget toplevel_w;
#define XtD     XtDisplay(toplevel_w)
extern Colormap xe_cm;
extern XtAppContext xe_app;

extern FILE *fopenh P_((char *name, char *how));
extern Now *mm_get_now P_((void));
extern char *getPrivateDir P_((void));
extern char *getShareDir P_((void));
extern char *syserrstr P_((void));
extern void get_xmstring P_((Widget w, char *resource, char **txtp));
extern void defaultTextFN P_((Widget w, int setcols, char *x, char *y));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void prompt_map_cb P_((Widget w, XtPointer client, XtPointer call));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void sv_update P_((Now *np, int how_much));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *msg, int app_modal));

static int hzn_rdmap P_((void));

static void hzn_create P_((void));
static void hzn_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void hzn_popdown_cb P_((Widget w, XtPointer client, XtPointer call));
static void hzn_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void hzn_browse_cb P_((Widget w, XtPointer client, XtPointer call));
static void hzn_displtb_cb P_((Widget w, XtPointer client, XtPointer call));
static void hzn_displtf_cb P_((Widget w, XtPointer client, XtPointer call));
static void hzn_filetb_cb P_((Widget w, XtPointer client, XtPointer call));
static void hzn_filetf_cb P_((Widget w, XtPointer client, XtPointer call));
static void hzn_getdispl P_((void));
static void hzn_choose P_((int choose_displ));

static Widget hznshell_w;	/* the main shell */
static Widget displtb_w;	/* fixed displacement TB */
static Widget displtf_w;	/* fixed displacement TF */
static Widget filetb_w;		/* file name TB */
static Widget filetf_w;		/* file name TF */
static Widget fsb_w;		/* browser FSB */

static int usedispl;		/* flag set when using fixed displ, else map */
static double displ;		/* displacement above horizon, rads */

typedef struct {
    float az, alt;		/* profile, rads E of N and up, respectively */
} Profile;
static Profile *profile;	/* malloced Profile[nprofile] sorted by inc az*/
static int nprofile;		/* entries in profile[] */

static char hzncategory[] = "Horizon Map";	/* Save category */

void
hzn_manage()
{
	if (!hznshell_w)
	    hzn_create();

        XtPopup (hznshell_w, XtGrabNone);
	set_something (hznshell_w, XmNiconic, (XtArgVal)False);
}

/* called to put up or remove the watch cursor.  */
void
hzn_cursor (c)
Cursor c;
{
	Window win;

	if (hznshell_w && (win = XtWindow(hznshell_w)) != 0) {
	    Display *dsp = XtDisplay(hznshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* given an az, return the horizon altitude, both in rads.
 */
double
hznAlt(az)
double az;
{
	Profile *pb, *pt;
	double daz;
	int t, b;

	/* always need the menu to get the default options */
	if (!hznshell_w)
	    hzn_create();

	/* displacement is the same always */
	if (usedispl)
	    return (displ);

	/* binary bracket */
	range (&az, 2*PI);
	t = nprofile - 1;
	b = 0;
	while (b < t-1) {
	    int m = (t+b)/2;
	    double maz = profile[m].az;
	    if (az < maz)
		t = m;
	    else
		b = m;
	}

	pt = &profile[t];
	pb = &profile[b];
	daz = pt->az - pb->az;
	if (b == t || daz == 0)
	    return (pb->alt);
	return (pb->alt + (az - pb->az)*(pt->alt - pb->alt)/daz);
}

static void
hzn_create()
{
	Widget f_w, rc_w;
	Widget w;
	Arg args[20];
	int n;

	/* create shell and main form */

	n = 0;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle,"xephem Sky Horizon setup");n++;
	XtSetArg (args[n], XmNiconName, "Horizon"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	hznshell_w = XtCreatePopupShell ("Horizon",topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (hznshell_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (hznshell_w, XmNpopdownCallback, hzn_popdown_cb, NULL);
	sr_reg (hznshell_w, "XEphem*Horizon.x", hzncategory, 0);
	sr_reg (hznshell_w, "XEphem*Horizon.y", hzncategory, 0);

	n = 0;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	f_w = XmCreateForm (hznshell_w, "HznForm", args, n);
        XtAddCallback (f_w, XmNhelpCallback, hzn_help_cb, NULL);
	XtManageChild (f_w);

	/* controls at the bottom */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 22); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 38); n++;
	w = XmCreatePushButton (f_w, "Close", args, n);
        XtAddCallback (w, XmNactivateCallback, hzn_close_cb, NULL);
	wtip (w, "Close this window");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 62); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 78); n++;
	w = XmCreatePushButton (f_w, "Help", args, n);
        XtAddCallback (w, XmNactivateCallback, hzn_help_cb, NULL);
	wtip (w, "Get more information about this window");
	XtManageChild (w);

	/* most stuff is in a RC */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	XtSetArg (args[n], XmNspacing, 5); n++;
	rc_w = XmCreateRowColumn (f_w, "RC", args, n);
	XtManageChild (rc_w);

	/* title */

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	w = XmCreateLabel (rc_w, "T", args, n);
	set_xmstring (w, XmNlabelString,
				"Choose method for specifying local Horizon");
	XtManageChild (w);

	/* fixed */

	n = 0;
	w = XmCreateLabel (rc_w, "Gap", args, n);
	set_xmstring (w, XmNlabelString, " ");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	displtb_w = XmCreateToggleButton (rc_w, "UseDisplacement", args, n);
	XtAddCallback (displtb_w, XmNvalueChangedCallback, hzn_displtb_cb, 0);
	wtip (displtb_w, "Define horizon with one altitude at all azimuths");
	usedispl = XmToggleButtonGetState(displtb_w);
	sr_reg (displtb_w, NULL, hzncategory, 1);
	set_xmstring (displtb_w, XmNlabelString,
				"Fixed displacement, Degrees above horizon:");
	XtManageChild (displtb_w);

	n = 0;
	XtSetArg (args[n], XmNcolumns, 30); n++;
	displtf_w = XmCreateTextField (rc_w, "Displacement", args, n);
	wtip (displtf_w, "Angle of Horizon cutoff above local horizontal");
	XtAddCallback (displtf_w, XmNactivateCallback, hzn_displtf_cb, 0);
	sr_reg (displtf_w, NULL, hzncategory, 1);
	XtManageChild (displtf_w);

	/* file */

	n = 0;
	w = XmCreateLabel (rc_w, "Gap", args, n);
	set_xmstring (w, XmNlabelString, " ");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	filetb_w = XmCreateToggleButton (rc_w, "UseMapFile", args, n);
	wtip (filetb_w, "Define horizon as a table of altitudes and azimuths");
	XtAddCallback (filetb_w, XmNvalueChangedCallback, hzn_filetb_cb, 0);
	set_xmstring (filetb_w, XmNlabelString,
					    "Horizon profile, map file name:");
	XtManageChild (filetb_w);

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	w = XmCreatePushButton (rc_w, "Browse...", args, n);
	wtip (w, "Select a file containing a horizon table");
	XtAddCallback (w, XmNactivateCallback, hzn_browse_cb, 0);
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNcolumns, 30); n++;
	filetf_w = XmCreateTextField (rc_w, "MapFilename", args, n);
	wtip (filetf_w, "Name of file containing horizon table");
	defaultTextFN (filetf_w, 1, getShareDir(), "auxil/sample.hzn");
	XtAddCallback (filetf_w, XmNactivateCallback, hzn_filetf_cb, 0);
	sr_reg (filetf_w, NULL, hzncategory, 1);
	XtManageChild (filetf_w);

	n = 0;
	w = XmCreateLabel (rc_w, "Gap", args, n);
	set_xmstring (w, XmNlabelString, " ");
	XtManageChild (w);

	/* perform the default */
	hzn_choose (usedispl);
}

/* called from Close */
/* ARGSUSED */
static void
hzn_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* outta here */
	XtPopdown (hznshell_w);
}

/* called from Ok */
/* ARGSUSED */
static void
hzn_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        static char *msg[] = {"Specify local horizon."};

	hlp_dialog ("Horizon", msg, sizeof(msg)/sizeof(msg[0]));

}

/* callback from the FSB Cancel PB */
/* ARGSUSED */
static void
hzn_fsbcancel_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (fsb_w)
	    XtUnmanageChild (fsb_w);
}

/* callback from the FSB Ok PB */
/* ARGSUSED */
static void
hzn_fsbok_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (fsb_w) {
	    char *fn;

	    /* copy file name to our structures */
	    get_xmstring (fsb_w, XmNdirSpec, &fn);
	    XmTextFieldSetString (filetf_w, fn);
	    XtFree (fn);
	    XtUnmanageChild (fsb_w);

	    /* implies choosing file name form */
	    hzn_choose(0);
	}
}

/* called from the Browse PB */
/* ARGSUSED */
static void
hzn_browse_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!fsb_w) {
	    Widget w;
	    Arg args[20];
	    int n;

	    n = 0;
	    XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	    XtSetArg (args[n], XmNtitle, "Horizon Profile File browser"); n++;
	    XtSetArg (args[n], XmNwidth, 400); n++;
	    XtSetArg (args[n], XmNmarginHeight, 10); n++;
	    XtSetArg (args[n], XmNmarginWidth, 10); n++;
	    fsb_w = XmCreateFileSelectionDialog (toplevel_w, "HFSB", args, n);
	    XtAddCallback (fsb_w, XmNmapCallback, prompt_map_cb, NULL);
	    XtAddCallback (fsb_w, XmNokCallback, hzn_fsbok_cb, NULL);
	    XtAddCallback (fsb_w, XmNcancelCallback, hzn_fsbcancel_cb, NULL);
	    set_xmstring (fsb_w, XmNcancelLabelString, "Close");

	    /* set default dir and pattern */
	    set_xmstring (fsb_w, XmNdirectory, getPrivateDir());
	    set_xmstring (fsb_w, XmNpattern, "*.hzn");

	    w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_HELP_BUTTON);
	    XtUnmanageChild (w);
	}

	XtManageChild (fsb_w);
}

/* callback when the main hzn window is closed */
/* ARGSUSED */
static void
hzn_popdown_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (fsb_w)
	    XtUnmanageChild (fsb_w);
}

/* Displacement TB callback */
/* ARGSUSED */
static void
hzn_displtb_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	hzn_choose (XmToggleButtonGetState (w));
}

/* Displacement TF callback */
/* ARGSUSED */
static void
hzn_displtf_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	hzn_choose (1);
}

/* File TB callback */
/* ARGSUSED */
static void
hzn_filetb_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	hzn_choose (!XmToggleButtonGetState (w));
}

/* File TF callback */
/* ARGSUSED */
static void
hzn_filetf_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	hzn_choose (0);
}


/* get displacement string into displ */
static void
hzn_getdispl()
{
	char *str = XmTextFieldGetString (displtf_w);
	displ = degrad(atof (str));
	XtFree (str);
}

/* called to make a choice.
 * control the Tbs as well as do the work.
 */
static void
hzn_choose (choose_displ)
int choose_displ;
{
	/* do the work */
	if (choose_displ)
	    hzn_getdispl();
	else {
	    if (hzn_rdmap() < 0)
		choose_displ = 1;	/* revert back */
	}

	/* implement radio box behavior */
        XmToggleButtonSetState (displtb_w, choose_displ, False);
        XmToggleButtonSetState (filetb_w, !choose_displ, False);
	usedispl = choose_displ;

	/* sky view update */
	sv_update (mm_get_now(), 1);

}

/* compare two Profiles' az, in qsort fashion */
static int
azcmp_f (v1, v2)
qsort_arg v1;
qsort_arg v2;
{
	double diff = ((Profile *)v1)->az - ((Profile *)v2)->az;
	return (diff < 0 ? -1 : (diff > 0 ? 1 : 0));
}

/* open the horizon map named in filetf_w and create a profile[] sorted by
 * increasing az.
 * we make sure the profile has complete coverage from 0..360 degrees.
 * return 0 if ok, else -1
 */
static int
hzn_rdmap()
{
	char *fn = XmTextFieldGetString (filetf_w);
	double az, alt;
	char buf[1024];
	FILE *fp;

	/* open file */
	fp = fopenh (fn, "r");
	if (!fp) {
	    sprintf (buf, "%s:\n%s", fn, syserrstr());
	    xe_msg (buf, 1);
	    XtFree (fn);
	    return (-1);
	}

	/* reset profile */
	if (profile) {
	    XtFree ((char *)profile);
	    profile = NULL;
	}
	nprofile = 0;

	/* read and store in profile[] */
	while (fgets (buf, sizeof(buf), fp)) {
	    if (sscanf (buf, "%lf %lf", &az, &alt) != 2)
		continue;
	    profile = (Profile *) XtRealloc ((void*)profile,
						(nprofile+1)*sizeof(Profile));
	    range (&az, 360.0);
	    profile[nprofile].az = (float) degrad(az);
	    if (alt > 90)
		alt = 90;
	    if (alt < -90)
		alt = -90;
	    profile[nprofile].alt = (float) degrad(alt);
	    nprofile++;
	}
	fclose (fp);

	if (nprofile == 0) {
	    sprintf (buf, "%s:\nFound no horizon entries.\nReverting to fixed displacement.", fn);
	    xe_msg (buf, 1);
	    XtFree (fn);
	    return (-1);
	} else {
	    sprintf (buf, "%s:\nRead %d horizon entr%s.", fn, nprofile,
						nprofile == 1 ? "y" : "ies");
	    xe_msg (buf, 0);
	}

	/* if get here nprofile > 0 */

	/* sort by az */
	qsort ((void *)profile, nprofile, sizeof(Profile), azcmp_f);


	/* insure full 0..360 coverage */
	if (profile[0].az > 0 || profile[nprofile-1].az < 2*PI) {
	    /* expand for 2 more, one on each end */
	    profile = (Profile *) XtRealloc ((void*)profile,
	    					(nprofile+2)*sizeof(Profile));
	    memmove ((void *)(profile+1), (void *)profile,
						    nprofile*sizeof(Profile));
	    nprofile += 2;

	    /* fill each end with the alt for the az closest to 0 */
	    profile[0].az = 0;
	    profile[nprofile-1].az = (float)(2*PI);
	    if (profile[1].az < 2*PI-profile[nprofile-2].az)
		profile[0].alt = profile[nprofile-1].alt = profile[1].alt;
	    else
		profile[0].alt = profile[nprofile-1].alt =
							profile[nprofile-2].alt;
	}

	/* done with filename */
	XtFree (fn);
	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: hznmenu.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
