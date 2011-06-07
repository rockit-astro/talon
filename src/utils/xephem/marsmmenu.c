/* code to manage the mars moon stuff.
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
#include <Xm/CascadeB.h>
#include <Xm/Separator.h>
#include <Xm/Scale.h>
#include <Xm/RowColumn.h>
#include <Xm/DrawingA.h>
#include <Xm/ToggleB.h>
#include <Xm/TextF.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "preferences.h"
#include "ps.h"


extern Widget	toplevel_w;
#define	XtD	XtDisplay(toplevel_w)
extern Colormap xe_cm;
extern char marscategory[];

extern FILE *fopenh P_((char *name, char *how));
extern Now *mm_get_now P_((void));
extern Obj *db_basic P_((int id));
extern Obj *db_scan P_((DBScan *sp));
extern XFontStruct * getXResFont P_((char *rn));
extern char *getShareDir P_((void));
extern double atod P_((char *buf));
extern double delra P_((double dra));
extern int any_ison P_((void));
extern int fs_fetch P_((Now *np, double ra, double dec, double fov,
    double mag, ObjF **opp));
extern int get_color_resource P_((Widget w, char *cname, Pixel *p));
extern int isUp P_((Widget shell));
extern int magdiam P_((int fmag, int magstp, double scale, double mag,
    double size));
extern int read_bdl P_((FILE *fp, double jd, double *xp, double *yp,
    double *zp, char ynot[]));
extern int skyloc_fifo P_((Obj *op));
extern void buttonAsButton P_((Widget w, int whether));
extern void db_scaninit P_((DBScan *sp, int mask, ObjF *op, int nop));
extern void db_update P_((Obj *op));
extern void f_prdec P_((Widget w, double a));
extern void f_double P_((Widget w, char *fmt, double f));
extern void f_ra P_((Widget w, double ra));
extern void fs_manage P_((void));
extern void fs_prdec P_((char out[], double a));
extern void fs_ra P_((char out[], double ra));
extern void get_something P_((Widget w, char *resource, XtArgVal value));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void mm_movie P_((double stepsz));
extern void obj_pickgc P_((Obj *op, Widget w, GC *gcp));
extern void register_selection P_((char *name));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void marsm_update P_((Now *np, int how_much));
extern void sv_draw_obj P_((Display *dsp, Drawable win, GC gc, Obj *op, int x,
    int y, int diam, int dotsonly));
extern void timestamp P_((Now *np, Widget w));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *buf, int app_modal));

#define	MINR	10	/* min distance for picking, pixels */

/* local record of what is now on screen for easy id from mouse picking.
 */
typedef struct {
    Obj o;		/* copy of object info.
    			 * copy from DB or, if mars moon, we fill in just
			 * o_name and s_mag/ra/dec
			 */
    int x, y;		/* screen coord */
} ScreenObj;

/* malloced arrays for each view */
typedef enum {
    SO_MAIN, SO_TOP, SO_N
} SOIdx;
static ScreenObj *screenobj[SO_N];
static int nscreenobj[SO_N];

static void marsm_create_shell_w P_((void));
static void marsm_create_tvform_w P_((void));
static void marsm_create_msform_w P_((void));
static void marsm_set_buttons P_((int whether));
static void marsm_sstats_close_cb P_((Widget w, XtPointer client,
    XtPointer call));
static void marsm_sstats_cb P_((Widget w, XtPointer client, XtPointer call));
static void marsm_option_cb P_((Widget w, XtPointer client, XtPointer call));
static void marsm_scale_cb P_((Widget w, XtPointer client, XtPointer call));
static void marsm_activate_cb P_((Widget w, XtPointer client, XtPointer call));
static void mt_unmap_cb P_((Widget w, XtPointer client, XtPointer call));
static void mt_map_cb P_((Widget w, XtPointer client, XtPointer call));
static void mt_track_size P_((void));
static void mt_da_exp_cb P_((Widget w, XtPointer client, XtPointer call));
static void marsm_popdown_cb P_((Widget w, XtPointer client, XtPointer call));
static void marsm_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void marsm_movie_cb P_((Widget w, XtPointer client, XtPointer call));
static void marsm_da_exp_cb P_((Widget w, XtPointer client, XtPointer call));
static void marsm_da_input_cb P_((Widget w, XtPointer client, XtPointer call));
static void marsm_create_popup P_((void));
static void marsm_fill_popup P_((ScreenObj *sop));
static void marsm_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void marsm_helpon_cb P_((Widget w, XtPointer client, XtPointer call));
static void marsm_locfifo_cb P_((Widget w, XtPointer client, XtPointer call));
static void marsm_calibline P_((Display *dsp, Drawable win, GC gc, int xc,
    int yc, char *tag, int tw, int th, int l));

static void add_screenobj P_((SOIdx, Obj *op, int x, int y));
static void reset_screenobj P_((SOIdx));
static ScreenObj *close_screenobj P_((SOIdx, int x, int y));

static void sky_background P_((Drawable win, unsigned w, unsigned h, int fmag,
    double ra0, double dec0, double scale, double rad, int fliptb, int fliplr));

static Widget mmshell_w;	/* main shell */
static Widget msform_w;		/* statistics form */
static Widget mmframe_w;	/* main frame */
static Widget mtform_w;		/* top-view form */
static Widget mtframe_w;	/* top-view frame */
static Widget mda_w;		/* main drawing area */
static Widget mtda_w;		/* top-view drawing area */
static Widget scale_w;		/* size scale */
static Widget limmag_w;		/* limiting magnitude scale */
static Widget dt_w;		/* main date/time stamp widget */
static Widget sdt_w;		/* statistics date/time stamp widget */
static Widget skybkg_w;		/* toggle for controlling sky background */
static Widget topview_w;	/* toggle for controlling top view */
static Pixmap marsm_pm;		/* main pixmap */
static Pixmap mt_pm;		/* top-view pixmap */
static GC marsm_fgc, marsm_bgc, marsm_xgc;	/* various colors and operators */
static XFontStruct *marsm_fs;	/* font for labels */
static int marsm_cw, marsm_ch;		/* size of label font */
static int marsm_selecting;	/* set while our fields are being selected */
static int brmoons;		/* whether we want to brightten the moons */
static int tags;		/* whether we want tags on the drawing */
static int flip_tb;		/* whether we want to flip top/bottom */
static int flip_lr;		/* whether we want to flip left/right */
static int skybkg;		/* whether we want sky background */
static int topview;		/* whether we want the top view */

static Widget mmpu_w;		/* main popup */
static Widget mmpu_name_w;	/* popup name label */
static Widget mmpu_ra_w;	/* popup RA label */
static Widget mmpu_dec_w;	/* popup Dec label */
static Widget mmpu_mag_w;	/* popup Mag label */

#define	MAXSCALE	10.0	/* max scale mag factor */
#define	MOVIE_STEPSZ	(1./12.)/* movie step size, hours */

/* field star support */
static ObjF *fstars;		/* malloced list of field stars, or NULL */
static int nfstars;		/* number of entries in fstars[] */
static double fsdec, fsra;	/* location when field stars were loaded */
#define	FSFOV	degrad(0.1)	/* size of FOV to fetch, rads */
#define	FSMAG	20.0		/* limiting mag for fetch */
#define	FSMOVE	degrad(.1)	/* reload when u has moved this far, rads */
static void marsm_loadfs P_((double ra, double dec));

