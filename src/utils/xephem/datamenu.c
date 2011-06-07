/* code to manage the stuff on the "data" menu.
 * functions for the main data table are prefixed with dm.
 * functions for the setup menu are prefixed with ds.
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#if defined(__STDC__)
#include <stdlib.h>
#include <string.h>
#endif

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/SelectioB.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "preferences.h"
#include "dm.h"

extern Widget	toplevel_w;
extern Colormap xe_cm;

extern FILE *fopenh P_((char *name, char *how));
extern Now *mm_get_now P_((void));
extern Obj *db_basic P_((int id));
extern char *cns_name P_((int id));
extern char *getPrivateDir P_((void));
extern char *mm_getsite P_((void));
extern char *syserrstr P_((void));
extern double atod P_((char *buf));
extern int any_ison P_((void));
extern int confirm P_((void));
extern int cns_pick P_((double ra, double dec, double e));
extern int existsh P_((char *filename));
extern int isUp P_((Widget shell));
extern void db_update P_((Obj *op));
extern void buttonAsButton P_((Widget w, int whether));
extern void defaultTextFN P_((Widget w, int setcols, char *x, char *y));
extern void dm_update P_((Now *np, int how_much));
extern void f_prdec P_((Widget w, double a));
extern void f_dm_angle P_((Widget w, double a));
extern void f_double P_((Widget w, char *fmt, double f));
extern void f_mtime P_((Widget w, double t));
extern void f_pangle P_((Widget w, double a));
extern void f_ra P_((Widget w, double ra));
extern void f_sexa P_((Widget wid, double a, int w, int fracbase));
extern void f_showit P_((Widget w, char *s));
extern void f_string P_((Widget w, char *s));
extern void f_time P_((Widget w, double t));
extern void fs_mtime P_((char out[], double t));
extern void fs_sexa P_((char buf[], double a, int w, int fracbase));
extern void get_something P_((Widget w, char *resource, XtArgVal value));
extern void get_xmstring P_((Widget w, char *resource, char **txtp));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void prompt_map_cb P_((Widget w, XtPointer client, XtPointer call));
extern void query P_((Widget tw, char *msg, char *label1, char *label2,
    char *label3, void (*func1)(), void (*func2)(), void (*func3)()));
extern void register_selection P_((char *name));
extern void riset_cir P_((Now *np, Obj *op, double dis, RiseSet *rp));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void timestamp P_((Now *np, Widget w));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *msg, int app_modal));

/* possible values for ColHdr.type. */
typedef enum {
    MISC_COL, RISET_COL, SEP_COL
} ColType;

void dm_create_shell P_((void));
void dm_setup_cb P_((Widget w, XtPointer client, XtPointer call));
void dm_separation P_((Obj *p, Obj *q, double *sp));

static void dm_set_buttons P_((int whether));
static void dm_create_table P_((Widget parent));
static void dm_activate_cb P_((Widget w, XtPointer client, XtPointer call));
static void dm_popdown_cb P_((Widget w, XtPointer client, XtPointer call));
static void dm_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void dm_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void dm_compute P_((int r, int force, Now *np));
static void dm_format P_((Now *np, Obj *op, RiseSet *rp, int c, Widget w));
static void dm_settags P_((void));
static void dm_showtim P_((Now *np, Widget w, double t));
static void dm_rs_hrsup P_((Now *np, Obj *op, Widget w, RiseSet *rp));
static void show_constellation P_((Now *np, Obj *op, Widget w));
static void ds_create_selection P_((Widget parent));
static void ds_setup_row_selections P_((void));
static void ds_setup_col_selections P_((ColType type));
static void ds_apply_selections P_((void));
static void ds_ctl_cb P_((Widget w, XtPointer client, XtPointer call));
static void ds_help P_((void));
static void ds_row_toggle_cb P_((Widget w, XtPointer client, XtPointer call));
static void ds_col_toggle_cb P_((Widget w, XtPointer client, XtPointer call));
static void ds_row_all_cb P_((Widget w, XtPointer client, XtPointer call));
static void ds_col_all_cb P_((Widget w, XtPointer client, XtPointer call));
static void ds_row_reset_cb P_((Widget w, XtPointer client, XtPointer call));
static void ds_col_reset_cb P_((Widget w, XtPointer client, XtPointer call));

static void dm_create_flist_w P_((void));
static void dm_flistok_cb P_((Widget w, XtPointer client, XtPointer call));
static void dm_flist_cb P_((Widget w, XtPointer client, XtPointer call));
static void flistok_append_cb P_((void));
static void flistok_overwrite_cb P_((void));
static void make_flist P_((char *name, char *how));
static void dm_list_tofile P_((FILE *fp));
static void dm_list_get P_((char buf[]));

typedef struct {
    int dbidx;		/* db index of object on this row */
    char *tip;		/* widget tip text */
    int on;		/* whether this row is currently to be on */
    Widget lw;		/* label widget for this row's header */
    Widget sw;		/* pushbutton widget for this row in selection menu */
} RowHdr;

static RowHdr row[NOBJ] = {
    {SUN,     "Display one row for the Sun"},
    {MOON,    "Display one row for the Moon"},
    {MERCURY, "Display one row for Mercury"},
    {VENUS,   "Display one row for Venus"},
    {MARS,    "Display one row for Mars"},
    {JUPITER, "Display one row for Jupiter"},
    {SATURN,  "Display one row for Saturn"},
    {URANUS,  "Display one row for Uranus"},
    {NEPTUNE, "Display one row for Neptune"},
    {PLUTO,   "Display one row for Pluto"},
    {OBJX,    "Display one row for this object"},
    {OBJY,    "Display one row for this object"},
    {OBJZ,    "Display one row for this object"},
};

typedef struct {
    ColType type;	/* general class of this column */
    char *name;		/* name of column, unless SEP_COL then use db_name */
    int dbidx;		/* if type == SEP_COL, db index of cross object */
    char *tip;		/* widget tip text, or NULL */
    int on;		/* whether this column is currently to be on */
    Widget rcw;		/* RowColumn widget for this column */
    Widget lw;		/* label widget for this column's header */
    Widget sw;		/* pushbutton widget for this col in selection menu */
} ColHdr;

/* tags for the various Data Selection control panel buttons */
typedef enum {
    OK, APPLY, CANCEL, HELP
} DSCtrls;

/* column details.
 * N.B. these must match the DMCol order
 */
static ColHdr col[] = {
    {MISC_COL,	"Cns",	  0, "Constellation"},
    {MISC_COL,	"RA",	  0, "Right Ascension (to Main's settings)"},
    {MISC_COL,	"HA",	  0, "Hour Angle"},
    {MISC_COL,	"Dec",	  0, "Declination (to Main's settings)"},
    {MISC_COL,	"Az",	  0, "Azimuth, E of N"},
    {MISC_COL,	"Alt",	  0, "Angle above horizon"},
    {MISC_COL,	"Zenith", 0, "Angle down from zenith"},
    {MISC_COL,	"Air",	  0, "Air mass"},
    {MISC_COL,	"Size",	  0, "Angular diameter, arc seconds"},
    {MISC_COL,	"VMag",	  0, "Apparent magnitude"},
    {MISC_COL,	"Phase",  0, "Percent illumination seen from Earth"},
    {MISC_COL,	"HeLat",  0, "Heliocentric latitude"},
    {MISC_COL,	"HeLong", 0, "Heliocentric longitude"},
    {MISC_COL,	"GLat",	  0, "Galactic latitude"},
    {MISC_COL,	"GLong",  0, "Galactic longitude"},
    {MISC_COL,	"EcLat",  0, "Geocentric Ecliptic latitude"},
    {MISC_COL,	"EcLong", 0, "Geocentric Ecliptic longitude"},
    {MISC_COL,	"EaDst",  0, "Distance from Earth, AU (moon is km)"},
    {MISC_COL,	"EaLght", 0, "Light travel time from Earth"},
    {MISC_COL,	"SnDst",  0, "Distance from Sun, AU"},
    {MISC_COL,	"SnLght", 0, "Light travel time from Sun"},
    {MISC_COL,	"Elong",  0, "Elongation: angular degrees from Sun, +E"},
    {RISET_COL,	"RisTm",  0, "Rise time, today"},
    {RISET_COL,	"RisAz",  0, "Rise azimuth, today"},
    {RISET_COL,	"TrnTm",  0, "Transit time, today"},
    {RISET_COL,	"TrnAlt", 0, "Transit altitude, today"},
    {RISET_COL,	"SetTm",  0, "Set time, today"},
    {RISET_COL,	"SetAz",  0, "Set azimuth, today"},
    {RISET_COL,	"HrsUp",  0, "Number of hours object is up, today"},
    {SEP_COL, (char *)0, SUN,     "Angular separation from the Sun"},
    {SEP_COL, (char *)0, MOON,    "Angular separation from the Moon"},
    {SEP_COL, (char *)0, MERCURY, "Angular separation from Mercury"},
    {SEP_COL, (char *)0, VENUS,   "Angular separation from Venus"},
    {SEP_COL, (char *)0, MARS,    "Angular separation from Mars"},
    {SEP_COL, (char *)0, JUPITER, "Angular separation from Jupiter"},
    {SEP_COL, (char *)0, SATURN,  "Angular separation from Saturn"},
    {SEP_COL, (char *)0, URANUS,  "Angular separation from Uranus"},
    {SEP_COL, (char *)0, NEPTUNE, "Angular separation from Neptune"},
    {SEP_COL, (char *)0, PLUTO,   "Angular separation from Pluto"},
    {SEP_COL, (char *)0, OBJX,    "Angular separation from this object"},
    {SEP_COL, (char *)0, OBJY,    "Angular separation from this object"},
    {SEP_COL, (char *)0, OBJZ,    "Angular separation from this object"},
};

