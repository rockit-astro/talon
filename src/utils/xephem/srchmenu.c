/* this file contains functions to support iterative xephem searches.
 * we support several kinds of searching and solving algorithms.
 * the expressions being evaluated are compiled and executed from compiler.c.
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
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/SelectioB.h>
#include <Xm/ToggleB.h>
#include <Xm/Text.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"

extern Widget	toplevel_w;
#define	XtD	XtDisplay(toplevel_w)
extern Colormap xe_cm;

extern Now *mm_get_now P_((void));
extern int any_ison P_((void));
extern int compile_expr P_((char *ex, char *errbuf));
extern int execute_expr P_((double *vp, char *errbuf));
extern int isUp P_((Widget shell));
extern int prog_isgood P_((void));
extern void all_selection_mode P_((int whether));
extern void buttonAsButton P_((Widget w, int whether));
extern void compiler_log P_((char *name, double value));
extern void f_double P_((Widget w, char *fmt, double f));
extern void f_scansex P_((double o, char bp[], double *np));
extern void f_showit P_((Widget w, char *str));
extern void f_time P_((Widget w, double t));
extern void get_xmstring P_((Widget w, char *resource, char **txtp));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void lst_selection P_((char *name));
extern void plt_selection P_((char *name));
extern void prompt_map_cb P_((Widget w, XtPointer client, XtPointer call));
extern void redraw_screen P_((int how_much));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void wtip P_((Widget w, char *string));
extern void xe_msg P_((char *msg, int app_modal));

static void srch_create_shell P_((void));
static int srch_isup P_((void));
static int srching_now P_((void));
static void srch_set_buttons P_((int whether));
static void srch_use_cb P_((Widget w, XtPointer client, XtPointer call));
static void srch_newfunc_cb P_((Widget w, XtPointer client, XtPointer call));
static void srch_compile_cb P_((Widget w, XtPointer client, XtPointer call));
static void srch_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void srch_clear_cb P_((Widget w, XtPointer client, XtPointer call));
static void srch_fields_cb P_((Widget w, XtPointer client, XtPointer call));
static void srch_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void srch_acc_ok_cb P_((Widget w, XtPointer client, XtPointer call));
static void srch_acc_cb P_((Widget wid, XtPointer client, XtPointer call));
static void srch_goal_cb P_((Widget w, XtPointer client, XtPointer call));
static void srch_on_off_cb P_((Widget w, XtPointer client, XtPointer call));
static int srch_minmax P_((double Mjd, double v, double *tmincp));
static int srch_solve0 P_((double Mjd, double v, double *tmincp));
static int srch_binary P_((double Mjd, double v, double *tmincp));


/* the widgets we need direct access to */
static Widget srchshell_w;
static Widget help_w;
static Widget acc_w;
static Widget field_w;
static Widget func_w;	/* contains the search function [to be] compiled */
static Widget err_w;
static Widget compile_w;
static Widget valu_w;	/* used to display most recent calulated value */
static Widget on_w;	/* whether searching is active comes directly from this
			 * widget's XmToggleButtonGetState()
			 */

/* name is it appears when valu_w is selected for plotting or lising */
static char srchvname[] = "SolveValue";
static char solvecategory[] = "Tools -- Solve";	/* Save category */

static int (*srch_f)();		/* 0 or pointer to one of the search functions*/
static int srch_tmscalled;	/* number of iterations so far */
static double tmlimit;		/* search accuracy, hrs */

static int srch_selecting;	/* whether our value is currently selectable */
static int srch_self_selection_mode;	/* flag to prevent self-selection */

/* called when the search menu is activated via the main menu pulldown.
 * if never called before, create and manage all the widgets as a child of a
 * form. otherwise, just go for it.
 */
void
srch_manage ()
{
	if (!srchshell_w)
	    srch_create_shell();
	
	XtPopup (srchshell_w, XtGrabNone);
	set_something (srchshell_w, XmNiconic, (XtArgVal)False);
	srch_set_buttons(srch_selecting);
}

/* called by other menus as they want to hear from our buttons or not.
 * the "on"s and "off"s stack - only really redo the buttons if it's the
 * first on or the last off.
 * N.B. we cooperate with a flag from the Enable pushbutton to prevent
 *   being able use the search function result as a term in the search funtion.
 */