/* arrays that use NM use [0] for the planet itself, and [1..NM-1] for moons.
 */
#define	NM	3		/* number of moons + 1 for planet */
static struct MoonNames {
    char *full;
    char *tag;
} mnames[NM] = {
    {"Mars", NULL},
    {"Phobos", "I"},
    {"Deimos", "II"},
};

enum {CEV, CSV, CX, CY, CZ, CRA, CDEC, CMAG, CNum};	/* marsm_w column */
static Widget	marsm_w[NM][CNum];		/* the data display widgets */

/* called when the mars moon view is activated from the mars pulldown.
 * then we begin a life of our own as a toplevel shell.
 * if never called before, create and manage all the widgets as a child of a
 * form. otherwise, just insure up.
 */
void
marsm_manage ()
{
	if (!mmshell_w) {
	    marsm_create_shell_w();
	    marsm_create_msform_w();
	    marsm_create_tvform_w();
	}
	
	XtPopup (mmshell_w, XtGrabNone);
	set_something (mmshell_w, XmNiconic, (XtArgVal)False);
	marsm_set_buttons(marsm_selecting);
}

/* called when the database has changed.
 * if we are drawing background, we'd best redraw everything.
 */
/* ARGSUSED */
void
marsm_newdb (appended)
int appended;
{
	if (skybkg)
	    marsm_update (mm_get_now(), 1);
}

int
marsm_ison()
{
	return (isUp(mmshell_w));
}

/* called by other menus as they want to hear from our buttons or not.
 * the "on"s and "off"s stack - only really redo the buttons if it's the
 * first on or the last off.
 */
void
marsm_selection_mode (whether)
int whether;	/* whether setting up for plotting or for not plotting */
{
	if (whether)
	    marsm_selecting++;
	else if (marsm_selecting > 0)
	    --marsm_selecting;

	if (mmshell_w)
	    if ((whether && marsm_selecting == 1)     /* first one to want on */
		|| (!whether && marsm_selecting == 0) /* last one to want off */)
		marsm_set_buttons (whether);
}

/* called to put up or remove the watch cursor.  */
void
marsm_cursor (c)
Cursor c;
{
	Window win;

	if (mmshell_w && (win = XtWindow(mmshell_w)) != 0) {
	    Display *dsp = XtDisplay(mmshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}

	if (msform_w && (win = XtWindow(msform_w)) != 0) {
	    Display *dsp = XtDisplay(msform_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}

	if (mtform_w && (win = XtWindow(mtform_w)) != 0) {
	    Display *dsp = XtDisplay(mtform_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* create the main moons shell */
static void
marsm_create_shell_w()
{
	typedef struct {
	    char *name;		/* toggle button instance name, or NULL */
	    char *label;	/* label */
	    int *flagp;		/* pointer to global flag, or NULL if none */
	    Widget *wp;		/* widget to save, or NULL */
	    char *tip;		/* widget tip */
	} Option;
	static Option options[] = {
	    {NULL,	   "Top view",		&topview, &topview_w,
	    	"When on, show view looking from high above N pole"},
	    {"SkyBkg",	   "Sky background",	&skybkg,  &skybkg_w,
	    	"When on, sky will include database objects and Field Stars"},
	    {"BrightMoons","Bright moons",	&brmoons, NULL,
		"Display moons disproportionately bright, even if below limit"},
	    {"Tags",	   "Tags",		&tags,    NULL,
		"Label each moon with its Roman Numeral sequence number"},
	    {"FlipTB",	   "Flip T/B",		&flip_tb, NULL,
		"Flip the display top-to-bottom"},
	    {"FlipLR",	   "Flip L/R",		&flip_lr, NULL,
		"Flip the display left-to-right"},
	};
	typedef struct {
	    char *label;	/* what goes on the help label */
	    char *key;		/* string to call hlp_dialog() */
	} HelpOn;
	static HelpOn helpon[] = {
	    {"Intro...",	"Mars Moons - intro"},
	    {"on Mouse...",	"Mars Moons - mouse"},
	    {"on Control...",	"Mars Moons - control"},
	    {"on View...",	"Mars Moons - view"},
	};
	Widget w;
	Widget mb_w, pd_w, cb_w;
	Widget mmform_w;
	XmString str;
	Arg args[20];
	int n;
	int i;

	/* create form and its shell */

	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Mars moon view"); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNiconName, "MarsMoon"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	mmshell_w = XtCreatePopupShell ("MarsMoons", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (mmshell_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (mmshell_w, XmNpopdownCallback, marsm_popdown_cb, 0);
	sr_reg (mmshell_w, "XEphem*MarsMoons.width", marscategory, 0);
	sr_reg (mmshell_w, "XEphem*MarsMoons.height", marscategory, 0);
	sr_reg (mmshell_w, "XEphem*MarsMoons.x", marscategory, 0);
	sr_reg (mmshell_w, "XEphem*MarsMoons.y", marscategory, 0);

	n = 0;
	mmform_w = XmCreateForm (mmshell_w, "MarsMoons", args, n);
	XtAddCallback (mmform_w, XmNhelpCallback, marsm_help_cb, 0);
	XtManageChild (mmform_w);

	/* create the menu bar across the top */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	mb_w = XmCreateMenuBar (mmform_w, "MB", args, n);
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

	    /* add the "Field stars" push button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "FS", args, n);
	    set_xmstring (w, XmNlabelString, "Field Stars ...");
	    XtAddCallback (w, XmNactivateCallback, (XtCallbackProc)fs_manage,0);
	    wtip (w, "Define where GSC and PPM catalogs are to be found");
	    XtManageChild (w);

	    /* add the "LOCFIFO" push button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "LOCFIFO", args, n);
	    set_xmstring (w, XmNlabelString, "Telescope command");
	    XtAddCallback (w, XmNactivateCallback, marsm_locfifo_cb, 0);
	    wtip (w, "Open export connection");
	    XtManageChild (w);

	    /* add the "movie" push button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Movie", args, n);
	    set_xmstring (w, XmNlabelString, "Movie Demo");
	    XtAddCallback (w, XmNactivateCallback, marsm_movie_cb, 0);
	    wtip (w, "Start/Stop a demonstration animation sequence");
	    XtManageChild (w);

	    /* add the "close" push button beneath a separator */

	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, marsm_close_cb, 0);
	    wtip (w, "Close this and all supporting dialogs");
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

	    for (i = 0; i < XtNumber(options); i++) {
		Option *op = &options[i];

		n = 0;
		XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
		XtSetArg (args[n], XmNmarginHeight, 0); n++;
		XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
		w = XmCreateToggleButton (pd_w, op->name ? op->name : "MMTB",
								    args, n);
		XtAddCallback (w, XmNvalueChangedCallback, marsm_option_cb, 
						    (XtPointer)(op->flagp));
		set_xmstring (w, XmNlabelString, op->label);
		if (op->flagp)
		    *(op->flagp) = XmToggleButtonGetState(w);
		if (op->wp)
		    *op->wp = w;
		if (op->tip)
		    wtip (w, op->tip);
		XtManageChild (w);
		if (op->name)
		    sr_reg (w, NULL, marscategory, 1);
	    }

	    /* add a separator */

	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    /* add the More Info control */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Stats", args, n);
	    set_xmstring (w, XmNlabelString, "More info...");
	    XtAddCallback (w, XmNactivateCallback, marsm_sstats_cb, NULL);
	    wtip (w, "Display additional details");
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
		XtAddCallback (w, XmNactivateCallback, marsm_helpon_cb,
							(XtPointer)(hp->key));
		XtManageChild (w);
		XmStringFree(str);
	    }

	/* make the date/time stamp label */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	dt_w = XmCreateLabel (mmform_w, "DateStamp", args, n);
	timestamp (mm_get_now(), dt_w);	/* establishes size */
	wtip (dt_w, "Date and Time for which map is computed");
	XtManageChild (dt_w);

	/* make the scale widget.
	 * attach both top and bottom so it's the one to follow resizing.
	 */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, mb_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, dt_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNmaximum, 100); n++;
	XtSetArg (args[n], XmNminimum, 0); n++;
	XtSetArg (args[n], XmNscaleMultiple, 10); n++;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNprocessingDirection, XmMAX_ON_TOP); n++;
	scale_w = XmCreateScale (mmform_w, "Scale", args, n);
	XtAddCallback (scale_w, XmNdragCallback, marsm_scale_cb, 0);
	XtAddCallback (scale_w, XmNvalueChangedCallback, marsm_scale_cb, 0);
	wtip (scale_w, "Zoom in and out");
	XtManageChild (scale_w);
	sr_reg (scale_w, NULL, marscategory, 0);

	/* make the limiting mag scale widget.
	 * attach both top and bottom so it's the one to follow resizing.
	 */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, mb_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, dt_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNmaximum, 20); n++;
	XtSetArg (args[n], XmNminimum, 0); n++;
	XtSetArg (args[n], XmNscaleMultiple, 1); n++;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNprocessingDirection, XmMAX_ON_TOP); n++;
	XtSetArg (args[n], XmNshowValue, True); n++;
	limmag_w = XmCreateScale (mmform_w, "LimMag", args, n);
	XtAddCallback (limmag_w, XmNdragCallback, marsm_scale_cb, 0);
	XtAddCallback (limmag_w, XmNvalueChangedCallback, marsm_scale_cb, 0);
	wtip (limmag_w, "Adjust the brightness and limiting magnitude");
	XtManageChild (limmag_w);
	sr_reg (limmag_w, NULL, marscategory, 0);

	/* make a frame for the drawing area.
	 * attach both top and bottom so it's the one to follow resizing.
	 */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, mb_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, dt_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, scale_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, limmag_w); n++;
	XtSetArg (args[n], XmNshadowType, XmSHADOW_ETCHED_OUT); n++;
	mmframe_w = XmCreateFrame (mmform_w, "UFrame", args, n);
	XtManageChild (mmframe_w);

	    /* make a drawing area for drawing the little map */

	    n = 0;
	    mda_w = XmCreateDrawingArea (mmframe_w, "Map", args, n);
	    XtAddCallback (mda_w, XmNexposeCallback, marsm_da_exp_cb, 0);
	    XtAddCallback (mda_w, XmNinputCallback, marsm_da_input_cb, 
	    						(XtPointer)SO_MAIN);
	    XtManageChild (mda_w);
}

