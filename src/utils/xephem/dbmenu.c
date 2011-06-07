/* code to manage the stuff on the "database" menu.
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#if defined(__STDC__)
#include <stdlib.h>
#endif

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/PanedW.h>
#include <Xm/CascadeB.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/FileSB.h>
#include <Xm/ScrolledW.h>
#include <Xm/Text.h>
#include <Xm/Separator.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "db.h"

extern Widget	toplevel_w;
#define	XtD	XtDisplay(toplevel_w)
extern Colormap xe_cm;

extern FILE *fopenh P_((char *name, char *how));
extern Obj *db_scan P_((DBScan *sp));
extern char *expand_home P_((char *path));
extern char *getShareDir P_((void));
extern char *getPrivateDir P_((void));
extern char *syserrstr P_((void));
extern int confirm P_((void));
extern int db_n P_((void));
extern int isUp P_((Widget w));
extern void db_read P_((char *fn, int nodups));
extern void all_newdb P_((int appended));
extern void db_clr_cp P_((void));
extern void db_connect_fifo P_((void));
extern void db_scaninit P_((DBScan *sp, int mask, ObjF *op, int nop));
extern void get_something P_((Widget w, char *resource, XtArgVal value));
extern void get_xmstring P_((Widget w, char *resource, char **txtp));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void prompt_map_cb P_((Widget w, XtPointer client, XtPointer call));
extern void query P_((Widget tw, char *msg, char *label1, char *label2,
    char *label3, void (*func1)(void), void (*func2)(void),
    void (*func3)(void)));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *msg, int app_modal));

static void db_create_shell P_((void));
static void initFSB P_((Widget fsb_w));
static void db_set_report P_((void));
static void db_load_cb P_((Widget w, XtPointer client, XtPointer data));
static void db_openfifo_cb P_((Widget w, XtPointer client, XtPointer data));
static void db_delall_cb P_((Widget w, XtPointer client, XtPointer data));
static void db_help_cb P_((Widget w, XtPointer client, XtPointer data));
static void db_helpon_cb P_((Widget w, XtPointer client, XtPointer data));
static void db_sharedir_cb P_((Widget w, XtPointer client, XtPointer data));
static void db_privdir_cb P_((Widget w, XtPointer client, XtPointer data));
static void db_close_cb P_((Widget w, XtPointer client, XtPointer data));
static void db_1catrow P_((Widget rc_w, DBCat *dbcp));

static Widget dbshell_w;	/* main shell */
static Widget catsw_w;		/* scrolled window managing the cat list */
static Widget report_w;		/* label with the dbstats */
static Widget nodups_w;		/* TB whether to avoid dups on file reads */
static Widget load1_w;		/* TB whether to load ObjX from .edb with 1 */
static Widget ncats_w;		/* number of catalogs label */
static Widget nobjs_w;		/* number of objects label */

/* bring up the db menu, creating if first time */
void
db_manage()
{
	if (!dbshell_w)
	    db_create_shell();

	db_set_report();	/* get a fresh count */
	XtPopup (dbshell_w, XtGrabNone);
	set_something (dbshell_w, XmNiconic, (XtArgVal)False);
}

/* called when the database beyond NOBJ has changed in any way.
 * as odd as this seems since *this* menu controls the db contents, this was
 *   added when the db fifo input path was added. it continued to be handy
 *   when initial db files could be loaded automatically and we introduced
 *   fields stars in 2.9.
 * all we do is update our tally, if we are up at all.
 */
/* ARGSUSED */
void
db_newdb (appended)
int appended;
{
	if (isUp(dbshell_w))
	    db_set_report();
}

/* called when any of the user-defined objects have changed.
 * all we do is update our tally, if we are up at all.
 */
/* ARGSUSED */
void
db_newobj (id)
int id;	/* OBJXYZ */
{
	if (isUp(dbshell_w))
	    db_set_report();
}

