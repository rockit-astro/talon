/* code to manage TLEs over the web */

#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#endif

#if defined(_POSIX_SOURCE)
#include <unistd.h>
#else
extern int close();
#endif

#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "net.h"
#include "patchlevel.h"

extern Widget toplevel_w;
#define XtD     XtDisplay(toplevel_w)
extern Colormap xe_cm;
extern XtAppContext xe_app;


extern FILE *fopenh P_((char *name, char *how));
extern char *getPrivateDir P_((void));
extern char *syserrstr P_((void));
extern int confirm P_((void));
extern int existsh P_((char *filename));
extern void XCheck P_((XtAppContext app));
extern void defaultTextFN P_((Widget w, int setcols, char *x, char *y));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void obj_set P_((Obj *op, int dbidx));
extern void query P_((Widget tw, char *msg, char *label1, char *label2,
    char *label3, void (*func1)(void), void (*func2)(void),
    void (*func3)(void)));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *msg, int app_modal));

static int fetchTLE P_((char *name, char *url));
static int tle2edb P_((char buf[]));
static void wt_create P_((void));
static void close_cb P_((Widget w, XtPointer client, XtPointer call));
static void erasetle_cb P_((Widget w, XtPointer client, XtPointer call));
static void assign_cb P_((Widget w, XtPointer client, XtPointer call));
static void help_cb P_((Widget w, XtPointer client, XtPointer call));
static void get_cb P_((Widget w, XtPointer client, XtPointer call));
static void savedb_cb P_((Widget w, XtPointer client, XtPointer call));
static void autoSaveName P_((char *name));
static void mk_urlrow P_((Widget rc_w, int i));
static void mk_save P_((Widget rc_w));
static void mk_assigns P_((Widget rc_w));
static void mk_tle P_((Widget rc_w));
static void mk_gap P_((Widget rc_w));

static Widget wtshell_w;	/* the main shell */
static Widget tle_w;		/* raw TLE text */
static Widget savedb_w;		/* save TLE filename text */
static Widget autoname_w;	/* TB whether to automatically set file name */

typedef struct {
    Widget name_w;		/* name Text widget */
    Widget url_w;		/* url Text widget */
} URLRow;
static URLRow urlrows[4];	/* one row for each satellite supported */

static char wtcategory[] = "Satellite TLE";	/* Save category */

void
wt_manage()
{
	if (!wtshell_w)
	    wt_create();

        XtPopup (wtshell_w, XtGrabNone);
	set_something (wtshell_w, XmNiconic, (XtArgVal)False);
}

/* called to put up or remove the watch cursor.  */
void
wt_cursor (c)
Cursor c;
{
	Window win;

	if (wtshell_w && (win = XtWindow(wtshell_w)) != 0) {
	    Display *dsp = XtDisplay(wtshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

static void
wt_create()
{
	Widget f_w, rc_w;
	Widget w;
	int i;
	Arg args[20];
	int n;

	/* create shell and main form */

	n = 0;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle,"xephem Web Satellite update");n++;
	XtSetArg (args[n], XmNiconName, "TLE"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	wtshell_w = XtCreatePopupShell ("WebTLE",topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (wtshell_w, XmNcolormap, (XtArgVal)xe_cm);
	sr_reg (wtshell_w, "XEphem*WebTLE.x", wtcategory, 0);
	sr_reg (wtshell_w, "XEphem*WebTLE.y", wtcategory, 0);

	n = 0;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	f_w = XmCreateForm (wtshell_w, "TLEForm", args, n);
        XtAddCallback (f_w, XmNhelpCallback, help_cb, NULL);
	XtManageChild (f_w);

	/* controls at the bottom */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 22); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 38); n++;
	w = XmCreatePushButton (f_w, "Close", args, n);
        XtAddCallback (w, XmNactivateCallback, close_cb, NULL);
	wtip (w, "Close this window");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 62); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 78); n++;
	w = XmCreatePushButton (f_w, "Help", args, n);
        XtAddCallback (w, XmNactivateCallback, help_cb, NULL);
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
	rc_w = XmCreateRowColumn (f_w, "RC", args, n);
	XtManageChild (rc_w);

	/* title */

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	w = XmCreateLabel (rc_w, "T", args, n);
	set_xmstring (w, XmNlabelString,
	    "Earth Satellite Two-Line-Element Orbital Elements Web Updates");
	XtManageChild (w);

	/* URLs */

	mk_gap (rc_w);

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	w = XmCreateLabel (rc_w, "UT", args, n);
	set_xmstring (w, XmNlabelString,
				"Get named Satellite TLE from http URL:");
	XtManageChild (w);

	    for (i = 0; i < XtNumber(urlrows); i++)
		mk_urlrow (rc_w, i);

	/* Raw TLE */

	mk_gap (rc_w);
	mk_tle (rc_w);

	/* Assign */

	mk_gap (rc_w);
	mk_assigns (rc_w);

	/* Save */

	mk_gap (rc_w);
	mk_save (rc_w);

	mk_gap (rc_w);
}

