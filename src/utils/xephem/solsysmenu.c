/* code to manage the stuff on the solar system display.
 * functions and data to support the main display begin with ss_.
 * function and data to support the stereo display begin with st_.
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#if defined(__STDC__)
#include <stdlib.h>
#else
extern void *calloc(), *malloc(), *realloc();
#endif
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/DrawingA.h>
#include <Xm/RowColumn.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/Separator.h>
#include <Xm/ToggleB.h>
#include <Xm/Text.h>
#include <Xm/Scale.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "trails.h"
#include "ps.h"

extern Widget	toplevel_w;
#define	XtD	XtDisplay(toplevel_w)
extern Colormap xe_cm;

extern Now *mm_get_now P_((void));
extern Obj *db_basic P_((int id));
extern Obj *db_scan P_((DBScan *sp));
extern int any_ison P_((void));
extern int execute_expr P_((double *vp, char *errbuf));
extern int isUp P_((Widget shell));
extern int obj_cir P_((Now *np, Obj *op));
extern int prog_isgood P_((void));
extern int tickmarks P_((double min, double max, int numdiv, double ticks[]));
extern void db_update P_((Obj *op));
extern void db_scaninit P_((DBScan *sp, int mask, ObjF *op, int nop));
extern void dm_newobj P_((int dbidx));
extern void dm_update P_((Now *np, int how_much));
extern void fs_prdec P_((char *buf, double a));
extern void fs_pangle P_((char *buf, double a));
extern void fs_ra P_((char *buf, double ra));
extern void f_time P_((Widget w, double t));
extern void fs_date P_((char out[], double jd));
extern void fs_sexa P_((char *out, double a, int w, int fracbase));
extern void fs_time P_((char out[], double t));
extern void get_something P_((Widget w, char *resource, XtArgVal value));
extern void get_views_font P_((Display *dsp, XFontStruct **fspp));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void mm_movie P_((double stepsz));
extern void obj_pickgc P_((Obj *op, Widget w, GC *gcp));
extern void obj_set P_((Obj *op, int dbidx));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void timestamp P_((Now *np, Widget w));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *string));
extern void xe_msg P_((char *msg, int app_modal));

/* heliocentric coordinates, and enough info to locate it on screen */
typedef struct {
    Obj o;		/* copy of Obj at the given moment */
    TrTS trts;		/* mjd when Obj was valid and whether to timestamp */
    int sx, sy;		/* main view window coords of object */
    int stx;		/* stereo view x coord (y is the same in both) */
    double x, y, z;	/* heliocentric cartesian coords */
} HLoc;

/* list of coords when "all objects" are being displayed */
typedef struct {
    Obj *op;		/* into the real DB */
    short sx, stx, sy;	/* dot location on screen */
} AllP;

static void ss_create_shell P_((void));
static void st_create_form P_((void));
static void ss_trail_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_activate_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_seo_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_allb_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_obj_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_changed_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_popdown_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_print_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_print P_((void));
static void ss_ps_annotate P_((Now *np));
static void ss_movie_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_helpon_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_da_exp_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_da_input_cb P_((Widget w, XtPointer client, XtPointer call));
static void st_parallax_cb P_((Widget w, XtPointer client, XtPointer call));
static void st_map_cb P_((Widget wid, XtPointer client, XtPointer call));
static void st_track_size P_((void));
static void st_unmap_cb P_((Widget wid, XtPointer client, XtPointer call));
static void st_da_exp_cb P_((Widget w, XtPointer client, XtPointer call));
static void st_da_input_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_popup P_((XEvent *ev));
static void ss_dv_cb P_((Widget w, XtPointer client, XtPointer call));
static void ss_create_popup P_((void));
static void ss_all P_((void));
static void ss_ss_all P_((void));
static void ss_st_all P_((void));
static void ss_allobj P_((Display *dsp, int stview, double scale, double selt,
    double celt, double selg, double celg, unsigned nx, unsigned ny));
static void ss_loc P_((HLoc *hp, double scale, double selt, double celt,
    double selg, double celg, unsigned nx, unsigned ny));
static int ss_newtrail P_((TrTS ts[], TrState *statep, XtPointer client));
static void hloc_reset P_((Now *np));
static HLoc *hloc_grow P_((int dbidx));
static int hloc_add P_((int dbidx, Now *np, int lbl));
static void ap_free P_((void));
static int ap_add P_((Obj *op, HLoc *hp));
static void ap_label_cb P_((Widget w, XtPointer client, XtPointer call));
static void ap_mkobj_cb P_((Widget w, XtPointer client, XtPointer call));
static void mk_gcs P_((void));

static Widget ssshell_w;	/* main solar system shell */
static Widget hr_w, hlng_w, hlat_w; /* scales for heliocentric R, long, lat */
static Widget ssda_w;		/* solar system drawring area */
static Widget ssframe_w;	/* main's DA frame */
static Widget dt_w;		/* date/time stamp label widget */
static Pixmap ss_pm;		/* main view pixmap */
static GC bgc;			/* background GC */
static int asc;			/* max ascent in background GC font */

static Widget stform_w;		/* main stereo form dialog */
static Widget parallax_w;	/* scale to set amount of parallax */
static Widget stda_w;		/* stereo solar system drawring area */
static Widget stframe_w;	/* stereo's DA frame */
static Pixmap st_pm;		/* stereo view pixmap */
static Widget stereo_w;		/* stereo option TB */

static TrState trstate = {
    TRLR_5, TRI_DAY, TRF_DATE, TRR_DAY, TRO_UPR, TRS_MEDIUM, 30
};

enum {DRAGTOO, ALLOBJ, TRAILS, ECLIPTIC, BLEGS, DBLEGS, LABELS, STEREO};

#define	WANTLBL		FUSER2	/* mark object to be labeled */

#define	MINMAG		3.0	/* minimum mag factor, pixels/AU */
#define	MAXMAG		250.0	/* maximum mag factor, pixels/AU */
#define	GAP		4	/* from object to its name, pixels */
#define	NECLPT		10	/* approx number of circles in ecliptic grid */
#define	NECLSEG		31	/* segments in each stereo ecliptic line */
#define	MOVIE_STEPSZ	120.0	/* movie step size, hours */
#define	BLOBW		2	/* size of dot dot drawn for objects, pixels */
#define	PICKRANGE	100	/* sqr of dist allowed from pointer */

/* whether each option is currently on */
static int allobj;
static int dragtoo;
static int trails;
static Widget trails_w;		/* trails TB, so we can turn on automatically*/
static int ecliptic;
static int blegs;
static int dblegs;
static int nametags;
static int stereo;
static int parallax;		/* current value of desired parallax */
static int anytrails;		/* set if any trails -- for postscript label */

/* these are managed by the hloc_* functions.
 * there is always at least one to show the object at the current location.
 */
static HLoc *points[NOBJ];	/* malloc'd lists of points on screen now */
static int npoints[NOBJ];	/* number of points */

static char obj_on[NOBJ];	/* 1 if object is on */
static Widget obj_w[NOBJ];	/* toggle buttons for each object */

#define	ALLPCK	64		/* increase allp this many at once */
static AllP *allp;		/* malloced list of "all" objects */
static int nallp;		/* entries in allp[] in use */
static int mallp;		/* entries in allp[] at most */

/* info about the popup widget and what it is currently working with */
typedef struct {
    Widget pu_w;
    Widget name_w;
    Widget ud_w;
    Widget ut_w;
    Widget ra_w;
    Widget dec_w;
    Widget hlong_w;
    Widget hlat_w;
    Widget eadst_w;
    Widget sndst_w;
    Widget elong_w;
    Widget mag_w;
    Widget lbl_w;
    Widget assign_w;
    Widget dv_w;
    Obj *op;			/* real or into HLoc.o if non-first trail */
    AllP *ap;			/* set if picked an at-large db object */
    HLoc *hp;			/* set if picked any member of trail(even 1st)*/
} Popup;
static Popup pu;

static GC egc;			/* gc for drawing */
static XFontStruct *efsp;	/* text info for drawing */

static char earthname[] = "Earth";		/* local "Moon" alias */
static char sscategory[] = "Solar System";	/* Save category */

/* called when the solar system view is activated via the main menu pulldown.
 * if never called before, create and manage all the widgets as a child of a
 * form. otherwise, just go for it.
 */
void
ss_manage ()
{
	if (!ssshell_w) {
	    ss_create_shell();
	    ss_create_popup();
	    st_create_form();
	    hloc_reset(mm_get_now());
	    timestamp (mm_get_now(), dt_w);
	}
	
	XtPopup (ssshell_w, XtGrabNone);
	set_something (ssshell_w, XmNiconic, (XtArgVal)False);
}

/* called when we are to update our view.
 * don't bother if we are unmanaged.
 */
/* ARGSUSED */
void
ss_update (np, how_much)
Now *np;
int how_much;
{
	if (!isUp(ssshell_w))
	    return;

	watch_cursor (1);

	/* set the first (current) entry for each basic object to now
	 * and erase all other entries.
	 */
	hloc_reset (np);

	/* redraw everything */
	ss_all();
	timestamp (np, dt_w);

	watch_cursor (0);
}

/* called when basic resources change.
 * rebuild and redraw.
 */
void
ss_newres()
{
	if (!ssshell_w)
	    return;
	mk_gcs();
	ss_update (mm_get_now(), 1);
}

/* database (beyond NOBJ) has changed, or been appended to.
 */
void
ss_newdb (appended)
int appended;
{
	/* only effects us if we are showing All db objects and we are up */
	if (allobj && isUp(ssshell_w))
	    ss_all();
}

/* called when one of the user defined objects has changed.
 * discard the points for that object.
 * if the object is now defined in the SS manage the toggle button.
 * N.B. no need to rebuild the scene -- ss_update() will be called for us.
 */
