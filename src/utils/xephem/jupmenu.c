/* code to manage the stuff on the "jupiter" menu.
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#if defined(__STDC__)
#include <stdlib.h>
#endif

#if defined(_POSIX_SOURCE)
#include <unistd.h>
#endif

#ifndef SEEK_END
#define	SEEK_END 2
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

extern FILE *fopenh P_((char *name, char *how));
extern Now *mm_get_now P_((void));
extern Obj *db_basic P_((int id));
extern Obj *db_scan P_((DBScan *sp));
extern XFontStruct * getXResFont P_((char *rn));
extern char *getShareDir P_((void));
extern char *syserrstr P_((void));
extern double atod P_((char *buf));
extern double delra P_((double dra));
extern int any_ison P_((void));
extern int fs_fetch P_((Now *np, double ra, double dec, double fov,
    double mag, ObjF **opp));
extern int get_color_resource P_((Widget w, char *cname, Pixel *p));
extern int gif2X P_((Display *dsp, Colormap cm, unsigned char gif[], int ngif,
    int *wp, int *hp, unsigned char **gifpix, XColor xcols[256], char err[]));
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
extern void freeXColors P_((Display *dsp, Colormap cm, XColor xcols[],
    int nxcols));
extern void fs_manage P_((void));
extern void fs_prdec P_((char out[], double a));
extern void fs_ra P_((char out[], double ra));
extern void get_something P_((Widget w, char *resource, XtArgVal value));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void jm_update P_((Now *np, int how_much));
extern void mm_movie P_((double stepsz));
extern void obj_pickgc P_((Obj *op, Widget w, GC *gcp));
extern void register_selection P_((char *name));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void sv_draw_obj P_((Display *dsp, Drawable win, GC gc, Obj *op, int x,
    int y, int diam, int dotsonly));
extern void timestamp P_((Now *np, Widget w));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *buf, int app_modal));

#define	MINR	10	/* min distance for picking, pixels */

/* These values are from the Explanatory Supplement.
 * Precession degrades them gradually over time.
 */
#define	POLE_RA		degrad(268.05)	/* RA of Jupiter's north pole */
#define	POLE_DEC	degrad(64.50)	/* Dec of Jupiter's north pole */

/* local record of what is now on screen for easy id from mouse picking.
 */