#define	NR	XtNumber(row)
#define	NC	XtNumber(col)

static Widget t_w[NR][NC];	/* pushbuttons within table */
static Widget datashell_w;	/* the overall table shell */
static Widget table_w;		/* the overall RowColumn table */
static Widget corner_w;		/* upper left corner of table */
static Widget hdrcol_w;		/* RowColumn for first column */
static Widget sel_w;		/* setup menu */
static Widget dt_w;		/* date/time stamp label widget */

typedef enum {LIMB, CENTER} RSPosOpt;

static Widget adprefr_w;	/* the text holding the adaptive ref horizon */
static Widget limb_w;		/* the Center/Limb toggle button */
static Widget limbl_w;		/* the Limb label on the data table */
static RSPosOpt limb;		/* one of CENTER or LIMB */
static Widget centric_w;	/* the centric label on the data table */
static Widget flist_w;		/* file list dialog widget */

static int dm_selecting;	/* set while our fields are being selected */

/* Save categories */
static char dtcategory[] = "Data Table";
static char dtrowscategory[] = "Data Table -- Rows";
static char dtrscategory[] = "Data Table -- Rise/Set";
static char dtmisccategory[] = "Data Table -- Miscellaneous";
static char dtsepcategory[] = "Data Table -- Separations";

/* called when the data menu is activated via the main menu pulldown.
 * if never called before, create all the widgets.
 */
void
dm_manage ()
{
	if (!datashell_w)
	    dm_create_shell();
	
	dm_update (mm_get_now(), 1);
	dm_set_buttons (dm_selecting);
	XtPopup (datashell_w, XtGrabNone);
	set_something (datashell_w, XmNiconic, (XtArgVal)False);

}

/* user-defined object dbidx has changed.
 * might have a new name, or might be defined or undefined now.
 * must check both the data table and the selection menu.
 * N.B. no need to recompute math -- dm_update() will be called for us.
 * N.B. this is also used by solsys and skyview to set a planet active.
 */
void
dm_newobj(dbidx)
int dbidx;
{
	static char me[] = "dm_newobj()";
	int i, c;

	/* might get called before we have been managed the first time */
	if (!datashell_w)
	    dm_create_shell();

	for (i = 0; i < NR; i++)
	    if (row[i].dbidx == dbidx) {
		Obj *op = db_basic (dbidx);
		if (op->o_type == UNDEFOBJ) {
		    /* it's now undefined so turn off */
		    row[i].on = False;
		    for (c = 0; c < NC; c++)
			XtUnmanageChild (t_w[i][c]);
		    XtUnmanageChild (row[i].lw);
		    XtUnmanageChild (row[i].sw);
		    XmToggleButtonSetState (row[i].sw, False, False);
		} else {
		    f_string (row[i].lw, op->o_name);
		    XtManageChild (row[i].sw);
		    f_string (row[i].sw, op->o_name);

		    /* as a nice gesture, turn the row on */
		    if (!row[i].on) {
			XmToggleButtonSetState (row[i].sw, True, False);
			ds_apply_selections();
		    }
		}
		break;
	    }
	if (i == NR) {
	    printf ("Bug: %s: dbidx not in row[]: 0x%x\n", me, dbidx);
	    exit (1);
	}

	for (i = 0; i < NC; i++)
	    if (col[i].type == SEP_COL && col[i].dbidx == dbidx) {
		Obj *op = db_basic (dbidx);
		if (op->o_type == UNDEFOBJ) {
		    /* it's now undefined so turn off */
		    col[i].on = False;
		    XtUnmanageChild (col[i].rcw);
		    XtUnmanageChild (col[i].sw);
		    XmToggleButtonSetState (col[i].sw, False, False);
		} else {
		    f_string (col[i].lw, op->o_name);
		    XtManageChild (col[i].sw);
		    f_string (col[i].sw, op->o_name);
		}
		break;
	    }
	if (i == NC) {
	    printf ("Bug: %s: dbidx not in col[]: 0x%x\n", me, dbidx);
	    exit (1);
	}

#if 0
	/* come on up and show the new user obj */
	if (!datashell_w)
	    dm_manage ();
#endif
}

/* called to recompute and fill in values for the data menu.
 * don't bother if it doesn't exist or is unmanaged now or no one is logging.
 */
void
dm_update (np, how_much)
Now *np;
int how_much;
{
	int i;

	if (!datashell_w)
	    return;
	if (!isUp(datashell_w) && !any_ison() && !how_much)
	    return;

	watch_cursor (1);

	/* update each row that is on */
	for (i = 0; i < NR; i++)
	    if (row[i].on)
		dm_compute (i, how_much, np);

	/* update the indicators */
	dm_settags();

	/* update the datestamp */
	timestamp (np, dt_w);

	watch_cursor (0);
}

/* called by other menus as they want to hear from our buttons or not.
 * the "on"s and "off"s stack - only really redo the buttons if it's the
 * first on or the last off.
 */
void
dm_selection_mode (whether)
int whether;	/* whether setting up for plotting or for not plotting */
{
	if (whether)
	    dm_selecting++;
	else if (dm_selecting > 0)
	    --dm_selecting;

	if (datashell_w)
	    if ((whether && dm_selecting == 1)     /* first one to want on */
		|| (!whether && dm_selecting == 0) /* last one to want off */)
		dm_set_buttons (whether);
}

/* return the user's horizon displacement, in rads.
 * + means less sky.
 */
static double
dm_hzndep()
{
	char *str = XmTextFieldGetString (adprefr_w);
	double dis = degrad(atod(str));

	XtFree (str);
	return (dis);
}

/* given the Now and Obj, fill in the RiseSet.
 * this takes into account the options currently in effect with the Data menu.
 */
void
dm_riset (np, op, rsp)
Now *np;
Obj *op;
RiseSet *rsp;
{
	double dis;	/* rads apparent horizon is above true */

	/* might get called before we have been managed the first time */
	if (!datashell_w)
	    dm_create_shell();

	/* user's displacement correction */
	dis = -dm_hzndep();

	/* add semi-diameter if want wrt limb */
        if (limb == LIMB)
	    dis += degrad(op->s_size/3600./2.0);

	riset_cir (np, op, dis, rsp);
}

