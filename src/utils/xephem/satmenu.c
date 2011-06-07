/* code to manage the stuff on the "saturn" menu.
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

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "preferences.h"
#include "ps.h"


extern Widget	toplevel_w;
#define	XtD	XtDisplay(toplevel_w)
extern Colormap xe_cm;

extern FILE *fopenh P_((char *name, char *how));
extern Now *mm_get_now P_((void));
extern Obj *db_basic P_((int id));
extern Obj *db_scan P_((DBScan *sp));
extern XFontStruct * getXResFont P_((char *rn));
extern char *getShareDir P_((void));
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
extern void sm_update P_((Now *np, int how_much));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void sv_draw_obj P_((Display *dsp, Drawable win, GC gc, Obj *op, int x,
    int y, int diam, int dotsonly));
extern void timestamp P_((Now *np, Widget w));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *string));
extern void xe_msg P_((char *buf, int app_modal));

#define	MINR	10	/* min distance for picking, pixels */

/* local record of what is now on screen for easy id from mouse picking.
 */
typedef struct {
    Obj o;		/* copy of object info.
    			 * copy from DB or, if saturn moon, we fill in just
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

static void sm_create_shell_w P_((void));
static void sm_create_tvform_w P_((void));
static void sm_create_ssform_w P_((void));
static void sm_set_buttons P_((int whether));
static void sm_sstats_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void sm_sstats_cb P_((Widget w, XtPointer client, XtPointer call));
static void sm_option_cb P_((Widget w, XtPointer client, XtPointer call));
static void sm_scale_cb P_((Widget w, XtPointer client, XtPointer call));
static void sm_activate_cb P_((Widget w, XtPointer client, XtPointer call));
static void sm_popdown_cb P_((Widget w, XtPointer client, XtPointer call));
static void st_unmap_cb P_((Widget w, XtPointer client, XtPointer call));
static void st_map_cb P_((Widget w, XtPointer client, XtPointer call));
static void st_track_size P_((void));
static void st_da_exp_cb P_((Widget w, XtPointer client, XtPointer call));
static void sm_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void sm_movie_cb P_((Widget w, XtPointer client, XtPointer call));
static void sm_da_exp_cb P_((Widget w, XtPointer client, XtPointer call));
static void sm_da_input_cb P_((Widget w, XtPointer client, XtPointer call));
static void sm_create_popup P_((void));
static void sm_fill_popup P_((ScreenObj *sop));
static void sm_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void sm_helpon_cb P_((Widget w, XtPointer client, XtPointer call));
static void sm_locfifo_cb P_((Widget w, XtPointer client, XtPointer call));
static void sm_calibline P_((Display *dsp, Drawable win, GC gc, int xc, int yc,
    char *tag, int tw, int th, int l));

static void add_screenobj P_((SOIdx, Obj *op, int x, int y));
static void reset_screenobj P_((SOIdx));
static ScreenObj *close_screenobj P_((SOIdx, int x, int y));

static void sky_background P_((Drawable win, unsigned w, unsigned h, int fmag,
    double ra0, double dec0, double scale, double rad, int fliptb, int fliplr));

static Widget satshell_w;	/* main shell */
static Widget ssform_w;		/* statistics form */
static Widget smframe_w;	/* main frame */
static Widget stform_w;		/* top-view form */
static Widget stframe_w;	/* top-view frame */
static Widget sda_w;		/* main drawing area */
static Widget stda_w;		/* top-view drawing area */
static Widget ringet_w, ringst_w;/* labels for displaying ring tilts */
static Widget scale_w;		/* size scale */
static Widget limmag_w;		/* limiting magnitude scale */
static Widget dt_w;		/* main date/time stamp widget */
static Widget sdt_w;		/* statistics date/time stamp widget */
static Widget skybkg_w;		/* toggle for controlling sky background */
static Widget topview_w;	/* toggle for controlling top view */
static Pixmap sm_pm;		/* main pixmap */
static Pixmap st_pm;		/* top-view pixmap */
static GC s_fgc, s_bgc, s_xgc;  /* various colors and operators */
static XFontStruct *s_fs;       /* font for labels */
static int s_cw, s_ch;          /* size of label font */
static int sm_selecting;	/* set while our fields are being selected */
static int brmoons;		/* whether we want to brightten the moons */
static int tags;		/* whether we want tags on the drawing */
static int flip_tb;		/* whether we want to flip top/bottom */
static int flip_lr;		/* whether we want to flip left/right */
static int skybkg;		/* whether we want sky background */
static int topview;		/* whether we want the top view */

static Widget smpu_w;		/* main popup */
static Widget smpu_name_w;	/* popup name label */
static Widget smpu_ra_w;	/* popup RA label */
static Widget smpu_dec_w;	/* popup Dec label */
static Widget smpu_mag_w;	/* popup Mag label */

#define	MAXSCALE	10.0	/* max scale mag factor */
#define	MOVIE_STEPSZ	0.25	/* movie step size, hours */

/* field star support */
static ObjF *fstars;            /* malloced list of field stars, or NULL */
static int nfstars;             /* number of entries in fstars[] */
static double fsdec, fsra;      /* location when field stars were loaded */
#define FSFOV   degrad(1.0)     /* size of FOV to fetch, rads */
#define FSMAG   20.0            /* limiting mag for fetch */
#define FSMOVE  degrad(.2)      /* reload when sat has moved this far, rads */
static void sm_loadfs P_((double ra, double dec));

/* arrays that use NM use [0] for the planet itself, and [1..NM-1] for moons.
 */
#define	NM	9		/* number of moons + 1 for planet */
static struct MoonNames {
    char *full;
    char *tag;
} mnames[NM] = {
    {"Saturn",	NULL},
    {"Mimas",	"I"},
    {"Enceladus","II"},
    {"Tethys",	"III"},
    {"Dione",	"IV"},
    {"Rhea",	"V"},
    {"Titan",	"VI"},
    {"Hyperion","VII"},
    {"Iapetus",	"VIII"},
};

enum {CEV, CSV, CX, CY, CZ, CRA, CDEC, CMAG, CNum};	/* s_w column index */
static Widget	s_w[NM][CNum];			/* the data display widgets */

static char satcategory[] = "Saturn";		/* Save category */

/* called when the saturn menu is activated via the main menu pulldown.
 * if never called before, create and manage all the widgets as a child of a
 * form. otherwise, otherwise, just get out there and do it!
 */
void
sm_manage ()
{
	if (!satshell_w) {
	    sm_create_shell_w();
	    sm_create_ssform_w();
	    sm_create_tvform_w();
	}
	
	XtPopup (satshell_w, XtGrabNone);
	set_something (satshell_w, XmNiconic, (XtArgVal)False);
	sm_set_buttons(sm_selecting);
}

/* called when the database has changed.
 * if we are drawing background, we'd best redraw everything.
 */
/* ARGSUSED */
void
sm_newdb (appended)
int appended;
{
	if (skybkg)
	    sm_update (mm_get_now(), 1);
}

int
sm_ison()
{
	return (isUp(satshell_w));
}

/* called by other menus as they want to hear from our buttons or not.
 * the "on"s and "off"s stack - only really redo the buttons if it's the
 * first on or the last off.
 */
void
sm_selection_mode (whether)
int whether;	/* whether setting up for plotting or for not plotting */
{
	if (whether)
	    sm_selecting++;
	else if (sm_selecting > 0)
	    --sm_selecting;

	if (satshell_w)
	    if ((whether && sm_selecting == 1)     /* first one to want on */
		|| (!whether && sm_selecting == 0) /* last one to want off */)
		sm_set_buttons (whether);
}

/* called to put up or remove the watch cursor.  */
void
sm_cursor (c)
Cursor c;
{
	Window win;

	if (satshell_w && (win = XtWindow(satshell_w)) != 0) {
	    Display *dsp = XtDisplay(satshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}

	if (ssform_w && (win = XtWindow(ssform_w)) != 0) {
	    Display *dsp = XtDisplay(ssform_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}

	if (stform_w && (win = XtWindow(stform_w)) != 0) {
	    Display *dsp = XtDisplay(stform_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* create the main shell */
static void
sm_create_shell_w()
{
	typedef struct {
	    char *name;		/* toggle button instance name, or NULL */
	    char *label;	/* label */
	    int *flagp;		/* pointer to global flag, or NULL if none */
	    Widget *wp;		/* widget to save, or NULL */
	    char *tip;          /* widget tip */
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
	    {"Intro...",	"Saturn - intro"},
	    {"on Mouse...",	"Saturn - mouse"},
	    {"on Control...",	"Saturn - control"},
	    {"on View...",	"Saturn - view"},
	};
	Widget w;
	Widget mb_w, pd_w, cb_w;
	Widget satform_w;
	XmString str;
	Arg args[20];
	int n;
	int i;

	/* create form and shell */

	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Saturn view"); n++;
	XtSetArg (args[n], XmNiconName, "Saturn"); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	satshell_w = XtCreatePopupShell ("Saturn", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (satshell_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (satshell_w, XmNpopdownCallback, sm_popdown_cb, 0);
	sr_reg (satshell_w, "XEphem*Saturn.width", satcategory, 0);
	sr_reg (satshell_w, "XEphem*Saturn.height", satcategory, 0);
	sr_reg (satshell_w, "XEphem*Saturn.x", satcategory, 0);
	sr_reg (satshell_w, "XEphem*Saturn.y", satcategory, 0);


	n = 0;
	satform_w = XmCreateForm (satshell_w, "SaturnF", args, n);
	XtAddCallback (satform_w, XmNhelpCallback, sm_help_cb, 0);
	XtManageChild (satform_w);

	/* create the menu bar across the top */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	mb_w = XmCreateMenuBar (satform_w, "MB", args, n);
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
	    set_xmstring (w, XmNlabelString, "Field Stars...");
	    XtAddCallback (w, XmNactivateCallback, (XtCallbackProc)fs_manage,0);
	    wtip (w, "Define where GSC and PPM catalogs are to be found");
	    XtManageChild (w);

	    /* add the "LOCFIFO" push button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "LOCFIFO", args, n);
	    set_xmstring (w, XmNlabelString, "Telescope command");
	    XtAddCallback (w, XmNactivateCallback, sm_locfifo_cb, 0);
	    wtip (w, "Open export connection");
	    XtManageChild (w);

	    /* add the "movie" push button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Movie", args, n);
	    set_xmstring (w, XmNlabelString, "Movie Demo");
	    XtAddCallback (w, XmNactivateCallback, sm_movie_cb, 0);
	    wtip (w, "Start/Stop a demonstration animation sequence");
	    XtManageChild (w);

	    /* add the "close" push button beneath a separator */

	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, sm_close_cb, 0);
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
		w = XmCreateToggleButton (pd_w, op->name ? op->name : "SMTB",
								    args, n);
		XtAddCallback (w, XmNvalueChangedCallback, sm_option_cb, 
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
		    sr_reg (w, NULL, satcategory, 1);
	    }

	    /* add a separator */

	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    /* add the More Info control */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Stats", args, n);
	    set_xmstring (w, XmNlabelString, "More info...");
	    XtAddCallback (w, XmNactivateCallback, sm_sstats_cb, NULL);
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
		XtAddCallback (w, XmNactivateCallback, sm_helpon_cb,
							(XtPointer)(hp->key));
		XtManageChild (w);
		XmStringFree(str);
	    }

	/* make the date/time stamp label */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	dt_w = XmCreateLabel (satform_w, "DateStamp", args, n);
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
	scale_w = XmCreateScale (satform_w, "Scale", args, n);
	XtAddCallback (scale_w, XmNdragCallback, sm_scale_cb, 0);
	XtAddCallback (scale_w, XmNvalueChangedCallback, sm_scale_cb, 0);
	wtip (scale_w, "Zoom in and out");
	XtManageChild (scale_w);
	sr_reg (scale_w, NULL, satcategory, 0);

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
	limmag_w = XmCreateScale (satform_w, "LimMag", args, n);
	XtAddCallback (limmag_w, XmNdragCallback, sm_scale_cb, 0);
	XtAddCallback (limmag_w, XmNvalueChangedCallback, sm_scale_cb, 0);
	wtip (limmag_w, "Adjust the brightness and limiting magnitude");
	XtManageChild (limmag_w);
	sr_reg (limmag_w, NULL, satcategory, 0);

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
	smframe_w = XmCreateFrame (satform_w, "SatFrame", args, n);
	XtManageChild (smframe_w);

	    /* make a drawing area for drawing the little map */

	    n = 0;
	    sda_w = XmCreateDrawingArea (smframe_w, "Map", args, n);
	    XtAddCallback (sda_w, XmNexposeCallback, sm_da_exp_cb, 0);
	    XtAddCallback (sda_w, XmNinputCallback, sm_da_input_cb, 
							(XtPointer)SO_MAIN);
	    XtManageChild (sda_w);
}

/* make the statistics form dialog */
static void
sm_create_ssform_w()
{
	typedef struct {
	    int col;		/* C* column code */
	    char *rcname;	/* name of rowcolumn */
	    char *collabel;	/* column label */
	    char *suffix;	/* suffix for plot/list/search name */
	    char *tip;          /* widget tip */
	} MoonColumn;
	static MoonColumn mc[] = {
	    {CEV,   "UEV",   "E",     "EVis",
		"Whether geometrically visible from Earth"},
	    {CSV,   "USV",   "S",     "SVis",
		"Whether in Sun light"},
	    {CX,   "SatX",   "X (+E)",     "X",
	    	"Apparent displacement east of planetary center"},
	    {CY,   "SatY",   "Y (+S)",     "Y",
	    	"Apparent displacement south of planetary center"},
	    {CZ,   "SatZ",   "Z (+front)", "Z",
	    	"Saturn radii towards Earth from planetary center"},
	    {CRA,  "SatRA",  "RA",         "RA",
	    	"Right Ascension (to Main's settings)"},
	    {CDEC, "SatDec", "Dec",        "Dec",
	    	"Declination (to Main's settings)"},
	    {CMAG, "SatMag", "Mag",        "Mag",
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
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_ANY); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	ssform_w = XmCreateFormDialog (satshell_w, "SaturnStats", args, n);
	set_something (ssform_w, XmNcolormap, (XtArgVal)xe_cm);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Saturn info"); n++;
	XtSetValues (XtParent(ssform_w), args, n);

	/* make ring tilt label */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	title_w = XmCreateLabel (ssform_w, "SatTL", args, n);
	XtManageChild (title_w);
	set_xmstring (title_w, XmNlabelString, "Ring tilt (degrees, front +S)");

	/* make the earth ring tilt r/c */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, title_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNnumColumns, 1); n++;
	rc_w = XmCreateRowColumn (ssform_w, "SatERC", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "SatETMsg", args, n);
	    set_xmstring (w, XmNlabelString, "From Earth:");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNuserData, "Saturn.ETilt"); n++;
	    ringet_w = XmCreatePushButton (rc_w, "SatET", args, n);
	    XtAddCallback (ringet_w, XmNactivateCallback, sm_activate_cb, 0);
	    wtip (ringet_w, "Ring tilt as seen from Earth");
	    XtManageChild (ringet_w);

	/* make the sun tilt r/c */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, title_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNnumColumns, 1); n++;
	rc_w = XmCreateRowColumn (ssform_w, "SatSRC", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "SatSTMsg", args, n);
	    set_xmstring (w, XmNlabelString, "From Sun:");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNuserData, "Saturn.STilt"); n++;
	    ringst_w = XmCreatePushButton (rc_w, "SatST", args, n);
	    XtAddCallback (ringst_w, XmNactivateCallback, sm_activate_cb, 0);
	    wtip (ringst_w, "Ring tilt as seen from Sun");
	    XtManageChild (ringst_w);

	/* make table title label */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	title_w = XmCreateLabel (ssform_w, "SatML", args, n);
	XtManageChild (title_w);
	set_xmstring (title_w, XmNlabelString,"Moon Positions (Saturn radii)");

	/* make the moon table, one column at a time */

	/* moon designator column */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, title_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNisAligned, True); n++;
	rc_w = XmCreateRowColumn (ssform_w, "SatDes", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "TL", args, n);
	    set_xmstring (w, XmNlabelString, "Tag");
	    wtip (w, "Roman Numeral sequence designation of moon");
	    XtManageChild (w);

	    for (i = 0; i < NM; i++) {
		char *tag = mnames[i].tag;

		n = 0;
		w = XmCreatePushButton (rc_w, "SatTag", args, n); /*PB for sz */
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
	rc_w = XmCreateRowColumn (ssform_w, "SatName", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "NL", args, n);
	    set_xmstring (w, XmNlabelString, "Name");
	    wtip (w, "Common name of body");
	    XtManageChild (w);

	    for (i = 0; i < NM; i++) {
		n = 0;
		w = XmCreatePushButton (rc_w, "SatName", args, n); /*PB for sz*/
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
	    rc_w = XmCreateRowColumn (ssform_w, mp->rcname, args, n);
	    XtManageChild (rc_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "SatLab", args, n);
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
		w = XmCreatePushButton(rc_w, "SatPB", args, n);
		XtAddCallback(w, XmNactivateCallback, sm_activate_cb, 0);
		XtManageChild (w);
		s_w[j][mp->col] = w;
	    }
	}

	/* make a separator */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (ssform_w, "Sep1", args, n);
	XtManageChild (sep_w);

	/* make a date/time stamp */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sdt_w = XmCreateLabel (ssform_w, "JDateStamp", args, n);
	wtip (sdt_w, "Date and Time for which data are computed");
	XtManageChild (sdt_w);

	/* make a separator */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sdt_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (ssform_w, "Sep2", args, n);
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
	w = XmCreatePushButton (ssform_w, "Close", args, n);
	XtAddCallback (w, XmNactivateCallback, sm_sstats_close_cb, 0);
	wtip (w, "Close this dialog");
	XtManageChild (w);
}

/* create stform_w, the top view dialog */
static void
sm_create_tvform_w()
{
	Arg args[20];
	int n;

	/* create form */
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNdefaultPosition, False); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	stform_w = XmCreateFormDialog (satshell_w, "SaturnTV", args, n);
	set_something (stform_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (stform_w, XmNunmapCallback, st_unmap_cb, 0);
	XtAddCallback (stform_w, XmNmapCallback, st_map_cb, NULL);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Saturn top view"); n++;
	XtSetValues (XtParent(stform_w), args, n);

	/* fill with a drawing area in a frame */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	stframe_w = XmCreateFrame (stform_w, "STVF", args, n);
	XtManageChild (stframe_w);

	    n = 0;
	    stda_w = XmCreateDrawingArea (stframe_w, "Map", args, n);
	    XtAddCallback (stda_w, XmNexposeCallback, st_da_exp_cb, NULL);
	    XtAddCallback (stda_w, XmNinputCallback, sm_da_input_cb, 
							(XtPointer)SO_TOP);
	    XtManageChild (stda_w);
}

/* go through all the buttons pickable for plotting and set whether they
 * should appear to look like buttons or just flat labels.
 */
static void
sm_set_buttons (whether)
int whether;	/* whether setting up for plotting or for not plotting */
{
	int i, j;

	for (i = 0; i < NM; i++) {
	    for (j = 0; j < CNum; j++)
		buttonAsButton (s_w[i][j], whether);
	}
	buttonAsButton (ringet_w, whether);
	buttonAsButton (ringst_w, whether);
}

/* callback when the Close button is activated on the stats menu */
/* ARGSUSED */
static void
sm_sstats_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (ssform_w);
}

/* callback when the More Info button is activated */
/* ARGSUSED */
static void
sm_sstats_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtManageChild (ssform_w);
	sm_set_buttons(sm_selecting);
}

