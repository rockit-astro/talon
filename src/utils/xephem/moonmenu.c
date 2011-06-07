/* code to manage the stuff on the moon display.
 */

#include <stdio.h>
#include <math.h>

#if defined(__STDC__)
#include <stdlib.h>
#include <string.h>
#else
extern void *malloc(), *realloc();
#endif

#if defined(_POSIX_SOURCE)
#include <unistd.h>
#else
extern int close();
#endif

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/DrawingA.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/Scale.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/ScrolledW.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "preferences.h"
#include "ps.h"
#include "fits.h"


extern Widget toplevel_w;
extern XtAppContext xe_app;
#define XtD XtDisplay(toplevel_w)
extern Colormap xe_cm;


extern FILE *fopenh P_((char *name, char *how));
extern Now *mm_get_now P_((void));
extern Obj *db_basic P_((int id));
extern Obj *db_scan P_((DBScan *sp));
extern char *getShareDir P_((void));
extern char *mm_getsite P_((void));
extern double atod P_((char *buf));
extern double delra P_((double dra));
extern int alloc_ramp P_((Display *dsp, XColor *basep, Colormap cm,
    Pixel pix[], int maxn));
extern int any_ison P_((void));
extern int fs_fetch P_((Now *np, double ra, double dec, double fov,
    double mag, ObjF **opp));
extern int get_color_resource P_((Widget w, char *cname, Pixel *p));
extern int gray_ramp P_((Display *dsp, Colormap cm, Pixel **pixp));
extern int isUp P_((Widget shell_w));
extern int magdiam P_((int fmag, int magstp, double scale, double mag,
    double size));
extern int openh P_((char *name, int flags, ...));
extern void buttonAsButton P_((Widget w, int whether));
extern void centerScrollBars P_((Widget sw_w));
extern void db_scaninit P_((DBScan *sp, int mask, ObjF *op, int nop));
extern void db_update P_((Obj *op));
extern void dm_newobj P_((int dbidx));
extern void dm_update P_((Now *np, int how_much));
extern void f_date P_((Widget w, double jd));
extern void f_double P_((Widget w, char *fmt, double f));
extern void f_mtime P_((Widget w, double t));
extern void f_pangle P_((Widget w, double a));
extern void f_showit P_((Widget w, char *s));
extern void f_dm_angle P_((Widget w, double a));
extern void fs_date P_((char out[], double jd));
extern void fs_dm_angle P_((char out[], double a));
extern void fs_manage P_((void));
extern void fs_time P_((char out[], double t));
extern void get_something P_((Widget w, char *resource, XtArgVal value));
extern void get_views_font P_((Display *dsp, XFontStruct **fspp));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void llibration P_((double JD, double *llatp, double *llonp));
extern void mm_movie P_((double stepsz));
extern void obj_pickgc P_((Obj *op, Widget w, GC *gcp));
extern void obj_set P_((Obj *op, int dbidx));
extern void range P_((double *v, double r));
extern void register_selection P_((char *name));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void sv_draw_obj P_((Display *dsp, Drawable win, GC gc, Obj *op, int x,
    int y, int diam, int dotsonly));
extern char *syserrstr P_((void));
extern void timestamp P_((Now *np, Widget w));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *msg, int app_modal));

typedef struct {
    double srlng;		/* longitude of the rising sun */
    double sslat;		/* lat of subsolar point */
    double llat, llong;		/* current libration in lat and long */
    double limb;		/* limb angle, rads ccw from up */
    double tilt;		/* angle of limb tilt, degrees towards earth */
} MoonInfo;

typedef struct {
    Obj o;			/* copy of sky object info */
    int x, y;			/* X location on mda_w */
} SkyObj;

static SkyObj *skyobjs;		/* malloced list of objects in sky background */
static int nskyobjs;		/* number of objects in skyobjs[] list */
static char mooncategory[] = "Moon";	/* Save category */

#define	MAXR	10		/* max gap when picking sky objects, pixels */	

static void resetSkyObj P_((void));
static void addSkyObj P_((Obj *op, int x, int y));
static Obj *closeSkyObj P_((int x, int y));
static void fill_skypopup P_((Obj *op));
static void m_create_skypopup P_((void));

static void m_create_shell P_((void));
static void m_create_msform P_((void));
static void m_create_esform P_((void));
static void m_set_buttons P_((int whether));
static void m_eshine_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_eshineclose_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_eshineup_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_movie_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_mstats_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_mstats_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_activate_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_print_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_print P_((void));
static void m_imps P_((void));
static void m_ps_annotate P_((void));
static void m_popdown_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_init_gcs P_((void));
static void m_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_helpon_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_pl_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_option_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_shrink_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_sizewidgets P_((void));
static int xy2ll P_((int x, int y, double *ltp, double *lgp));
static int ll2xy P_((double lt, double lg, int *xp, int *yp));
static int overMoon P_((int x, int y));
static void m_pointer_eh P_((Widget w, XtPointer client, XEvent *ev,
    Boolean *continue_to_dispatch));
static void m_exp_cb P_((Widget w, XtPointer client, XtPointer call));
static void m_redraw P_((void));
static void m_refresh P_((XExposeEvent *ep));
static void m_getsize P_((Drawable d, unsigned *wp, unsigned *hp,unsigned *dp));
static void m_info P_((Now *np));
static void m_draw P_((void));
static int mxim_create P_((void));
static void mxim_setup P_(());
static void image_setup P_((void (*fp)(), void *arg));
static void m_schematic P_((void));
static int m_esedges P_((int y, double sinsrl, int right, int *lxp, int *rxp));
static int m_shrink P_((int f));
static void mBWdither P_((void));
static void m_resize P_((unsigned char *in, int nr, int nc, int f,
    unsigned char *out));
static void m_umbra P_((void));
static void fliptb P_((void));
static void fliplr P_((void));
static void mi_draw P_((void));
static void m_orientation P_((void));
static void m_labels P_((void));
static void m_sky P_((void));
static void m_grid P_((void));
static void m_sub P_((void));
static void m_mark_libr P_((void));
static void m_reportloc P_((Display *dsp, Window win, int x, int y));

static void makeGlassImage P_((Display *dsp));
static void makeGlassGC P_((Display *dsp, Window win));
static void doGlass P_((Display *dsp, Window win, int b1p, int m1,
    int b1r, int wx, int wy));
static void fillGlass P_((int wx, int wy));
static void m_readdb P_((void));
static void m_freedb P_((void));
static void m_popup P_((XEvent *ep));

/* stuff for the moon image.
 * obviously, size and centering metrics are tuned to a particular image.
 */
#define	XEVERS		3	/* required version in header */
#define MOONRAD		314	/* radius of moon image, pixels */
#define TOPMARG		21	/* top margin of moon image, pixels */
#define LEFTMARG	21	/* left margin of moon image, pixels */
#define	MNROWS		670	/* number of rows in moon image */
#define	MNCOLS		670	/* number of columns in moon image */
#define	BORD		200	/* extra drawing area border for labels/stars */
#define	CLNG	degrad(-4.3)	/* longitude of image center, rads east */
#define	CLAT	degrad(1.0)	/* latitude of image center, rads north */
#define MOVIE_STEPSZ    (2.)	/* movie step size without stars, hours */
#define MOVIE_SSTEPSZ   (1./60)	/* movie step size with stars, hours */

static unsigned char *mimage;	/* malloced array of full-size moon image */
static Pixel *mgray;		/* gray-scale ramp for drawing image */
static int nmgray;		/* number of pixels usable in mgray[] */
static Pixel mbg;		/* background color for image */
static int mdepth;		/* depth of image, in bits */
static int mbpp;		/* bits per pixel in image: 1, 8, 16 or 32 */
static int topmarg, leftmarg;	/* current TOPMARG/LEFTMARG after resize */
static int moonrad;		/* current MOONRAD after resize, pixels */
static int mnrows, mncols;	/* current size of image after resize, pixels */
static XImage *m_xim;		/* XImage of moon now at current size */
static int mshrink;		/* current shrink factor: 1 .. MAXSHRINK */
static MoonInfo minfo;		/* stats about the current situation */

#define	MAXSHRINK	6	/* max shrink factor */
#define	SYNP		29.53	/* average synodic period, days */
#define	SIDP		27.32	/* average sidereal period, days */
#define	MINKM		125	/* smallest feature we ever label, km */
#define	ESHINEF		10	/* earthshine factor range: 0 .. this */
#define	LMFRAC		40	/* libr marker is 1/x this of total size */
#define	LGAP		20	/* gap between NSEW labels and image edge */
#define	FMAG		12	/* faintest mag of sky background object */

/* main's widgets */
static Widget mshell_w;		/* main moon shell */
static Widget msform_w;		/* statistics form dialog */
static Widget msw_w;		/* main scrolled window */
static Widget mda_w;		/* image view DrawingArea */
static Pixmap m_pm;		/* image view staging pixmap */
static Widget dt_w;		/* main date/time stamp widget */
static Widget sdt_w;		/* statistics date/time stamp widget */

/* "More info" widgets */
static Widget srlng_w;		/* sunrise longitude PB widget */
static Widget sslat_w;		/* subsolar latitude PB widget */
static Widget llat_w;		/* lib in lat PB widget */
static Widget llong_w;		/* lib in long PB widget */
static Widget limb_w;		/* limb angle PB widget */
static Widget lib_w;		/* total lib PB widget */
static Widget sunalt_w;		/* sun altitude under cursor */
static Widget lat_w, lng_w;	/* lat/long under cursor */
static Widget nsrt_w, nsrd_w;	/* next sunrise time/date under cursor */
static Widget nsst_w, nssd_w;	/* next sunset time/date under cursor */

/* earthshine widgets */
static Widget esform_w;		/* main dialog */
static Widget eshine_w;		/* earthshine factor scale */

/* lunar surface popup's widgets */
static Widget pu_w;		/* main popup */
static Widget pu_name_w;	/* popup name label */
static Widget pu_type_w;	/* popup type label */
static Widget pu_size_w;	/* popup size label */
static Widget pu_lat_w;		/* popup latitude label */
static Widget pu_lng_w;		/* popup longitude label */
static Widget pu_alt_w;		/* popup sun altitude label */
static Widget pu_pl_w;		/* popup persistent label TB */

/* sky background object's widgets */
static Widget skypu_w;		/* main popup */
static Widget skypu_name_w;	/* popup name label */
static Widget skypu_mag_w;	/* popup mag label */
static Widget skypu_b_w;	/* basic assignment PB */
static Widget skypu_a_w;	/* object x/y/z assignment PB */
static Obj *skypu_op;		/* current object being referred to */

/* field star support */
static ObjF *fstars;            /* malloced list of field stars, or NULL */
static int nfstars;             /* number of entries in fstars[] */
static double fsdec, fsra;      /* location when field stars were loaded */
#define FSFOV   degrad(3.0)     /* size of FOV to fetch, rads */
#define FSMAG   ((double)FMAG)	/* limiting mag for fetch */
#define FSMOVE  degrad(.2)      /* reload when moon has moved this far, rads */
static void m_loadfs P_((Now *np, double ra, double dec));

static GC m_fgc, m_bgc, m_agc;	/* various GCs */
static XFontStruct *m_fsp;	/* label font info */

static int m_selecting;		/* set while our fields are being selected */

static XImage *glass_xim;	/* glass XImage -- 0 means new or can't */
static GC glassGC;		/* GC for glass border */

#define	GLASSSZ		50	/* mag glass width and heigth, pixels */
#define	GLASSMAG	2	/* mag glass factor (may be any integer > 0) */

/* used with m_assign_cb() */
typedef enum {
    A_BASIC, A_OBJX, A_OBJY, A_OBJZ
} Assign_t;

/* options list */
typedef enum {
    SPACECRAFT_OPT, GRID_OPT, LABELS_OPT, FLIPLR_OPT, FLIPTB_OPT,
    SKY_OPT, IMAGE_OPT, UMBRA_OPT, N_OPT
} Option;
static int option[N_OPT];
static Widget option_w[N_OPT];

#define	NSTARS		150	/* total number of fake stars to sprinkle in */
#define	NBSTARS		20	/* number of bright stars to sprinkle in */
#define	NVBSTARS	5	/* number of very bright stars to sprinkle in */

/* return the effective x or y values, allowing for flip options.
 * N.B. this is with respect to the image, not the drawing area.
 */
#define	FX(x)	(option[FLIPLR_OPT] ? mncols-1-(x) : (x))
#define	FY(y)	(option[FLIPTB_OPT] ? mnrows-1-(y) : (y))

/* lunar database */
typedef struct {
    char *name;			/* malloced name */
    char *type;			/* malloced type (may be shared) */
    short x, y;			/* image location, X pixels coords */
    short sz; 			/* diam w/o forshortening, km,
    				 * or landing date if < 0
				 */
    char pl;			/* whether want persistent label */
    char rjust;			/* whether to right- else left-justify */
} MoonDB;
static MoonDB *moondb;		/* malloced array from file */
static int nmoondb;		/* entries in moondb[] */
#define	MDIA	3476		/* moon diameter, km */
static int m_wantlabel P_((MoonDB *mp));
static void fill_popup P_((MoonDB *mp, int x, int y));
static void m_create_popup P_((void));
static MoonDB *closeMoonDB P_((int x, int y));

/* called when the moon view is activated via the main menu pulldown.
 * if first time, build everything, else just up we go.
 * allow for retrying to read the image file each time until find it.
 */