/* make the statistics form dialog */
static void
marsm_create_msform_w()
{
	typedef struct {
	    int col;		/* C* column code */
	    char *rcname;	/* name of rowcolumn */
	    char *collabel;	/* column label */
	    char *suffix;	/* suffix for plot/list/search name */
	    char *tip;		/* widget tip */
	} MoonColumn;
	static MoonColumn mc[] = {
	    {CEV,   "UEV",   "E",     "EVis",
		"Whether geometrically visible from Earth"},
	    {CSV,   "USV",   "S",     "SVis",
		"Whether in Sun light"},
	    {CX,   "UX",   "X (+E)",     "X",
		"Apparent displacement east of planetary center"},
	    {CY,   "UY",   "Y (+S)",     "Y",
		"Apparent displacement south of planetary center"},
	    {CZ,   "UZ",   "Z (+front)", "Z",
		"Mars radii towards Earth from planetary center"},
	    {CRA,  "URA",  "RA",         "RA",
		"Right Ascension (to Main's settings)"},
	    {CDEC, "UDec", "Dec",        "Dec",
		"Declination (to Main's settings)"},
	    {CMAG, "UMag", "Mag",        "Mag",
		"Apparent visual magnitude"},
	};
	Widget w;
	Widget rc_w, title_w, sep_w;
	Arg args[20];
	int n;
	int i;

	/* create form */
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	msform_w = XmCreateFormDialog (mmshell_w, "MarsMoonStats", args, n);
	set_something (msform_w, XmNcolormap, (XtArgVal)xe_cm);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Mars Moon info"); n++;
	XtSetValues (XtParent(msform_w), args, n);

	/* make table title label */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	title_w = XmCreateLabel (msform_w, "UML", args, n);
	XtManageChild (title_w);
	set_xmstring (title_w, XmNlabelString,"Moon Positions (Mars radii)");

	/* make the moon table, one column at a time */

	/* moon designator column */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, title_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNisAligned, True); n++;
	rc_w = XmCreateRowColumn (msform_w, "UDes", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "TL", args, n);
	    set_xmstring (w, XmNlabelString, "Tag");
	    wtip (w, "Roman Numeral sequence designation of moon");
	    XtManageChild (w);

	    for (i = 0; i < NM; i++) {
		char *tag = mnames[i].tag;

		n = 0;
		w = XmCreatePushButton (rc_w, "UTag", args, n); /*PB for sz */
		set_xmstring (w, XmNlabelString, tag ? tag : " ");
		buttonAsButton (w, False);
		XtManageChild (w);
	    }

	/* moon name column */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, title_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, rc_w); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNisAligned, True); n++;
	rc_w = XmCreateRowColumn (msform_w, "UName", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "NL", args, n);
	    set_xmstring (w, XmNlabelString, "Name");
	    wtip (w, "Common name of body");
	    XtManageChild (w);

	    for (i = 0; i < NM; i++) {
		n = 0;
		w = XmCreatePushButton (rc_w, "UName", args, n); /*PB for sz*/
		set_xmstring (w, XmNlabelString, mnames[i].full);
		buttonAsButton (w, False);
		XtManageChild (w);
	    }

	/* make each of the X/Y/Z/Mag/RA/DEC information columns */

	for (i = 0; i < XtNumber (mc); i++) {
	    MoonColumn *mp = &mc[i];
	    int j;

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, title_w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNleftWidget, rc_w); n++;
	    XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	    XtSetArg (args[n], XmNisAligned, True); n++;
	    rc_w = XmCreateRowColumn (msform_w, mp->rcname, args, n);
	    XtManageChild (rc_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "ULab", args, n);
	    set_xmstring (w, XmNlabelString, mp->collabel);
	    if (mp->tip)
		wtip (w, mp->tip);
	    XtManageChild (w);

	    for (j = 0; j < NM; j++) {
		char *sel;

		sel = XtMalloc (strlen(mnames[j].full) + strlen(mp->suffix)+2);
		(void) sprintf (sel, "%s.%s", mnames[j].full, mp->suffix);

		n = 0;
		XtSetArg (args[n], XmNuserData, sel); n++;
		w = XmCreatePushButton(rc_w, "UPB", args, n);
		XtAddCallback(w, XmNactivateCallback, marsm_activate_cb, 0);
		XtManageChild (w);
		marsm_w[j][mp->col] = w;
	    }
	}

	/* make a separator */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (msform_w, "Sep1", args, n);
	XtManageChild (sep_w);

	/* make a date/time stamp */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sdt_w = XmCreateLabel (msform_w, "JDateStamp", args, n);
	wtip (sdt_w, "Date and Time for which data are computed");
	XtManageChild (sdt_w);

	/* make a separator */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sdt_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (msform_w, "Sep2", args, n);
	XtManageChild (sep_w);

	/* make the close button */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
        XtSetArg (args[n], XmNbottomOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 30); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 70); n++;
	w = XmCreatePushButton (msform_w, "Close", args, n);
	XtAddCallback (w, XmNactivateCallback, marsm_sstats_close_cb, 0);
	wtip (w, "Close this dialog");
	XtManageChild (w);
}

