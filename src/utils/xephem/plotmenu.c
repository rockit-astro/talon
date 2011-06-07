/* code to manage the stuff on the "plot" menu.
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
#include <Xm/DrawingA.h>
#include <Xm/FileSB.h>
#include <Xm/Label.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/TextF.h>

#include "P_.h"
#include "ps.h"

extern Widget	toplevel_w;
#define	XtD	XtDisplay(toplevel_w)
extern Colormap xe_cm;

extern FILE *fopenh P_((char *name, char *how));
extern char *getPrivateDir P_((void));
extern char *getShareDir P_((void));
extern char *syserrstr P_((void));
extern int confirm P_((void));
extern int existsh P_((char *filename));
extern int isUp P_((Widget shell));
extern int plot_cartesian P_((FILE *pfp, Widget widget, unsigned int nx,
    unsigned int ny, int flipx, int flipy, int grid, int xasdate));
extern void all_selection_mode P_((int whether));
extern void defaultTextFN P_((Widget w, int setcols, char *x, char *y));
extern void f_string P_((Widget w, char *s));
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
extern void sr_unreg P_((Widget w));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *string));
extern void xe_msg P_((char *msg, int app_modal));

static void plt_select P_((int whether));
static void plot_create_shell P_((void));
static void plt_activate_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_popdown_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_browse_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_fsbcancel_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_fsbok_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_reset P_((void));
static void plt_stop_selecting P_((void));
static void plt_turn_off P_((void));
static void init_next_row P_((void));
static void plt_try_append P_((void));
static void plt_try_overwrite P_((void));
static void plt_try_cancel P_((void));
static void plt_try_turn_on P_((void));
static void plt_turn_on P_((char *how));
static void plt_da_manage P_((void));
static void plt_da_destroy P_((Widget da_w));
static void plt_print P_((void));
static void plt_da_print_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_da_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_da_unmap_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_da_flipx_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_da_flipy_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_da_grid_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_da_xasdate_cb P_((Widget w, XtPointer client, XtPointer call));
static void plt_da_exp_cb P_((Widget da_w, XtPointer client, XtPointer call));
static void plt_drawall P_((Widget da_w));


/* max number of things we can keep track of at once to plot */
#define	MAXPLTLINES	10
#define	MAXFLDNAM	32	/* longest allowed field name */

static Widget plotshell_w;	/* main shell */
static Widget fsb_w;		/* FSB */
static Widget select_w, active_w, prompt_w;
static Widget title_w, filename_w;
static Widget table_w[MAXPLTLINES][4];	/* column indeces follow.. */
#define	T	0		/* tag label text field */
#define	X	1		/* X label */
#define	Y	2		/* Y label */
#define	FORM	3		/* the form that holds the three above */
static int selecting_xy;	/* one of X or Y */

static FILE *plt_fp;            /* the plot file; == 0 means don't plot */

static Widget da_w_save;	/* used to set up for a print action */

/* plt_activate_cb client values. */
enum { SELECT, ACTIVE, SHOW};

static char plotcategory[] = "Tools -- Plot";	/* Save category */

/* store the name of each x and y line to track and their values.
 * we get the label straight from the Text widget in the table as needed.
 */
typedef struct {
    char pl_xn[MAXFLDNAM];	/* name of x field, or 0 if none */
    double pl_xv;		/* last known value of x field */
    char pl_yn[MAXFLDNAM];	/* name of y field, or 0 if none */
    double pl_yv;		/* last known value of x field */
} PltLine;
static PltLine pltlines[MAXPLTLINES];
static int npltlines;		/* number of pltlines[] in actual use */

/* one of these gets malloced and passed to the drawing area expose callback via
 * its client parameter. be sure to free it when the parent FormDialog goes
 * away too.
 * by doing this, we can have lots of different plots up at once and yet we
 * don't have to keep track of them - they track themselves.
 */
typedef struct {
    char *filename;	/* name of file being plotted (also malloced) */
    FILE *fp;		/* FILE pointer for the file */
    int flipx, flipy;	/* flip state for this instance */
    Widget fx_w, fy_w;	/* widgets for sr_unreg() */
    int grid;		/* whether to include a grid */
    int xasdate;	/* whether to show x axis as dates */
    Widget as_w, g_w;	/* widgets for sr_unreg() */
} DrawInfo;

/* called when the plot menu is activated via the main menu pulldown.
 * if never called before, create and manage all the widgets as a child of a
 * form. otherwise, just get going.
 */
void
plot_manage ()
{
	if (!plotshell_w)
	    plot_create_shell();
	
	XtPopup (plotshell_w, XtGrabNone);
	set_something (plotshell_w, XmNiconic, (XtArgVal)False);
}