void
m_manage ()
{
	if (!mshell_w) {
	    /* menu one-time-only work */

	    /* build dialogs */
	    m_create_shell();
	    m_create_msform();
	    m_create_popup();
	    m_create_skypopup();
	    m_create_esform();

	    /* establish depth, colors and bits per pixel */
	    get_something (mda_w, XmNdepth, (XtArgVal)&mdepth);
	    m_init_gcs();
	    mbpp = (mdepth == 1 || nmgray == 2) ? 1 :
				    (mdepth>=17 ? 32 : (mdepth >= 9 ? 16 : 8));
	}

	if (!mimage) {
	    /* read image and display at initial size */
	    if (m_shrink(mshrink) < 0)
		return;
	}
	
	XtPopup (mshell_w, XtGrabNone);
	set_something (mshell_w, XmNiconic, (XtArgVal)False);
	centerScrollBars (msw_w);
}

int
m_ison()
{
	return (isUp(mshell_w));
}

/* commanded from main to update with a new set of circumstances */
void
m_update (np, how_much)
Now *np;
int how_much;
{
	if (!mshell_w)
	    return;
	if (!isUp(mshell_w) && !any_ison() && !how_much)
	    return;

	watch_cursor (1);
	m_redraw();
	watch_cursor (0);
}

/* called when basic resources change.
 * rebuild and redraw.
 */
void
m_newres()
{
	if (!mshell_w)
	    return;
	m_init_gcs();
	m_update (mm_get_now(), 1);
}

/* called when the database has changed.
 * if we are drawing background, we'd best redraw everything.
 */
void
m_newdb (appended)
int appended;
{
	if (option[SKY_OPT] && isUp(mshell_w))
	    m_redraw();
}

/* called by other menus as they want to hear from our buttons or not.
 * the "on"s and "off"s stack - only really redo the buttons if it's the
 * first on or the last off.
 */
void
m_selection_mode (whether)
int whether;	/* whether setting up for plotting or for not plotting */
{
	if (whether)
	    m_selecting++;
	else if (m_selecting > 0)
	    --m_selecting;

	if (mshell_w) {
	    if ((whether && m_selecting == 1)     /* first one to want on */
		|| (!whether && m_selecting == 0) /* last one to want off */)
		m_set_buttons (whether);
	}
}

/* called to put up or remove the watch cursor.  */
void
m_cursor (c)
Cursor c;
{
	Window win;

	if (mshell_w && (win = XtWindow(mshell_w)) != 0) {
	    Display *dsp = XtDisplay(mshell_w);
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
}

static void
m_create_shell()
{
	typedef struct {
	    Option o;		/* which option */
	    char *name;		/* name of TB */
	    char *title;	/* title string of option */
	    char *tip;		/* widget tip */
	} OpSetup;
	static OpSetup ops[] = {
	    {SPACECRAFT_OPT,	"Spacecraft",	"Spacecraft",
	    	"Label each Spacecraft landing site"},
	    {LABELS_OPT,	"Labels",	"Features",
	    	"Label significant natural features"},
	    {SKY_OPT,		"SkyBkg",	"Sky background",
	    	"When on, sky will include database objects and Field Stars"},
	    {IMAGE_OPT,		"Image",	"Use real image",
	    	"When on, scene will include actual Lunar image"},
	    {UMBRA_OPT,		"Umbra",	"{Pen}Umbra",
	    	"Display edges of Earth shadow (if near a lunar eclipse)"},
	    {FLIPTB_OPT,	"FlipTB",	"Flip T/B",
	    	"Flip map top-to-bottom"},
	    {FLIPLR_OPT,	"FlipLR",	"Flip L/R",
	    	"Flip map left-to-right"},
	    {GRID_OPT,		"Grid",		"Grid",
	    	"Overlay 15 degree grid and mark Sub-Earth location"},
	};
	typedef struct {
	    char *label;	/* what goes on the help label */
	    char *key;		/* string to call hlp_dialog() */
	} HelpOn;
	static HelpOn helpon[] = {
	    {"Intro...",	"Moon - intro"},
	    {"on Mouse...",	"Moon - mouse"},
	    {"on Control...",	"Moon - control"},
	    {"on View...",	"Moon - view"},
	    {"on Scale...",	"Moon - scale"},
	};
	Widget mb_w, pd_w, cb_w;
	Widget w;
	Widget mform_w;
	char msg[1024];
	XmString str;
	unsigned long mask;
	Arg args[20];
	int i;
	int n;

	/* create master form */
	n = 0;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle, "xephem Moon view"); n++;
	XtSetArg (args[n], XmNiconName, "Moon"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	mshell_w = XtCreatePopupShell ("Moon", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (mshell_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (mshell_w, XmNpopdownCallback, m_popdown_cb, 0);
	sr_reg (mshell_w, "XEphem*Moon.width", mooncategory, 0);
	sr_reg (mshell_w, "XEphem*Moon.height", mooncategory, 0);
	sr_reg (mshell_w, "XEphem*Moon.x", mooncategory, 0);
	sr_reg (mshell_w, "XEphem*Moon.y", mooncategory, 0);

	n = 0;
	XtSetArg (args[n], XmNhorizontalSpacing, 5); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	mform_w = XmCreateForm (mshell_w, "MoonF", args, n);
	XtAddCallback (mform_w, XmNhelpCallback, m_help_cb, 0);
	XtManageChild (mform_w);

	/* create the menu bar across the top */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	mb_w = XmCreateMenuBar (mform_w, "MB", args, n);
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

	    /* the "Print" push button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "PrintPB", args, n);
	    set_xmstring (w, XmNlabelString, "Print...");
	    XtAddCallback (w, XmNactivateCallback, m_print_cb, 0);
	    wtip (w, "Print the current Moon map");
	    XtManageChild (w);

	    /* the "Field stars" push button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "FS", args, n);
	    set_xmstring (w, XmNlabelString, "Field Stars...");
	    XtAddCallback (w, XmNactivateCallback, (XtCallbackProc)fs_manage,0);
	    wtip (w, "Define where GSC and PPM catalogs are to be found");
	    XtManageChild (w);

	    /* button to bring up the earthshine dialog */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "ESPB", args, n);
	    XtAddCallback (w, XmNactivateCallback, m_eshineup_cb, 0);
	    set_xmstring (w, XmNlabelString, "Set Earthshine...");
	    wtip (w, "Display a dialog to set brightness of dark half of moon");
	    XtManageChild (w);

	    /* button for Movie demo */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "EMPB", args, n);
	    XtAddCallback (w, XmNactivateCallback, m_movie_cb, 0);
	    set_xmstring (w, XmNlabelString, "Movie Demo");
	    wtip (w, "Start/Stop a fun time-lapse animation");
	    XtManageChild (w);

	    /* add a separator */
	    n = 0;
	    w = XmCreateSeparator (pd_w, "CtS", args, n);
	    XtManageChild (w);

	    /* add the close button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, m_close_cb, 0);
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

	    /* add options */

	    for (i = 0; i < XtNumber(ops); i++) {
		OpSetup *osp = &ops[i];
		Option o = osp->o;

		n = 0;
		XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
		w = XmCreateToggleButton (pd_w, osp->name, args, n);
		XtAddCallback(w, XmNvalueChangedCallback, m_option_cb,
								(XtPointer)o);
		set_xmstring (w, XmNlabelString, osp->title);
		option[o] = XmToggleButtonGetState (w);
		option_w[o] = w;
		if (osp->tip)
		    wtip (w, osp->tip);
		XtManageChild (w);
		sr_reg (w, NULL, mooncategory, 1);
	    }

	    /* add a separator */

	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    /* add the More Info control */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Stats", args, n);
	    set_xmstring (w, XmNlabelString, "More info...");
	    XtAddCallback (w, XmNactivateCallback, m_mstats_cb, 0);
	    wtip (w, "Display additional information");
	    XtManageChild (w);

	/* make the Scale pulldown */

	n = 0;
	XtSetArg (args[n], XmNradioBehavior, True); n++;
	pd_w = XmCreatePulldownMenu (mb_w, "ScalePD", args, n);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pd_w);  n++;
	    XtSetArg (args[n], XmNmnemonic, 'S'); n++;
	    cb_w = XmCreateCascadeButton (mb_w, "ScaleCB", args, n);
	    set_xmstring (cb_w, XmNlabelString, "Scale");
	    wtip (cb_w, "Set the image size");
	    XtManageChild (cb_w);

	    for (i = 1; i <= MAXSHRINK; i++) {
		char name[64], title[64];
		int shrink;

		/* just advertise whole values */
		if (MAXSHRINK%i)
		    continue;
		shrink = MAXSHRINK/i;

		(void) sprintf (name, "Scale%dX", shrink);
		n = 0;
		w = XmCreateToggleButton (pd_w, name, args, n);
		XtAddCallback(w, XmNvalueChangedCallback, m_shrink_cb,
								(XtPointer)i);
		(void) sprintf (title, " Scale %d X", shrink);
		set_xmstring (w, XmNlabelString, title);
		XtManageChild (w);

		/* pick up user's default */
		if (XmToggleButtonGetState(w)) {
		    if (mshrink != 0) {
			(void)sprintf(msg,"Multiple setting: ignoring Moon*%s",
									name);
			xe_msg (msg, 0);
		    } else
			mshrink = i;
		}

		/* if done and none set, default to last one */
		if (i == MAXSHRINK && mshrink == 0) {
		    xe_msg("No Moon*Scale<n>X resource -- defaulting to 1",0);
		    XmToggleButtonSetState (w, True, False);
		    mshrink = MAXSHRINK;
		}

		sr_reg (w, NULL, mooncategory, 1);
	    }

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
		XtAddCallback (w, XmNactivateCallback, m_helpon_cb,
							(XtPointer)(hp->key));
		XtManageChild (w);
		XmStringFree(str);
	    }

	/* make a label for the date stamp */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrecomputeSize, False); n++;
	dt_w = XmCreateLabel (mform_w, "DateStamp", args, n);
	timestamp (mm_get_now(), dt_w);	/* sets initial size */
	wtip (dt_w, "Date and Time for which map is computed");
	XtManageChild (dt_w);

	/* make a drawing area in a scrolled window for the image view */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, mb_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, dt_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNscrollingPolicy, XmAUTOMATIC); n++;
	XtSetArg (args[n], XmNvisualPolicy, XmVARIABLE); n++;
	msw_w = XmCreateScrolledWindow (mform_w, "MoonSW", args, n);
	XtManageChild (msw_w);

	    n = 0;
	    XtSetArg (args[n], XmNmarginWidth, 0); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    mda_w = XmCreateDrawingArea (msw_w, "MoonDA", args, n);
	    XtAddCallback (mda_w, XmNexposeCallback, m_exp_cb, NULL);
	    mask = Button1MotionMask | ButtonPressMask | ButtonReleaseMask
						    | PointerMotionHintMask;
	    XtAddEventHandler (mda_w, mask, False, m_pointer_eh, 0);
	    XtManageChild (mda_w);

	    /* SW assumes work is its child but just to be tiddy about it .. */
	    set_something (msw_w, XmNworkWindow, (XtArgVal)mda_w);

	/* match SW background to DA */
	get_something (msw_w, XmNclipWindow, (XtArgVal)&w);
	if (w) {
	    Pixel p;
	    get_something (mda_w, XmNbackground, (XtArgVal)&p);
	    set_something (w, XmNbackground, (XtArgVal)p);
	}
}