/* update the list of catalogs.
 */
void
db_newcatmenu (dbcp, ndbcp)
DBCat dbcp[];
int ndbcp;
{
	char buf[128];
	Arg args[20];
	Widget ww;
	int n;

#if 0
	for (n = 0; n < ndbcp; n++) {
	    int t;
	    printf ("%-6.6s:", dbcp[n].name);
	    for (t = 0; t < NOBJTYPES; t++)
		printf (" %6d", dbcp[n].start[t]);
	    printf (",");
	    for (t = 0; t < NOBJTYPES; t++)
		printf (" %4d", dbcp[n].n[t]);
	    printf ("\n");
	}
	printf ("\n");
#endif

	/* create if not already */
	if (!dbshell_w)
	    db_create_shell();

	/* set count label */
	sprintf (buf, "%d Loaded Catalogs", ndbcp);
	set_xmstring (ncats_w, XmNlabelString, buf);

	/* replace workWindow */
	get_something (catsw_w, XmNworkWindow, (XtArgVal)&ww);
	XtDestroyWidget (ww);
	n = 0;
	XtSetArg (args[n], XmNspacing, 0); n++;
	ww = XmCreateRowColumn (catsw_w, "CatRC", args, n);
	wtip (ww, "List of all catalogs now in memory");
	set_something (catsw_w, XmNworkWindow, (XtArgVal)ww);

	/* fill with each cat info */
	while (ndbcp--)
	    db_1catrow (ww, dbcp++);

	/* ok */
	XtManageChild (ww);
}