typedef struct {
    Obj o;		/* copy of object info.
    			 * copy from DB or, if jupiter moon, we fill in just
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

static void jm_create_shell_w P_((void));
static void jm_create_tvform_w P_((void));
static void jm_create_jsform_w P_((void));
static void jm_set_buttons P_((int whether));
static void jm_sstats_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void jm_sstats_cb P_((Widget w, XtPointer client, XtPointer call));
static void jm_option_cb P_((Widget w, XtPointer client, XtPointer call));
static void jm_scale_cb P_((Widget w, XtPointer client, XtPointer call));
static void jm_activate_cb P_((Widget w, XtPointer client, XtPointer call));
static void jt_unmap_cb P_((Widget w, XtPointer client, XtPointer call));
static void jt_map_cb P_((Widget w, XtPointer client, XtPointer call));
static void jt_track_size P_((void));
static void jt_da_exp_cb P_((Widget w, XtPointer client, XtPointer call));
static void jm_popdown_cb P_((Widget w, XtPointer client, XtPointer call));
static void jm_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void jm_movie_cb P_((Widget w, XtPointer client, XtPointer call));
static void jm_da_exp_cb P_((Widget w, XtPointer client, XtPointer call));
static void jm_da_input_cb P_((Widget w, XtPointer client, XtPointer call));
static void jm_create_popup P_((void));
static void jm_fill_popup P_((ScreenObj *sop));
static void jm_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void jm_helpon_cb P_((Widget w, XtPointer client, XtPointer call));
static void jm_locfifo_cb P_((Widget w, XtPointer client, XtPointer call));
static void jm_grsl_cb P_((Widget w, XtPointer client, XtPointer call));
static void j_calibline P_((Display *dsp, Drawable win, GC gc, int xc, int yc,
    char *tag, int tw, int th, int l));

static void add_screenobj P_((SOIdx, Obj *op, int x, int y));
static void reset_screenobj P_((SOIdx));
static ScreenObj *close_screenobj P_((SOIdx, int x, int y));

static void sky_background P_((Drawable win, unsigned w, unsigned h, int fmag,
    double ra0, double dec0, double scale, double rad, int fliptb, int fliplr));

static Widget jupshell_w;	/* main shell */
static Widget jsform_w;		/* statistics form */
static Widget jmframe_w;	/* main frame */
static Widget jtform_w;		/* top-view form */
static Widget jtframe_w;	/* top-view frame */
static Widget jda_w;		/* main drawing area */
static Widget jtda_w;		/* top-view drawing area */
static Widget cmlI_w, cmlII_w;	/* labels for displaying GRS coords */
static Widget scale_w;		/* size scale */
static Widget limmag_w;		/* limiting magnitude scale */
static Widget dt_w;		/* main date/time stamp widget */
static Widget sdt_w;		/* statistics date/time stamp widget */
static Widget skybkg_w;		/* toggle for controlling sky background */
static Widget topview_w;	/* toggle for controlling top view */
static Widget grsl_w;		/* TF of GRS Sys II longitude, degrees */
static Pixmap jm_pm;		/* main pixmap */
static Pixmap jt_pm;		/* top-view pixmap */
static GC j_fgc, j_bgc, j_xgc;	/* various colors and operators */
static XFontStruct *j_fs;	/* font for labels */
static int j_cw, j_ch;		/* size of label font */
static int jm_selecting;	/* set while our fields are being selected */
static int brmoons;		/* whether we want to brightten the moons */
static int tags;		/* whether we want tags on the drawing */
static int flip_tb;		/* whether we want to flip top/bottom */
static int flip_lr;		/* whether we want to flip left/right */
static int skybkg;		/* whether we want sky background */
static int topview;		/* whether we want the top view */

static Widget jmpu_w;		/* main popup */
static Widget jmpu_name_w;	/* popup name label */
static Widget jmpu_ra_w;	/* popup RA label */
static Widget jmpu_dec_w;	/* popup Dec label */
static Widget jmpu_mag_w;	/* popup Mag label */

#define	MAXSCALE	10.0	/* max scale mag factor */
#define	MOVIE_STEPSZ	0.25	/* movie step size, hours */

/* field star support */
static ObjF *fstars;		/* malloced list of field stars, or NULL */
static int nfstars;		/* number of entries in fstars[] */
static double fsdec, fsra;	/* location when field stars were loaded */
#define	FSFOV	degrad(1.0)	/* size of FOV to fetch, rads */
#define	FSMAG	20.0		/* limiting mag for fetch */
#define	FSMOVE	degrad(.2)	/* reload when jup has moved this far, rads */
static void jm_loadfs P_((double ra, double dec));

/* arrays that use NM use [0] for the planet itself, and [1..NM-1] for moons.
 */
#define	NM	5		/* number of moons + 1 for planet */
static struct MoonNames {
    char *full;
    char *tag;
} mnames[NM] = {
    {"Jupiter", NULL},
    {"Io", "I"},
    {"Europa", "II"},
    {"Ganymede", "III"},
    {"Callisto", "IV"}
};

enum {CEV, CSV, CX, CY, CZ, CRA, CDEC, CMAG, CNum};	/* j_w column index */
static Widget	j_w[NM][CNum];			/* the data display widgets */

static char jupcategory[] = "Jupiter";		/* Save category */

/* info about the gif we use to morph an image */
static char jmapbasenm[] = "jupitermap.gif";	/* base file name */
static XColor jmapxcols[256];			/* pixels and colors */
static unsigned char *jmappix;			/* malloced wxh of gif pixels */
#define	JMAPGRSX	125			/* gif x of GRS */
#define	JMAPW		512			/* gif width */
#define	JMAPH		256			/* gif height */
static Pixel jbg_p;				/* background color */

/* called when the jupiter menu is activated via the main menu pulldown.
 * if never called before, create and manage all the widgets as a child of a
 * form. otherwise, just get out there and do it!
 */
void
jm_manage ()
{
	if (!jupshell_w) {
	    jm_create_shell_w();
	    jm_create_jsform_w();
	    jm_create_tvform_w();
	}
	
	XtPopup (jupshell_w, XtGrabNone);
	set_something (jupshell_w, XmNiconic, (XtArgVal)False);
	jm_set_buttons(jm_selecting);
}

/* called when the database has changed.
 * if we are drawing background, we'd best redraw everything.
 */
/* ARGSUSED */
void
jm_newdb (appended)
int appended;
{
	if (skybkg)
	    jm_update (mm_get_now(), 1);
}

int
jm_ison()
{
	return (isUp(jupshell_w));
}

/* called by other menus as they want to hear from our buttons or not.
 * the "on"s and "off"s stack - only really redo the buttons if it's the
 * first on or the last off.
 */
void
jm_selection_mode (whether)
int whether;	/* whether setting up for plotting or for not plotting */
{
	if (whether)
	    jm_selecting++;
	else if (jm_selecting > 0)
	    --jm_selecting;

	if (jupshell_w)
	    if ((whether && jm_selecting == 1)     /* first one to want on */
		|| (!whether && jm_selecting == 0) /* last one to want off */)
		jm_set_buttons (whether);
}

/* called to put up or remove the watch cursor.  */
void
jm_cursor (c)
Cursor c;
{
	Window win;

	if (jupshell_w && (win = XtWindow(jupshell_w)) != 0) {
	    Display *dsp = XtDisplay(jupshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}

	if (jsform_w && (win = XtWindow(jsform_w)) != 0) {
	    Display *dsp = XtDisplay(jsform_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}

	if (jtform_w && (win = XtWindow(jtform_w)) != 0) {
	    Display *dsp = XtDisplay(jtform_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* create the main shell */
static void
jm_create_shell_w()
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
	    {"Intro...",	"Jupiter - intro"},
	    {"on Mouse...",	"Jupiter - mouse"},
	    {"on Control...",	"Jupiter - control"},
	    {"on View...",	"Jupiter - view"},
	};
	Widget w;
	Widget mb_w, pd_w, cb_w;
	Widget jupform_w;
	XmString str;
	Arg args[20];
	int n;
	int i;

	/* create shell and form */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Jupiter view"); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNiconName, "Jupiter"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	jupshell_w = XtCreatePopupShell ("Jupiter", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (jupshell_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (jupshell_w, XmNpopdownCallback, jm_popdown_cb, 0);
	sr_reg (jupshell_w, "XEphem*Jupiter.width", jupcategory, 0);
	sr_reg (jupshell_w, "XEphem*Jupiter.height", jupcategory, 0);
	sr_reg (jupshell_w, "XEphem*Jupiter.x", jupcategory, 0);
	sr_reg (jupshell_w, "XEphem*Jupiter.y", jupcategory, 0);

	n = 0;
	jupform_w = XmCreateForm (jupshell_w, "JupiterF", args, n);
	XtAddCallback (jupform_w, XmNhelpCallback, jm_help_cb, 0);
	XtManageChild (jupform_w);

	/* create the menu bar across the top */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	mb_w = XmCreateMenuBar (jupform_w, "MB", args, n);
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
	    XtAddCallback (w, XmNactivateCallback, jm_locfifo_cb, 0);
	    wtip (w, "Open export connection");
	    XtManageChild (w);

	    /* add the "movie" push button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Movie", args, n);
	    set_xmstring (w, XmNlabelString, "Movie Demo");
	    XtAddCallback (w, XmNactivateCallback, jm_movie_cb, 0);
	    wtip (w, "Start/Stop a demonstration animation sequence");
	    XtManageChild (w);

	    /* add the "close" push button beneath a separator */

	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, jm_close_cb, 0);
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
		w = XmCreateToggleButton (pd_w, op->name ? op->name : "JMTB",
								    args, n);
		XtAddCallback (w, XmNvalueChangedCallback, jm_option_cb, 
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
		    sr_reg (w, NULL, jupcategory, 1);
	    }

	    /* add a separator */

	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    /* add the More Info control */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Stats", args, n);
	    set_xmstring (w, XmNlabelString, "More info...");
	    XtAddCallback (w, XmNactivateCallback, jm_sstats_cb, NULL);
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
		XtAddCallback (w, XmNactivateCallback, jm_helpon_cb,
							(XtPointer)(hp->key));
		XtManageChild (w);
		XmStringFree(str);
	    }

	/* make the date/time stamp label */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	dt_w = XmCreateLabel (jupform_w, "DateStamp", args, n);
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
	scale_w = XmCreateScale (jupform_w, "Scale", args, n);
	XtAddCallback (scale_w, XmNdragCallback, jm_scale_cb, 0);
	XtAddCallback (scale_w, XmNvalueChangedCallback, jm_scale_cb, 0);
	wtip (scale_w, "Zoom in and out");
	XtManageChild (scale_w);
	sr_reg (scale_w, NULL, jupcategory, 0);

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
	limmag_w = XmCreateScale (jupform_w, "LimMag", args, n);
	XtAddCallback (limmag_w, XmNdragCallback, jm_scale_cb, 0);
	XtAddCallback (limmag_w, XmNvalueChangedCallback, jm_scale_cb, 0);
	wtip (limmag_w, "Adjust the brightness and limiting magnitude");
	XtManageChild (limmag_w);
	sr_reg (limmag_w, NULL, jupcategory, 0);

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
	jmframe_w = XmCreateFrame (jupform_w, "JupFrame", args, n);
	XtManageChild (jmframe_w);

	    /* make a drawing area for drawing the little map */

	    n = 0;
	    jda_w = XmCreateDrawingArea (jmframe_w, "Map", args, n);
	    XtAddCallback (jda_w, XmNexposeCallback, jm_da_exp_cb, 0);
	    XtAddCallback (jda_w, XmNinputCallback, jm_da_input_cb, 
	    						(XtPointer)SO_MAIN);
	    XtManageChild (jda_w);
}

/* make the statistics form dialog */
static void
jm_create_jsform_w()
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
	    {CX,   "JupX",   "X (+E)",     "X",
		"Apparent displacement east of planetary center"},
	    {CY,   "JupY",   "Y (+S)",     "Y",
		"Apparent displacement south of planetary center"},
	    {CZ,   "JupZ",   "Z (+front)", "Z",
		"Jupiter radii towards Earth from planetary center"},
	    {CRA,  "JupRA",  "RA",         "RA",
		"Right Ascension (to Main's settings)"},
	    {CDEC, "JupDec", "Dec",        "Dec",
		"Declination (to Main's settings)"},
	    {CMAG, "JupMag", "Mag",        "Mag",
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
	jsform_w = XmCreateFormDialog (jupshell_w, "JupiterStats", args, n);
	set_something (jsform_w, XmNcolormap, (XtArgVal)xe_cm);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Jupiter info"); n++;
	XtSetValues (XtParent(jsform_w), args, n);

	/* make CML title label */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	title_w = XmCreateLabel (jsform_w, "JupTL", args, n);
	XtManageChild (title_w);
	set_xmstring (title_w, XmNlabelString,
					"Central Meridian Longitudes (degs):");

	/* make the Sys I r/c */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, title_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNnumColumns, 1); n++;
	rc_w = XmCreateRowColumn (jsform_w, "IRC", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "JupCMLIMsg", args, n);
	    set_xmstring (w, XmNlabelString, "Sys I:");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNuserData, "Jupiter.CMLI"); n++;
	    cmlI_w = XmCreatePushButton (rc_w, "JupCMLI", args, n);
	    XtAddCallback (cmlI_w, XmNactivateCallback, jm_activate_cb, 0);
	    wtip (cmlI_w, "Longitude currently facing Earth, in system I coordinates");
	    XtManageChild (cmlI_w);

	/* make the Sys II r/c */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, title_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNnumColumns, 1); n++;
	rc_w = XmCreateRowColumn (jsform_w, "IIRC", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "JupCMLIIMsg", args, n);
	    set_xmstring (w, XmNlabelString, "Sys II:");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNuserData, "Jupiter.CMLII"); n++;
	    cmlII_w = XmCreatePushButton (rc_w, "JupCMLII", args, n);
	    XtAddCallback (cmlII_w, XmNactivateCallback, jm_activate_cb, 0);
	    wtip (cmlII_w, "Longitude currently facing Earth, in system II coordinates");
	    XtManageChild (cmlII_w);

	/* make the GRS/CML prompt and TF */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNtopOffset, 5); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 48); n++;
	w = XmCreateLabel (jsform_w, "GRSCMLL", args, n);
	XtManageChild (w);
	set_xmstring (w, XmNlabelString, "GRS Sys II Long:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNtopOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 52); n++;
	XtSetArg (args[n], XmNcolumns, 10); n++;
	grsl_w = XmCreateTextField (jsform_w, "GRSSysIILong", args, n);
	XtAddCallback (grsl_w, XmNactivateCallback, jm_grsl_cb, NULL);
	XtManageChild (grsl_w);
	sr_reg (grsl_w, NULL, jupcategory, 1);

	/* make table title label */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, grsl_w); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	title_w = XmCreateLabel (jsform_w, "JupML", args, n);
	XtManageChild (title_w);
	set_xmstring (title_w, XmNlabelString,"Moon Positions (Jupiter radii)");

	/* make the moon table, one column at a time */

	/* moon designator column */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, title_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNisAligned, True); n++;
	rc_w = XmCreateRowColumn (jsform_w, "JupDes", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "TL", args, n);
	    set_xmstring (w, XmNlabelString, "Tag");
	    wtip (w, "Roman Numeral sequence designation of moon");
	    XtManageChild (w);

	    for (i = 0; i < NM; i++) {
		char *tag = mnames[i].tag;

		n = 0;
		w = XmCreatePushButton (rc_w, "JupTag", args, n); /*PB for sz */
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
	rc_w = XmCreateRowColumn (jsform_w, "JupName", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "NL", args, n);
	    set_xmstring (w, XmNlabelString, "Name");
	    wtip (w, "Common name of body");
	    XtManageChild (w);

	    for (i = 0; i < NM; i++) {
		n = 0;
		w = XmCreatePushButton (rc_w, "JupName", args, n); /*PB for sz*/
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
	    rc_w = XmCreateRowColumn (jsform_w, mp->rcname, args, n);
	    XtManageChild (rc_w);

	    n = 0;
	    w = XmCreateLabel (rc_w, "JupLab", args, n);
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
		w = XmCreatePushButton(rc_w, "JupPB", args, n);
		XtAddCallback(w, XmNactivateCallback, jm_activate_cb, 0);
		XtManageChild (w);
		j_w[j][mp->col] = w;
	    }
	}

	/* make a separator */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (jsform_w, "Sep1", args, n);
	XtManageChild (sep_w);

	/* make a date/time stamp */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sdt_w = XmCreateLabel (jsform_w, "JDateStamp", args, n);
	wtip (sdt_w, "Date and Time for which data are computed");
	XtManageChild (sdt_w);

	/* make a separator */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sdt_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (jsform_w, "Sep2", args, n);
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
	w = XmCreatePushButton (jsform_w, "Close", args, n);
	XtAddCallback (w, XmNactivateCallback, jm_sstats_close_cb, 0);
	wtip (w, "Close this dialog");
	XtManageChild (w);
}