void
ss_newobj (dbidx)
int dbidx;
{
	static char me[] = "ss_newobj()";
	Obj *op;

	/* we might get called before we are ever brought up the first time */
	if (!ssshell_w)
	    return;

	if (dbidx >= NOBJ) {
	    printf ("%s: dbidx=%d but NOBJ=%d\n", me, dbidx, NOBJ);
	    exit (1);
	}

	if (points[dbidx])
	    XtFree ((char *)points[dbidx]);
	points[dbidx] = NULL;
	npoints[dbidx] = 0;

	op = db_basic(dbidx);
	if (is_ssobj(op)) {
	    XtManageChild (obj_w[dbidx]);
	    XmToggleButtonSetState (obj_w[dbidx], True, False);
	    set_xmstring (obj_w[dbidx], XmNlabelString, op->o_name);
	    obj_on[dbidx] = 1;
	} else {
	    XtUnmanageChild (obj_w[dbidx]);
	    XmToggleButtonSetState (obj_w[dbidx], False, False);
	    obj_on[dbidx] = 0;
	}
	    
}

int
ss_ison()
{
	return (isUp(ssshell_w));
}

/* called to put up or remove the watch cursor.  */
void
ss_cursor (c)
Cursor c;
{
	Window win;

	if (ssshell_w && (win = XtWindow(ssshell_w)) != 0) {
	    Display *dsp = XtDisplay(ssshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* create the main solarsystem shell */
static void
ss_create_shell()
{
	typedef struct {
	    char *label;	/* what goes on the help label */
	    char *key;		/* string to call hlp_dialog() */
	} HelpOn;
	static HelpOn helpon[] = {
	    {"Intro...",	"Solsys - intro"},
	    {"on Mouse...",	"Solsys - mouse"},
	    {"on Control...",	"Solsys - control"},
	    {"on View...",	"Solsys - view"},
	    {"on Objects...",	"Solsys - objects"},
	    {"Misc...",		"Solsys - misc"},
	};
	typedef struct {
	    char *name;		/* name of widget, or NULL for sep */
	    char *label;	/* label on toggle button */
	    int id;		/* one of the toggle ids */
	    int *state;		/* int we use to keep state */
	    Widget *wp;		/* widget, or NULL */
	    char *tip;		/* tips text */
	} DrCtrl;
	static DrCtrl drctrls[] = {
	    {"Trails",   "Trails",   TRAILS,   &trails,   &trails_w, 
	    	"Display trails, if currently defined"},
	    {"Ecliptic", "Ecliptic", ECLIPTIC, &ecliptic, NULL, 
	    	"Display sun-centered circles to mark the ecliptic plane"},
	    {"Labels",   "Labels",   LABELS,   &nametags, NULL, 
	    	"Display names near the dots denoting the Objects"},
	    {"BasicLegs","Basic Legs",BLEGS,    &blegs,    NULL,
	    	"Connect planets and ObjXYZ objects to the ecliptic to depict height"},
	    {"DBLegs",   "DB Legs",  DBLEGS,   &dblegs,   NULL,
	    	"Connect other Database objects to the ecliptic to depict height"},
	};
	Widget mb_w, pd_w, cb_w;
	Widget w;
	Widget ssform_w;
	XmString str;
	Arg args[20];
	int i;
	int n;

	/* create the form and shell */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Solar System view"); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNiconName, "SolSys"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	ssshell_w = XtCreatePopupShell ("SolarSystem", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (ssshell_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (ssshell_w, XmNpopdownCallback, ss_popdown_cb, 0);
	sr_reg (ssshell_w, "XEphem*SolarSystem.width", sscategory, 0);
	sr_reg (ssshell_w, "XEphem*SolarSystem.height", sscategory, 0);
	sr_reg (ssshell_w, "XEphem*SolarSystem.x", sscategory, 0);
	sr_reg (ssshell_w, "XEphem*SolarSystem.y", sscategory, 0);

	n = 0;
	ssform_w = XmCreateForm (ssshell_w, "SSF", args, n);
	XtAddCallback (ssform_w, XmNhelpCallback, ss_help_cb, 0);
	XtManageChild (ssform_w);

	/* put a menu bar across the top with the various pulldown menus */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	mb_w = XmCreateMenuBar (ssform_w, "MB", args, n);
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

	    /* add the print push button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "SSPrint", args, n);
	    set_xmstring (w, XmNlabelString, "Print...");
	    XtAddCallback (w, XmNactivateCallback, ss_print_cb, (XtPointer)1);
	    wtip (w, "Print this view");
	    XtManageChild (w);

	    /* make a PB to bring up the trail definer */

	    str = XmStringCreate("Create Trails...", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreatePushButton (pd_w, "TPB", args, n);
	    XtAddCallback (w, XmNactivateCallback, ss_trail_cb, NULL);
	    wtip (w, "Set up to display object locations over time");
	    XtManageChild (w);
	    XmStringFree (str);

	    /* add the "movie" push button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Movie", args, n);
	    set_xmstring (w, XmNlabelString, "Movie Demo");
	    XtAddCallback (w, XmNactivateCallback, ss_movie_cb, 0);
	    wtip (w, "Start/Stop a demonstration animation sequence");
	    XtManageChild (w);

	    /* add the "drag too" toggle button */

	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
	    w = XmCreateToggleButton (pd_w, "LiveDrag", args, n);
	    XtAddCallback (w, XmNvalueChangedCallback, ss_activate_cb,
							(XtPointer)DRAGTOO);
	    wtip(w,"Update scene while dragging scales, not just when release");
	    XtManageChild (w);
	    set_xmstring (w, XmNlabelString, "Live Dragging");
	    dragtoo = XmToggleButtonGetState (w);
	    sr_reg (w, NULL, sscategory, 1);

	    /* add the "stereo" toggle button */

	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
	    stereo_w = XmCreateToggleButton (pd_w, "Stereo", args, n);
	    XtAddCallback (stereo_w, XmNvalueChangedCallback, ss_activate_cb, 
							    (XtPointer)STEREO);
	    wtip (stereo_w, "Open another window showing view for 2nd eye");
	    XtManageChild (stereo_w);
	    stereo = XmToggleButtonGetState (stereo_w);

	    /* add the "close" push button beneath a separator */

	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, ss_close_cb, 0);
	    wtip (w, "Close this and all supporing dialogs");
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

	    /* make all the view options TBs */

	    for (i = 0; i < XtNumber(drctrls); i++) {
		DrCtrl *cp = &drctrls[i];

		if (cp->name) {
		    str = XmStringCreate(cp->label, XmSTRING_DEFAULT_CHARSET);
		    n = 0;
		    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
		    XtSetArg (args[n], XmNmarginHeight, 0); n++;
		    XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
		    XtSetArg (args[n], XmNlabelString, str); n++;
		    w = XmCreateToggleButton(pd_w, cp->name, args, n);
		    *(cp->state) = XmToggleButtonGetState (w);
		    XtAddCallback(w, XmNvalueChangedCallback, ss_activate_cb,
							    (XtPointer)cp->id);
		    if (cp->wp)
			*(cp->wp) = w;
		    if (cp->tip)
			wtip (w, cp->tip);
		    XtManageChild (w);
		    XmStringFree (str);
		    sr_reg (w, NULL, sscategory, 1);
		} else {
		    n = 0;
		    w = XmCreateSeparator (pd_w, "Sep", args, n);
		    XtManageChild (w);
		}
	    }

	/* make the Objects pulldown */

	n = 0;
	pd_w = XmCreatePulldownMenu (mb_w, "ObjPD", args, n);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pd_w);  n++;
	    XtSetArg (args[n], XmNmnemonic, 'O'); n++;
	    cb_w = XmCreateCascadeButton (mb_w, "ObjCB", args, n);
	    set_xmstring (cb_w, XmNlabelString, "Objects");
	    XtManageChild (cb_w);

	    /* make a short-cut which turns on all basic objects */
	    n = 0;
	    w = XmCreatePushButton (pd_w, "AllB", args, n);
	    XtAddCallback (w, XmNactivateCallback, ss_allb_cb, NULL);
	    set_xmstring (w, XmNlabelString, "All Basic");
	    wtip (w, "Shortcut to turn on just Sun, planets and user-defined objects");
	    XtManageChild (w);

	    /* make a short-cut which turns on just Sun+Earth+any objxyz */
	    n = 0;
	    w = XmCreatePushButton (pd_w, "SCPB", args, n);
	    XtAddCallback (w, XmNactivateCallback, ss_seo_cb, NULL);
	    set_xmstring (w, XmNlabelString, "Just S+E+ObjXYZ");
	    wtip (w, "Shortcut to turn on just Sun, Earth and user-defined objects");
	    XtManageChild (w);

	    /* toggle for showing all DB objects too */

	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
	    w = XmCreateToggleButton (pd_w, "AllObjs", args, n);
	    XtAddCallback (w, XmNvalueChangedCallback, ss_activate_cb,
							    (XtPointer)ALLOBJ);
	    set_xmstring (w, XmNlabelString, "All DB too");
	    wtip (w, "Show all other Solar system objects too");
	    XtManageChild (w);
	    allobj = XmToggleButtonGetState(w);
	    sr_reg (w, NULL, sscategory, 1);

	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    /* make the object toggle buttons.
	     * fill in the names we can now.
	     * default all solsys objects on.
	     */
	    for (i = 0; i < NOBJ; i++) {
		Obj *op = db_basic(i);
		char *tbname;

		/* name TB for planet if known (or Earth for Moon) */
		if (i < MOON)
		    tbname = op->o_name;
		else if (i == MOON)
		    tbname = earthname;
		else
		    tbname = "ObjTB";

		n = 0;
		XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
		XtSetArg (args[n], XmNmarginHeight, 0); n++;
		XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
		obj_w[i] = XmCreateToggleButton (pd_w, tbname, args, n);
		wtip (obj_w[i], "Whether to display this object at all");
		XtAddCallback (obj_w[i], XmNvalueChangedCallback, ss_obj_cb,
							    (XtPointer)i);
		if (is_ssobj(op)) {
		    set_xmstring (obj_w[i], XmNlabelString,
			is_planet(op,MOON)? earthname : op->o_name);
		    XtManageChild (obj_w[i]);
		    /* let resources determine whether basic objs are set */
		    if (i > MOON)
			XmToggleButtonSetState (obj_w[i], True, False);
		}
		obj_on[i] = XmToggleButtonGetState (obj_w[i]);
		if (i <= MOON)
		    sr_reg (obj_w[i], NULL, sscategory, 1);
	    }

	/* make the "help" pulldown */

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
		HelpOn *hop = &helpon[i];

		n = 0;
		w = XmCreatePushButton (pd_w, "Help", args, n);
		XtManageChild (w);
		XtAddCallback (w, XmNactivateCallback, ss_helpon_cb, 
							(XtPointer)(hop->key));
		set_xmstring (w, XmNlabelString, hop->label);
	    }

	/* make the time/date stamp label across the bottom */

	n = 0;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	dt_w = XmCreateLabel (ssform_w, "DateStamp", args, n);
	wtip (dt_w, "Date and Time for which map is computed");
	XtManageChild (dt_w);

	/* make the bottom scale */

	n = 0;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, dt_w); n++;
	XtSetArg (args[n], XmNmaximum, 359); n++;
	XtSetArg (args[n], XmNminimum, 0); n++;
	XtSetArg (args[n], XmNscaleMultiple, 1); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNprocessingDirection, XmMAX_ON_RIGHT); n++;
	XtSetArg (args[n], XmNshowValue, True); n++;
	hlng_w = XmCreateScale (ssform_w, "HLongScale", args, n);
	XtAddCallback (hlng_w, XmNdragCallback, ss_changed_cb, 0);
	XtAddCallback (hlng_w, XmNvalueChangedCallback, ss_changed_cb, 0);
	wtip (hlng_w, "Set heliocentric longitude of vantage point");
	XtManageChild (hlng_w);
	sr_reg (hlng_w, NULL, sscategory, 0);

	/* make the left scale */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, mb_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, hlng_w); n++;
	XtSetArg (args[n], XmNdecimalPoints, 1); n++;
	XtSetArg (args[n], XmNmaximum, 100); n++;
	XtSetArg (args[n], XmNminimum, 0); n++;
	XtSetArg (args[n], XmNscaleMultiple, 1); n++;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNprocessingDirection, XmMAX_ON_TOP); n++;
	hr_w = XmCreateScale (ssform_w, "DistScale", args, n);
	XtAddCallback (hr_w, XmNdragCallback, ss_changed_cb, 0);
	XtAddCallback (hr_w, XmNvalueChangedCallback, ss_changed_cb, 0);
	wtip (hr_w, "Zoom in and out");
	XtManageChild (hr_w);
	sr_reg (hr_w, NULL, sscategory, 0);

	/* make the right scale */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, mb_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, hlng_w); n++;
	XtSetArg (args[n], XmNmaximum, 90); n++;
	XtSetArg (args[n], XmNminimum, -90); n++;
	XtSetArg (args[n], XmNscaleMultiple, 1); n++;
	XtSetArg (args[n], XmNprocessingDirection, XmMAX_ON_TOP); n++;
	XtSetArg (args[n], XmNprocessingDirection, XmMAX_ON_TOP); n++;
	XtSetArg (args[n], XmNshowValue, True); n++;
	hlat_w = XmCreateScale (ssform_w, "HLatScale", args, n);
	XtAddCallback (hlat_w, XmNdragCallback, ss_changed_cb, 0);
	XtAddCallback (hlat_w, XmNvalueChangedCallback, ss_changed_cb, 0);
	wtip (hlat_w, "Set heliocentric latitude of vantage point");
	XtManageChild (hlat_w);
	sr_reg (hlat_w, NULL, sscategory, 0);

	/* make a frame for the drawing area */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, mb_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, hlng_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, hr_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, hlat_w); n++;
	XtSetArg (args[n], XmNshadowType, XmSHADOW_ETCHED_OUT); n++;
	ssframe_w = XmCreateFrame (ssform_w, "SolarFrame", args, n);
	XtManageChild (ssframe_w);

	/* make a drawing area for drawing the solar system */

	n = 0;
	ssda_w = XmCreateDrawingArea (ssframe_w, "SolarDA", args, n);
	XtAddCallback (ssda_w, XmNexposeCallback, ss_da_exp_cb, 0);
	XtAddCallback (ssda_w, XmNinputCallback, ss_da_input_cb, 0);
	XtManageChild (ssda_w);
}