/* callback from any of the option buttons.
 * client points to global flag to set; some don't have any.
 * in any case then just redraw everything.
 */
/* ARGSUSED */
static void
sm_option_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (client) {
	    int *flagp = (int *)client;
	    *flagp = XmToggleButtonGetState(w);
	    if (flagp == &topview) {
		if (topview) {
		    if (!stform_w)
			sm_create_tvform_w();
		    XtManageChild (stform_w);
		} else
		    XtUnmanageChild (stform_w);
	    }
	}

	sm_update (mm_get_now(), 1);
}

/* callback from the scales changing.
 * just redraw everything.
 */
/* ARGSUSED */
static void
sm_scale_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	sm_update (mm_get_now(), 1);
}

/* callback from any of the data menu buttons being activated.
 */
/* ARGSUSED */
static void
sm_activate_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (sm_selecting) {
	    char *name;
	    get_something (w, XmNuserData, (XtArgVal)&name);
	    register_selection (name);
	}
}

/* callback from either expose or resize of the topview.
 */
/* ARGSUSED */
static void
st_da_exp_cb (w, client, call)
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
	    printf ("Unexpected satform_w event. type=%d\n", c->reason);
	    exit(1);
	}

        XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);
	if (!st_pm || (int)nx != last_nx || (int)ny != last_ny) {
	    if (st_pm)
		XFreePixmap (dsp, st_pm);
	    st_pm = XCreatePixmap (dsp, win, nx, ny, d);
	    last_nx = nx;
	    last_ny = ny;
	    st_track_size();
	    sm_update (mm_get_now(), 1);
	} else
	    XCopyArea (dsp, st_pm, win, s_fgc, 0, 0, nx, ny, 0, 0);
}