/* called to put up or remove the watch cursor.  */
void
db_cursor (c)
Cursor c;
{
	Window win;

	if (dbshell_w && (win = XtWindow(dbshell_w)) != 0) {
	    Display *dsp = XtDisplay(dbshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* return 1 if want to autoload ObjXYZ if read a .edb with 1 entry, else 0 */
int
db_load1()
{
	/* create if not already */
	if (!dbshell_w)
	    db_create_shell();

	return (XmToggleButtonGetState(load1_w));
}

/* create a shell to allow user to manage files . */
static void
db_create_shell ()
{
	typedef struct {
	    char *label;	/* what goes on the help label */
	    char *key;		/* string to call hlp_dialog() */
	    char *tip;		/* button tip text */
	} HelpOn;
	static HelpOn helpon[] = {
	    {"Intro...", "Database - intro",
		"How to load and delete .edb catalogs files"},
	    {"on Control...", "Database - control",
		"Information about the Control pulldown menu"},
	    {"File format...", "Database - files",
		"Definition of the XEphem .edb file format"},
	    {"Notes...", "Database - notes",
		"Additional info about the XEphem data base"},
	};
	Widget mb_w, pd_w, cb_w;
	Widget pw_w, rc_w, fsb_w;
	Widget dbform_w;
	XmString str;
	Widget w;
	Arg args[20];
	int n;
	int i;
	
	/* create outter shell and form */

	n = 0;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle, "xephem Database Load/Delete"); n++;
	XtSetArg (args[n], XmNiconName, "DB"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	dbshell_w = XtCreatePopupShell ("DB", topLevelShellWidgetClass,
						    toplevel_w, args, n);
	set_something (dbshell_w, XmNcolormap, (XtArgVal)xe_cm);
	sr_reg (dbshell_w, "XEphem*DB.x", dbcategory, 0);
	sr_reg (dbshell_w, "XEphem*DB.y", dbcategory, 0);
	sr_reg (dbshell_w, "XEphem*DB.height", dbcategory, 0);
	sr_reg (dbshell_w, "XEphem*DB.width", dbcategory, 0);

	n = 0;
	XtSetArg (args[n], XmNmarginWidth, 5); n++;
	dbform_w = XmCreateForm (dbshell_w, "DBManage", args, n);
	XtAddCallback (dbform_w, XmNhelpCallback, db_help_cb, 0);
	XtManageChild(dbform_w);

	/* create the menu bar across the top */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	mb_w = XmCreateMenuBar (dbform_w, "MB", args, n);
	XtManageChild (mb_w);

	/* make the Control pulldown */

	n = 0;
	pd_w = XmCreatePulldownMenu (mb_w, "ControlPD", args, n);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pd_w);  n++;
	    XtSetArg (args[n], XmNmnemonic, 'C'); n++;
	    cb_w = XmCreateCascadeButton (mb_w, "ControlCB", args, n);
	    set_xmstring (cb_w, XmNlabelString, "Control");
	    XtManageChild (cb_w);

	    /* delete all */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "DelAll", args, n);
	    XtAddCallback (w, XmNactivateCallback, db_delall_cb, 0);
	    set_xmstring(w, XmNlabelString, "Delete all");
	    wtip (w, "Remove all files from memory");
	    XtManageChild (w);

	    /* make the open fifo button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "OpenFIFO", args, n);
	    XtAddCallback (w, XmNactivateCallback, db_openfifo_cb, 0);
	    set_xmstring(w, XmNlabelString, "Open DB Fifo");
	    wtip (w, "Activate import path for Object definitions");
	    XtManageChild (w);

	    /* make the no-dups toggle button */

	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True);  n++;
	    nodups_w = XmCreateToggleButton (pd_w, "NoDups", args, n);
	    set_xmstring(nodups_w, XmNlabelString, "No Dups");
	    wtip (nodups_w,
		"When on, Loading skips objects from other catalogs whose name already exists in memory");
	    XtManageChild (nodups_w);
	    sr_reg (nodups_w, NULL, dbcategory, 1);

	    /* make the load-1 toggle button */

	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True);  n++;
	    load1_w = XmCreateToggleButton (pd_w, "Load1", args, n);
	    set_xmstring(load1_w, XmNlabelString, "Load Obj when 1");
	    wtip (load1_w,
		"When on, reading a file containing 1 entry also loads ObjX, Y or Z");
	    XtManageChild (load1_w);
	    sr_reg (load1_w, NULL, dbcategory, 1);

	    /* add a separator */

	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    /* add the close button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, db_close_cb, 0);
	    wtip (w, "Close this window");
	    XtManageChild (w);

	/* make the help pulldown */

	n = 0;
	pd_w = XmCreatePulldownMenu (mb_w, "HelpPD", args, n);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pd_w);  n++;
	    XtSetArg (args[n], XmNmnemonic, 'H'); n++;
	    cb_w = XmCreateCascadeButton (mb_w, "HelpCB", args, n);
	    set_xmstring (cb_w, XmNlabelString, "Help");
	    XtManageChild (cb_w);
	    set_something (mb_w, XmNmenuHelpWidget, (XtArgVal)cb_w);

	    for (i = 0; i < XtNumber(helpon); i++) {
		HelpOn *hp = &helpon[i];

		str = XmStringCreate (hp->label, XmSTRING_DEFAULT_CHARSET);
		n = 0;
		XtSetArg (args[n], XmNlabelString, str); n++;
		XtSetArg (args[n], XmNmarginHeight, 0); n++;
		w = XmCreatePushButton (pd_w, "Help", args, n);
		XtAddCallback (w, XmNactivateCallback, db_helpon_cb,
							(XtPointer)(hp->key));
		if (hp->tip)
		    wtip (w, hp->tip);
		XtManageChild (w);
		XmStringFree(str);
	    }

	/* make the report output-only text under a label */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, mb_w); n++;
	XtSetArg (args[n], XmNtopOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	rc_w = XmCreateRowColumn (dbform_w, "RCO", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    nobjs_w = XmCreateLabel (rc_w, "NObjs", args, n);
	    XtManageChild (nobjs_w);

	    n = 0;
	    XtSetArg (args[n], XmNeditable, False); n++;
	    XtSetArg (args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	    XtSetArg (args[n], XmNcursorPositionVisible, False); n++;
	    XtSetArg (args[n], XmNblinkRate, 0); n++;
	    XtSetArg (args[n], XmNrows, 14); n++;
	    XtSetArg (args[n], XmNcolumns, 32); n++;
	    report_w = XmCreateText (rc_w, "Report", args, n);
	    wtip (report_w,
			"Breakdown of number and types of objects in memory");
	    XtManageChild (report_w);

	/* label for Catalogs */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNtopOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	ncats_w = XmCreateLabel (dbform_w, "NCats", args, n);
	set_xmstring (ncats_w, XmNlabelString, "0 Loaded Catalogs");
	XtManageChild (ncats_w);

	/* put the Catalogs and FSB in a paned window */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, ncats_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNmarginWidth, 1); n++;
	XtSetArg (args[n], XmNsashHeight, 10); n++;
	XtSetArg (args[n], XmNspacing, 14); n++;
	pw_w = XmCreatePanedWindow (dbform_w, "Pane", args, n);
	XtManageChild (pw_w);

	/* create a RC in a SW for showing and deleting loaded catalogs.
	 * the RC is replaced each time the list changes.
	 */

	n = 0;
	XtSetArg (args[n], XmNscrollingPolicy, XmAUTOMATIC); n++;
	catsw_w = XmCreateScrolledWindow (pw_w, "CatSW", args, n);
	XtManageChild (catsw_w);

	    n = 0;
	    w = XmCreateRowColumn (catsw_w, "CatRCDummy", args, n);
	    XtManageChild (w);

	/* make the file selection box */

	n = 0;
	XtSetArg (args[n], XmNskipAdjust, True); n++; /* PW constraint */
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	fsb_w = XmCreateFileSelectionBox (pw_w, "FSB", args, n);
	initFSB (fsb_w);	/* do this before managing for Ultrix :-) */
	XtManageChild (fsb_w);

#if XmVersion >= 1001
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_OK_BUTTON);
	XmProcessTraversal (w, XmTRAVERSE_CURRENT);
	XmProcessTraversal (w, XmTRAVERSE_CURRENT); /* yes, twice!! */
#endif
}