/* create mtform_w, the top view dialog */
static void
marsm_create_tvform_w()
{
	Arg args[20];
	int n;

	/* create form */
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	mtform_w = XmCreateFormDialog (mmshell_w, "MarsMoonsTV", args, n);
	set_something (mtform_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (mtform_w, XmNunmapCallback, mt_unmap_cb, 0);
	XtAddCallback (mtform_w, XmNmapCallback, mt_map_cb, NULL);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Mars Moons top view"); n++;
	XtSetValues (XtParent(mtform_w), args, n);

	/* fill with a drawing area in a frame */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	mtframe_w = XmCreateFrame (mtform_w, "MTVF", args, n);
	XtManageChild (mtframe_w);

	    n = 0;
	    mtda_w = XmCreateDrawingArea (mtframe_w, "Map", args, n);
	    XtAddCallback (mtda_w, XmNexposeCallback, mt_da_exp_cb, NULL);
	    XtAddCallback (mtda_w, XmNinputCallback, marsm_da_input_cb, 
							(XtPointer)SO_TOP);
	    XtManageChild (mtda_w);
}

/* go through all the buttons pickable for plotting and set whether they
 * should appear to look like buttons or just flat labels.
 */
static void
marsm_set_buttons (whether)
int whether;	/* whether setting up for plotting or for not plotting */
{
	int i, j;

	for (i = 0; i < NM; i++)
	    for (j = 0; j < CNum; j++)
		buttonAsButton (marsm_w[i][j], whether);
}

/* callback when the Close button is activated on the stats menu */
/* ARGSUSED */
static void
marsm_sstats_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (msform_w);
}

/* callback when the the More Info button is activated */
/* ARGSUSED */
static void
marsm_sstats_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtManageChild (msform_w);
	marsm_set_buttons(marsm_selecting);
}

/* callback from any of the option buttons.
 * client points to global flag to set; some don't have any.
 * in any case then just redraw everything.
 */
/* ARGSUSED */
static void
marsm_option_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (client) {
	    int *flagp = (int *)client;
	    *flagp = XmToggleButtonGetState(w);
	    if (flagp == &topview) {
		if (topview) {
		    if (!mtform_w)
			marsm_create_tvform_w();
		    XtManageChild (mtform_w);
		} else
		    XtUnmanageChild (mtform_w);
	    }
	}

	marsm_update (mm_get_now(), 1);
}

/* callback from the scales changing.
 * just redraw everything.
 */
/* ARGSUSED */
static void
marsm_scale_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	marsm_update (mm_get_now(), 1);
}

/* callback from any of the data menu buttons being activated.
 */
/* ARGSUSED */
static void
marsm_activate_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (marsm_selecting) {
	    char *name;
	    get_something (w, XmNuserData, (XtArgVal)&name);
	    register_selection (name);
	}
}

/* callback from either expose or resize of the topview.
 */
/* ARGSUSED */
static void
mt_da_exp_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static int last_nx, last_ny;
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
        Window win = XtWindow(w);
	Display *dsp = XtDisplay(w);
	unsigned int nx, ny, bw, d;
	Window root;
	int x, y;

	/* filter out a few oddball cases */
	switch (c->reason) {
	case XmCR_EXPOSE: {
	    /* turn off gravity so we get expose events for either shrink or
	     * expand.
	     */
	    static int before;
	    XExposeEvent *e = &c->event->xexpose;

	    if (!before) {
		XSetWindowAttributes swa;
                unsigned long mask = CWBitGravity | CWBackingStore;

		swa.bit_gravity = ForgetGravity;
		swa.backing_store = NotUseful; /* we use a pixmap */
		XChangeWindowAttributes (dsp, win, mask, &swa);
		before = 1;
	    }

	    /* wait for the last in the series */
	    if (e->count != 0)
		return;
	    break;
	    }
	default:
	    printf ("Unexpected mmform_w event. type=%d\n", c->reason);
	    exit(1);
	}

        XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);
	if (!mt_pm || (int)nx != last_nx || (int)ny != last_ny) {
	    if (mt_pm)
		XFreePixmap (dsp, mt_pm);
	    mt_pm = XCreatePixmap (dsp, win, nx, ny, d);
	    last_nx = nx;
	    last_ny = ny;
	    mt_track_size();
	    marsm_update (mm_get_now(), 1);
	} else
	    XCopyArea (dsp, mt_pm, win, marsm_fgc, 0, 0, nx, ny, 0, 0);
}

/* called whenever the topview scene is mapped. */
/* ARGSUSED */
static void
mt_map_cb (wid, client, call)
Widget wid;
XtPointer client;
XtPointer call;
{
	mt_track_size();
}

/* set the width of the topview DrawingArea the same as the main window's.
 * we also try to center it just above, but it doesn't always work.
 */
static void
mt_track_size()
{
	Dimension w, h;
	Position mfx, mfy, mdx, mdy;
	Position sdy;
	Arg args[20];
	int n;

	/* set widths equal */
	n = 0;
	XtSetArg (args[n], XmNwidth, &w); n++;
	XtGetValues (mda_w, args, n);

	n = 0;
	XtSetArg (args[n], XmNwidth, w); n++;
	XtSetValues (mtda_w, args, n);

	n = 0;
	XtSetArg (args[n], XmNheight, &h); n++;
	XtGetValues (mtda_w, args, n);

	/* set locations -- allow for different stuff on top of drawingareas */
	n = 0;
	XtSetArg (args[n], XmNx, &mfx); n++;
	XtSetArg (args[n], XmNy, &mfy); n++;
	XtGetValues (mmshell_w, args, n);
	n = 0;
	XtSetArg (args[n], XmNx, &mdx); n++;
	XtSetArg (args[n], XmNy, &mdy); n++;
	XtGetValues (mmframe_w, args, n);
	n = 0;
	XtSetArg (args[n], XmNy, &sdy); n++;
	XtGetValues (mtframe_w, args, n);

	n = 0;
	XtSetArg (args[n], XmNx, mfx+mdx); n++;
	XtSetArg (args[n], XmNy, mfy - h - sdy - mdy - 20); n++;
	XtSetValues (mtform_w, args, n);
}

/* callback when topview dialog is unmapped */
/* ARGSUSED */
static void
mt_unmap_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonSetState (topview_w, False, True);
}

