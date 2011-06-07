/* code to support the preferences facility.
 */

#include <stdio.h>

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/ToggleB.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/Separator.h>

#include "P_.h"
#include "circum.h"
#include "preferences.h"


extern void redraw_screen P_((int how_much));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_manage P_((void));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void src_manage P_((void));
extern void srf_manage P_((void));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *msg, int app_modal));

char prefcategory[] = "Main -- Preferences";	/* Save category */

/* info to build each those preferences that are simple pairs */
typedef struct {
    int prefname;	/* one of Preferences enum */
    char *pdname;	/* pulldown name */
    char *tip;		/* tip text for the main cascade pair */
    char *cblabel;	/* cascade button label */
    char cbmne;		/* cascade button mnemonic character */
    XtCallbackProc cb;	/* callback function */
    int op1pref;	/* option 1 PREF code */
    char *op1name;	/* option 1 TB name */
    char op1mne;	/* option 1 TB mnemonic character */
    char *op1tip;	/* option 1 tip string */
    int op2pref;	/* option 2 PREF code */
    char *op2name;	/* option 2 TB name */
    char op2mne;	/* option 2 TB mnemonic character */
    char *op2tip;	/* option 2 tip string */
} PrefPair;

static void pref_topogeo_cb P_((Widget w, XtPointer client, XtPointer call));
static void pref_date_cb P_((Widget w, XtPointer client, XtPointer call));
static void pref_units_cb P_((Widget w, XtPointer client, XtPointer call));
static void pref_tz_cb P_((Widget w, XtPointer client, XtPointer call));
static void pref_dpy_prec_cb P_((Widget w, XtPointer client, XtPointer call));
static void pref_msg_bell_cb P_((Widget w, XtPointer client, XtPointer call));
static void pref_prefill_cb P_((Widget w, XtPointer client, XtPointer call));
static void pref_tips_cb P_((Widget w, XtPointer client, XtPointer call));
static void pref_confirm_cb P_((Widget w, XtPointer client, XtPointer call));
static void pref_simplepair P_((Widget pd, PrefPair *p));

static PrefPair prefpairs[] = {
    {PREF_EQUATORIAL, "Equatorial",
	"Whether RA/Dec values are topocentric or geocentric",
	"Equatorial", 'E', pref_topogeo_cb,
	PREF_TOPO, "Topocentric", 'T', "local perspective",
	PREF_GEO, "Geocentric", 'G', "Earth-centered perspective"
    },

    {PREF_DPYPREC, "Precision",
       "Whether numeric values are shown with more or fewer significant digits",
	"Precision", 'P', pref_dpy_prec_cb,
	PREF_HIPREC, "Hi", 'H', "display full precision",
	PREF_LOPREC, "Low", 'L', "use less room"
    },

    {PREF_MSG_BELL, "MessageBell",
	"Whether to beep when a message is added to the Message dialog",
	"Message Bell", 'M', pref_msg_bell_cb,
	PREF_NOMSGBELL, "Off", 'f', "other people are busy",
	PREF_MSGBELL, "On", 'O', "the beeps are useful"
    },

    {PREF_PRE_FILL, "PromptPreFill",
	"Whether prompt dialogs are prefilled with their current value",
	"Prompt Prefill", 'f', pref_prefill_cb,
	PREF_NOPREFILL, "No", 'N', "fresh prompt each time",
	PREF_PREFILL, "Yes", 'Y', "current value is often close"
    },

    {PREF_UNITS, "Units",
	"Whether to use english or metric units",
	"Units", 'U', pref_units_cb,
	PREF_ENGLISH, "English", 'E', "Feet, Fahrenheit",
	PREF_METRIC, "Metric", 'M', "Meters, Celsius"
    },

    {PREF_ZONE, "TZone",
	"Whether time stamps and the calendar are in local time or UTC",
	"Time zone", 'z', pref_tz_cb,
	PREF_LOCALTZ, "Local", 'L', "as per TZ Offset",
	PREF_UTCTZ, "UTC", 'U', "Coordinated Universal Time"
    },

    {PREF_TIPS, "Tips",
	"Whether to display these little tip boxes!",
	"Show help tips", 't', pref_tips_cb,
	PREF_NOTIPS, "No", 'N', "they are in the way",
	PREF_TIPSON, "Yes", 'Y', "they are faster than reading Help"
    },

    {PREF_CONFIRM, "Confirm",
	"Whether to ask before performing irreversible actions",
	"Confirmations", 'C', pref_confirm_cb,
	PREF_NOCONFIRM, "No", 'N', "just do it",
	PREF_CONFIRMON, "Yes", 'Y', "ask first"
    }
};

/* record of preferences values */
static int prefs[NPREFS];

/* Create "Preferences" PulldownMenu.
 * use the given menu_bar widget as a base.
 * this is called early when the main menu bar is being built..
 * initialize the prefs[] array from the initial state of the toggle buttons.
 * also, tack on some Save and resource controls.
 */
