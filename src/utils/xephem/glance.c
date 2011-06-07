/* code to manage the Night-at-a-Glance feature.
 */

#include <stdio.h>
#include <math.h>

#if defined(__STDC__)
#include <stdlib.h>
#include <string.h>
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
#include "dm.h"
#include "ps.h"


extern Widget toplevel_w;
extern XtAppContext xe_app;
#define XtD XtDisplay(toplevel_w)
extern Colormap xe_cm;

extern Now *mm_get_now P_((void));
extern Obj *db_basic P_((int id));
extern Obj *db_scan P_((DBScan *sp));
extern char *mm_getsite P_((void));
extern int gray_ramp P_((Display *dsp, Colormap cm, Pixel **pix));
extern int isUp P_((Widget shell));
extern void db_scaninit P_((DBScan *sp, int mask, ObjF *op, int nop));
extern void db_update P_((Obj *op));
extern void dm_riset P_((Now *np, Obj *op, RiseSet *rsp));
extern void fs_time P_((char out[], double t));
extern void get_something P_((Widget w, char *resource, XtArgVal value));
extern void get_xmstring P_((Widget w, char *resource, char **txtp));
extern void get_xmlabel_font P_((Widget w, XFontStruct **f));
extern void get_views_font P_((Display *dsp, XFontStruct **fspp));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void obj_pickgc P_((Obj *op, Widget w, GC *gcp));
extern void prompt_map_cb P_((Widget w, XtPointer client, XtPointer call));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void timestamp P_((Now *np, Widget w));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *msg, int app_modal));

static int ng_ison P_((void));
static void ng_create_shell P_((void));
static void ng_create_popup P_((void));
static void ng_print_cb P_((Widget w, XtPointer client, XtPointer call));
static void ng_da_input_cb P_((Widget w, XtPointer client, XtPointer call));
static void ng_print P_((void));
static void ng_ps_annotate P_((void));
static void ng_popdown_cb P_((Widget w, XtPointer client, XtPointer call));
static void ng_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void ng_init_gcs P_((void));
static void ng_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void ng_exp_cb P_((Widget w, XtPointer client, XtPointer call));
static void ng_redraw P_((void));
static void ng_refresh P_((XExposeEvent *ep));
static void ng_drawpm P_((void));

/* main's widgets */
static Widget ngshell_w;	/* main shell */
static Widget ngda_w;		/* image view DrawingArea */
static Widget pu_w;		/* popup shell */
static Widget puname_w;		/* popup objects name label */
static Pixmap ng_pm;		/* image view staging pixmap */
static Widget dt_w;		/* main date/time stamp widget */

static GC ng_gc;		/* misc GC */
static XFontStruct *ng_vfs;	/* views font */
static XFontStruct *ng_lfs;	/* label font */
static int ng_w, ng_h;		/* current ngda_w size */
static int skyh;		/* sky region height, pixels */
static int midx;		/* window x @ local midnight */
static int leftday, rightday;	/* days +/1 wrt midx */

static Pixel *ngcolors;		/* gray-scale ramp */
static int ncolors;		/* actual number in ngcolors[] */

#define	UPLT	3		/* Up-time line thickness, pixels */
#define	TMLT	2		/* major tick mark line thickness, pixels */

/* info about stuff to put in the popup */
typedef struct {
    char *prompt;		/* prompt, or NULL for gap */
    int id;			/* one of *_ID from dm.h */
    char *tip;			/* helpful tip */
    Widget w;			/* label widget */
} PopupInfo;
static PopupInfo puinfo[] = {
    {"Rise:", RSTIME_ID,
	"Rise time during date below, as per time zone Preference"},
    {"Transit:", TRTIME_ID,
	"Transit time during date below, as per time zone Preference"},
    {"Transit Alt:", TRALT_ID,
	"Transit altitude during date below, DD:MM"},
    { "Set:", SETTIME_ID,
	"Set time during date below, as per time zone Preference"},
};