/* create the stereo solarsystem form */
static void
st_create_form()
{
	Arg args[20];
	int n;

	/* create form */
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNdefaultPosition, False); n++;
	XtSetArg (args[n], XmNnoResize, True); n++;	/* user can't resize */
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	stform_w = XmCreateFormDialog (ssshell_w, "StereoSolarSystem", args,n);
	set_something (stform_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (stform_w, XmNmapCallback, st_map_cb, NULL);
	XtAddCallback (stform_w, XmNunmapCallback, st_unmap_cb, 0);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Stereo Solar System"); n++;
	XtSetValues (XtParent(stform_w), args, n);

	/* make the parallax scale at the bottom.
	 * the value is the offset of the sun (at a=0), in pixels.
	 */

	n = 0;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNmaximum, 20); n++;
	XtSetArg (args[n], XmNminimum, -20); n++;
	XtSetArg (args[n], XmNscaleMultiple, 2); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNprocessingDirection, XmMAX_ON_RIGHT); n++;
	parallax_w = XmCreateScale (stform_w, "Parallax", args, n);
	XtAddCallback (parallax_w, XmNdragCallback, st_parallax_cb, 0);
	XtAddCallback (parallax_w, XmNvalueChangedCallback, st_parallax_cb, 0);
	XtManageChild (parallax_w);
	XmScaleGetValue (parallax_w, &parallax);

	/* make a frame for the drawing area */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, parallax_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNshadowType, XmSHADOW_ETCHED_OUT); n++;
	stframe_w = XmCreateFrame (stform_w, "StereoFrame", args, n);
	XtManageChild (stframe_w);

	/* make a drawing area for drawing the stereo solar system */

	n = 0;
	stda_w = XmCreateDrawingArea (stframe_w, "StereoDA", args, n);
	XtAddCallback (stda_w, XmNexposeCallback, st_da_exp_cb, 0);
	XtAddCallback (stda_w, XmNinputCallback, st_da_input_cb, 0);
	XtManageChild (stda_w);
}

/* callback to bring up the "Create trails.." PB.
 */
/* ARGSUSED */
static void
ss_trail_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	tr_setup ("xephem Solar System trails setup", "Solar System", &trstate,
							    ss_newtrail, NULL);
}

/* callback from the control toggle buttons
 */
/* ARGSUSED */
static void
ss_activate_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int what = (int) client;

	switch (what) {
	case DRAGTOO:
	    dragtoo = XmToggleButtonGetState(w);
	    break;
	case ALLOBJ:
	    allobj = XmToggleButtonGetState(w);
	    ss_all ();
	    break;
	case TRAILS:
	    trails = XmToggleButtonGetState(w);
	    ss_all ();
	    break;
	case ECLIPTIC:
	    ecliptic = XmToggleButtonGetState(w);
	    ss_all ();
	    break;
	case DBLEGS:
	    dblegs = XmToggleButtonGetState(w);
	    ss_all ();
	    break;
	case BLEGS:
	    blegs = XmToggleButtonGetState(w);
	    ss_all ();
	    break;
	case LABELS:
	    nametags = XmToggleButtonGetState(w);
	    ss_all ();
	    break;
	case STEREO:
	    stereo = XmToggleButtonGetState(w);
	    if (stereo)
		XtManageChild(stform_w); /* expose will update it */
	    else
		XtUnmanageChild(stform_w);
	    break;
	default:
	    printf ("solsysmenu.c: unknown toggle button\n");
	    exit(1);
	}
}

/* callback from the sun+earth+objxyz shortcut */
/* ARGSUSED */
static void
ss_seo_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int i;

	/* turn everything off */
	for (i = 0; i < NOBJ; i++) {
	    obj_on[i] = 0;
	    XmToggleButtonSetState (obj_w[i], False, False);
	}

	/* turn on just sun, earth and defined solar system user objects */
	obj_on[SUN] = 1;
	XmToggleButtonSetState (obj_w[SUN], True, False);
	obj_on[MOON] = 1;
	XmToggleButtonSetState (obj_w[MOON], True, False);
	for (i = OBJX; i <= OBJZ; i++) {
	    Obj *op = db_basic (i);
	    if (is_ssobj(op)) {
		obj_on[i] = 1;
		XmToggleButtonSetState (obj_w[i], True, False);
	    }
	}

	/* redraw */
	ss_all();
}

/* callback from the just-basic shortcut */
/* ARGSUSED */
static void
ss_allb_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int i;

	for (i = 0; i < NOBJ; i++) {
	    Obj *op = db_basic (i);
	    if (is_ssobj(op)) {
		obj_on[i] = 1;
		XmToggleButtonSetState (obj_w[i], True, False);
	    } else {
		obj_on[i] = 0;
		XmToggleButtonSetState (obj_w[i], False, False);
	    }
	}

	/* redraw */
	ss_all();
}

/* callback from the object on/off toggle buttons.
 * client is the dbidx.
 */
/* ARGSUSED */
static void
ss_obj_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int who = (int) client;

	obj_on[who] = XmToggleButtonGetState(w);
	ss_all();
}

/* callback when any of the scales change value.
 * we disable dragging when "All objects" is on.
 */
/* ARGSUSED */
static void
ss_changed_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmScaleCallbackStruct *sp = (XmScaleCallbackStruct *) call;

	if (w != hr_w && w != hlng_w && w != hlat_w) {
	    printf ("solsysmenu.c: Unknown scaled callback\n");
	    exit(1);
	}

	if (!allobj || dragtoo || sp->reason == XmCR_VALUE_CHANGED)
	    ss_all();
}