/* create jtform_w, the top view dialog */
static void
jm_create_tvform_w()
{
	Arg args[20];
	int n;

	/* create form */
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNdefaultPosition, False); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	jtform_w = XmCreateFormDialog (jupshell_w, "JupiterTV", args, n);
	set_something (jtform_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (jtform_w, XmNunmapCallback, jt_unmap_cb, 0);
	XtAddCallback (jtform_w, XmNmapCallback, jt_map_cb, NULL);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Jupiter top view"); n++;
	XtSetValues (XtParent(jtform_w), args, n);

	/* fill with a drawing area in a frame */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	jtframe_w = XmCreateFrame (jtform_w, "JTVF", args, n);
	XtManageChild (jtframe_w);

	    n = 0;
	    jtda_w = XmCreateDrawingArea (jtframe_w, "Map", args, n);
	    XtAddCallback (jtda_w, XmNexposeCallback, jt_da_exp_cb, NULL);
	    XtAddCallback (jtda_w, XmNinputCallback, jm_da_input_cb, 
							(XtPointer)SO_TOP);
	    XtManageChild (jtda_w);
}

/* go through all the buttons pickable for plotting and set whether they
 * should appear to look like buttons or just flat labels.
 */
static void
jm_set_buttons (whether)
int whether;	/* whether setting up for plotting or for not plotting */
{
	int i, j;

	for (i = 0; i < NM; i++) {
	    for (j = 0; j < CNum; j++)
		buttonAsButton (j_w[i][j], whether);
	}
	buttonAsButton (cmlI_w, whether);
	buttonAsButton (cmlII_w, whether);
}

/* callback when the ENTER key is typed on the GRS Long TF */
/* ARGSUSED */
static void
jm_grsl_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	jm_update (mm_get_now(), 1);
}

/* callback when the Close button is activated on the stats menu */
/* ARGSUSED */
static void
jm_sstats_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (jsform_w);
}

/* callback when the More Info button is activated */
/* ARGSUSED */
static void
jm_sstats_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtManageChild (jsform_w);
	jm_set_buttons(jm_selecting);
}

/* callback from any of the option buttons.
 * client points to global flag to set; some don't have any.
 * in any case then just redraw everything.
 */
