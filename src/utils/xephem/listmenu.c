/* code to manage the stuff on the "listing" menu.
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#if defined(__STDC__)
#include <stdlib.h>
#endif

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/DrawingA.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>
#include <Xm/TextF.h>

#include "P_.h"

extern Widget toplevel_w;
extern Colormap xe_cm;

extern FILE *fopenh P_((char *name, char *how));
extern char *getPrivateDir P_((void));
extern char *syserrstr P_((void));
extern int confirm P_((void));
extern int existsh P_((char *filename));
extern int isUp P_((Widget shell));
extern int listing_ison P_((void));
extern void all_selection_mode P_((int whether));
extern void defaultTextFN P_((Widget w, int setcols, char *x, char *y));
extern void f_string P_((Widget w, char *s));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void query P_((Widget tw, char *msg, char *label1, char *label2,
    char *label3, void (*func1)(void), void (*func2)(void),
    void (*func3)(void)));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *msg, int app_modal));

static void lst_select P_((int whether));
static void lst_create_shell P_((void));
static void lst_activate_cb P_((Widget w, XtPointer client, XtPointer call));
static void lst_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void lst_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void lst_reset P_((void));
static void lst_stop_selecting P_((void));
static void lst_turn_off P_((void));
static void lst_try_append P_((void));
static void lst_try_overwrite P_((void));
static void lst_try_cancel P_((void));
static void lst_try_turn_on P_((void));
static void lst_turn_on P_((char *how));
static void lst_hdr P_((void));

#define	COMMENT	'*'		/* comment character */

/* max number of fields we can keep track of at once to list */
#define MAXLSTFLDS	10
#define MAXLSTSTR	32	/* longest string we can list */
#define MAXFLDNAM	32	/* longest allowed field name */

static Widget lstshell_w;
static Widget select_w, active_w, prompt_w, colhdr_w, latex_w;
static Widget title_w, filename_w;
static Widget table_w[MAXLSTFLDS];	/* row indeces follow.. */

static FILE *lst_fp;            /* the listing file; == 0 means don't plot */
static int lst_new;		/* 1 when open until first set are printed */
static int lst_latex;		/* 1 when want latex format */


/* lst_activate_cb client values. */
typedef enum {
    SELECT, ACTIVE, COLHDR, LATEX
} Options;

/* store the name and string value of each field to track.
 * we get the label straight from the Text widget in the table as needed.
 */
typedef struct {
    char l_name[MAXFLDNAM];	/* name of field we are listing */
    char l_str[MAXLSTSTR];	/* last know string value of field */
    int l_w;			/* column width -- 0 until known */
} LstFld;
static LstFld lstflds[MAXLSTFLDS];
static int nlstflds;		/* number of lstflds[] in actual use */

static char listcategory[] = "Tools -- List"; /* Save category */

/* called when the list menu is activated via the main menu pulldown.
 * if never called before, create and manage all the widgets as a child of a
 * form. otherwise, go for it.
 */
void
lst_manage ()
{
	if (!lstshell_w)
	    lst_create_shell();
	
	XtPopup (lstshell_w, XtGrabNone);
	set_something (lstshell_w, XmNiconic, (XtArgVal)False);
}

/* called by the other menus (data, etc) as their buttons are
 * selected to inform us that that button is to be included in a listing.
 */
void
lst_selection (name)
char *name;
{
	Widget tw;


	if (!isUp(lstshell_w) || !XmToggleButtonGetState(select_w))
		    return;

	tw = table_w[nlstflds];
	set_xmstring (tw, XmNlabelString, name);
	XtManageChild (tw);

	(void) strncpy (lstflds[nlstflds].l_name, name, MAXFLDNAM);
	if (++nlstflds == MAXLSTFLDS)
	    lst_stop_selecting();
}

/* called as each different field is written -- just save in lstflds[]
 * if we are potentially interested.
 */
void
lst_log (name, str)
char *name;
char *str;
{
	if (listing_ison()) {
	    LstFld *lp;
	    for (lp = lstflds; lp < &lstflds[nlstflds]; lp++)
		if (strcmp (name, lp->l_name) == 0) {
		    (void) strncpy (lp->l_str, str, MAXLSTSTR-1);
		    break;
		}
	}
}

/* called when all fields have been updated and it's time to
 * write the active listing to the current listing file, if one is open.
 */