/* callback from the Close button.
 */
/* ARGSUSED */
static void
ss_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* let popdown do rest of the work */
	XtPopdown (ssshell_w);
}

/* callback from popping down the main view */
/* ARGSUSED */
static void
ss_popdown_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (stform_w);

	if (ss_pm) {
	    XFreePixmap (XtD, ss_pm);
	    ss_pm = (Pixmap) 0;
	}

	/* stop movie that might be running */
	mm_movie (0.0);

	ap_free();
}

/* callback from the Print button.
 */
/* ARGSUSED */
static void
ss_print_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        XPSAsk ("Solar System", ss_print);
}

/* proceed to generate a postscript file.
 * call XPSClose() when finished.
 */
static void
ss_print ()
{
	Now *np = mm_get_now();
        unsigned int nx, ny;
	unsigned int bw, d;
	Window root;
	int x, y;

	if (!ss_ison()) {
	    xe_msg ("Solar System View must be open to print.", 1);
	    XPSClose();
	    return;
	}

	watch_cursor(1);

	/* get info about ss_pm size */
        XGetGeometry(XtD, ss_pm, &root, &x, &y, &nx, &ny, &bw, &d);

	/* draw in an area 6.5w x 7h centered 1in down from top */
	if (14*nx >= 13*ny)
	    XPSXBegin (ss_pm, 0, 0, nx, ny, 1*72, 10*72, (int)(6.5*72));
	else {
	    int pw = 72*7*nx/ny;	/* width on paper when 7 hi */
	    XPSXBegin (ss_pm, 0, 0, nx, ny, (int)((8.5*72-pw)/2), 10*72, pw);
	}

	/* redraw the main view -- *not* the stereo view */
	ss_ss_all ();

	/* no more X captures */
	XPSXEnd();

	/* add some extra info */
	ss_ps_annotate (np);

	/* finished */
	XPSClose();

	watch_cursor(0);
}

static void
ss_ps_annotate (np)
Now *np;
{
	char dir[128];
	char buf[128];
	char *bp;
	double elt, elng;
	int sv;
	int y;

	XmScaleGetValue (hlat_w, &sv);
	elt = degrad(sv);
	XmScaleGetValue (hlng_w, &sv);
	elng = degrad(sv);

	/* title, of sorts */

	y = AROWY(9);
	(void) sprintf (dir, "(XEphem Solar System View) 306 %d cstr\n", y);
	XPSDirect (dir);

	/* label trail interval, if any */
	if (trails && anytrails) {
	    switch (trstate.i) {
	    case TRI_5MIN: (void) strcpy (buf, "5 Minutes"); break;
	    case TRI_HOUR: (void) strcpy (buf, "1 Hour"); break;
	    case TRI_DAY:  (void) strcpy (buf, "1 Day"); break;
	    case TRI_WEEK: (void) strcpy (buf, "1 Week"); break;
	    case TRI_MONTH:(void) strcpy (buf, "1 Month"); break;
	    case TRI_YEAR: (void) strcpy (buf, "1 Year"); break;
	    case TRI_CUSTOM: (void) sprintf (buf, "%g Days", trstate.customi);
	    								break;
	    default: printf ("Solsys trstate.i = %d\n", trstate.i); exit(1);
	    }

	    y = AROWY(8);
	    (void) sprintf (dir, "(Trail Interval is %s) 306 %d cstr\n", buf,y);
	    XPSDirect (dir);
	}

	/* left column */
	y = AROWY(7);
	fs_sexa (buf, raddeg(elt), 3, 60);
	for (bp = buf; *bp == ' '; bp++) continue;
	(void) sprintf (dir,
		"(Heliocentric Latitude:) 204 %d rstr (%s) 214 %d lstr\n",
								    y, bp, y);
	XPSDirect (dir);

	y = AROWY(6);
	fs_sexa (buf, raddeg(elng), 4, 60);
	for (bp = buf; *bp == ' '; bp++) continue;
	(void) sprintf (dir,
		"(Heliocentric Longtude:) 204 %d rstr (%s) 214 %d lstr\n",
								    y, bp, y);
	XPSDirect (dir);

	/* right column */
	y = AROWY(7);
	fs_time (buf, mjd_hr(mjd));
	for (bp = buf; *bp == ' '; bp++) continue;
	(void) sprintf (dir, "(UTC Time:) 450 %d rstr (%s) 460 %d lstr\n",
								    y, bp, y);
	XPSDirect (dir);

	y = AROWY(6);
	fs_date (buf, mjd_day(mjd));
	for (bp = buf; *bp == ' '; bp++) continue;
	(void) sprintf (dir, "(UTC Date:) 450 %d rstr (%s) 460 %d lstr\n",
								    y, bp, y);
	XPSDirect (dir);
}

/* callback from the Movie button.
 */
/* ARGSUSED */
static void
ss_movie_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	mm_movie (MOVIE_STEPSZ);
}

/* callback from the Help button.
 */
/* ARGSUSED */
static void
ss_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg[] = {
"This displays the solar system. The sun is always at the center. The left",
"slider controls your distance from the sun - further up is closer. The",
"bottom slider controls your heliocentric longitude. The right slider controls",
"your heliocentric latitude - your angle above the ecliptic."
};

	hlp_dialog ("Solsys", msg, sizeof(msg)/sizeof(msg[0]));
}

/* callback from a specific Help button.
 * client is a string to use with hlp_dialog().
 */
/* ARGSUSED */
static void
ss_helpon_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	hlp_dialog ((char *)client, NULL, 0);
}

/* expose of solar system drawing area.
 */
/* ARGSUSED */
static void
ss_da_exp_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static unsigned ss_last_nx, ss_last_ny;
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	Display *dsp = XtDisplay (w);
	Window win = XtWindow (w);
	unsigned int ss_nx, ss_ny;
	unsigned int bw, d;
	Window root;
	int x, y;

	switch (c->reason) {
	case XmCR_EXPOSE: {
	    /* turn off gravity so we get expose events for either shrink or
	     * expand.
	     */
	    static int before;
	    XExposeEvent *e = &c->event->xexpose;

	    if (!before) {
		XSetWindowAttributes swa;
		swa.bit_gravity = ForgetGravity;
		XChangeWindowAttributes (e->display, e->window,
							    CWBitGravity, &swa);
		before = 1;
	    }
	    /* wait for the last in the series */
	    if (e->count != 0)
		return;
	    break;
	    }
	default:
	    printf ("Unexpected ssda_w event. type=%d\n", c->reason);
	    exit(1);
	}

	/* make sure gc's are ready */
	if (!bgc)
	    mk_gcs();

	/* make the pixmap if new or changed size */
	XGetGeometry(dsp, win, &root, &x, &y, &ss_nx, &ss_ny, &bw, &d);
	if (!ss_pm || ss_nx != ss_last_nx || ss_ny != ss_last_ny) {
	    if (ss_pm)
		XFreePixmap (dsp, ss_pm);
	    ss_pm = XCreatePixmap (dsp, win, ss_nx, ss_ny, d);
	    ss_last_nx = ss_nx;
	    ss_last_ny = ss_ny;
	    if (stereo)
		st_track_size();
	}

	ss_ss_all();
}

/* a dot has been picked on the main view: find what it is and report it. */
/* ARGSUSED */
static void
ss_da_input_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	XEvent *ev;
	int x, y;		/* mouse coords */
	HLoc *minhp;		/* closest trailed entry */
	Obj *hop;		/* trailed real or copy in minhp */
	AllP *closeap;		/* closest "at large" db entry */
	int mind;		/* minimum distance from cursor so far */
	int dbidx, i;		/* indices */

	if (c->reason != XmCR_INPUT)
	    return;
	ev = c->event;
	if (ev->xany.type != ButtonPress || ev->xbutton.button != Button3)
	    return;

	x = ((XButtonPressedEvent *)ev)->x;
	y = ((XButtonPressedEvent *)ev)->y;

	/* first check the basic objects and their trails */
	minhp = (HLoc *)0;
	hop = (Obj *)0;
	closeap = (AllP *)0;
	mind = PICKRANGE+1;
	for (dbidx = 0; dbidx < NOBJ; dbidx++) {
	    if (!obj_on[dbidx])
		continue;
	    for (i = 0; i < npoints[dbidx]; i++) {
		HLoc *hp = &points[dbidx][i];
		int d = (x - hp->sx)*(x - hp->sx) + (y - hp->sy)*(y - hp->sy);

		if (d < mind) {
		    mind = d;
		    minhp = hp;
		    hop = (i == 0) ? db_basic(dbidx) : &hp->o;
		}
	    }
	}

	/* then check the other objects for anything closer still */
	for (i = 0; i < nallp; i++) {
	    AllP *ap = &allp[i];
	    int d = (x - ap->sx)*(x - ap->sx) + (y - ap->sy)*(y - ap->sy);
	    if (d < mind) {
		mind = d;
		closeap = ap;
	    }
	}

	/* do nothing if none within picking range */
	if (mind > PICKRANGE)
	    return;

	if (closeap) {
	    /* closest was an at-large db object */
	    pu.ap = closeap;
	    pu.op = closeap->op;
	    pu.hp = (HLoc *)0;
	} else {
	    /* closest was a trailed basic item */
	    pu.ap = (AllP *)0;
	    pu.hp = minhp;
	    pu.op = hop;
	}

	ss_popup (ev);
}

/* ARGSUSED */
static void
st_parallax_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmScaleGetValue (w, &parallax);
	ss_all ();
}

/* called whenever the stereo scene is mapped. */
/* ARGSUSED */
static void
st_map_cb (wid, client, call)
Widget wid;
XtPointer client;
XtPointer call;
{
	st_track_size();
}

/* set the size of the stereo DrawingArea the same as the main window's.
 * we also try to position it just to the left, but it doesn't always work.
 */