/* called to put up or remove the watch cursor.  */
void
dm_cursor (c)
Cursor c;
{
	Window win;

	if (datashell_w && (win = XtWindow(datashell_w)) != 0) {
	    Display *dsp = XtDisplay(datashell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}

	if (sel_w && (win = XtWindow(sel_w)) != 0) {
	    Display *dsp = XtDisplay(sel_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* given a DMCol, fill str[] with its column heading.
 * return 0 if column is on, else -1
 */
int
dm_colHeader (c, str)
DMCol c;
char str[];
{
	if (!col[c].on)
	    return (-1);
	if (c >= SEP_SUN_ID && c <= SEP_OBJZ_ID) {
	    Obj *op = db_basic (col[c].dbidx);
	    strcpy (str, op->o_type == UNDEFOBJ ? "??" : op->o_name);
	} else {
	    strcpy (str, col[c].name);
	}
	return (0);
}

/* given circumstances and DMCol, fill str[] with its string value.
 * return 0 if column is on, else -1 (but always fill in str).
 */
int
dm_colFormat (np, op, rp, c, str)
Now *np;
Obj *op;
RiseSet *rp;
DMCol c;
char str[];
{
	static Widget w;
	char *lstr;

	/* we get the value round-about via a widget because dm_format() can
	 * not be written to return a string (because it must use the funcs
	 * that call field_log())
	 */
	if (!w)
	    w = XmCreateLabel (toplevel_w, "DMFT", NULL, 0);
	dm_format (np, op, rp, c, w);
	get_xmstring (w, XmNlabelString, &lstr);
	(void) strcpy (str, lstr);
	XtFree (lstr);

	return (col[c].on ? 0 : -1);
}

/* N.B. might be called early if preloading certain .edb files
 */
void
dm_create_shell()
{
	Widget ctlrc_w, w;
	Widget mb_w, cb_w, pd_w;
	Widget dform_w;
	Arg args[20];
	int n;

	/* benign if called > 1 */
	if (datashell_w)
	    return;

	/* create the shell and main form */
	n = 0;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle, "xephem Data Table"); n++;
	XtSetArg (args[n], XmNiconName, "Data"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	datashell_w = XtCreatePopupShell ("Data", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (datashell_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (datashell_w, XmNpopdownCallback, dm_popdown_cb, 0);
	sr_reg (datashell_w, "XEphem*Data.x", dtcategory, 0);
	sr_reg (datashell_w, "XEphem*Data.y", dtcategory, 0);

	n = 0;
	dform_w = XmCreateForm (datashell_w, "DTForm", args, n);
	XtAddCallback (dform_w, XmNhelpCallback, dm_help_cb, 0);
	XtManageChild (dform_w);

	/* make the menubar */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	mb_w = XmCreateMenuBar (dform_w, "MB", args, n);
	XtManageChild (mb_w);

	    /* make the Control pulldown */

	    n = 0;
	    pd_w = XmCreatePulldownMenu (mb_w, "CPD", args, n);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pd_w); n++;
	    XtSetArg (args[n], XmNmnemonic, 'C'); n++;
	    cb_w = XmCreateCascadeButton (mb_w, "Control", args, n);
	    XtManageChild (cb_w);

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Setup", args, n);
	    XtAddCallback (w, XmNactivateCallback, dm_setup_cb, 0);
	    set_xmstring (w, XmNlabelString, "Setup...");
	    wtip (w, "Define desired rows and columns for the table");
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (pd_w, "List", args, n);
	    XtAddCallback (w, XmNactivateCallback, dm_flist_cb, 0);
	    set_xmstring (w, XmNlabelString, "List...");
	    wtip (w, "Save current table to a file");
	    XtManageChild (w);

	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, dm_close_cb, 0);
	    wtip (w, "Close this and all supporting dialogs");
	    XtManageChild (w);

	    /* make the Help pulldown */

	    n = 0;
	    pd_w = XmCreatePulldownMenu (mb_w, "HPD", args, n);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pd_w); n++;
	    XtSetArg (args[n], XmNmnemonic, 'H'); n++;
	    cb_w = XmCreateCascadeButton (mb_w, "Help", args, n);
	    XtManageChild (cb_w);
	    set_something (mb_w, XmNmenuHelpWidget, (XtArgVal)cb_w);

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Help", args, n);
	    XtAddCallback (w, XmNactivateCallback, dm_help_cb, 0);
	    XtManageChild (w);

	/* make a rowcolumn for the bottom control panel */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	ctlrc_w = XmCreateRowColumn (dform_w, "DataTblRC", args, n);
	XtManageChild (ctlrc_w);

	    /* make the limb and centric indicators in frames.
	     * turn them on and off by managing the frames -- but not yet!
	     */

	    n = 0;
	    w = XmCreateFrame (ctlrc_w, "DLimblF", args, n);
	    n = 0;
	    limbl_w = XmCreateLabel (w, "DLimblL", args, n);
	    wtip (limbl_w, "Indicates whether Rise/Set data are for Limb or Center");
	    XtManageChild (limbl_w);

	    n = 0;
	    w = XmCreateFrame (ctlrc_w, "DCentricF", args, n);
	    n = 0;
	    centric_w = XmCreateLabel (w, "DCentricL", args, n);
	    wtip (centric_w, "Indicates Geocentric or Topocentric vantage, and equinox");
	    XtManageChild (centric_w);

	    /* make a label for the date/time stamp */

	    n = 0;
	    dt_w = XmCreateLabel (ctlrc_w, "DateStamp", args, n);
	    wtip (dt_w, "Date and Time for which data are computed");
	    timestamp (mm_get_now(), dt_w);	/* sets initial size correctly*/
	    XtManageChild (dt_w);

	/* create the table */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, mb_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, ctlrc_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	table_w = XmCreateRowColumn (dform_w, "DataTable", args, n);
	XtManageChild (table_w);

	dm_create_table (table_w);

	/* create the selection dialog.
	 * don't manage it yet but its state info is used right off.
	 */
	ds_create_selection(datashell_w);
	ds_apply_selections();
}

/* callback to bring up the Data setup dialog */
/* ARGSUSED */
void
dm_setup_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!sel_w)
	    dm_create_shell();

	if (!XtIsManaged(sel_w)) {
	    watch_cursor(1);
	    ds_setup_row_selections();
	    ds_setup_col_selections(MISC_COL);
	    ds_setup_col_selections(RISET_COL);
	    ds_setup_col_selections(SEP_COL);
	    XtManageChild (sel_w);
	    watch_cursor(0);
	}
}

/* go through all the buttons and set whether they
 * should appear to look like buttons or just flat labels.
 */
static void
dm_set_buttons (whether)
int whether;	/* whether setting up for plotting or for not plotting */
{
	int r, c;
	int i;

	/* do these always too in case user is trying a new color */
	buttonAsButton (corner_w, 0);
	for (i = 0; i < NR; i++)
	    buttonAsButton (row[i].lw, 0);
	for (i = 0; i < NC; i++)
	    buttonAsButton (col[i].lw, 0);

	for (r = 0; r < NR; r++)
	    for (c = 0; c < NC; c++)
		if (t_w[r][c])
		    buttonAsButton (t_w[r][c], whether);
}

/* create the main data table - everything but the first column is unmanaged.
 */
static void
dm_create_table(parent)
Widget parent;	/* overall RowColumn */
{
	Arg args[20];
	XmString str;
	Widget w;
	int r, c;
	int n;

	/* first column is the row headers.
	 * it's always managed so init what rows we can too.
	 */
	n = 0;
	hdrcol_w = XmCreateRowColumn (parent, "DataHdrC", args, n);
	XtManageChild (hdrcol_w);

	    /* first row is a dummy */
	    n = 0;
	    str = XmStringCreateLtoR (" ", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    corner_w = XmCreatePushButton (hdrcol_w, "DataCorner", args, n);
	    XmStringFree (str);
	    XtManageChild (corner_w);

	    /* remaining rows are per object */
	    for (r = 0; r < NR; r++) {
		Obj *op = db_basic (row[r].dbidx);
		n = 0;
		if (op->o_type != UNDEFOBJ)
		    w = XmCreatePushButton (hdrcol_w, op->o_name, args, n);
		else
		    w = XmCreatePushButton (hdrcol_w, "DRow", args, n);
		row[r].lw = w;

	    }

	/* remaining columns.
	 * don't manage any but set names of what we can now too.
	 */
	for (c = 0; c < NC; c++) {
	    Widget rcw;

	    n = 0;
	    XtSetArg (args[n], XmNadjustMargin, False); n++;
	    XtSetArg (args[n], XmNisAligned, False); n++;
	    rcw = col[c].rcw = XmCreateRowColumn (parent, "DataCol", args, n);

	    /* first row is column header */
	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    if (col[c].type != SEP_COL)
		w = XmCreatePushButton (rcw, col[c].name, args, n);
	    else {
		Obj *op = db_basic (col[c].dbidx);
		if (op->o_type != UNDEFOBJ)
		    w = XmCreatePushButton (rcw, op->o_name, args, n);
		else
		    w = XmCreatePushButton (rcw, "DCHdr", args, n);
	    }
	    col[c].lw = w;
	    if (col[c].tip)
		wtip (w, col[c].tip);
	    XtManageChild (w);

	    /* remaining rows are per object */
	    for (r = 0; r < NR; r++) {
		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		w = t_w[r][c] = XmCreatePushButton (rcw, "DataPB", args, n);
		XtAddCallback(w, XmNactivateCallback, dm_activate_cb,
					(XtPointer)((c<<8)|r));
	    }
	}
}

/* callback from any of the data menu buttons being activated.
 * do nothing unless we are being used to set up a selection collection.
 * if the latter, make a name for our field, put it in UserData and inform
 * all interested parties.
 * to form the name, client data is (col<<8)|row
 * N.B. we assume we can't be called if our row and column are not on.
 */
static void
/* ARGSUSED */
dm_activate_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char me[] = "dm_activate_cb()";

	if (dm_selecting) {
	    int r = (unsigned)client & 0xff;
	    int c = (unsigned)client >> 8;
	    char *name, *rname, *cname;
	    Obj *op;
	    char *userD;   /* Heller, pg 852, say's this is type Pointer?? */
	    int len;

	    /* figure out our row name */
	    if (!row[r].on) {
		printf ("Bug: %s: row[%d] not on\n", me, r);
		exit (1);
	    }
	    op = db_basic (row[r].dbidx);
	    rname = op->o_name;
	    len = strlen (rname);

	    /* figure out our col name */
	    if (!col[c].on) {
		printf ("Bug: %s: col[%d] not on\n", me, c);
		exit (1);
	    }
	    if (col[c].type == SEP_COL) {
		op = db_basic (col[c].dbidx);
		cname = op->o_name;
	    } else
		cname = col[c].name;
	    len += strlen(cname);

	    name = XtMalloc (len + 2);	/* '.' plus '\0' */
	    (void) sprintf (name, "%s.%s", rname, cname);

	    /* set XmNuserData to be the name we want to go by */
	    get_something (w, XmNuserData, (XtArgVal)&userD);
	    if (userD)
		XtFree (userD);
	    userD = name;
	    set_something (w, XmNuserData, (XtArgVal)userD);

	    /* tell the world our name */
	    register_selection (name);
	}
}

/* callback when main dialog is popped down */
static void
/* ARGSUSED */
dm_popdown_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (sel_w && XtIsManaged (sel_w))
	    XtUnmanageChild (sel_w);
	if (flist_w && XtIsManaged (flist_w))
	    XtUnmanageChild (flist_w);
}