/* ARGSUSED */
static void
jm_option_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (client) {
	    int *flagp = (int *)client;
	    *flagp = XmToggleButtonGetState(w);
	    if (flagp == &topview) {
		if (topview) {
		    if (!jtform_w)
			jm_create_tvform_w();
		    XtManageChild (jtform_w);
		} else
		    XtUnmanageChild (jtform_w);
	    }
	}

	jm_update (mm_get_now(), 1);
}

/* callback from the scales changing.
 * just redraw everything.
 */
/* ARGSUSED */
static void
jm_scale_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	jm_update (mm_get_now(), 1);
}

/* callback from any of the data menu buttons being activated.
 */
/* ARGSUSED */
static void
jm_activate_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (jm_selecting) {
	    char *name;
	    get_something (w, XmNuserData, (XtArgVal)&name);
	    register_selection (name);
	}
}

/* callback from either expose or resize of the topview.
 */
/* ARGSUSED */
static void
jt_da_exp_cb (w, client, call)
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
	    printf ("Unexpected jupform_w event. type=%d\n", c->reason);
	    exit(1);
	}

        XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);
	if (!jt_pm || (int)nx != last_nx || (int)ny != last_ny) {
	    if (jt_pm)
		XFreePixmap (dsp, jt_pm);
	    jt_pm = XCreatePixmap (dsp, win, nx, ny, d);
	    last_nx = nx;
	    last_ny = ny;
	    jt_track_size();
	    jm_update (mm_get_now(), 1);
	} else
	    XCopyArea (dsp, jt_pm, win, j_fgc, 0, 0, nx, ny, 0, 0);
}

/* called whenever the topview scene is mapped. */
/* ARGSUSED */
static void
jt_map_cb (wid, client, call)
Widget wid;
XtPointer client;
XtPointer call;
{
	jt_track_size();
}

/* set the width of the topview DrawingArea the same as the main window's.
 * we also try to center it just above, but it doesn't always work.
 */
static void
jt_track_size()
{
	Dimension w, h;
	Position mfx, mfy, mdx, mdy;
	Position sdy;
	Arg args[20];
	int n;

	/* set widths equal */
	n = 0;
	XtSetArg (args[n], XmNwidth, &w); n++;
	XtGetValues (jda_w, args, n);

	n = 0;
	XtSetArg (args[n], XmNwidth, w); n++;
	XtSetValues (jtda_w, args, n);

	n = 0;
	XtSetArg (args[n], XmNheight, &h); n++;
	XtGetValues (jtda_w, args, n);

	/* set locations -- allow for different stuff on top of drawingareas */
	n = 0;
	XtSetArg (args[n], XmNx, &mfx); n++;
	XtSetArg (args[n], XmNy, &mfy); n++;
	XtGetValues (jupshell_w, args, n);
	n = 0;
	XtSetArg (args[n], XmNx, &mdx); n++;
	XtSetArg (args[n], XmNy, &mdy); n++;
	XtGetValues (jmframe_w, args, n);
	n = 0;
	XtSetArg (args[n], XmNy, &sdy); n++;
	XtGetValues (jtframe_w, args, n);

	n = 0;
	XtSetArg (args[n], XmNx, mfx+mdx); n++;
	XtSetArg (args[n], XmNy, mfy - h - sdy - mdy - 20); n++;
	XtSetValues (jtform_w, args, n);
}

/* callback when topview dialog is unmapped */
/* ARGSUSED */
static void
jt_unmap_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonSetState (topview_w, False, True);
}

/* callback when main shell is popped down */
/* ARGSUSED */
static void
jm_popdown_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (jsform_w);
	XtUnmanageChild (jtform_w);

	/* free pixmap */
        if (jm_pm) {
	    XFreePixmap (XtDisplay(jda_w), jm_pm);
	    jm_pm = (Pixmap) NULL;
	}

	/* free image info */
	if (jmappix) {
	    free ((char *)jmappix);
	    jmappix = 0;
	    freeXColors (XtD, xe_cm, jmapxcols, 256);
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
jm_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* let popdown do all the work */
	XtPopdown (jupshell_w);
}

/* callback from the Movie button
 */
/* ARGSUSED */
static void
jm_movie_cb (w, client, call)
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
jm_da_exp_cb (w, client, call)
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
	    printf ("Unexpected jupform_w event. type=%d\n", c->reason);
	    exit(1);
	}

	XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);
	if (!jm_pm || (int)nx != last_nx || (int)ny != last_ny) {
	    if (jm_pm)
		XFreePixmap (dsp, jm_pm);
	    jm_pm = XCreatePixmap (dsp, win, nx, ny, d);
	    last_nx = nx;
	    last_ny = ny;
	    if (topview)
		jt_track_size();
	    jm_update (mm_get_now(), 1);
	} else
	    XCopyArea (dsp, jm_pm, win, j_fgc, 0, 0, nx, ny, 0, 0);
}

/* callback from mouse or keyboard input over either drawing area.
 * client is one of SOIdx.
 */
/* ARGSUSED */
static void
jm_da_input_cb (w, client, call)
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

	/* put info in popup, jmpu_w, creating it first if necessary  */
	if (!jmpu_w)
	    jm_create_popup();
	jm_fill_popup (sop);

	/* put it on screen */
	XmMenuPosition (jmpu_w, (XButtonPressedEvent *)ev);
	XtManageChild (jmpu_w);
}

/* create the (unmanaged for now) popup menu in jmpu_w. */
static void
jm_create_popup()
{
	static Widget *puw[] = {
	    &jmpu_name_w,
	    &jmpu_ra_w,
	    &jmpu_dec_w,
	    &jmpu_mag_w,
	};
	Widget w;
	Arg args[20];
	int n;
	int i;

	n = 0;
	XtSetArg (args[n], XmNisAligned, True); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	jmpu_w = XmCreatePopupMenu (jda_w, "JupPU", args, n);

	/* stack everything up in labels */
	for (i = 0; i < XtNumber(puw); i++) {
	    n = 0;
	    w = XmCreateLabel (jmpu_w, "SPUL", args, n);
	    XtManageChild (w);
	    *puw[i] = w;
	}
}

/* put up a popup at ev with info about sop */
static void
jm_fill_popup (sop)
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
	set_xmstring (jmpu_name_w, XmNlabelString, buf2);

	fs_ra (buf, ra);
	(void) sprintf (buf2, "  RA: %s", buf);
	set_xmstring (jmpu_ra_w, XmNlabelString, buf2);

	fs_prdec (buf, dec);
	(void) sprintf (buf2, " Dec: %s", buf);
	set_xmstring (jmpu_dec_w, XmNlabelString, buf2);

	(void) sprintf (buf2, " Mag: %.3g", mag);
	set_xmstring (jmpu_mag_w, XmNlabelString, buf2);
}

/* callback from the Help all button
 */
/* ARGSUSED */
static void
jm_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg[] = {
"This is a simple schematic depiction of Jupiter and its moons.",
};

	hlp_dialog ("Jupiter", msg, XtNumber(msg));
}

/* callback from a specific Help button.
 * client is a string to use with hlp_dialog().
 */
/* ARGSUSED */
static void
jm_helpon_cb (w, client, call)
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
jm_locfifo_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Obj *op = db_basic (JUPITER);
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
static void jupiter_data();
static void moonradec();
static void jm_draw_map ();
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