/* called whenever the topview scene is mapped. */
/* ARGSUSED */
static void
st_map_cb (wid, client, call)
Widget wid;
XtPointer client;
XtPointer call;
{
	st_track_size();
}

/* set the width of the topview DrawingArea the same as the main window's.
 * we also try to center it just above, but it doesn't always work.
 */
static void
st_track_size()
{
	Dimension w, h;
	Position mfx, mfy, mdx, mdy;
	Position sdy;
	Arg args[20];
	int n;

	/* set widths equal */
	n = 0;
	XtSetArg (args[n], XmNwidth, &w); n++;
	XtGetValues (sda_w, args, n);

	n = 0;
	XtSetArg (args[n], XmNwidth, w); n++;
	XtSetValues (stda_w, args, n);

	n = 0;
	XtSetArg (args[n], XmNheight, &h); n++;
	XtGetValues (stda_w, args, n);

	/* set locations -- allow for different stuff on top of drawingareas */
	n = 0;
	XtSetArg (args[n], XmNx, &mfx); n++;
	XtSetArg (args[n], XmNy, &mfy); n++;
	XtGetValues (satshell_w, args, n);
	n = 0;
	XtSetArg (args[n], XmNx, &mdx); n++;
	XtSetArg (args[n], XmNy, &mdy); n++;
	XtGetValues (smframe_w, args, n);
	n = 0;
	XtSetArg (args[n], XmNy, &sdy); n++;
	XtGetValues (stframe_w, args, n);

	n = 0;
	XtSetArg (args[n], XmNx, mfx+mdx); n++;
	XtSetArg (args[n], XmNy, mfy - h - sdy - mdy - 20); n++;
	XtSetValues (stform_w, args, n);
}