/* callback from the Data table Close button
 */
static void
/* ARGSUSED */
dm_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* let popdown do the real work */
	XtPopdown (datashell_w);
}

/* callback from the Data table Help button
 */
static void
/* ARGSUSED */
dm_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg[] = {
"This table displays various information about the planets and objects.",
"To reduce computation and save screen space, each row and column may be",
"individually turned off or on using the Select button."
};

	hlp_dialog ("Data Table", msg, XtNumber(msg));
}

/* compute and print body info in data menu format */
/* ARGSUSED */
static void
dm_compute (r, force, np)
int r;		/* which row */
int force;	/* whether to print for sure or only if things have changed */
Now *np;
{
	RiseSet rs;
	Obj *op;
	int c;
	int did_rs = 0;

	op = db_basic (row[r].dbidx);
	db_update (op);

	for (c = 0; c < NC; c++)
	    if (col[c].on) {
		if (col[c].type == RISET_COL && !did_rs) {
		    dm_riset (np, op, &rs);
		    did_rs = 1;
		}
		dm_format(np, op, &rs, c, t_w[r][c]);
	    }
}

static void
dm_format (np, op, rp, c, w)
Now *np;
Obj *op;
RiseSet *rp;
int c;
Widget w;
{
	static char me[] = "dm_format()";
	double tmp;
	Obj *bop;

	switch (c) {
	case CONSTEL_ID:
	    show_constellation (np, op, w);
	    break;
	case RA_ID:
	    f_ra (w, op->s_ra);
	    break;
	case HA_ID:
	    radec2ha (np, op->s_ra, op->s_dec, &tmp);
	    tmp = radhr (tmp);
	    if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		f_sexa (w, tmp, 3, 600);
	    else
		f_sexa (w, tmp, 3, 360000);
	    break;
	case DEC_ID:
	    f_prdec (w, op->s_dec);
	    break;
	case AZ_ID:
	    f_pangle (w, op->s_az);
	    break;
	case ALT_ID:
	    f_pangle (w, op->s_alt);
	    break;
	case ZEN_ID:
	    f_pangle (w, PI/2 - op->s_alt);
	    break;
	case Z_ID: /* airmass */
	    airmass (op->s_alt, &tmp);
	    f_double (w, "%5.2f", tmp);
	    break;
	case HLONG_ID:
	    if (is_ssobj(op))
		f_pangle (w, op->s_hlong);
	    else {
		if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		    f_string (w, "      ");
		else
		    f_string (w, "         ");
	    }
	    break;
	case HLAT_ID:
	    if (is_ssobj(op))
		f_pangle (w, op->s_hlat);
	    else {
		if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		    f_string (w, "      ");
		else
		    f_string (w, "         ");
	    }
	    break;
	case GLONG_ID: {
	    double glat, glng;
	    double e = epoch == EOD ? mjd : epoch;
	    eq_gal (e, op->s_ra, op->s_dec, &glat, &glng);
	    f_pangle (w, glng);
	    }
	    break;
	case GLAT_ID: {
	    double glat, glng;
	    double e = epoch == EOD ? mjd : epoch;
	    eq_gal (e, op->s_ra, op->s_dec, &glat, &glng);
	    f_pangle (w, glat);
	    }
	    break;
	case ECLONG_ID: {
	    double eclat, eclng;
	    eq_ecl (mjd, op->s_gaera, op->s_gaedec, &eclat, &eclng);
	    f_pangle (w, eclng);
	    }
	    break;
	case ECLAT_ID: {
	    double eclat, eclng;
	    eq_ecl (mjd, op->s_gaera, op->s_gaedec, &eclat, &eclng);
	    f_pangle (w, eclat);
	    }
	    break;
	case EDST_ID:
	    if (is_planet(op, MOON)) {
		tmp = op->s_edist;
		if (pref_get(PREF_UNITS) == PREF_ENGLISH) {
		    /* s_edist is stored in au, want miles */
		    tmp *= MAU*FTPM/5280.0;
		} else {
		    /* s_edist is stored in au, want km */
		    tmp *= MAU/1000.0;
		}
		f_double (w, "%6.0f", tmp);
	    } else if (is_ssobj(op)) {
		/* show distance in au */
		f_double (w, op->s_edist >= 9.99995 ? "%6.3f" : "%6.4f",
								op->s_edist);
	    } else
		f_string (w, "      ");
	    break;
	case ELGHT_ID:
	    if (is_planet(op,MOON)) {
	        double m = op->s_edist*LTAU;	/* seconds */
		if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		    f_double (w, "%5.3f", m);
		else
		    f_double (w, "% 7.5f", m);	/* 8.6 is ridiculous */
	    } else if (is_ssobj(op)) {
	        double m = (op->s_edist*LTAU)/3600.0;	/* hours */
		if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		    f_mtime (w, m);	/* hh:mm */
		else
		    f_time (w, m);	/* hh:mm:ss */
	    } else {
		if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		    f_string (w, "     ");
		else
		    f_string (w, "        ");
	    }
	    break;
	case SDST_ID:
	    if (is_ssobj(op) && !is_planet(op, SUN))
		f_double (w, op->s_sdist >= 9.99995 ? "%6.3f" : "%6.4f",
								op->s_sdist);
	    else
		f_string (w, "      ");
	    break;
	case SLGHT_ID:
	    if (is_ssobj(op) && !is_planet(op, SUN)) {
	        double m = (op->s_sdist*LTAU)/3600.0;	/* hours */
		if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		    f_mtime (w, m);	/* hh:mm */
		else
		    f_time (w, m);	/* hh:mm:ss */
	    } else {
		if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		    f_string (w, "     ");
		else
		    f_string (w, "        ");
	    }
	    break;
	case ELONG_ID:
	    if (op->o_type != EARTHSAT)
		f_double (w, "%6.1f", op->s_elong);
	    else
		f_string (w, "      ");
	    break;
	case SIZE_ID:
	    if (op->o_type != EARTHSAT) {
		if (op->s_size < 999.5)
		    f_double (w, "%5.1f", op->s_size);
		else
		    f_double (w, "%5.0f", op->s_size);
	    } else
		f_string (w, "     ");
	    break;
	case VMAG_ID:
	    if (op->o_type != EARTHSAT) {
	        double m = get_mag(op);
	        f_double (w, m <= -9.95 ? "%4.0f" : "%4.1f", m);
	    } else
		f_string (w, "    ");
	    break;
	case PHS_ID:
	    if (is_ssobj(op) && !is_planet(op, SUN))
		f_double (w, " %3.0f", op->s_phase);
	    else
		f_string (w, "    ");
	    break;

	case RSTIME_ID:
	    if (rp->rs_flags & RS_ERROR)
		f_string (w, "Error");
	    else if (rp->rs_flags & RS_CIRCUMPOLAR)
		f_string (w, "CirPl");
	    else if (rp->rs_flags & RS_NEVERUP)
		f_string (w, "NvrUp");
	    else if (rp->rs_flags & RS_NORISE)
		f_string (w, "NoRis");
	    else
		dm_showtim (np, w, rp->rs_risetm);	/* 5 chars wide */
	    break;

	case RSAZ_ID:
	    if (rp->rs_flags & RS_ERROR)
		f_string (w, " Error");
	    else if (rp->rs_flags & RS_CIRCUMPOLAR)
		f_string (w, "CirPol");
	    else if (rp->rs_flags & RS_NEVERUP)
		f_string (w, " NvrUp");
	    else if (rp->rs_flags & RS_NORISE)
		f_string (w, "NoRise");
	    else
		f_dm_angle (w, rp->rs_riseaz);	/* 6 chars wide */
	    break;

	case SETTIME_ID:
	    if (rp->rs_flags & RS_ERROR)
		f_string (w, "Error");
	    else if (rp->rs_flags & RS_CIRCUMPOLAR)
		f_string (w, "CirPl");
	    else if (rp->rs_flags & RS_NEVERUP)
		f_string (w, "NvrUp");
	    else if (rp->rs_flags & RS_NOSET)
		f_string (w, "NoSet");
	    else
		dm_showtim (np, w, rp->rs_settm);	/* 5 chars wide */
	    break;

	case SETAZ_ID:
	    if (rp->rs_flags & RS_ERROR)
		f_string (w, " Error");
	    else if (rp->rs_flags & RS_CIRCUMPOLAR)
		f_string (w, "CirPol");
	    else if (rp->rs_flags & RS_NEVERUP)
		f_string (w, " NvrUp");
	    else if (rp->rs_flags & RS_NOSET)
		f_string (w, " NoSet");
	    else
		f_dm_angle (w, rp->rs_setaz);	/* 6 chars wide */
	    break;

	case TRTIME_ID:
	    if (rp->rs_flags & RS_ERROR)
		f_string (w, "Error");
	    else if (rp->rs_flags & RS_NEVERUP)
		f_string (w, "NvrUp");
	    else if (rp->rs_flags & RS_NOTRANS)
		f_string (w, "NoTrn");
	    else
		dm_showtim (np, w, rp->rs_trantm);	/* 5 chars wide */
	    break;

	case TRALT_ID:
	    if (rp->rs_flags & RS_ERROR)
		f_string (w, " Error");
	    else if (rp->rs_flags & RS_NEVERUP)
		f_string (w, " NvrUp");
	    else if (rp->rs_flags & RS_NOTRANS)
		f_string (w, "NoTran");
	    else {
		f_dm_angle (w, rp->rs_tranalt);	/* 6 chars wide */
	    }
	    break;

	case HRSUP_ID:
	    dm_rs_hrsup (np, op, w, rp);
	    break;

	default:
	    /* these are effectively all the separation columns */
	    if (c < 0 || c >= NC) {
		printf ("Bug: %s: c = %d but max = %d\n", me, c, NC-1);
		exit (1);
	    }
	    if (col[c].type != SEP_COL) {
		printf ("Bug: %s: col[%d].type = 0x%x\n", me, c, col[c].type);
		exit (1);
	    }
	    if (op != (bop= db_basic(col[c].dbidx)) && bop->o_type != UNDEFOBJ){
		double sep;
		db_update(bop);
		dm_separation (op, bop, &sep);
		f_pangle (w, sep);
	    } else {
		if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		    f_string (w, "      ");
		else
		    f_string (w, "         ");
	    }
	    break;
	}
}