static void
m_create_msform()
{
	typedef struct {
	    char *label;
	    Widget *wp;
	    char *tip;
	} DItem;
	static DItem citems[] = {
	    {"Under Cursor:", NULL},
	    {"Latitude +N:",  &lat_w, "Selenographic latitude under cursor"},
	    {"Longitude +E:", &lng_w, "Selenographic longitude under cursor"},
	    {"Sun altitude:", &sunalt_w,
	    	"Sun angle above horizon as seen from location under cursor"},
	    {"Next Sunrise:", &nsrt_w,
	    	"Time of next sunrise as seen from location under cursor"},
	    {" ",             &nsrd_w,
	    	"Date of next sunrise as seen from location under cursor"},
	    {"Next Sunset:",  &nsst_w,
	    	"Time of next sunset as seen from location under cursor"},
	    {" ",             &nssd_w,
	    	"Date of next sunset as seen from location under cursor"},
	};
	Widget rc_w;
	Widget sep_w;
	Widget w;
	XmString str;
	Arg args[20];
	int n;
	int i;

	/* create form */
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	msform_w = XmCreateFormDialog (mshell_w, "MoonStats", args, n);
	set_something (msform_w, XmNcolormap, (XtArgVal)xe_cm);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Moon info"); n++;
	XtSetValues (XtParent(msform_w), args, n);

	/* make a rowcolumn to hold the cursor tracking info */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNspacing, 5); n++; /* matches RC below better */
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNnumColumns, XtNumber(citems)); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	rc_w = XmCreateRowColumn (msform_w, "SRC", args, n);
	XtManageChild (rc_w);

	    for (i = 0; i < XtNumber(citems); i++) {
		DItem *dp = &citems[i];

		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		w = XmCreateLabel (rc_w, "CLbl", args, n);
		set_xmstring (w, XmNlabelString, dp->label);
		XtManageChild (w);

		n = 0;
		XtSetArg (args[n], XmNrecomputeSize, False); n++;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
		w = XmCreateLabel (rc_w, "CVal", args, n);
		set_xmstring (w, XmNlabelString, " ");

		if (dp->wp)
		    *(dp->wp) = w;
		if (dp->tip)
		    wtip (w, dp->tip);
		XtManageChild (w);
	    }

	/* make a separator between the 2 data sets */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (msform_w, "Sep1", args, n);
	XtManageChild(sep_w);

	/* make a rowcolumn to hold the labels and info buttons */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNnumColumns, 6); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	rc_w = XmCreateRowColumn (msform_w, "SRC", args, n);
	XtManageChild (rc_w);

	    /* make the srlng, sslat and libration in lat/long rows */

	    str = XmStringCreate ("Sunrise Long:", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateLabel (rc_w, "MLCoL", args, n);
	    XtManageChild (w);
	    XmStringFree (str);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	    XtSetArg (args[n], XmNuserData, "Moon.SunRiseLng"); n++;
	    srlng_w = XmCreatePushButton (rc_w, "MLCoLPB", args, n);
	    XtAddCallback (srlng_w, XmNactivateCallback, m_activate_cb, NULL);
	    wtip (srlng_w, "Selenographic longitude where sun is now rising");
	    XtManageChild (srlng_w);

	    str = XmStringCreate ("Subsolar Lat:", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateLabel (rc_w, "MLSL", args, n);
	    XtManageChild (w);
	    XmStringFree (str);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	    XtSetArg (args[n], XmNuserData, "Moon.SubSolLat"); n++;
	    sslat_w = XmCreatePushButton (rc_w, "MLSLPB", args, n);
	    XtAddCallback (sslat_w, XmNactivateCallback, m_activate_cb, NULL);
	    wtip (sslat_w, "Selenographic latitude at which sun is directly overhead");
	    XtManageChild (sslat_w);

	    str = XmStringCreate ("Libr in Lat:", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateLabel (rc_w, "MLLatL", args, n);
	    XtManageChild (w);
	    XmStringFree (str);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	    XtSetArg (args[n], XmNuserData, "Moon.LibrLat"); n++;
	    llat_w = XmCreatePushButton (rc_w, "MLLatPB", args, n);
	    XtAddCallback (llat_w, XmNactivateCallback, m_activate_cb, NULL);
	    wtip (llat_w, "Current tilt in latitude from nominal `face-on' direction, + Lunar N");
	    XtManageChild (llat_w);

	    str = XmStringCreate ("Libr in Long:", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateLabel (rc_w, "MLLongL", args, n);
	    XtManageChild (w);
	    XmStringFree (str);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	    XtSetArg (args[n], XmNuserData, "Moon.LibrLong"); n++;
	    llong_w = XmCreatePushButton (rc_w, "MLLongPB", args, n);
	    XtAddCallback (llong_w, XmNactivateCallback, m_activate_cb, NULL);
	    wtip (llong_w, "Current tilt in longitude from nominal `face-on' direction, + Lunar E");
	    XtManageChild (llong_w);

	    /* make the limb/tilt rows */

	    str = XmStringCreate ("Limb angle:", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateLabel (rc_w, "MLimbL", args, n);
	    XtManageChild (w);
	    XmStringFree (str);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	    XtSetArg (args[n], XmNuserData, "Moon.LibrLimb"); n++;
	    limb_w = XmCreatePushButton (rc_w, "MLimbPB", args, n);
	    XtAddCallback (limb_w, XmNactivateCallback, m_activate_cb, NULL);
	    wtip (limb_w, "Limb angle nearest Earth, + Lunar W of N");
	    XtManageChild (limb_w);

	    str = XmStringCreate ("Tilt:", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateLabel (rc_w, "MLibL", args, n);
	    XtManageChild (w);
	    XmStringFree (str);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	    XtSetArg (args[n], XmNuserData, "Moon.LibrTilt"); n++;
	    lib_w = XmCreatePushButton (rc_w, "MLibPB", args, n);
	    XtAddCallback (lib_w, XmNactivateCallback, m_activate_cb, NULL);
	    wtip (lib_w, "Degrees by which nominal face is tilted towards Earth");
	    XtManageChild (lib_w);

	/* add a separator */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (msform_w, "Sep2", args, n);
	XtManageChild (sep_w);

	/* add a label for the current date/time stamp */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	sdt_w = XmCreateLabel (msform_w, "SDTstamp", args, n);
	wtip (sdt_w, "Date and Time for which data are computed");
	XtManageChild (sdt_w);

	/* add a separator */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sdt_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (msform_w, "Sep3", args, n);
	XtManageChild (sep_w);

	/* add a close button */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 20); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 80); n++;
	w = XmCreatePushButton (msform_w, "Close", args, n);
	XtAddCallback (w, XmNactivateCallback, m_mstats_close_cb, 0);
	wtip (w, "Close this dialog");
	XtManageChild (w);
}

/* create the little earthshine scale factor dialog */
static void
m_create_esform()
{
	Arg args[20];
	Widget sep_w;
	Widget w;
	int min, max, v;
	int n;

	/* create form */
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNfractionBase, 10); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	esform_w = XmCreateFormDialog (mshell_w, "MoonES", args, n);
	set_something (esform_w, XmNcolormap, (XtArgVal)xe_cm);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Moon Earthshine"); n++;
	XtSetValues (XtParent(esform_w), args, n);

	/* make a scale */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 10); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightOffset, 10); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNshowValue, True); n++;
	XtSetArg (args[n], XmNscaleMultiple, 1); n++;
	XtSetArg (args[n], XmNminimum, 0); n++;
	XtSetArg (args[n], XmNmaximum, ESHINEF); n++;
	eshine_w = XmCreateScale (esform_w, "Earthshine", args, n);
	wtip (eshine_w, "Set to desired relative Earthshine brightness");
	XtManageChild (eshine_w);
	sr_reg (eshine_w, NULL, mooncategory, 1);

	get_something (eshine_w, XmNminimum, (XtArgVal)&min);
	get_something (eshine_w, XmNmaximum, (XtArgVal)&max);
	get_something (eshine_w, XmNvalue, (XtArgVal)&v);

	if (min >= max || v < min || v > max || v == 0) {
	    xe_msg ("Bogus moon Earthshine values -- setting defaults", 0);
	    set_something (eshine_w, XmNminimum, (XtArgVal)1);
	    set_something (eshine_w, XmNmaximum, (XtArgVal)10);
	    set_something (eshine_w, XmNvalue, (XtArgVal)4);
	}

	/* make a separator */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, eshine_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 10); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightOffset, 10); n++;
	sep_w = XmCreateSeparator (esform_w, "Sep", args, n);
	XtManageChild(sep_w);

	/* make the buttons at the bottom */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 2); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 4); n++;
	w = XmCreatePushButton (esform_w, "Apply", args, n);
	XtAddCallback (w, XmNactivateCallback, m_eshine_cb, NULL);
	wtip (w, "Make it so");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 6); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 8); n++;
	w = XmCreatePushButton (esform_w, "Close", args, n);
	XtAddCallback (w, XmNactivateCallback, m_eshineclose_cb, NULL);
	wtip (w, "Close this dialog");
	XtManageChild (w);
}

/* go through all the buttons pickable for plotting and set whether they
 * should appear to look like buttons or just flat labels.
 */
static void
m_set_buttons (whether)
int whether;	/* whether setting up for plotting or for not plotting */
{
	buttonAsButton (srlng_w, whether);
	buttonAsButton (sslat_w, whether);
	buttonAsButton (llat_w, whether);
	buttonAsButton (llong_w, whether);
	buttonAsButton (limb_w, whether);
	buttonAsButton (lib_w, whether);
}

/* callback from the Apply button in the earthshine dialog */
/* ARGSUSED */
static void
m_eshine_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	m_redraw();
}

/* callback from the Close button in the earthshine dialog */
/* ARGSUSED */
static void
m_eshineclose_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (esform_w);
}

/* called to toggle whether the earthshine eshineactor is dialog */
/* ARGSUSED */
static void
m_eshineup_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (XtIsManaged(esform_w))
	    XtUnmanageChild (esform_w);
	else
	    XtManageChild (esform_w);
}


/* callback from the Close button on the stats menu.
 */
/* ARGSUSED */
static void
m_mstats_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (msform_w);
}

/* callback from the More Info button on the main menu.
 */
/* ARGSUSED */
static void
m_mstats_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtManageChild (msform_w);
	m_set_buttons(m_selecting);
}

/* store intensity v into array[x,y] */
static void
array_pixel (array, x, y, v)
void *array;	/* target of pixels */
int x, y;	/* coord of pixel */
int v;		/* value */
{
	unsigned short *a = (unsigned short *)array;

	a[y*mncols + x] = v;
}

/* callback from the Print PB */
/* ARGSUSED */
static void
m_print_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        XPSAsk ("Moon View", m_print);
}

/* proceed to generate a postscript file.
 * call XPSClose() when finished.
 */
static void
m_print ()
{
	/* must be up */
	if (!m_ison()) {
	    xe_msg ("Moon must be open to print.", 1);
	    XPSClose();
	    return;
	}

	watch_cursor(1);

	/* fit view in square across the top and prepare to capture X calls */
	XPSXBegin (m_pm, 0, 0, mncols+2*BORD, mnrows+2*BORD, 1*72, 10*72,
								(int)(6.5*72));

	/* redraw everything now */
	m_redraw();

        /* no more X captures */
	XPSXEnd();

	/* add some extra info */
	m_ps_annotate ();

	/* finished */
	XPSClose();

	watch_cursor(0);
}

/* render the image in postscript */
static void
m_imps ()
{
	int npix = mnrows * mncols;
	unsigned short *pix;
	char *cmap;
	int i;

	/* get colormap and pixel arrays */
	cmap = malloc (nmgray);
	if (!cmap) {
	    xe_msg ("No memory for colormap to print", 1);
	    return;
	}
	pix = (unsigned short *) malloc (npix*sizeof(unsigned short));
	if (!pix) {
	    xe_msg ("No memory for pixel array to print", 1);
	    free ((void *)cmap);
	    return;
	}

	/* init colormap to inverse gray ramp. 
	 * then print inverse again, to get black text.
	 * set background to black (er, white) to avoid wasting ink.
	 */
	for (i = 0; i < nmgray; i++)
	    cmap[i] = nmgray - 1 - i;
	cmap[0] = 0;

	/* render image into pix[] */
	image_setup (array_pixel, (void *)pix);

	/* render in postscript -- it's already flipped */
	XPSImage (m_pm, pix, cmap, nmgray-1, 1, BORD, BORD, mnrows, mncols,0,0);

	/* ok */
	free ((void *)cmap);
	free ((void *)pix);
}

static void
m_ps_annotate ()
{
	Now *np = mm_get_now();
        char dir[128];
	char buf[128];
	int ctr = 306;  /* = 8.5*72/2 */
	int lx = 180, rx = 460;
	int y;

	/* caption */
	y = AROWY(13);
	if (option[SKY_OPT])
	    (void) sprintf (buf, "XEphem %s Moon View",
		    pref_get(PREF_EQUATORIAL)==PREF_GEO ? "Geocentric"
							: "Topocentric");
	else
	    (void) strcpy (buf, "XEphem Moon View");
	(void) sprintf (dir, "(%s) %d %d cstr", buf, ctr, y);
	XPSDirect (dir);

	y = AROWY(9);
	fs_date (buf, mjd_day(mjd));
	(void) sprintf (dir, "(UTC Date:) %d %d rstr (%s) %d %d lstr\n",
							lx, y, buf, lx+10, y);
	XPSDirect (dir);

	fs_time (buf, mjd_hr(mjd));
	(void) sprintf (dir, "(UTC Time:) %d %d rstr (%s) %d %d lstr\n",
							rx, y, buf, rx+10, y);
	XPSDirect (dir);

	y = AROWY(8);
	fs_dm_angle (buf, minfo.srlng);
	(void) sprintf (dir,"(Sunrise Longitude:) %d %d rstr (%s) %d %d lstr\n",
							lx, y, buf, lx+10, y);
	XPSDirect (dir);

	fs_dm_angle (buf, minfo.sslat);
	(void) sprintf (dir,"(Subsolar Latitude:) %d %d rstr (%s) %d %d lstr\n",
							rx, y, buf, rx+10, y);
	XPSDirect (dir);

	y = AROWY(7);
	fs_dm_angle (buf, minfo.llat);
	(void) sprintf (dir,
			"(Libration in Latitude:) %d %d rstr (%s) %d %d lstr\n",
							lx, y, buf, lx+10, y);
	XPSDirect (dir);

	fs_dm_angle (buf, minfo.llong);
	(void) sprintf (dir,
		    "(Libration in Longitude:) %d %d rstr (%s) %d %d lstr\n",
							rx, y, buf, rx+10, y);
	XPSDirect (dir);

	y = AROWY(6);
	fs_dm_angle (buf, minfo.limb);
	(void) sprintf (dir,
		    "(Limb Angle:) %d %d rstr (%s) %d %d lstr\n",
							lx, y, buf, lx+10, y);
	XPSDirect (dir);

	fs_dm_angle (buf, minfo.limb);
	(void) sprintf (dir,
		    "(Limb Tilt Angle:) %d %d rstr (%6.3f) %d %d lstr\n",
						rx, y, minfo.tilt, rx+10, y);
	XPSDirect (dir);

	/* add site/lat/long if showing real stars and topocentric */
	if (option[SKY_OPT] && pref_get(PREF_EQUATORIAL) == PREF_TOPO) {
	    char *site;

	    /* put site name under caption */
	    site = mm_getsite();
	    if (site) {
		y = AROWY(12);
		(void) sprintf (dir, "(%s) %d %d cstr\n",
	    				XPSCleanStr(site,strlen(site)), ctr, y);
		XPSDirect (dir);
	    }

	    /* then add lat/long */
	    y = AROWY(10);

	    fs_sexa (buf, raddeg(fabs(lat)), 3, 3600);
	    (void) sprintf (dir, "(Latitude:) %d %d rstr (%s %c) %d %d lstr\n",
				    lx, y, buf, lat < 0 ? 'S' : 'N', lx+10, y);
	    XPSDirect (dir);

	    fs_sexa (buf, raddeg(fabs(lng)), 4, 3600);
	    (void) sprintf (dir,"(Longitude:) %d %d rstr (%s %c) %d %d lstr\n",
				    rx, y, buf, lng < 0 ? 'W' : 'E', rx+10, y);
	    XPSDirect (dir);
	}
}

/* callback from any of the data menu buttons being activated.
 */
/* ARGSUSED */
static void
m_activate_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (m_selecting) {
	    char *name;
	    get_something (w, XmNuserData, (XtArgVal)&name);
	    register_selection (name);
	}
}

/* callback from mshell_w being popped down.
 */