void
listing()
{
	if (lst_fp) {
	    /* list in order of original selection */
	    LstFld *lp;

	    /* print headings if this the first time */
	    if (lst_new) {
		lst_hdr();
		lst_new = 0;
	    }

	    /* now print the fields */
	    for (lp = lstflds; lp < &lstflds[nlstflds]; lp++) {
		(void) fprintf (lst_fp, "  %-*s", lp->l_w, lp->l_str);
		if (lst_latex && lp < &lstflds[nlstflds-1])
		    (void) fprintf (lst_fp, "&");
	    }
	    if (lst_latex)
	    	(void) fprintf (lst_fp, "\\\\");
	    (void) fprintf (lst_fp, "\n");
	    fflush (lst_fp);	/* to allow monitoring */
	}
}

int
listing_ison()
{
	return (lst_fp != 0);
}

/* called to put up or remove the watch cursor.  */
void
lst_cursor (c)
Cursor c;
{
	Window win;

	if (lstshell_w && (win = XtWindow(lstshell_w)) != 0) {
	    Display *dsp = XtDisplay(lstshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* inform the other menues whether we are setting up for them to tell us
 * what fields to list.
 */
static void
lst_select(whether)
int whether;
{
	all_selection_mode(whether);
}

static void
lst_create_shell()
{
	typedef struct {
	    int indent;		/* amount to indent, pixels */
	    char *iname;	/* instance name, if Saveable */
	    char *title;
	    int cb_data;
	    Widget *wp;
	    char *tip;
	} TButton;
	static TButton tbs[] = {
	    {0, NULL, "Select fields", SELECT, &select_w,
		"When on, data fields eligible for listing are selectable buttons"},
	    {0, NULL, "Create list file", ACTIVE, &active_w,
		"When on, selected fields are written to the named file at each main Update"},
	    {15, "Headings", "Include column headings", COLHDR, &colhdr_w,
		"Whether file format will include a heading over each column"},
	    {15, "LaTex", "List in LaTeX format", LATEX, &latex_w,
		"Whether to separate columns with `&' and end lines with `\\\\'"},
	};
	XmString str;
	Widget w, rc_w, f_w;
	Widget lstform_w;
	Arg args[20];
	int i, n;

	/* create form dialog */
	n = 0;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle, "xephem Listing Control"); n++;
	XtSetArg (args[n], XmNiconName, "List"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	lstshell_w = XtCreatePopupShell ("List", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (lstshell_w, XmNcolormap, (XtArgVal)xe_cm);
	sr_reg (lstshell_w, "XEphem*List.x", listcategory, 0);
	sr_reg (lstshell_w, "XEphem*List.y", listcategory, 0);

	n = 0;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_ANY); n++;
        XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	lstform_w = XmCreateForm(lstshell_w, "ListF", args, n);
	XtAddCallback (lstform_w, XmNhelpCallback, lst_help_cb, 0);
	XtManageChild (lstform_w);

	/* make a RowColumn to hold everything */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 10); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightOffset, 10); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	XtSetArg (args[n], XmNspacing, 5); n++;
	rc_w = XmCreateRowColumn (lstform_w, "ListRC", args, n);
	XtManageChild (rc_w);

	/* make the control toggle buttons */

	for (i = 0; i < XtNumber(tbs); i++) {
	    TButton *tbp = &tbs[i];

	    str = XmStringCreate(tbp->title, XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNmarginWidth, tbp->indent); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateToggleButton(rc_w, tbp->iname ? tbp->iname : "ListTB",
								    args, n);
	    XmStringFree (str);
	    XtAddCallback(w, XmNvalueChangedCallback, lst_activate_cb,
						    (XtPointer)tbp->cb_data);
	    if (tbp->wp)
		*tbp->wp = w;
	    if (tbp->tip)
		wtip (w, tbp->tip);
	    XtManageChild (w);
	    if (tbp->iname)
		sr_reg (w, NULL, listcategory, 1);
	}

	/* create filename text area and its label */

	n = 0;
	str = XmStringCreate("File name:", XmSTRING_DEFAULT_CHARSET);
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNlabelString, str); n++;
	w = XmCreateLabel (rc_w, "ListFnL", args, n);
	XmStringFree (str);
	XtManageChild (w);

	n = 0;
	filename_w = XmCreateTextField (rc_w, "Filename", args, n);
	defaultTextFN (filename_w, 1, getPrivateDir(), "xephemlist.txt");
	wtip (filename_w, "Enter name of file to write");
	XtManageChild (filename_w);
	sr_reg (filename_w, NULL, listcategory, 1);

	/* create title text area and its label */

	n = 0;
	str = XmStringCreate("Title:", XmSTRING_DEFAULT_CHARSET);
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNlabelString, str); n++;
	w = XmCreateLabel (rc_w, "ListTL", args, n);
	XtManageChild (w);
	XmStringFree (str);

	n = 0;
	XtSetArg (args[n], XmNcolumns, 40); n++;
	title_w = XmCreateTextField (rc_w, "ListTitle", args, n);
	wtip (title_w, "Enter a title to be written to the file");
	XtManageChild (title_w);

	/* create prompt line -- it will be managed as necessary */

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	prompt_w = XmCreateLabel (rc_w, "ListPrompt", args, n);

	/* make the field name table, but don't manage them now */
	for (i = 0; i < MAXLSTFLDS; i++) {
	    n = 0;
	    table_w[i] = XmCreateLabel(rc_w, "ListLabel", args, n);
	}

	/* create a separator */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (w);

	/* make a form to hold the close and help buttons evenly */

	n = 0;
	XtSetArg (args[n], XmNfractionBase, 7); n++;
	f_w = XmCreateForm (rc_w, "ListCF", args, n);
	XtManageChild(f_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 1); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 3); n++;
	    w = XmCreatePushButton (f_w, "Close", args, n);
	    wtip (w, "Close this dialog (but continue listing if active)");
	    XtAddCallback (w, XmNactivateCallback, lst_close_cb, 0);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 4); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 6); n++;
	    w = XmCreatePushButton (f_w, "Help", args, n);
	    wtip (w, "More detailed usage information");
	    XtAddCallback (w, XmNactivateCallback, lst_help_cb, 0);
	    XtManageChild (w);
}