/* called to recompute and fill in values for the jupiter menu.
 * don't bother if it doesn't exist or is unmanaged now or no one is logging.
 */
void
jm_update (np, how_much)
Now *np;
int how_much;
{
	static char fmt[] = "%7.3f";
	Obj *jop = db_basic (JUPITER);
	MoonData md[NM];
	int wantstats;
	double cmlI, cmlII;
	double jupsize;
	int i;

	/* see if we should bother */
	if (!jupshell_w)
	    return;
	wantstats = XtIsManaged(jsform_w) || any_ison() || how_much;
	if (!isUp(jupshell_w) && !wantstats)
	    return;

	watch_cursor (1);

        /* compute md[0].x/y/z/mag/ra/dec and md[1..NM-1].x/y/z/mag info */
        jupiter_data(mjd, jop, &jupsize, &cmlI, &cmlII, md);

	/* set md[0..NM-1].evis */
	md[0].evis = 1;
	for (i = 1; i < NM; i++)
	    moonEVis (&md[i]);

	/* set md[0..NM-1].svis */
	md[0].svis = 1;
	for (i = 1; i < NM; i++)
	    moonSVis (jop, &md[i]);

        /* find md[1..NM-1].ra/dec from md[0].ra/dec and md[1..NM-1].x/y */
	moonradec (jupsize, md);

	if (wantstats) {
	    for (i = 0; i < NM; i++) {
		f_double (j_w[i][CEV], "%1.0f", (double)md[i].evis);
		f_double (j_w[i][CSV], "%1.0f", (double)md[i].svis);
		f_double (j_w[i][CX], fmt, md[i].x);
		f_double (j_w[i][CY], fmt, md[i].y);
		f_double (j_w[i][CZ], fmt, md[i].z);
		f_double (j_w[i][CMAG], "%4.1f", md[i].mag);
		f_ra (j_w[i][CRA], md[i].ra);
		f_prdec (j_w[i][CDEC], md[i].dec);
	    }

	    f_double (cmlI_w, fmt, raddeg(cmlI));
	    f_double (cmlII_w, fmt, raddeg(cmlII));
	    timestamp (np, sdt_w);
	}

	if (isUp(jupshell_w)) {
	    jm_draw_map (jda_w, jop, jupsize, cmlII, md);
	    timestamp (np, dt_w);
	}

	watch_cursor (0);
}

/* called when basic resources change.
 * rebuild and redraw.
 */
void
jm_newres()
{
	if (!jupshell_w)
	    return;
	make_gcs (jda_w);
	jm_update (mm_get_now(), 1);
}

/* read the jupiter map gif.
 * harmless if called again.
 * return 0 if ok else -1.
 */
static int
jm_getgif(dsp)
Display *dsp;
{
	unsigned char *gif;
	int ngif;
	char fn[256];
	char buf[256];
	int w, h;
	FILE *fp;

	/* benign if called again */
	if (jmappix)
	    return (0);

	/* read in the gif */
	sprintf (fn, "%s/auxil/%s",  getShareDir(), jmapbasenm);
	fp = fopenh (fn, "r");
	if (!fp) {
	    sprintf (buf, "%s:\n%s", fn, syserrstr());
	    xe_msg (buf, 1);
	    return (-1);
	}
	if (fseek (fp, 0L, SEEK_END) < 0) {
	    sprintf (buf, "%s:\nCan not seek", jmapbasenm);
	    xe_msg (buf, 1);
	    fclose (fp);
	    return (-1);
	}
	ngif = ftell (fp);
	gif = XtMalloc (ngif);
	rewind (fp);
	if (fread (gif, ngif, 1, fp) != 1) {
	    sprintf (buf, "%s:\n%s", jmapbasenm, syserrstr());
	    xe_msg (buf, 1);
	    fclose (fp);
	    return (-1);
	}
	fclose (fp);

	/* explode */
	if (gif2X (dsp, xe_cm, gif, ngif, &w, &h, &jmappix,
						    jmapxcols, buf) < 0) {
	    sprintf (fn, "%s:\n%s", jmapbasenm, buf);
	    xe_msg (fn, 1);
	    return (-1);
	}

	if (w != JMAPW || h != JMAPH) {
	    sprintf (buf, "%s:\nincorrect file size", jmapbasenm);
	    xe_msg (buf, 1);
	    return (-1);
	}

	return (0);
}

/* fill win with front image of Jupiter at the given location, size and
 * rotation.
 * factoids: default orientation is E on right, S on top. So, GRS moves from
 *   right to left. Spot is in S hemisphere. CML increases with time.
 */
static void
jm_fimage (dsp, win, lx, ty, sz, grs)
Display *dsp;		/* Display */
Drawable win;		/* drawable */
int lx, ty, sz;		/* left x, top y, size */
double grs;		/* angle from center to grs, rads */
{
	Obj *jop = db_basic (JUPITER);
	XImage *xip;
	int depth;
	int bpp;
	int nbytes;
	char *data;
	int x0, szr, szr2, x, y;
	double tvc, pvc, theta, phi, sa, ca;

	/* get the gif */
	if (jm_getgif(dsp) < 0)
	    return;

	/* create a working image */
	get_something (jda_w, XmNdepth, (XtArgVal)&depth);
	bpp = depth>=17 ? 32 : (depth >= 9 ? 16 : 8);
	nbytes = sz*sz*bpp/8;
	data = XtMalloc (nbytes);
	xip = XCreateImage (dsp, DefaultVisual (dsp, DefaultScreen(dsp)),
	    /* depth */         depth,
	    /* format */        ZPixmap,
	    /* offset */        0,
	    /* data */          data,
	    /* width */         sz,
	    /* height */        sz,
	    /* pad */           bpp,
	    /* bpl */           0);
	xip->bitmap_bit_order = LSBFirst;
	xip->byte_order = LSBFirst;

	/* sky-plane rotation */
	tvc = PI/2.0 - jop->s_dec;
	pvc = jop->s_ra;
	theta = PI/2.0 - POLE_DEC;
	phi = POLE_RA;
	sa = sin(tvc)*sin(theta)*(cos(pvc)*sin(phi) - sin(pvc)*cos(phi));
	ca = sqrt (1.0 - sa*sa);

	/* morph. include effects of sky plane rotation,
	 * x,y limb foreshortening and y latitude stretch.
	 * TODO: limb darkening
	 */
	x0 = JMAPGRSX + (int)(grs/(2*PI)*JMAPW) + JMAPW;
	szr = sz/2;
	szr2 = szr*szr;
	for (y = 0; y < sz; y++) {
	    int imy = flip_tb ? y - szr : szr - y;
	    for (x = 0; x < sz; x++) {
		int imx = flip_lr ? szr - x : x - szr;
		double imxp =  imx*ca + imy*sa;
		double imyp = -imx*sa + imy*ca;
		double imy2 = imyp*imyp;
		double imr2 = imxp*imxp + imy2;
		unsigned long p;

		if (imr2 >= szr2)
		    p = jbg_p;
		else {
		    double xmult = JMAPW*szr/((2*PI)*sqrt((double)szr2 - imy2));
		    int xg = (x0 - (int)(xmult*asin((double)imx/szr)))%JMAPW;
		    int yg = (3*JMAPH/2 +
				(int)(JMAPH*asin((double)imyp/szr)/PI))%JMAPH;
		    p = jmapxcols[(int)jmappix[yg*JMAPW+xg]].pixel;
		}
		XPutPixel (xip, x, y, p);
	    }
	}

	/* copy to win */
	XPutImage (dsp, win, j_fgc, xip, 0, 0, lx, ty, sz, sz);

	/* done */
	free ((void *)xip->data);
	xip->data = NULL;
	XDestroyImage (xip);
}