/* callback when topview dialog is unmapped */
/* ARGSUSED */
static void
st_unmap_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonSetState (topview_w, False, True);
}

/* callback when main shell is popped down */
/* ARGSUSED */
static void
sm_popdown_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (ssform_w);
	XtUnmanageChild (stform_w);

        if (sm_pm) {
	    XFreePixmap (XtDisplay(sda_w), sm_pm);
	    sm_pm = (Pixmap) NULL;
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
sm_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* let pop down do all the work */
	XtPopdown (satshell_w);
}

/* callback from the Movie button
 */
/* ARGSUSED */
static void
sm_movie_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* smoother if turn off the sky background */
	skybkg = 0;
	XmToggleButtonSetState (skybkg_w, False, False);

	mm_movie (MOVIE_STEPSZ);
}

/* callback from either expose or resize of the drawing area.
 */
/* ARGSUSED */
static void
sm_da_exp_cb (w, client, call)
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
	    printf ("Unexpected satform_w event. type=%d\n", c->reason);
	    exit(1);
	}

        XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);
	if (!sm_pm || (int)nx != last_nx || (int)ny != last_ny) {
	    if (sm_pm)
		XFreePixmap (dsp, sm_pm);
	    sm_pm = XCreatePixmap (dsp, win, nx, ny, d);
	    last_nx = nx;
	    last_ny = ny;
	    if (topview)
		st_track_size();
	    sm_update (mm_get_now(), 1);
	} else
	    XCopyArea (dsp, sm_pm, win, s_fgc, 0, 0, nx, ny, 0, 0);
}

/* callback from mouse or keyboard input over either drawing area
 * client is one of SOIdx.
 */
/* ARGSUSED */
static void
sm_da_input_cb (w, client, call)
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

	/* put info in popup, smpu_w, creating it first if necessary  */
	if (!smpu_w)
	    sm_create_popup();
	sm_fill_popup (sop);

	/* put it on screen */
	XmMenuPosition (smpu_w, (XButtonPressedEvent *)ev);
	XtManageChild (smpu_w);
}

/* create the (unmanaged for now) popup menu in smpu_w. */
static void
sm_create_popup()
{
	static Widget *puw[] = {
	    &smpu_name_w,
	    &smpu_ra_w,
	    &smpu_dec_w,
	    &smpu_mag_w,
	};
	Widget w;
	Arg args[20];
	int n;
	int i;

	n = 0;
	XtSetArg (args[n], XmNisAligned, True); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	smpu_w = XmCreatePopupMenu (sda_w, "SatPU", args, n);

	/* stack everything up in labels */
	for (i = 0; i < XtNumber(puw); i++) {
	    n = 0;
	    w = XmCreateLabel (smpu_w, "SPUL", args, n);
	    XtManageChild (w);
	    *puw[i] = w;
	}
}

/* put up a popup at ev with info about sop */
static void
sm_fill_popup (sop)
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
	set_xmstring (smpu_name_w, XmNlabelString, buf2);

	fs_ra (buf, ra);
	(void) sprintf (buf2, "  RA: %s", buf);
	set_xmstring (smpu_ra_w, XmNlabelString, buf2);

	fs_prdec (buf, dec);
	(void) sprintf (buf2, " Dec: %s", buf);
	set_xmstring (smpu_dec_w, XmNlabelString, buf2);

	(void) sprintf (buf2, " Mag: %.3g", mag);
	set_xmstring (smpu_mag_w, XmNlabelString, buf2);
}

/* callback from the Help all button
 */
/* ARGSUSED */
static void
sm_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg[] = {
"This is a simple schematic depiction of Saturn and its moons.",
};

	hlp_dialog ("Saturn", msg, XtNumber(msg));
}

/* callback from a specific Help button.
 * client is a string to use with hlp_dialog().
 */
/* ARGSUSED */
static void
sm_helpon_cb (w, client, call)
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
sm_locfifo_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Obj *op = db_basic (SATURN);
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
static void saturn_data();
static void moonradec();
static void sm_draw_map ();
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

/* called to recompute and fill in values for the saturn menu.
 * don't bother if it doesn't exist or is unmanaged now or no one is logging.
 */
void
sm_update (np, how_much)
Now *np;
int how_much;
{
	static char fmt[] = "%7.3f";
	Obj *sop = db_basic (SATURN);
	MoonData md[NM];
	int wantstats;
	double etilt, stilt;
	double satsize;
	int i;

	/* see if we should bother */
	if (!satshell_w)
	    return;
	wantstats = XtIsManaged(ssform_w) || any_ison() || how_much;
	if (!isUp(satshell_w) && !wantstats)
	    return;

	watch_cursor (1);

	/* compute md[0].x/y/z/mag/ra/dec and md[1..NM-1].x/y/z/mag info */
        saturn_data(mjd + MJD0, sop, &satsize, &etilt, &stilt, md);

	/* set md[0..NM-1].evis */
	md[0].evis = 1;
	for (i = 1; i < NM; i++)
	    moonEVis (&md[i]);

	/* set md[0..NM-1].svis */
	md[0].svis = 1;
	for (i = 1; i < NM; i++)
	    moonSVis (sop, &md[i]);

	/* find md[1..NM-1].ra/dec from md[0].ra/dec and md[1..NM-1].x/y */
	moonradec (satsize, md);

	if (wantstats) {
	    for (i = 0; i < NM; i++) {
		f_double (s_w[i][CEV], "%1.0f", (double)md[i].evis);
		f_double (s_w[i][CSV], "%1.0f", (double)md[i].svis);
		f_double (s_w[i][CX], fmt, md[i].x);
		f_double (s_w[i][CY], fmt, md[i].y);
		f_double (s_w[i][CZ], fmt, md[i].z);
		f_double (s_w[i][CMAG], "%4.1f", md[i].mag);
		f_ra (s_w[i][CRA], md[i].ra);
		f_prdec (s_w[i][CDEC], md[i].dec);
	    }

	    f_double (ringet_w, fmt, raddeg(etilt));
	    f_double (ringst_w, fmt, raddeg(stilt));
	    timestamp (np, sdt_w);
	}

	if (isUp(satshell_w)) {
	    sm_draw_map (sop, etilt, satsize, md);
	    timestamp (np, dt_w);
	}

	watch_cursor (0);
}

/* called when basic resources change.
 * rebuild and redraw.
 */
void
sm_newres()
{
	if (!satshell_w)
	    return;
	make_gcs (sda_w);
	sm_update (mm_get_now(), 1);
}

/* given the loc of the moons, draw a nifty little picture.
 * also save resulting screen locs of everything in the screenobj array.
 * scale of the locations is in terms of saturn radii == 1.
 * unflipped view is S up, E right.
 * positive tilt means the front rings are tilted southward, in rads.
 * planet itself is in md[0], moons in md[1..NM-1].
 *     +md[].x: East, sat radii
 *     +md[].y: South, sat radii
 *     +md[].z: in front, sat radii
 */