/* callback when main shell is popped down */
/* ARGSUSED */
static void
marsm_popdown_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (msform_w);
	XtUnmanageChild (mtform_w);

        if (marsm_pm) {
	    XFreePixmap (XtDisplay(mda_w), marsm_pm);
	    marsm_pm = (Pixmap) NULL;
	}

	/* free any field stars */
	if (fstars) {
	    free ((void *)fstars);
	    fstars = NULL;
	    nfstars = 0;
	}

	/* stop movie that might be running */
	mm_movie (0.0);
}

/* callback from the main Close button */
/* ARGSUSED */
static void
marsm_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* let popdown do all the work */
	XtPopdown (mmshell_w);
}

/* callback from the Movie button
 */
/* ARGSUSED */
static void
marsm_movie_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* best effect if turn off worrying about the sky background */
	skybkg = 0;
	XmToggleButtonSetState (skybkg_w, False, False);

	mm_movie (MOVIE_STEPSZ);
}

/* callback from either expose or resize of the drawing area.
 */
/* ARGSUSED */
static void
marsm_da_exp_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static int last_nx, last_ny;
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	Window win = XtWindow(w);
	Display *dsp = XtDisplay(w);
	unsigned int nx, ny, bw, d;
	Window root;
	int x, y;

	/* filter out a few oddball cases */
	switch (c->reason) {
	case XmCR_EXPOSE: {
	    /* turn off gravity so we get expose events for either shrink or
	     * expand.
	     */
	    static int before;
	    XExposeEvent *e = &c->event->xexpose;

	    if (!before) {
		XSetWindowAttributes swa;
		unsigned long mask = CWBitGravity | CWBackingStore;

		swa.bit_gravity = ForgetGravity;
		swa.backing_store = NotUseful; /* we use a pixmap */
		XChangeWindowAttributes (dsp, win, mask, &swa);
		before = 1;
	    }

	    /* wait for the last in the series */
	    if (e->count != 0)
		return;
	    break;
	    }
	default:
	    printf ("Unexpected mmform_w event. type=%d\n", c->reason);
	    exit(1);
	}

	XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);
	if (!marsm_pm || (int)nx != last_nx || (int)ny != last_ny) {
	    if (marsm_pm)
		XFreePixmap (dsp, marsm_pm);
	    marsm_pm = XCreatePixmap (dsp, win, nx, ny, d);
	    last_nx = nx;
	    last_ny = ny;
	    if (topview)
		mt_track_size();
	    marsm_update (mm_get_now(), 1);
	} else
	    XCopyArea (dsp, marsm_pm, win, marsm_fgc, 0, 0, nx, ny, 0, 0);
}

/* callback from mouse or keyboard input over either drawing area.
 * client is one of SOIdx.
 */
/* ARGSUSED */
static void
marsm_da_input_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	ScreenObj *sop;
	SOIdx soidx;
	XEvent *ev;
	int x, y;

	/* get x and y value iff it was from mouse button 3 */
	if (c->reason != XmCR_INPUT)
	    return;
	ev = c->event;
	if (ev->xany.type != ButtonPress || ev->xbutton.button != 3)
	    return;
	x = ev->xbutton.x;
	y = ev->xbutton.y;

	/* find something close on the screen */
	soidx = (SOIdx) client;
	sop = close_screenobj (soidx, x, y);
	if (!sop)
	    return;

	/* put info in popup, mmpu_w, creating it first if necessary  */
	if (!mmpu_w)
	    marsm_create_popup();
	marsm_fill_popup (sop);

	/* put it on screen */
	XmMenuPosition (mmpu_w, (XButtonPressedEvent *)ev);
	XtManageChild (mmpu_w);
}

/* create the (unmanaged for now) popup menu in mmpu_w. */
static void
marsm_create_popup()
{
	static Widget *puw[] = {
	    &mmpu_name_w,
	    &mmpu_ra_w,
	    &mmpu_dec_w,
	    &mmpu_mag_w,
	};
	Widget w;
	Arg args[20];
	int n;
	int i;

	n = 0;
	XtSetArg (args[n], XmNisAligned, True); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	mmpu_w = XmCreatePopupMenu (mda_w, "UPU", args, n);

	/* stack everything up in labels */
	for (i = 0; i < XtNumber(puw); i++) {
	    n = 0;
	    w = XmCreateLabel (mmpu_w, "SPUL", args, n);
	    XtManageChild (w);
	    *puw[i] = w;
	}
}

/* put up a popup at ev with info about sop */
static void
marsm_fill_popup (sop)
ScreenObj *sop;
{
	char *name;
	double ra, dec, mag;
	char buf[64], buf2[64];

	name = sop->o.o_name;
	mag = get_mag(&sop->o);
	ra = sop->o.s_ra;
	dec = sop->o.s_dec;

	(void) sprintf (buf2, "%.*s", MAXNM, name);
	set_xmstring (mmpu_name_w, XmNlabelString, buf2);

	fs_ra (buf, ra);
	(void) sprintf (buf2, "  RA: %s", buf);
	set_xmstring (mmpu_ra_w, XmNlabelString, buf2);

	fs_prdec (buf, dec);
	(void) sprintf (buf2, " Dec: %s", buf);
	set_xmstring (mmpu_dec_w, XmNlabelString, buf2);

	(void) sprintf (buf2, " Mag: %.3g", mag);
	set_xmstring (mmpu_mag_w, XmNlabelString, buf2);
}

/* callback from the Help all button
 */
/* ARGSUSED */
static void
marsm_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg[] = {
"This is a simple schematic depiction of Mars and its moons.",
};

	hlp_dialog ("Mars Moons", msg, XtNumber(msg));
}

/* callback from a specific Help button.
 * client is a string to use with hlp_dialog().
 */
/* ARGSUSED */
static void
marsm_helpon_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	hlp_dialog ((char *)client, NULL, 0);
}

/* callback from the loc fifo control.
 */
/* ARGSUSED */
static void
marsm_locfifo_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Obj *op = db_basic (MARS);
	(void) skyloc_fifo (op);
}

/* add one entry to the given global screenobj array.
 */
static void
add_screenobj (soidx, op, x, y)
SOIdx soidx;
Obj *op;
int x, y;
{
	char *mem = (char *) screenobj[soidx];
	int nmem = nscreenobj[soidx];
	ScreenObj *sop;

	/* grow screenobj by one */
	if (mem)
	    mem = realloc ((char *)mem, (nmem+1)*sizeof(ScreenObj));
	else
	    mem = malloc (sizeof(ScreenObj));
	if (!mem) {
	    char msg[256];
	    (void) sprintf (msg, "Out of memory -- %s will not be pickable",
								    op->o_name);
	    xe_msg (msg, 0);
	    return;
	}
	screenobj[soidx] = (ScreenObj *) mem;

	sop = &screenobj[soidx][nscreenobj[soidx]++];

	/* fill new entry */
	sop->o = *op;
	sop->x = x;
	sop->y = y;
}

/* reclaim any existing screenobj entries from the given collection */
static void
reset_screenobj(soidx)
SOIdx soidx;
{
	if (screenobj[soidx]) {
	    free ((char *)screenobj[soidx]);
	    screenobj[soidx] = NULL;
	}
	nscreenobj[soidx] = 0;
}

/* find the entry in the given screenobj closest to [x,y] within MINR.
 * if found return the ScreenObj *, else NULL.
 */