/* fill win with top image of Jupiter at the given location, size and
 * rotation.
 */
static void
jm_timage (dsp, win, lx, ty, sz, grs)
Display *dsp;		/* Display */
Drawable win;		/* drawable */
int lx, ty, sz;		/* left x, top y, size */
double grs;		/* angle from center to grs, rads */
{
	Obj *jop = db_basic (JUPITER);
	XImage *xip;
	int depth;
	int bpp;
	int nbytes;
	char *data;
	int x0, szr, szr2, x, y;
	double xmult, ymult;
	double tvc, pvc, theta, phi, sa, ca;

	/* get the gif */
	if (jm_getgif(dsp) < 0)
	    return;

	/* create a working image */
	get_something (jda_w, XmNdepth, (XtArgVal)&depth);
	bpp = depth>=17 ? 32 : (depth >= 9 ? 16 : 8);
	nbytes = sz*sz*bpp/8;
	data = XtMalloc (nbytes);
	xip = XCreateImage (dsp, DefaultVisual (dsp, DefaultScreen(dsp)),
	    /* depth */         depth,
	    /* format */        ZPixmap,
	    /* offset */        0,
	    /* data */          data,
	    /* width */         sz,
	    /* height */        sz,
	    /* pad */           bpp,
	    /* bpl */           0);
	xip->bitmap_bit_order = LSBFirst;
	xip->byte_order = LSBFirst;

	/* sky-plane rotation */
	tvc = PI/2.0 - jop->s_dec;
	pvc = jop->s_ra;
	theta = PI/2.0 - POLE_DEC;
	phi = POLE_RA;
	sa = sin(tvc)*sin(theta)*(cos(pvc)*sin(phi) - sin(pvc)*cos(phi));
	ca = sqrt (1.0 - sa*sa);

	/* morph.
	 * TODO: limb darkening, tilt
	 */
	x0 = JMAPGRSX + (int)(grs/(2*PI)*JMAPW) + JMAPW;
	szr = sz/2;
	szr2 = sz*sz/4;
	xmult = JMAPW/(2*PI);
	ymult = (double)JMAPH/sz;
	for (y = 0; y < sz; y++) {
	    double imy = szr - y;
	    double imy2 = imy*imy;
	    for (x = 0; x < sz; x++) {
		double imx = flip_lr ? szr - x : x - szr;
		double imr2 = imx*imx + imy2;
		unsigned long p;

		if (imr2 > szr2)
		    p = jbg_p;
		else {
		    int xg = (x0 - (int)(xmult*atan2(imx,-imy)))%JMAPW;
		    int yg = (int)(ymult*sqrt(imr2));
		    if (!flip_tb)
			yg = JMAPH - yg;
		    p = jmapxcols[(int)jmappix[yg*JMAPW+xg]].pixel;
		}
		XPutPixel (xip, x, y, p);
	    }
	}

	/* copy to win */
	XPutImage (dsp, win, j_fgc, xip, 0, 0, lx, ty, sz, sz);

	/* done */
	free ((void *)xip->data);
	xip->data = NULL;
	XDestroyImage (xip);
}

/* given the loc of the moons, draw a nifty little picture.
 * also save resulting screen locs of everything in the screenobj array.
 * scale of the locations is in terms of jupiter radii == 1.
 * unflipped view is S up, E right.
 * planet itself is in md[0], moons in md[1..NM-1].
 *     +md[].x: East, jup radii
 *     +md[].y: South, jup radii
 *     +md[].z: in front, jup radii
 */