/* called from Close */
/* ARGSUSED */
static void
close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* outta here */
	XtPopdown (wtshell_w);
}

/* called to erase the TLE text field */
/* ARGSUSED */
static void
erasetle_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmTextSetString (tle_w, "");
}

/* called from any of the Assign PBs.
 * client is one of OBJX/Y/Z
 */
/* ARGSUSED */
static void
assign_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int obji = (int) client;
	char buf[512];
	Obj o;

	if (tle2edb(buf) < 0)
	    return;
	db_crack_line (buf, &o, NULL);
	obj_set (&o, obji);
	if (XmToggleButtonGetState (autoname_w))
	    autoSaveName (o.o_name);
}

/* called from any of the Get PBs.
 * client is index into urlrows[].
 */
/* ARGSUSED */
static void
get_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int rowi = (int)client;
	char *name = XmTextFieldGetString(urlrows[rowi].name_w);
	char *url = XmTextFieldGetString(urlrows[rowi].url_w);

	if (!fetchTLE (name, url) && XmToggleButtonGetState (autoname_w))
	    autoSaveName (name);

	XtFree (name);
	XtFree (url);
}

static void
savedb_go (append)
int append;
{
	char buf[256];
	FILE *fp;
	char *fn;

	/* convert tle to edb */
	if (tle2edb(buf) < 0)
	    return;

	/* open file */
	fn = XmTextFieldGetString (savedb_w);
	fp = fopenh (fn, append ? "a" : "w");
	if (!fp) {
	    sprintf (buf, "%s:\n%s", fn, syserrstr());
	    xe_msg (buf, 1);
	    XtFree (fn);
	    return;
	}

	/* write file */
	fprintf (fp, "%s\n", buf);
	fclose (fp);
	sprintf (buf, "%s:\nWritten successfully", fn);
	xe_msg(buf, confirm());

	/* clean up */
	XtFree (fn);
}

static void
savedb_append()
{
	savedb_go(1);
}

static void
savedb_overwrite()
{
	savedb_go(0);
}

/* called from the Save .edb PB.
 */
/* ARGSUSED */
static void
savedb_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char *fn;

	fn = XmTextFieldGetString (savedb_w);
	if (existsh (fn) == 0 && confirm()) {
	    char buf[512];
	    (void) sprintf (buf, "%s exists:\nAppend or Overwrite?", fn);
	    query (toplevel_w, buf, "Append", "Overwrite", "Cancel",
				savedb_append, savedb_overwrite, NULL);
	} else
	    savedb_overwrite();
	XtFree (fn);
}

/* called from Ok */
/* ARGSUSED */
static void
help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        static char *msg[] = {"Update Satellite TLE from Web."};

	hlp_dialog ("WebTLE", msg, sizeof(msg)/sizeof(msg[0]));

}

/* make URL row in the given parrent RC based on urlrows[i].
 * widget names are Name<i> and URL<i>
 */
static void
mk_urlrow (prc_w, i)
Widget prc_w;
int i;
{
	URLRow *up = &urlrows[i];
	Widget rc_w;
	Widget w;
	char name[32];
	Arg args[20];
	int n;

	/* main horizontal RC */

	n = 0;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNspacing, 6); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	rc_w = XmCreateRowColumn (prc_w, "URLs", args, n);
	XtManageChild (rc_w);

	/* "Get" PB */

	n = 0;
	w = XmCreatePushButton (rc_w, "Get", args, n);
	XtAddCallback (w, XmNactivateCallback, get_cb, (XtPointer)i);
	wtip (w, "Fetch this satellite from the Web");
	XtManageChild (w);

	/* Name TF */

	n = 0;
	sprintf (name, "Name%d", i);
	XtSetArg (args[n], XmNcolumns, 13); n++;
	w = XmCreateTextField (rc_w, name, args, n);
	XtManageChild (w);
	sr_reg (w, NULL, wtcategory, 1);
	up->name_w = w;
	wtip (w, "String which must be part of satellite name.");

	/* URL TF */

	n = 0;
	sprintf (name, "URL%d", i);
	XtSetArg (args[n], XmNcolumns, 59); n++;
	w = XmCreateTextField (rc_w, name, args, n);
	XtManageChild (w);
	sr_reg (w, NULL, wtcategory, 1);
	up->url_w = w;
	wtip (w, "URL to fetch and search for named satellite");
}

/* make the Save table entry off the given parent RC
 */