static ScreenObj *
close_screenobj (soidx, x, y)
SOIdx soidx;
int x, y;
{
	ScreenObj *scop = screenobj[soidx];
	ScreenObj *minsop = NULL;
	int nobj = nscreenobj[soidx];
	int minr = 2*MINR;
	int i;

	for (i = 0; i < nobj; i++) {
	    ScreenObj *sop = &scop[i];
	    int r = abs(x - sop->x) + abs(y - sop->y);
	    if (r < minr) {
		minr = r;
		minsop = sop;
	    }
	}
	if (minr <= MINR)
	    return (minsop);
	else
	    return (NULL);
}

typedef struct {
    double x, y, z;	/* radii: +x:east +y:south +z:front */
    double ra, dec;
    double mag;
    int evis;		/* geometrically visible from earth */
    int svis;		/* in sun light */
} MoonData;

/* functions */
static void marsm_data();
static void moonradec();
static void marsm_draw_map ();
static void make_gcs ();

/* set svis according to whether moon is in sun light */
static void
moonSVis(op, mdp)
Obj *op;
MoonData *mdp;
{
	Obj *eop = db_basic (SUN);
	double esd = eop->s_edist;
	double eod = op->s_edist;
	double sod = op->s_sdist;
	double soa = degrad(op->s_elong);
	double esa = asin(esd*sin(soa)/sod);
	double   h = sod*op->s_hlat;
	double nod = h*(1./eod - 1./sod);
	double xp, yp, zp;
	double xpp, ypp, zpp;
	double ca, sa;
	int outside;
	int infront;

	ca = cos(esa);
	sa = sin(esa);
	xp =  ca*mdp->x + sa*mdp->z;
	yp =  mdp->y;
	zp = -sa*mdp->x + ca*mdp->z;

	ca = cos(nod);
	sa = sin(nod);
	xpp = xp;
	ypp = ca*yp - sa*zp;
	zpp = sa*yp + ca*zp;

	outside = xpp*xpp + ypp*ypp > 1.0;
	infront = zpp > 0.0;
	mdp->svis = outside || infront;
}

/* set evis according to whether moon is geometrically visible from earth */
static void
moonEVis (mdp)
MoonData *mdp;
{
	int outside = mdp->x*mdp->x + mdp->y*mdp->y > 1.0;
	int infront = mdp->z > 0.0;
	mdp->evis = outside || infront;
}

/* called to recompute and fill in values for the Mars moon menu.
 * don't bother if it doesn't exist or is unmanaged now or no one is logging.
 */
void
marsm_update (np, how_much)
Now *np;
int how_much;
{
	static char fmt[] = "%7.3f";
	Obj *mop = db_basic (MARS);
	MoonData md[NM];
	int wantstats;
	double usize;
	int i;

	/* see if we should bother */
	if (!mmshell_w)
	    return;
	wantstats = XtIsManaged(msform_w) || any_ison() || how_much;
	if (!isUp(mmshell_w) && !wantstats)
	    return;

	watch_cursor (1);

        /* compute md[0].x/y/z/mag/ra/dec and md[1..NM-1].x/y/z/mag info */
        marsm_data(mjd, mop, &usize, md);

	/* set md[0..NM-1].evis */
	md[0].evis = 1;
	for (i = 1; i < NM; i++)
	    moonEVis (&md[i]);

	/* set md[0..NM-1].svis */
	md[0].svis = 1;
	for (i = 1; i < NM; i++)
	    moonSVis (mop, &md[i]);

        /* find md[1..NM-1].ra/dec from md[0].ra/dec and md[1..NM-1].x/y */
	moonradec (usize, md);

	if (wantstats) {
	    for (i = 0; i < NM; i++) {
		f_double (marsm_w[i][CEV], "%1.0f", (double)md[i].evis);
		f_double (marsm_w[i][CSV], "%1.0f", (double)md[i].svis);
		f_double (marsm_w[i][CX], fmt, md[i].x);
		f_double (marsm_w[i][CY], fmt, md[i].y);
		f_double (marsm_w[i][CZ], fmt, md[i].z);
		f_double (marsm_w[i][CMAG], "%4.1f", md[i].mag);
		f_ra (marsm_w[i][CRA], md[i].ra);
		f_prdec (marsm_w[i][CDEC], md[i].dec);
	    }

	    timestamp (np, sdt_w);
	}

	if (isUp(mmshell_w)) {
	    marsm_draw_map (mda_w, mop, usize, md);
	    timestamp (np, dt_w);
	}

	watch_cursor (0);
}

/* called when basic resources change.
 * rebuild and redraw.
 */
void
marsm_newres()
{
	if (!mmshell_w)
	    return;
	make_gcs (mda_w);
	marsm_update (mm_get_now(), 1);
}

/* given the loc of the moons, draw a nifty little picture.
 * also save resulting screen locs of everything in the screenobj array.
 * scale of the locations is in terms of Mars radii == 1.
 * unflipped view is S up, E right.
 * planet itself is in md[0], moons in md[1..NM-1].
 *     +md[].x: East, mars radii
 *     +md[].y: South, mars radii
 *     +md[].z: in front, mars radii
 */