/* called by the other menus (data, etc) as their buttons are
 * selected to inform us that that button is to be included in a plot.
 */
void
plt_selection (name)
char *name;
{
	Widget tw;

	if (!isUp(plotshell_w) || !XmToggleButtonGetState(select_w))
	    return;

	tw = table_w[npltlines][selecting_xy];
	set_xmstring (tw, XmNlabelString, name);
	XtManageChild (tw);

	if (selecting_xy == X) {
	    (void) strncpy (pltlines[npltlines].pl_xn, name, MAXFLDNAM-1);
	    selecting_xy = Y;
	    XtManageChild (prompt_w);
	    f_string (prompt_w, "Select quantity for Y..");
	} else {
	    (void) strncpy (pltlines[npltlines].pl_yn, name, MAXFLDNAM-1);
	    if (++npltlines == MAXPLTLINES)
		plt_stop_selecting();
	    else {
		selecting_xy = X;
		init_next_row();
	    }
	}
}

/* called as each different field is written -- just save in pltlines[]
 * if we are interested in it.
 * might have the same field listed more than once so can't stop if find one.
 */
void
plt_log (name, value)
char *name;
double value;
{
	if (plt_fp) {
	    PltLine *plp;
	    for (plp = pltlines; plp < &pltlines[npltlines]; plp++) {
		if (strcmp (name, plp->pl_xn) == 0)
		    plp->pl_xv = value;
		if (strcmp (name, plp->pl_yn) == 0)
		    plp->pl_yv = value;
	    }
	}
}

/* called when all fields have been updated and it's time to
 * write the active plotfields to the current plot file, if one is open.
 */
void
plot()
{
	if (plt_fp) {
	    /* plot in order of original selection */
	    PltLine *plp;
	    for (plp = pltlines; plp < &pltlines[npltlines]; plp++) {
		char *lbl = XmTextFieldGetString (table_w[plp-pltlines][T]);
		(void) fprintf (plt_fp, "%c,%.12g,%.12g\n", lbl[0],
						    plp->pl_xv, plp->pl_yv);
		XtFree (lbl);
	    }
	}
}

int
plot_ison()
{
	return (plt_fp != 0);
}