static char naagcategory[] = "Night at a Glance";	/* Save category */

/* called when the NG view is activated via the main menu pulldown.
 * if first time, build everything, else just get going.
 */
void
ng_manage ()
{
	if (!ngshell_w) {
	    ng_create_shell();
	    ng_create_popup();
	    ng_init_gcs();
	}

	XtPopup (ngshell_w, XtGrabNone);
	set_something (ngshell_w, XmNiconic, (XtArgVal)False);
}

/* commanded from main to update with a new set of circumstances */
/* ARGSUSED */
void
ng_update (np, how_much)
Now *np;
int how_much;
{
	/* only if we're up */
	if (!ng_ison())
	    return;
	ng_redraw();
}

/* called when basic resources change.
 * rebuild and redraw.
 */
void
ng_newres()
{
	if (!ngshell_w)
	    return;
	ng_init_gcs();
	ng_update (mm_get_now(), 1);
}

/* called when a user-defined object has changed.
 */
void
ng_newobj(dbidx)
int dbidx;		/* OBJXYZ */
{
	/* only if we're up */
	if (!ng_ison())
	    return;
	ng_redraw();
}

/* called to put up or remove the watch cursor.  */
void
ng_cursor (c)
Cursor c;
{
	Window win;

	if (ngshell_w && (win = XtWindow(ngshell_w)) != 0) {
	    Display *dsp = XtDisplay(ngshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

static int
ng_ison()
{
	return (isUp(ngshell_w));
}

static void
ng_create_shell()
{
	Widget mb_w, pd_w, cb_w, fr_w;
	Widget ngform_w;
	Widget w;
	Arg args[20];
	int n;

	/* create master shell and a form */

	n = 0;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle, "xephem Night at a Glance"); n++;
	XtSetArg (args[n], XmNiconName, "Night"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	ngshell_w = XtCreatePopupShell ("NaaGlance", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (ngshell_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (ngshell_w, XmNpopdownCallback, ng_popdown_cb, 0);
	sr_reg (ngshell_w, "XEphem*NaaGlance.width", naagcategory, 0);
	sr_reg (ngshell_w, "XEphem*NaaGlance.height", naagcategory, 0);
	sr_reg (ngshell_w, "XEphem*NaaGlance.x", naagcategory, 0);
	sr_reg (ngshell_w, "XEphem*NaaGlance.y", naagcategory, 0);

	n = 0;
	XtSetArg (args[n], XmNhorizontalSpacing, 5); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	XtSetArg (args[n], XmNmarginWidth, 5); n++;
	ngform_w = XmCreateForm (ngshell_w, "NaaGlanceF", args, n);
	XtAddCallback (ngform_w, XmNhelpCallback, ng_help_cb, 0);
	XtManageChild (ngform_w);

	/* create the menu bar across the top */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	mb_w = XmCreateMenuBar (ngform_w, "MB", args, n);
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
	    w = XmCreatePushButton (pd_w, "MPrint", args, n);
	    set_xmstring (w, XmNlabelString, "Print...");
	    XtAddCallback (w, XmNactivateCallback, ng_print_cb, 0);
	    wtip (w, "Print the current map");
	    XtManageChild (w);

	    /* add a separator */
	    n = 0;
	    w = XmCreateSeparator (pd_w, "CtS", args, n);
	    XtManageChild (w);

	    /* add the close button */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, ng_close_cb, 0);
	    wtip (w, "Close this dialog");
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

	    n = 0;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    w = XmCreatePushButton (pd_w, "Help", args, n);
	    XtAddCallback (w, XmNactivateCallback, ng_help_cb, 0);
	    XtManageChild (w);
	    wtip (w, "More info about this dialog");

	/* make a label for the date stamp */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrecomputeSize, False); n++;
	dt_w = XmCreateLabel (ngform_w, "DateStamp", args, n);
	timestamp (mm_get_now(), dt_w);	/* sets initial size */
	wtip (dt_w, "Date and Time for which map is computed");
	XtManageChild (dt_w);

	/* make a drawing area for the image view in a frame */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, mb_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, dt_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	fr_w = XmCreateFrame (ngform_w, "Frame", args, n);
	XtManageChild (fr_w);

	    n = 0;
	    ngda_w = XmCreateDrawingArea (fr_w, "Map", args, n);
	    XtAddCallback (ngda_w, XmNexposeCallback, ng_exp_cb, NULL);
	    XtAddCallback (ngda_w, XmNinputCallback, ng_da_input_cb, 0);
	    XtManageChild (ngda_w);
}

static void
ng_create_popup()
{
	Arg args[20];
	Widget w;
	int i, n;

	/* create the outer form */
	n = 0;
	XtSetArg (args[n], XmNisAligned, True); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	pu_w = XmCreatePopupMenu (ngda_w, "NGPopup", args, n);

	n = 0;
	puname_w = XmCreateLabel (pu_w, "PUName", args, n);
	wtip (puname_w, "Name of object");
	XtManageChild (puname_w);

	/* create the label widgets */
	for (i = 0; i < XtNumber(puinfo); i++) {
	    PopupInfo *pip = &puinfo[i];

	    n = 0;
	    w = XmCreateLabel (pu_w, "NGPopUL", args, n);
	    pip->w = w;
	    XtManageChild (w);
	    wtip (w, pip->tip);

	    if (!pip->prompt)
		set_xmstring (w, XmNlabelString, " ");
	}
}

/* callback from the Print PB */
/* ARGSUSED */
static void
ng_print_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        XPSAsk ("Night-at-a-Glance", ng_print);
}

/* callback from the inputCallback*/
/* ARGSUSED */
static void
ng_da_input_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	Now *np = mm_get_now();
	Now daynow;
	RiseSet rs;
	XEvent *ev;
	Obj *op;
	int obji;
	int i;

	/* confirm button 3 press event within sky portion */
	if (c->reason != XmCR_INPUT)
	    return;
	ev = c->event;
	if (ev->xany.type != ButtonPress || ev->xbutton.button != 3)
	    return;
	if (ev->xbutton.y >= skyh)
	    return;

	/* get Obj */
	obji = ev->xbutton.y*NOBJ/skyh;
	op = db_basic (obji);
	if (op->o_type == UNDEFOBJ)
	    return;

	/* set np to proper day */
	if (ev->xbutton.x < midx) {
	    if (leftday) {
		daynow = *np;
		daynow.n_mjd += leftday;
		np = &daynow;
	    }
	} else {
	    if (rightday) {
		daynow = *np;
		daynow.n_mjd += rightday;
		np = &daynow;
	    }
	}

	/* set name */
	set_xmstring (puname_w, XmNlabelString, op->o_name);

	/* find and set info */
	dm_riset (np, op, &rs);
	for (i = 0; i < XtNumber(puinfo); i++) {
	    PopupInfo *pip = &puinfo[i];
	    if (pip->prompt) {
		char fmt[64], buf[64];
		dm_colFormat (np, op, &rs, pip->id, fmt);
		sprintf (buf, "%s %s", pip->prompt, fmt);
		set_xmstring (pip->w, XmNlabelString, buf);
	    }
	}

	/* popup */
	XmMenuPosition (pu_w, (XButtonPressedEvent *)ev);
	XtManageChild (pu_w);
}

/* proceed to generate a postscript file.
 * call XPSClose() when finished.
 */
static void
ng_print ()
{
	/* must be up */
	if (!ng_ison()) {
	    xe_msg ("N-ata-G must be open to print.", 1);
	    XPSClose();
	    return;
	}

	watch_cursor(1);

	/* fit view in square across the top and prepare to capture X calls */
	if (ng_w > ng_h)
	    XPSXBegin (ng_pm, 0, 0, ng_w, ng_h, 1*72, 10*72, (int)(6.5*72));
	else {
	    int pw = (int)(72*6.5*ng_w/ng_h+.5);  /* width on paper as 6.5 hi */
	    XPSXBegin (ng_pm, 0, 0, ng_w, ng_h, (int)((8.5*72-pw)/2), 10*72,pw);
	}

	/* redraw everything now */
	ng_redraw();

        /* no more X captures */
	XPSXEnd();

	/* add some extra info */
	ng_ps_annotate ();

	/* finished */
	XPSClose();

	watch_cursor(0);
}

static void
ng_ps_annotate ()
{
	Now *np = mm_get_now();
        char dir[128];
	char buf[128];
	int ctr = 306;  /* = 8.5*72/2 */
	int lx = 145, rx = 460;
	char *site;
	int y;

	/* caption */
	y = AROWY(10);
	(void) strcpy (buf, "XEphem Night-at-a-Glance");
	(void) sprintf (dir, "(%s) %d %d cstr", buf, ctr, y);
	XPSDirect (dir);

	/* put site name under caption */
	site = mm_getsite();
	if (site) {
	    y = AROWY(9);
	    (void) sprintf (dir, "(%s) %d %d cstr\n",
	    				XPSCleanStr(site,strlen(site)), ctr, y);
	    XPSDirect (dir);
	}

	/* date and time on the left */
	y = AROWY(9);
	fs_date (buf, mjd_day(mjd));
	(void) sprintf (dir, "(UTC Date:) %d %d rstr (%s) %d %d lstr\n",
							lx, y, buf, lx+10, y);
	XPSDirect (dir);

	y = AROWY(8);
	fs_time (buf, mjd_hr(mjd));
	(void) sprintf (dir, "(UTC Time:) %d %d rstr (%s) %d %d lstr\n",
							lx, y, buf, lx+10, y);
	XPSDirect (dir);

	/* lat/long on the right */
	y = AROWY(9);

	fs_sexa (buf, raddeg(fabs(lat)), 3, 3600);
	(void) sprintf (dir, "(Latitude:) %d %d rstr (%s %c) %d %d lstr\n",
				    rx, y, buf, lat < 0 ? 'S' : 'N', rx+10, y);
	XPSDirect (dir);

	y = AROWY(8);
	fs_sexa (buf, raddeg(fabs(lng)), 4, 3600);
	(void) sprintf (dir,"(Longitude:) %d %d rstr (%s %c) %d %d lstr\n",
				    rx, y, buf, lng < 0 ? 'W' : 'E', rx+10, y);
	XPSDirect (dir);
}

/* callback from ngshell_w being popped down. */
/* ARGSUSED */
static void
ng_popdown_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (ng_pm) {
	    XFreePixmap (XtD, ng_pm);
	    ng_pm = 0;
	}
}