void
srch_selection_mode (whether)
int whether;	/* whether setting up for plotting or for not plotting */
{
	if (srch_self_selection_mode)
	    return;

	if (whether)
	    srch_selecting++;
	else if (srch_selecting > 0)
	    --srch_selecting;

	if (isUp(srchshell_w))
	    if ((whether && srch_selecting == 1)     /* first one to want on */
		|| (!whether && srch_selecting == 0) /* last one to want off */)
		srch_set_buttons (whether);
}

/* called when other modules, such as data menu, have a button pushed
 * and we have asked (by enabling field buttons) that they inform us that
 * that button is to be included in the search function.
 * other modules that use buttons, such as plotting and listing, might ask
 * too so it might not really be us that wants to use it.
 */
void
srch_selection (name)
char *name;
{
	int ins;
	char *qname;

	if (!isUp(srchshell_w) || !XmToggleButtonGetState(field_w))
	    return;

	ins = XmTextGetInsertionPosition (func_w);
	qname = XtMalloc (strlen(name) + 3);	/* two '"' and \0 */
	(void) sprintf (qname, "\"%s\"", name);
	XmTextReplace (func_w, /* from */ ins, /* to */ ins, qname);
	XtFree (qname);

	/* move the focus right back to the search menu so the insertion point
	 * remains visible.
	 */
	XSetInputFocus (XtDisplay(srchshell_w), XtWindow(srchshell_w),
						RevertToParent, CurrentTime);
}

/* if searching is in effect call the search type function.
 * it might modify *tmincp according to where it next wants to eval.
 * (remember tminc is in hours, not days).
 * if searching ends for any reason it is also turned off.
 * if we are not searching but are plotting or listing we still execute the
 *   search function (if it is ok) and flog and display it.
 * return 0 if caller can continue or -1 if it is time to stop.
 */
int
srch_eval(Mjd, tmincp)
double Mjd;
double *tmincp;
{
	int s;

	if (prog_isgood() && any_ison()) {
	    char errbuf[128];
	    double v;
	    s = execute_expr (&v, errbuf);
	    if (s == 0) {
		f_double (valu_w, "%g", v);
		if (srching_now()) {
		    s = (*srch_f)(Mjd, v, tmincp);
		    srch_tmscalled++;
		}
	    } else {
		char srchmsg[256];
		(void) sprintf (srchmsg, "Search evaluation error: %.200s",
								    errbuf);
		xe_msg (srchmsg, 0);
	    }
	} else
	    s = 0;

	if (s < 0)
	    XmToggleButtonSetState(on_w, False, /*invoke cb*/True);

	return (s);
}

/* called by other systems to decide whether it is worth computing and
 *   logging values to the search system.
 * we say True whenever there is a validly compiled function and either we are
 *   searching (obviously) or the srch control menu is up.
 */
int
srch_ison()
{
	return (prog_isgood() && (srching_now() || srch_isup()));
}

/* called as each different field is written -- just tell the compiler
 * if we are interested in it.
 * we have to check if *anything* is on because we might be plotting/listing
 *   the srch function itself.
 */
void
srch_log (name, value)
char *name;
double value;
{
	if (any_ison())
	    compiler_log (name, value);
}