/* called to put up or remove the watch cursor.  */
void
plt_cursor (c)
Cursor c;
{
	Window win;

	if (plotshell_w && (win = XtWindow(plotshell_w)) != 0) {
	    Display *dsp = XtDisplay(plotshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* inform the other menues whether we are setting up for them to tell us
 * what fields to plot.
 */
static void
plt_select(whether)
int whether;
{
	all_selection_mode(whether);
}

static void
plot_create_shell()
{
	static struct {
	    char *title;
	    int cb_data;
	    Widget *wp;
	    char *tip;
	} tbs[] = {
	    {"Select fields", SELECT, &select_w,
	    	"When on, data fields eligible for plotting are selectable buttons"},
	    {"Create plot file", ACTIVE, &active_w,
	    	"When on, selected fields are written to the named file at each main Update"},
	    {"Show plot file", SHOW, NULL, "Display the named plot file"},
	};
	XmString str;
	Widget f_w, rc_w;
	Widget plotform_w;
	Widget w;
	Arg args[20];
	int i, n;

	/* create the main dialog */

	n = 0;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle, "xephem Plot Control"); n++;
	XtSetArg (args[n], XmNiconName, "Plot"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	plotshell_w = XtCreatePopupShell ("Plot", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (plotshell_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (plotshell_w, XmNpopdownCallback, plt_popdown_cb, NULL);
	sr_reg (plotshell_w, "XEphem*Plot.x", plotcategory, 0);
	sr_reg (plotshell_w, "XEphem*Plot.y", plotcategory, 0);

	n = 0;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_ANY); n++;
	plotform_w = XmCreateForm (plotshell_w, "PlotF", args, n);
	XtAddCallback (plotform_w, XmNhelpCallback, plt_help_cb, 0);
	XtManageChild (plotform_w);

	/* make a RowColumn to hold everything */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNspacing, 6); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	rc_w = XmCreateRowColumn(plotform_w, "PlotRC", args, n);
	XtManageChild (rc_w);

	/* make the control toggle buttons */

	for (i = 0; i < XtNumber(tbs); i++) {
	    str = XmStringCreate(tbs[i].title, XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateToggleButton(rc_w, "PlotTB", args, n);
	    XmStringFree (str);
	    XtAddCallback(w, XmNvalueChangedCallback, plt_activate_cb,
						    (XtPointer)tbs[i].cb_data);
	    if (tbs[i].wp)
		*tbs[i].wp = w;
	    if (tbs[i].tip)
		wtip (w, tbs[i].tip);
	    XtManageChild (w);
	}

	/* create filename text area and its label */

	n = 0;
	str = XmStringCreate("File name:", XmSTRING_DEFAULT_CHARSET);
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNlabelString, str); n++;
	w = XmCreateLabel (rc_w, "PlotFnL", args, n);
	XmStringFree (str);
	XtManageChild (w);

	n = 0;
	w = XmCreatePushButton (rc_w, "Browse...", args, n);
	XtAddCallback (w, XmNactivateCallback, plt_browse_cb, NULL);
	wtip (w, "Open a file selection box for hunting .plt files");
	XtManageChild (w);

	n = 0;
	filename_w = XmCreateTextField (rc_w, "Filename", args, n);
	defaultTextFN (filename_w, 1, getShareDir(), "auxil/analemma.plt");
	wtip (filename_w, "Enter name of file to write");
	XtManageChild (filename_w);
	sr_reg (filename_w, NULL, plotcategory, 1);

	/* create title text area and its label */

	n = 0;
	str = XmStringCreate("Title:", XmSTRING_DEFAULT_CHARSET);
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNlabelString, str); n++;
	w = XmCreateLabel (rc_w, "PlotTL", args, n);
	XtManageChild (w);
	XmStringFree (str);

	n = 0;
	XtSetArg (args[n], XmNcolumns, 40); n++;
	title_w = XmCreateTextField (rc_w, "PlotTitle", args, n);
	wtip (title_w, "Enter a title to be written to the file");
	XtManageChild (title_w);

	/* create prompt line -- it will be managed as necessary */

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	prompt_w = XmCreateLabel (rc_w, "PlotPrompt", args, n);

	/* create the table.
	 * each row is in a form to control its shape.
	 * loop index of -1 is used to make the column headings.
	 * the table entries are all managed but the forms are not at this time.
	 */

	for (i = -1; i < MAXPLTLINES; i++) {
	    n = 0;
	    XtSetArg (args[n], XmNfractionBase, 9); n++;
	    f_w = XmCreateForm (rc_w, "PlotTF", args, n);

	    /* save the form unless we are just making the column headings */
	    if (i == -1)
		XtManageChild (f_w);
	    else
		table_w[i][FORM] = f_w;

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 0); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 1); n++;
	    if (i == -1) {
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		w = XmCreateLabel (f_w, "Tag", args, n);
		wtip (w, "Column of tags for each X/Y pair to plot");
		XtManageChild (w);
	    } else {
		XtSetArg (args[n], XmNmaxLength, 1); n++;
		XtSetArg (args[n], XmNcolumns, 1); n++;
		table_w[i][T] = XmCreateTextField (f_w, "PlotTag", args, n);
		XtManageChild (table_w[i][T]);
	    }

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 2); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 5); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    if (i == -1) {
		w = XmCreateLabel (f_w, "_X_", args, n);
		wtip (w, "Column of X fields for each X/Y pair to plot");
		XtManageChild (w);
	    } else {
		table_w[i][X] = XmCreateLabel (f_w, "PlotX", args, n);
		XtManageChild (table_w[i][X]);
	    }

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 6); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 9); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    if (i == -1) {
		w = XmCreateLabel (f_w, "_Y_", args, n);
		wtip (w, "Column of Y fields for each X/Y pair to plot");
		XtManageChild (w);
	    } else {
		table_w[i][Y] = XmCreateLabel (f_w, "PlotY", args, n);
		XtManageChild (table_w[i][Y]);
	    }
	}

	/* create a separator */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (w);

	/* make a form to hold the close and help buttons evenly */

	n = 0;
	XtSetArg (args[n], XmNfractionBase, 7); n++;
	f_w = XmCreateForm (rc_w, "PlotCF", args, n);
	XtManageChild(f_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 1); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 3); n++;
	    w = XmCreatePushButton (f_w, "Close", args, n);
	    wtip (w, "Close this dialog ((but continue plotting if active)");
	    XtAddCallback (w, XmNactivateCallback, plt_close_cb, 0);
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
	    XtAddCallback (w, XmNactivateCallback, plt_help_cb, 0);
	    XtManageChild (w);
}

/* callback from any of the plot menu toggle buttons being activated.
 */