static void
sm_draw_map (sop, etilt, satsize, md)
Obj *sop;
double etilt;
double satsize;
MoonData md[NM];
{
#define RLW     3       /* ring line width, pixels */
#define NORM    60.0    /* max Iapetus orbit radius; used to normalize */
#define	IRR	1.528	/* inner edge of ring system */
#define	ORR	2.267	/* outter edge of A ring */
#define MAPSCALE(r)     ((r)*((int)nx)/NORM/2*scale)
#define	XCORD(x)	((int)(((int)nx)/2.0 + ew*MAPSCALE(x) + 0.5))
#define	YCORD(y)	((int)(((int)ny)/2.0 - ns*MAPSCALE(y) + 0.5))
#define	ZCORD(z)	((int)(((int)ny)/2.0 +    MAPSCALE(z) + 0.5))

	Display *dsp = XtDisplay(sda_w);
	Window win;
	Window root;
	double scale;
	int sv;
	int fmag;
	char c;
	int x, y;
	int irw, irh, orw, orh, irx, iry, orx, ory;
	unsigned int nx, ny, bw, d;
	int ns = flip_tb ? -1 : 1;
	int ew = flip_lr ? -1 : 1;
	int scale1;
	int i, b;

	/* first the main graphic */
	win = XtWindow (sda_w);

	if (!s_fgc) 
	    make_gcs(sda_w);

	XmScaleGetValue (limmag_w, &fmag);
	XmScaleGetValue (scale_w, &sv);
	scale = pow(MAXSCALE, sv/100.0);

	/* get size and erase */
	XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);
	XFillRectangle (dsp, sm_pm, s_bgc, 0, 0, nx, ny);
	scale1 = (int)floor(MAPSCALE(1)+0.5);

	/* prepare for a new list of things on the screen */
	reset_screenobj(SO_MAIN);

	/* draw background objects, if desired */
	if (skybkg)
	    sky_background(sm_pm, nx, ny, fmag, md[0].ra, md[0].dec,
			satsize/MAPSCALE(2), satsize*NORM, flip_tb, flip_lr);

	/* draw labels */
	c = flip_lr ? 'W' : 'E';
	XDrawString(dsp, sm_pm, s_fgc, nx-s_cw-1, ny/2-2, &c, 1);
	c = flip_tb ? 'N' : 'S';
	XDrawString(dsp, sm_pm, s_fgc, (nx-s_cw)/2-1, s_fs->ascent, &c, 1);

	/* draw saturn in center with unit radius */
	XPSFillArc(dsp, sm_pm, s_fgc, nx/2-scale1, ny/2-scale1, 2*scale1-1,
						    2*scale1-1, 0, 360*64);
	add_screenobj (SO_MAIN, sop, nx/2, ny/2);

	/* rings of radius IRR and ORR.
	 * draw rings in front of planet using xor.
	 * positive tilt means the front rings are tilted southward.
	 * always draw the solid s_fgc last in case we are near the ring plane.
	 */
	irh = (int)(MAPSCALE(2*IRR*fabs(sin(etilt))));
	irw = (int)(MAPSCALE(2*IRR));
	orh = (int)(MAPSCALE(2*ORR*fabs(sin(etilt))));
	orw = (int)MAPSCALE(2*ORR);
	irx = (int)(nx/2 - MAPSCALE(IRR));
	iry = (int)(ny/2 - MAPSCALE(IRR*fabs(sin(etilt))));
	orx = (int)(nx/2 - MAPSCALE(ORR));
	ory = (int)(ny/2 - MAPSCALE(ORR*fabs(sin(etilt))));
	if (irh < RLW || orh < RLW) {
	    /* too near the ring plane to draw a fill ellipse */
	    XDrawLine (dsp, sm_pm, s_fgc, orx, ny/2, nx-orx, ny/2);
	} else {
	    /* near rings are up if tilt is positive and we are not flipping n/s
	     * or if tilt is negative and we are flipping n/s.
	     */
	    int nearup = ((etilt>0.0) == !flip_tb);

	    XDrawArc (dsp, sm_pm, s_xgc, irx, iry, irw, irh,
					     nearup ? 0 : 180*64, 180*64-1);
	    XDrawArc (dsp, sm_pm, s_xgc, orx, ory, orw, orh,
					     nearup ? 0 : 180*64, 180*64-1);
	    XDrawArc (dsp, sm_pm, s_fgc, irx, iry, irw, irh,
					     nearup ? 180*64 : 0, 180*64-1);
	    XDrawArc (dsp, sm_pm, s_fgc, orx, ory, orw, orh,
					     nearup ? 180*64 : 0, 180*64-1);
	}

	/* draw 1' calibration line */
	if (tags)
	    sm_calibline (dsp, sm_pm, s_fgc, nx/2, 5*ny/6, "1'", s_cw*2, s_ch+4,
				(int)MAPSCALE(degrad(1.0/60.0)/(satsize/2)));

	/* draw each moon
	 */
	for (i = 1; i < NM; i++) {
	    double mx = md[i].x;
	    double my = md[i].y;
	    double mag = md[i].mag;
	    int diam;
	    Obj o;

	    if (!md[i].evis)
		continue;	/* behind saturn */
	    diam = magdiam (fmag, 1, satsize/(2*scale1), mag, 0.0);
	    if (brmoons)
		diam += 3;
	    if (diam <= 0)
		continue;	/* too faint */

	    x = XCORD(mx);
	    y = YCORD(my);
	    if (diam == 1)
		XDrawPoint(dsp, sm_pm, s_xgc, x, y);
	    else
		XPSFillArc (dsp, sm_pm, s_xgc, x-diam/2, y-diam/2, diam, diam,
								    0, 360*64);

	    /* add object to list of screen objects drawn */
	    (void) strcpy (o.o_name, mnames[i].full);
	    set_smag (&o, mag);
	    o.s_ra = (float)md[i].ra;
	    o.s_dec = (float)md[i].dec;
	    add_screenobj (SO_MAIN, &o, x, y);

	    if (tags && mnames[i].tag)
		XDrawString(dsp, sm_pm, s_xgc, x-s_cw/2, y+2*s_ch,
					mnames[i].tag, strlen(mnames[i].tag));
	}

	XCopyArea (dsp, sm_pm, win, s_fgc, 0, 0, nx, ny, 0, 0);

	/* then the top view, if desired */

	if (!topview || !st_pm)	/* let expose make new pixmap */
	    return;

	/* get size and erase */
	win = XtWindow (stda_w);
	XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);
	XFillRectangle (dsp, st_pm, s_bgc, 0, 0, nx, ny);

	/* draw saturn in center with unit radius */
	i = (int)(ew*64*raddeg(asin(sin(degrad(sop->s_elong))/sop->s_sdist)));
	XPSFillArc(dsp, st_pm, s_fgc, nx/2-scale1, ny/2-scale1, 2*scale1-1,
						    2*scale1-1, -i, -180*64);
	reset_screenobj(SO_TOP);
	add_screenobj (SO_TOP, sop, nx/2, ny/2);

	/* rings of radius IRR and ORR... simple arcs from up here */
	irw = (int)(MAPSCALE(2*IRR));
	orw = (int)(MAPSCALE(2*ORR));
	irx = (int)(nx/2 - MAPSCALE(IRR));
	iry = (int)(ny/2 - MAPSCALE(IRR));
	orx = (int)(nx/2 - MAPSCALE(ORR));
	ory = (int)(ny/2 - MAPSCALE(ORR));
	b = (int)(64*raddeg(acos(1./IRR)));
	XDrawArc (dsp, st_pm, s_xgc, irx, iry, irw, irw, b-i, -(180*64+2*b));
	b = (int)(64*raddeg(acos(1./ORR)));
	XDrawArc (dsp, st_pm, s_xgc, orx, ory, orw, orw, b-i, -(180*64+2*b));

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

	    diam = magdiam (fmag, 1, satsize/(2*scale1), mag, 0.0);
	    if (brmoons)
		diam += 3;
	    if (diam <= 0)
		continue;	/* too faint */

	    x = XCORD(mx);
	    y = ZCORD(mz);
	    if (diam == 1)
		XDrawPoint (dsp, st_pm, s_xgc, x, y);
	    else
		XPSFillArc (dsp, st_pm, s_xgc, x-diam/2, y-diam/2, diam, diam,
								    0, 360*64);

	    /* add object to list of screen objects drawn */
	    (void) strcpy (o.o_name, mnames[i].full);
	    set_smag (&o, mag);
	    o.s_ra = (float)md[i].ra;
	    o.s_dec = (float)md[i].dec;
	    add_screenobj (SO_TOP, &o, x, y);

	    if (tags && mnames[i].tag)
		XDrawString(dsp, st_pm, s_xgc, x-s_cw/2, y+2*s_ch,
					mnames[i].tag, strlen(mnames[i].tag));
	}

	/* draw label towards earth */
	if (tags) {
	    XPoint xp[5];

	    XDrawString(dsp, st_pm, s_fgc, nx/2, ny-s_ch, "Earth", 5);
	    xp[0].x = nx/2 - 15; xp[0].y = ny - 2*s_ch;
	    xp[1].x = 0;         xp[1].y = 3*s_ch/2;
	    xp[2].x = -5;        xp[2].y = -3*s_ch/4;
	    xp[3].x = 10;        xp[3].y = 0;
	    xp[4].x = -5;        xp[4].y = 3*s_ch/4;
	    XDrawLines (dsp, st_pm, s_fgc, xp, 5, CoordModePrevious);
	}

	XCopyArea (dsp, st_pm, win, s_fgc, 0, 0, nx, ny, 0, 0);
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
	get_color_resource (w, "saturnColor", &fg);
	gcv.foreground = fg;
	s_fgc = XCreateGC (dsp, win, gcm, &gcv);
	s_fs = XQueryFont (dsp, XGContextFromGC (s_fgc));
	s_cw = s_fs->max_bounds.width;
	s_ch = s_fs->max_bounds.ascent + s_fs->max_bounds.descent;

	gcm = GCForeground;
	get_color_resource (w, "SaturnBackground", &bg);
	gcv.foreground = bg;
	s_bgc = XCreateGC (dsp, win, gcm, &gcv);

	gcm = GCForeground | GCFunction | GCFont;
	fsp = getXResFont ("moonsFont");
	gcv.foreground = fg ^ bg;
	gcv.function = GXxor;
	gcv.font = fsp->fid;
	s_xgc = XCreateGC (dsp, win, gcm, &gcv);
}