/* init the directory and pattern resources of the given FileSelectionBox.
 * we try to pull these from the basic program resources.
 * also redefine the button for our purposes.
 */
static void
initFSB (fsb_w)
Widget fsb_w;
{
	Arg args[20];
	char buf[1024];
	Widget w;
	int n;

	/* helpful tips -- Buttons are really gadgets */
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_FILTER_TEXT);
	wtip (w,
	    "Any file name pattern; or use buttons below for handy presets.");
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_DIR_LIST);
	wtip (w, "Browse directories, double-click to move");
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_LIST);
	wtip (w, "Files in chosen directory that match pattern; double-click to load");
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_TEXT);
	wtip (w, "File name; enter manually or by browsing with above lists");

	/* set dir and pattern */
	(void) sprintf (buf, "%s/catalogs",  getShareDir());
	set_xmstring (fsb_w, XmNdirectory, expand_home(buf));
	set_xmstring (fsb_w, XmNpattern, "*.edb");

	/* use the Ok button to mean Load */
	XtAddCallback (fsb_w, XmNokCallback, db_load_cb, NULL);
	set_xmstring (fsb_w, XmNokLabelString, "Load");

	/* use Cancel for handy Shared dir */
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_CANCEL_BUTTON);
	XtAddCallback(w, XmNactivateCallback, db_sharedir_cb, (XtPointer)fsb_w);
	set_xmstring (w, XmNlabelString, "Shared\nDir");
	n = 0;
	XtSetArg (args[n], XmNmarginWidth, 2); n++;
	XtSetArg (args[n], XmNmarginLeft, 2); n++;
	XtSetArg (args[n], XmNmarginRight, 2); n++;
	XtSetValues (w, args, n);

	/* use Help for handy Private dir */
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_HELP_BUTTON);
	XtRemoveAllCallbacks (w, XmNactivateCallback);
	XtAddCallback(w, XmNactivateCallback, db_privdir_cb, (XtPointer)fsb_w);
	set_xmstring (w, XmNlabelString, "Private\nDir");
	n = 0;
	XtSetArg (args[n], XmNmarginWidth, 3); n++;
	XtSetArg (args[n], XmNmarginLeft, 3); n++;
	XtSetArg (args[n], XmNmarginRight, 3); n++;
	XtSetValues (w, args, n);
}