/* called to put up or remove the watch cursor.  */
void
srch_cursor (c)
Cursor c;
{
	Window win;

	if (srchshell_w && (win = XtWindow(srchshell_w)) != 0) {
	    Display *dsp = XtDisplay(srchshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

static void
srch_create_shell()
{
	static struct {
	    char *title;
	    XtPointer cb_data;
	    char *tip;
	} rbtb[] = {
	    {"Find Extreme", (XtPointer)srch_minmax,
	    	"Solve for a time at which Function experiences a local minima or maxima"},
	    {"Find 0", (XtPointer)srch_solve0,
	    	"Solve for a time at which the Function evaluates to 0"},
	    {"Binary", (XtPointer)srch_binary,
	    	"Step time along until the truth value of the Function changes"},
	};
	XmString str;
	char *txt;
	Widget w, trc_w, brc_w, f_w, rb_w;
	Widget srchform_w;
	Arg args[20];
	int i, n;

	/* create form dialog */
	n = 0;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNiconName, "Solve"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	XtSetArg (args[n], XmNtitle, "xephem Solver Control"); n++;
	srchshell_w = XtCreatePopupShell ("Solve", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (srchshell_w, XmNcolormap, (XtArgVal)xe_cm);
	sr_reg (srchshell_w, "XEphem*Solve.x", solvecategory, 0);
	sr_reg (srchshell_w, "XEphem*Solve.y", solvecategory, 0);

	n = 0;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_GROW); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	srchform_w = XmCreateForm (srchshell_w, "SrchF", args, n);
	XtAddCallback (srchform_w, XmNhelpCallback, srch_help_cb, 0);
	XtManageChild (srchform_w);

	/* create a RowColumn to hold stuff on top of the text */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNspacing, 5); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	trc_w = XmCreateRowColumn (srchform_w, "TSrchRC", args, n);
	XtManageChild (trc_w);

	/* searching on/off toggle button */

	n = 0;
	on_w = XmCreateToggleButton (trc_w, "SAct", args, n);
	XtAddCallback (on_w, XmNvalueChangedCallback, srch_on_off_cb, 0);
	set_xmstring (on_w, XmNlabelString, "Solver is Active");
	wtip (on_w, "When on, solver controls Main's step size and searches for goal");
	XtManageChild (on_w);

	/* create a separator */

	n = 0;
	w = XmCreateSeparator (trc_w, "Sep1", args, n);
	XtManageChild (w);

	/* compiler area title */

	n = 0;
	f_w = XmCreateForm (trc_w, "SrchAF", args, n);
	XtManageChild (f_w);

	    str = XmStringCreate ("Function:", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateLabel (f_w, "FuncL", args, n);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomOffset, 2); n++;
	    w = XmCreatePushButton (f_w, "Clear", args, n);
	    XtAddCallback (w, XmNactivateCallback, srch_clear_cb, NULL);
	    XtManageChild (w);

	/* create another RC for the bottom stuff.
	 * text will connect them together.
	 * this is so text follows user resizing.
	 */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNspacing, 5); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	brc_w = XmCreateRowColumn (srchform_w, "BSrchRC", args, n);
	XtManageChild (brc_w);

	/* compiler message label */

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	err_w = XmCreateLabel (brc_w, "SrchErrsL", args, n);
	set_xmstring (err_w, XmNlabelString, " ");
	wtip (err_w, "Result from compiling the search Function");
	XtManageChild (err_w);

	/* use-fields button */

	str = XmStringCreate("Enable field buttons", XmSTRING_DEFAULT_CHARSET);
	n = 0;
	XtSetArg (args[n], XmNlabelString, str); n++;
	field_w = XmCreateToggleButton (brc_w, "SrchFEnable", args, n);
	XtAddCallback (field_w, XmNvalueChangedCallback, srch_fields_cb, 0);
	wtip (field_w, "When on, data fields eligible for use in Function are selectable buttons");
	XtManageChild (field_w);
	XmStringFree(str);

	/* COMPILE push button */

	n = 0;
	XtSetArg (args[n], XmNshowAsDefault, True); n++;
	compile_w = XmCreatePushButton (brc_w, "Compile", args, n);
	XtAddCallback (compile_w, XmNactivateCallback, srch_compile_cb, 0);
	set_something (srchform_w, XmNdefaultButton, (XtArgVal)w);
	wtip (compile_w, "Press to compile the Function");
	XtManageChild (compile_w);

	/* create a separator */

	n = 0;
	w = XmCreateSeparator (brc_w, "Sep2", args, n);
	XtManageChild (w);

	/* create goal radio box and its toggle buttons */

	str = XmStringCreate ("Goal:", XmSTRING_DEFAULT_CHARSET);
	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNlabelString, str); n++;
	w = XmCreateLabel (brc_w, "SrchRBL", args, n);
	XtManageChild(w);
	XmStringFree(str);

	n = 0;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	rb_w = XmCreateRadioBox (brc_w, "SrchGoalRadioBox", args, n);
	XtManageChild (rb_w);

	    for (i = 0; i < XtNumber(rbtb); i++) {
		n = 0;
		str = XmStringCreate(rbtb[i].title, XmSTRING_DEFAULT_CHARSET);
		XtSetArg (args[n], XmNlabelString, str); n++;
		w = XmCreateToggleButton (rb_w, "SrchRBTB", args, n);
		XtAddCallback (w, XmNvalueChangedCallback, srch_goal_cb,
						    (XtPointer)rbtb[i].cb_data);
		if (rbtb[i].tip)
		    wtip (w, rbtb[i].tip);
		XtManageChild (w);
		XmStringFree(str);
	    }

	/* create a separator */

	n = 0;
	w = XmCreateSeparator (brc_w, "Sep3", args, n);
	XtManageChild (w);

	/* Accuracy label and its push button in a form */

	n = 0;
	f_w = XmCreateForm (brc_w, "SrchAF", args, n);
	XtManageChild (f_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    acc_w = XmCreatePushButton (f_w, "Accuracy", args, n);
	    XtAddCallback (acc_w, XmNactivateCallback, srch_acc_cb, 0);
	    wtip (acc_w, "Set desired time accuracy of solution");
	    XtManageChild (acc_w);
	    sr_reg (acc_w, NULL, solvecategory, 1);

	    get_xmstring (acc_w, XmNlabelString, &txt);
	    f_scansex (0.0, txt, &tmlimit);
	    XtFree (txt);

	    str = XmStringCreate("Desired accuracy:", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNrightWidget, acc_w); n++;
	    XtSetArg (args[n], XmNrightOffset, 5); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateLabel (f_w, "SrchAccL", args, n);
	    XtManageChild (w);
	    XmStringFree (str);

	/* current search value in a form */

	n = 0;
	f_w = XmCreateForm (brc_w, "SrchVF", args, n);
	XtManageChild (f_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNuserData, (XtArgVal)srchvname); n++;
	    valu_w = XmCreatePushButton (f_w, "CurValue", args, n);
	    set_xmstring (valu_w, XmNlabelString, "0.0");
	    buttonAsButton (valu_w, False);
	    XtAddCallback (valu_w, XmNactivateCallback, srch_use_cb, 0);
	    wtip (valu_w, "Value of Function evaluated at current time");
	    XtManageChild (valu_w);

	    str = XmStringCreate ("Current value: ", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNrightWidget, valu_w); n++;
	    XtSetArg (args[n], XmNrightOffset, 5); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateLabel (f_w, "SrchValuL", args, n);
	    XtManageChild (w);
	    XmStringFree (str);

	/* create a separator */

	n = 0;
	w = XmCreateSeparator (brc_w, "Sep4", args, n);
	XtManageChild (w);

	/* form to hold bottom control buttons */

	n = 0;
	XtSetArg (args[n], XmNfractionBase, 7); n++;
	f_w = XmCreateForm (brc_w, "SrchCF", args, n);
	XtManageChild (f_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 1); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 3); n++;
	    w = XmCreatePushButton(f_w, "Close", args,n);
	    XtAddCallback(w, XmNactivateCallback, srch_close_cb, 0);
	    wtip (w, "Close this dialog (but continue solving if active)");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 4); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 6); n++;
	    help_w = XmCreatePushButton(f_w, "Help", args, n);
	    XtAddCallback(help_w, XmNactivateCallback, srch_help_cb, 0);
	    wtip (help_w, "More detailed usage information");
	    XtManageChild (help_w);

	/* function text connected in between the rc's.
	 * arrange for Return to activate the Compile button.
	 */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, trc_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, brc_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	XtSetArg (args[n], XmNwordWrap, True); n++;
	func_w = XmCreateText (srchform_w, "Function", args, n);
	XtAddCallback (func_w, XmNvalueChangedCallback, srch_newfunc_cb, NULL);
	wtip(func_w,"Function to solve: enter fields manually, or via buttons");
	XtManageChild (func_w);
	sr_reg (func_w, NULL, solvecategory, 1);
}