/* called from Close button */
static void
ng_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtPopdown (ngshell_w);
	/* popping down ngshell_w will do all the real work */
}

/* callback from the Help all button
 */
/* ARGSUSED */
static void
ng_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg[] = {
	    "This is the night at a glance.",
	};

	hlp_dialog ("NightAtAGlance", msg, XtNumber(msg));
}

/* expose (or reconfig) of NAAG drawing area.
 */
/* ARGSUSED */
static void
ng_exp_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	Display *dsp = XtDisplay(ngda_w);
	Window win = XtWindow(ngda_w);
	Window root;
	int x, y;
	unsigned int bw, d;
	unsigned int wid, hei;

	switch (c->reason) {
	case XmCR_EXPOSE: {
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
	    printf ("Unexpected ngda_w event. type=%d\n", c->reason);
	    exit(1);
	}

	XGetGeometry (dsp, win, &root, &x, &y, &wid, &hei, &bw, &d);

	if (!ng_pm || (int)wid != ng_w || (int)hei != ng_h) {
	    if (ng_pm) {
		XFreePixmap (dsp, ng_pm);
		ng_pm = (Pixmap) NULL;
	    }

	    ng_pm = XCreatePixmap (dsp, win, wid, hei, d);
	    ng_w = wid;
	    ng_h = hei;

	    ng_redraw();
	} else {
	    /* same size; just copy from the pixmap */
	    ng_refresh(NULL);
	}
}