static void
jm_draw_map (w, jop, jupsize, cmlII, md)
Widget w;
Obj *jop;
double jupsize;
double cmlII;
MoonData md[NM];
{
#define	NORM	27.0	/* max Callisto orbit radius; used to normalize */
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
	char *grslstr;
	double grs;
	char c;
	int x, y;
	XPoint xp[5];
	char buf[64];
	int l;
	unsigned int nx, ny, bw, d;
	int ns = flip_tb ? -1 : 1;
	int ew = flip_lr ? -1 : 1;
	int scale1;
	int i;

	/* first the main graphic */
	win = XtWindow (jda_w);

	if (!j_fgc)
	    make_gcs(w);

	XmScaleGetValue (limmag_w, &fmag);
	XmScaleGetValue (scale_w, &sv);
	scale = pow(MAXSCALE, sv/100.0);

	/* get size and erase */
	XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);
	XFillRectangle (dsp, jm_pm, j_bgc, 0, 0, nx, ny);
	scale1 = (int)floor(MAPSCALE(1)+0.5);

	/* prepare for a new list of things on the screen */
	reset_screenobj(SO_MAIN);

	/* draw background objects, if desired */
	if (skybkg)
	    sky_background(jm_pm, nx, ny, fmag, md[0].ra, md[0].dec,
			jupsize/MAPSCALE(2), jupsize*NORM, flip_tb, flip_lr);

	/* draw labels */
	c = flip_lr ? 'W' : 'E';
	XDrawString(dsp, jm_pm, j_fgc, nx-j_cw-1, ny/2-2, &c, 1);
	c = flip_tb ? 'N' : 'S';
	XDrawString(dsp, jm_pm, j_fgc, (nx-j_cw)/2-1, j_fs->ascent, &c, 1);

	/* draw Jupiter in center with unit radius */
	grslstr = XmTextFieldGetString (grsl_w);
	grs = degrad(atod(grslstr))-cmlII;
	XtFree (grslstr);
	jm_fimage (dsp, jm_pm, nx/2-scale1, ny/2-scale1, 2*scale1-1, grs);
	add_screenobj (SO_MAIN, jop, nx/2, ny/2);

	/* draw 1' calibration line */
	j_calibline (dsp, jm_pm, j_fgc, nx/2, 5*ny/6, "1'", j_cw*2, j_ch+4,
				(int)MAPSCALE(degrad(1.0/60.0)/(jupsize/2)));

	/* draw each moon
	 */
	for (i = 1; i < NM; i++) {
	    double mx = md[i].x;
	    double my = md[i].y;
	    double mag = md[i].mag;
	    int diam;
	    Obj o;

	    if (!md[i].evis)
		continue;	/* behind jupiter */
	    diam = magdiam (fmag, 1, jupsize/(2*scale1), mag, 0.0);
	    if (brmoons)
		diam += 3;
	    if (diam <= 0)
		continue;	/* too faint */

	    x = XCORD(mx);
	    y = YCORD(my);
	    if (diam == 1)
		XDrawPoint(dsp, jm_pm, j_xgc, x, y);
	    else
		XFillArc (dsp, jm_pm, j_xgc, x-diam/2, y-diam/2, diam, diam,
								    0, 360*64);

	    /* add object to list of screen objects drawn */
	    (void) strcpy (o.o_name, mnames[i].full);
	    set_smag (&o, mag);
	    o.s_ra = (float)md[i].ra;
	    o.s_dec = (float)md[i].dec;
	    add_screenobj (SO_MAIN, &o, x, y);

	    if (tags && mnames[i].tag)
		XDrawString(dsp, jm_pm, j_xgc, x-j_cw/2, y+2*j_ch,
					mnames[i].tag, strlen(mnames[i].tag));
	}

	XCopyArea (dsp, jm_pm, win, j_fgc, 0, 0, nx, ny, 0, 0);

	/* then the top view, if desired */

	if (!topview || !jt_pm)	/* let expose make new pixmap */
	    return;

	/* get size and erase */
	win = XtWindow (jtda_w);
	XGetGeometry(dsp, win, &root, &x, &y, &nx, &ny, &bw, &d);
	XFillRectangle (dsp, jt_pm, j_bgc, 0, 0, nx, ny);

	/* draw jupiter in center with unit radius, cover dark side */
	jm_timage (dsp, jt_pm, nx/2-scale1, ny/2-scale1, 2*scale1-1, grs);
	i = (int)(-ew*64*raddeg(asin(sin(degrad(jop->s_elong))/jop->s_sdist)));
	XFillArc(dsp, jt_pm, j_bgc, nx/2-scale1-1, ny/2-scale1-1, 2*scale1+1,
						    2*scale1+1, i, 180*64);
	reset_screenobj(SO_TOP);
	add_screenobj (SO_TOP, jop, nx/2, ny/2);

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

	    diam = magdiam (fmag, 1, jupsize/(2*scale1), mag, 0.0);
	    if (brmoons)
		diam += 3;
	    if (diam <= 0)
		continue;	/* too faint */

	    x = XCORD(mx);
	    y = ZCORD(mz);
	    if (diam == 1)
		XDrawPoint (dsp, jt_pm, j_xgc, x, y);
	    else
		XFillArc (dsp, jt_pm, j_xgc, x-diam/2, y-diam/2, diam, diam,
								    0, 360*64);

	    /* add object to list of screen objects drawn */
	    (void) strcpy (o.o_name, mnames[i].full);
	    set_smag (&o, mag);
	    o.s_ra = (float)md[i].ra;
	    o.s_dec = (float)md[i].dec;
	    add_screenobj (SO_TOP, &o, x, y);

	    if (tags && mnames[i].tag)
		XDrawString(dsp, jt_pm, j_xgc, x-j_cw/2, y+2*j_ch,
					mnames[i].tag, strlen(mnames[i].tag));
	}

	/* draw labels */

	l = sprintf (buf, "%s Pole", flip_tb ? "North" : "South");
	XDrawString(dsp, jt_pm, j_fgc, (nx-j_cw*l)/2, j_ch, buf, l);
	XDrawString(dsp, jt_pm, j_fgc, nx/2, ny-j_ch, "to Earth", 8);
	xp[0].x = nx/2 - 15; xp[0].y = ny - 2*j_ch;
	xp[1].x = 0;         xp[1].y = 3*j_ch/2;
	xp[2].x = -3;        xp[2].y = -3*j_ch/4;
	xp[3].x = 6;         xp[3].y = 0;
	xp[4].x = -3;        xp[4].y = 3*j_ch/4;
	XDrawLines (dsp, jt_pm, j_fgc, xp, 5, CoordModePrevious);

	XCopyArea (dsp, jt_pm, win, j_fgc, 0, 0, nx, ny, 0, 0);
}

/* make the colors and GCs.
 * TODO: reclaim old stuff if called again.
 */
static void
make_gcs (w)
Widget w;
{
	Display *dsp = XtDisplay(w);
	Window win = XtWindow(w);
	XFontStruct *fsp;
	XGCValues gcv;
	unsigned int gcm;
	Pixel fg;

	gcm = GCForeground;
	get_color_resource (w, "jupiterColor", &fg);
	gcv.foreground = fg;
	j_fgc = XCreateGC (dsp, win, gcm, &gcv);
	j_fs = XQueryFont (dsp, XGContextFromGC (j_fgc));
	j_cw = j_fs->max_bounds.width;
	j_ch = j_fs->max_bounds.ascent + j_fs->max_bounds.descent;

	gcm = GCForeground;
	get_color_resource (w, "JupiterBackground", &jbg_p);
	gcv.foreground = jbg_p;
	j_bgc = XCreateGC (dsp, win, gcm, &gcv);

	gcm = GCForeground | GCFunction | GCFont;
	fsp = getXResFont ("moonsFont");
	gcv.foreground = fg ^ jbg_p;
	gcv.function = GXxor;
	gcv.font = fsp->fid;
	j_xgc = XCreateGC (dsp, win, gcm, &gcv);
}

/* draw a calibration line l pixels long centered at [xc,yc] marked with tag
 * with is in a bounding box tw x th.
 */
static void
j_calibline (dsp, win, gc, xc, yc, tag, tw, th, l)
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
	jm_loadfs (ra0, dec0);

	/* scan the database and draw whatever is near */
	for (db_scaninit(&dbs, ALLM, fstars, nfstars);
					    (op = db_scan (&dbs)) != NULL; ) {
	    double dra, ddec;
	    GC gc;
	    int dx, dy, x, y;
	    int diam;

	    if (is_planet (op, JUPITER)) {
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
jm_loadfs (ra, dec)
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
            (void) sprintf (msg, "Jupiter View added %d field stars", nfstars);
	    xe_msg (msg, 0);
	    fsdec = dec;
	    fsra = ra;
	}
}

/* given jupiter loc in md[0].ra/dec and size, and location of each moon in 
 * md[1..NM-1].x/y in jup radii, find ra/dec of each moon in md[1..NM-1].ra/dec.
 */
static void
moonradec (jupsize, md)
double jupsize;		/* jup diameter, rads */
MoonData md[NM];	/* fill in RA and Dec */
{
	double juprad = jupsize/2;
	double jupra = md[0].ra;
	double jupdec = md[0].dec;
	int i;

	for (i = 1; i < NM; i++) {
	    double dra  = juprad * md[i].x;
	    double ddec = juprad * md[i].y;
	    md[i].ra  = jupra + dra;
	    md[i].dec = jupdec - ddec;
	}
}

#define	dsin(x)	sin(degrad(x))
#define	dcos(x)	cos(degrad(x))

static int use_bdl P_((double jd, MoonData md[NM]));
static void meeus_jupiter P_((double d, double *cmlI, double *cmlII,
    MoonData md[NM]));

/* given a modified julian date (ie, days since Jan .5 1900), d, return x, y, z
 *   location of each Galilean moon as a multiple of Jupiter's radius. on this
 *   scale, Callisto is never more than 26.5593. +x is easterly, +y is
 *   southerly, +z is towards earth. x and z are relative to the equator
 *   of Jupiter; y is further corrected for earth's position above or below
 *   this plane. also, return the system I and II central meridian longitude,
 *   in degress, relative to the true disk of jupiter and corrected for light
 *   travel time. also return jupiter ra/dec/mag in md[0] and diam, in rads.
 *   also return nominal magnitude of each moon.
 */