static void
marsm_draw_map (w, mop, usize, md)
Widget w;
Obj *mop;
double usize;
MoonData md[NM];
{
#define	NORM	100.0	/* max radius; used to normalize */
#define	MAPSCALE(r)	((r)*((int)nx)/NORM/2*scale)
#define	XCORD(x)	((int)(((int)nx)/2.0 + ew*MAPSCALE(x) + 0.5))
#define	YCORD(y)	((int)(((int)ny)/2.0 - ns*MAPSCALE(y) + 0.5))
#define ZCORD(z)        ((int)(((int)ny)/2.0 +    MAPSCALE(z) + 0.5))

	Display *dsp = XtDisplay(w);
	Window win;
	Window root;
	double scale;
	int sv;
	int fmag;
	char c;
	int x, y;
	unsigned int nx, ny, bw, d;
	int ns = flip_tb ? -1 : 1;
	int ew = flip_lr ? -1 : 1;
	int scale1;
	int i;

	/* first the main graphic */
	win = XtWindow (mda_w);

	if (!marsm_fgc)
	    make_gcs(w);

	XmScaleGetValue (limmag_w, &fmag);
	XmScaleGetValue (scale_w, &sv);
	scale = pow(MAXSCALE, sv/100.0);

	/* get size and erase */
	XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);
	XFillRectangle (dsp, marsm_pm, marsm_bgc, 0, 0, nx, ny);
	scale1 = (int)floor(MAPSCALE(1)+0.5);

	/* prepare for a new list of things on the screen */
	reset_screenobj(SO_MAIN);

	/* draw background objects, if desired */
	if (skybkg)
	    sky_background(marsm_pm, nx, ny, fmag, md[0].ra, md[0].dec,
			    usize/MAPSCALE(2), usize*NORM, flip_tb, flip_lr);

	/* draw labels */
	c = flip_lr ? 'W' : 'E';
	XDrawString(dsp, marsm_pm, marsm_fgc, nx-marsm_cw-1, ny/2-2, &c, 1);
	c = flip_tb ? 'N' : 'S';
	XDrawString(dsp, marsm_pm, marsm_fgc, (nx-marsm_cw)/2-1,
						    marsm_fs->ascent, &c, 1);

	/* draw Mars in center with unit radius */
	XPSFillArc(dsp, marsm_pm, marsm_fgc, nx/2-scale1, ny/2-scale1,
					    2*scale1-1, 2*scale1-1, 0, 360*64);
	add_screenobj (SO_MAIN, mop, nx/2, ny/2);

	/* draw 1' calibration line */
	if (tags)
	    marsm_calibline (dsp, marsm_pm, marsm_fgc, nx/2, 5*ny/6, "1'",
				marsm_cw*2, marsm_ch+4,
				(int)MAPSCALE(degrad(1.0/60.0)/(usize/2)));

	/* draw each moon
	 */
	for (i = 1; i < NM; i++) {
	    double mx = md[i].x;
	    double my = md[i].y;
	    double mag = md[i].mag;
	    int diam;
	    Obj o;

	    if (!md[i].evis)
		continue;	/* behind Mars */
	    diam = magdiam (fmag, 1, usize/(2*scale1), mag, 0.0);
	    if (brmoons)
		diam += 3;
	    if (diam <= 0)
		continue;	/* too faint */

	    x = XCORD(mx);
	    y = YCORD(my);
	    if (diam == 1)
		XDrawPoint(dsp, marsm_pm, marsm_xgc, x, y);
	    else
		XPSFillArc (dsp, marsm_pm, marsm_xgc, x-diam/2, y-diam/2,
							diam, diam, 0, 360*64);

	    /* add object to list of screen objects drawn */
	    (void) strcpy (o.o_name, mnames[i].full);
	    set_smag (&o, mag);
	    o.s_ra = (float)md[i].ra;
	    o.s_dec = (float)md[i].dec;
	    add_screenobj (SO_MAIN, &o, x, y);

	    if (tags && mnames[i].tag)
		XDrawString(dsp, marsm_pm, marsm_xgc, x-marsm_cw/2,
			    y+2*marsm_ch, mnames[i].tag, strlen(mnames[i].tag));
	}

	XCopyArea (dsp, marsm_pm, win, marsm_fgc, 0, 0, nx, ny, 0, 0);

	/* then the top view, if desired */

	if (!topview || !mt_pm)	/* let expose make new pixmap */
	    return;

	/* get size and erase */
	win = XtWindow (mtda_w);
	XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);
	XFillRectangle (dsp, mt_pm, marsm_bgc, 0, 0, nx, ny);

	/* draw Mars in center with unit radius */
	i = (int)(ew*64*raddeg(asin(sin(degrad(mop->s_elong))/mop->s_sdist)));
	XPSFillArc(dsp, mt_pm, marsm_fgc, nx/2-scale1, ny/2-scale1, 2*scale1-1,
						    2*scale1-1, -i, -180*64);
	reset_screenobj(SO_TOP);
	add_screenobj (SO_TOP, mop, nx/2, ny/2);

	/* draw each moon
	 */
	for (i = 1; i < NM; i++) {
	    double mx = md[i].x;
	    double mz = md[i].z;
	    double mag = md[i].mag;
	    int diam;
	    Obj o;

	    if (!md[i].svis)
		continue;

	    diam = magdiam (fmag, 1, usize/(2*scale1), mag, 0.0);
	    if (brmoons)
		diam += 3;
	    if (diam <= 0)
		continue;	/* too faint */

	    x = XCORD(mx);
	    y = ZCORD(mz);
	    if (diam == 1)
		XDrawPoint (dsp, mt_pm, marsm_xgc, x, y);
	    else
		XPSFillArc (dsp, mt_pm, marsm_xgc, x-diam/2, y-diam/2, diam,
							    diam, 0, 360*64);

	    /* add object to list of screen objects drawn */
	    (void) strcpy (o.o_name, mnames[i].full);
	    set_smag (&o, mag);
	    o.s_ra = (float)md[i].ra;
	    o.s_dec = (float)md[i].dec;
	    add_screenobj (SO_TOP, &o, x, y);

	    if (tags && mnames[i].tag)
		XDrawString(dsp, mt_pm, marsm_xgc, x-marsm_cw/2, y+2*marsm_ch,
					mnames[i].tag, strlen(mnames[i].tag));
	}

	/* draw label towards earth */
	if (tags) {
	    XPoint xp[5];

	    XDrawString(dsp, mt_pm, marsm_fgc, nx/2, ny-marsm_ch, "Earth", 5);
	    xp[0].x = nx/2 - 15; xp[0].y = ny - 2*marsm_ch;
	    xp[1].x = 0;         xp[1].y = 3*marsm_ch/2;
	    xp[2].x = -5;        xp[2].y = -3*marsm_ch/4;
	    xp[3].x = 10;        xp[3].y = 0;
	    xp[4].x = -5;        xp[4].y = 3*marsm_ch/4;
	    XDrawLines (dsp, mt_pm, marsm_fgc, xp, 5, CoordModePrevious);
	}

	XCopyArea (dsp, mt_pm, win, marsm_fgc, 0, 0, nx, ny, 0, 0);
}

static void
make_gcs (w)
Widget w;
{
	Display *dsp = XtDisplay(w);
	Window win = XtWindow(w);
	XFontStruct *fsp;
	XGCValues gcv;
	unsigned int gcm;
	Pixel fg, bg;

	gcm = GCForeground;
	(void) get_color_resource (w, "marsColor", &fg);
	gcv.foreground = fg;
	marsm_fgc = XCreateGC (dsp, win, gcm, &gcv);
	marsm_fs = XQueryFont (dsp, XGContextFromGC (marsm_fgc));
	marsm_cw = marsm_fs->max_bounds.width;
	marsm_ch = marsm_fs->max_bounds.ascent + marsm_fs->max_bounds.descent;

	gcm = GCForeground;
	(void) get_color_resource (w, "MarsBackground", &bg);
	gcv.foreground = bg;
	marsm_bgc = XCreateGC (dsp, win, gcm, &gcv);

	gcm = GCForeground | GCFunction | GCFont;
	fsp = getXResFont ("moonsFont");
	gcv.foreground = fg ^ bg;
	gcv.function = GXxor;
	gcv.font = fsp->fid;
	marsm_xgc = XCreateGC (dsp, win, gcm, &gcv);
}

/* draw a calibration line l pixels long centered at [xc,yc] marked with tag
 * in a bounding box tw x th.
 */
static void
marsm_calibline (dsp, win, gc, xc, yc, tag, tw, th, l)
Display *dsp;
Drawable win;
GC gc;
int xc, yc;
char *tag;
int tw, th;
int l;
{
	int lx = xc - l/2;
	int rx = lx + l;
	int i;

	XDrawLine (dsp, win, gc, lx, yc, rx, yc);

	for (i = 0; i <= 6; i++) {
	    int x = lx+i*l/6;
	    int dy = 3 + ((i%6)?0:3);

	    XDrawLine (dsp, win, gc, x, yc-dy, x, yc+dy);
	}

	XDrawString (dsp, win, gc, xc-tw/2, yc+th, tag, strlen(tag));
}

/* draw all database objects in a small sky patch about the given center.
 * get field stars too if enabled and we have moved enough.
 * save objects and screen locs in the global screenobj array for picking.
 * this is used to draw the backgrounds for the planet closeups.
 * Based on work by: Dan Bruton <WDB3926@acs.tamu.edu>
 */