/* redraw the current scene from scratch */
static void
ng_redraw()
{
	ng_drawpm ();
	ng_refresh(NULL);
	timestamp (mm_get_now(), dt_w);
}

/* copy the ng_pm pixmap to the drawing area ngda_w.
 * if ep just copy that much, else copy all.
 */
static void
ng_refresh(ep)
XExposeEvent *ep;
{
	Display *dsp = XtDisplay(ngda_w);
	Window win = XtWindow (ngda_w);
	Pixmap pm = ng_pm;
	unsigned w, h;
	int x, y;

	/* ignore of no pixmap now */
	if (!pm)
	    return;

	if (ep) {
	    x = ep->x;
	    y = ep->y;
	    w = ep->width;
	    h = ep->height;
	} else {
	    w = ng_w;
	    h = ng_h;
	    x = y = 0;
	}

	XCopyArea (dsp, pm, win, ng_gc, x, y, w, h, x, y);
}

/* get the gray scale and other one-time pixel-related stuff.
 * N.B. just call this once.
 * TODO: reclaim old stuff if called again.
 */
static void
ng_init_gcs()
{
	Window win = XtWindow(toplevel_w);
	Display *dsp = XtD;

	/* gc for pixmap copying and wide lines */
	ng_gc = XCreateGC (dsp, win, 0L, NULL);

	/* prepare for strings */
	get_views_font (dsp, &ng_vfs);
	get_xmlabel_font (dt_w, &ng_lfs);

	/* build gray ramp for night fade */
	ncolors = gray_ramp (dsp, xe_cm, &ngcolors);
}