/* ARGSUSED */
static void
plt_activate_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *t = (XmToggleButtonCallbackStruct *) call;
	int what = (int) client;

	switch (what) {
	case SELECT:
	    if (t->set) {
		/* first turn off plotting, if on, while we change things */
		if (XmToggleButtonGetState(active_w))
		    XmToggleButtonSetState(active_w, False, True);
		plt_reset();	/* reset pltlines array and unmanage the table*/
		plt_select(1);	/* inform other menus to inform us of fields */
		init_next_row();/* set first tag to something and show it */
		selecting_xy = X;
	    } else
		plt_stop_selecting();
	    break;
	case ACTIVE:
	    if (t->set) {
		/* first turn off selecting, if on */
		if (XmToggleButtonGetState(select_w))
		    XmToggleButtonSetState(select_w, False, True);
		plt_try_turn_on();
	    } else
		plt_turn_off();
	    break;
	case SHOW:
	    /* turn off plotting, if on, to make sure file is complete. */
	    if (XmToggleButtonGetState(active_w))
		XmToggleButtonSetState(active_w, False, True);
	    plt_da_manage();
	    /* we want this to work like it was a pushbutton, really */
	    XmToggleButtonSetState(w, False, False);
	    break;
	}
}

/* callback from the FSB Cancel PB */
/* ARGSUSED */
static void
plt_fsbcancel_cb (w, client, call)
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
plt_fsbok_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (fsb_w) {
	    char *fn;

	    get_xmstring (fsb_w, XmNdirSpec, &fn);
	    XmTextFieldSetString (filename_w, fn);
	    XtFree (fn);
	    XtUnmanageChild (fsb_w);
	}
}

/* callback from the Browse PB */
/* ARGSUSED */
static void
plt_browse_cb (w, client, call)
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
	    XtSetArg (args[n], XmNtitle, "Plot File browser"); n++;
	    XtSetArg (args[n], XmNwidth, 400); n++;
	    XtSetArg (args[n], XmNmarginHeight, 10); n++;
	    XtSetArg (args[n], XmNmarginWidth, 10); n++;
	    fsb_w = XmCreateFileSelectionDialog (toplevel_w, "PFSB", args, n);
	    XtAddCallback (fsb_w, XmNmapCallback, prompt_map_cb, NULL);
	    XtAddCallback (fsb_w, XmNokCallback, plt_fsbok_cb, NULL);
	    XtAddCallback (fsb_w, XmNcancelCallback, plt_fsbcancel_cb, NULL);
	    set_xmstring (fsb_w, XmNcancelLabelString, "Close");

	    /* set default dir and pattern */
	    set_xmstring (fsb_w, XmNdirectory, getPrivateDir());
	    set_xmstring (fsb_w, XmNpattern, "*.plt");

	    w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_HELP_BUTTON);
	    XtUnmanageChild (w);
	}

	XtManageChild (fsb_w);
}

/* callback when the main plot window is closed */
/* ARGSUSED */
static void
plt_popdown_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (fsb_w)
	    XtUnmanageChild (fsb_w);
}

/* callback from the Close button.
 */
/* ARGSUSED */
static void
plt_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtPopdown (plotshell_w);
}

/* callback from the Help button.
 */
/* ARGSUSED */
static void
plt_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
static char *help_msg[] = {
"This menu controls the plot generation and display functionality of xephem.",
"Select fields to form x/y pairs, enable plotting to write them to a file on",
"each xephem iteration step, then view. Each file may be titled, as desired."
};
	hlp_dialog ("Plot", help_msg, sizeof(help_msg)/sizeof(help_msg[0]));
}

/* forget our list, and unmanage the table.
 */
static void
plt_reset()
{
	int i;

	for (i = 0; i < npltlines; i++)
	    XtUnmanageChild (table_w[i][FORM]);

	npltlines = 0;
}

/* stop selecting: unmanage a partitially filled in line specs; tell
 * everybody else to drop their buttons, make sure toggle is off.
 */
static void
plt_stop_selecting()
{
	if (npltlines < MAXPLTLINES)
	    XtUnmanageChild (table_w[npltlines][FORM]);

	XmToggleButtonSetState (select_w, False, False);
	plt_select(0);
	XtUnmanageChild (prompt_w);
}

static void
plt_turn_off ()
{
	if (plt_fp) {
	    (void) fclose (plt_fp);
	    plt_fp = 0;
	}
}

static void
init_next_row()
{
	char buf[100];

	XtManageChild (prompt_w);
	f_string (prompt_w, "Select quantity for X..");

	(void) sprintf (buf, "%c", 'A' + npltlines);
	XmTextFieldSetString (table_w[npltlines][T], buf);

	f_string (table_w[npltlines][X], " ");
	f_string (table_w[npltlines][Y], " ");

	XtManageChild (table_w[npltlines][FORM]);
}

/* called from the query routine when want to append to an existing plot file.*/
static void
plt_try_append()
{
	plt_turn_on("a");
}