/* return True whenever the srch control menu is up */
static int
srch_isup()
{
	return (isUp(srchshell_w));
}

/* return True whenever we are actually in the midst of controlling a search.
 */
static int
srching_now()
{
	return (on_w && XmToggleButtonGetState(on_w));
}

/* go through all the buttons pickable for plotting and set whether they
 * should appear to look like buttons.
 */
static void
srch_set_buttons (whether)
int whether;	/* whether setting up for plotting or for not plotting */
{
	buttonAsButton(valu_w, whether);
}

/* callback from the value button when it is to be used for
 * plotting or listing. if we have been put us in selecting mode, we look like
 * a button and we should inform them we have been picked.
 * otherwise, we do nothing (we didn't look like a button anyway).
 */
/* ARGSUSED */
static void
srch_use_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char msg[] =
	    "You must first successfully compile a search\nfunction before value may be selected.";

	if (srch_selecting) {
	    if (prog_isgood()) {
		plt_selection (srchvname);
		lst_selection (srchvname);
	    } else
		xe_msg (msg, 0);
	}
}

/* callback when the function definition changes
 */
/* ARGSUSED */
static void
srch_newfunc_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	f_showit (err_w, "New function not compiled");
}

/* callback from the compile button.
 */
/* ARGSUSED */
static void
srch_compile_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char *newexp;
	char errbuf[256];

	newexp = XmTextGetString (func_w);
	if (compile_expr (newexp, errbuf) <  0)
	    set_xmstring (err_w, XmNlabelString, errbuf);
	else
	    set_xmstring (err_w, XmNlabelString, "No compile errors");

	XtFree (newexp);

	/* as a courtesy, turn off searching if it's currently active */
	if (XmToggleButtonGetState(on_w))
	    XmToggleButtonSetState(on_w, False, True/*invoke cb*/);

	/* compute the new function.
	 * must update everything else first because our function uses that
	 *   stuff in all likelihood.
	 */
	if (prog_isgood()) {
	    Now *np = mm_get_now();
	    redraw_screen(1);
	    (void) srch_eval (mjd, (double *)NULL);
	}
}