static void
sky_background (win, w, h, fmag, ra0,dec0,scale,rad,fliptb,fliplr)
Drawable win;		/* window to draw on */
unsigned w, h;		/* window size */
int fmag;		/* faintest magnitude to display */
double ra0, dec0;	/* center of patch, rads */
double scale;		/* rads per pixel */
double rad;		/* maximum radius to draw away from ra0/dec0, rads */
int fliptb, fliplr;	/* flip direction; default is S up E right */
{
	static int before;
	double cdec0 = cos(dec0);
	DBScan dbs;
	Obj *op;

	if (!before && pref_get(PREF_EQUATORIAL) == PREF_GEO) {
	    xe_msg ("Equatorial preference should probably be set to Topocentric", 1);
	    before = 1;
	}

        /* update field star list */
	marsm_loadfs (ra0, dec0);

	/* scan the database and draw whatever is near */
	for (db_scaninit(&dbs, ALLM, fstars, nfstars);
					    (op = db_scan (&dbs)) != NULL; ) {
	    double dra, ddec;
	    GC gc;
	    int dx, dy, x, y;
	    int diam;

	    if (is_planet (op, MARS)) {
		/* we draw it elsewhere */
		continue;
	    }

	    db_update (op);

	    /* find size, in pixels. */
	    diam = magdiam (fmag, 1, scale, get_mag(op),
						degrad(op->s_size/3600.0));
	    if (diam <= 0)
		continue;

	    /* find x/y location if it's in the general area */
	    dra = op->s_ra - ra0;	/* + E */
	    ddec = dec0 - op->s_dec;	/* + S */
	    if (fabs(ddec) > rad || delra(dra)*cdec0 > rad)
		continue;
	    dx = (int)(dra/scale+.5);
	    dy = (int)(ddec/scale+.5);
	    x = fliplr ? (int)w/2-dx : (int)w/2+dx;
	    y = fliptb ? (int)h/2+dy : (int)h/2-dy;

	    /* pick a gc */
	    obj_pickgc(op, toplevel_w, &gc);

	    /* draw 'er */
	    sv_draw_obj (XtD, win, gc, op, x, y, diam, 0);

	    /* save 'er */
	    add_screenobj (SO_MAIN, op, x, y);
	}

	sv_draw_obj (XtD, win, (GC)0, NULL, 0, 0, 0, 0);	/* flush */
}

/* load field stars around the given location, unless current set is
 * already close enough.
 */
static void
marsm_loadfs (ra, dec)
double ra, dec;
{
	Now *np = mm_get_now();

	if (fstars && fabs(dec-fsdec)<FSMOVE && cos(dec)*delra(ra-fsra)<FSMOVE)
	    return;

	if (fstars) {
	    free ((void *)fstars);
	    fstars = NULL;
	    nfstars = 0;
	}

        nfstars = fs_fetch (np, ra, dec, FSFOV, FSMAG, &fstars);

	if (nfstars > 0) {
	    char msg[128];
            (void) sprintf (msg, "Mars Moon View added %d field stars",nfstars);
	    xe_msg (msg, 0);
	    fsdec = dec;
	    fsra = ra;
	}
}

/* given Mars loc in md[0].ra/dec and size, and location of each moon in 
 * md[1..NM-1].x/y in u radii, find ra/dec of each moon in md[1..NM-1].ra/dec.
 */
static void
moonradec (usize, md)
double usize;		/* u diameter, rads */
MoonData md[NM];	/* fill in RA and Dec */
{
	double urad = usize/2;
	double ura = md[0].ra;
	double udec = md[0].dec;
	int i;

	for (i = 1; i < NM; i++) {
	    double dra  = urad * md[i].x;
	    double ddec = urad * md[i].y;
	    md[i].ra  = ura + dra;
	    md[i].dec = udec - ddec;
	}
}

#define	dsin(x)	sin(degrad(x))
#define	dcos(x)	cos(degrad(x))

static int use_bdl P_((double jd, MoonData md[NM]));

/* given a modified julian date (ie, days since Jan .5 1900), d, return x, y, z
 *   location of each Mars' moon as a multiple of Mars's radius. +x is
 *   easterly, +y is southerly, +z is towards earth. x and z are relative to
 *   the equator of Mars; y is further corrected for earth's position above
 *   or below this plane. corrected for light travel time. also return Mars
 *   ra/dec/mag in md[0] and diam, in rads. also return nominal magnitude of
 *   each moon.
 */
static void
marsm_data(d, mop, usize, md)
Obj *mop;
double d;
double *usize;
MoonData md[NM];	/* fill in md[0].ra/dec/mag/x/y/z for Mars,
			 * md[1..NM-1].x/y/z/mag for each moon
			 */
{
        static int using_bdl = -1; /* log which model, but only when changes */
	double dmag = 5.0*log10(mop->s_edist + 0.4);

	md[0].ra = mop->s_ra;
	md[0].dec = mop->s_dec;
	md[0].mag = get_mag(mop);

	md[0].x = 0;
	md[0].y = 0;
	md[0].z = 0;

	*usize = degrad(mop->s_size/3600.0);

	/* from Pasachoff/Menzel: brightest @ .6 AU */

	md[1].mag = 11.8 + dmag;
	md[2].mag = 12.9 + dmag;

	/* get moon data from BDL if possible */

	if (use_bdl (d+MJD0, md) == 0) {
	    if (using_bdl != 1) {
		xe_msg ("Using BDL model for Mars' moons", 0);
		using_bdl = 1;
	    }
	} else {
	    int i;

	    for (i = 1; i < NM; i++)
		md[i].x = md[i].y = md[i].z = 0;
	    if (using_bdl != 0) {
		xe_msg ("No model available for Mars' moons.", 1);
		using_bdl = 0;
	    }
	}
}

/* hunt for BDL file. return 0 if ok, else -1 */
static int
use_bdl (JD, md)
double JD;		/* julian date */
MoonData md[NM];        /* fill md[1..NM-1].x/y/z for each moon */
{
#define	MRAU	.00002269	/* Mars radius, AU */
	static FILE *fp;	/* keep open once find */
	double x[NM], y[NM], z[NM];
	char buf[1024];
	int i;

	/* only valid 1999 through 2010 */
	if (JD < 2451179.50000 || JD >= 2455562.5)
	    return (-1);

	/* open first time */
	if (!fp) {
	    (void) sprintf (buf, "%s/auxil/mars.9910", getShareDir());
	    fp = fopenh (buf, "r");
	    if (!fp)
		return (-1);
	}

	/* use it */
	rewind (fp);
	if ((i = read_bdl (fp, JD, x, y, z, buf)) < 0) {
	    xe_msg (buf, 1);
	    fclose (fp);
	    fp = NULL;
	    return (-1);
	}
	if (i != NM-1) {
	    sprintf (buf, "BDL says %d moons, code expects %d", i, NM-1);
	    xe_msg (buf, 1);
	    fclose (fp);
	    fp = NULL;
	    return (-1);
	}

	/* copy into md[1..NM-1] with our scale and sign conventions */
	for (i = 1; i < NM; i++) {
	    md[i].x =  x[i-1]/MRAU;	/* we want u radii +E */
	    md[i].y = -y[i-1]/MRAU;	/* we want u radii +S */
	    md[i].z = -z[i-1]/MRAU;	/* we want u radii +front */
	}

	/* ok */
	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: marsmmenu.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