/* setup the limb and centric tag labels according to the current options */
static void
dm_settags()
{
	char str[1024], estr[64];
	Now *np = mm_get_now();

	if (epoch == EOD)
	    (void) strcpy (estr, "EOD");
	else {
	    double y;
	    mjd_year (epoch, &y);
	    (void) sprintf (estr, "%.1f", y);
	}
	(void) sprintf (str, "Equ: %s %s",
		    pref_get(PREF_EQUATORIAL) == PREF_GEO ? "Geo":"Topo", estr);
	f_showit (centric_w, str);

	limb = XmToggleButtonGetState (limb_w) ? LIMB : CENTER;
	f_showit (limbl_w, limb == LIMB ? "Limb" : "Center");
}

/* display the rise/set/transit mjd time t as hours in widget w.
 * convert tm to local time if tzpref && PREF_ZONE == PREF_LOCALTZ.
 */
static void
dm_showtim (np, w, t)
Now *np;
Widget w;
double t;
{
	if (pref_get(PREF_ZONE) == PREF_LOCALTZ)
	    t -= tz/24.0;

	f_mtime (w, mjd_hr(t));			/* 5 chars: "hh:mm" */
}

/* display the total hours this object has been up.
 * N.B. insure string length is always 5 chars wide.
 * N.B. Earth satellites are up from rise to set, period, and either or both
 *   of these may be tomorrow if they are later than the current time because
 *   we scan for earth sat rise/set events up to 24 hours in the future
 *   regardless of whether it crosses over into tomorrow. Also, since we can't
 *   assume a nominal diurnal period who's to say how much it's up if all we
 *   have is a rise time that is later than a set time,
 */
/* ARGSUSED */
static void
dm_rs_hrsup (np, op, w, rp)
Now *np;
Obj *op;
Widget w;
RiseSet *rp;
{
	double r, s, up;

	/* first some special cases */
	if (rp->rs_flags & RS_ERROR) {
	    f_string (w, "Error ");
	    return;
	}
	if (rp->rs_flags & RS_CIRCUMPOLAR) {
	    f_double (w, "%3.0f:00", 24.0); /* f_mtime() changes to 00:00 */
	    return;
	}
	if (rp->rs_flags & RS_NEVERUP) {
	    f_mtime (w, 0.0);
	    return;
	}
	if (rp->rs_flags & (RS_NORISE|RS_NOSET)) {
	    f_string (w, "      ");
	    return;
	}

	r = rp->rs_risetm;
	s = rp->rs_settm;
	up = s - r;
	 
	if (up < 0) {
	    /* we assume diurnal motion except for fast satellites */
	    if (op->o_type == EARTHSAT && op->es_n > FAST_SAT_RPD) {
		f_string (w, "      ");
		return;
	    }
	    up += 1.0;
	}

	f_mtime (w, up*24.0);			/* 5 chars: "hh:mm" */
}

static void
show_constellation (np, op, w)
Now *np;
Obj *op;
Widget w;
{
	char nm[10], *name;
	int id;

        id = cns_pick (op->s_ra, op->s_dec, epoch == EOD ? mjd : epoch);
	name = cns_name (id);
	(void) sprintf (nm, "%.3s", name);
	f_string(w, nm);
}

/* compute the separation between the two sky locations */
void
dm_separation (p, q, sp)
Obj *p, *q;
double *sp;
{
	double spy, cpy, px, qx, sqy, cqy;

	spy = sin (p->s_dec);
	cpy = cos (p->s_dec);
	px = p->s_ra;
	qx = q->s_ra;
	sqy = sin (q->s_dec);
	cqy = cos (q->s_dec);

	*sp = acos(spy*sqy + cpy*cqy*cos(px-qx));
}