/* find x positions of line for given rise/set circumstances.
 * we position across ng_w based on local time with 0 in the center.
 * N.B. this is just the geometry, we ignore rp->rs_flags
 */
static void
rs_x (np, rp, xr, xs)
Now *np;
RiseSet *rp;
int *xr, *xs;
{
	double t;

	t = fmod (mjd_hr(rp->rs_risetm)-tz+36.0, 24.0)/24.0;
	*xr = (int)floor(ng_w*t+0.5);
	t = fmod (mjd_hr(rp->rs_settm)-tz+36.0, 24.0)/24.0;
	*xs = (int)floor(ng_w*t+0.5);
}

/* find x positions of line for given rise/set circumstances.
 * we position across ng_w based on local time where midnight is at midx.
 */
static int
rs_xrs (np, op, midx, nowx, xr, xs)
Now *np;
Obj *op;
int midx, nowx;
int *xr, *xs;
{
	RiseSet rs;
	int nowright;

	/* find x's */
	if (op->o_type == UNDEFOBJ)
	    return (-1);
	db_update (op);
	dm_riset (np, op, &rs);
	if (rs.rs_flags & (RS_RISERR|RS_SETERR|RS_NEVERUP))
	    return (-1);
	rs_x (np, &rs, xr, xs);

	/* if event occurs on opposite side of midnight as now, redo for
	 * its day
	 */
	nowright = (nowx > midx);
	if ((*xr > midx) != nowright || (*xs > midx) != nowright) {
	    Now daynow;
	    RiseSet dayrs;
	    int dayxr, dayxs;

	    memcpy ((void*)&daynow, (void*)np, sizeof(Now));
	    daynow.n_mjd += nowright ? -1 : 1;
	    dm_riset (&daynow, op, &dayrs);
	    rs_x (&daynow, &dayrs, &dayxr, &dayxs);
	    if ((*xr > midx) != nowright)
		*xr = dayxr;
	    if ((*xs > midx) != nowright)
		*xs = dayxs;
	}

	/* catch a few odd cases */
	if (rs.rs_flags & (RS_CIRCUMPOLAR|RS_NORISE))
	    *xr = 0;	/* already up */
	if (rs.rs_flags & (RS_CIRCUMPOLAR|RS_NOSET))
	    *xs = ng_w;	/* still up */
	return (0);
}