/* called from the query routine when want to overwrite to an existing plot
 * file.
 */
static void
plt_try_overwrite()
{
	plt_turn_on("w");
}

/* called from the query routine when want decided not to make a plot file.  */
static void
plt_try_cancel()
{
	XmToggleButtonSetState (active_w, False, False);
}

/* attempt to open file for use as a plot file.
 * if it doesn't exist, then go ahead and make it.
 * but if it does, first ask wheher to append or overwrite.
 */
static void
plt_try_turn_on()
{
	char *txt = XmTextFieldGetString (filename_w);
	if (existsh (txt) == 0 && confirm()) {
	    char *buf;
	    buf = XtMalloc (strlen(txt)+100);
	    (void) sprintf (buf, "%s exists:\nAppend or Overwrite?", txt);
	    query (toplevel_w, buf, "Append", "Overwrite", "Cancel",
			    plt_try_append, plt_try_overwrite, plt_try_cancel);
	    XtFree (buf);
	} else
	    plt_try_overwrite();

	XtFree (txt);
}

/* turn on plotting.
 * establish a file to use (and thereby set plt_fp, the plotting_is_on flag).
 */
static void
plt_turn_on (how)
char *how;	/* fopen how argument */
{
	char *txt;

	/* plotting is on if file opens ok */
	txt = XmTextFieldGetString (filename_w);
	plt_fp = fopenh (txt, how);
	if (!plt_fp) {
	    char *buf;
	    XmToggleButtonSetState (active_w, False, False);
	    buf = XtMalloc (strlen(txt)+100);
	    (void) sprintf (buf, "%s:\n%s", txt, syserrstr());
	    xe_msg (buf, 1);
	    XtFree (buf);
	}
	XtFree (txt);
	
	if (plt_fp) {
	    /* add a title if it's not null */
	    txt = XmTextFieldGetString (title_w);
	    if (txt[0] != '\0')
		(void) fprintf (plt_fp, "* %s\n", txt);
	    XtFree (txt);
	}
}

/* make a new drawing area widget and manage it. it's unmanaged and destroyed
 *   from the Close button or if something goes wrong during plotting.
 * open the plot file and save it, the current state of the flipx/flipy/grid
 *   buttons and the filename in a DrawInfo struct in the userData resource
 *   for the FormDialog where the drawingarea callback can get at it each time.
 * this way, we can have lots of different plots up at once yet we don't
 *   have to keep track of them.
 * by leaving the file open, we gain some protection against it being removed
 *   or renamed.
 */