/* draw a calibration line l pixels long centered at [xc,yc] marked with tag
 * with is in a bounding box tw x th.
 */
static void
sm_calibline (dsp, win, gc, xc, yc, tag, tw, th, l)
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

	XDrawLine (dsp, win, gc, lx, yc, rx, yc);
	XDrawLine (dsp, win, gc, lx, yc-3, lx, yc+3);
	XDrawLine (dsp, win, gc, rx, yc-3, rx, yc+3);
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
	sm_loadfs (ra0, dec0);

	/* scan the database and draw whatever is near */
	for (db_scaninit(&dbs, ALLM, fstars, nfstars);
					    (op = db_scan (&dbs)) != NULL; ) {
	    double dra, ddec;
	    GC gc;
	    int dx, dy, x, y;
	    int diam;

	    if (is_planet (op, SATURN)) {
		/* we draw it elsewhere :-) */
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
	    dx = (int)(dra/scale);
	    dy = (int)(ddec/scale);
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
sm_loadfs (ra, dec)
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
            (void) sprintf (msg, "Saturn View added %d field stars", nfstars);
	    xe_msg (msg, 0);
	    fsdec = dec;
	    fsra = ra;
	}
}

/* given saturn loc in md[0].ra/dec and size, and location of each moon in 
 * md[1..NM-1].x/y in sat radii, find ra/dec of each moon in md[1..NM-1].ra/dec.
 */
static void
moonradec (satsize, md)
double satsize;		/* sat diameter, rads */
MoonData md[NM];	/* fill in RA and Dec */
{
	double satrad = satsize/2;
	double satra = md[0].ra;
	double satdec = md[0].dec;
	int i;

	for (i = 1; i < NM; i++) {
	    double dra  = satrad * md[i].x;
	    double ddec = satrad * md[i].y;
	    md[i].ra  = satra + dra;
	    md[i].dec = satdec - ddec;
	}
}

static void ring_tilt P_((Obj *sop, double JD, double *etiltp, double *stiltp));
static void bruton_saturn P_((Obj *sop, double JD, MoonData md[NM]));
static int use_bdl P_((double JD, MoonData md[NM]));

/* get saturn info in md[0].x/y/z/ra/dec/mag.
 * get moon info in md[1..NM-1].x/y/z/mag (not ra, dec)
 */
static void
saturn_data (JD, sop, sizep, etiltp, stiltp, md)
double JD;
Obj *sop;
double *sizep;			/* saturn's angular diam, rads */
double *etiltp, *stiltp;	/* earth and sun tilts -- +S */
MoonData md[NM];        /* fill in md[0].ra/dec/mag/x/y/z for satmenu,
                         * md[1..NM-1].x/y/z/mag for each moon
			 */
{
	static int using_bdl = -1; /* log which model, but only when changes */

	md[0].ra = sop->s_ra;
	md[0].dec = sop->s_dec;
	md[0].mag = get_mag(sop);

	md[0].x = 0;
	md[0].y = 0;
	md[0].z = 0;

	*sizep = degrad(sop->s_size/3600.0);

	/*  Visual Magnitude of the Satellites */

	md[1].mag = 13; md[2].mag = 11.8; md[3].mag = 10.3; md[4].mag = 10.2;
	md[5].mag = 9.8; md[6].mag = 8.4; md[7].mag = 14.3; md[8].mag = 11.2;

	/* get tilts from sky and tel code */

	ring_tilt (sop, JD, etiltp, stiltp);

	/* get moon data from BDL if possible, else Bruton's model */

	if (use_bdl (JD, md) == 0) {
	    if (using_bdl != 1) {
		xe_msg ("Using BDL model for Saturn's moons", 0);
		using_bdl = 1;
	    }
	} else {
	    if (using_bdl != 0) {
		xe_msg ("Using Bruton model for Saturn's moons", 0);
		using_bdl = 0;
	    }
	    bruton_saturn (sop, JD, md);
	}
}

/* hunt for BDL file. return 0 if ok, else -1 */
static int
use_bdl (JD, md)
double JD;		/* julian date */
MoonData md[NM];        /* fill md[1..NM-1].x/y/z for each moon */
{
#define	SATRAU	.0004014253	/* saturn radius, AU */
	static FILE *fp;	/* keep open once find */
	double x[NM], y[NM], z[NM];
	char buf[1024];
	int i;

	/* only valid 1999 through 2010 */
	if (JD < 2451179.50000 || JD >= 2455562.5)
	    return (-1);

	/* open first time */
	if (!fp) {
	    (void) sprintf (buf, "%s/auxil/saturne.9910", getShareDir());
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
	    md[i].x =  x[i-1]/SATRAU;	/* we want sat radii +E */
	    md[i].y = -y[i-1]/SATRAU;	/* we want sat radii +S */
	    md[i].z = -z[i-1]/SATRAU;	/* we want sat radii +front */
	}

	/* ok */
	return (0);
}