void
pref_create_pulldown (menu_bar)
Widget menu_bar;
{
	Widget w, cb_w, pd, pull_right;
	Widget tb1, tb2, tb3;
	Arg args[20];
	int i, n;

	/* make the pulldown */
	n = 0;
	pd = XmCreatePulldownMenu (menu_bar, "Preferences", args, n);

	/* add the simple preferences */
	for (i = 0; i < XtNumber(prefpairs); i++)
	    pref_simplepair (pd, &prefpairs[i]);


	/* add the date formats pullright menu -- it has 3 options */

	n = 0;
	XtSetArg (args[n], XmNradioBehavior, True); n++;
	pull_right = XmCreatePulldownMenu (pd, "DateFormat",args,n);

	    n = 0;
	    XtSetArg (args[n], XmNmnemonic, 'M'); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    tb1 = XmCreateToggleButton (pull_right, "MDY", args, n);
	    XtManageChild (tb1);
	    XtAddCallback (tb1, XmNvalueChangedCallback, pref_date_cb,
						    (XtPointer)PREF_MDY);
	    set_xmstring (tb1, XmNlabelString, "M/D/Y");
	    wtip (tb1, "Month / Day / Year");

	    n = 0;
	    XtSetArg (args[n], XmNmnemonic, 'Y'); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    tb2 = XmCreateToggleButton (pull_right, "YMD", args, n);
	    XtAddCallback (tb2, XmNvalueChangedCallback, pref_date_cb,
						    (XtPointer)PREF_YMD);
	    XtManageChild (tb2);
	    set_xmstring (tb2, XmNlabelString, "Y/M/D");
	    wtip (tb2, "Year / Month / Day");

	    n = 0;
	    XtSetArg (args[n], XmNmnemonic, 'D'); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    tb3 = XmCreateToggleButton (pull_right, "DMY", args, n);
	    XtAddCallback (tb3, XmNvalueChangedCallback, pref_date_cb,
						    (XtPointer)PREF_DMY);
	    XtManageChild (tb3);
	    set_xmstring (tb3, XmNlabelString, "D/M/Y");
	    wtip (tb3, "Day / Month / Year");

	    if (XmToggleButtonGetState(tb1))
		prefs[PREF_DATE_FORMAT] = PREF_MDY;
	    else if (XmToggleButtonGetState(tb2))
		prefs[PREF_DATE_FORMAT] = PREF_YMD;
	    else if (XmToggleButtonGetState(tb3))
		prefs[PREF_DATE_FORMAT] = PREF_DMY;
	    else {
		xe_msg ("No DateFormat preference is set -- defaulting to MDY", 0);
		XmToggleButtonSetState (tb1, True, False);
		prefs[PREF_DATE_FORMAT] = PREF_MDY;
	    }

	    sr_reg (tb1, NULL, prefcategory, 1);
	    sr_reg (tb2, NULL, prefcategory, 1);
	    sr_reg (tb3, NULL, prefcategory, 1);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pull_right);  n++;
	    XtSetArg (args[n], XmNmnemonic, 'D'); n++;
	    cb_w = XmCreateCascadeButton(pd,"DateFormatCB",args,n);
	    XtManageChild (cb_w);
	    set_xmstring (cb_w, XmNlabelString, "Date Formats");
	    wtip (cb_w, "Format for displaying dates");

	/* glue the pulldown to the menubar with a cascade button */

	n = 0;
	XtSetArg (args[n], XmNsubMenuId, pd);  n++;
	XtSetArg (args[n], XmNmnemonic, 'P'); n++;
	cb_w = XmCreateCascadeButton (menu_bar, "PreferencesCB", args, n);
	set_xmstring (cb_w, XmNlabelString, "Preferences");
	XtManageChild (cb_w);
	wtip (cb_w, "Options effecting overall XEphem operation");

	/* tack on some Save and other resource controls */

	n = 0;
	w = XmCreateSeparator (pd, "Sep", args, n);
	XtManageChild (w);

	n = 0;
	w = XmCreatePushButton (pd, "Fonts", args, n);
	XtAddCallback (w, XmNactivateCallback, (XtCallbackProc)srf_manage, 0);
	wtip (w, "Try different fonts");
	set_xmstring (w, XmNlabelString, "Fonts...");
	XtManageChild (w);

	n = 0;
	w = XmCreatePushButton (pd, "Colors", args, n);
	XtAddCallback (w, XmNactivateCallback, (XtCallbackProc)src_manage, 0);
	wtip (w, "Try different colors");
	set_xmstring (w, XmNlabelString, "Colors...");
	XtManageChild (w);

	n = 0;
	w = XmCreatePushButton (pd, "Save", args, n);
	XtAddCallback (w, XmNactivateCallback, (XtCallbackProc)sr_manage, 0);
	wtip (w, "Save settings to disk");
	set_xmstring (w, XmNlabelString, "Save...");
	XtManageChild (w);
}

/* called anytime we want to know a preference.
 */