/* callback from the help button.
 */
/* ARGSUSED */
static void
srch_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
static char *help_msg[] = {
"This menu controls the automatic searching facility. You define an arithmetic",
"or boolean function, using most of the fields xephem displays, then xephem",
"will automatically evaluate the function and adjust the time on each",
"iteration to search for the goal.",
"",
"To perform a search:",
"   enter a function,",
"   compile it,",
"   select a goal,",
"   set the desired accuracy,",
"   enable searching,",
"   perform the search by stepping xephem."
};

	hlp_dialog ("Search", help_msg, sizeof(help_msg)/sizeof(help_msg[0]));
}

/* callback from the "clear" function puch button.
 */
/* ARGSUSED */
static void
srch_clear_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char *txt = XmTextGetString (func_w);
	XmTextReplace (func_w, 0, strlen(txt), "");
	XtFree (txt);
}

/* callback from the "field enable" push button.
 * inform the other menues whether we are setting up for them to tell us
 * what fields to plot.
 */
/* ARGSUSED */
static void
srch_fields_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int whether = XmToggleButtonGetState(w);

	/* don't use our own result to srch with */
	srch_self_selection_mode = 1;
	all_selection_mode(whether);
	srch_self_selection_mode = 0;
}

/* callback from the Close button.
 */
/* ARGSUSED */
static void
srch_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtPopdown (srchshell_w);
}

/* user typed OK to the accuracy prompt. get his new value and use it */
/* ARGSUSED */
static void
srch_acc_ok_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char *text;
	
	get_xmstring(w, XmNtextString, &text);
	f_scansex (tmlimit, text, &tmlimit);
	XtFree (text);
	f_time (acc_w, tmlimit);
	XtUnmanageChild (w);
}

/* callback from the accuracy-change-request pushbutton.
 * put up a prompt dialog to ask for a new value.
 */