static void
st_track_size()
{
	Dimension w, h;
	Position mfx, mfy, mdy;
	Position sdy;
	Arg args[20];
	int n;

	/* set sizes equal */
	n = 0;
	XtSetArg (args[n], XmNwidth, &w); n++;
	XtSetArg (args[n], XmNheight, &h); n++;
	XtGetValues (ssda_w, args, n);

	n = 0;
	XtSetArg (args[n], XmNwidth, w); n++;
	XtSetArg (args[n], XmNheight, h); n++;
	XtSetValues (stda_w, args, n);

	/* set locations -- allow for different stuff on top of drawingareas */
	n = 0;
	XtSetArg (args[n], XmNx, &mfx); n++;
	XtSetArg (args[n], XmNy, &mfy); n++;
	XtGetValues (ssshell_w, args, n);
	n = 0;
	XtSetArg (args[n], XmNy, &mdy); n++;
	XtGetValues (ssframe_w, args, n);
	n = 0;
	XtSetArg (args[n], XmNy, &sdy); n++;
	XtGetValues (stframe_w, args, n);

	n = 0;
	XtSetArg (args[n], XmNx, mfx-w-30); n++;
	XtSetArg (args[n], XmNy, mfy + (mdy-sdy)); n++;
	XtSetValues (stform_w, args, n);
}

/* callback from unmapping the stereo view */
/* ARGSUSED */
static void
st_unmap_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	stereo = 0;
	XmToggleButtonSetState(stereo_w, False, False);

	if (st_pm) {
	    XFreePixmap (XtD, st_pm);
	    st_pm = (Pixmap) 0;
	}
}

/* expose of stereo solar system drawing area.
 */
/* ARGSUSED */
static void
st_da_exp_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static unsigned st_last_nx, st_last_ny;
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	Display *dsp = XtDisplay (w);
	Window win = XtWindow (w);
	unsigned int st_nx, st_ny;
	unsigned int bw, d;
	Window root;
	int x, y;

	switch (c->reason) {
	case XmCR_EXPOSE: {
	    /* turn off gravity so we get expose events for either shrink or
	     * expand.
	     */
	    static int before;
	    XExposeEvent *e = &c->event->xexpose;

	    if (!before) {
		XSetWindowAttributes swa;
		swa.bit_gravity = ForgetGravity;
		XChangeWindowAttributes (e->display, e->window,
							    CWBitGravity, &swa);
		before = 1;
	    }
	    /* wait for the last in the series */
	    if (e->count != 0)
		return;
	    break;
	    }
	default:
	    printf ("Unexpected stda_w event. type=%d\n", c->reason);
	    exit(1);
	}

	/* make sure gc's are ready */
	if (!bgc)
	    mk_gcs();

	/* make the pixmap if new or changed size */
	XGetGeometry(dsp, win, &root, &x, &y, &st_nx, &st_ny, &bw, &d);
	if (!st_pm || st_nx != st_last_nx || st_ny != st_last_ny) {
	    if (st_pm)
		XFreePixmap (dsp, st_pm);
	    st_pm = XCreatePixmap (dsp, win, st_nx, st_ny, d);
	    st_last_nx = st_nx;
	    st_last_ny = st_ny;
	    st_track_size();
	}

	ss_st_all();
}

/* a dot has been picked on the stereo map: find what it is and report it. */
/* ARGSUSED */
static void
st_da_input_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	XEvent *ev;
	int x, y;		/* mouse coords */
	HLoc *minhp;		/* closest trailed entry */
	Obj *hop;		/* trailed real or copy in minhp */
	AllP *closeap;		/* closest "at large" db entry */
	int mind;		/* minimum distance from cursor so far */
	int dbidx, i;		/* indices */

	if (c->reason != XmCR_INPUT)
	    return;
	ev = c->event;
	if (ev->xany.type != ButtonPress || ev->xbutton.button != Button3)
	    return;

	x = ((XButtonPressedEvent *)ev)->x;
	y = ((XButtonPressedEvent *)ev)->y;

	/* first check the basic objects and their trails */
	minhp = (HLoc *)0;
	hop = (Obj *)0;
	closeap = (AllP *)0;
	mind = PICKRANGE+1;
	for (dbidx = 0; dbidx < NOBJ; dbidx++) {
	    if (!obj_on[dbidx])
		continue;
	    for (i = 0; i < npoints[dbidx]; i++) {
		HLoc *hp = &points[dbidx][i];
		int d = (x - hp->stx)*(x - hp->stx) + (y - hp->sy)*(y - hp->sy);

		if (d < mind) {
		    mind = d;
		    minhp = hp;
		    hop = (i == 0) ? db_basic(dbidx) : &hp->o;
		}
	    }
	}

	/* then check the other objects for anything closer still */
	for (i = 0; i < nallp; i++) {
	    AllP *ap = &allp[i];
	    int d = (x - ap->stx)*(x - ap->stx) + (y - ap->sy)*(y - ap->sy);
	    if (d < mind) {
		mind = d;
		closeap = ap;
	    }
	}

	/* do nothing if none within picking range */
	if (mind > PICKRANGE)
	    return;

	if (closeap) {
	    /* closest was an at-large db object */
	    pu.ap = closeap;
	    pu.op = closeap->op;
	    pu.hp = (HLoc *)0;
	} else {
	    /* closest was a trailed basic item */
	    pu.ap = (AllP *)0;
	    pu.hp = minhp;
	    pu.op = hop;
	}

	ss_popup (ev);
}

/* fill in the popup with goodies from pu.
 * display fields the same way they are in main data menu.
 * position the popup as indicated by ev and display it.
 * it goes down by itself.
 */
static void
ss_popup (ev)
XEvent *ev;
{
	Now *np = mm_get_now();
	Obj *op = pu.op;
	double Mjd = pu.hp ? pu.hp->trts.t : mjd;
	char buf[128];
	double d;

	/* set TB based on op */
	XmToggleButtonSetState (pu.lbl_w, (op->o_flags & WANTLBL), False);

	if (is_planet(op, MOON)) {
	    /* MOON is used to denote Earth.
	     * must use trail entry even if first. others don't need this
	     * because they don't fool with the type.
	     */
	    op = &pu.hp->o;
	    set_xmstring (pu.name_w, XmNlabelString, earthname);
	    XtManageChild (pu.ud_w);
	    XtManageChild (pu.ut_w);
	    XtManageChild (pu.hlong_w);
	    XtUnmanageChild (pu.hlat_w);
	    XtUnmanageChild (pu.eadst_w);
	    XtManageChild (pu.sndst_w);
	    XtUnmanageChild (pu.elong_w);
	} else {
	    set_xmstring (pu.name_w, XmNlabelString, op->o_name);
	    if (is_planet (op, SUN)) {
		XtUnmanageChild (pu.ud_w);
		XtUnmanageChild (pu.ut_w);
		XtUnmanageChild (pu.hlong_w);
		XtUnmanageChild (pu.hlat_w);
		XtUnmanageChild (pu.eadst_w);
		XtUnmanageChild (pu.sndst_w);
		XtUnmanageChild (pu.elong_w);
	    } else {
		XtManageChild (pu.ud_w);
		XtManageChild (pu.ut_w);
		XtManageChild (pu.hlong_w);
		XtManageChild (pu.hlat_w);
		XtManageChild (pu.eadst_w);
		XtManageChild (pu.sndst_w);
		XtManageChild (pu.elong_w);
	    }
	}

	(void)strcpy (buf, "UT Date: ");
	fs_date (buf+strlen(buf), mjd_day(Mjd));
	set_xmstring (pu.ud_w, XmNlabelString, buf);

	(void)strcpy (buf, "UT Time: ");
	fs_time (buf+strlen(buf), mjd_hr(Mjd));
	set_xmstring (pu.ut_w, XmNlabelString, buf);

	(void)strcpy (buf, "RA: ");
	fs_ra (buf+strlen(buf), op->s_ra);
	set_xmstring (pu.ra_w, XmNlabelString, buf);

	(void)strcpy (buf, "Dec: ");
	fs_prdec (buf+strlen(buf), op->s_dec);
	set_xmstring (pu.dec_w, XmNlabelString, buf);

	(void) sprintf (buf, "Mag: %.3g", get_mag(op));
	set_xmstring (pu.mag_w, XmNlabelString, buf);

	(void)strcpy (buf, "Hel Long: ");
	fs_pangle (buf+strlen(buf), op->s_hlong);
	set_xmstring (pu.hlong_w, XmNlabelString, buf);

	(void)strcpy (buf, "Hel Lat: ");
	fs_pangle (buf+strlen(buf), op->s_hlat);
	set_xmstring (pu.hlat_w, XmNlabelString, buf);

	d = op->s_edist;
	(void)strcpy (buf, "Earth Dist: ");
	(void)sprintf (buf+strlen(buf), d >= 9.99995 ? "%6.3f" : "%6.4f", d);
	set_xmstring (pu.eadst_w, XmNlabelString, buf);

	(void)strcpy (buf, "Sun Dist: ");
	d = is_planet(op, MOON) ? op->s_edist : op->s_sdist;
	(void)sprintf (buf+strlen(buf), d >= 9.99995 ? "%6.3f" : "%6.4f", d);
	set_xmstring (pu.sndst_w, XmNlabelString, buf);

	(void)strcpy (buf, "Elongation: ");
	(void)sprintf (buf+strlen(buf), "%6.1f", op->s_elong);
	set_xmstring (pu.elong_w, XmNlabelString, buf);

	if (pu.ap) {
	    /* op is an at-large object from the db */
	    XtManageChild (pu.assign_w);
	    XtUnmanageChild (pu.dv_w);
	} else {
	    /* op is one of the basic objects */
	    XtManageChild (pu.dv_w);
	    XtUnmanageChild (pu.assign_w);
	}

	XmMenuPosition (pu.pu_w, (XButtonPressedEvent *)ev);
	XtManageChild (pu.pu_w);
}