int
pref_get(pref)
Preferences pref;
{
	return (prefs[pref]);
}

/* call to force a certain preference, return the old setting.
 * Use this wisely.. it does *not* change the menu system.
 * Invented to support forcing MDY in fs_date(), so far no other uses justified.
 */
int
pref_set (pref, new)
Preferences pref;
int new;
{
	int prior = pref_get(pref);
	prefs[pref] = new;
	return (prior);
}

/* return 1 if want to confirm, else 0 */
int
confirm()
{
	return (pref_get (PREF_CONFIRM) == PREF_CONFIRMON);
}

/* make one option pair.
 * the state of op1 determines the initial settings. to put it another way,
 * if neither option is set the *second* becomes the default.
 */
static void
pref_simplepair (pd, pp)
Widget pd;	/* parent pulldown menu */
PrefPair *pp;
{
	Widget pull_right, cb_w;
	Widget tb1, tb2;
	Arg args[20];
	int t;
	int n;

	n = 0;
	XtSetArg (args[n], XmNradioBehavior, True); n++;
	pull_right = XmCreatePulldownMenu (pd, pp->pdname, args,n);

	    n = 0;
	    XtSetArg (args[n], XmNmnemonic, pp->op1mne); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    tb1 = XmCreateToggleButton (pull_right, pp->op1name, args, n);
	    XtAddCallback (tb1, XmNvalueChangedCallback, pp->cb,
							(XtPointer)pp->op1pref);
	    if (pp->op1tip)
		wtip (tb1, pp->op1tip);
	    XtManageChild (tb1);
	    sr_reg (tb1, NULL, prefcategory, 1);

	    n = 0;
	    XtSetArg (args[n], XmNmnemonic, pp->op2mne); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    tb2 = XmCreateToggleButton (pull_right, pp->op2name, args, n);
	    XtAddCallback (tb2, XmNvalueChangedCallback, pp->cb,
							(XtPointer)pp->op2pref);
	    if (pp->op2tip)
		wtip (tb2, pp->op2tip);
	    XtManageChild (tb2);

	    t = XmToggleButtonGetState(tb1);
	    prefs[pp->prefname] = t ? pp->op1pref : pp->op2pref;
	    XmToggleButtonSetState (tb2, !t, False);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pull_right);  n++;
	    XtSetArg (args[n], XmNmnemonic, pp->cbmne); n++;
	    cb_w = XmCreateCascadeButton (pd, "PrefCB", args, n);
	    XtManageChild (cb_w);
	    set_xmstring (cb_w, XmNlabelString, pp->cblabel);
	    if (pp->tip)
		wtip (cb_w, pp->tip);
}

/* called when a PREF_DATE_FORMAT preference changes.
 * the new value is in client.
 */
/* ARGSUSED */
static void
pref_date_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)call;

	if (s->set) {
	    prefs[PREF_DATE_FORMAT] = (int)client;
	    redraw_screen (1);
	}
}

/* called when a PREF_UNITS preference changes.
 * the new value is in client.
 */
/* ARGSUSED */
static void
pref_units_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)call;

	if (s->set) {
	    prefs[PREF_UNITS] = (int)client;
	    redraw_screen (1);
	}
}

/* called when a PREF_ZONE preference changes.
 * the new value is in client.
 */
/* ARGSUSED */
static void
pref_tz_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)call;

	if (s->set) {
	    prefs[PREF_ZONE] = (int)client;
	    redraw_screen (1);
	}
}

/* called when a PREF_DPYPREC preference changes.
 * the new value is in client.
 */
/* ARGSUSED */
static void
pref_dpy_prec_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)call;

	if (s->set) {
	    prefs[PREF_DPYPREC] = (int)client;
	    redraw_screen (1);
	}
}

/* called when a PREF_EQUATORIAL preference changes.
 * the new value is in client.
 */
/* ARGSUSED */
static void
pref_topogeo_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)call;

	if (s->set) {
	    prefs[PREF_EQUATORIAL] = (int)client;
	    redraw_screen (1);
	}
}

/* called when a PREF_MSG_BELL preference changes.
 * the new value is in client.
 */
/* ARGSUSED */
static void
pref_msg_bell_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)call;

	if (s->set) {
	    prefs[PREF_MSG_BELL] = (int)client;
	}
}

/* called when a PREF_PRE_FILL preference changes.
 * the new value is in client.
 */
/* ARGSUSED */
static void
pref_prefill_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)call;

	if (s->set) {
	    prefs[PREF_PRE_FILL] = (int)client;
	}
}

/* called when a PREF_TIPS preference changes.
 * the new value is in client.
 */
/* ARGSUSED */
static void
pref_tips_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)call;

	if (s->set) {
	    prefs[PREF_TIPS] = (int)client;
	}
}

/* called when a PREF_CONFIRM preference changes.
 * the new value is in client.
 */
/* ARGSUSED */
static void
pref_confirm_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)call;

	if (s->set) {
	    prefs[PREF_CONFIRM] = (int)client;
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: preferences.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