/* ARGSUSED */
static void
srch_acc_cb (wid, client, call)
Widget wid;
XtPointer client;
XtPointer call;
{
	static Widget dw;
	Widget w;
	
	if (!dw) {
	    XmString str, title;
	    Arg args[20];
	    int n;

	    str = XmStringCreate ("Accuracy (h:m:s):",
						    XmSTRING_DEFAULT_CHARSET);
	    title = XmStringCreate ("xephem Solver Accuracy",
						    XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg(args[n], XmNselectionLabelString, str);  n++;
	    XtSetArg(args[n], XmNdialogTitle, title);  n++;
	    XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	    XtSetArg(args[n], XmNmarginWidth, 10);  n++;
	    XtSetArg(args[n], XmNmarginHeight, 10);  n++;
	    dw = XmCreatePromptDialog(toplevel_w, "Accuracy", args, n);
	    set_something (dw, XmNcolormap, (XtArgVal)xe_cm);
	    XtAddCallback (dw, XmNokCallback, srch_acc_ok_cb, NULL);
	    XtAddCallback (dw, XmNmapCallback, prompt_map_cb, NULL);
	    XmStringFree (str);
	    XmStringFree (title);

	    w = XmSelectionBoxGetChild (dw, XmDIALOG_HELP_BUTTON);
	    XtUnmanageChild (w);
	}

	set_xmstring(dw, XmNtextString, "");

	XtManageChild (dw);

#if XmVersion >= 1001
	w = XmSelectionBoxGetChild (dw, XmDIALOG_TEXT);
	XmProcessTraversal (w, XmTRAVERSE_CURRENT);
	XmProcessTraversal (w, XmTRAVERSE_CURRENT); /* yes, twice!! */
#endif
}

/* callback from the search goal selection radio buttons.
 * same callback used for all of them.
 * client is pointer to desired search function.
 */
/* ARGSUSED */
static void
srch_goal_cb(w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int (*sfp)()= (int (*)())client;

	if (XmToggleButtonGetState(w)) {
	    /* better turn off searching if changing the search function! */
	    if (srch_f != sfp && srching_now())
		XmToggleButtonSetState(on_w, False, True /* invoke cb */);
	    srch_f = sfp;
	}
}

/* callback from the on/off toggle button activate.
 */
/* ARGSUSED */
static void
srch_on_off_cb(w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg =
	    "You must first successfully compile a search Function and\n select a Goal algorithm before searching may be enabled.";

	if (XmToggleButtonGetState(w)) {
	    /* insure there is a valid function and goal algorithm selected
	     * if turning searching on.
	     */
	    if (!prog_isgood()) {
		XmToggleButtonSetState(on_w, False, True/* invoke cb */);
		xe_msg (msg, 1);
		 
	    } else if (srch_f == 0) {
		XmToggleButtonSetState(on_w, False, True/* invoke cb */);
		xe_msg (msg, 1);
	    } else {
		srch_tmscalled = 0;
		/* turning on searhing so as a courtesy turn off the
		 * field selection mechanism if it's on now.
		 */
		if (XmToggleButtonGetState(field_w))
		    XmToggleButtonSetState(field_w, False, True);
	    }
	}
}

/* use successive paraboloidal fits to find when expression is at a
 * local minimum or maximum.
 */
static int
srch_minmax(Mjd, v, tmincp)
double Mjd;
double v;
double *tmincp;
{
	static double base;		/* for better stability */
	static double x_1, x_2, x_3;	/* keep in increasing order */
	static double y_1, y_2, y_3;
	double xm, a, b;

	if (srch_tmscalled == 0) {
	    base = Mjd;
	    x_1 = 0.0;
	    y_1 = v;
	    return (0);
	}
	Mjd -= base;
	if (srch_tmscalled == 1) {
	    /* put in one of first two slots */
	    if (Mjd < x_1) {
	        x_2 = x_1;  y_2 = y_1;
		x_1 = Mjd; y_1 = v;
	    } else {
		x_2 = Mjd; y_2 = v;
	    }
	    return (0);
	}
	if (srch_tmscalled == 2 || fabs(Mjd - x_1) < fabs(Mjd - x_3)) {
	    /* closer to x_1 so discard x_3.
	     * or if it's our third value we know to "discard" x_3.
	     */
	    if (Mjd > x_2) {
		x_3 = Mjd; y_3 = v;
	    } else {
		x_3 = x_2;  y_3 = y_2;
		if (Mjd > x_1) {
		    x_2 = Mjd; y_2 = v;
		} else {
		    x_2 = x_1;  y_2 = y_1;
		    x_1 = Mjd; y_1 = v;
		}
	    }
	    if (srch_tmscalled == 2)
		return (0);
	} else {
	    /* closer to x_3 so discard x_1 */
	    if (Mjd < x_2) {
		x_1 = Mjd;  y_1 = v;
	    } else {
		x_1 =  x_2;  y_1 = y_2;
		if (Mjd < x_3) {
		    x_2 = Mjd; y_2 = v;
		} else {
		    x_2 =  x_3; y_2 = y_3;
		    x_3 = Mjd; y_3 = v;
		}
	    }
	}

#ifdef TRACEMM
	{ char buf[NC];
	  (void) sprintf (buf, "x_1=%g y_1=%g x_2=%g y_2=%g x_3=%g y_3=%g",
						x_1, y_1, x_2, y_2, x_3, y_3);
	  xe_msg (buf,0);
	}
#endif
	a = y_1*(x_2-x_3) - y_2*(x_1-x_3) + y_3*(x_1-x_2);
	if (fabs(a) < 1e-10) {
	    /* near-0 zero denominator, ie, curve is pretty flat here,
	     * so assume we are done enough.
	     * signal this by forcing a 0 tminc.
	     */
	    *tmincp = 0.0;
	    return (-1);
	}
	b = (x_1*x_1)*(y_2-y_3) - (x_2*x_2)*(y_1-y_3) + (x_3*x_3)*(y_1-y_2);
	xm = -b/(2.0*a);
	*tmincp = (xm - Mjd)*24.0;
	return (fabs (*tmincp) < tmlimit ? -1 : 0);
}

/* use secant method to solve for time when expression passes through 0.
 */
static int
srch_solve0(Mjd, v, tmincp)
double Mjd;
double v;
double *tmincp;
{
	static double x0, x_1;	/* x(n-1) and x(n) */
	static double y_0, y_1;	/* y(n-1) and y(n) */
	double x_2;		/* x(n+1) */
	double df;		/* y(n) - y(n-1) */

	switch (srch_tmscalled) {
	case 0: x0 = Mjd; y_0 = v; return(0);
	case 1: x_1 = Mjd; y_1 = v; break;
	default: x0 = x_1; y_0 = y_1; x_1 = Mjd; y_1 = v; break;
	}

	df = y_1 - y_0;
	if (fabs(df) < 1e-10) {
	    /* near-0 zero denominator, ie, curve is pretty flat here,
	     * so assume we are done enough.
	     * signal this by forcing a 0 tminc.
	     */
	    *tmincp = 0.0;
	    return (-1);
	}
	x_2 = x_1 - y_1*(x_1-x0)/df;
	*tmincp = (x_2 - Mjd)*24.0;
	return (fabs (*tmincp) < tmlimit ? -1 : 0);
}

/* binary search for time when expression changes from its initial state.
 * if the change is outside the initial tminc range, then keep searching in that
 *    direction by tminc first before starting to divide down.
 */
static int
srch_binary(Mjd, v, tmincp)
double Mjd;
double v;
double *tmincp;
{
	static double lb, ub;		/* lower and upper bound */
	static int initial_state;
	int this_state = v >= 0.5;

#define	FLUNDEF	-9e10

	if (srch_tmscalled == 0) {
	    if (*tmincp >= 0.0) {
		/* going forwards in time so first Mjd is lb and no ub yet */
		lb = Mjd;
		ub = FLUNDEF;
	    } else {
		/* going backwards in time so first Mjd is ub and no lb yet */
		ub = Mjd;
		lb = FLUNDEF;
	    }
	    initial_state = this_state;
	    return (0);
	}

	if (ub != FLUNDEF && lb != FLUNDEF) {
	    if (this_state == initial_state)
		lb = Mjd;
	    else
		ub = Mjd;
	    *tmincp = ((lb + ub)/2.0 - Mjd)*24.0;
#ifdef TRACEBIN
	    { char buf[NC];
	      (void) sprintf (buf, "lb=%g ub=%g tminc=%g Mjd=%g is=%d ts=%d",
			    lb, ub, *tmincp, Mjd, initial_state, this_state);
	      xe_msg(buf, 0);
	    }
#endif
	    /* signal to stop if asking for time change less than TMLIMIT */
	    return (fabs (*tmincp) < tmlimit ? -1 : 0);
	} else if (this_state != initial_state) {
	    /* gone past; turn around half way */
	    if (*tmincp >= 0.0)
		ub = Mjd;
	    else
		lb = Mjd;
	    *tmincp /= -2.0;
	    return (0);
	} else {
	    /* just keep going, looking for first state change but we keep
	     * learning the lower (or upper, if going backwards) bound.
	     */
	    if (*tmincp >= 0.0)
		lb = Mjd;
	    else
		ub = Mjd;
	    return (0);
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: srchmenu.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