static void
plt_da_manage()
{
	Widget daform_w;
	Widget da_w, w;
	Widget mb_w, pd_w, cb_w;
	XmString str;
	Arg args[20];
	char titlebuf[64];
	int n;
	DrawInfo *di;
	FILE *fp;
	char *fn;

	/* first make sure we can open the plot file */
	fn = XmTextFieldGetString (filename_w);
	fp = fopenh (fn, "r");
	if (!fp) {
	    char *buf;
	    buf = XtMalloc (strlen(fn)+100);
	    (void) sprintf (buf, "%s:\n%s", fn, syserrstr());
	    xe_msg (buf, 1);
	    XtFree (buf);
	    XtFree (fn);
	    return;
	}

	/* create the form dialog parent. */
	n = 0;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	daform_w = XmCreateFormDialog (plotshell_w, "PlotDisplay", args, n);
	set_something (daform_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (daform_w, XmNmapCallback, prompt_map_cb, NULL);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	(void) sprintf (titlebuf, "xephem Plot of `%.*s'",
						(int)sizeof(titlebuf)-20, fn);
	n = 0;
	XtSetArg (args[n], XmNtitle, titlebuf); n++;
	XtSetValues (XtParent(daform_w), args, n);

	/* make the DrawInfo structure and save it in the userData of the Form.
	 * the memory gets freed when the dialog is closed/unmanaged.
	 * the options get inited from their toggle buttons.
	 */
	di = (DrawInfo *) XtMalloc (sizeof(DrawInfo));
	di->filename = fn;
	di->fp = fp;
	set_something (daform_w, XmNuserData, (XtArgVal)di);

	/* create the menu bar across the top */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	mb_w = XmCreateMenuBar (daform_w, "MB", args, n);
	XtManageChild (mb_w);
	
	/* create the drawing area and connect plt_da_exp_cb().
	 * N.B. be sure this guys parent is the FormDialog so exp_cb can find
	 *   the DrawInfo by looking there at its userData.
	 * make this as high as it is wide when it is first mapped.
	 * N.B. if ever want this is a frame beware that other functions
	 *   assume that the daform_w is the parent of the DrawingArea.
	 */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, mb_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 2); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNmarginWidth, 0); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	da_w = XmCreateDrawingArea (daform_w, "PlotDA", args, n);
	XtAddCallback (da_w, XmNexposeCallback, plt_da_exp_cb, NULL);
	XtManageChild (da_w);

	/* when unmap the form, pass the da to the unmap callback */
	XtAddCallback (daform_w, XmNunmapCallback, plt_da_unmap_cb,
							    (XtPointer)da_w);

	/* make the Control pulldown */

	n = 0;
	pd_w = XmCreatePulldownMenu (mb_w, "ControlPD", args, n);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pd_w);  n++;
	    XtSetArg (args[n], XmNmnemonic, 'C'); n++;
	    cb_w = XmCreateCascadeButton (mb_w, "ControlCB", args, n);
	    set_xmstring (cb_w, XmNlabelString, "Control");
	    XtManageChild (cb_w);

	    /* make the print button */
	    n = 0;
	    w = XmCreatePushButton (pd_w, "PltPrint", args, n);
	    XtAddCallback (w, XmNactivateCallback, plt_da_print_cb, da_w);
	    set_xmstring (w, XmNlabelString, "Print...");
	    wtip (w, "Print this plot");
	    XtManageChild (w);

	    /* add a separator */
	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    /* make the close button.
	     * it destroys the dialog and frees the DrawInfo struct.
	     */
	    n = 0;
	    w = XmCreatePushButton (pd_w, "Close", args, n);
	    wtip (w, "Close this plot display");
	    XtAddCallback (w, XmNactivateCallback, plt_da_close_cb, daform_w);
	    XtManageChild (w);

	/* make the View pulldown */

	n = 0;
	pd_w = XmCreatePulldownMenu (mb_w, "ViewPD", args, n);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pd_w);  n++;
	    XtSetArg (args[n], XmNmnemonic, 'V'); n++;
	    cb_w = XmCreateCascadeButton (mb_w, "ViewCB", args, n);
	    set_xmstring (cb_w, XmNlabelString, "View");
	    XtManageChild (cb_w);

	    str = XmStringCreate("Flip X", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateToggleButton(pd_w, "FlipX", args, n);
	    di->flipx = XmToggleButtonGetState (w);
	    di->fx_w = w;
	    XmStringFree (str);
	    XtAddCallback (w, XmNvalueChangedCallback, plt_da_flipx_cb, da_w);
	    wtip (w, "Flip left-to-right");
	    XtManageChild (w);
	    /* TODO - can not use until fix findWFB()
	     * sr_reg (w, NULL, plotcategory, 1);
	     */

	    str = XmStringCreate("Flip Y", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateToggleButton(pd_w, "FlipY", args, n);
	    di->flipy = XmToggleButtonGetState (w);
	    di->fy_w = w;
	    XmStringFree (str);
	    XtAddCallback (w, XmNvalueChangedCallback, plt_da_flipy_cb, da_w);
	    wtip (w, "Flip top-to-bottom");
	    XtManageChild (w);
	    /* TODO - can not use until fix findWFB()
	     * sr_reg (w, NULL, plotcategory, 1);
	     */

	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    w = XmCreateToggleButton(pd_w, "Grid", args, n);
	    di->grid = XmToggleButtonGetState (w);
	    di->g_w = w;
	    XtAddCallback (w, XmNvalueChangedCallback, plt_da_grid_cb, da_w);
	    wtip (w, "Overlay plot with calibrated grid");
	    XtManageChild (w);
	    /* TODO - can not use until fix findWFB()
	     * sr_reg (w, NULL, plotcategory, 1);
	     */

	    str = XmStringCreate("Show X-Axis as Dates",
						    XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateToggleButton(pd_w, "XAsDate", args, n);
	    di->xasdate = XmToggleButtonGetState (w);
	    di->as_w = w;
	    XmStringFree (str);
	    XtAddCallback (w, XmNvalueChangedCallback, plt_da_xasdate_cb, da_w);
	    wtip (w, "Assume ordinate values are decimal years and display as dates");
	    XtManageChild (w);
	    /* TODO - can not use until fix findWFB()
	     * sr_reg (w, NULL, plotcategory, 1);
	     */

	/* go. the expose will do the actual plotting */
	XtManageChild (daform_w);
}

/* called with the DrawingArea when finished with a plot.
 * use it to get the FormDialog parent and reclaim all memory in its userData
 *   DrawInfo and destroy the Form and hence everything in it.
 */
static void
plt_da_destroy (da_w)
Widget da_w;
{
	Widget daform_w = XtParent(da_w);
	DrawInfo *di;

	get_something (daform_w, XmNuserData, (XtArgVal)&di);
	(void) fclose (di->fp);
	XtFree(di->filename);
	XtFree((char *)di);
	/* TODO: don't do these until fix findWFB() 
	sr_unreg (di->fx_w);
	sr_unreg (di->fy_w);
	sr_unreg (di->g_w);
	sr_unreg (di->as_w);
	*/

	/* destroy the shell, not just the form */
	while (!XtIsWMShell (daform_w))
	    daform_w = XtParent(daform_w);
	XtDestroyWidget(daform_w);

	/* invalidate the saved print widget if it matches */
	if (da_w_save == da_w)
	    da_w_save = 0;
}

/* proceed to generate a postscript file from da_w_save if it is still up.
 * reset da_w_save and call XPSClose() when finished.
 */
static void
plt_print()
{
	if (da_w_save) {
	    Dimension w, h;
	    int iw, ih;

	    get_something (da_w_save, XmNwidth, (XtArgVal)&w);
	    get_something (da_w_save, XmNheight, (XtArgVal)&h);
	    iw = (int)w;
	    ih = (int)h;

	    /* draw in an area 6.5w x 8h centered 1in down from top */
	    if (16*iw >= 13*ih)
		XPSXBegin (XtWindow(da_w_save), 0, 0, iw, ih, 1*72, 10*72,
							    (int)(6.5*72));
	    else {
		int pw = 72*8*iw/ih;	/* width on paper when 8 hi */
		XPSXBegin (XtWindow(da_w_save), 0, 0, iw, ih,
					    (int)((8.5*72-pw)/2), 10*72, pw);
	    }

	    plt_drawall (da_w_save);
	    XPSXEnd();
	    XPSClose();
	    da_w_save = 0;
	} else {
	    xe_msg ("Can not print after plot has been dismissed.", 1);
	    XPSClose();
	}
}

/* called when the Print button is pushed on a plot.
 * store the da_w (passed in client) and ask for print setup info.
 */
/* ARGSUSED */
static void
plt_da_print_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Widget da_w = (Widget)client;

	da_w_save = da_w;
	XPSAsk ("Plot", plt_print);
}

/* called when the Close button is pushed on a plot.
 * just unmap the dialog (passed as client).
 */
/* ARGSUSED */
static void
plt_da_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild ((Widget)client);
}