/* callback from any of the listing menu toggle buttons being activated.
 */
/* ARGSUSED */
static void
lst_activate_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *t = (XmToggleButtonCallbackStruct *) call;
	Options op = (Options) client;

	switch (op) {
	case SELECT:
	    if (t->set) {
		/* first turn off listing, if on, while we change things */
		if (XmToggleButtonGetState(active_w))
		    XmToggleButtonSetState(active_w, False, True);
		lst_reset();	/* reset lstflds array and unmanage the table*/
		lst_select(1);	/* inform other menus to inform us of fields */
		XtManageChild (prompt_w);
		f_string (prompt_w, "Select quantity for next column...");
	    } else
		lst_stop_selecting();
	    break;

	case ACTIVE:
	    if (t->set) {
		/* first turn off selecting, if on */
		if (XmToggleButtonGetState(select_w))
		    XmToggleButtonSetState(select_w, False, True);
		lst_try_turn_on();
	    } else
		lst_turn_off();
	    break;

	case COLHDR:
	    break;	/* toggle state is sufficient */

	case LATEX:
	    lst_latex = t->set;
	    break;
	}
}

/* callback from the Close button.
 */
/* ARGSUSED */
static void
lst_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtPopdown (lstshell_w);
}

/* callback from the Help
 */
/* ARGSUSED */
static void
lst_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg[] = {
"Select fields to become each column of a listing, then run xephem. Each step",
"will yield one line in the output file. The filename may be specified in the",
"text area provided."
};

	hlp_dialog ("Listing", msg, sizeof(msg)/sizeof(msg[0]));
}

/* forget our list, and unmanage the table.
 */
static void
lst_reset()
{
	int i;

	for (i = 0; i < nlstflds; i++)
	    XtUnmanageChild (table_w[i]);

	nlstflds = 0;
}

/* stop selecting: tell everybody else to drop their buttons, make sure toggle
 * is off.
 */
static void
lst_stop_selecting()
{
	XmToggleButtonSetState (select_w, False, False);
	lst_select(0);
	XtUnmanageChild (prompt_w);
}

static void
lst_turn_off ()
{
	if (lst_fp) {
	    (void) fclose (lst_fp);
	    lst_fp = 0;
	}
}

/* called from the query routine when want to append to an existing list file.*/
static void
lst_try_append()
{
	lst_turn_on("a");
}

/* called from the query routine when want to overwrite to an existing list
 * file.
 */
static void
lst_try_overwrite()
{
	lst_turn_on("w");
}

/* called from the query routine when decided not to make a listing file.  */
static void
lst_try_cancel()
{
	XmToggleButtonSetState (active_w, False, False);
}