/* ARGSUSED */
static void
m_popdown_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (esform_w);
	XtUnmanageChild (msform_w);

	/* freeing the pixmap will prevent any useless updates while off and
	 * insure a fresh update on the next expose.
	 */
	if (m_pm) {
	    XFreePixmap (XtD, m_pm);
	    m_pm = 0;
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

/* called from Close button */
static void
m_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtPopdown (mshell_w);
	/* let popdown do all the work */
}

/* callback from the any of the option TBs.
 * Option enum is in client.
 */
/* ARGSUSED */
static void
m_option_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Option opt = (Option)client;
	int set;

	watch_cursor (1);

	/* toggle the option */
	option[opt] = set = XmToggleButtonGetState (w);

	switch (opt) {
	case GRID_OPT:
	    if (set) {
		m_grid();
		m_sub ();
		m_refresh (NULL);
	    } else
		m_redraw();
	    break;

	case SKY_OPT:
	    if (set) {
		m_sky();	/* can just add real stars */
		m_refresh (NULL);
	    } else
		m_redraw(); /* to erase */
	    break;

	case IMAGE_OPT:
	    m_redraw();
	    break;

	case UMBRA_OPT:
	    if (set) {
		m_umbra();
		m_refresh (NULL);
	    } else
		m_redraw();
	    break;

	case SPACECRAFT_OPT:	/* FALLTHRU */
	case LABELS_OPT:
	    if (set) {
		m_labels();
		m_refresh (NULL);
	    } else
		m_redraw();
	    break;

	case FLIPTB_OPT:
	    fliptb();
	    m_redraw();
	    break;

	case FLIPLR_OPT:
	    fliplr();
	    m_redraw();
	    break;

	case N_OPT:
	    break;
	}


	watch_cursor (0);
}

/* callback from any of the Scale TBs.
 * client is shrink factor: 1..MAXSHRINK
 */
/* ARGSUSED */
static void
m_shrink_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int f = (int)client;
	int set = XmToggleButtonGetState (w);

	if (!set || f == mshrink)
	    return;
	if (m_shrink (f) < 0)
	    return;
	mshrink = f;
}


/* callback for when the Movie button is activated. */
/* ARGSUSED */
static void
m_movie_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        mm_movie (option[SKY_OPT] ? MOVIE_SSTEPSZ : MOVIE_STEPSZ);
}

/* callback from the Help all button
 */
/* ARGSUSED */
static void
m_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg[] = {
	    "This is a depiction of the Moon.",
	};

	hlp_dialog ("Moon", msg, XtNumber(msg));
}

/* callback from a specific Help button.
 * client is a string to use with hlp_dialog().
 */
/* ARGSUSED */
static void
m_helpon_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	hlp_dialog ((char *)client, NULL, 0);
}


/* given an [x,y] on m_xim, find latitude, rads north, and longitude, rads e.
 * N.B. [x,y] must be with respect to the _original_ image size and orientation.
 * return 0 if ok (inside image) else -1.
 */
static int
xy2ll (x, y, ltp, lgp)
int x, y;
double *ltp, *lgp;
{
	double cx, cy;
	double lt, lg;

	/* convert to image center, scaled to radius */
	cx = (double)(x - (leftmarg + moonrad))/moonrad;	/* + right */
	cy = (double)((topmarg + moonrad)  - y)/moonrad;	/* + up */

	if (cx*cx + cy*cy > 1.0)
	    return (-1);

	lt = asin (cy);
	lg = asin(cx/cos(lt));

	lt += CLAT;
	lg += CLNG;
	if (lt > PI/2) {
	    lt = PI - lt;
	    lg += PI;
	} else if (lt < -PI/2) {
	    lt = -PI - lt;
	    lg += PI;
	}

	*ltp = lt;
	*lgp = lg;
	range (lgp, 2*PI);

	return (0);
}

/* given a lat(+N)/long(+E) find its [x,y] location on mda_w.
 * N.B. the returned [x,y] is with respect to the _original_ size and
 *   orientation.
 * return 0 if ok (on front side) else -1.
 */
static int
ll2xy (lt, lg, xp, yp)
double lt, lg;
int *xp, *yp;
{
	double cx, cy, cz;
	double coslt, sinlt;

	lt -= CLAT;
	lg -= CLNG;
	if (lt > PI/2) {
	    lt = PI - lt;
	    lg += PI;
	} else if (lt < -PI/2) {
	    lt = -PI - lt;
	    lg += PI;
	}

	coslt = cos(lt);
	cz = moonrad*coslt*cos(lg);
	if (cz < 0.0)
	    return (-1);

	sinlt = sin(lt);
	cx = moonrad*coslt*sin(lg);
	cy = moonrad*sinlt;

	*xp = (int)(cx + (leftmarg + moonrad) + 0.5);
	*yp = (int)((topmarg + moonrad) -  cy + 0.5);

	return (0);
}

/* return True if the given [x,y], relative to mda_w, refers to a spot over the
 * lunar image, else return 0.
 */
static int
overMoon (x, y)
int x, y;
{
	/* convert to image center */
	x -= BORD + leftmarg + moonrad;
	y -= BORD + topmarg  + moonrad;

	return (x*x + y*y <= moonrad*moonrad);
}

/* event handler from all Button events on the mda_w */
static void
m_pointer_eh (w, client, ev, continue_to_dispatch)
Widget w;
XtPointer client;
XEvent *ev;
Boolean *continue_to_dispatch;
{
	Display *dsp = ev->xany.display;
	Window win = ev->xany.window;
	int evt = ev->type;
	Window root, child;
	int rx, ry, x, y;
	unsigned mask;
	int m1, b1p, b1r, b3p;

	/* what happened? */
	m1  = evt == MotionNotify  && ev->xmotion.state  == Button1Mask;
	b1p = evt == ButtonPress   && ev->xbutton.button == Button1;
	b1r = evt == ButtonRelease && ev->xbutton.button == Button1;
	b3p = evt == ButtonPress   && ev->xbutton.button == Button3;

	/* do we care? */
	if (!m1 && !b1p && !b1r && !b3p)
	    return;

	/* where are we? */
	XQueryPointer (dsp, win, &root, &child, &rx, &ry, &x, &y, &mask);

	/* dispatch */
	if (b3p)
	    m_popup (ev);
	if (b1p || m1 || b1r) {
	    doGlass (dsp, win, b1p, m1, b1r, x, y);
	    m_reportloc (dsp, win, x, y);
	}
}

/* expose (or reconfig) of moon image view drawing area.
 */
/* ARGSUSED */
static void
m_exp_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	XExposeEvent *e = &c->event->xexpose;
	Display *dsp = e->display;
	Window win = e->window;

	watch_cursor (1);

	switch (c->reason) {
	case XmCR_EXPOSE: {
	    /* turn off gravity so we get expose events for either shrink or
	     * expand.
	     * also center the scroll bars initially.
	     */
	    static int before;

	    if (!before) {
		XSetWindowAttributes swa;
		unsigned long mask = CWBitGravity | CWBackingStore;

		swa.bit_gravity = ForgetGravity;
		swa.backing_store = NotUseful;
		XChangeWindowAttributes (e->display, e->window, mask, &swa);

		before = 1;
	    }
	    break;
	    }
	default:
	    printf ("Unexpected mda_w event. type=%d\n", c->reason);
	    exit(1);
	}


	/* insure pixmap exists -- it gets destroyed whenever mda_w is resized
	 * or the dialog is unmanaged.
	 */
	if (!m_pm) {
	    unsigned wid, hei, d;

	    m_getsize (win, &wid, &hei, &d);
	    if ((int)wid != mncols+2*BORD || (int)hei != mnrows+2*BORD) {
		printf ("m_da: Bad size: wid=%d mncols=%d hei=%d mnrows=%d\n",
					wid, mncols, hei, mnrows);
		exit(1);
	    }

	    m_pm = XCreatePixmap (dsp, win, wid, hei, d);
	    m_draw();
	}

	/* update exposed area */
	m_refresh (e);

	watch_cursor (0);
}

/* redraw the current scene */
static void
m_redraw()
{
	watch_cursor (1);

	m_draw ();
	m_refresh(NULL);

	watch_cursor (0);
}

/* copy the m_pm pixmap to the drawing area mda_w.
 * if ep just copy that much, else copy all.
 */
static void
m_refresh(ep)
XExposeEvent *ep;
{
	Display *dsp = XtDisplay(mda_w);
	Window win = XtWindow (mda_w);
	Pixmap pm = m_pm;
	unsigned w, h;
	int x, y;

	/* ignore if no pixmap now */
	if (!pm)
	    return;

	if (ep) {
	    x = ep->x;
	    y = ep->y;
	    w = ep->width;
	    h = ep->height;
	} else {
	    w = mncols+2*BORD;
	    h = mnrows+2*BORD;
	    x = y = 0;
	}

	XCopyArea (dsp, pm, win, m_fgc, x, y, w, h, x, y);
}

/* get the width, height and depth of the given drawable */
static void
m_getsize (d, wp, hp, dp)
Drawable d;
unsigned *wp, *hp, *dp;
{
	Window root;
	int x, y;
	unsigned int bw;

	XGetGeometry (XtD, d, &root, &x, &y, wp, hp, &bw, dp);
}

/* make the various gcs, handy pixel values and fill in mgray[].
 * N.B. just call this once.
 * TODO: reclaim old stuff if called again.
 */
static void
m_init_gcs()
{
	Display *dsp = XtD;
	Window win = XtWindow(toplevel_w);
	Colormap cm = xe_cm;
	XGCValues gcv;
	unsigned int gcm;
	Pixel fg;
	Pixel p;

	/* fg and bg */
	get_something (mda_w, XmNforeground, (XtArgVal)&fg);
	(void) get_color_resource (mda_w, "MoonBackground", &mbg);

	gcm = GCForeground | GCBackground;
	gcv.foreground = fg;
	gcv.background = mbg;
	m_fgc = XCreateGC (dsp, win, gcm, &gcv);

	gcv.foreground = mbg;
	gcv.background = fg;
	m_bgc = XCreateGC (dsp, win, gcm, &gcv);

	/* make the label marker gc */
	(void) get_color_resource (mda_w, "MoonAnnotColor", &p);
	gcm = GCForeground | GCBackground;
	gcv.foreground = p;
	gcv.background = mbg;
	m_agc = XCreateGC (dsp, win, gcm, &gcv);
	get_views_font (dsp, &m_fsp);

	/* build gray-scale ramp for image */
	nmgray = gray_ramp (dsp, cm, &mgray);

	/* unless we will be using a bitmap, force color 0 to background. */
	if (nmgray > 2)
	    mgray[0] = mbg;
}

/* compute supporting moon info in minfo */
static void
m_info(np)
Now *np;
{
	double colong;

	/* get libration info */
	llibration (mjd+MJD0, &minfo.llat, &minfo.llong);
	minfo.limb = atan2 (-minfo.llong, minfo.llat);
	if (minfo.limb < 0.0)
	    minfo.limb += 2*PI;	/* limb angle is traditionally 0..360 */
	minfo.tilt =
		raddeg(sqrt(minfo.llat*minfo.llat + minfo.llong*minfo.llong));

	/* get new terminator location as 0..360 +W */
	moon_colong (mjd+MJD0, 0.0, 0.0, &colong, NULL, NULL, &minfo.sslat);

	/* convert to sunrise longitude */
	minfo.srlng = -colong;
	range (&minfo.srlng, 2*PI);
}

/* update "more info" labels.
 * then, if m_pm is defined, compute a scene onto it.
 */
static void
m_draw ()
{
	Now *np = mm_get_now();

	/* update the info */
	m_info (np);

	/* update data labels */
	f_dm_angle (srlng_w, minfo.srlng);
	f_dm_angle (sslat_w, minfo.sslat);
	f_dm_angle (llat_w, minfo.llat);
	f_dm_angle (llong_w, minfo.llong);
	f_dm_angle (limb_w, minfo.limb);
	f_double (lib_w, "%6.3f", minfo.tilt);

	/* update time stamps too */
	timestamp (np, dt_w);
	timestamp (np, sdt_w);

	if (m_pm)
	    mi_draw ();
}

/* draw moon onto m_pm, using minfo.
 * N.B. this just fills the pixmap; call m_refresh() to copy to the screen.
 */
static void
mi_draw ()
{
	/* check assumptions -- even graphic uses image under the glass */
	if (!mimage) {
	    printf ("No moon mimage!\n");
	    exit (1);
	}
	if (!m_xim) {
	    printf ("No m_xim!\n");
	    exit (1);
	}
	if (!m_pm) {
	    printf ("No m_pm Pixmap!\n");
	    exit(1);
	}

	/* fill in m_xim from mimage */
	mxim_setup ();

	if (option[IMAGE_OPT]) {

	    /* clear m_pm */
	    XFillRectangle (XtD,  m_pm,  m_bgc, 0, 0, mncols+2*BORD,
								mnrows+2*BORD);
	
	    /* copy m_xim to p_pm */
	    XPutImage (XtD, m_pm, m_fgc, m_xim, 0, 0, BORD, BORD,mncols,mnrows);

	    /* and print image if we are doing that */
	    if (XPSDrawing())
		m_imps();

	} else {

	    /* clear m_pm, but leave page white if not printing in color */
	    if (XPSInColor())
		XPSFillRectangle(XtD, m_pm, m_bgc, 0, 0, mncols+2*BORD,
								mnrows+2*BORD);
	    else
		XFillRectangle (XtD,m_pm,m_bgc,0,0,mncols+2*BORD,mnrows+2*BORD);

	    m_schematic();
	}

	/* add eclipse pen/umbra boundaries */
	if (option[UMBRA_OPT])
	    m_umbra();

	/* add the libration marker */
	m_mark_libr ();

	/* add grid and sub points, if enabled */
	if (option[GRID_OPT]) {
	    m_grid();
	    m_sub ();
	}

	/* add any desired feature labels */
	m_labels();

	/* add sky background objects */
	resetSkyObj();
	if (option[SKY_OPT])
	    m_sky();

	/* add orientation markings */
	m_orientation();
}