/* create the id popup */
static void
ss_create_popup()
{
	static struct {
	    Widget *wp;
	    char *tip;
	} puw[] = {
	    {&pu.name_w,  "Object name"},
	    {&pu.ud_w,    "UT Date of this information"},
	    {&pu.ut_w,    "UT Time of this information"},
	    {&pu.ra_w,	  "RA of object"},
	    {&pu.dec_w,   "Declination of object"},
	    {&pu.hlong_w, "Heliocentric longitude"},
	    {&pu.hlat_w,  "Heliocentric latitide"},
	    {&pu.eadst_w, "AU from Earth to object"},
	    {&pu.sndst_w, "AU from Sun to object"},
	    {&pu.elong_w, "Angle between Sun and object, +East"},
	    {&pu.mag_w,   "Nominal magnitude"},
	};
	Arg args[20];
	Widget w;
	Widget pd_w;
	int n;
	int i;

	/* create the popup */
	n = 0;
	XtSetArg (args[n], XmNisAligned, True); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	pu.pu_w = XmCreatePopupMenu (ssda_w, "SSPopup", args, n);

	/* create the main label widgets */

	for (i = 0; i < XtNumber(puw); i++) {
	    n = 0;
	    w = XmCreateLabel (pu.pu_w, "SSPopLbl", args, n);
	    XtManageChild (w);
	    *(puw[i].wp) = w;
	    wtip (w, puw[i].tip);
	}

	/* separator */

	n = 0;
	w = XmCreateSeparator (pu.pu_w, "Sep", args, n);
	XtManageChild (w);

	/* the label TB */

	n = 0;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
	pu.lbl_w = XmCreateToggleButton (pu.pu_w, "LblTB", args, n);
	XtAddCallback (pu.lbl_w, XmNvalueChangedCallback, ap_label_cb, NULL);
	set_xmstring (pu.lbl_w, XmNlabelString, "Persistent Label");
	wtip (pu.lbl_w, "Whether to always label this object on the map");
	XtManageChild (pu.lbl_w);

	/* the Show in Data Table PB */

	n = 0;
	pu.dv_w = XmCreatePushButton (pu.pu_w, "DVTB", args, n);
	XtAddCallback (pu.dv_w, XmNactivateCallback, ss_dv_cb, NULL);
	set_xmstring (pu.dv_w, XmNlabelString, "Show in Data Table");
	wtip (pu.dv_w, "Enable the row for this object in the Data Table");
	XtManageChild (pu.dv_w);

	/* the Make ObjXYZ cascade -- it gets managed later as needed */

	n = 0;
	pd_w = XmCreatePulldownMenu (pu.pu_w, "ObjXYZPDM", args, n);

	n = 0;
	XtSetArg (args[n], XmNsubMenuId, pd_w); n++;
	pu.assign_w = XmCreateCascadeButton (pu.pu_w, "ObjXYZCB", args, n);
        wtip (pu.assign_w, "Assign this object to a row in the Data Table");
	set_xmstring (pu.assign_w, XmNlabelString, "Assign");

	    n = 0;
	    w = XmCreatePushButton (pd_w, "ObjX", args, n);
	    XtAddCallback (w, XmNactivateCallback, ap_mkobj_cb,(XtPointer)OBJX);
	    set_xmstring (w, XmNlabelString, "To ObjX");
	    wtip (w, "Assign this to user Object X and display in Data Table");
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (pd_w, "ObjY", args, n);
	    XtAddCallback (w, XmNactivateCallback, ap_mkobj_cb,(XtPointer)OBJY);
	    set_xmstring (w, XmNlabelString, "To ObjY");
	    wtip (w, "Assign this to user Object Y and display in Data Table");
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (pd_w, "ObjZ", args, n);
	    XtAddCallback (w, XmNactivateCallback, ap_mkobj_cb,(XtPointer)OBJZ);
	    set_xmstring (w, XmNlabelString, "To ObjZ");
	    wtip (w, "Assign this to user Object Z and display in Data Table");
	    XtManageChild (w);
}

/* redraw the main view and, if enables, the stereo view.
 * ok to use this everywhere _except_ the individual window expose callbacks.
 */
static void
ss_all()
{
	watch_cursor (1);

	ss_ss_all();

	if (stereo)
	    ss_st_all();

	watch_cursor (0);
}

/* redraw the main sol system view.
 */
static void
ss_ss_all()
{
	Display *dsp = XtDisplay(ssda_w);
	Window ss_win = XtWindow(ssda_w);
	unsigned int ss_nx, ss_ny;
	unsigned int bw, d;
	Window root;
	int x, y;
	int dbidx;
	int sv;			/* ScaleValue tmp */
	double scale;		/* pixels per au */
	double elt, selt, celt;	/* heliocentric lat of eye, rads */
	double elg, selg, celg;	/* heliocentric lng of eye, rads */

	watch_cursor (1);

	/* flag set for postscript if any trails drawn */
	anytrails = 0;

	/* get the sizes of the windows we'll use */
	XGetGeometry(dsp, ss_win, &root, &x, &y, &ss_nx, &ss_ny, &bw, &d);

	/* clear */
	XFillRectangle (dsp, ss_pm, bgc, 0, 0, ss_nx, ss_ny);

	/* establish the scales */
	XmScaleGetValue (hr_w, &sv);
	scale = MINMAG * pow (MAXMAG/MINMAG, sv/100.);
	XmScaleGetValue (hlat_w, &sv);
	elt = degrad(sv);
	selt = sin(elt);
	celt = cos(elt);
	XmScaleGetValue (hlng_w, &sv);
	elg = degrad(sv+90.0);
	selg = sin(elg);
	celg = cos(elg);

	/* draw the ecliptic plane, if desired */
	if (ecliptic) {
	    double minau, maxau;
	    double aus[NECLPT+2];
	    char spacing[64];
	    int nau, i;

	    XSetFont (XtD, egc, efsp->fid);

	    minau = 0;
	    maxau = ss_nx > ss_ny ? ss_nx/2.0/scale : ss_ny/2.0/scale;
	    nau = tickmarks (minau, maxau, NECLPT, aus);

	    /* draw tick mark spacing message in upper left corner */
	    (void) sprintf (spacing, "%g AU", aus[1] - aus[0]);
	    XPSDrawString(dsp, ss_pm, egc, 1, asc+1, spacing, strlen(spacing));

	    /* draw each grid line.
	     * the main view uses simple elipses.
	     */
	    for (i = 0; i < nau; i++) {
		int arcx, arcy, arcw, arch;
		double au = aus[i];
		HLoc hhl, whl;		/* width and height */

		if (au <= 0.0)
		    continue;

		whl.x = au*celg;	/* a point to the right */
		whl.y = au*selg;
		whl.z = 0.0;
		ss_loc (&whl, scale, selt, celt, selg, celg, ss_nx, ss_ny);
		hhl.x = -au*selg;	/* a point up */
		hhl.y = au*celg;
		hhl.z = 0.0;
		ss_loc (&hhl, scale, selt, celt, selg, celg, ss_nx, ss_ny);
		if (selt < 0.0)
		    hhl.sy = ss_ny - hhl.sy;

		arcx = ss_nx - whl.sx;
		arcy = hhl.sy;
		arcw = 2*whl.sx - ss_nx - 1;
		arch = ss_ny - 2*hhl.sy - 1;
		if (arch <= 0)
		    arch = 1;	/* avoid pushing our luck with XDrawArc */
		if (arcw <= 0)
		    arcw = 1;	/* avoid pushing our luck with XDrawArc */
		XPSDrawArc (dsp, ss_pm, egc, arcx, arcy, arcw, arch, 0,360*64);
	    }
	}

	/* display each point and any additional points as trails. */
	for (dbidx = 0; dbidx < NOBJ; dbidx++) {
	    Obj *op = db_basic(dbidx);
	    GC gc;

	    obj_pickgc (op, ssda_w, &gc); /* moonColor for Earth? */

	    if (npoints[dbidx] > 0 && obj_on[dbidx]) {
		int np = npoints[dbidx];
		int i;

		for (i = 0; i < np; i++) {
		    HLoc *hp = &points[dbidx][i];
		    HLoc leghl; /* for projection of planet on ecliptic plane */

		    if (!trails && i > 0)
			break;

		    /* compute screen location */
		    ss_loc (hp, scale, selt, celt, selg, celg, ss_nx, ss_ny);

		    /* draw leg down to ecliptic if desired */
		    if (blegs) {
			leghl = *hp;
			leghl.z = 0;
			ss_loc (&leghl, scale, selt, celt, selg, celg,
								ss_nx, ss_ny);
			XPSDrawLine(dsp, ss_pm, gc, hp->sx, hp->sy, leghl.sx,
								    leghl.sy);
		    }

		    /* draw a blob for the object */
		    XPSDrawArc (dsp, ss_pm, gc, hp->sx-1, hp->sy-1, BLOBW,BLOBW,
								    0, 64*360);

		    /* connect and draw time stamp if more than one trail point.
		     * (first point is current pos; 2nd is first of trail set)
		     * we do our own tick marks.
		     */
		    if (i > 1) {
			HLoc *pp = &points[dbidx][i-1];	/* prior trail point */

			tr_draw(dsp, ss_pm, gc, 0, &hp->trts,
							i==2 ? &pp->trts : NULL,
				    &trstate, pp->sx, pp->sy, hp->sx, hp->sy);
			anytrails++;
		    }

		    /* draw the SUN as a circle at the center */
		    if (dbidx == SUN)
			XPSDrawArc (dsp, ss_pm, gc, hp->sx-3, hp->sy-3,
							    7, 7, 0, 64*360);

		    /* draw the object name if desired.
		     * first item on list uses real object's flags.
		     */
		    if (i > 0)
			op = &hp->o;
		    if ((op->o_flags & WANTLBL) || (i == 0 && nametags)) {
			char *name = dbidx == MOON ? earthname : op->o_name;
			XPSDrawString (dsp, ss_pm, gc, hp->sx+GAP, hp->sy,
							    name, strlen(name));
		    }
		}
	    }
	}

	/* now draw the "all" objects, if interested */
	ap_free();
	if (allobj)
	    ss_allobj (dsp, 0, scale, selt, celt, selg, celg, ss_nx, ss_ny);

	/* copy the work pixmap to the screen */
	XCopyArea (dsp, ss_pm, ss_win, bgc, 0, 0, ss_nx, ss_ny, 0, 0);

	/* all finished */
	watch_cursor (0);
}