/* draw the entire scene in ng_pm, which is known to be ng_w x ng_h.
 * N.B. this just fills the pixmap; call ng_refresh() to copy to the screen.
 */
static void
ng_drawpm ()
{
	static char utclabel[] = "UTC";
	Now *np = mm_get_now();
	Display *dsp = XtD;
	char buf[32], buf2[32];
	int risex, setx;
	int dawnx, duskx;
	double dawn, dusk;
	int hw, n;
	int status;
	Obj *op;
	RiseSet rs;
	int mincol, maxcol, ncols;
	int dir, asc, des;
	XCharStruct overall;
	int fw, fh;
	int nowx;
	int lmarg;
	int x1, x2;
	int i;
	int x, y;

	/* get size of a typical string in the label font */
	XTextExtents (ng_lfs, "5", 1, &dir, &asc, &des, &overall);
	fw = overall.width;
	fh = 2*(overall.ascent + overall.descent);	/* room for gap */

	/* find left margin for TZ labels */
	x = strlen(tznm) + 1;	/* allow hour label moving left to be centered*/
	y = strlen(utclabel) + 1;	/* " */
	lmarg = fw * (x > y ? x : y);

	/* find graph height sans room for scale and labeling at bottom */
	skyh = ng_h - 4*fh;

	/* black-to-white is too harsh, use grays */
	mincol = ncolors/5;			/* dark */
	maxcol = 4*ncolors/5;			/* bright */
	ncols = maxcol-mincol;			/* rails, not posts */

	/* find times of twilight and sunrise/set */
	op = db_basic (SUN);
	db_update (op);
	twilight_cir (np, dip, &dawn, &dusk, &status);
	rs.rs_risetm = dawn;
	rs.rs_settm = dusk;
	rs.rs_flags = status;
	if (rs.rs_flags) {
	    dm_riset (np, op, &rs);
	    if (rs.rs_flags & RS_CIRCUMPOLAR) {
		risex = dawnx = 0;
		setx = duskx = ng_w-1;
	    } else if (rs.rs_flags) {
		setx = duskx = 0;
		risex = dawnx = ng_w-1;
	    } else {
		rs_x (np, &rs, &risex, &setx);
		mincol += (maxcol-mincol)/4;	/* no night.. not so dark */
		ncols = maxcol-mincol;
		dawnx = duskx = (risex+setx)/2;
		if (setx > risex) {
		    dawnx += ng_w/2;
		    duskx += ng_w/2;
		}
	    }
	} else {
	    rs_x (np, &rs, &dawnx, &duskx);
	    dm_riset (np, op, &rs);
	    if (rs.rs_flags) {
		maxcol -= (maxcol-mincol)/4;	/* no day.. not so bright */
		ncols = maxcol-mincol;
		risex = setx = (duskx+dawnx)/2;
		if (dawnx > duskx) {
		    risex += ng_w/2;
		    setx += ng_w/2;
		}
	    } else
		rs_x (np, &rs, &risex, &setx);
	}

	/* canonize to window size */
	dawnx %= ng_w;
	risex %= ng_w;
	setx %= ng_w;
	duskx %= ng_w;

	/* find x of local midnight and now */
	rs.rs_risetm = tz/24.+.5;	/* local midnight as an mjd */
	rs.rs_settm = mjd;		/* now */
	rs_x (np, &rs, &midx, &nowx);

	/* fill block at bottom for annotations -- screen only */
	XSetForeground (dsp, ng_gc, BlackPixel (dsp, DefaultScreen(dsp)));
	XFillRectangle (dsp, ng_pm, ng_gc, 0, skyh, ng_w, ng_h-skyh);

	/* draw night and day "sky".
	 * screen gets rectangles and shaded bands,
	 * paper just gets labeled lines at dawn and dusk.
	 */
	XSetForeground (dsp, ng_gc, ngcolors[mincol]);
	XSetLineAttributes (dsp, ng_gc, 0, LineSolid, CapButt, JoinMiter);
	if (XPSDrawing()) {
	    XSetFont (dsp, ng_gc, ng_lfs->fid);
	    XPSDrawString (dsp, ng_pm, ng_gc, dawnx-5*fw, 20, "Dawn", 4);
	    XPSDrawString (dsp, ng_pm, ng_gc, duskx+fw, 20, "Dusk", 4);
	    XPSDrawLine (dsp, ng_pm, ng_gc, dawnx, 0, dawnx, skyh);
	    XPSDrawLine (dsp, ng_pm, ng_gc, duskx, 0, duskx, skyh);
	}
	XSetForeground (dsp, ng_gc, ngcolors[maxcol]);
	if (setx > risex) {
	    XFillRectangle (dsp, ng_pm, ng_gc, risex, 0, setx-risex, skyh);
	} else {
	    XFillRectangle (dsp, ng_pm, ng_gc, risex, 0, ng_w-risex, skyh);
	    XFillRectangle (dsp, ng_pm, ng_gc, 0, 0, setx, skyh);
	}
	XSetForeground (dsp, ng_gc, ngcolors[mincol]);
	if (duskx > dawnx) {
	    XFillRectangle (dsp, ng_pm, ng_gc, duskx, 0, ng_w-duskx, skyh);
	    XFillRectangle (dsp, ng_pm, ng_gc, 0, 0, dawnx, skyh);
	} else {
	    XFillRectangle (dsp, ng_pm, ng_gc, duskx, 0, dawnx-duskx, skyh);
	}

	/* thin bands to show twilights -- beware of wrap */
	if (risex < dawnx)
	    risex += ng_w;
	for (x = dawnx; x < risex; x++) {
	    Pixel p = ngcolors[mincol+ncols*(x-dawnx)/(risex-dawnx)];
	    XSetForeground (dsp, ng_gc, p);
	    XDrawLine (dsp, ng_pm, ng_gc, x%ng_w, 0, x%ng_w, skyh-1);
	}
	if (setx > duskx)
	    duskx += ng_w;
	for (x = setx; x < duskx; x++) {
	    Pixel p = ngcolors[mincol+ncols*(duskx-x)/(duskx-setx)];
	    XSetForeground (dsp, ng_gc, p);
	    XDrawLine (dsp, ng_pm, ng_gc, x%ng_w, 0, x%ng_w, skyh-1);
	}

	/* overlay thin vertical line at current time */
	XSetForeground (dsp, ng_gc, WhitePixel (dsp, DefaultScreen(dsp)));
	XPSDrawLine (dsp, ng_pm, ng_gc, nowx, 0, nowx, skyh+fh);

	/* overlay the names and thick bars */
	XSetLineAttributes (dsp, ng_gc, UPLT, LineSolid, CapButt, JoinMiter);
	XSetFont (dsp, ng_gc, ng_vfs->fid);
	for (i = 0; i < NOBJ; i++) {
	    unsigned long gcm;
	    XGCValues gcv;
	    char *name;
	    GC gc;
	    int y;

	    /* get rise and set x values, or skip if undef or never up */
	    op = db_basic(i);
	    if (rs_xrs (np, op, midx, nowx, &risex, &setx) < 0)
		continue;

	    /* get the standard color for this object type */
	    obj_pickgc (op, ngda_w, &gc);

	    /* name a little above the line, avoid far right */
	    y = (i+1)*skyh/NOBJ-3*UPLT;
	    name = op->o_name;
	    if (risex > setx && ng_w-risex < 50)
		XPSDrawString (dsp, ng_pm, gc, 10, y, name, strlen(name));
	    else
		XPSDrawString (dsp, ng_pm, gc, risex, y, name, strlen(name));

	    /* grab the color but use our thick line gc */
	    gcm = GCForeground;
	    XGetGCValues (dsp, gc, gcm, &gcv);
	    XSetForeground (dsp, ng_gc, gcv.foreground);
	    y += 3*UPLT/2;

	    /* draw line .. beware of wrap */
	    if (risex < setx) {
		XPSDrawLine (dsp, ng_pm, ng_gc, risex, y, setx, y);
	    } else {
		XPSDrawLine (dsp, ng_pm, ng_gc, 0, y, setx, y);
		XPSDrawLine (dsp, ng_pm, ng_gc, risex, y, ng_w, y);
	    }
	}

	/* hours and tick marks */
	XSetForeground (dsp, ng_gc, WhitePixel (dsp, DefaultScreen(dsp)));
	XSetFont (dsp, ng_gc, ng_lfs->fid);
	for (i = 0; i < 24; i++) {
	    int ieven = !(i%2);

	    rs.rs_risetm = (i+tz)/24.+.5;	/* local as an mjd */
	    rs.rs_settm = i/24.+.5;		/* utc as an mjd */
	    rs_x (np, &rs, &x1, &x2);

	    if (x1 > TMLT && x1 < ng_w-TMLT) {
		int boty, thick;

                y = skyh;
		if (ieven) {
		    thick = TMLT;
		    boty = skyh + 4*fh/6;
		} else {
		    thick = 0;
		    boty = skyh + 3*fh/6;
		}
		XSetLineAttributes (dsp, ng_gc, thick, LineSolid, CapButt,
								JoinMiter);
		XPSDrawLine (dsp, ng_pm, ng_gc, x1, y, x1, boty);
	    }

	    n = sprintf (buf, "%d", i);
	    hw = (fw*n)/2;
	    if (ieven && x1-hw > lmarg && x1+hw < ng_w) {
		y = skyh + 2*fh-2;
		XPSDrawString (dsp, ng_pm, ng_gc, x1-hw, y, buf, n);
	    }
	    if (ieven && x2-hw > lmarg && x2+hw < ng_w) {
		y = skyh + 3*fh-2;
		XPSDrawString (dsp, ng_pm, ng_gc, x2-hw, y, buf, n);
	    }

	}

	/* line to make it an "axis" */
	XSetLineAttributes (dsp, ng_gc, TMLT, LineSolid, CapButt, JoinMiter);
	y = skyh;
	XPSDrawLine (dsp, ng_pm, ng_gc, 0, y, ng_w, y);

	/* zones at left */
	y = skyh + 2*fh-2;
	n = strlen (tznm);
	XPSDrawString (dsp, ng_pm, ng_gc, 2, y, tznm, n);
	y = skyh + 3*fh-2;
	n = strlen (utclabel);
	XPSDrawString (dsp, ng_pm, ng_gc, 2, y, utclabel, n);

	/* local dates -- today and the other */
	XSetForeground (dsp, ng_gc, WhitePixel (dsp, DefaultScreen(dsp)));
	y = skyh + 4*fh-2;
	if (nowx < midx) {
	    fs_date (buf, mjd_day(mjd-tz/24.));
	    fs_date (buf2, mjd_day(mjd-tz/24.+1));
	    leftday = 0;
	    rightday = 1;
	} else {
	    fs_date (buf, mjd_day(mjd-tz/24.-1));
	    fs_date (buf2, mjd_day(mjd-tz/24.));
	    leftday = -1;
	    rightday = 0;
	}
	n = strlen (buf);
	XPSDrawString (dsp, ng_pm, ng_gc, (midx-fw*n)/2, y, buf, n);
	n = strlen (buf2);
	XPSDrawString (dsp, ng_pm, ng_gc, (ng_w+midx-fw*n)/2, y, buf2, n);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: glance.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