/* label the database entries marked for name display */
static void
m_labels()
{
	MoonDB *mp;

	XSetFont (XtD, m_agc, m_fsp->fid);

	for (mp = moondb; mp < &moondb[nmoondb]; mp++)
	    if (m_wantlabel(mp)) {
		char *n = mp->name;
		int l = strlen (n);
		int x = FX(mp->x)+BORD;
		int y = FY(mp->y)+BORD;

		if (mp->sz < 0) {
		    /* spacecraft */
		    XPSDrawArc (XtD, m_pm, m_agc, x-2, y-2, 5, 5, 0, 360*64);
		    if (!mp->rjust == option[FLIPLR_OPT]) {
			int dir, asc, des;
			XCharStruct ap;

			XQueryTextExtents (XtD, m_fsp->fid, n, l, &dir, &asc,
								    &des,&ap);
			XPSDrawString (XtD, m_pm, m_agc, x-ap.width-4, y-4,n,l);
		    } else
			XPSDrawString (XtD, m_pm, m_agc, x+4, y-4, n, l);
		} else
		    XPSDrawString (XtD, m_pm, m_agc, x, y, n, l);
	    }
}

/* add background sky objects to m_pm and to skyobjs[] list */
static void
m_sky()
{
	static int before;
	Now *np = mm_get_now();
	double scale;		/* image scale, rads per pixel */
	double mra, mdec;	/* moon's ra and dec */
	double cmdec;		/* cos mdec */
	double mlat, mlng;	/* moon's ecliptic coords */
	double cmlat;		/* cos mlat*/
	double maxr;		/* max dst from center we want to draw, rads */
	DBScan dbs;
	Obj *moonop;
	Obj *op;

	if (!before && pref_get(PREF_EQUATORIAL) == PREF_GEO) {
	    xe_msg ("Equatorial preference should probably be set to Topocentric", 1);
	    before = 1;
	}

	/* get current moon info and derive scale, etc */
	moonop = db_basic (MOON);
	db_update (moonop);
	mra = moonop->s_ra;
	mdec = moonop->s_dec;
	cmdec = cos(mdec);
	eq_ecl (mjd, mra, mdec, &mlat, &mlng);
	cmlat = cos(mlat);
	scale = (degrad(moonop->s_size/3600.0)/2.0) / moonrad;
	maxr = ((mnrows > mncols ? mnrows : mncols)/2 + BORD)*scale;

	/* load field stars */
	m_loadfs (np, mra, mdec);

	/* scan the database and draw whatever is near */
	for (db_scaninit(&dbs, ALLM, fstars, nfstars);
					    (op = db_scan (&dbs)) != NULL; ) {

	    double ra, dec;	/* object's ra/dec */
	    double olat, olng;	/* object's ecliptic lat/lng */
	    double dlat, dlng;	/* diff from moon's */
	    int dx, dy;
	    int x, y;
	    int diam;
	    GC gc;

	    if (is_planet (op, MOON)) {
		/* we draw it elsewhere :-) */
		continue;
	    }

	    db_update (op);
	    ra = op->s_ra;
	    dec = op->s_dec;

	    /* find size, in pixels. */
	    diam = magdiam (FMAG, 2, scale, get_mag(op),
					    degrad(op->s_size/3600.0));
	    /* reject if too faint */
	    if (diam <= 0)
		continue;

	    /* or if it's obviously outside field of view */
	    if (fabs(mdec - dec) > maxr || delra(mra - ra)*cmdec > maxr)
		continue;

	    /* ecliptic coords match moons tilt far better than equatorial */
	    eq_ecl (mjd, ra, dec, &olat, &olng);

	    /* find [x,y] relative to image center */
	    dlat = mlat - olat;
	    dlng = mlng - olng;
	    dx = (int)floor((dlng*cmlat)/scale + 0.5);	/* + right */
	    dy = (int)floor(dlat/scale + 0.5);		/* + down */

	    /* reject if behind too */
	    if (dx*dx + dy*dy < moonrad*moonrad)
		continue;

	    /* allow for flipping and shift to find window coords */
	    x = FX(dx+mncols/2) + BORD;
	    y = FY(dy+mnrows/2) + BORD;

	    /* pick a gc */
	    obj_pickgc(op, toplevel_w, &gc);

	    /* draw 'er */
	    sv_draw_obj (XtD, m_pm, gc, op, x, y, diam, 1);

	    /* add to skyobjs[] list */
	    addSkyObj (op, x, y);
	}

        sv_draw_obj (XtD, m_pm, (GC)0, NULL, 0, 0, 0, 0);        /* flush */
}

/* load field stars around the given location, unless current set is
 * already close enough.
 */
static void
m_loadfs (np, ra, dec)
Now *np;
double ra, dec;
{

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
            (void) sprintf (msg, "Moon View added %d field stars", nfstars);
	    xe_msg (msg, 0);
	    fsdec = dec;
	    fsra = ra;
	}
}

/* given a MoonDB entry, return 1 if we want to show its label now, else 0. */
static int
m_wantlabel (mp)
MoonDB *mp;
{
	double lt, lg;
	int ok;

	/* always show if marked as persistent */
	if (mp->pl)
	    return (1);

	/* or if want spacecraft and it appears to be one and
	 * we are at full size or this is an Apollo site
	 */
	if (option[SPACECRAFT_OPT] && mp->sz < 0
		    && (mshrink == 1 || !strncmp (mp->name, "Apollo", 6)))
	    return (1);

	/* but never show otherwise if disabled */
	if (!option[LABELS_OPT])
	    return (0);

	/* don't show any if just too small */
	if (mshrink > MAXSHRINK/2)
	    return (0);

	/* only show certain types */
	ok = !strcmp (mp->type, "Crater") || !strcmp (mp->type, "Mountain")
	    || !strcmp (mp->type, "Ocean") || !strncmp (mp->type, "Sea", 3);
	if (!ok)
	    return (0);

	/* don't show any near the limb */
	if (xy2ll (mp->x, mp->y, &lt, &lg) < 0)
	    return (0);
	ok = lt < degrad(70) && lt > degrad(-70) &&
					(lg < degrad(70) || lg > degrad(290));
	if (!ok)
	    return (0);

	/* always show these */
	ok = !strcmp(mp->name, "Tycho") || !strcmp (mp->name, "Copernicus") ||
	    !strcmp(mp->name, "Plato") || !strcmp (mp->name, "Aristarchus");

	/* show only features large enough for this scale */
	return (ok || mp->sz > MINKM*mshrink*mshrink);
}

/* draw the N/S E/W labels on the four edges of the m_pm
 * we are showing ecliptic coords.
 */
static void
m_orientation()
{
	int fw, fa, fd;
	int dir, asc, des;
	XCharStruct xcs;

	XQueryTextExtents (XtD, m_fsp->fid, "W", 1, &dir, &asc, &des, &xcs);
	fw = xcs.width;
	fa = xcs.ascent;
	fd = xcs.descent;

	XSetFont (XtD, m_agc, m_fsp->fid);
	XPSDrawString (XtD, m_pm, m_agc, BORD+(mncols-fw)/2, BORD-fd-LGAP,
					    option[FLIPTB_OPT] ? "S" : "N", 1);
	XPSDrawString (XtD, m_pm, m_agc, BORD+(mncols-fw)/2,BORD+mnrows+fa+LGAP,
					    option[FLIPTB_OPT] ? "N" : "S", 1);
	XPSDrawString (XtD, m_pm, m_agc, BORD-fw-LGAP, BORD+(mnrows+fa)/2,
					    option[FLIPLR_OPT] ? "W" : "E", 1);
	XPSDrawString (XtD, m_pm, m_agc, BORD+mncols+LGAP, BORD+(mnrows+fa)/2,
					    option[FLIPLR_OPT] ? "E" : "W", 1);
}

/* draw a coordinate grid over the image moon on m_pm */
static void
m_grid()
{
#define	GSP	degrad(15.0)	/* grid spacing */
#define	FSP	(GSP/4.)	/* fine spacing */
	Display *dsp = XtDisplay (mda_w);
	Window win = m_pm;
	double lt, lg;
	int x, y;

	/* lines of equal lat */
	for (lt = PI/2 - GSP; lt >= -PI/2 + GSP; lt -= GSP) {
	    XPoint xpt[(int)(PI/FSP)+1];
	    int npts = 0;

	    for (lg = -PI/2; lg <= PI/2; lg += FSP) {
		if (ll2xy(lt, lg, &x, &y) < 0)
		    continue;
		if (npts >= XtNumber(xpt)) {
		    printf ("Moon lat grid overflow\n");
		    exit (1);
		}
		xpt[npts].x = FX(x) + BORD;
		xpt[npts].y = FY(y) + BORD;
		npts++;
	    }
	    XPSDrawLines (dsp, win, m_agc, xpt, npts, CoordModeOrigin);
	}

	/* lines of equal longitude */
	for (lg = -PI/2; lg <= PI/2; lg += GSP) {
	    XPoint xpt[(int)(PI/FSP)+1];
	    int npts = 0;

	    for (lt = -PI/2; lt <= PI/2; lt += FSP) {
		if (ll2xy(lt, lg, &x, &y) < 0)
		    continue;
		if (npts >= XtNumber(xpt)) {
		    printf ("Moon lng grid overflow\n");
		    exit (1);
		}
		xpt[npts].x = FX(x) + BORD;
		xpt[npts].y = FY(y) + BORD;
		npts++;
	    }
	    XPSDrawLines (dsp, win, m_agc, xpt, npts, CoordModeOrigin);
	}
}

/* draw an X at the subearth and a circle at the subsolar spot using minfo */
static void
m_sub ()
{
#define	SUBR	4
	Display *dsp = XtDisplay (mda_w);
	Window win = m_pm;
	int x, y;

	/* subearth point */
	if (ll2xy (minfo.llat, minfo.llong, &x, &y) == 0) {
	    x = FX(x) + BORD;
	    y = FY(y) + BORD;
	    XPSDrawLine (dsp, win, m_agc, x-SUBR, y-SUBR, x+SUBR, y+SUBR);
	    XPSDrawLine (dsp, win, m_agc, x-SUBR, y+SUBR, x+SUBR, y-SUBR);
	}

	/* subsolar point -- open circle */
	if (ll2xy (minfo.sslat, minfo.srlng + PI/2, &x, &y) == 0) {
	    x = FX(x) + BORD;
	    y = FY(y) + BORD;
	    XPSDrawArc (dsp, win, m_agc, x-SUBR/2, y-SUBR/2,SUBR,SUBR,0,360*64);
	}

	/* anti-subsolar point -- filled circle */
	if (ll2xy (-minfo.sslat, minfo.srlng - PI/2, &x, &y) == 0) {
	    x = FX(x) + BORD;
	    y = FY(y) + BORD;
	    XPSFillArc (dsp, win, m_agc, x-SUBR/2, y-SUBR/2,SUBR,SUBR,0,360*64);
	}
}

/* create m_xim of size mnrowsXmncols, depth mdepth and bit-per-pixel mbpp.
 * make a Bitmap if only have 1 bit per pixel, otherwise a Pixmap.
 * return 0 if ok else -1 and xe_msg().
 */
static int
mxim_create ()
{
	Display *dsp = XtDisplay (mda_w);
	int nbytes = (mnrows+7)*(mncols+7)*mbpp/8;
	char *data;

	/* get memory for image pixels.  */
	data = (char *) malloc (nbytes);
	if (!data) {
	    char msg[1024];
	    (void)sprintf(msg,"Can not get %d bytes for shadow pixels", nbytes);
	    xe_msg (msg, 1);
	    return (-1);
	}

	/* create the XImage */
	m_xim = XCreateImage (dsp, XDefaultVisual (dsp, DefaultScreen(dsp)),
	    /* depth */         mbpp == 1 ? 1 : mdepth,
	    /* format */        mbpp == 1 ? XYBitmap : ZPixmap,
	    /* offset */        0,
	    /* data */          data,
	    /* width */         mncols,
	    /* height */        mnrows,
	    /* pad */           mbpp < 8 ? 8 : mbpp,
	    /* bpl */           0);
	if (!m_xim) {
	    xe_msg ("Can not create shadow XImage", 1);
	    free ((void *)data);
	    return (-1);
	}

        m_xim->bitmap_bit_order = LSBFirst;
	m_xim->byte_order = LSBFirst;

	/* ok */
	return (0);
}

/* store intensity v into array[x,y] (which is really m_xim) */
static void
mxim_pixel (array, x, y, v)
void *array;	/* target of pixels */
int x, y;	/* coord of pixel */
int v;		/* value */
{
	Pixel p = mgray[v];

	XPutPixel ((XImage *)array, x, y, p);
}

/* fill in m_xim from mimage and knowledge of the sunrise longitude and
 * earthshine.
 */
static void
mxim_setup ()
{
	image_setup (mxim_pixel, (void *)m_xim);
}

/* compute a scene from mimage, knowledge of the sunrise longitude and
 * earthshine.
 * store the image using (*fp)(arg, x, y, gray_value).
 */
static void
image_setup (fp, arg)
void (*fp)();	/* function to set a pixel in arg */
void *arg;	/* where to store pixels */
{
	double esrlng = minfo.srlng-CLNG; /* correct for earth viewpoint */
	int right = cos(esrlng) < 0;	  /*whther shadw reaches to right limb*/
	double sinsrl = sin(esrlng);
	unsigned char *mp;
	int x, y;
	int esf;

	get_something (eshine_w, XmNvalue, (XtArgVal)&esf);

	/* copy intensities through mgray[] map to get pixels */
	for (y = 0; y < mnrows; y++) {
	    int lx, rx;		/* left and right edge of scan line to darken */
	    int fy;		/* (possibly) flipped y */

	    (void) m_esedges (y, sinsrl, right, &lx, &rx);
	    fy = FY(y);

	    /* scan across the whole row, drawing shadow between lx and rx */
	    mp = &mimage[fy*mncols];
	    for (x = 0; x < mncols; x++) {
		int i = (int)(*mp++);
		int v;

		if (x >= lx && x <= rx)
		    v = i*nmgray*esf/ESHINEF/256; /* shadow */
		else
		    v = i*nmgray/256;

		(*fp) (arg, x, fy, v);
	    }
	}
}