/* redraw the stereo view.
 */
static void
ss_st_all()
{
	Display *dsp = XtDisplay(stda_w);
	Window st_win = XtWindow(stda_w);
	unsigned int st_nx, st_ny;
	unsigned int bw, d;
	Window root;
	int x, y;
	int dbidx;
	int sv;			/* ScaleValue tmp */
	double scale;		/* pixels per au */
	double elt, selt, celt;	/* heliocentric lat of eye, rads */
	double elg, selg, celg;	/* heliocentric lng of eye, rads */

	watch_cursor (1);

	/* get the sizes of the windows we'll use */
	XGetGeometry(dsp, st_win, &root, &x, &y, &st_nx, &st_ny, &bw, &d);

	/* clear */
	XFillRectangle (dsp, st_pm, bgc, 0, 0, st_nx, st_ny);

	/* establish the scales */
	XmScaleGetValue (hr_w, &sv);
	scale = MINMAG * pow (MAXMAG/MINMAG, sv/100.);
	XmScaleGetValue (hlat_w, &sv);
	elt = degrad(sv);
	selt = sin(elt);
	celt = cos(elt);
	XmScaleGetValue (hlng_w, &sv);
	elg = degrad(sv+90.0);
	selg = sin(elg);
	celg = cos(elg);

	/* draw the ecliptic plane, if desired */
	if (ecliptic) {
	    double minau, maxau;
	    double aus[NECLPT+2];
	    char spacing[64];
	    int nau, i;

	    XSetFont (XtD, egc, efsp->fid);

	    minau = 0;
	    maxau = st_nx > st_ny ? st_nx/2.0/scale : st_ny/2.0/scale;
	    nau = tickmarks (minau, maxau, NECLPT, aus);

	    /* draw tick mark spacing message in upper left corner */
	    (void) sprintf (spacing, "%g AU", aus[1] - aus[0]);
	    XDrawString(dsp, st_pm, egc, 1, asc+1, spacing, strlen(spacing));

	    /* draw each grid line.
	     * the stereo view uses polylines drawn in world coords -- slower!
	     */
	    for (i = 0; i < nau; i++) {
		double au = aus[i];
		XPoint xps[NECLSEG+1];	/* +1 to close */
		int s;

		if (au <= 0.0)
		    continue;

		for (s = 0; s < NECLSEG; s++) {
		    double hlng = s*2*PI/NECLSEG;
		    HLoc hloc;

		    hloc.x = au*cos(hlng);
		    hloc.y = au*sin(hlng);
		    hloc.z = 0.0;
		    ss_loc (&hloc, scale, selt, celt, selg, celg, st_nx, st_ny);
		    xps[s].x = (short) hloc.stx;
		    xps[s].y = (short) hloc.sy;
		}

		xps[NECLSEG] = xps[0];	/* close */
		XDrawLines (dsp, st_pm, egc, xps, NECLSEG+1, CoordModeOrigin);
	    }
	}

	/* display each point and any additional points as trails. */
	for (dbidx = 0; dbidx < NOBJ; dbidx++) {
	    Obj *op = db_basic(dbidx);
	    GC gc;

	    obj_pickgc (op, ssda_w, &gc); /* moonColor for Earth? */

	    if (npoints[dbidx] > 0 && obj_on[dbidx]) {
		int np = npoints[dbidx];
		int i;

		for (i = 0; i < np; i++) {
		    HLoc *hp = &points[dbidx][i];
		    HLoc leghl; /* for projection of planet on ecliptic plane */

		    if (!trails && i > 0)
			break;

		    /* compute screen location */
		    ss_loc (hp, scale, selt, celt, selg, celg, st_nx, st_ny);

		    /* draw leg down to ecliptic if desired */
		    if (blegs) {
			leghl = *hp;
			leghl.z = 0;
			ss_loc (&leghl, scale, selt, celt, selg, celg,
								st_nx, st_ny);
			XPSDrawLine(dsp, st_pm, gc, hp->stx, hp->sy, leghl.stx,
								    leghl.sy);
		    }

		    /* draw a blob for the object */
		    XPSDrawArc (dsp, st_pm, gc, hp->stx-1, hp->sy-1,BLOBW,BLOBW,
								    0, 64*360);

		    /* connect and draw time stamp if more than one trail point.
		     * (first point is current pos; 2nd is first of trail set)
		     * we do our own tick marks.
		     */
		    if (i > 1) {
			HLoc *pp = &points[dbidx][i-1];	/* prior trail point */

			tr_draw(dsp, st_pm, gc, 0, &hp->trts,
							i==2 ? &pp->trts : NULL,
				    &trstate, pp->stx, pp->sy, hp->stx, hp->sy);
			anytrails++;
		    }

		    /* draw the SUN as a circle at the center */
		    if (dbidx == SUN)
			XPSDrawArc (dsp, st_pm, gc, hp->stx-3, hp->sy-3,
							    7, 7, 0, 64*360);

		    /* draw the object name if desired.
		     * first item on list uses real object's flags.
		     */
		    if (i > 0)
			op = &hp->o;
		    if ((op->o_flags & WANTLBL) || (i == 0 && nametags)) {
			char *name = dbidx == MOON ? earthname : op->o_name;
			XPSDrawString (dsp, st_pm, gc, hp->stx+GAP, hp->sy,
							    name, strlen(name));
		    }
		}
	    }
	}

	/* now draw the "all" objects, if interested */
	if (allobj)
	    ss_allobj (dsp, 1, scale, selt, celt, selg, celg, st_nx, st_ny);

	/* copy the work pixmap to the screen */
	XCopyArea (dsp, st_pm, st_win, bgc, 0, 0, st_nx, st_ny, 0, 0);

	/* all finished */
	watch_cursor (0);
}

/* draw all the comets and asteroids in the db.
 * if stview it's the stereo view so use st_pm and HLoc.stx, else ss_pm/sx.
 * add the coordinates in allp[] unless drawing the stereo view.
 */
static void
ss_allobj (dsp, stview, scale, selt, celt, selg, celg, nx, ny)
Display *dsp;
int stview;
double scale;		/* pixels per au */
double selt, celt;	/* heliocentric lat of eye, rads */
double selg, celg;	/* heliocentric lng of eye, rads */
unsigned nx, ny;
{
#define	ASCHSZ		50
	XPoint xps[ASCHSZ], *xp = xps;
	XSegment xls[ASCHSZ], *xl = xls;
	XArc xas[ASCHSZ], *xa = xas;
	Drawable win = stview ? st_pm : ss_pm;
	int dbmask = ELLIPTICALM | HYPERBOLICM | PARABOLICM;
	Obj *op;
	DBScan dbs;
	GC gc = 0;

	for (db_scaninit(&dbs, dbmask, NULL, 0); (op = db_scan(&dbs))!=NULL; ) {
	    double sd;
	    HLoc hl;

	    db_update (op);
	    if (!gc)
		obj_pickgc (op, ssda_w, &gc);	/* use first asteriod for gc */

	    sd = op->s_sdist;
	    hl.x = sd*cos(op->s_hlat)*cos(op->s_hlong);
	    hl.y = sd*cos(op->s_hlat)*sin(op->s_hlong);
	    hl.z = sd*sin(op->s_hlat);

	    ss_loc (&hl, scale, selt, celt, selg, celg, nx, ny);

	    if (!stview) {
		if (ap_add (op, &hl) < 0) {
		    xe_msg ("No memory for All Objects", 1);
		    ap_free();
		    return;
		}
	    }

	    /* draw object, possibly with a leg to the ecliptic.
	     * draw as a blob if want legs, else just a point
	     */
	    if (dblegs) {
		HLoc leghl;

		/* the blob */
		xa->x = (short)((stview ? hl.stx : hl.sx) - BLOBW/2);
		xa->y = (short)hl.sy;
		xa->width = BLOBW;
		xa->height = BLOBW;
		xa->angle1 = 0;
		xa->angle2 = 360*64;
		xa++;

		if (xa == &xas[ASCHSZ]) {
		    XPSDrawArcs (dsp, win, gc, xas, ASCHSZ);
		    xa = xas;
		}

		/* the leg */
		leghl = hl;
		leghl.z = 0;
		ss_loc (&leghl, scale, selt, celt, selg, celg, nx, ny);
		if (stview) {
		    xl->x1 = (short)hl.stx;
		    xl->x2 = (short)leghl.stx;
		} else {
		    xl->x1 = (short)hl.sx;
		    xl->x2 = (short)leghl.sx;
		}
		xl->y1 = (short)hl.sy;
		xl->y2 = (short)leghl.sy;
		xl++;

		if (xl == &xls[ASCHSZ]) {
		    XPSDrawSegments (dsp, win, gc, xls, ASCHSZ);
		    xl = xls;
		}
	    } else {
		xp->x = stview ? (short)hl.stx : (short)hl.sx;
		xp->y = (short)hl.sy;
		xp++;

		if (xp == &xps[ASCHSZ]) {
		    XPSDrawPoints (dsp, win, gc, xps, ASCHSZ, CoordModeOrigin);
		    xp = xps;
		}
	    }

	    /* draw label if interested */
	    if (nametags || (op->o_flags & WANTLBL))
		XPSDrawString (dsp, win, gc, stview ? hl.stx+GAP : hl.sx+GAP,
					hl.sy, op->o_name, strlen(op->o_name));
	}

	/* clean up any partial items */
	if (xp > xps)
	    XPSDrawPoints (dsp, win, gc, xps, xp - xps, CoordModeOrigin);
	if (xl > xls)
	    XPSDrawSegments (dsp, win, gc, xls, xl - xls);
	if (xa > xas)
	    XPSDrawArcs (dsp, win, gc, xas, xa - xas);
}