static void
mk_save (prc_w)
Widget prc_w;
{
	Widget rc_w;
	Widget w;
	Arg args[20];
	int n;

	/* title line and control */

	n = 0;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg (args[n], XmNspacing, 20); n++;
	rc_w = XmCreateRowColumn (prc_w, "SRC", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    w = XmCreateLabel (rc_w, "VT", args, n);
	    set_xmstring (w, XmNlabelString, "Save Satellite TLE:");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
	    XtSetArg (args[n], XmNindicatorOn, True); n++;
	    autoname_w = XmCreateToggleButton (rc_w, "AutoName", args, n);
	    set_xmstring (autoname_w, XmNlabelString, "Auto file name");
	    wtip (autoname_w,
		    "If On, set file name when a TLE is retrieved or assigned");
	    XtManageChild (autoname_w);
	    sr_reg (autoname_w, NULL, wtcategory, 1);


	/* main controls */

	n = 0;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNspacing, 6); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	rc_w = XmCreateRowColumn (prc_w, "URLs", args, n);
	XtManageChild (rc_w);

	/* "Save" PB */

	n = 0;
	w = XmCreatePushButton (rc_w, "Save", args, n);
	XtAddCallback (w, XmNactivateCallback, savedb_cb, 0);
	XtManageChild (w);
	wtip (w, "Save the Satellite TLE above to the given file name");

	/* label */

	n = 0;
	w = XmCreateLabel (rc_w, "FN", args, n);
	set_xmstring (w, XmNlabelString, "File name:");
	XtManageChild (w);

	/* filename TF */

	n = 0;
	XtSetArg (args[n], XmNcolumns, 60); n++;
	savedb_w = XmCreateTextField (rc_w, "EDBFileName", args, n);
	defaultTextFN (savedb_w, 0, getPrivateDir(), "xxx.edb");
	XtManageChild (savedb_w);
	wtip (savedb_w,"File name to in which to save the above Satellite TLE");
}

/* make the TLE Text off the given parent RC
 */