/* callback from the Shared dir PB.
 * client is the FSB
 */
/* ARGSUSED */
static void
db_sharedir_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Widget fsb_w = (Widget)client;
	char buf[1024];

	(void) sprintf (buf, "%s/catalogs", getShareDir());
	set_xmstring (fsb_w, XmNdirectory, expand_home(buf));
}

/* callback from the Private dir PB.
 * client is the FSB
 */
/* ARGSUSED */
static void
db_privdir_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Widget fsb_w = (Widget)client;

	set_xmstring (fsb_w, XmNdirectory, getPrivateDir());
}

/* compile the stats report into report_w.
 * N.B. we do not include the planets nor the user objects.
 */
static void
db_set_report()
{
	DBScan dbs;
	char report[1024];
	int mask = ALLM & ~PLANETM;
	Obj *op;
	int nes=0, ne=0, np=0, nh=0;
	int nc=0, ng=0, nj=0, nn=0, npn=0, nl=0, nq=0, nr=0, ns=0, no=0;
	int t=0;

	for (db_scaninit(&dbs, mask, NULL, 0); (op = db_scan(&dbs))!=NULL; ){
	    switch (op->o_type) {
	    case FIXED:
		switch (op->f_class) {
		case 'C': case 'U': case 'O': nc++; t++; break;
		case 'G': case 'H': case 'A': ng++; t++; break;
		case 'N': case 'F': case 'K': nn++; t++; break;
		case 'J': nj++; t++; break;
		case 'P': npn++; t++; break;
		case 'L': nl++; t++; break;
		case 'Q': nq++; t++; break;
		case 'R': nr++; t++; break;
		case 'T': case 'B': case 'D': case 'M': case 'S': case 'V': 
		    ns++; t++; break;
		default: no++; t++; break;
		}
		break;
	    case ELLIPTICAL: ne++; t++; break;
	    case HYPERBOLIC: nh++; t++; break;
	    case PARABOLIC: np++; t++; break;
	    case EARTHSAT: nes++; t++; break;
	    case UNDEFOBJ: break;
	    default:
		printf ("Unknown object type: %d\n", op->o_type);
		exit (1);
	    }
	}

	(void) sprintf (report, "\
%6d Solar -- elliptical\n\
%6d Solar -- hyperbolic\n\
%6d Solar -- parabolic\n\
%6d Earth satellites\n\
%6d Clusters (C,U,O)\n\
%6d Galaxies (G,H,A)\n\
%6d Planetary Nebulae (P)\n\
%6d Nebulae (N,F,K)\n\
%6d Pulsars (L)\n\
%6d Quasars (Q)\n\
%6d Radio sources (J)\n\
%6d Supernova Remnants (R)\n\
%6d Stars (S,V,D,B,M,T)\n\
%6d Undefined\
",
	ne, nh, np, nes, nc, ng, npn, nn, nl, nq, nj, nr, ns, no);

	set_something (report_w, XmNvalue, (XtArgVal)report);

	/* set count in lable */
	sprintf (report, "%d Loaded Objects", t);
	set_xmstring (nobjs_w, XmNlabelString, report);
}

/* callback from the Ok button along the bottom of the FSB.
 * we take this to mean load the current selection.
 */