/* create the selections dialog */
static void
ds_create_selection(parent)
Widget parent;
{
	static char me[] = "ds_create_selection()";
	static struct { /* info to streamline creation of control buttons */
	    DSCtrls id;
	    int lpos, rpos;
	    char *name;
	    char *tip;
	} ctlbtns[] = {
	    {OK,     1,   3, "Ok",    "Apply new settings and close"},
	    {APPLY,  4,   6, "Apply", "Apply new settings and stay up"},
	    {CANCEL, 7,   9, "Close", "Make no changes; just close"},
	    {HELP,   10, 12, "Help",  "More detailed descriptions"}
	};
	Arg args[20];
	XmString str;
	Widget rl_w, cl_w, arc_w;
	Widget rf_w, mf_w, rsf_w, sf_w, ctlf_w, rowrc_w, mrc_w, rsrc_w, src_w;
	Widget w;
	Widget rb_w;
	int n;
	int i;

	/* create form */
	n = 0;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNallowOverlap, False); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	sel_w = XmCreateFormDialog (parent, "DataSelection", args, n);
	set_something (sel_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (sel_w, XmNmapCallback, prompt_map_cb, NULL);
	XtAddCallback (sel_w, XmNhelpCallback, ds_ctl_cb, (XtPointer)HELP);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Data Table setup"); n++;
	XtSetValues (XtParent(sel_w), args, n);

	/* make a form for bottom control panel */
	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNfractionBase, 13); n++;
	ctlf_w = XmCreateForm (sel_w, "DataSelF", args, n);
	XtManageChild (ctlf_w);

	    /* make the control buttons */

	    for (i = 0; i < XtNumber(ctlbtns); i++) {
		n = 0;
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNtopOffset, 5); n++;
		XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNbottomOffset, 5); n++;
		XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
		XtSetArg (args[n], XmNleftPosition, ctlbtns[i].lpos); n++;
		XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
		XtSetArg (args[n], XmNrightPosition, ctlbtns[i].rpos); n++;
		w = XmCreatePushButton (ctlf_w, ctlbtns[i].name, args, n);
		XtAddCallback (w, XmNactivateCallback, ds_ctl_cb,
						    (XtPointer)ctlbtns[i].id);
		wtip (w, ctlbtns[i].tip);
		XtManageChild (w);
	    }

	/* make the Rows heading */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNmarginHeight, 4); n++;
	rl_w = XmCreateLabel (sel_w, "Rows:", args, n);
	XtManageChild (rl_w);

	/* make the row selection rc in a frame */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rl_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, ctlf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	rf_w = XmCreateFrame (sel_w, "DSRFrame", args, n);
	XtManageChild (rf_w);

	n = 0;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	rowrc_w = XmCreateRowColumn (rf_w, "DataSelRows", args, n);
	XtManageChild (rowrc_w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreateLabel (rowrc_w, "L", args, n);
	    set_xmstring (w, XmNlabelString, "Objects");
	    XtManageChild (w);

	    /* fill up with buttons for each possible row.
	     * fill in name if it's a planet since that won't change.
	     */

	    /* make the "Toggle" push button */

	    str = XmStringCreate("Toggle", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (rowrc_w, "DSRToggle", args, n);
	    XtAddCallback (w, XmNactivateCallback, ds_row_toggle_cb, 0);
	    wtip (w, "Exchange on and off items in this column");
	    XtManageChild (w);
	    XmStringFree (str);

	    /* make the "All" push button */

	    str = XmStringCreate("All", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (rowrc_w, "DSRAll", args, n);
	    XtAddCallback (w, XmNactivateCallback, ds_row_all_cb, 0);
	    XtManageChild (w);
	    wtip (w, "Turn all items in this row on");
	    XmStringFree (str);

	    /* make the "Reset" push button */

	    str = XmStringCreate("Reset", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (rowrc_w, "DSRReset", args, n);
	    XtAddCallback (w, XmNactivateCallback, ds_row_reset_cb, 0);
	    XtManageChild (w);
	    wtip (w, "Return each item in this column to its original state");
	    XmStringFree (str);

	    /* make 1 PB for each row */
	    for (i = 0; i < NR; i++) {
		Obj *op = db_basic(row[i].dbidx);
		char *name = op->o_type != UNDEFOBJ ? op->o_name : "RowSelObj";

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		w = row[i].sw = XmCreateToggleButton(rowrc_w, name, args, n);

		if (op->o_type != UNDEFOBJ)
		    XtManageChild (w);
		if (row[i].dbidx <= MOON)
		    sr_reg (w, NULL, dtrowscategory, 1);
		if (row[i].tip)
		    wtip (w, row[i].tip);
	    }

	/* make the Columns heading */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, rf_w); n++;
	XtSetArg (args[n], XmNleftOffset, 10); n++;
	XtSetArg (args[n], XmNmarginHeight, 4); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	cl_w = XmCreateLabel (sel_w, "Columns:", args, n);
	XtManageChild (cl_w);

	/* make the misc col selection rc in a frame */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, cl_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, ctlf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, rf_w); n++;
	XtSetArg (args[n], XmNleftOffset, 10); n++;
	mf_w = XmCreateFrame (sel_w, "DSMCFrame", args, n);
	XtManageChild (mf_w);

	n = 0;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	arc_w = XmCreateRowColumn (mf_w, "DataSelMiscCols", args, n);
	XtManageChild (arc_w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreateLabel (arc_w, "L", args, n);
	    set_xmstring (w, XmNlabelString, "Miscellaneous");
	    XtManageChild (w);

	    /* fill up with buttons for each possible col in misc range.
	     * set those columns names that are stable.
	     */

	    /* make the "Toggle" push button */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (arc_w, "Toggle", args, n);
	    XtAddCallback (w, XmNactivateCallback, ds_col_toggle_cb,
							(XtPointer)MISC_COL);
	    wtip (w, "Exchange on and off items in this column");
	    XtManageChild (w);

	    /* make the "All" push button */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (arc_w, "All", args, n);
	    XtAddCallback (w, XmNactivateCallback, ds_col_all_cb,
							(XtPointer)MISC_COL);
	    wtip (w, "Turn all items in this row on");
	    XtManageChild (w);

	    /* make the "Reset" push button */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (arc_w, "Reset", args, n);
	    XtAddCallback (w, XmNactivateCallback, ds_col_reset_cb,
							(XtPointer)MISC_COL);
	    wtip (w, "Return each item in this column to its original state");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	    XtSetArg (args[n], XmNadjustMargin, False); n++;
	    XtSetArg (args[n], XmNisAligned, False); n++;
	    XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	    XtSetArg (args[n], XmNnumColumns, 2); n++;
	    mrc_w = XmCreateRowColumn (arc_w, "DataSelMiscCols", args, n);
	    XtManageChild (mrc_w);

	/* make the rise/set col selection rc in a frame */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, cl_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, ctlf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, mf_w); n++;
	rsf_w = XmCreateFrame (sel_w, "DSRCFrame", args, n);
	XtManageChild (rsf_w);

	n = 0;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	rsrc_w = XmCreateRowColumn (rsf_w, "DataSelRisetCols", args, n);
	XtManageChild (rsrc_w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreateLabel (rsrc_w, "L", args, n);
	    set_xmstring (w, XmNlabelString, "Rise/Set");
	    XtManageChild (w);

	    /* fill up with buttons for each possible col in rise/set range.
	     * set those columns names that are stable.
	     */

	    /* make the "Toggle" push button */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (rsrc_w, "Toggle", args, n);
	    XtAddCallback (w, XmNactivateCallback, ds_col_toggle_cb,
							(XtPointer)RISET_COL);
	    wtip (w, "Exchange on and off items in this column");
	    XtManageChild (w);

	    /* make the "All" push button */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (rsrc_w, "All", args, n);
	    XtAddCallback (w, XmNactivateCallback, ds_col_all_cb,
							(XtPointer)RISET_COL);
	    wtip (w, "Turn all items in this row on");
	    XtManageChild (w);

	    /* make the "Reset" push button */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (rsrc_w, "Reset", args, n);
	    XtAddCallback (w, XmNactivateCallback, ds_col_reset_cb,
							(XtPointer)RISET_COL);
	    wtip (w, "Return each item in this column to its original state");
	    XtManageChild (w);

	    /* make the displacement prompt */

	    n = 0;
	    XtSetArg (args[n], XmNcolumns, 5); n++;
	    adprefr_w = XmCreateTextField (rsrc_w, "AdpDispl", args, n);
	    wtip (adprefr_w, "Degrees the local horizon is above horizontal");
	    XtManageChild (adprefr_w);

	    /* make the CENTER/LIMB radio box */

	    n = 0;
	    w = XmCreateFrame (rsrc_w, "DSLimbF", args, n);
	    XtManageChild (w);
	    n = 0;
	    rb_w = XmCreateRadioBox (w, "DSLimbRB", args, n);
	    XtManageChild (rb_w);

		n = 0;
		limb_w = XmCreateToggleButton (rb_w, "Limb", args, n);
		wtip (limb_w, "Compute Rise/Set events with respect to upper limb");
		sr_reg (limb_w, NULL, dtrscategory, 1);
		XtManageChild (limb_w);

		n = 0;
		XtSetArg(args[n], XmNset, !XmToggleButtonGetState(limb_w)); n++;
		w = XmCreateToggleButton (rb_w, "Center", args, n);
		wtip (w, "Compute Rise/Set events with respect to body center");
		XtManageChild (w);

	/* make the separations col selection rc in a frame */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, cl_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, ctlf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, rsf_w); n++;
	sf_w = XmCreateFrame (sel_w, "DSRCFrame", args, n);
	XtManageChild (sf_w);

	n = 0;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	src_w = XmCreateRowColumn (sf_w, "DataSelSepCols", args, n);
	XtManageChild (src_w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreateLabel (src_w, "L", args, n);
	    set_xmstring (w, XmNlabelString, "Separations");
	    XtManageChild (w);

	    /* fill up with buttons for each possible col in sep range.
	     * set those columns names that are stable.
	     */

	    /* make the "Toggle" push button */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (src_w, "Toggle", args, n);
	    XtAddCallback (w, XmNactivateCallback, ds_col_toggle_cb,
							(XtPointer)SEP_COL);
	    wtip (w, "Exchange on and off items in this column");
	    XtManageChild (w);

	    /* make the "All" push button */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (src_w, "All", args, n);
	    XtAddCallback (w, XmNactivateCallback, ds_col_all_cb,
							(XtPointer)SEP_COL);
	    wtip (w, "Turn all items in this row on");
	    XtManageChild (w);

	    /* make the "Reset" push button */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (src_w, "Reset", args, n);
	    XtAddCallback (w, XmNactivateCallback, ds_col_reset_cb,
							(XtPointer)SEP_COL);
	    wtip (w, "Return each item in this column to its original state");
	    XtManageChild (w);

	/* now fill in the column entries */
	for (i = 0; i < NC; i++) {
	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    switch (col[i].type) {
	    case MISC_COL:
		/* one of the misc columns */
		w = col[i].sw = XmCreateToggleButton(mrc_w, col[i].name,args,n);
		sr_reg (w, NULL, dtmisccategory, 1);
		XtManageChild (w);
		break;
	    case RISET_COL:
		/* one of the rise/set columns */
		w = col[i].sw = XmCreateToggleButton(rsrc_w,col[i].name,args,n);
		sr_reg (w, NULL, dtrscategory, 1);
		XtManageChild (w);
		break;
	    case SEP_COL: {
		/* one of the separation columns */
		Obj *op = db_basic (col[i].dbidx);
		if (op->o_type != UNDEFOBJ) {
		    w = col[i].sw =
				XmCreateToggleButton(src_w,op->o_name,args,n);
		    XtManageChild (w);
		} else
		    w=col[i].sw= XmCreateToggleButton(src_w,"ColSelObj",args,n);
		if (col[i].dbidx <= MOON)
		    sr_reg (w, NULL, dtsepcategory, 1);	/*not ObjXYZ*/
		}
		break;
	    default:
		printf ("Bug: %s: col[%d].type = 0x%x\n", me, i, col[i].type);
		exit (1);
		break;
	    }

	    if (col[i].tip)
		wtip (w, col[i].tip);
	}
}

/* set up the Data selection row menu based on what is currently on and defined.
 */
static void
ds_setup_row_selections()
{
	int i;

	for (i = 0; i < NR; i++) {
	    Widget sw = row[i].sw;
	    Obj *op = db_basic (row[i].dbidx);
	    if (op->o_type == UNDEFOBJ)
		XtUnmanageChild (sw);
	    else 
		XtManageChild (sw);
	    XmToggleButtonSetState (sw, row[i].on, False);
	}
}

/* set up a Data selection col menu based on what is currently on and defined.
 */
static void
ds_setup_col_selections(type)
ColType type;
{
	int i;

	for (i = 0; i < NC; i++) {
	    if (col[i].type == type) {
		Widget sw = col[i].sw;
		if (col[i].type == SEP_COL) {
		    Obj *op = db_basic (col[i].dbidx);
		    if (op->o_type == UNDEFOBJ)
			XtUnmanageChild (sw);
		    else 
			XtManageChild (sw);
		}
		XmToggleButtonSetState (sw, col[i].on, False);
	    }
	}

	switch (type) {
	case RISET_COL:
	    XmToggleButtonSetState (limb_w, limb == LIMB, True);
	    break;
	default:
	    break;
	}
}


/* change the Data table according to what is now defined and set up in the
 * Selection menu.
 * N.B. can be called before we are managed.
 */
static void
ds_apply_selections()
{
	int i, c;
	int n_riset;
	int n_sep;

	watch_cursor(1);

	for (i = 0; i < NR; i++) {
	    int wantset = XmToggleButtonGetState(row[i].sw);
	    if (wantset != row[i].on) {
		if (wantset) {
		    for (c = 0; c < NC; c++)
			XtManageChild (t_w[i][c]);
		    XtManageChild (row[i].lw);
		} else {
		    for (c = 0; c < NC; c++)
			XtUnmanageChild (t_w[i][c]);
		    XtUnmanageChild (row[i].lw);
		}
		row[i].on = wantset;
	    }
	}

	n_sep = n_riset = 0;
	for (i = 0; i < NC; i++) {
	    int wantset = XmToggleButtonGetState(col[i].sw);
	    if (wantset != col[i].on) {
		if (wantset)
		    XtManageChild (col[i].rcw);
		else
		    XtUnmanageChild (col[i].rcw);
		col[i].on = wantset;
	    }
	    if (col[i].on) {
		if (col[i].type == RISET_COL)
		    n_riset++;
		if (col[i].type == SEP_COL)
		    n_sep++;
	    }
	}

	dm_settags();
	if (n_riset)
	    XtManageChild (XtParent(limbl_w));
	else
	    XtUnmanageChild (XtParent(limbl_w));

	if (col[RA_ID].on || col[DEC_ID].on || n_sep > 0)
	    XtManageChild (XtParent(centric_w));
	else
	    XtUnmanageChild (XtParent(centric_w));

	watch_cursor(0);
}

/* callback from any of the Data selection control panel buttons.
 * which is in client.
 */
static void
/* ARGSUSED */
ds_ctl_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	DSCtrls id = (DSCtrls) client;

	switch (id) {
	case OK:
	    ds_apply_selections();
	    dm_update (mm_get_now(), 1);
	    XtUnmanageChild (sel_w);
	    break;
	case APPLY:
	    ds_apply_selections();
	    dm_update (mm_get_now(), 1);
	    break;
	case CANCEL:
	    XtUnmanageChild (sel_w);
	    break;
	case HELP:
	    ds_help();
	    break;
	}
}

/* called from the Data selection table Help button
 */
static void
ds_help ()
{
	static char *msg[] = {
"This table lets you configure the rows and columns of the data table."
};

	hlp_dialog ("DataSelection Table", msg, XtNumber(msg));
}

/* callback from the Data selection row toggle button.
 */
/* ARGSUSED */
static void
ds_row_toggle_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int i;

	for (i = 0; i < NR; i++) {
	    Widget sw = row[i].sw;
	    if (XtIsManaged(sw))
		XmToggleButtonSetState (sw,
		    !XmToggleButtonGetState(sw), False);
	}
}