static void
mk_tle (prc_w)
Widget prc_w;
{
	Widget w, hrc_w;
	Arg args[20];
	int n;

	/* title line */

	n = 0;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg (args[n], XmNspacing, 20); n++;
	hrc_w = XmCreateRowColumn (prc_w, "TRC", args, n);
	XtManageChild (hrc_w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    w = XmCreateLabel (hrc_w, "ST", args, n);
	    set_xmstring (w, XmNlabelString, "Satellite TLE:");
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (hrc_w, "Erase", args, n);
	    wtip (w, "Clear out the TLE text box");
	    XtAddCallback (w, XmNactivateCallback, erasetle_cb, 0);
	    XtManageChild (w);

	/* text field */

	n = 0;
	XtSetArg (args[n], XmNeditable, True); n++;
	XtSetArg (args[n], XmNrows, 3); n++;
	XtSetArg (args[n], XmNcolumns, 69); n++;
	XtSetArg (args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	tle_w = XmCreateText (prc_w, "TLE", args, n);
	XtManageChild (tle_w);
	wtip(tle_w, "Name of satellite followed by Two-Line-Element");
}

/* make the Assign to ObjXYZ Pbs off the given parent RC
 */
static void
mk_assigns (prc_w)
Widget prc_w;
{
	Widget rc_w, w;
	Arg args[20];
	int n;

	/* title line */

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	w = XmCreateLabel (prc_w, "AT", args, n);
	set_xmstring (w, XmNlabelString,
			    "Assign Satellite TLE to User Defined Object:");
	XtManageChild (w);


	/* Hor RC for controls */

	n = 0;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNspacing, 10); n++;
	XtSetArg (args[n], XmNadjustLast, False); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	rc_w = XmCreateRowColumn (prc_w, "URLs", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    XtSetArg (args[n], XmNmarginWidth, 10); n++;
	    w = XmCreatePushButton (rc_w, "X", args, n);
	    set_xmstring (w, XmNlabelString, "Assign to ObjX");
	    XtAddCallback (w, XmNactivateCallback, assign_cb, (XtPointer)OBJX);
	    XtManageChild (w);
	    wtip (w, "Assign above Satellite TLE to ObjX");

	    n = 0;
	    XtSetArg (args[n], XmNmarginWidth, 10); n++;
	    w = XmCreatePushButton (rc_w, "Y", args, n);
	    set_xmstring (w, XmNlabelString, "Assign to ObjY");
	    XtAddCallback (w, XmNactivateCallback, assign_cb, (XtPointer)OBJY);
	    XtManageChild (w);
	    wtip (w, "Assign above Satellite TLE to ObjY");

	    n = 0;
	    XtSetArg (args[n], XmNmarginWidth, 10); n++;
	    w = XmCreatePushButton (rc_w, "Z", args, n);
	    set_xmstring (w, XmNlabelString, "Assign to ObjZ");
	    XtAddCallback (w, XmNactivateCallback, assign_cb, (XtPointer)OBJZ);
	    XtManageChild (w);
	    wtip (w, "Assign above Satellite TLE to ObjZ");
}

/* make a blank label for use as a gap in the given RC */
static void
mk_gap (rc_w)
Widget rc_w;
{
	Widget w = XmCreateLabel (rc_w, " ", NULL, 0);
	XtManageChild (w);
}

/* get the TLE for the named satellite from the given URL
 * return 0 if all ok else -1.
 */
static int
fetchTLE (name, url)
char *name, *url;
{
	static char http[] = "http://";
	char buf[512], msg[128];
	char l0[128], l1[128], l2[128];
	char *l0p = l0, *l1p = l1, *l2p = l2;
	char host[128];
	char *slash;
	int sockfd;
	Obj o;
	int found;

	watch_cursor(1);

	if (strncmp (url, http, 7)) {
	    sprintf (msg, "URL must begin with %s", http);
	    xe_msg (msg, 1);
	    watch_cursor (0);
	    return (-1);
	}

	slash = strchr (url+7, '/');
	if (!slash) {
	    sprintf (msg, "Badly formed URL");
	    xe_msg (msg, 1);
	    watch_cursor (0);
	    return (-1);
	}

	stopd_up();

	sprintf (host, "%.*s", slash-url-7, url+7);
	sprintf (buf, "%s HTTP/1.0\r\nUser-Agent: xephem/%s\r\n\r\n", slash, PATCHLEVEL);
	sockfd = httpGET (host, buf, msg);
	if (sockfd < 0) {
	    xe_msg (msg, 1);
	    stopd_down();
	    watch_cursor (0);
	    return (-1);
	}

	found = 0;
	l0[0] = l1[0] = '\0';
	while (recvlineb (sockfd, l2p, sizeof(l2)) > 0) {
	    char *lswap;
	    XCheck (xe_app);	/* looks more lively while working */
	    if (!db_tle (l0p, l1p, l2p, &o) && strstr (l0p, name)) {
		l0p[strlen(l0p)-1] = '\0';
		l1p[strlen(l1p)-1] = '\0';
		l2p[strlen(l2p)-1] = '\0';
		sprintf (buf, "%s\n%s\n%s", l0p, l1p, l2p);
		XmTextSetString (tle_w, buf);
		found = 1;
		break;
	    }
	    lswap = l0p;
	    l0p = l1p;
	    l1p = l2p;
	    l2p = lswap;

	}

	close (sockfd);

	stopd_down();

	if (!found) {
	    sprintf (buf, "%s\nNot found in URL", name);
	    xe_msg (buf, 1);
	    watch_cursor(0);
	    return (-1);
	}

	watch_cursor(0);
	return (0);
}

/* convert text in tle_w into .edb format.
 * return 0 if ok, else -1
 */
static int
tle2edb (buf)
char buf[];
{
	char *name, *l1, *l2;
	char *str;
	Obj o;

	str = XmTextGetString (tle_w);
	name = str;
	if (!(l1 = strchr (str, '\n')) || !(l2 = strchr (l1+1, '\n'))
				       || db_tle (name, l1+1, l2+1, &o) < 0) {
	    sprintf (buf, "Bad TLE format");
	    xe_msg (buf, 1);
	    XtFree (str);
	    return (-1);
	}
	db_write_line (&o, buf);
	XtFree (str);
	return (0);
}

/* insert the given satellite name into the current Save name if possible.
 * omit any punctuation.
 */
static void
autoSaveName (name)
char *name;
{
	char *fn;
	char *dotedb;

	/* replace from .edb suffix back to first sep */
	fn = XmTextFieldGetString (savedb_w);
	dotedb = strstr (fn, ".edb");
	if (dotedb) {
	    char *newfn = XtMalloc (strlen(fn)+strlen(name)+100);
	    char *nopun = XtMalloc (strlen(name) + 100);
	    char *ap, *op;

	    /* back up to beginning of basename */
	    while (dotedb >= fn && *dotedb != '/' && *dotedb != '\\')
		--dotedb;
	    dotedb++;

	    /* make copy of name without punct */
	    for (ap = name, op = nopun; *name; name++)
		if (isalnum(*name))
		    *op++ = *name;
	    *op = '\0';

	    /* new file name */
	    sprintf (newfn, "%.*s%s.edb", dotedb-fn, fn, nopun);
	    XmTextFieldSetString (savedb_w, newfn);

	    XtFree (newfn);
	    XtFree (nopun);
	}

	XtFree (fn);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: webtlemenu.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