/*  */
/*     SS2TXT.BAS                     Dan Bruton, astro@tamu.edu */
/*  */
/*       This is a text version of SATSAT2.BAS.  It is smaller, */
/*    making it easier to convert other languages (250 lines */
/*    compared to 850 lines). */
/*  */
/*       This BASIC program computes and displays the locations */
/*    of Saturn's Satellites for a given date and time.  See */
/*    "Practical Astronomy with your Calculator" by Peter */
/*    Duffett-Smith and the Astronomical Almanac for explanations */
/*    of some of the calculations here.  The code is included so */
/*    that users can make changes or convert to other languages. */
/*    This code was made using QBASIC (comes with DOS 5.0). */
/*  */
/*    ECD: merged with Sky and Tel, below, for better earth and sun ring tilt */
/*  */

/* ECD: BASICeze */
#define	FOR	for
#define	IF	if
#define	ELSE	else
#define	COS	cos
#define	SIN	sin
#define	TAN	tan
#define ATN	atan
#define ABS	fabs
#define SQR	sqrt

/* find saturn moon data from Bruton's model */
/* this originally computed +X:East +Y:North +Z:behind in [1..8] indeces.
 * and +tilt:front south, rads
 * then we adjust things in md[].x/y/z/mag to fit into our MoonData format.
 */
static void
bruton_saturn (sop, JD, md)
Obj *sop;		/* saturn */
double JD;		/* julian date */
MoonData md[NM];        /* fill md[1..NM-1].x/y/z for each moon */
{
     /* ECD: code does not use [0].
      * ECD and why 11 here? seems like 9 would do
      */
     double SMA[11], U[11], U0[11], PD[11];
     double X[NM], Y[NM], Z[NM];

     double P,TP,TE,EP,EE,RE0,RP0,RS;
     double JDE,LPE,LPP,LEE,LEP;
     double NN,ME,MP,VE,VP;
     double LE,LP,RE,RP,DT,II,F,F1;
     double RA,DECL;
     double TVA,PVA,TVC,PVC,DOT1,INC,TVB,PVB,DOT2,INCI;
     double TRIP,GAM,TEMPX,TEMPY,TEMPZ;
     int I;

     /* saturn */
     RA = sop->s_ra;
     DECL = sop->s_dec;

     /*  ******************************************************************** */
     /*  *                                                                  * */
     /*  *                        Constants                                 * */
     /*  *                                                                  * */
     /*  ******************************************************************** */
     P = PI / 180;
     /*  Orbital Rate of Saturn in Radians per Days */
     TP = 2 * PI / (29.45771 * 365.2422);
     /*  Orbital Rate of Earth in Radians per Day */
     TE = 2 * PI / (1.00004 * 365.2422);
     /*  Eccentricity of Saturn's Orbit */
     EP = .0556155;
     /*  Eccentricity of Earth's Orbit */
     EE = .016718;
     /*  Semimajor axis of Earth's and Saturn's orbit in Astronomical Units */
     RE0 = 1; RP0 = 9.554747;
     /*  Semimajor Axis of the Satellites' Orbit in Kilometers */
     SMA[1] = 185600; SMA[2] = 238100; SMA[3] = 294700; SMA[4] = 377500;
     SMA[5] = 527200; SMA[6] = 1221600; SMA[7] = 1483000; SMA[8] = 3560100;
     /*  Eccentricity of Satellites' Orbit [Program uses 0] */
     /*  Synodic Orbital Period of Moons in Days */
     PD[1] = .9425049;
     PD[2] = 1.3703731;
     PD[3] = 1.8880926;
     PD[4] = 2.7375218;
     PD[5] = 4.5191631;
     PD[6] = 15.9669028;
     PD[7] = 21.3174647;
     PD[8] = 79.9190206;	/* personal mail 1/14/95 */
     RS = 60330; /*  Radius of planet in kilometers */
    
     /*  ******************************************************************** */
     /*  *                                                                  * */
     /*  *                      Epoch Information                           * */
     /*  *                                                                  * */
     /*  ******************************************************************** */
     JDE = 2444238.5; /*  Epoch Jan 0.0 1980 = December 31,1979 0:0:0 UT */
     LPE = 165.322242 * P; /*  Longitude of Saturn at Epoch */
     LPP = 92.6653974 * P; /*  Longitude of Saturn`s Perihelion */
     LEE = 98.83354 * P; /*  Longitude of Earth at Epoch */
     LEP = 102.596403 * P; /*  Longitude of Earth's Perihelion */
     /*  U0[I] = Angle from inferior geocentric conjuction */
     /*          measured westward along the orbit at epoch */
     U0[1] = 18.2919 * P;
     U0[2] = 174.2135 * P;
     U0[3] = 172.8546 * P;
     U0[4] = 76.8438 * P;
     U0[5] = 37.2555 * P;
     U0[6] = 57.7005 * P;
     U0[7] = 266.6977 * P;
     U0[8] = 195.3513 * P;	/* from personal mail 1/14/1995 */
    
     /*  ******************************************************************** */
     /*  *                                                                  * */
     /*  *                    Orbit Calculations                            * */
     /*  *                                                                  * */
     /*  ******************************************************************** */
     /*  ****************** FIND MOON ORBITAL ANGLES ************************ */
     NN = JD - JDE; /*  NN = Number of days since epoch */
     ME = ((TE * NN) + LEE - LEP); /*  Mean Anomoly of Earth */
     MP = ((TP * NN) + LPE - LPP); /*  Mean Anomoly of Saturn */
     VE = ME; VP = MP; /*  True Anomolies - Solve Kepler's Equation */
     FOR (I = 1; I <= 3; I++) {
	 VE = VE - (VE - (EE * SIN(VE)) - ME) / (1 - (EE * COS(VE)));
	 VP = VP - (VP - (EP * SIN(VP)) - MP) / (1 - (EP * COS(VP)));
     }
     VE = 2 * ATN(SQR((1 + EE) / (1 - EE)) * TAN(VE / 2));
     IF (VE < 0) VE = (2 * PI) + VE;
     VP = 2 * ATN(SQR((1 + EP) / (1 - EP)) * TAN(VP / 2));
     IF (VP < 0) VP = (2 * PI) + VP;
     /*   Heliocentric Longitudes of Earth and Saturn */
     LE = VE + LEP; IF (LE > (2 * PI)) LE = LE - (2 * PI);
     LP = VP + LPP; IF (LP > (2 * PI)) LP = LP - (2 * PI);
     /*   Distances of Earth and Saturn from the Sun in AU's */
     RE = RE0 * (1 - EE * EE) / (1 + EE * COS(VE));
     RP = RP0 * (1 - EP * EP) / (1 + EP * COS(VP));
     /*   DT = Distance from Saturn to Earth in AU's - Law of Cosines */
     DT = SQR((RE * RE) + (RP * RP) - (2 * RE * RP * COS(LE - LP)));
     /*   II = Angle between Earth and Sun as seen from Saturn */
     II = RE * SIN(LE - LP) / DT;
     II = ATN(II / SQR(1 - II * II)); /*   ArcSIN and Law of Sines */
     /*    F = NN - (Light Time to Earth in days) */
     F = NN - (DT / 173.83);
     F1 = II + MP - VP;
     /*  U(I) = Angle from inferior geocentric conjuction measured westward */
     FOR (I = 1; I < NM; I++) {
	U[I] = U0[I] + (F * 2 * PI / PD[I]) + F1;
	U[I] = ((U[I] / (2 * PI)) - (int)(U[I] / (2 * PI))) * 2 * PI;

     }

     /*  **************** FIND INCLINATION OF RINGS ************************* */
     /*  Use dot product of Earth-Saturn vector and Saturn's rotation axis */
     TVA = (90 - 83.51) * P; /*  Theta coordinate of Saturn's axis */
     PVA = 40.27 * P; /*  Phi coordinate of Saturn's axis */
     TVC = (PI / 2) - DECL;
     PVC = RA;
     DOT1 = SIN(TVA) * COS(PVA) * SIN(TVC) * COS(PVC);
     DOT1 = DOT1 + SIN(TVA) * SIN(PVA) * SIN(TVC) * SIN(PVC);
     DOT1 = DOT1 + COS(TVA) * COS(TVC);
     INC = ATN(SQR(1 - DOT1 * DOT1) / DOT1); /*    ArcCOS */
     IF (INC > 0) INC = (PI / 2) - INC; ELSE INC = -(PI / 2) - INC;

     /*  ************* FIND INCLINATION OF IAPETUS' ORBIT ******************* */
     /*  Use dot product of Earth-Saturn vector and Iapetus' orbit axis */
     /*  Vector B */
     TVB = (90 - 75.6) * P; /*  Theta coordinate of Iapetus' orbit axis (estimate) */
     PVB = 21.34 * 2 * PI / 24; /*  Phi coordinate of Iapetus' orbit axis (estimate) */
     DOT2 = SIN(TVB) * COS(PVB) * SIN(TVC) * COS(PVC);
     DOT2 = DOT2 + SIN(TVB) * SIN(PVB) * SIN(TVC) * SIN(PVC);
     DOT2 = DOT2 + COS(TVB) * COS(TVC);
     INCI = ATN(SQR(1 - DOT2 * DOT2) / DOT2); /*    ArcCOS */
     IF (INCI > 0) INCI = (PI / 2) - INCI; ELSE INCI = -(PI / 2) - INCI;

     /*  ************* FIND ROTATION ANGLE OF IAPETUS' ORBIT **************** */
     /*  Use inclination of Iapetus' orbit with respect to ring plane */
     /*  Triple Product */
     TRIP = SIN(TVC) * COS(PVC) * SIN(TVA) * SIN(PVA) * COS(TVB);
     TRIP = TRIP - SIN(TVC) * COS(PVC) * SIN(TVB) * SIN(PVB) * COS(TVA);
     TRIP = TRIP + SIN(TVC) * SIN(PVC) * SIN(TVB) * COS(PVB) * COS(TVA);
     TRIP = TRIP - SIN(TVC) * SIN(PVC) * SIN(TVA) * COS(PVA) * COS(TVB);
     TRIP = TRIP + COS(TVC) * SIN(TVA) * COS(PVA) * SIN(TVB) * SIN(PVB);
     TRIP = TRIP - COS(TVC) * SIN(TVB) * COS(PVB) * SIN(TVA) * SIN(PVA);
     GAM = -1 * ATN(TRIP / SQR(1 - TRIP * TRIP)); /*  ArcSIN */
    
     /*  ******************************************************************** */
     /*  *                                                                  * */
     /*  *                     Compute Moon Positions                       * */
     /*  *                                                                  * */
     /*  ******************************************************************** */
     FOR (I = 1; I < NM - 1; I++) {
	 X[I] = -1 * SMA[I] * SIN(U[I]) / RS;
	 Z[I] = -1 * SMA[I] * COS(U[I]) / RS;	/* ECD */
	 Y[I] = SMA[I] * COS(U[I]) * SIN(INC) / RS;
     }
     /*  ************************* Iapetus' Orbit *************************** */
     TEMPX = -1 * SMA[8] * SIN(U[8]) / RS;
     TEMPZ = -1 * SMA[8] * COS(U[8]) / RS;
     TEMPY = SMA[8] * COS(U[8]) * SIN(INCI) / RS;
     X[8] = TEMPX * COS(GAM) + TEMPY * SIN(GAM); /*       Rotation */
     Z[8] = TEMPZ * COS(GAM) + TEMPY * SIN(GAM);
     Y[8] = -1 * TEMPX * SIN(GAM) + TEMPY * COS(GAM);
    
#ifdef SHOWALL
     /*  ******************************************************************** */
     /*  *                                                                  * */
     /*  *                          Show Results                            * */
     /*  *                                                                  * */
     /*  ******************************************************************** */
     printf ("                           Julian Date : %g\n", JD);
     printf ("             Right Ascension of Saturn : %g Hours\n", RA * 24 / (2 * PI));
     printf ("                 Declination of Saturn : %g\n", DECL / P);
     printf ("   Ring Inclination as seen from Earth : %g\n", -1 * INC / P);
     printf ("      Heliocentric Longitude of Saturn : %g\n", LP / P);
     printf ("       Heliocentric Longitude of Earth : %g\n", LE / P);
     printf ("                Sun-Saturn-Earth Angle : %g\n", II / P);
     printf ("     Distance between Saturn and Earth : %g AU = %g million miles\n", DT, (DT * 93));
     printf ("       Light time from Saturn to Earth : %g minutes\n", DT * 8.28);
     TEMP = 2 * ATN(TAN(165.6 * P / (2 * 3600)) / DT) * 3600 / P;
     printf ("                Angular Size of Saturn : %g arcsec\n", TEMP);
     printf ("  Major Angular Size of Saturn's Rings : %g arcsec\n", RS4 * TEMP / RS);
     printf ("  Minor Angular Size of Saturn's Rings : %g arcsec\n", ABS(RS4 * TEMP * SIN(INC) / RS));
#endif

     /* copy into md[1..NM-1] with our sign conventions */
     for (I = 1; I < NM; I++) {
	md[I].x =  X[I];	/* we want +E */
	md[I].y = -Y[I];	/* we want +S */
	md[I].z = -Z[I];	/* we want +front */
     }
}