/* callback from any of the Data selection col toggle button.
 */
static void
/* ARGSUSED */
ds_col_toggle_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int what = (int)client;
	int i;

	for (i = 0; i < NC; i++)
	    if (col[i].type == what) {
		Widget sw = col[i].sw;
		if (XtIsManaged(sw))
		    XmToggleButtonSetState (sw,
			!XmToggleButtonGetState(sw), False);
	    }
}

/* callback from the Data selection row all toggle button.
 */
static void
/* ARGSUSED */
ds_row_all_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int i;

	for (i = 0; i < NR; i++) {
	    Widget sw = row[i].sw;
	    if (XtIsManaged(sw) && !XmToggleButtonGetState (sw))
		XmToggleButtonSetState(sw, True, False);
	}
}

/* callback from any of the Data selection col all toggle buttons.
 */
static void
/* ARGSUSED */
ds_col_all_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int what = (int)client;
	int i;

	for (i = 0; i < NC; i++)
	    if (col[i].type == what) {
	        Widget sw = col[i].sw;
		if (XtIsManaged(sw) && !XmToggleButtonGetState (sw))
		    XmToggleButtonSetState(sw, True, False);
	    }
}

/* callback from the Data selection row reset button.
 */
static void
/* ARGSUSED */
ds_row_reset_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	ds_setup_row_selections();
}