/* draw a schematic scene in m_pm from knowledge of the sunrise longitude
 * and earthshine.
 */
static void
m_schematic ()
{
#define	NESV	60	/* N earthshine polygon vertices */
	static GC esgc;
	Display *dsp = XtDisplay(mda_w);
	double esrlng = minfo.srlng-CLNG; /* correct for earth viewpoint */
	int right = cos(esrlng) < 0;	  /*whther shadw reaches to right limb*/
	double sinsrl = sin(esrlng);
	XPoint *xps;
	int nxps;
	int esf;
	int i;

	/* get memory for polygon points */
	xps = (XPoint *) malloc (NESV * sizeof(XPoint));
	if (!xps) {
	    xe_msg ("No memory for moon schematic", 1);
	    return;
	}
	nxps = 0;

	/* make sure we have a GC to use */
	if (!esgc)
	    esgc = XCreateGC (dsp, m_pm, (unsigned int)0, (XGCValues *)0);

	/* start with full white moon -- b/w paper just needs border.
	 * also, just a bit small since earthshine is a bit coarse.
	 */
	XSetForeground (dsp, esgc, mgray[nmgray-1]);
	if (XPSInColor())
	    XPSFillArc (dsp, m_pm, esgc, BORD+leftmarg+2, BORD+topmarg+2,
					2*moonrad-4, 2*moonrad-4, 0, 360*64);
	else {
	    XPSDrawArc (dsp, m_pm, esgc, BORD+leftmarg+2, BORD+topmarg+2,
					2*moonrad-4, 2*moonrad-4, 0, 360*64);
	    XFillArc (dsp, m_pm, esgc, BORD+leftmarg+2, BORD+topmarg+1,
					2*moonrad-4, 2*moonrad-4, 0, 360*64);
	}

	/* get earthshine value */
	get_something (eshine_w, XmNvalue, (XtArgVal)&esf);

	/* overlay earthshine polygon -- down left edge then up right edge.
	 * divide around the circle edge evenly.
	 */
	for (i = 0; i < NESV/2; i++) {
	    int y = (int)(topmarg + moonrad*(1+cos(i*PI/(NESV/2))) + 0.5);
	    int lx, rx;		/* left and right edge of scan line to darken */

	    if (m_esedges (y, sinsrl, right, &lx, &rx) < 0)
		continue;

	    xps[nxps].x = lx + BORD;
	    xps[nxps].y = FY(y) + BORD;
	    nxps++;
	}
	for (i = NESV/2; i < NESV; i++) {
	    int y = (int)(topmarg + moonrad*(1+cos(i*PI/(NESV/2))) + 0.5);
	    int lx, rx;		/* left and right edge of scan line to darken */

	    if (m_esedges (y, sinsrl, right, &lx, &rx) < 0)
		continue;
	    xps[nxps].x = rx + BORD;
	    xps[nxps].y = FY(y) + BORD;
	    nxps++;
	}

	if (nxps > NESV) {
	    printf ("Moonmenu: nxps=%d but NESV=%d\n", nxps, NESV);
	    exit (1);
	}
	XSetForeground (dsp, esgc, mgray[(nmgray-1)*esf/ESHINEF]);
	XPSFillPolygon (dsp, m_pm, esgc, xps, nxps, Complex, CoordModeOrigin);

	free ((void *)xps);
}

/* compute the left and right edge of earthshine for the given y.
 * all coords wrt to m_xim and current orientation.
 * sinsrl is sin of longitude of the rising sun.
 */
static int
m_esedges (y, sinsrl, right, lxp, rxp)
int y;		/* image y */
double sinsrl;	/* sin of longitude of the rising sun */
int right;	/* whether shadow reaches to right limb */
int *lxp, *rxp;	/* left and right x of edges of earthshine */
{
	int lx, rx;	/* left and right edge of scan line to darken */
	int yc;		/* y with respect to moon center */
	int ret;	/* return value */

	yc = y - (topmarg + moonrad);

	/* find left and right edge of shadow */
	if (abs(yc) <= moonrad) {
	    int r;		/* pixels to limb at this y */

	    r = (int)(sqrt((double)(moonrad*moonrad - yc*yc))+1); /* round up */

	    /* compute shadow edges with respect to center */
	    if (right) {
		rx = r;
		lx = -(int)(r * sinsrl);
	    } else {
		lx = -r;
		rx = (int)(r * sinsrl);
	    }

	    /* convert to X coords */
	    lx += leftmarg + moonrad;
	    rx += leftmarg + moonrad;

	    ret = 0;
	} else {
	    /* above or below moon image so no shadow */
	    lx = -1;
	    rx = mncols;
	    ret = -1;
	}

	/* allow for flipping */
	if (option[FLIPLR_OPT]) {
	    int tmp = lx;
	    lx = FX(rx);
	    rx = FX(tmp);
	}

	*lxp = lx;
	*rxp = rx;

	return (ret);
}

/* do everything necessary to see a fresh image, shrunk by factor f:
 * read full moon file; shrink by a factor of f; replace mimage with result;
 * resize mda_w and m_xim; reread database (so [x,y] values match new scale).
 * when done, set mimage, m_xim, mncols, mnrows, topmarg, leftmarg and moonrad.
 * return 0 if all ok, else xe_msg() and return -1.
 */
static int
m_shrink (f)
int f;
{
	char msg[1024];
	char fn[1024];
	FImage moonfits;
	unsigned char *newim, *im;
	int imnr, imnc;
	int newnr, newnc;
	int v;
	int fd;

	/* open moon image */
	(void) sprintf (fn, "%s/auxil/moon.fts",  getShareDir());
	fd = openh (fn, 0);
	if (fd < 0) {
	    (void) sprintf (msg, "%s: %s\n", fn, syserrstr());
	    xe_msg (msg, 1);
	    return (-1);
	}

	/* read moon file into moonfits */
	if (readFITS (fd, &moonfits, msg) < 0) {
	    char msg2[1024];
	    (void) sprintf (msg2, "%s: %s", fn, msg);
	    xe_msg (msg2, 1);
	    (void) close (fd);
	    return (-1);
	}
	(void) close (fd);

	/* make sure it's the new flipped version */
	if (getIntFITS (&moonfits, "XEVERS", &v) < 0 || v != XEVERS) {
	    (void) sprintf (msg, "%s: Incorrect version", fn);
	    xe_msg (msg, 1);
	    return (-1);
	}

	/* make some local shortcuts */
	im = (unsigned char *) moonfits.image;
	imnr = moonfits.sh;
	imnc = moonfits.sw;

	/* make some sanity checks */

	if (moonfits.bitpix != 8 || imnr != MNROWS || imnc != MNCOLS) {
	    (void) sprintf (msg, "%s: Expected %d x %d but found %d x %d",
						fn, MNROWS, MNCOLS, imnr, imnc);
	    xe_msg (msg, 1);
	    resetFImage (&moonfits);
	    return (-1);
	}

	/* set newim to resized version of im (it _is_ im if f is just 1) */
	if (f != 1) {
	    unsigned char *newmem;

	    newnr = imnr / f;
	    newnc = imnc / f;

	    /* get memory for resized copy */
	    newmem = (unsigned char *) malloc (newnr * newnc);
	    if (!newmem) {
		(void)sprintf (msg,"No mem for 1/%dX %d x %d -> %d x %d resize",
						f, imnr, imnc, newnr, newnc);
		xe_msg (msg, 1);
		free ((void *)im);
		return (-1);
	    }

	    m_resize (im, MNROWS, MNCOLS, f, newmem);
	    newim = newmem;
	    free ((void *)im);
	} else {
	    newim = im;
	    newnr = MNROWS;
	    newnc = MNCOLS;
	}

	/* commit newim to mimage -- set global metrics */
	if (mimage) {
	    free ((void *)mimage);
	    mimage = NULL;
	}
	mimage = newim;
	mnrows = newnr;
	mncols = newnc;
	topmarg = (int)((double)TOPMARG/f + 0.5);
	leftmarg = (int)((double)LEFTMARG/f + 0.5);
	moonrad = (int)((double)MOONRAD/f + 0.5);

	/* dither mimage if we only have 2 colors to work with */
	if (mbpp == 1) 
	    mBWdither();

	/* flip mimage, as desired */
	if (option[FLIPTB_OPT])
	    fliptb();
	if (option[FLIPLR_OPT])
	    fliplr();

	/* (re)create the X image */
	if (m_xim) {
	    free ((void *)m_xim->data);
	    m_xim->data = NULL;
	    XDestroyImage (m_xim);
	    m_xim = NULL;
	}
	if (mxim_create() < 0) {
	    resetFImage (&moonfits);
	    mimage = NULL;
	    return (-1);
	}

	/* (re)read moon database */
	m_readdb();

	/* delete pixmap so new one will get built on next expose */
	if (m_pm) {
	    XFreePixmap (XtD, m_pm);
	    m_pm = 0;
	}

	/* size the drawing area to hold the new image plus a border.
	 * this will give us an expose event to show the new image.
	 * also center the scrollbars.
	 */
	m_sizewidgets ();
	centerScrollBars(msw_w);

	return(0);
}

/* set size of mda_w as desired based on mnrows and mncols */
static void
m_sizewidgets ()
{
	int neww = mncols + 2*BORD;
	int newh = mnrows + 2*BORD;

	/* mda_w should be image size + BORD all around */
	set_something (mda_w, XmNwidth, (XtArgVal)neww);
	set_something (mda_w, XmNheight, (XtArgVal)newh);
}

/* copy image in, of size nr x nr, to out by shrinking a factor f.
 * N.B. we assume out is (nr/f) * (nr/f) bytes.
 */
static void
m_resize(in, nr, nc, f, out)
unsigned char *in, *out;
int nr, nc;
int f;
{
	int outx, outy;
	int noutx, nouty;

	noutx = nc/f;
	nouty = nr/f;
	for (outy = 0; outy < nouty; outy++) {
	    unsigned char *inrp = &in[outy*f*nr];
	    for (outx = 0; outx < noutx; outx++) {
		*out++ = *inrp;
		inrp += f;
	    }
	}
}

/* dither mimage into a 2-intensity image: 0 and 255.
 * form 2x2 tiles whose pattern depends on intensity peak and spacial layout.
 */
static void
mBWdither()
{
	int idx[4];
	int y;

	idx[0] = 0;
	idx[1] = 1;
	idx[2] = mncols;
	idx[3] = mncols+1;

	for (y = 0; y < mnrows - 1; y += 2) {
	    unsigned char *mp = &mimage[y*mncols];
	    unsigned char *lp;

	    for (lp = mp + mncols - 1; mp < lp; mp += 2) {
		int sum, numon;
		int i;

		sum = 0;
		for (i = 0; i < 4; i++)
		    sum += (int)mp[idx[i]];
		numon = sum*5/1021;	/* 1021 is 255*4 + 1 */

		for (i = 0; i < 4; i++)
		    mp[idx[i]] = 0;

		switch (numon) {
		case 0:
		    break;
		case 1:
		case 2:
		    mp[idx[0]] = 255;
		    break;
		case 3:
		    mp[idx[0]] = 255;
		    mp[idx[1]] = 255;
		    mp[idx[3]] = 255;
		    break;
		case 4:
		    mp[idx[0]] = 255;
		    mp[idx[1]] = 255;
		    mp[idx[2]] = 255;
		    mp[idx[3]] = 255;
		    break;
		default:
		    printf ("Bad numon: %d\n", numon);
		    exit(1);
		}
	    }
	}
}

/* draw umbra and penumbra boundaries */
static void
m_umbra()
{
	Now *np = mm_get_now();
	Obj *mop = db_basic (MOON);
	Obj *sop = db_basic (SUN);
	double scale;		/* image scale, rads per pixel */
	double mrad;		/* moon radius, rads */
	double ara, adec;	/* anti-solar loc, equatorial coords, rads */
	double alat, alng;	/* anti-solar loc, ecliptic coords, rads */
	double mlat, mlng;	/* moon loc, ecliptic coords, rads */
	double dlat, dlng;	/* difference, rads */
	double cmlat;		/* cos moon's lat */
	double sed, med;	/* sun-earth and moon-earth dist, au */
	double u, p;		/* umbra and penumbra radius, m at moon dist*/
	double maxr;
	int dx, dy;
	int x, y, r;

	/* get basic info */
	db_update (mop);
	db_update (sop);
	mrad = degrad(mop->s_size/3600.0)/2.0;
	scale = mrad / moonrad;
	ara = sop->s_gaera + PI;
	adec = -sop->s_gaedec;
	eq_ecl (mjd, ara, adec, &alat, &alng);
	eq_ecl (mjd, mop->s_gaera, mop->s_gaedec, &mlat, &mlng);
	dlat = mlat - alat;
	dlng = mlng - alng;
	cmlat = cos (mlat);

	/* skip if obviously outside fov */
	maxr = (sqrt((double)mnrows*mnrows + mncols*mncols) + BORD)*scale;
	if (fabs(dlat) > maxr || delra(dlng)*cmlat > maxr)
	    return;

	/* find center of circle */
	dx = (int)floor((dlng*cmlat)/scale + 0.5);	/* + right */
	dy = (int)floor(dlat/scale + 0.5);		/* + down */
	x = FX(dx+mncols/2) + BORD;
	y = FY(dy+mnrows/2) + BORD;

	/* compute diameters at moon (based on similar triangles) */
	med = mop->s_edist;
	sed = sop->s_edist;
	u = ERAD - med/sed*(SRAD-ERAD);
	r = (int)floor(u/MRAD*moonrad + 0.5);
	XPSDrawArc (XtD, m_pm, m_fgc, x-r, y-r, 2*r, 2*r, 0, 360*64);
	p = ERAD + med/sed*(SRAD-ERAD);
	r = (int)floor(p/MRAD*moonrad + 0.5);
	XSetLineAttributes (XtD, m_fgc, 0, LineOnOffDash, CapButt, JoinMiter);
	XPSDrawArc (XtD, m_pm, m_fgc, x-r, y-r, 2*r, 2*r, 0, 360*64);
	XSetLineAttributes (XtD, m_fgc, 0, LineSolid, CapButt, JoinMiter);

	/* tried to draw a line along path by computing ecl loc +- a few
	 * hours from now but doesn't work since the perspective changes.
	 */
}