/*
 *  RINGS OF SATURN by Olson, et al, BASIC Code from Sky & Telescope, May 1995.
 *  As converted from BASIC to C by pmartz@dsd.es.com (Paul Martz)
 *  Adapted to xephem by Elwood Charles Downey.
 */

static void
ring_tilt (sop, JD, etiltp, stiltp)
Obj *sop;			/* saturn */
double JD;			/* Julian date */
double *etiltp, *stiltp;	/* tilt from earth and sun, rads southward */
{
	Obj *eop = db_basic(SUN);	/* for earth */
	double t, i, om;
	double x, y, z;
	double la, be;
	double s, b, sp, bp;
	double sr, sb, sl, er, eb, el;

	t = (JD-2451545.)/365250.;
	i = degrad(28.04922-.13*t+.0004*t*t);
	om = degrad(169.53+13.826*t+.04*t*t);

	sb = sop->s_hlat;
	sl = sop->s_hlong;
	sr = sop->s_sdist;

	eb = 0.0;
	el = eop->s_hlong;
	er = eop->s_edist;

	x = sr*cos(sb)*cos(sl)-er*cos(el);
	y = sr*cos(sb)*sin(sl)-er*sin(el);
	z = sr*sin(sb)-er*sin(eb);

	la = atan(y/x);
	if (x<0) la+=PI;
	be = atan(z/sqrt(x*x+y*y));

	s = sin(i)*cos(be)*sin(la-om)-cos(i)*sin(be);
	b = atan(s/sqrt(1.-s*s));
	sp = sin(i)*cos(sb)*sin(sl-om)-cos(i)*sin(sb);
	bp = atan(sp/sqrt(1.-sp*sp));

	*etiltp = b;
	*stiltp = bp;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: satmenu.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