/* callback from any of the Data selection col reset buttons.
 */
/* ARGSUSED */
static void
ds_col_reset_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	ds_setup_col_selections((ColType)client);
}

/* create the list filename prompt */
static void
dm_create_flist_w()
{
	Arg args[20];
	int n;

	n = 0;
        XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg(args[n], XmNmarginWidth, 10);  n++;
	XtSetArg(args[n], XmNmarginHeight, 10);  n++;
	flist_w = XmCreatePromptDialog (datashell_w, "DataSel", args,n);
	set_something (flist_w, XmNcolormap, (XtArgVal)xe_cm);
	set_xmstring (flist_w, XmNdialogTitle, "xephem Data list");
	set_xmstring (flist_w, XmNselectionLabelString, "File name:");
	defaultTextFN (XmSelectionBoxGetChild(flist_w, XmDIALOG_TEXT), 1,
						getPrivateDir(), "datatbl.txt");
	XtUnmanageChild (XmSelectionBoxGetChild(flist_w, XmDIALOG_HELP_BUTTON));
	XtAddCallback (flist_w, XmNokCallback, dm_flistok_cb, NULL);
	XtAddCallback (flist_w, XmNmapCallback, prompt_map_cb, NULL);
}

/* called when the Ok button is hit in the file list prompt */
/* ARGSUSED */
static void
dm_flistok_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char buf[1024];
	char *name;

	get_xmstring(w, XmNtextString, &name);

	if (strlen(name) == 0) {
	    xe_msg ("Please enter a file name.", 1);
	    XtFree (name);
	    return;
	}

	if (existsh (name) == 0 && confirm()) {
	    (void) sprintf (buf, "%s exists: Append or Overwrite?", name);
	    query (toplevel_w, buf, "Append", "Overwrite", "Cancel",
				flistok_append_cb, flistok_overwrite_cb, NULL);
	} else
	    flistok_overwrite_cb();

	XtFree (name);
}

/* called when we want to append to a flist file */
static void
flistok_append_cb ()
{
	char *name;

	get_xmstring (flist_w, XmNtextString, &name);
	make_flist (name, "a");
	XtFree (name);
}

/* called when we want to ceate a new flist file */
static void
flistok_overwrite_cb ()
{
	char *name;

	get_xmstring (flist_w, XmNtextString, &name);
	make_flist (name, "w");
	XtFree (name);
}

/* open the named flist file "a" or "w" and fill it in. */
static void
make_flist (name, how)
char *name;
char *how;
{
	FILE *fp = fopenh (name, how);

	if (fp) {
	    dm_list_tofile (fp);
	    (void) fclose (fp);
	} else {
	    char buf[1024];

	    (void) sprintf (buf, "Can not open %s: %s", name, syserrstr());
	    xe_msg (buf, 1);
	}
}

/* callback from file List control button. */
/* ARGSUSED */
static void
dm_flist_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!flist_w)
	    dm_create_flist_w();

	if (!XtIsManaged(flist_w))
	    XtManageChild (flist_w);
}

/* write the current data table to the given file */
static void
dm_list_tofile(fp)
FILE *fp;
{
	char buf[(NR+3)*(NC*20)];	/* plenty big :-) */

	dm_list_get (buf);
	fprintf (fp, "%s", buf);
}

/* fetch the current table into the given buffer.
 * TODO: send to the XA_PRIMARY selection someday.
 */
static void
dm_list_get (buf)
char buf[];
{
	Now *np = mm_get_now();
	char *onerow[NC];
	char *colhdrs[NC];
	char *rowhdrs[NR];
	int maxch[NC];
	int maxrh;
	char *str;
	int r, c;
	int l;

	/* init as legal string */
	buf[0] = '\0';

	/* malloc all the row headers to find longest */
	maxrh = 0;
	for (r = 0; r < NR; r++) {
	    if (!row[r].on)
		continue;
	    get_xmstring (row[r].lw, XmNlabelString, &rowhdrs[r]);
	    l = strlen (rowhdrs[r]);
	    if (l > maxrh)
		maxrh = l;
	}

	/* malloc all col headers and entries in first row to get max column
	 * widths.
	 * N.B. this assumes all data rows in a given column are the same
	 * length.
	 */
	for (c = 0; c < NC; c++) {
	    if (!col[c].on)
		continue;
	    get_xmstring (t_w[0][c], XmNlabelString, &onerow[c]);
	    maxch[c] = strlen (onerow[c]);
	    get_xmstring (col[c].lw, XmNlabelString, &colhdrs[c]);
	    l = strlen (colhdrs[c]);
	    if (l > maxch[c])
		maxch[c] = l;
	}

	/* print the column headers, freeing strings as we go */
	sprintf (buf+strlen(buf), "%*s", maxrh, "");
	for (c = 0; c < NC; c++) {
	    if (!col[c].on)
		continue;
	    str = colhdrs[c];
	    sprintf (buf+strlen(buf), " %-*s", maxch[c], str);
	    XtFree (str);
	}
	sprintf (buf+strlen(buf), "\n");

	/* now print the table, freeing strings as we go */
	for (r = 0; r < NR; r++) {
	    if (!row[r].on)
		continue;
	    sprintf (buf+strlen(buf), "%-*s", maxrh, rowhdrs[r]);
	    XtFree (rowhdrs[r]);
	    for (c = 0; c < NC; c++) {
		if (!col[c].on)
		    continue;
		if (r == 0)
		    str = onerow[c];
		else
		    get_xmstring (t_w[r][c], XmNlabelString, &str);
		sprintf (buf+strlen(buf), " %-*s", maxch[c], str);
		XtFree (str); /* includes onerow[] entries too */
	    }
	    sprintf (buf+strlen(buf), "\n");
	}
	sprintf (buf+strlen(buf), "\n");

	/* print the footer */
	if (XtIsManaged(XtParent(limbl_w))) {
	    get_xmstring (limbl_w, XmNlabelString, &str);
	    sprintf (buf+strlen(buf), "%s ", str);
	    XtFree (str);
	}
	if (XtIsManaged(XtParent(centric_w))) {
	    get_xmstring (centric_w, XmNlabelString, &str);
	    sprintf (buf+strlen(buf), "%s ", str);
	    XtFree (str);
	}
	get_xmstring (dt_w, XmNlabelString, &str);
	sprintf (buf+strlen(buf), "%s:\n", str);
	XtFree (str);

	/* add more details */
	strcat (buf, "\n");
	sprintf (buf+strlen(buf), " Lat: ");
	fs_sexa (buf+strlen(buf), raddeg(lat), 4, 3600);
	strcat (buf, "\n");
	sprintf (buf+strlen(buf), "Long: ");
	fs_sexa (buf+strlen(buf), -raddeg(lng), 4, 3600);
	strcat (buf, "\n");
	sprintf (buf+strlen(buf), "Elev: %gm\n", elev*ERAD);
	sprintf (buf+strlen(buf), "Temp: %gC\n", temp);
	sprintf (buf+strlen(buf), "Pres: %gmB\n", pressure);
	if ((str = mm_getsite()) != NULL)
	    sprintf (buf+strlen(buf), "Site: %s\n", str);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: datamenu.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