/* flip mimage left/right */
static void
fliplr()
{
	int x, y;

	for (y = 0; y < mnrows; y++) {
	    unsigned char *rp = &mimage[y*mncols];
	    for (x = 0; x < mncols/2; x++) {
		unsigned char tmp = rp[x];
		rp[x] = rp[mncols-x-1];
		rp[mncols-x-1] = tmp;
	    }
	}
}

/* flip mimage top/bottom.
 * N.B. will flip back option if can't do it for some reason.
 */
static void
fliptb()
{
	char buf[2048];		/* plenty :-) */
	int y;

	if (mncols > sizeof(buf)) {
	    (void) sprintf (buf, "Can not flip -- rows are longer than %d",
	    							sizeof(buf));
	    xe_msg (buf, 1);
	    XmToggleButtonSetState (option_w[FLIPTB_OPT],
						option[FLIPTB_OPT] ^= 1, False);
	    return;
	}
	    

	for (y = 0; y < mnrows/2; y++) {
	    unsigned char *r0 = &mimage[y*mncols];
	    unsigned char *r1 = &mimage[(mnrows-y-1)*mncols];

	    (void) memcpy (buf, (void *)r0, mncols);
	    (void) memcpy ((void *)r0, (void *)r1, mncols);
	    (void) memcpy ((void *)r1, buf, mncols);
	}
}

/* draw a marker on m_pm to show the limb angle favored by libration */
static void
m_mark_libr ()
{
	int r;		/* radius of marker */
	int x, y;	/* center of marker */

	r = moonrad/LMFRAC;
	x = leftmarg + (int)(moonrad*(1 - sin(minfo.limb)));
	y = topmarg  + (int)(moonrad*(1 - cos(minfo.limb)));

	x = FX(x);
	y = FY(y);

	XPSFillArc (XtD, m_pm, m_agc, BORD+x-r, BORD+y-r, 2*r, 2*r, 0, 360*64);
}

/* report the location of x,y, which are with respect to mda_w.
 * N.B. allow for flipping and the borders.
 */
static void
m_reportloc (dsp, win, x, y)
Display *dsp;
Window win;
int x, y;
{
	double lt, lg;

	/* convert from mda_w coords to m_xim coords */
	x -= BORD;
	y -= BORD;

	if (xy2ll (FX(x), FY(y), &lt, &lg) == 0) {
	    Now *np = mm_get_now();
	    double lmjd = pref_get(PREF_ZONE)==PREF_LOCALTZ ? mjd-tz/24.0 : mjd;
	    double srlng;
	    double t;
	    double a;

	    f_dm_angle (lat_w, lt);
	    f_dm_angle (lng_w, lg);

	    moon_colong (mjd+MJD0, lt, lg, &srlng, NULL, &a, NULL);
	    f_dm_angle (sunalt_w, a);

	    srlng = -srlng;
	    range (&srlng, 2*PI);

	    t = srlng-lg;
	    range (&t, 2*PI);
	    t = lmjd + t/(2*PI)*SYNP;
	    f_date (nsrd_w, mjd_day(t));
	    f_mtime (nsrt_w, mjd_hr(t));

	    t = srlng-lg+PI;
	    range (&t, 2*PI);
	    t = lmjd + t/(2*PI)*SYNP;
	    f_date (nssd_w, mjd_day(t));
	    f_mtime (nsst_w, mjd_hr(t));

	} else {
	    set_xmstring (lat_w, XmNlabelString, " ");
	    set_xmstring (lng_w, XmNlabelString, " ");
	    set_xmstring (nsrt_w, XmNlabelString, " ");
	    set_xmstring (nsrd_w, XmNlabelString, " ");
	    set_xmstring (nsst_w, XmNlabelString, " ");
	    set_xmstring (nssd_w, XmNlabelString, " ");
	}
}

/* make glass_xim of size GLASSSZ*GLASSMAG and same genre as m_xim.
 * leave glass_xim NULL if trouble.
 */
static void
makeGlassImage (dsp)
Display *dsp;
{
	int nbytes = (GLASSSZ*GLASSMAG+7) * (GLASSSZ*GLASSMAG+7) * mbpp/8;
	char *glasspix = (char *) malloc (nbytes);

	if (!glasspix) {
	    char msg[1024];
	    (void) sprintf (msg, "Can not malloc %d for Glass pixels", nbytes);
	    xe_msg (msg, 0);
	    return;
	}

	glass_xim = XCreateImage (dsp, XDefaultVisual (dsp, DefaultScreen(dsp)),
	    /* depth */         m_xim->depth,
	    /* format */        m_xim->format,
	    /* offset */        0,
	    /* data */          glasspix,
	    /* width */         GLASSSZ*GLASSMAG,
	    /* height */        GLASSSZ*GLASSMAG,
	    /* pad */           mbpp < 8 ? 8 : mbpp,
	    /* bpl */           0);

	if (!glass_xim) {
	    free ((void *)glasspix);
	    xe_msg ("Can not make Glass XImage", 0);
	    return;
	}

        glass_xim->bitmap_bit_order = LSBFirst;
	glass_xim->byte_order = LSBFirst;
}

/* make glassGC */
static void
makeGlassGC (dsp, win)
Display *dsp;
Window win;
{
	XGCValues gcv;
	unsigned int gcm;
	Pixel p;

	(void) get_color_resource (mda_w, "GlassBorderColor", &p);
	gcm = GCForeground;
	gcv.foreground = p;
	glassGC = XCreateGC (dsp, win, gcm, &gcv);
}

/* handle the operation of the magnifying glass.
 * this is called whenever there is left button activity over the image.
 */
static void
doGlass (dsp, win, b1p, m1, b1r, wx, wy)
Display *dsp;
Window win;
int b1p, m1, b1r;	/* button/motion state */
int wx, wy;		/* window coords of cursor */
{
	static int lastwx, lastwy;
	int rx, ry, rw, rh;		/* region */

	/* check for first-time stuff */
	if (!glass_xim)
	    makeGlassImage (dsp);
	if (!glass_xim)
	    return; /* oh well */
	if (!glassGC)
	    makeGlassGC (dsp, win);

	if (m1) {

	    /* motion: put back old pixels that won't just be covered again */

	    /* first the vertical strip that is uncovered */

	    rh = GLASSSZ*GLASSMAG;
	    ry = lastwy - (GLASSSZ*GLASSMAG/2);
	    if (ry < 0) {
		rh += ry;
		ry = 0;
	    }
	    if (wx < lastwx) {
		rw = lastwx - wx;	/* cursor moved left */
		rx = wx + (GLASSSZ*GLASSMAG/2);
	    } else {
		rw = wx - lastwx;	/* cursor moved right */
		rx = lastwx - (GLASSSZ*GLASSMAG/2);
	    }
	    if (rx < 0) {
		rw += rx;
		rx = 0;
	    }

	    if (rw > 0 && rh > 0)
		XCopyArea (dsp, m_pm, win, m_fgc, rx, ry, rw, rh, rx, ry);

	    /* then the horizontal strip that is uncovered */

	    rw = GLASSSZ*GLASSMAG;
	    rx = lastwx - (GLASSSZ*GLASSMAG/2);
	    if (rx < 0) {
		rw += rx;
		rx = 0;
	    }
	    if (wy < lastwy) {
		rh = lastwy - wy;	/* cursor moved up */
		ry = wy + (GLASSSZ*GLASSMAG/2);
	    } else {
		rh = wy - lastwy;	/* cursor moved down */
		ry = lastwy - (GLASSSZ*GLASSMAG/2);
	    }
	    if (ry < 0) {
		rh += ry;
		ry = 0;
	    }

	    if (rw > 0 && rh > 0)
		XCopyArea (dsp, m_pm, win, m_fgc, rx, ry, rw, rh, rx, ry);
	}

	if (b1p || m1) {

	    /* start or new location: show glass and save new location */

	    fillGlass (wx-BORD, wy-BORD);
	    XPutImage (dsp, win, m_fgc, glass_xim, 0, 0,
			wx-(GLASSSZ*GLASSMAG/2), wy-(GLASSSZ*GLASSMAG/2),
			GLASSSZ*GLASSMAG, GLASSSZ*GLASSMAG);
	    lastwx = wx;
	    lastwy = wy;

	    /* kinda hard to tell boundry of glass so draw a line around it */
	    XDrawRectangle (dsp, win, glassGC,
			wx-(GLASSSZ*GLASSMAG/2), wy-(GLASSSZ*GLASSMAG/2),
			GLASSSZ*GLASSMAG-1, GLASSSZ*GLASSMAG-1);
	}

	if (b1r) {

	    /* end: restore all old pixels */

	    rx = lastwx - (GLASSSZ*GLASSMAG/2);
	    rw = GLASSSZ*GLASSMAG;
	    if (rx < 0) {
		rw += rx;
		rx = 0;
	    }

	    ry = lastwy - (GLASSSZ*GLASSMAG/2);
	    rh = GLASSSZ*GLASSMAG;
	    if (ry < 0) {
		rh += ry;
		ry = 0;
	    }

	    if (rw > 0 && rh > 0)
		XCopyArea (dsp, m_pm, win, m_fgc, rx, ry, rw, rh, rx, ry);
	}
}

/* fill glass_xim with GLASSSZ*GLASSMAG view of m_xim centered at coords
 * xc,yc. take care at the edges (m_xim is mnrows x mncols)
 */
static void
fillGlass (xc, yc)
int xc, yc;
{
	int sx, sy;	/* coords in m_xim */
	int gx, gy;	/* coords in glass_xim */
	int i, j;

	gy = 0;
	gx = 0;
	for (sy = yc-GLASSSZ/2; sy < yc+GLASSSZ/2; sy++) {
	    for (sx = xc-GLASSSZ/2; sx < xc+GLASSSZ/2; sx++) {
		Pixel p;

		if (sx < 0 || sx >= mncols || sy < 0 || sy >= mnrows)
		    p = XGetPixel (m_xim, 0, 0);
		else
		    p = XGetPixel (m_xim, sx, sy);
		for (i = 0; i < GLASSMAG; i++)
		    for (j = 0; j < GLASSMAG; j++)
			XPutPixel (glass_xim, gx+i, gy+j, p);
		gx += GLASSMAG;
	    }
	    gx = 0;
	    gy += GLASSMAG;
	}
}

/* read Moon database: fill up moodb[] and set nmoondb.
 * N.B. to fill in x,y we assume the lat/long match the image orientation in
 *   its original form, ie, up N right E.
 * if fail, leave moondb NULL.
 */
static void
m_readdb()
{
#define	NDBCHUNKS	32	/* malloc room for these many more each time */
	char msg[1024];
	char buf[1024];
	char fn[1024];
	int ndb;
	FILE *fp;

	/* open file */
	(void) sprintf (fn, "%s/auxil/moon_db",  getShareDir());
	fp = fopenh (fn, "r");
	if (!fp) {
	    (void) sprintf (msg, "%s: %s", fn , syserrstr());
	    xe_msg (msg, 1);
	    return;
	}

	/* reset moondb[] and add an initial fresh chunk */
	m_freedb();
	moondb = (MoonDB *) malloc (NDBCHUNKS * sizeof(MoonDB));
	if (!moondb) {
	    xe_msg ("No memory for moon database", 1);
	    (void) fclose (fp);
	    return;
	}
	ndb = NDBCHUNKS;

	watch_cursor (1);

	/* read and add each line to list */
	while (fgets (buf, sizeof(buf), fp)) {
	    MoonDB *mp;
	    char name[128];
	    char type[128];
	    double lt, lg;
	    int x, y;
	    char *new;
	    int km, rjust;
	    int n;
	    int i;

	    /* crack the line -- just skip it if not recognized */
	    n = sscanf(buf,"%[^|]|%[^|]|%lf|%lf|%d|%d", name, type, &lt,
							    &lg, &km, &rjust);
	    if (n != 6)
		continue;

	    /* grow moondb if now filled */
	    if (nmoondb == ndb) {
		new = realloc ((void *)moondb, (ndb+NDBCHUNKS)*sizeof(MoonDB));
		if (!new) {
		    xe_msg ("Short of moon DB memory", 1);
		    break;	/* go with what we have now */
		}

		ndb += NDBCHUNKS;
		moondb = (MoonDB *)new;
	    }
	    mp = &moondb[nmoondb];

	    /* build new entry at mp */

	    /* location */
	    lt = degrad(lt);
	    lg = degrad(lg);
	    if (ll2xy (lt, lg, &x, &y) < 0)
		continue;	/* off the image */
	    mp->x = x;
	    mp->y = y;

	    /* copy name */
	    new = malloc (strlen (name) + 1);
	    if (!new) {
		xe_msg ("Short of 'name' memory for moon DB", 1);
		break;	/* go with what we have */
	    }
	    mp->name = strcpy (new, name);

	    /* copy type, but try to reuse */
	    for (i = 0; i < nmoondb; i++)
		if (strcmp (moondb[i].type, type) == 0) {
		    mp->type = moondb[i].type;
		    break;	/* reuse type string */
		}
	    if (i == nmoondb) {
		/* first time we've seen this type */
		new = malloc (strlen (type) + 1);
		if (!new) {
		    xe_msg ("Short of 'type' memory for moon DB", 1);
		    free ((void *)mp->name);
		    break;	/* go with what we have */
		}
		mp->type = strcpy (new, type);
	    }

	    /* radius as seen overhead, km */
	    mp->sz = km;

	    /* initialy not persistent label and set justification */
	    mp->pl = 0;
	    mp->rjust = rjust;

	    /* all ok */
	    nmoondb++;
	}

	/* finished with file */
	(void) fclose (fp);

	/* warn if nothing found */
	if (nmoondb == 0) {
	    (void) sprintf (msg, "%s: No moon database entries found", fn);
	    xe_msg (msg, 0);
	}

	/* ok */
	watch_cursor(0);
}