/* called when a plot is unmapped, either via a WM command or the Close button.
 * free the DrawInfo and destroy the DrawingArea (passed as client).
 */
/* ARGSUSED */
static void
plt_da_unmap_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	plt_da_destroy ((Widget)client);
}

/* callback from the Flip X toggle button within the drawing FormDiag itself.
 * toggle the x bit in the parent's DrawInfo structure and fake an expose.
 * client is the DrawingArea widget.
 */
/* ARGSUSED */
static void
plt_da_flipx_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *t = (XmToggleButtonCallbackStruct *) call;
	Widget da_w = (Widget) client;
	Widget daform_w = XtParent(da_w);
	Display *dsp = XtDisplay(da_w);
	Window win = XtWindow(da_w);
	DrawInfo *di;
	XExposeEvent ev;
	Window root;
	int x, y;
	unsigned int nx, ny, bw, d;

	get_something (daform_w, XmNuserData, (XtArgVal)&di);
	di->flipx = t->set;

	XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);

	ev.type = Expose;
	ev.send_event = 1;	/* gets set anyways */
	ev.display = dsp;
	ev.window = win;
	ev.x = ev.y = 0;
	ev.width = nx;
	ev.height = ny;
	ev.count = 0;

	XSendEvent (dsp, win, /*propagate*/False, ExposureMask, (XEvent *)&ev);
}

/* callback from the Flip Y toggle button within the drawing FormDiag itself.
 * toggle the y bit in the parent's DrawInfo structure and fake an expose.
 * client is the DrawingArea widget.
 */
/* ARGSUSED */
static void
plt_da_flipy_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *t = (XmToggleButtonCallbackStruct *) call;
	Widget da_w = (Widget) client;
	Widget daform_w = XtParent(da_w);
	Display *dsp = XtDisplay(da_w);
	Window win = XtWindow(da_w);
	DrawInfo *di;
	XExposeEvent ev;
	Window root;
	int x, y;
	unsigned int nx, ny, bw, d;

	get_something (daform_w, XmNuserData, (XtArgVal)&di);
	di->flipy = t->set;

	XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);

	ev.type = Expose;
	ev.send_event = 1;	/* gets set anyways */
	ev.display = dsp;
	ev.window = win;
	ev.x = ev.y = 0;
	ev.width = nx;
	ev.height = ny;
	ev.count = 0;

	XSendEvent (dsp, win, /*propagate*/False, ExposureMask, (XEvent *)&ev);
}