static void
jupiter_data(d, jop, jupsize, cmlI, cmlII, md)
double d;
Obj *jop;
double *jupsize;
double *cmlI, *cmlII;	/* central meridian longitude, rads */
MoonData md[NM];	/* fill in md[0].ra/dec/mag/x/y/z for jupiter,
			 * md[1..NM-1].x/y/z/mag for each moon
			 */
{
        static int using_bdl = -1; /* log which model, but only when changes */

	md[0].ra = jop->s_ra;
	md[0].dec = jop->s_dec;
	md[0].mag = get_mag(jop);

	md[0].x = 0;
	md[0].y = 0;
	md[0].z = 0;

	*jupsize = degrad(jop->s_size/3600.0);

	/* from Pasachoff/Menzel */

	md[1].mag = 5.0;
	md[2].mag = 5.3;
	md[3].mag = 4.6;
	md[4].mag = 5.6;

	/* get moon data from BDL if possible, else Meeus' model */

	if (use_bdl (d+MJD0, md) == 0) {
	    if (using_bdl != 1) {
		xe_msg ("Using BDL model for Galilean moons", 0);
		using_bdl = 1;
	    }
	    meeus_jupiter (d, cmlI, cmlII, NULL);
	} else {
	    if (using_bdl != 0) {
		xe_msg ("Using Meeus model for Galilean moons", 0);
		using_bdl = 0;
	    }
	    meeus_jupiter (d, cmlI, cmlII, md);
	}
}

/* computer location of GRS and Galilean moons.
 * if md == NULL, don't do moons.
 * from "Astronomical Formulae for Calculators", 2nd ed, by Jean Meeus,
 *   Willmann-Bell, Richmond, Va., U.S.A. (c) 1982, chapters 35 and 36.
 */
static void
meeus_jupiter(d, cmlI, cmlII, md)
double d;
double *cmlI, *cmlII;	/* central meridian longitude, rads */
MoonData md[NM];	/* fill in md[1..NM-1].x/y/z for each moon.
			 * N.B. md[0].ra/dec must already be set
			 */
{
	double A, B, Del, J, K, M, N, R, V;
	double cor_u1, cor_u2, cor_u3, cor_u4;
	double solc, tmp, G, H, psi, r, r1, r2, r3, r4;
	double u1, u2, u3, u4;
	double lam, Ds;
	double z1, z2, z3,  z4;
	double De, dsinDe;
	double theta, phi;
	double tvc, pvc;
	double salpha, calpha;
	int i;

	V = 134.63 + 0.00111587 * d;

	M = (358.47583 + 0.98560003*d);
	N = (225.32833 + 0.0830853*d) + 0.33 * dsin (V);

	J = 221.647 + 0.9025179*d - 0.33 * dsin(V);

	A = 1.916*dsin(M) + 0.02*dsin(2*M);
	B = 5.552*dsin(N) + 0.167*dsin(2*N);
	K = (J+A-B);
	R = 1.00014 - 0.01672 * dcos(M) - 0.00014 * dcos(2*M);
	r = 5.20867 - 0.25192 * dcos(N) - 0.00610 * dcos(2*N);
	Del = sqrt (R*R + r*r - 2*R*r*dcos(K));
	psi = raddeg (asin (R/Del*dsin(K)));

	*cmlI  = degrad(268.28 + 877.8169088*(d - Del/173) + psi - B);
	range (cmlI, 2*PI);
	*cmlII = degrad(290.28 + 870.1869088*(d - Del/173) + psi - B);
	range (cmlII, 2*PI);

	/* that's it if don't want moon info too */
	if (!md)
	    return;

	solc = (d - Del/173.);	/* speed of light correction */
	tmp = psi - B;

	u1 = 84.5506 + 203.4058630 * solc + tmp;
	u2 = 41.5015 + 101.2916323 * solc + tmp;
	u3 = 109.9770 + 50.2345169 * solc + tmp;
	u4 = 176.3586 + 21.4879802 * solc + tmp;

	G = 187.3 + 50.310674 * solc;
	H = 311.1 + 21.569229 * solc;
      
	cor_u1 =  0.472 * dsin (2*(u1-u2));
	cor_u2 =  1.073 * dsin (2*(u2-u3));
	cor_u3 =  0.174 * dsin (G);
	cor_u4 =  0.845 * dsin (H);
      
	r1 = 5.9061 - 0.0244 * dcos (2*(u1-u2));
	r2 = 9.3972 - 0.0889 * dcos (2*(u2-u3));
	r3 = 14.9894 - 0.0227 * dcos (G);
	r4 = 26.3649 - 0.1944 * dcos (H);

	md[1].x = -r1 * dsin (u1+cor_u1);
	md[2].x = -r2 * dsin (u2+cor_u2);
	md[3].x = -r3 * dsin (u3+cor_u3);
	md[4].x = -r4 * dsin (u4+cor_u4);

	lam = 238.05 + 0.083091*d + 0.33*dsin(V) + B;
	Ds = 3.07*dsin(lam + 44.5);
	De = Ds - 2.15*dsin(psi)*dcos(lam+24.)
		- 1.31*(r-Del)/Del*dsin(lam-99.4);
	dsinDe = dsin(De);

	z1 = r1 * dcos(u1+cor_u1);
	z2 = r2 * dcos(u2+cor_u2);
	z3 = r3 * dcos(u3+cor_u3);
	z4 = r4 * dcos(u4+cor_u4);

	md[1].y = z1*dsinDe;
	md[2].y = z2*dsinDe;
	md[3].y = z3*dsinDe;
	md[4].y = z4*dsinDe;

	/* compute sky transformation angle as triple vector product */
	tvc = PI/2.0 - md[0].dec;
	pvc = md[0].ra;
	theta = PI/2.0 - POLE_DEC;
	phi = POLE_RA;
	salpha = -sin(tvc)*sin(theta)*(cos(pvc)*sin(phi) - sin(pvc)*cos(phi));
	calpha = sqrt (1.0 - salpha*salpha);

	for (i = 0; i < NM; i++) {
	    double tx =  md[i].x*calpha + md[i].y*salpha;
	    double ty = -md[i].x*salpha + md[i].y*calpha;
	    md[i].x = tx;
	    md[i].y = ty;
	}

	md[1].z = z1;
	md[2].z = z2;
	md[3].z = z3;
	md[4].z = z4;
}

/* hunt for BDL file. return 0 if ok, else -1 */
static int
use_bdl (JD, md)
double JD;		/* julian date */
MoonData md[NM];        /* fill md[1..NM-1].x/y/z for each moon */
{
#define	JUPRAU	.0004769108	/* jupiter radius, AU */
	static FILE *fp;	/* keep open once find */
	double x[NM], y[NM], z[NM];
	char buf[1024];
	int i;

	/* only valid 1999 through 2010 */
	if (JD < 2451179.50000 || JD >= 2455562.5)
	    return (-1);

	/* open first time */
	if (!fp) {
	    (void) sprintf (buf, "%s/auxil/jupiter.9910", getShareDir());
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
	    md[i].x =  x[i-1]/JUPRAU;	/* we want jup radii +E */
	    md[i].y = -y[i-1]/JUPRAU;	/* we want jup radii +S */
	    md[i].z = -z[i-1]/JUPRAU;	/* we want jup radii +front */
	}

	/* ok */
	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: jupmenu.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