/* compute location of HLoc in window of size [nx,ny].
 * N.B. others assume we only use hp->{x,y,z} and set lp->{sx,sy,sty}
 */
static void
ss_loc (hp, scale, selt, celt, selg, celg, nx, ny)
HLoc *hp;
double scale;		/* mag factor */
double selt, celt;	/* sin/cos heliocentric lat of eye, rads */
double selg, celg;	/* sin/cos heliocentric lng of eye, rads */
unsigned nx, ny;	/* size of drawing area, in pixels */
{
	double x, y, z;	/* progressive transform values... */
	double xp, yp, zp;
	double xpp, ypp, zpp;
	double back;

	/* initial loc of points[i] */
	x = hp->x;
	y = hp->y;
	z = hp->z;

	/* rotate by -elg about z axis to get to xz plane.
	 * once we rotate up about x to the z axis (next step) that will put
	 * +x to the right and +y up.
	 * tmp = -elg;
	 * xp = x*cos(tmp) - y*sin(tmp);
	 * yp = x*sin(tmp) + y*cos(tmp);
	 */
	xp =  x*celg + y*selg;
	yp = -x*selg + y*celg;
	zp = z;

	/* rotate by -(PI/2-elt) about x axis to get to z axis.
	 * +x right, +y up, +z towards, all in AU.
	 * tmp = -(PI/2-elt);
	 * ypp = yp*cos(tmp) - zp*sin(tmp);
	 * zpp = yp*sin(tmp) + zp*cos(tmp);
	 */
	xpp = xp;
	ypp =  yp*selt + zp*celt;
	zpp = -yp*celt + zp*selt;

	/* now, straight ortho projection */
	hp->sx = (int)(nx/2 + xpp*scale);
	hp->sy = (int)(ny/2 - ypp*scale);

	/* back is y coord, in AU, behind which there is no parallax.
	 * parallax is the offset of the sun (at a=0), in pixels.
	 */
	back = (nx > ny ? nx : ny)/-2.0/scale;  /* based on screen size */
	if (zpp < back)
	    hp->stx = hp->sx;
	else
	    hp->stx = hp->sx + (int)(parallax*(back-zpp)/back);
}

/* called by trails.c to create a new set of trails for all solar sys objects.
 * TODO: client is unused; any validation checks we can do?
 * return 0 if ok else -1.
 */
/* ARGSUSED */
static int
ss_newtrail (ts, statep, client)
TrTS ts[];
TrState *statep;
XtPointer client;
{
	Now *np = mm_get_now();
	int dbidx;
	Now n;

	watch_cursor (1);

	/* discard previous set and just insert the current position */
	hloc_reset(np);

	/* work with a local copy since we change mjd */
	n = *np;

	/* add statep->nticks entries for each object.
	 * N.B. we tag earth's data (from SUN object) as object MOON.
	 */
	for (dbidx = 0; dbidx < NOBJ; dbidx++) {
	    int i;

	    for (i = 0; i < statep->nticks; i++) {
		TrTS *tp = &ts[i];

		/* set up n.n_mjd */
		n.n_mjd = tp->t;

		/* add an entry */
		if (hloc_add (dbidx, &n, tp->lbl) < 0)
		    break;
	    }
	}

	/* retain state for next time */
	trstate = *statep;

	/* enable Trail option as a service */
	if (!trails) {
	    XmToggleButtonSetState (trails_w, True, False);
	    trails = 1;
	}

	/* redraw everything to display the new trails, if we are up */
	if (isUp(ssshell_w))
	    ss_all ();

	watch_cursor (0);

	return (0);
}

/* set the first entry (the one for the current object location) for each
 * object at *np, and erase any other entries.
 */
static void
hloc_reset (np)
Now *np;
{
	int dbidx;

	for (dbidx = 0; dbidx < NOBJ; dbidx++)  {
	    /* free all points memory */
	    if (points[dbidx])
		free ((char*)points[dbidx]);
	    points[dbidx] = NULL;
	    npoints[dbidx] = 0;

	    /* add a single entry for current location of object */
	    (void) hloc_add (dbidx, np, 0);
	}
}

/* make room for one more entry for the given db object in the points array and
 *   return its new address or NULL and xe_msg() if no more memory.
 */
static HLoc *
hloc_grow (dbidx)
int dbidx;
{
	char *newmem;
	int n = npoints[dbidx] + 1;

	if (points[dbidx])
	    newmem = realloc (points[dbidx], n*sizeof(HLoc));
	else
	    newmem = malloc (n*sizeof(HLoc));
	if (!newmem) {
	    xe_msg ("No memory for new point", 0);
	    return (NULL);
	}

	points[dbidx] = (HLoc *) newmem;
	npoints[dbidx] = n;

	return (&points[dbidx][n-1]);
}

/* add one entry for the given database object for np.
 * N.B. we tag earth's data (from SUN object) as object MOON.
 * return 0 if ok else xe_msg() and -1 if trouble.
 */
static int
hloc_add (dbidx, np, lbl)
int dbidx;
Now *np;
int lbl;
{
	Obj *op = db_basic (dbidx);
	HLoc *hp;
	double sd;

	/* skip objects not in solar system */
	if (!is_ssobj(op))
	    return(0);

	/* just one SUN entry will do since it is fixed at the center */
	if (dbidx == SUN && npoints[SUN] > 0)
	    return(0);

	/* add memory for one more entry */
	hp = hloc_grow (dbidx);
	if (!hp)
	    return (-1); /* go with what we have */

	/* compute the circumstances at np and save in hp->o.
	 * we get earth info from SUN then tag it as MOON.
	 */
	if (dbidx == MOON) {
	    /* really want earth info here; get it from SUN */
	    op = db_basic (SUN);
	    hp->o = *op;
	    op = &hp->o;
	    (void) obj_cir (np, op);
	    sd = op->s_edist;
	    op->pl.pl_code = MOON;
	} else {
	    hp->o = *op;
	    op = &hp->o;
	    (void) obj_cir (np, op);
	    sd = op->s_sdist;
	}

	/* don't inherit WANTLBL */
	op->o_flags &= ~WANTLBL;

	/* compute cartesian coords */
	hp->x = sd*cos(op->s_hlat)*cos(op->s_hlong);
	hp->y = sd*cos(op->s_hlat)*sin(op->s_hlong);
	hp->z = sd*sin(op->s_hlat);

	/* save the trail info */
	hp->trts.t = mjd;
	hp->trts.lbl = lbl;

	return (0);
}

/* free allp array, if any */
static void
ap_free()
{
	if (allp) {
	    free ((void *)allp);
	    allp = NULL;
	    nallp = 0;
	    mallp = 0;
	}
}

/* add op and hp to allp.
 * return 0 if ok else -1.
 */
static int
ap_add (op, hp)
Obj *op;
HLoc *hp;
{
	AllP *ap;

	/* insure there is room for one more */
	if (nallp >= mallp) {
	    char *newmem;

	    newmem = allp ? realloc ((void *)allp, (mallp+ALLPCK)*sizeof(AllP))
	    		  : malloc (ALLPCK*sizeof(AllP));
	    if (!newmem)
		return (-1);
	    allp = (AllP *)newmem;
	    mallp += ALLPCK;
	}

	/* add to end of list, and inc count */
	ap = &allp[nallp++];
	ap->op = op;
	ap->sx = hp->sx;
	ap->stx = hp->stx;
	ap->sy = hp->sy;

	return (0);
}

/* called by the persistent label popup PB.
 * toggle the WANTLBL of pu.op and redraw.
 */
/* ARGSUSED */
static void
ap_label_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Obj *op = pu.op;

	if (op->o_flags & WANTLBL)
	    op->o_flags &= ~WANTLBL;
	else
	    op->o_flags |= WANTLBL;

	ss_all();
}

/* called to make the object at pu.op the current ObjXYZ as per client.
 * N.B. we assume pu.op is not a basic object because ss_popup() only enables
 *   this button then.
 */
/* ARGSUSED */
static void
ap_mkobj_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	obj_set(pu.op, (int)client);
}

/* called to enable a basic object in the data view. the trick is to puzzle
 * out which one from the pointers in pu.
 */
/* ARGSUSED */
static void
ss_dv_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int dbidx, i;

	for (dbidx = 0; dbidx < NOBJ; dbidx++) {
	    if (pu.op == db_basic(dbidx))
		goto found;
	    for (i = 0; i < npoints[dbidx]; i++)
		if (pu.op == &points[dbidx][i].o)
		    goto found;
	}
	return;	/* oh well */

    found:
	if (dbidx == MOON)
	    dbidx = SUN;
	dm_newobj (dbidx);
	dm_update (mm_get_now(), 1);
}

/* create our GCs.
 * TODO: reclaim old stuff if called again.
 */
static void
mk_gcs()
{
	Display *dsp = XtDisplay(ssda_w);
	Window win = XtWindow(ssda_w);
	XGCValues gcv;
	unsigned int gcm;
	XFontStruct *fs;
	Pixel p;

	gcm = GCForeground;
	get_something (ssda_w, XmNbackground, (XtArgVal)&p);
	gcv.foreground = p;
	bgc = XCreateGC (dsp, win, gcm, &gcv);
	fs = XQueryFont (dsp, XGContextFromGC (bgc));
	asc = fs->max_bounds.ascent;

	gcm = GCForeground;
	get_something (ssda_w, XmNforeground, (XtArgVal)&p);
	gcv.foreground = p;
	egc = XCreateGC (dsp, win, gcm, &gcv);
	get_views_font (XtD, &efsp);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: solsysmenu.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