/* callback from the grid toggle button within the drawing FormDiag itself.
 * toggle the grid flag in the parent's DrawInfo structure and fake an expose.
 * client is the DrawingArea widget.
 */
/* ARGSUSED */
static void
plt_da_grid_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *t = (XmToggleButtonCallbackStruct *) call;
	Widget da_w = (Widget) client;
	Widget daform_w = XtParent(da_w);
	Display *dsp = XtDisplay(da_w);
	Window win = XtWindow(da_w);
	DrawInfo *di;
	XExposeEvent ev;
	Window root;
	int x, y;
	unsigned int nx, ny, bw, d;

	get_something (daform_w, XmNuserData, (XtArgVal)&di);
	di->grid = t->set;

	XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);

	ev.type = Expose;
	ev.send_event = 1;	/* gets set anyways */
	ev.display = dsp;
	ev.window = win;
	ev.x = ev.y = 0;
	ev.width = nx;
	ev.height = ny;
	ev.count = 0;

	XSendEvent (dsp, win, /*propagate*/False, ExposureMask, (XEvent *)&ev);
}

/* callback from the X-is-date toggle button within the drawing FormDiag itself.
 * toggle the xisdate flag in the parent's DrawInfo structure and fake an
 * expose. client is the DrawingArea widget.
 */
/* ARGSUSED */
static void
plt_da_xasdate_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *t = (XmToggleButtonCallbackStruct *) call;
	Widget da_w = (Widget) client;
	Widget daform_w = XtParent(da_w);
	Display *dsp = XtDisplay(da_w);
	Window win = XtWindow(da_w);
	DrawInfo *di;
	XExposeEvent ev;
	Window root;
	int x, y;
	unsigned int nx, ny, bw, d;

	get_something (daform_w, XmNuserData, (XtArgVal)&di);
	di->xasdate = t->set;

	XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);

	ev.type = Expose;
	ev.send_event = 1;	/* gets set anyways */
	ev.display = dsp;
	ev.window = win;
	ev.x = ev.y = 0;
	ev.width = nx;
	ev.height = ny;
	ev.count = 0;

	XSendEvent (dsp, win, /*propagate*/False, ExposureMask, (XEvent *)&ev);
}

/* plot drawing area's expose callback.
 * redraw the graph to the (new?) size.
 * get a DrawInfo from our parent's userData.
 */
/* ARGSUSED */
static void
plt_da_exp_cb (da_w, client, call)
Widget da_w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;

	switch (c->reason) {
	case XmCR_EXPOSE: {
	    XExposeEvent *e = &c->event->xexpose;
	    XSetWindowAttributes swa;
	    unsigned long mask = CWBitGravity;

	    /* turn off gravity so we get fresh exposes when grow and shrink.
	     * do it every time because this expose handler is used for all
	     * the windows -- yes, it is over kill but not worth remembering
	     * which windows have already been set.
	     */
	    swa.bit_gravity = ForgetGravity;
	    XChangeWindowAttributes (e->display, e->window, mask, &swa);

	    /* wait for the last in the series */
	    if (e->count != 0)
		return;
	    break;
	    }
	default:
	    printf ("Unexpected da_w event. type=%d\n", c->reason);
	    exit(1);
	}

	/* draw the plot from scratch */
	plt_drawall (da_w);
}

/* given a drawing area, draw the plot.
 * remember: the parent of the da is the form and it's userData is the di info.
 */
static void
plt_drawall (da_w)
Widget da_w;
{
	Dimension w, h;
	Widget daform_w;
	DrawInfo *di;

	watch_cursor (1);

	get_something (da_w, XmNwidth, (XtArgVal)&w);
	get_something (da_w, XmNheight, (XtArgVal)&h);

	daform_w = XtParent(da_w);
	get_something (daform_w, XmNuserData, (XtArgVal)&di);
	XClearWindow (XtDisplay(da_w), XtWindow(da_w));
	rewind (di->fp);
	if (plot_cartesian (di->fp, da_w, w, h, di->flipx, di->flipy, di->grid,
							    di->xasdate) < 0) {
	    /* had trouble, so done with this FormDialog.
	     */
	    char buf[128];
	    (void) sprintf (buf, "Error plotting `%.*s'\n", (int)sizeof(buf)-20,
								di->filename);
	    xe_msg (buf, 0);
	    plt_da_destroy (da_w);
	}

	watch_cursor (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: plotmenu.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