/* ARGSUSED */
static void
db_load_cb (w, client, data)
Widget w;
XtPointer client;
XtPointer data;
{
	XmFileSelectionBoxCallbackStruct *s =
				    (XmFileSelectionBoxCallbackStruct *)data;
	DBCat *dbcp;
	char *fn;

	if (s->reason != XmCR_OK) {
	    printf ("%s: Unknown reason = 0x%x\n", "db_load_cb()", s->reason);
	    exit (1);
	}

	watch_cursor(1);
	XmStringGetLtoR (s->value, XmSTRING_DEFAULT_CHARSET, &fn);
	dbcp = db_catfind (fn);
	if (dbcp)
	    db_catdel (dbcp);
	db_read (fn, XmToggleButtonGetState (nodups_w));
	all_newdb(1);
	XtFree (fn);
	watch_cursor(0);
}

static void
dbdelall()
{
	db_del_all();
	all_newdb(0);
}

/* callback when user wants to delete all files
 */
/* ARGSUSED */
static void
db_delall_cb (w, client, data)
Widget w;
XtPointer client;
XtPointer data;
{
	if (confirm())
	    query (toplevel_w, "Delete all files from memory?",
		   "Yes .. delete all", "No .. no change", NULL,
		   dbdelall, NULL, NULL);
	else 
	    dbdelall();
}

/* callback from the open fifo button */
/* ARGSUSED */
static void
db_openfifo_cb (w, client, data)
Widget w;
XtPointer client;
XtPointer data;
{
	db_connect_fifo();
}

/* ARGSUSED */
static void
db_help_cb (w, client, data)
Widget w;
XtPointer client;
XtPointer data;
{
	static char *msg[] = {
"This displays a count of the various types of objects currently in memory.",
"Database files may be read in to add to this list or the list may be deleted."
};

	hlp_dialog ("DataBase", msg, XtNumber(msg));
}

/* callback from a specific Help button.
 * client is a string to use with hlp_dialog().
 */
/* ARGSUSED */
static void
db_helpon_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	hlp_dialog ((char *)client, NULL, 0);
}

/* ARGSUSED */
static void
db_close_cb (w, client, data)
Widget w;
XtPointer client;
XtPointer data;
{
	XtPopdown (dbshell_w);
}

/* call from any of the Delete catalog PBs.
 * client is a DBCat *.
 */
/* ARGSUSED */
static void
catdel_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	watch_cursor(1);
	db_catdel ((DBCat *)client);
	all_newdb(0);
	watch_cursor(0);
}

/* build one new catalog entry */
static void
db_1catrow (rc_w, dbcp)
Widget rc_w;
DBCat *dbcp;
{
	Widget f_w, pb_w;
	Widget nl_w, cl_w;
	char buf[32];
	Arg args[20];
	int n;
	int i, nobjs;

	n = 0;
	f_w = XmCreateForm (rc_w, "CatForm", args, n);
	XtManageChild (f_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNshadowThickness, 1); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	pb_w = XmCreatePushButton (f_w, "Delete", args, n);
	wtip (pb_w, "Delete all objects from this catalog");
	XtAddCallback (pb_w, XmNactivateCallback, catdel_cb, (XtPointer)dbcp);
	XtManageChild (pb_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, pb_w); n++;
	XtSetArg (args[n], XmNleftOffset, 4); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 45); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	nl_w = XmCreateLabel (f_w, "NObjs", args, n);
	XtManageChild (nl_w);

	for (nobjs = i = 0; i < NOBJTYPES; i++)
	    nobjs += dbcp->n[i];
	(void) sprintf (buf, "%6d", nobjs);
	set_xmstring (nl_w, XmNlabelString, buf);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 47); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	cl_w = XmCreateLabel (f_w, "Name", args, n);
	XtManageChild (cl_w);
	set_xmstring (cl_w, XmNlabelString, dbcp->name);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: dbmenu.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