/* attempt to open file for use as a listing file.
 * if it doesn't exist, then go ahead and make it.
 * but if it does, first ask wheher to append or overwrite.
 */
static void
lst_try_turn_on()
{
	char *txt = XmTextFieldGetString (filename_w);

	if (existsh (txt) == 0 && confirm()) {
	    char *buf;
	    buf = XtMalloc (strlen(txt)+100);
	    (void) sprintf (buf, "%s exists: Append or Overwrite?", txt);
	    query (toplevel_w, buf, "Append", "Overwrite", "Cancel",
			    lst_try_append, lst_try_overwrite, lst_try_cancel);
	    XtFree (buf);
	} else
	    lst_try_overwrite();

	XtFree (txt);
}

/* turn on listing facility.
 * establish a file to use (and thereby set lst_fp, the "listing-is-on" flag).
 */
static void
lst_turn_on (how)
char *how;	/* fopen how argument */
{
	char *txt;

	/* listing is on if file opens ok */
	txt = XmTextFieldGetString (filename_w);
	lst_fp = fopenh (txt, how);
	if (!lst_fp) {
	    char *buf;
	    XmToggleButtonSetState (active_w, False, False);
	    buf = XtMalloc (strlen(txt)+100);
	    (void) sprintf (buf, "%s: %s", txt, syserrstr());
	    xe_msg (buf, 1);
	    XtFree (buf);
	}
	XtFree (txt);

	lst_new = 1;	/* trigger fresh column headings */
	/* TODO: not when appending? */
}

/* print the title.
 * then set each l_w. if column headings are enabled, use and also print them.
 *   else just use l_str.
 */
static void
lst_hdr ()
{
	LstFld *lp;
	int col;
	char *txt;

	/* add a title if desired */
	txt = XmTextFieldGetString (title_w);
	if (txt[0] != '\0')
	    (void) fprintf (lst_fp, "%c %s\n", COMMENT, txt);
	XtFree (txt);

	col = XmToggleButtonGetState (colhdr_w);

	/* set lp->l_w to max of str, prefix and suffix lengths */
	for (lp = lstflds; lp < &lstflds[nlstflds]; lp++) {
	    int l = strlen (lp->l_str);
	    if (col) {
		int nl = strlen(lp->l_name);
		char *dp;

		for (dp = lp->l_name; *dp && *dp != '.'; dp++)
		    continue;
		if (*dp) {
		    int pl = dp - lp->l_name;	/* prefix */
		    int sl = nl - pl - 1; 	/* suffix */
		    if (pl > l) l = pl;
		    if (sl > l) l = sl;
		} else {
		    if (nl > l) l = nl;
		}
	    }
	    lp->l_w = l;
	}

	if (col) {
	    int printed_anything;

	    /* print first row of column headings */
	    for (lp = lstflds; lp < &lstflds[nlstflds]; lp++) {
		char cmt = lp == lstflds && !lst_latex ? COMMENT : ' ';
		char *dp;

		for (dp = lp->l_name; *dp && *dp != '.'; dp++)
		    continue;
		if (*dp)
		    fprintf (lst_fp, "%c %-*.*s", cmt, lp->l_w,
						    dp-lp->l_name, lp->l_name);
		else
		    fprintf (lst_fp, "%c %-*s", cmt, lp->l_w, lp->l_name);
		if (lst_latex && lp < &lstflds[nlstflds-1])
		    fprintf (lst_fp, "&");
	    }
	    if (lst_latex)
	    	fprintf (lst_fp, "\\\\");
	    fprintf (lst_fp, "\n");

	    /* print second row of column headings */
	    printed_anything = 0;
	    for (lp = lstflds; lp < &lstflds[nlstflds]; lp++) {
		char cmt = lp == lstflds && !lst_latex ? COMMENT : ' ';
		char *dp;

		for (dp = lp->l_name; *dp && *dp != '.'; dp++)
		    continue;
		if (*dp) {
		    fprintf (lst_fp, "%c %-*s", cmt, lp->l_w, dp+1);
		    printed_anything = 1;
		} else
		    fprintf (lst_fp, "%c %-*s", cmt, lp->l_w, "");
		if (lst_latex && printed_anything && lp < &lstflds[nlstflds-1])
		    fprintf (lst_fp, "&");
	    }
	    if (lst_latex && printed_anything)
	    	fprintf (lst_fp, "\\\\");
	    fprintf (lst_fp, "\n");
	}

	fflush (lst_fp); /* to allow monitoring */
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: listmenu.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