/* free moondb[] and the names and types.
 * N.B. beware that type strings may not all be unique.
 * N.B. we allow for moondb being already NULL.
 */
static void
m_freedb()
{
	int i;

	if (moondb == NULL)
	    return;

	for (i = 0; i < nmoondb; i++) {
	    MoonDB *mp = &moondb[i];

	    /* free name for sure -- each is unique */
	    free ((void *)mp->name);

	    /* free type once -- pointer might be reused */
	    if (mp->type) {
		int j;

		free ((void *)mp->type);
		for (j = i+1; j < nmoondb; j++)
		    if (moondb[j].type == mp->type)
			moondb[j].type = NULL;
	    }
	}

	/* now free the array itself */
	free ((void *)moondb);
	moondb = NULL;
	nmoondb = 0;
}

/* free the list of sky objects, if any */
static void
resetSkyObj ()
{
	if (skyobjs) {
	    free ((void *) skyobjs);
	    skyobjs = NULL;
	}
	nskyobjs = 0;
}

/* add op at [x,y] to the list of background sky objects */
static void
addSkyObj (op, x, y)
Obj *op;
int x, y;
{
	char *newmem;

	if (skyobjs)
	    newmem = realloc ((void *)skyobjs, (nskyobjs+1) * sizeof(SkyObj));
	else
	    newmem = malloc (sizeof(SkyObj));
	if (!newmem) {
	    xe_msg ("No mem for more sky objects", 1);
	    return;
	}

	skyobjs = (SkyObj *) newmem;
	skyobjs[nskyobjs].o = *op;
	skyobjs[nskyobjs].x = x;
	skyobjs[nskyobjs].y = y;
	nskyobjs++;
}

/* find the object in skyobjs[] that is closest to [x,y].
 * return NULL if none within MAXR.
 */
static Obj *
closeSkyObj (x, y)
int x, y;
{
	SkyObj *closesp = NULL;
	int mind = 0;
	int i;

	for (i = 0; i < nskyobjs; i++) {
	    SkyObj *sp = &skyobjs[i];
	    int d = abs(sp->x - x) + abs(sp->y - y);

	    if (!closesp || d < mind) {
		mind = d;
		closesp = sp;
	    }
	}

	if (closesp && mind <= MAXR)
	    return (&closesp->o);
	return (NULL);
}

/* called when hit button 3 over image */
static void
m_popup (ep)
XEvent *ep;
{
	XButtonEvent *bep;
	int x, y;

	bep = &ep->xbutton;
	x = bep->x;
	y = bep->y;

	if (overMoon (x, y)) {
	    MoonDB *mp = closeMoonDB (x-BORD, y-BORD);
	    fill_popup (mp, x-BORD, y-BORD);
	    XmMenuPosition (pu_w, (XButtonPressedEvent *)ep);
	    XtManageChild (pu_w);
	} else {
	    Obj *op = closeSkyObj (x, y);
	    if (op) {
		fill_skypopup (op);
		XmMenuPosition (skypu_w, (XButtonPressedEvent *)ep);
		XtManageChild (skypu_w);
	    }
	}
}

/* create the popup menu */
static void
m_create_popup()
{
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNisAligned, True); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	pu_w = XmCreatePopupMenu (mda_w, "MPU", args, n);

	n = 0;
	pu_name_w = XmCreateLabel (pu_w, "MPN", args, n);
	wtip (pu_name_w, "Name of feature");
	XtManageChild (pu_name_w);

	n = 0;
	pu_type_w = XmCreateLabel (pu_w, "MPT", args, n);
	wtip (pu_type_w, "Basic type of feature");
	XtManageChild (pu_type_w);

	n = 0;
	pu_size_w = XmCreateLabel (pu_w, "MPS", args, n);
	wtip (pu_size_w, "Nominal size of feature");
	XtManageChild (pu_size_w);

	n = 0;
	pu_lat_w = XmCreateLabel (pu_w, "MPLat", args, n);
	wtip (pu_lat_w, "Selenographic latitude of this feature");
	XtManageChild (pu_lat_w);

	n = 0;
	pu_lng_w = XmCreateLabel (pu_w, "MPLng", args, n);
	wtip (pu_lng_w, "Selenographic longitude of this feature");
	XtManageChild (pu_lng_w);

	n = 0;
	pu_alt_w = XmCreateLabel (pu_w, "MPAlt", args, n);
	wtip (pu_alt_w, "Altitude of Sun above horizon at this feature");
	XtManageChild (pu_alt_w);

	/* persistent label TB -- managed as needed */
	n = 0;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
	pu_pl_w = XmCreateToggleButton (pu_w, "MPPL", args, n);
	XtAddCallback (pu_pl_w, XmNvalueChangedCallback, m_pl_cb, 0);
	set_xmstring (pu_pl_w, XmNlabelString, "Persistent label");
	wtip (pu_pl_w, "Whether to always label this object on the map.");
}

/* fill in popup widgets from mp. if mp == NULL use x,y and skip name.
 * x and y are in image coords, (as in m_xim), not mda_w.
 * N.B. since we assume the database matches the original image orientation
 *   there is no need to correct mp->{x,y} for flipping.
 */
static void
fill_popup (mp, x, y)
MoonDB *mp;
int x, y;
{
	Now *np = mm_get_now();
	double lt, lg;
	double a;
	char str[64];

	if (mp) {
	    XtManageChild (pu_name_w);
	    f_showit (pu_name_w, mp->name);
	    XtManageChild (pu_type_w);
	    f_showit (pu_type_w, mp->type);
	    XtManageChild (pu_size_w);
	    if (mp->sz < 0)
		f_double (pu_size_w, "%.0f", -(double)mp->sz);
	    else if (pref_get(PREF_UNITS) == PREF_ENGLISH)
		f_double (pu_size_w, "%.0f mi", (double)mp->sz/1.609);
	    else
		f_double (pu_size_w, "%.0f km", (double)mp->sz);
	    XtManageChild (pu_pl_w);
	    XmToggleButtonSetState (pu_pl_w, mp->pl, False);
	    set_something (pu_pl_w, XmNuserData, (XtArgVal)mp);
	    xy2ll (mp->x, mp->y, &lt, &lg);
	} else {
	    XtUnmanageChild (pu_name_w);
	    XtUnmanageChild (pu_type_w);
	    XtUnmanageChild (pu_size_w);
	    XtUnmanageChild (pu_pl_w);
	    xy2ll (FX(x), FY(y), &lt, &lg);
	}

	(void) strcpy (str, "Lat: ");
	if (lt < 0) {
	    fs_dm_angle (str+5, -lt);
	    (void) strcat (str, " S");
	} else {
	    fs_dm_angle (str+5, lt);
	    (void) strcat (str, " N");
	}
	f_showit (pu_lat_w, str);

	(void) strcpy (str, "Long: ");
	if (lg > PI) {
	    fs_dm_angle (str+6, 2*PI-lg);
	    (void) strcat (str, " W");
	} else {
	    fs_dm_angle (str+6, lg);
	    (void) strcat (str, " E");
	}
	f_showit (pu_lng_w, str);

	moon_colong (mjd+MJD0, lt, lg, NULL, NULL, &a, NULL);
	(void) strcpy (str, "Sun alt: ");
	fs_dm_angle (str+9, a);
	f_showit (pu_alt_w, str);
}

/* find the smallest entry in moondb[] that [x,y] is within.
 * if find one return its *MoonDB else NULL.
 * x and y are in image coords (as in m_xim), not mda_w.
 * N.B. we allow for flipping.
 */
static MoonDB *
closeMoonDB (x, y)
int x, y;
#define	RSLOP	5
{
	MoonDB *mp, *smallp;
	double lt, lg;		/* location of [x,y] */
	double forsh;		/* foreshortening (cos of angle from center) */
	int minsz;
	double minr;

	/* allow for flipping */
	x = FX(x);
	y = FY(y);

	/* find forshortening */
	if (xy2ll (x, y, &lt, &lg) < 0)
	    return (NULL);
	solve_sphere (lg - CLNG, PI/2-CLAT, sin(lt), cos(lt), &forsh, NULL);

	watch_cursor(1);

	minsz = 100000;
	minr = 1e6;
	smallp = NULL;
	for (mp = moondb; mp < &moondb[nmoondb]; mp++) {
	    int dx, dy;
	    int sz = mp->sz*moonrad/MDIA; /* radius, in pixels */
	    double r;

	    dx = mp->x - x;
	    dy = mp->y - y;
	    r = sqrt((double)dx*dx + dy*dy);

	    /* it's a candidate if we are inside its (foreshortened) size
	     * or we are within RSLOP pixel of it.
	     */
	    if ((r <= sz*forsh && sz < minsz) || (r < RSLOP && r < minr)) {
		smallp = mp;
		minsz = sz;
		minr = r;
	    }
	}

	watch_cursor(0);

	return (smallp);
}

/* called when the persistent label TB is activated in the popup.
 * userData is the MoonDB entry whose pl we toggle, then redraw.
 */
/* ARGSUSED */
static void
m_pl_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	MoonDB *mp;

	get_something (w, XmNuserData, (XtArgVal)&mp);
	mp->pl = XmToggleButtonGetState (w);

	/* draw just labels if adding or redraw all to erase */
	if (mp->pl) {
	    m_labels();
	    m_refresh (NULL);
	} else
	    m_redraw();
}

/* assign the object pointed to by skypu_op to ObjX/Y/Z or in Data Table,
 * depending on client.
 */
static void
m_assign_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char me[] = "m_assign_cb()";
	Assign_t a = (Assign_t)client;

	switch (a) {
	case A_BASIC:
	    if (is_type (skypu_op, PLANETM)) {
		dm_newobj (((ObjPl *)skypu_op)->pl_code);
		dm_update (mm_get_now(), 1);
		return;
	    } else {
		printf ("%s: op not basic: %s\n", me, skypu_op->o_name);
		exit(1);
	    }
	    break;
	case A_OBJX:
	    obj_set (skypu_op, OBJX);
	    break;
	case A_OBJY:
	    obj_set (skypu_op, OBJY);
	    break;
	case A_OBJZ:
	    obj_set (skypu_op, OBJZ);
	    break;
	default:
	    printf ("%s: bad c = %d\n", me, a);
	    exit(1);
	}
}

/* create the sky background object popup menu */
static void
m_create_skypopup()
{
	Widget pd, w;
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNisAligned, True); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	skypu_w = XmCreatePopupMenu (mda_w, "MSKYPU", args, n);

	n = 0;
	skypu_name_w = XmCreateLabel (skypu_w, "MSKYPN", args, n);
	wtip (skypu_name_w, "Name of object");
	XtManageChild (skypu_name_w);

	n = 0;
	skypu_mag_w = XmCreateLabel (skypu_w, "MSKYPM", args, n);
	wtip (skypu_mag_w, "Nominal magnitude");
	XtManageChild (skypu_mag_w);

	/* a PB to turn on a basic object in the data table */
	n = 0;
	skypu_b_w = XmCreatePushButton (skypu_w, "ABPB", args, n);
	XtAddCallback (skypu_b_w, XmNactivateCallback, m_assign_cb,
							    (XtPointer)A_BASIC);
	set_xmstring (skypu_b_w, XmNlabelString, "Show in Data Table");

	/* a CB to assign to a user-object */
	n = 0;
	pd = XmCreatePulldownMenu (skypu_w, "APD", args, n);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pd); n++;
	    skypu_a_w = XmCreateCascadeButton (skypu_w, "Assign", args, n);

	    n = 0;
	    w = XmCreatePushButton (pd, "APB", args, n);
	    set_xmstring (w, XmNlabelString, "to ObjX");
	    XtAddCallback (w, XmNactivateCallback, m_assign_cb,
							    (XtPointer)A_OBJX);
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (pd, "APB", args, n);
	    set_xmstring (w, XmNlabelString, "to ObjY");
	    XtAddCallback (w, XmNactivateCallback, m_assign_cb,
							    (XtPointer)A_OBJY);
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (pd, "APB", args, n);
	    set_xmstring (w, XmNlabelString, "to ObjZ");
	    XtAddCallback (w, XmNactivateCallback, m_assign_cb,
							    (XtPointer)A_OBJZ);
	    XtManageChild (w);

}

/* fill skypu_w with info from op */
static void
fill_skypopup (op)
Obj *op;
{
	char buf[32];

	/* save in case it's assigned */
	skypu_op = op;

	/* enable one or the other assign option */
	if (is_type(op, PLANETM)) {
	    XtManageChild (skypu_b_w);
	    XtUnmanageChild (skypu_a_w);
	} else {
	    XtManageChild (skypu_a_w);
	    XtUnmanageChild (skypu_b_w);
	}

	/* show name and mag */
	f_showit (skypu_name_w, op->o_name);
	(void) sprintf (buf, "%.2f", get_mag(op));
	f_showit (skypu_mag_w, buf);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: moonmenu.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
