/* code code to manage the stuff on the sky view display.
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <time.h>

#if defined(__STDC__)
#include <stdlib.h>
#include <limits.h>
#else
#define	SHRT_MAX	((((1<<(8*sizeof(short)-2)) - 1) << 1) + 1)
#define	SHRT_MIN	(-SHRT_MAX+1)
#endif 

#if defined(_POSIX_SOURCE)
#include <unistd.h>
#else
extern int close();
extern int read();
#endif

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/DrawingA.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/RowColumn.h>
#include <Xm/ToggleB.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/Scale.h>
#include <Xm/Separator.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "fits.h"
#include "trails.h"
#include "skyeyep.h"
#include "skyhist.h"
#include "skylist.h"
#include "preferences.h"
#include "ps.h"
#include "dm.h"
#include "skytoolbar.h"

extern Widget toplevel_w;
extern XtAppContext xe_app;
#define XtD XtDisplay(toplevel_w)
extern Colormap xe_cm;

extern Now *mm_get_now P_((void));
extern Obj *db_basic P_((int id));
extern Obj *db_scan P_((DBScan *sp));
extern FILE *fopenh P_((char *name, char *how));
extern FImage *sf_getFImage P_((void));
extern XFontStruct * getXResFont P_((char *rn));
extern Pixmap sf_getPixmap P_((void));
extern char *cns_name P_((int id));
extern char *getPrivateDir P_((void));
extern char *getShareDir P_((void));
extern char *getXRes P_((char *name, char *def));
extern char *mm_getsite P_((void));
extern char *msa_atlas P_((double ra, double dec));
extern char *syserrstr P_((void));
extern char *um_atlas P_((double ra, double dec));
extern double hznAlt P_((double az));
extern int RADec2xy P_((FImage *fip, double ra, double dec, double *xp,
    double *yp));
extern int cns_figure P_((int id, double e, double ra[], double dec[],
    int dcodes[]));
extern int cns_list P_((double ra, double dec, double e, double rad,
    int ids[]));
extern int cns_edges P_((double e, double **era0, double **edec0,
    double **era1, double **edec1));
extern int cns_pick P_((double ra, double dec, double e));
extern int f_ison P_((void));
extern int fs_fetch P_((Now *np, double ra, double dec, double fov,
    double mag, ObjF **opp));
extern int fs_pmon P_((void));
extern void fs_manage P_((void));
extern int get_color_resource P_((Widget w, char *cname, Pixel *p));
extern int isUp P_((Widget shell));
extern int lc P_((int cx, int cy, int cw, int x1, int y1, int x2, int y2,
    int *sx1, int *sy1, int *sx2, int *sy2));
extern int magdiam P_((int fmag, int magstp, double scale, double mag,
    double size));
extern int openh P_((char *name, int flags, ...));
extern int sf_ison P_((void));
extern int skyloc_fifo P_((Obj *op));
extern int svf_filter_ok P_((Obj *op));
extern int svf_ismanaged P_((void));
extern int xy2RADec P_((FImage *fip, double x, double y, double *rap,
    double *decp));
extern void XCheck P_((XtAppContext app));
extern void av_load P_((Obj *op));
extern void db_scaninit P_((DBScan *sp, int mask, ObjF *op, int nop));
extern void db_update P_((Obj *op));
extern void dm_riset P_((Now *np, Obj *op, RiseSet *rsp));
extern void dm_newobj P_((int dbidx));
extern void dm_update P_((Now *np, int how_much));
extern void f_showit P_((Widget w, char *s));
extern void fs_dm_angle P_((char out[], double a));
extern void fs_date P_((char out[], double jd));
extern void fs_prdec P_((char out[], double jd));
extern void fs_mtime P_((char out[], double t));
extern void fs_pangle P_((char out[], double a));
extern void fs_ra P_((char out[], double ra));
extern void fs_time P_((char out[], double t));
extern void get_something P_((Widget w, char *resource, XtArgVal value));
extern void get_views_font P_((Display *dsp, XFontStruct **fspp));
extern void get_tracking_font P_((Display *dsp, XFontStruct **fspp));
extern void get_xmstring P_((Widget w, char *resource, char **txtp));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void hzn_manage P_((void));
extern void loadGreek P_((Display *dsp, Drawable win, GC *greekgcp,
    XFontStruct **greekfspp));
extern void mm_setepoch P_((double yr));
extern void obj_pickgc P_((Obj *op, Widget w, GC *gcp));
extern void obj_set P_((Obj *op, int dbidx));
extern void se_add P_((double ra, double dec, double alt, double az));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sf_doGlass P_((Display *dsp, Window win, int b1p, int m1,
    int lr, int tb, int wx, int wy));
extern void sf_getName P_((char *filename, char *objname));
extern void sf_manage P_((void));
extern void sf_newPixmap P_((Widget w, int lr, int tb));
extern void sf_off P_((void));
extern void sf_ps P_((Window w, int lr, int tb));
extern void sf_unmanage P_((void));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void sv_all P_((Now *np));
extern void sv_draw_obj P_((Display *dsp, Drawable win, GC gc, Obj *op, int x,
    int y, int diam, int dotsonly));
extern void sv_update P_((Now *np, int how_much));
extern void sv_loadfs P_((int force));
extern void svf_automag P_((double fov));
extern void svf_create P_((Widget shell_w));
extern void svf_getmaglimits P_((int *stmagp, int *ssmagp, int *dsmagp,
    int *magstpp));
extern void svf_gettables P_((char tt[NOBJTYPES], char ct[NCLASSES]));
extern void svf_manage P_((void));
extern void svf_setmaglimits P_((int stmag, int ssmag, int dsmag, int magstp));
extern void svf_settables P_((char tt[NOBJTYPES], char ct[NCLASSES]));
extern void svf_unmanage P_((void));
extern void timestamp P_((Now *np, Widget w));
extern void watch_cursor P_((int want));
extern void xe_msg P_((char *msg, int app_modal));
extern void wtip P_((Widget w, char *tip));
extern void zero_mem P_((void *loc, unsigned len));

/* we keep a linked-list of TrailObj's we want trails for. these in turn
 *   contain an array of TSky's for each location in the trail.
 * objects are added and controlled from the popup; we only ever actually
 *   discard an object if the whole db changes or it is off when we get an
 *   update.
 */
typedef struct {
    unsigned char flags;/* OBJF_* flags (shared with Obj->o_flags) */
    TrTS trts;		/* mjd of o and whether to draw timestamp */
    Obj o;		/* copy of the object at ts_mjd */
} TSky;
typedef struct _trailObj {
    struct _trailObj *ntop;	/* pointer to next, or NULL */
    int on;		/* turn trail on/off (discarded if off during update) */
    TrState trs;	/* general details of the trail setup */
    Obj *op;		/* pointer to actual db object being trailed */
    int nsky;		/* number of items in use within sky[] */
    TSky sky[1];	/* array of Objs (gotta love C :-) */
} TrailObj;

static double x_angle P_((Obj *op));
static void sv_copy_sky P_((void));
static void sv_create_svshell P_((void));
static void sv_create_find P_((Widget parent));
static void sv_create_options P_((void));
static void sv_create_op_lbl P_((Widget rc_w));
static void sv_create_grid_option P_((Widget f_w));
static void sv_closeopdialog_cb P_((Widget w, XtPointer client,XtPointer call));
static void sv_opdialog_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_popdown_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_helpon_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_aa_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_cyl_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_filter_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_fits_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_gt_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_grid_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_brs_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_gridtf_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_in_fifo_cb P_((Widget w, XtPointer client, XtPointer call));
static void skyinfifo_cb P_((XtPointer client, int *fdp, XtInputId *idp));
static void sv_loc_fifo_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_list_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_print_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_print P_((void));
static void sv_ps_annotate P_((Now *np));
static void sv_track_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_set_fov P_((double fov));
static void sv_da_exp_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_resize P_((int neww, int newh, int aspectok));
static void sv_da_input_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_da_motion_eh P_((Widget w, XtPointer client, XEvent *ev,
    Boolean *continue_to_dispatch));
static void sv_scale_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_lbln_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_lblm_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_option_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_eyep_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_altdecsc_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_fovsc_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_finding_cb P_((Widget wid, XtPointer client, XtPointer call));
static void sv_find_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_popup P_((XEvent *ev, Obj *op, TSky *tsp));
static void sv_riset P_((Obj *op));
static void sv_create_popup P_((void));
static void sv_magscale P_((Display *dsp, Window win));
static void sv_create_zoomcascade P_((Widget pd_w));
static void sv_create_objcascade P_((Widget pd_w));
static void sv_create_labellr P_((Widget pd_w));
static void sv_pu_activate_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_pu_zoom_cb P_((Widget w, XtPointer client, XtPointer call));
static void sv_pu_trail_cb P_((Widget wid, XtPointer client, XtPointer call));
static void sv_pu_label_cb P_((Widget wid, XtPointer client, XtPointer call));
static void sv_pu_track_cb P_((Widget wid, XtPointer client, XtPointer call));
static void sv_telpd_cb P_((Widget wid, XtPointer client, XtPointer call));
static void sv_dtelpd_cb P_((Widget wid, XtPointer client, XtPointer call));
static void sv_addtelpd P_((Obj *op));
static void sv_read_scale P_((int which));
static void sv_set_scale P_((int which, int nofits, int cutzoom));
static void sv_draw_track_coords P_((int new, int sep, double altdec,
    double azra));
static void sv_erase_track_coords P_((void));
static int sv_mark P_((Obj *op, int in_center, int out_center, int mark,
    int below_msg, int use_window, double newfov));
static void sv_set_dashed_cnsgc P_((Display *dsp, int dashed));
static void svtb_sync P_((void));
static void tobj_rmoff P_((void));
static void tobj_rmobj P_((Obj *op));
static TrailObj *tobj_addobj P_((Obj *op, int nsky));
static void tobj_newdb P_((void));
static TrailObj *tobj_find P_((Obj *op));
static void tobj_display_all P_((void));
static void aatwarn_msg P_((void));
static int sv_mktrail P_((TrTS ts[], TrState *statep, XtPointer client));
static int sv_dbobjloc P_((Obj *op, int *xp, int *yp));
static int sv_trailobjloc P_((TSky *tsp, int *xp, int *yp));
static int sv_precheck P_((Obj *op));
static int sv_onscrn P_((Obj *op));
static int sv_loc P_((double altdec, double azra, int *xp, int *yp));
static int sv_unloc P_((int x, int y, double *altdecp, double *azrap));
static void sv_other P_((double altdec, double azra, int aa,
    double *altdecp, double *azrap));
static void sv_fullwhere P_((Now *np, double altdec, double azra, int aa,
    double *altp, double *azp, double *rap, double *decp));
static void hznPoly P_((Display *dsp, Window win, GC gc, XPoint xpts[],
    int npts));
static void draw_hznmap P_((Now *np, Display *dsp, Window win, GC gc));
static void draw_ecliptic P_((Now *np, Display *dsp, Window win, GC gc));
static void draw_umbra P_((Now *np, Display *dsp, Window win, GC gc));
static void draw_equator P_((Now *np, Display *dsp, Window win, GC gc));
static void draw_galactic P_((Now *np, Display *dsp, Window win, GC gc));
static int split_line P_((Display *dsp, Window win, GC gc, int x1, int y1,
    int x2, int y2));
static int split_lines P_((Display *dsp, Window win, GC gc,XPoint xp[],int np));
static int split_segs P_((Display *dsp, Window win,GC gc,XSegment sp[],int ns));
static void split_wrap P_((XSegment *sp, int *xwp, int *ywp));
static void draw_cnsbounds P_((Now *np, Display *dsp, Window win));
static void draw_cns P_((Now *np, Display *dsp, Window win));
static void draw_cnsname P_(( Display *dsp, Window win, int conid,
    int minx, int miny, int maxx, int maxy));
static int draw_grid_label P_((Display *dsp, Window win, GC gc, int x, int y,
    int dx, int dy, int samesys, int arlabel, double dv, double dh));
static void draw_grid P_((Display *dsp, Window win, GC gc));
static void draw_allobjs P_((Display *dsp, Drawable win));
static void draw_label P_((Window win, GC gc, Obj *op, int flags, int x, int y,
    int d));
static int chk_greeklabel P_((char name[], int *glp, char *gcodep));
static void gridStepLabel P_((void));
static void draw_eyep P_((Display *dsp, Window win, GC gc));
static int objdiam P_((Obj *op));
static void sv_mk_gcs P_((void));
static void sky_setup_infifo P_((void));
static int segisvis P_((int x1, int y1, int x2, int y2,
    int *cx1, int *cy1, int *cx2, int *cy2));
static void chkFS_to P_((XtPointer client, XtIntervalId *id));
static int hznOpOk P_((Obj *op));

#define	MAXGRID		20	/* max grid lines each dimension */
#define	GMARG		20	/* min margin of grid label from edge */
#define	NGSEGS		3	/* piecewise segments per grid arc */
#define	NHZNPTS		360	/* number of horizon map steps */
#define	EQ_NON		3	/* dots on in equator line pattern */
#define	EQ_NOFF		6	/* dots off in equator line pattern */
#define	ECL_NON		2	/* dots on in ecliptic line pattern */
#define	ECL_NOFF	2	/* dots off in ecliptic line pattern */
#define	GAL_NON		1	/* dots on in galaxy plane line pattern */
#define	GAL_NOFF	4	/* dots off in galaxy plane line pattern */
#define	GAL_W		5	/* half-width of galactic pole X */
#define	MARKR		20	/* pointer marker half-width, pixels */
#define	TICKLEN		2	/* length of trail tickmarks, pixels */
#define	MINEPR		5	/* minimum eyepiece radius we'll draw, pixels */
#define	MIND		5	/* min diam for non-star non-planet symbols*/
#define MAXBRLBLS       100	/* max number of brightest objects we label */
#define	MAGBOXN		6	/* number of mag steps to show in box */
#define	FOV_STEP	5	/* FOV min and step size, arc mins */
#define	MAXPMOK		30	/* min days for fs reload (Barnard's ~ 1"/mo) */
#define	FSTO		1000	/* field star load timer, ms */
#define	MAXFSFOV	15.0	/* max fov we ever load field stars, degs */
#define	ASR		5	/* anti-solar marker radius, pixels */
#define	TBORD		4	/* tracking border from edge, pixels */
#define	UDLRF		8	/* up/dn/lf/rt toolbar motion fraction */
#define	LOSTBSZ		10	/* Label option TB and Scale size */

static char telAnon[] = "TelAnon";  /* name used for anon objects (pure loc) */

Widget svshell_w;		/* main sky view shell */
static Widget svda_w;		/* sky view drawing area */
static Widget svops_w;		/* main options dialog */
static Widget fov_w, altdec_w, azra_w;	/* scale widgets ... */
static Widget fovl_w, altdecl_w, azral_w; /*   ... and their labels */
static Widget hgrid_w, vgrid_w;	/* h and v grid spacing TF */
static Widget hgridl_w, vgridl_w;	/* h and v grid spacing labels */
static Widget grid_w;		/* want grid TB */
static Widget autogrid_w;	/* want auto grid spacing TB */
static Widget gridlbl_w;	/* want grid labels TB */
static Widget aagrid_w;		/* want AA grid TB */
static Widget naagrid_w;	/* notwant AA grid TB */
static Widget aa_w, rad_w;	/* altaz/radec toggle buttons */
static Widget sph_w, cyl_w;	/* spherical/cylindrical toggle buttons */
static Widget fliplr_w, fliptb_w;	/* orientation TBs */
static Widget conn_w, conf_w;	/* constellation name and fig TBs */
static Widget conb_w, cona_w;	/* constellation boundry and abbr TBs */
static Widget justd_w;		/* justdots TB */
static Widget eclip_w, galac_w, eq_w;	/* eclip, galactic, equator TBs */
static Widget dt_w;		/* the date/time stamp label widget */
static Widget find_w[3];	/* PBs for "local" objxyz */
static Pixmap sv_pm;		/* off-screen pixmap we *really* draw */
static XFontStruct *sv_tf;	/* font for the tracking coord display*/
static XFontStruct *sv_pf;	/* font for all object labels */
static XFontStruct *sv_gf;	/* greek font */
static XFontStruct *sv_cf;	/* constellation names font */
static XFontStruct *sv_rf;	/* grid font */
static GC sv_ggc;		/* greek gc */
static Pixmap trk_pm;		/* used to draw track info smoothly */
static unsigned int trk_w, trk_h;	/* size of trk_pm, iff trk_pm */

/* pixels and GCs
 */
static Pixel fg_p;		/* fg color */
static Pixel bg_p;		/* background color */
static Pixel cnsfig_p;		/* constellation figures color */
static Pixel cnsbnd_p;		/* constellation boundaries color */
static Pixel cnsnam_p;		/* constellation name color */
static Pixel sky_p;		/* sky background color */
static Pixel hzn_p;		/* horizon profile color */
static Pixel bgfg_p;		/* known visible over bg_p */
static Pixel grid_p;		/* grid color */
static Pixel eq_p;		/* equator color */
static GC sv_gc;		/* the default GC */
static GC sv_cnsgc;		/* the GC for constellations */
static GC sv_strgc;		/* gc for use in drawing text */
static GC zm_xorgc;		/* gc for zoom box */

static int aa_mode = -1;	/* 1 for alt/az or 0 for ra/dec */
static int cyl_proj = -1;	/* 1 for cylindrical proj or 0 for spherical */
static double sv_altdec;	/* view center alt or dec, rads */
static double sv_azra;		/* view center az or ra, rads */
static double sv_vfov;		/* vertical field of view, rads */
static double sv_hfov;		/* horizontal field of view, rads */
static double sv_dfov;		/* diagonal field of view, rads */
static unsigned int sv_w, sv_h;	/* size of svda_w, pixels */
static int justdots;		/* set when only want to use dots on the map */
static int want_livedrag;	/* set when want live dragging */
static int flip_lr;		/* set when want to flip left/right */
static int flip_tb;		/* set when want to flip top/bottom */
static int want_ecliptic;	/* set when want to see the ecliptic */
static int want_eq;		/* set when want to see the equator */
static int want_galactic;	/* set when want to see galactic poles and eq*/
static int want_hznmap;		/* set when want to see horizon map */
static int want_conbounds;	/* set when want to see the constel boundaries*/
static int want_configures;	/* set when want to see the constel figures */
static int want_connames;	/* set when want to see the constel names */
static int want_conabbr;	/* set when want to see the constel abbrevs */
static int want_eyep;		/* set when want to see eyepieces */
static int want_grid;		/* set when we want to draw the coord grid */
static int want_aagrid;		/* set when we want an AA grid */
static int want_gridlbl;	/* set when we want grid labeling */
static int want_autogrid;	/* set when we want auto coord spacing */
static int want_fs;		/* set when we want to see field stars */
static int want_magscale;	/* set when we want to see the mag scale */
static int want_automag;	/* set when we want to automatically set mags */
static int user_automagoff;	/* set when user manually turns off automag */
static int anytrails;		/* set to add postscript trail time comment */
static int sf_nup;		/* set if current image has north up */
static int sf_elf;		/* set if current image has east left */

static ObjF *fldstars;		/* malloced list of field stars, or NULL */
static int nfldstars;		/* number of entries in fldstars[] */

static TrailObj *trailobj;	/* head of a malloced linked-list -- 0 when
				 * empty
				 */
static TrState trstate = {	/* trail setup state */
    TRLR_FL, TRI_DAY, TRF_DATE, TRR_DAY, TRO_PATHL, TRS_MEDIUM, 10
};
static Obj *track_op;		/* object to track, when not NULL */
static Widget tracktb_w;	/* toggle button indicating tracking is on */
static int sv_ournewobj;	/* used to inhibit useless redrawing */
static int infifowason;		/* set when infifo was on before */
static Widget wanteyep_w;	/* TB for whether to show eyepieces */
static Widget wantfs_w;		/* TB for whether to show field stars */
static Widget wantmag_w;	/* TB for whether to show mag scale */
static Widget wantamag_w;	/* TB for whether to automatically set mag */
static Widget hznmap_w;		/* TB for whether to show horizon map */
static Widget ttbar_w;		/* top toolbar RC */
static Widget ltbar_w;		/* left toolbar RC */
static Widget telpd_w;		/* Telescope pulldown, for adding histories */

static int lbl_lst;		/* star name/mag label options */
static int lbl_lss;		/* sol sys name/mag label options */
static int lbl_lds;		/* deep sky name/mag label options */
static Widget lbl_nst_w;	/* TB for star name label */
static Widget lbl_mst_w;	/* TB for star mag label */
static Widget lbl_nss_w;	/* TB for sol sys name label */
static Widget lbl_mss_w;	/* TB for sol sys mag label */
static Widget lbl_nds_w;	/* TB for deep sky name label */
static Widget lbl_mds_w;	/* TB for deep sky mag label */
static Widget lbl_bst_w;	/* Scale for brightest stars */
static Widget lbl_bss_w;	/* Scale for brightest sol sys */
static Widget lbl_bds_w;	/* Scale for brightest deep sky */

static int want_infifo;		/* set to accept aiming coord on fifo */
static Widget infifo_w;		/* toggle button controlling in fifo */
static int want_locfifo;	/* set to enable sending menu coord to fifo */
static Widget locfifo_w;	/* toggle button controlling loc fifo */
static unsigned long eyep_p;	/* eyepiece color */
static EyePiece *largeyepr;	/* largest eyepiece printed, for print label */
static int neyepr;		/* number of eyepieces printed, " */

/* info for the popup widget.
 * we make one once and keep reusing it -- seems to be a bit faster that way.
 * but see sv_popup() for all the work to get it customized for each object.
 */
typedef struct {
    Widget pu_w;		/* the overall PopupMenu */
    Widget name_w;		/* label for object name */
    Widget desc_w;		/* label for object description */
    Widget spect_w;		/* label for object spectral class */
    Widget size_w;		/* label for object size */
    Widget pa_w;		/* label for object position angle */
    Widget ud_w;		/* label for object UT date */
    Widget ut_w;		/* label for object UT time */
    Widget rise_w;      	/* label for object UT rise time */
    Widget trans_w;     	/* label for object UT transit time */
    Widget transalt_w;     	/* label for object UT transit altitude */
    Widget set_w;       	/* label for object UT set time */
    Widget ra_w;		/* label for object RA */
    Widget dec_w;		/* label for object Dec */
    Widget alt_w;		/* label for object Alt */
    Widget az_w;		/* label for object Az */
    Widget mag_w;		/* label for object mag */
    Widget locfifo_w;		/* PB to send coords to LOCFIFO */
    Widget av_w;		/* PB to load nearest AAVSO */
    Widget label_w;		/* CB to select left/label */
    Widget llabel_w;		/* TB to select left label be drawn */
    Widget rlabel_w;		/* TB to select right label be drawn */
    Widget assign_w;		/* CB to make this object xyz */
    Widget dv_w;		/* PB to make this object show on the data tbl*/
    Widget track_w;		/* TB to track this object */
    Widget trail_w;		/* TB to let existing trail be turned on/off */
    Widget newtrail_w;		/* PB to create new trail */
    Obj *op;			/* real database pointer, or possibly svobj */
    TSky *tsp;			/* used iff we are displaying a trailed object*/
} Popup;
static Popup pu;
static Obj svobj;		/* used to point when far from any real object*/

/* popup button activate codes */
enum {
    AIM, EYEPIECE, LOCFIFO, LOADAV, ADDP2DV, MK_OBJX, MK_OBJY,
    MK_OBJZ, NEWTRAIL
};

enum SCALES {			/* scale changed codes */
    FOV_S, ALTDEC_S, AZRA_S
};

/* zoom support */
typedef struct {
    int x0, y0, x1, y1;		/* aoi corners */
    double ad;			/* alt/dec */
    double ar;			/* az/ra */
    double fov;			/* fov */
} ZM_Undo;			/* info about each undo level */
static ZM_Undo *zm_undo;	/* malloced list of zoom undo info */
static int zm_nundo;		/* n valid entries in zm_undo */
static int zm_cundo;		/* current depth on zm_undo */
static int zm_x0, zm_y0;	/* one working aoi corner */
static int zm_x1, zm_y1;	/* other working corner */
static void zm_noundo P_((void));
static int zm_addundo P_((void));
static void zm_installundo P_((void));
static void zm_cutundo P_((void));
static void zm_draw P_((void));

static char skyocategory[] = "Sky View -- Options";	/* Save category */
char skycategory[] = "Sky View";			/* Save category */

/* used for creating the Options menu */
typedef struct {
    char *label;	/* toggle button label */
    char oneofmany;	/* 1 for ONE_OF_MANY, 0 for N_OF_MANY */
    char *name;		/* instance name */
    int *flagp;		/* address of flag it controls */
    Widget *wp;		/* controlling widget, or NULL */
    char *tip;		/* tip text */
} ViewOpt;

static XtIntervalId fs_to;	/* set while checking field stars */

/* bring up Sky view shell, creating if first time */
void
sv_manage ()
{
	if (!svshell_w) {
	    /* one-time-only stuff */

	    /* create the main form, the filter and history list */
	    sv_create_svshell();

	    /* make the gcs and set up pixels */
	    sv_mk_gcs();
	}

        XtPopup (svshell_w, XtGrabNone);
	set_something (svshell_w, XmNiconic, (XtArgVal)False);
	if (fs_to != 0) {
	    XtRemoveTimeOut (fs_to);
	    fs_to = 0;
	}
	fs_to = XtAppAddTimeOut (xe_app, FSTO, chkFS_to, 0);
	/* rely on expose to cause a fresh update */

	if (infifowason) {
	    want_infifo = 1;
	    sky_setup_infifo ();
	}
}

/* called when basic resources change.
 * rebuild and redraw.
 */
void
sv_newres()
{
	if (!svshell_w)
	    return;
	sv_mk_gcs();
	svtb_newpm(ttbar_w);
	sv_update (mm_get_now(), 1);
}

/* return true if sky view is up now and ok to draw to, else 0 */
int
sv_ison()
{
	return(isUp(svshell_w) && sv_pm);
}

/* called when we are to update our view.
 * don't bother if our own sv_ournewobj flag is set, if we are not managed or
 *   if fields are off.
 * discard any trails that have been turned off.
 * reaim if we are tracking an object.
 */
/* ARGSUSED */
void
sv_update (np, how_much)
Now *np;
int how_much;
{
	if (sv_ournewobj)
	    return;

	if (!sv_ison() || !f_ison())
	    return;

	/* remove trails not on now */
	tobj_rmoff();

	if (track_op) {
	    db_update (track_op);
	    if (sv_mark (track_op, 1, 1, 1, 1, 0, 0.0) < 0)
		sv_all(np);	/* still need it even if mark failed */
	} else 
	    sv_all(np);
}

/* called when a user-defined object has changed.
 * take it off the trailobj list, if it's there (it's ok if it's not).
 * also make sure we aren't tracking it if it has become undefined.
 * then since we rely on knowing our update will be called we need do nothing
 *   more to redisplay without the object.
 */
void
sv_newobj(dbidx)
int dbidx;		/* OBJXYZ */
{
	Obj *op = db_basic(dbidx);

	tobj_rmobj (op);

	if (track_op == op && op->o_type == UNDEFOBJ) {
	    XmToggleButtonSetState (tracktb_w, False, False);
	    XtSetSensitive (tracktb_w, False);
	    track_op = NULL;
	}
}

/* called when the db (beyond NOBJ) has changed.
 * if it was appended to we can just redraw else it was reduced and we need to
 * discard any trails we are keeping for objects that no longer exist and
 * make sure no pending trail creations can come in and do something.
 */
void
sv_newdb(appended)
int appended;
{
	if (!appended) {
	    tobj_newdb();
	    pu.op = NULL;	/* effectively disables sv_mktrail() */
	}

	sv_update (mm_get_now(), 1);
}

/* called by skyfits to engage a new fits image */
void
sv_newfits ()
{
	double ra, dec, ra1, dec1, ra2, dec2, fov;
	double equinox;
	FImage *fip;

	watch_cursor(1);

	/* fetch the fits descriptor */
	fip = sf_getFImage();
	if (!fip) {
	    printf ("newfits Bug! FITS file disappeared in newfits()!\n");
	    exit(1);
	}

	/* get size first .. we already know there are WCS fields */
	(void) xy2RADec (fip, fip->sw/2.0, fip->sh/2.0, &ra, &dec);

	/* find image width -- not critical to be exact */
	(void) xy2RADec (fip, 0.0, fip->sh/2.0, &ra1, &dec1);
	(void) xy2RADec (fip, fip->sw-1.0, fip->sh/2.0, &ra2, &dec2);
	fov = acos (sin(dec1)*sin(dec2) + cos(dec1)*cos(dec2)*cos(ra2-ra1));

	/* find which way is nominally up */
	(void) xy2RADec (fip, 0.0, 0.0, &ra2, &dec2);
	sf_nup = (dec2 > dec1);

	/* find which way is nominally left */
	(void) xy2RADec (fip, fip->sw/2.0, fip->sh/2.0, &ra2, &dec2);
	sf_elf = (ra1 > ra2);

	/* aa_mode and sph_proj.
	 * N.B. do this before calling sv_aim_scales().
	 */
	if (aa_mode) {
	    aa_mode = 0;
	    XmToggleButtonSetState (aa_w, aa_mode, False);
	    XmToggleButtonSetState (rad_w, !aa_mode, False);
	}
	if (cyl_proj) {
	    cyl_proj = 0;
	    XmToggleButtonSetState (cyl_w, cyl_proj, False);
	    XmToggleButtonSetState (sph_w, !cyl_proj, False);
	    svtb_updateCyl (cyl_proj);
	}

	/* set aim scales */
	sv_altdec = dec;
	sv_set_scale(ALTDEC_S, 0, 1);
	sv_azra = ra;
	sv_set_scale(AZRA_S, 0, 1);

	/* set fov scale */
	sv_w = fip->sw;
	sv_h = fip->sh;
	sv_set_fov (fov);
	sv_set_scale(FOV_S, 0, 1);

	/* build a pixmap to match our current orientation */
	sf_newPixmap (svda_w, sf_elf ? flip_lr : !flip_lr,
						sf_nup ? flip_tb : !flip_tb);

	/* force epoch to match EQUINOX field */
	if (getRealFITS (fip, "EQUINOX", &equinox) == 0)
	    mm_setepoch(equinox);

	/* new size now */
	sv_resize (fip->sw, fip->sh, 0);

	watch_cursor(0);
}

/* called to reinstate a SvHistory */
void
svh_goto (hp)
SvHistory *hp;
{
	/* restore aa_mode.
	 * N.B. do this before calling sv_set_scale().
	 */
	if (hp->aa_mode != aa_mode) {
	    aa_mode = hp->aa_mode;
	    XmToggleButtonSetState (aa_w, aa_mode, False);
	    XmToggleButtonSetState (rad_w, !aa_mode, False);
	}

	/* restore aim scales */
	sv_altdec = hp->altdec;
	sv_set_scale (ALTDEC_S, 1, 1);
	sv_azra = hp->azra;
	sv_set_scale (AZRA_S, 1, 1);

	/* restore fov scale */
	sv_set_fov (hp->fov);
	sv_set_scale(FOV_S, 1, 1);

	/* restore flip orientation */
	XmToggleButtonSetState (fliplr_w, flip_lr = hp->flip_lr, False);
	XmToggleButtonSetState (fliptb_w, flip_tb = hp->flip_tb, False);

	/* restore mag limits and filter settings */
	svf_setmaglimits (hp->stmag, hp->ssmag, hp->dsmag, hp->magstp);
	svf_settables (hp->type_table, hp->fclass_table);

	/* restore the grid options */
	XmToggleButtonSetState (grid_w, want_grid = hp->grid, False);
	XmToggleButtonSetState (autogrid_w, want_autogrid = hp->autogrid,False);
	XmToggleButtonSetState (aagrid_w, want_aagrid = hp->aagrid, False);
	XmToggleButtonSetState (naagrid_w, !want_aagrid, False);
	XmToggleButtonSetState (gridlbl_w, want_gridlbl = hp->gridlbl, False);
	XmTextFieldSetString (vgrid_w, hp->vgrid);
	XmTextFieldSetString (hgrid_w, hp->hgrid);
	gridStepLabel();

	/* restore labeling options */
	lbl_lst = hp->lbl_lst;
	XmToggleButtonSetState(lbl_nst_w, !!(lbl_lst & OBJF_NLABEL), False);
	XmToggleButtonSetState(lbl_mst_w, !!(lbl_lst & OBJF_MLABEL), False);
	lbl_lss = hp->lbl_lss;
	XmToggleButtonSetState(lbl_nss_w, !!(lbl_lss & OBJF_NLABEL), False);
	XmToggleButtonSetState(lbl_mss_w, !!(lbl_lss & OBJF_MLABEL), FALSE);
	lbl_lds = hp->lbl_lds;
	XmToggleButtonSetState(lbl_nds_w, !!(lbl_lds & OBJF_NLABEL), False);
	XmToggleButtonSetState(lbl_mds_w, !!(lbl_lds & OBJF_MLABEL), False);
	XmScaleSetValue (lbl_bst_w, hp->lbl_bst);
	XmScaleSetValue (lbl_bss_w, hp->lbl_bss);
	XmScaleSetValue (lbl_bds_w, hp->lbl_bds);

	/* restore other misc options */
	XmToggleButtonSetState (justd_w, justdots = hp->justd, False);
	XmToggleButtonSetState (eclip_w, want_ecliptic = hp->eclip, False);
	XmToggleButtonSetState (eq_w, want_eq = hp->eq, False);
	XmToggleButtonSetState (galac_w, want_galactic = hp->galac, False);
	XmToggleButtonSetState (hznmap_w, want_hznmap = hp->hznmap, False);
	XmToggleButtonSetState (conb_w, want_conbounds = hp->conb, False);
	XmToggleButtonSetState (conf_w, want_configures = hp->conf, False);
	XmToggleButtonSetState (conn_w, want_connames = hp->conn, False);
	XmToggleButtonSetState (cona_w, want_conabbr = hp->cona, False);
	XmToggleButtonSetState (wanteyep_w, want_eyep = hp->eyep, False);
	XmToggleButtonSetState (wantmag_w, want_magscale = hp->magscale, False);
	XmToggleButtonSetState (wantamag_w, want_automag = hp->automag, False);
	XmToggleButtonSetState (cyl_w, cyl_proj = hp->cyl_proj, False);
	XmToggleButtonSetState (sph_w, !hp->cyl_proj, False);

	/* sync the toolbar to match */
	svtb_sync();

	/* don't even try to get back a FITS image */
	sf_off();

	/* resize will do it all */
	sv_resize (hp->winw, hp->winh, 0);
}

/* called by the history mechanism when it needs to know the current settings.
 */
void
svh_get (hp)
SvHistory *hp;
{
	Dimension w, h;
	char *str;
	Arg args[10];
	int n;

	hp->fov = sv_vfov;
	hp->altdec = sv_altdec;
	hp->azra = sv_azra;
	hp->aa_mode = aa_mode;
	hp->flip_lr = flip_lr;
	hp->flip_tb = flip_tb;
	svf_getmaglimits (&hp->stmag, &hp->ssmag, &hp->dsmag, &hp->magstp);
	hp->justd = justdots;
	hp->eclip = want_ecliptic;
	hp->eq = want_eq;
	hp->galac = want_galactic;
	hp->hznmap = want_hznmap;
	hp->conb = want_conbounds;
	hp->conf = want_configures;
	hp->conn = want_connames;
	hp->cona = want_conabbr;
	hp->eyep = want_eyep;
	hp->magscale = want_magscale;
	hp->automag = want_automag;
	hp->cyl_proj = cyl_proj;

	hp->grid = want_grid;
	hp->autogrid = want_autogrid;
	hp->aagrid = want_aagrid;
	hp->gridlbl = want_gridlbl;
	str = XmTextFieldGetString (vgrid_w);
	(void) strcpy (hp->vgrid, str);
	XtFree (str);
	str = XmTextFieldGetString (hgrid_w);
	(void) strcpy (hp->hgrid, str);
	XtFree (str);

	hp->lbl_lst = lbl_lst;
	hp->lbl_lss = lbl_lss;
	hp->lbl_lds = lbl_lds;
	XmScaleGetValue (lbl_bst_w, &hp->lbl_bst);
	XmScaleGetValue (lbl_bss_w, &hp->lbl_bss);
	XmScaleGetValue (lbl_bds_w, &hp->lbl_bds);
	svf_gettables (hp->type_table, hp->fclass_table);

	n = 0;
	XtSetArg (args[n], XmNwidth, (XtArgVal)&w); n++;
	XtSetArg (args[n], XmNheight, (XtArgVal)&h); n++;
	XtGetValues (svda_w, args, n);
	hp->winw = w;
	hp->winh = h;
}

/* turn off auto mag for an internal logic reason.
 * N.B. do not use this to implement user turning off manually, that would
 *   interfere with user_automagoff.
 */
void
sv_amagoff ()
{
	want_automag = 0;
	XmToggleButtonSetState (wantamag_w, False, False);
	svtb_updateAutoMag (0);
}

/* called when the toolbar "brighter" PB is hit */
void
svtb_brighter_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int stmag, ssmag, dsmag, magstp;

	svf_getmaglimits (&stmag, &ssmag, &dsmag, &magstp);
	stmag += 1;
	ssmag += 1;
	dsmag += 1;
	sv_amagoff ();
	svf_setmaglimits (stmag, ssmag, dsmag, magstp);
	sv_all(mm_get_now());
}

/* called when either toolbar "flip" PB is hit.
 * client is 0 for l/r, else 1 for t/b
 */
void
svtb_flip_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int tb = (int)client;

	if (tb)
	    XmToggleButtonSetState (fliptb_w, flip_tb ^= 1, True);
	else
	    XmToggleButtonSetState (fliplr_w, flip_lr ^= 1, True);
}

/* called when the toolbar "constel" TB changes */
void
svtb_constel_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int set = XmToggleButtonGetState(w);

	if (set) {
	    XmToggleButtonSetState (conf_w, True, False);
	    XmToggleButtonSetState (cona_w, True, False);
	    XmToggleButtonSetState (conn_w, False, False);
	    XmToggleButtonSetState (conb_w, False, False);
	} else {
	    XmToggleButtonSetState (conf_w, False, False);
	    XmToggleButtonSetState (cona_w, False, False);
	    XmToggleButtonSetState (conn_w, False, False);
	    XmToggleButtonSetState (conb_w, False, False);
	}

	want_conbounds = XmToggleButtonGetState (conb_w);
	want_configures = XmToggleButtonGetState (conf_w);
	want_connames = XmToggleButtonGetState (conn_w);
	want_conabbr = XmToggleButtonGetState (cona_w);

	sv_all(mm_get_now());
}

/* called when the toolbar "dimmer" PB is hit */
void
svtb_dimmer_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int stmag, ssmag, dsmag, magstp;

	svf_getmaglimits (&stmag, &ssmag, &dsmag, &magstp);
	stmag -= 1;
	ssmag -= 1;
	dsmag -= 1;
	sv_amagoff ();
	svf_setmaglimits (stmag, ssmag, dsmag, magstp);
	sv_all(mm_get_now());
}

/* called when the toolbar "grid" TB changes */
void
svtb_grid_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int onnow = XmToggleButtonGetState(grid_w);

	/* rely on redraw in callback */
	if (onnow) {
	    XmToggleButtonSetState (grid_w, False, True);
	} else {
	    /* match grid coord to display mode */
	    want_aagrid = aa_mode;
	    XmToggleButtonSetState (aagrid_w, want_aagrid, False);
	    XmToggleButtonSetState (naagrid_w, !want_aagrid, False);
	    XmToggleButtonSetState (grid_w, True, True);
	}
}

/* called when the toolbar "automag" TB changes */
void
svtb_automag_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonSetState (wantamag_w,
				    !XmToggleButtonGetState(wantamag_w), True);
}

/* called when the toolbar "horizon" TB changes */
void
svtb_hzn_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonSetState (hznmap_w, !XmToggleButtonGetState(hznmap_w), 1);
}

/* called when the toolbar "field stars" TB is hit */
void
svtb_fstars_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonSetState (wantfs_w, !XmToggleButtonGetState(wantfs_w), 1);
}

/* called when the toolbar "names" TB changes */
void
svtb_names_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int set = XmToggleButtonGetState (w);

	if (set) {
	    XmToggleButtonSetState (lbl_nst_w, True, False);
	    lbl_lst |= OBJF_NLABEL;
	    XmToggleButtonSetState (lbl_nss_w, True, False);
	    lbl_lss |= OBJF_NLABEL;
	    XmToggleButtonSetState (lbl_nds_w, True, False);
	    lbl_lds |= OBJF_NLABEL;
	} else {
	    XmToggleButtonSetState (lbl_nst_w, False, False);
	    lbl_lst &= ~OBJF_NLABEL;
	    XmToggleButtonSetState (lbl_nss_w, False, False);
	    lbl_lss &= ~OBJF_NLABEL;
	    XmToggleButtonSetState (lbl_nds_w, False, False);
	    lbl_lds &= ~OBJF_NLABEL;
	}

	sv_all(mm_get_now());
}

/* called when the toolbar "planes" TB changes */
void
svtb_planes_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int set = XmToggleButtonGetState (w);

	XmToggleButtonSetState (eclip_w, set, False);
	XmToggleButtonSetState (galac_w, set, False);
	XmToggleButtonSetState (eq_w, set, False);
	want_eq = want_galactic = want_ecliptic = set;

	sv_all(mm_get_now());
}

/* called when the toolbar "orient" TB changes */
void
svtb_orient_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (aa_mode)
	    XmToggleButtonSetState (rad_w, True, True);
	else
	    XmToggleButtonSetState (aa_w, True, True);
}

/* called when the toolbar "cylindrical projection" TB is hit */
void
svtb_proj_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonSetState (sph_w, !XmToggleButtonGetState(w), True);
}

/* called when the toolbar "print" PB is hit */
void
svtb_print_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XPSAsk ("Sky View", sv_print);
}

/* called when the toolbar "zoom in" PB is hit */
void
svtb_zoomin_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* asserts */
	if (zm_cundo < 0) {
	    printf ("zoom Bug! zm_cundo=%d\n", zm_cundo);
	    exit(1);
	}

	/* use undo list if in the middle somewhere else create new */
	if (zm_nundo - zm_cundo >= 2) {
	    /* install next from undo stack */
	    zm_cundo++;
	    zm_installundo ();
	    svtb_zoomok(1);
	    svtb_unzoomok(1);
	    sv_all(mm_get_now());
	} else {
	    /* save new */
	    if (zm_addundo() < 0)
		xe_msg ("Center of zoom box must be inside map", 1);
	    else {
		svtb_zoomok(0);
		sv_all(mm_get_now());
		svtb_unzoomok(1);
	    }
	}
}

/* called when the toolbar "unzoom" PB is hit */
void
svtb_unzoom_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* asserts */
	if (!zm_undo || zm_cundo <= 0) {
	    printf ("unzoom Bug! zm_undo=%d zm_cundo=%d\n", !!zm_undo,zm_cundo);
	    exit(1);
	}

	/* show previous undo; disable PB if hit the top */
	--zm_cundo;
	zm_installundo();
	svtb_zoomok(1);
	sv_all(mm_get_now());
	svtb_unzoomok (zm_cundo != 0);	/* no more unzooms */
}

/* sync the toolbar to match the options */
static void
svtb_sync()
{
	svtb_updateCyl (cyl_proj);
	svtb_updateGrid (want_grid);
	svtb_updateLRFlip (flip_lr);
	svtb_updateTBFlip (flip_tb);
	svtb_updateAutoMag (want_automag);
	svtb_updateNames ((lbl_lst&OBJF_NLABEL) || (lbl_lss&OBJF_NLABEL)
						    || (lbl_lds&OBJF_NLABEL));
	svtb_updateHorizon (want_hznmap);
	svtb_updateFStars (want_fs);
	svtb_updateCns (want_conbounds || want_configures || want_connames ||
							     want_conabbr);
	svtb_updatePlanes (want_eq || want_ecliptic || want_galactic);
}

/* mark the given object, centering if outside current fov.
 * N.B. we do *not* update the s_ fields of op.
 */
void
sv_point (op)
Obj *op;
{
	if (!sv_ison() || !op || op->o_type == UNDEFOBJ)
	    return;

	if (want_infifo)
	    XmToggleButtonSetState (infifo_w, False, True);
	(void) sv_mark (op, 0, 1, 1, 1, 0, 0.0);
}

/* show a marker at the location of the given object *if* it's in fov now.
 * N.B. we do *not* update the s_ fields of op.
 */
void
sv_id (op)
Obj *op;
{
	if (!sv_ison() || !op || op->o_type == UNDEFOBJ)
	    return;

	(void) sv_mark (op, 0, 0, 1, 1, 0, 0.0);
}

/* called to put up or remove the watch cursor.  */
void
sv_cursor (c)
Cursor c;
{
	Window win;

	if (svshell_w && (win = XtWindow(svshell_w)) != 0) {
	    Display *dsp = XtDisplay(svshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}

	if (svops_w && (win = XtWindow(svops_w)) != 0) {
	    Display *dsp = XtDisplay(svops_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* draw everything subject to any filtering.
 */
void
sv_all(np)
Now *np;
{
	Display *dsp = XtDisplay(svda_w);
	/* N.B. don't cache sv_pm, it can change if resized vis XmUpdate */

	/* busy */
	watch_cursor(1);

	/* put up the timestamp */
	timestamp (np, dt_w);

	/* rebuild a clean sky */
	if (!cyl_proj && sv_dfov > PI) {
	    /* horizon will show */
	    int w = (int)(PI*sv_h/sv_vfov);
	    int x = (sv_w - w)/2;
	    int y = (sv_h - w)/2;

	    XSetForeground (dsp, sv_gc, bg_p);
	    XFillRectangle (dsp, sv_pm, sv_gc, 0, 0, sv_w, sv_h);
	    XSetForeground (dsp, sv_gc, sky_p);
	    XPSDrawArc (dsp, sv_pm, sv_gc, x, y, w, w, 0, 360*64);
	    XFillArc (dsp, sv_pm, sv_gc, x, y, w, w, 0, 360*64);

	    /* we assume no fits file could ever be displayed this large */
	} else {
	    /* start with image if there is one, else set up a solid sky bkq */
	    if (sf_ison()) {
		Pixmap fpm = sf_getPixmap();
		if (!fpm) {
		    printf ("sv_all Bug! FITS pixmap disappeared!\n");
		    exit (1);
		}
		XCopyArea (dsp, fpm, sv_pm, sv_gc, 0, 0, sv_w, sv_h, 0, 0);
		sf_ps (sv_pm, sf_elf ? flip_lr : !flip_lr,
						sf_nup ? flip_tb : !flip_tb);
	    } else {
		XSetForeground (dsp, sv_gc, sky_p);
		XPSDrawRectangle (dsp, sv_pm, sv_gc, 0, 0, sv_w-1, sv_h-1);
		XFillRectangle (dsp, sv_pm, sv_gc, 0, 0, sv_w, sv_h);
	    }
	}

	/* draw eyepieces next so everything is on top of them */
	if (want_eyep)
	    draw_eyep (dsp, sv_pm, sv_gc);

	/* draw the coord grid if desired */
	if (want_grid) {
	    XSetForeground (dsp, sv_gc, grid_p);
	    draw_grid(dsp, sv_pm, sv_gc);
	}

	/* draw the equator if desired */
	if (want_eq) {
	    XSetForeground (dsp, sv_gc, eq_p);
	    draw_equator (np, dsp, sv_pm, sv_gc);
	}

	/* draw the galactic poles and eq if desired */
	if (want_galactic) {
	    XSetForeground (dsp, sv_gc, eq_p);
	    draw_galactic (np, dsp, sv_pm, sv_gc);
	}

	/* draw the ecliptic */
	if (want_ecliptic) {
	    XSetForeground (dsp, sv_gc, eq_p);
	    draw_ecliptic (np, dsp, sv_pm, sv_gc);
	}

	/* draw constellation boundaries if desired */
	if (want_conbounds)
	    draw_cnsbounds (np, dsp, sv_pm);

	/* draw constellation figures and/or names if desired */
	if (want_configures || want_connames || want_conabbr)
	    draw_cns (np, dsp, sv_pm);

	/* go through the trailobj list and display that stuff too. */
	tobj_display_all();

	/* draw umbra (late, so shows over moon) */
	if (want_ecliptic) {
	    XSetForeground (dsp, sv_gc, eq_p);
	    draw_umbra (np, dsp, sv_pm, sv_gc);
	}

	/* draw horizon map if desired, late so skyline can do clipping */
	if (want_hznmap) {
	    XSetForeground (dsp, sv_gc, hzn_p);
	    draw_hznmap (np, dsp, sv_pm, sv_gc);
	}

	/* draw all the objects -- they do their own clipping so
	 * this way their names are not clipped
	 */
	draw_allobjs (dsp, sv_pm);

	/* draw the magnitude scale last so it always covers everything else */
	if (want_magscale)
	    sv_magscale (dsp, sv_pm);

	/* and we're done */
	sv_copy_sky();

	watch_cursor(0);

}

/* compute the angle ccw from 3 o'clock at which we should draw the galaxy
 * ellipse
 */
static double
x_angle (op)
Obj *op;
{
	Now *np = mm_get_now();
	double na;		/* angle from 3 o'clock to N */
	double pa;		/* object's PA (angle E of N) */
	double a;		/* ellipse axis: angle CCW from 3 oclk*/
	int x0, y0;		/* location of object */
	int xn, yn;		/* location a little N of object */
	int xe, ye;		/* location a little E of object */
	double step;		/* "a little" */
	Obj o;			/* test object */
	double altdec, azra;	/* test location */
	int eccw;		/* 1 if E shows CCW of N, else 0 */

	/* move by about 50 pixels each way (or 1 degree if new) */
	step = sv_h>0 ? sv_vfov/sv_h*50 : degrad(1);

	/* location of object */
	o.o_type = FIXED;
	o.f_RA = op->s_ra;
	o.f_dec = op->s_dec;
	o.f_epoch = (float)epoch;
	(void) obj_cir (np, &o);
	altdec = aa_mode ? o.s_alt : o.s_dec;
	azra   = aa_mode ? o.s_az  : o.s_ra;
	(void) sv_loc (altdec, azra, &x0, &y0);

	/* location due north */
	o.f_RA = op->s_ra;
	o.f_dec = (float)(op->s_dec + step);
	if (o.f_dec > PI/2) {
	    o.f_RA += (float)PI;
	    o.f_dec = (float)PI - o.f_dec;
	}
	(void) obj_cir (np, &o);
	altdec = aa_mode ? o.s_alt : o.s_dec;
	azra   = aa_mode ? o.s_az  : o.s_ra;
	(void) sv_loc (altdec, azra, &xn, &yn);

	/* location due east */
	o.f_RA = (float)(op->s_ra + step/cos(op->s_dec));
	o.f_dec = op->s_dec;
	(void) obj_cir (np, &o);
	altdec = aa_mode ? o.s_alt : o.s_dec;
	azra   = aa_mode ? o.s_az  : o.s_ra;
	(void) sv_loc (altdec, azra, &xe, &ye);

	/* see which way is east from north -- use cross product */
	eccw = (xn-x0)*(y0-ye)-(y0-yn)*(xe-x0) > 0 ? 1 : 0;

	/* find angle ccw from 3-oclock */
	if (xn == x0)
	    na = yn > y0 ? -PI/2 : PI/2;
	else
	    na = atan2 ((double)(y0-yn), (double)(xn-x0));
	pa = get_pa (op);
	a = eccw ? na+pa : na-pa;
	range (&a, 2*PI);

	return (a);
}

/* draw the given object so it has a nominal diameter of diam pixels.
 * we maintain a static cache of common X drawing objects for efficiency.
 * (mallocing seemed to keep the memory arena too fragmented).
 * to force a flush of this cache, call with op == NULL.
 * N.B. X's DrawRect function is actually w+1 wide and h+1 high.
 */
void
sv_draw_obj (dsp, win, gc, op, x, y, diam, dotsonly)
Display *dsp;
Drawable win;
GC gc;
Obj *op;
int x, y;
int diam;
int dotsonly;	/* whether to use dots for all star varieties */
{
#define	MINSYMDIA	4	/* never use fancy symbol at diam this small */
#define	CACHE_SZ	100	/* size of points/arcs/etc cache */
#define	CACHE_PAD	10	/* most we ever need in one call */
#define	CACHE_HWM	(CACHE_SZ - CACHE_PAD)	/* hi water mark */
	static XPoint     xpoints[CACHE_SZ],    *xp  = xpoints;
	static XArc       xdrawarcs[CACHE_SZ],  *xda = xdrawarcs;
	static XArc       xfillarcs[CACHE_SZ],  *xfa = xfillarcs;
	static XSegment   xsegments[CACHE_SZ],  *xs  = xsegments;
	static XRectangle xdrawrects[CACHE_SZ], *xdr = xdrawrects;
	static XRectangle xfillrects[CACHE_SZ], *xfr = xfillrects;
	static GC cache_gc;
	int force;
	int n;

	/* for sure if no op or different gc */
	force = !op || gc != cache_gc;

	if ((n = xp - xpoints) >= CACHE_HWM || (force && n > 0)) {
	    XPSDrawPoints (dsp, win, cache_gc, xpoints, n, CoordModeOrigin);
	    xp = xpoints;
	}
	if ((n = xda - xdrawarcs) >= CACHE_HWM || (force && n > 0)) {
	    XPSDrawArcs (dsp, win, cache_gc, xdrawarcs, n);
	    xda = xdrawarcs;
	}
	if ((n = xfa - xfillarcs) >= CACHE_HWM || (force && n > 0)) {
	    XPSFillArcs (dsp, win, cache_gc, xfillarcs, n);
	    xfa = xfillarcs;
	}
	if ((n = xs - xsegments) >= CACHE_HWM || (force && n > 0)) {
	    XPSDrawSegments (dsp, win, cache_gc, xsegments, n);
	    xs = xsegments;
	}
	if ((n = xdr - xdrawrects) >= CACHE_HWM || (force && n > 0)) {
	    XPSDrawRectangles (dsp, win, cache_gc, xdrawrects, n);
	    xdr = xdrawrects;
	}
	if ((n = xfr - xfillrects) >= CACHE_HWM || (force && n > 0)) {
	    XPSFillRectangles (dsp, win, cache_gc, xfillrects, n);
	    xfr = xfillrects;
	}

	cache_gc = gc;

	if (!op)
	    return;	/* just flushing, thanks */

	/* if object is smallish then we don't use the fancy
	 * symbols, just use dots or filled circles.
	 */
	if (diam <= MINSYMDIA) {
	    if (diam <= 1) {
		xp->x = x;
		xp->y = y;
		xp++;
	    } else {
		xfa->x = x - diam/2;
		xfa->y = y - diam/2;
		xfa->width = diam;
		xfa->height = diam;
		xfa->angle1 = 0;
		xfa->angle2 = 360*64;
		xfa++;
	    }
	    return;
	}

	switch (op->o_type) {
	case PLANET:
	    /* filled circle */
	    xfa->x = x - diam/2;
	    xfa->y = y - diam/2;
	    xfa->width = diam;
	    xfa->height = diam;
	    xfa->angle1 = 0;
	    xfa->angle2 = 360*64;
	    xfa++;
	    break;

	case FIXED:
	    switch (op->f_class) {
	    case 'G': /* galaxies */
	    case 'H':
		{
		    int xa;		/* X angle: + CCW from 3 oclock */
		    int hh;		/* half-height */
		    double a;		/* ellipse axis: angle CCW from 3 oclk*/

		    a = x_angle (op);

		    /* convert to X Windows units */
		    xa = (int)floor(raddeg(a)*64 + 0.5);

		    /* draw ellipse */
		    n = diam/2;
		    hh = (int)floor(n*get_ratio(op) + 0.5);
		    XPSDrawEllipse (dsp, win, gc, x - n, y - hh,
						    xa, 2*n, 2*hh, 0, 360*64);
		}
		break;

	    case 'A': /* galaxy cluster */
		/* ellipse */
		n = diam/2;
		xda->x = x - n;
		xda->y = y - n/2;
		xda->width = diam;
		xda->height = n;
		xda->angle1 = 0;
		xda->angle2 = 360*64;
		xda++;
		break;

	    case 'C': /* globular clusters */
		/* plus in a circle, like Tirion */
		n = diam/2;
		xda->x = x - n;
		xda->y = y - n;
		xda->width = 2*n;
		xda->height = 2*n;
		xda->angle1 = 0;
		xda->angle2 = 360*64;
		xda++;
		xs->x1 = x-n; xs->y1 = y; xs->x2 = x+n; xs->y2 = y; xs++;
		xs->x1 = x; xs->y1 = y-n; xs->x2 = x; xs->y2 = y+n; xs++;
		break;

	    case 'U': /* glubular cluster within nebulosity */
		/* plus in a dotted circle */
		n = diam/2;
		xs->x1 = x-n; xs->y1 = y; xs->x2 = x+n; xs->y2 = y; xs++;
		xs->x1 = x; xs->y1 = y-n; xs->x2 = x; xs->y2 = y+n; xs++;

		/* FALLTHRU */

	    case 'O': /* open cluster */
		/* dotted circle */
		{
		    int nd = (int)(PI*diam/12 + 1); /* every 3rd dot */
		    double da = PI/2/nd;
		    int r  = diam/2;
		    int i;

		    /* reflect for quadrants 2-4 */
		    for (i = 0; i < nd; i++) {
			int dx = (int)floor(r*cos(i*da) + 0.5);
			int dy = (int)floor(r*sin(i*da) + 0.5);

			/* be mindful of filling the xpoints cache */
			if ((n = xp - xpoints) > CACHE_HWM-4) {
			    XPSDrawPoints (dsp, win, cache_gc, xpoints, n,
							    CoordModeOrigin);
			    xp = xpoints;
			}

			xp->x = x + dx; xp->y = y + dy; xp++;
			xp->x = x - dy; xp->y = y + dx; xp++;
			xp->x = x - dx; xp->y = y - dy; xp++;
			xp->x = x + dy; xp->y = y - dx; xp++;
		    }
		}
		break;

	    case 'P': /* planetary nebula */
		/* open square */
		n = diam/2;
		xdr->x = x-n;
		xdr->y = y-n;
		xdr->width = diam;
		xdr->height = diam;
		xdr++;
		break;

	    case 'R': /* supernova remnant */
		/* two concentric circles */
		n = diam/2;
		xda->x = x - n;
		xda->y = y - n;
		xda->width = diam;
		xda->height = diam;
		xda->angle1 = 0;
		xda->angle2 = 360*64;
		xda++;

		if (diam > 7) {
		    xda->x = x - n + 3;
		    xda->y = y - n + 3;
		    xda->width = diam-6;
		    xda->height = diam-6;
		    xda->angle1 = 0;
		    xda->angle2 = 360*64;
		    xda++;
		} else {
		    xp->x = x;
		    xp->y = y;
		    xp++;
		}

		break;

	    case 'S': /* stars */
		/* filled circle */
		n = diam/2;
		xfa->x = x - n;
		xfa->y = y - n;
		xfa->width = diam;
		xfa->height = diam;
		xfa->angle1 = 0;
		xfa->angle2 = 360*64;
		xfa++;
		break;

	    case 'D': /* double stars */
	    case 'B': /* binary stars */
		/* filled circle with one horizontal line through it */
		n = diam/2;
		xfa->x = x - n;
		xfa->y = y - n;
		xfa->width = diam;
		xfa->height = diam;
		xfa->angle1 = 0;
		xfa->angle2 = 360*64;
		xfa++;

		if (!dotsonly) {
		    xs->x1 = x - n - diam/4;
		    xs->y1 = y;
		    xs->x2 = x - n + diam + diam/4;
		    xs->y2 = y;
		    xs++;
		}

		break;

	    case 'M': /* multiple stars */
		/* filled circle with two horizontal lines through it */
		n = diam/2;
		xfa->x = x - n;
		xfa->y = y - n;
		xfa->width = diam;
		xfa->height = diam;
		xfa->angle1 = 0;
		xfa->angle2 = 360*64;
		xfa++;

		if (!dotsonly) {
		    xs->x1 = x - n - diam/4;
		    xs->x2 = x - n + diam + diam/4;
		    xs->y2 = xs->y1 = y - n + n/2 + 1;
		    xs++;
		    xs->x1 = x - n - diam/4;
		    xs->x2 = x - n + diam + diam/4;
		    xs->y2 = xs->y1 = y - n + diam - 1 - n/2;
		    xs++;
		}

		break;
	    case 'V': /* variable star */
		/* central dot with concentric circle */
		n = diam/2;
		if (dotsonly) {
		    xfa->x = x - n;
		    xfa->y = y - n;
		    xfa->width = diam;
		    xfa->height = diam;
		    xfa->angle1 = 0;
		    xfa->angle2 = 360*64;
		    xfa++;
		} else {
		    xda->x = x - n;
		    xda->y = y - n;
		    xda->width = diam;
		    xda->height = diam;
		    xda->angle1 = 0;
		    xda->angle2 = 360*64;
		    xda++;

		    if (diam > 7) {
			xfa->x = x - n + 3;
			xfa->y = y - n + 3;
			xfa->width = diam-6;
			xfa->height = diam-6;
			xfa->angle1 = 0;
			xfa->angle2 = 360*64;
			xfa++;
		    } else {
			if (diam == 6) {
			    xfa->x = x - 1;
			    xfa->y = y - 1;
			    xfa->width = 2;
			    xfa->height = 2;
			    xfa->angle1 = 0;
			    xfa->angle2 = 360*64;
			    xfa++;
			} else {
			    xp->x = x;
			    xp->y = y;
			    xp++;
			}
		    }
		}

		break;

	    case 'F': /* diffuse nebula */
	    case 'K': /* dark nebulae */
		/* open diamond */
		n = diam/2;
		xs->x1 = x-n; xs->y1 = y; xs->x2 = x; xs->y2 = y-n; xs++;
		xs->x1 = x; xs->y1 = y-n; xs->x2 = x+n; xs->y2 = y; xs++;
		xs->x1 = x+n; xs->y1 = y; xs->x2 = x; xs->y2 = y+n; xs++;
		xs->x1 = x; xs->y1 = y+n; xs->x2 = x-n; xs->y2 = y; xs++;
		break;

	    case 'N': /* bright nebulae */
		/* open hexagon */
		n = diam/2;
		xs->x1 = x-n; xs->y1 = y; xs->x2 = x-n/2; xs->y2 = y-n; xs++;
		xs->x1 = x-n/2; xs->y1 = xs->y2 = y-n; xs->x2 = x+n/2;  xs++;
		xs->x1 = x+n/2; xs->y1 = y-n; xs->x2 = x+n; xs->y2 = y; xs++;
		xs->x1 = x+n; xs->y1 = y; xs->x2 = x+n/2; xs->y2 = y+n; xs++;
		xs->x1 = x+n/2; xs->y1 = xs->y2 = y+n; xs->x2 = x-n/2;  xs++;
		xs->x1 = x-n/2; xs->y1 = y+n; xs->x2 = x-n; xs->y2 = y; xs++;
		break;

	    case 'Q': /* Quasar */
		/* plus sign */
		n = diam/2;
		xs->x1 = x-n; xs->y1 = y; xs->x2 = x+n; xs->y2 = y; xs++;
		xs->x1 = x; xs->y1 = y-n; xs->x2 = x; xs->y2 = y+n; xs++;
		break;

	    case 'L':	/* Pulsar */
		/* vertical line */
		n = diam/2;
		xs->x1 = x; xs->y1 = y-n; xs->x2 = x; xs->y2 = y+n; xs++;
		break;

	    case 'J':	/* Radio source */
		/* half-circle pointing up and to the left */
		n = diam/2;
		xda->x = x - n;
		xda->y = y - n;
		xda->width = diam;
		xda->height = diam;
		xda->angle1 = 225*64;
		xda->angle2 = 180*64;
		xda++;
		break;

	    case 'T':	/* stellar object */
		if (dotsonly) {
		    /* filled circle */
		    n = diam/2;
		    xfa->x = x - n;
		    xfa->y = y - n;
		    xfa->width = diam;
		    xfa->height = diam;
		    xfa->angle1 = 0;
		    xfa->angle2 = 360*64;
		    xfa++;
		    break;
		}
		/* FALLTHRU */

	    default:	/* unknown type */
		/* an X */
		n = diam/3;
		xs->x1= x-n; xs->y1= y-n; xs->x2= x+n; xs->y2= y+n; xs++;
		xs->x1= x-n; xs->y1= y+n; xs->x2= x+n; xs->y2= y-n; xs++;
		break;
	    }
	    break;

	case HYPERBOLIC:
	case PARABOLIC:
	    /* hopefully has a tail -- draw a filled circle with one */
	    n = diam/2;
	    xfa->x = x;
	    xfa->y = y;
	    xfa->width = n;
	    xfa->height = n;
	    xfa->angle1 = 0;
	    xfa->angle2 = 360*64;
	    xfa++;
	    xfa->x = x - n;
	    xfa->y = y - n;
	    xfa->width = diam;
	    xfa->height = diam;
	    xfa->angle1 = 120*64;
	    xfa->angle2 = 30*64;
	    xfa++;
	    break;

	case ELLIPTICAL: /* asteroids */
	    /* filled square */
	    n = diam/2;
	    xfr->x = x - n;
	    xfr->y = y - n;
	    xfr->width = diam;
	    xfr->height = diam;
	    xfr++;

	    break;
	case EARTHSAT:
	    /* open circle */
	    n = diam/2;
	    xda->x = x - n;
	    xda->y = y - n;
	    xda->width = diam;
	    xda->height = diam;
	    xda->angle1 = 0;
	    xda->angle2 = 360*64;
	    xda++;
	    break;

	default:
	    printf ("Bad type to sv_draw_obj: %d\n", op->o_type);
	    exit(1);
	}
}

/* called by sky*.c to get the current pointing info */
void
sv_getcenter (aamodep, fovp, altp, azp, rap, decp)
int *aamodep;
double *fovp;
double *altp;
double *azp;
double *rap;
double *decp;
{
	sv_fullwhere (mm_get_now(), sv_altdec, sv_azra, aa_mode, altp, azp,
								rap, decp);

	*aamodep = aa_mode;
	*fovp = sv_vfov;
}

/* return to the caller our current field star list.
 * if !want_fs we return NULL even if there is a pending list.
 */
void
sv_getfldstars (fspp, nfsp)
ObjF **fspp;
int *nfsp;
{
	if (want_fs) {
	    *fspp = fldstars;
	    *nfsp = nfldstars;
	} else {
	    *fspp = NULL;
	    *nfsp = 0;
	}
}

/* called by trails.c to create a new trail for pu.op.
 * client is the Obj* to which a trail will be added.
 * return 0 if ok else -1.
 */
static int
sv_mktrail (ts, statep, client)
TrTS ts[];
TrState *statep;
XtPointer client;
{
	Obj *trop = (Obj *)client;
	TrailObj *top;
	Now *np, now;
	int i;

	/* set trop to object getting new trail.
	 * started out as popup object but if that has changed we search the
	 * db to make sure it is still valid. if not, well it's gone.
	 */
	if (trop != pu.op) {
	    DBScan dbs;
	    Obj *op;

	    for (db_scaninit(&dbs, ALLM, want_fs ? fldstars : NULL, nfldstars);
					    (op = db_scan (&dbs)) != NULL; )
		if (trop == op)
		    break;
	    if (!op) {
		xe_msg ("Object has disappeared -- no trail created", 1);
		return (-1);
	    }
	}

	/* turn on watch cursor for really slow systems */
	watch_cursor (1);

	/* discard any preexisting trails for this object. */
	tobj_rmobj (trop);	/* silently does nothing if op not on list */

	/* allocate a new trailobj for this trail */
	top = tobj_addobj (trop, statep->nticks);

	/* work with a copy of Now because we modify n_mjd */
	np = mm_get_now();
	now = *np;

	/* make each entry */
	for (i = 0; i < statep->nticks; i++) {
	    TSky *tsp = &top->sky[i];
	    TrTS *tp = &ts[i];

	    /* save the timestamp info time */
	    tsp->trts = *tp;

	    /* compute circumstances */
	    tsp->o = *trop;
	    now.n_mjd = tp->t;
	    (void) obj_cir (&now, &tsp->o);
	}

	/* save the other setup details for this trail */
	top->trs = *statep;

	/* retain setup state as default for next time */
	trstate = *statep;

	/* redraw everything with the new trail, if we are up */
	if (isUp(svshell_w))
	    sv_all (mm_get_now());

	watch_cursor (0);

	return (0);
}

/* helper function to fill in the Help menu*/
static void
sv_fillin_help (mb_w)
Widget mb_w;
{
	typedef struct {
	    char *label;	/* what goes on the help label */
	    char *key;		/* string to call hlp_dialog() */
	} HelpOn;
	static HelpOn helpon[] = {
	    {"Intro...",	"Sky View - intro"},
	    {"on Control...",	"Sky View - control"},
	    {"on Locate...",	"Sky View - locate"},
	    {"on Telescope...",	"Sky View - telescope"},
	    {"on History...",	"Sky View - history"},
	    {"on Mouse...",	"Sky View - mouse"},
	    {"on Trails...",	"Sky View - trails"},
	    {"on Scales...",	"Sky View - scales"},
	    {"Misc...",		"Sky View - misc"},
	};
	Widget pd_w, cb_w;
	Widget w;
	XmString str;
	Arg args[20];
	int n;
	int i;

	n = 0;
	pd_w = XmCreatePulldownMenu (mb_w, "HPD", args, n);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pd_w); n++;
	    XtSetArg (args[n], XmNmnemonic, 'H'); n++;
	    cb_w = XmCreateCascadeButton (mb_w, "Help", args, n);
	    XtManageChild (cb_w);
	    set_something (mb_w, XmNmenuHelpWidget, (XtArgVal)cb_w);

	    for (i = 0; i < XtNumber(helpon); i++) {
		HelpOn *hp = &helpon[i];

		str = XmStringCreate (hp->label, XmSTRING_DEFAULT_CHARSET);
		n = 0;
		XtSetArg (args[n], XmNlabelString, str); n++;
		XtSetArg (args[n], XmNmarginHeight, 0); n++;
		w = XmCreatePushButton (pd_w, "Help", args, n);
		XtAddCallback (w, XmNactivateCallback, sv_helpon_cb,
							(XtPointer)(hp->key));
		XtManageChild (w);
		XmStringFree(str);
	    }
}

/* helper function to fill in the menubar */
static void
sv_fillin_mb (mb_w)
Widget mb_w;
{
	Widget cb_w, pd_w;
	Widget w;
	XmString str;
	Arg args[20];
	int n;

	/* control pulldown */

	n = 0;
	pd_w = XmCreatePulldownMenu (mb_w, "CPD", args, n);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pd_w); n++;
	    XtSetArg (args[n], XmNmnemonic, 'C'); n++;
	    cb_w = XmCreateCascadeButton (mb_w, "Control", args, n);
	    wtip (cb_w, "Access to many Sky View supporting features");
	    XtManageChild (cb_w);

	    /* create the list control */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "List", args, n);
	    XtAddCallback (w, XmNactivateCallback, sv_list_cb, NULL);
	    set_xmstring (w, XmNlabelString, "List...");
	    wtip (w,
	       "Create a file listing all objects in the current view");
	    XtManageChild (w);

	    /* create the print control */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "SVPrint", args, n);
	    XtAddCallback (w, XmNactivateCallback, sv_print_cb, NULL);
	    set_xmstring (w, XmNlabelString, "Print...");
	    wtip (w, "Print the current Sky View map");
	    XtManageChild (w);

	    /* make the pb that controls the filter menu */

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Filter", args, n);
	    XtAddCallback (w, XmNactivateCallback, sv_filter_cb, NULL);
	    set_xmstring (w, XmNlabelString, "Filter...");
	    wtip (w, "Dialog to select type and brightness of objects");
	    XtManageChild (w);

	    /* make the pb which can bring up the FITS file dialog */
	    n = 0;
	    w = XmCreatePushButton (pd_w, "Fits", args, n);
	    XtAddCallback (w, XmNactivateCallback, sv_fits_cb, NULL);
	    set_xmstring (w, XmNlabelString, "Image...");
	    wtip (w, "Display FITS files and retrieve DSS images");
	    XtManageChild (w);

	    /* make the PB which can bring up the Options dialog */
	    n = 0;
	    w = XmCreatePushButton (pd_w, "FPB", args, n);
	    XtAddCallback (w, XmNactivateCallback, sv_opdialog_cb, 0);
	    set_xmstring (w, XmNlabelString, "Options...");
	    wtip (w, "List of several viewing options");
	    XtManageChild (w);

	    /* make the pb which can bring up the horizon setup dialog */
	    n = 0;
	    w = XmCreatePushButton (pd_w, "Hzn", args, n);
	    XtAddCallback (w, XmNactivateCallback,(XtCallbackProc)hzn_manage,0);
	    set_xmstring (w, XmNlabelString, "Horizon...");
	    wtip (w, "Specify local horizon");
	    XtManageChild (w);

	    /* make the pb which can bring up the field star setup dialog */
	    n = 0;
	    w = XmCreatePushButton (pd_w, "FS", args, n);
	    XtAddCallback (w, XmNactivateCallback, (XtCallbackProc)fs_manage,0);
	    set_xmstring (w, XmNlabelString, "Field Stars...");
	    wtip (w, "Setup for GSC and other big catalogs");
	    XtManageChild (w);

	    /* Pb to toggle the Eyepiece setup controls */
	    n = 0;
	    w = XmCreatePushButton (pd_w, "SVEPSz", args, n);
	    XtAddCallback (w, XmNactivateCallback, sv_eyep_cb, NULL);
	    set_xmstring (w, XmNlabelString, "Eyepieces...");
	    wtip (w, "Setup Eyepiece parameters");
	    XtManageChild (w);

	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    /* create the tracking tb */

	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNset, False); n++;	/* insure not overrode*/
	    tracktb_w = XmCreateToggleButton (pd_w, "Tracking", args, n);
	    XtAddCallback (tracktb_w, XmNvalueChangedCallback, sv_track_cb, 0);
	    XtSetSensitive (tracktb_w, False);
	    wtip (tracktb_w,"When on, Sky View stays locked onto an object");
	    XtManageChild (tracktb_w);

	    n = 0;
	    w = XmCreateSeparator (pd_w, "Sep", args, n);
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (pd_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, sv_close_cb, 0);
	    wtip (w, "Close this and all supporting dialogs");
	    XtManageChild (w);

	/* make the "locate" cascade button and pulldown */

	sv_create_find (mb_w);

	/* make the Fifos pulldown */

	n = 0;
	telpd_w = XmCreatePulldownMenu (mb_w, "FPD", args, n);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, telpd_w); n++;
	    XtSetArg (args[n], XmNmnemonic, 'T'); n++;
	    cb_w = XmCreateCascadeButton (mb_w, "Telescope", args, n);
	    wtip (cb_w, "Control connections to telescope control system");
	    XtManageChild (cb_w);

	    /* make the "In FIFO" toggle button */

	    str = XmStringCreate ("Enable Telescope Marker",
	    					XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    infifo_w = XmCreateToggleButton (telpd_w, "InFIFO", args, n);
	    XtAddCallback (infifo_w, XmNvalueChangedCallback, sv_in_fifo_cb, 0);
	    wtip (infifo_w, "Mark telescope location on map");
	    XtManageChild (infifo_w);
	    XmStringFree(str);
	    want_infifo = XmToggleButtonGetState (infifo_w);
	    sky_setup_infifo();

	    /* make the "Loc FIFO" toggle button */

	    str = XmStringCreate ("Enable Telescope Control",
						    XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    locfifo_w = XmCreateToggleButton (telpd_w, "LocFIFO", args, n);
	    XtAddCallback (locfifo_w, XmNvalueChangedCallback, sv_loc_fifo_cb,
									NULL);
	    wtip (locfifo_w, "Send mouse coordinates to telescope and add to list below");
	    XtManageChild (locfifo_w);
	    want_locfifo = XmToggleButtonGetState (locfifo_w);
	    XmStringFree(str);

	/* history */
	svh_create (mb_w);

	/* help */
	sv_fillin_help (mb_w);
}

/* create the main skyview shell */
static void
sv_create_svshell()
{
	Widget altdecsc_w, fovsc_w;
	Widget mb_w;
	Widget fr_w;
	Widget svform_w;
	XmString xms;
	Arg args[20];
	EventMask mask;
	int n;

	/* create shell and form */

	n = 0;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle, "xephem Sky View"); n++;
	XtSetArg (args[n], XmNiconName, "Sky"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	svshell_w = XtCreatePopupShell ("SkyView", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (svshell_w, XmNcolormap, (XtArgVal)xe_cm);
        XtAddCallback (svshell_w, XmNpopdownCallback, sv_popdown_cb, 0);
	sr_reg (svshell_w, "XEphem*SkyView.width", skycategory, 0);
	sr_reg (svshell_w, "XEphem*SkyView.height", skycategory, 0);
	sr_reg (svshell_w, "XEphem*SkyView.x", skycategory, 0);
	sr_reg (svshell_w, "XEphem*SkyView.y", skycategory, 0);

	n = 0;
	svform_w = XmCreateForm (svshell_w, "SVForm", args, n);
	XtAddCallback (svform_w, XmNhelpCallback, sv_help_cb, 0);
	XtManageChild (svform_w);

	/* make a menu bar across the top -- fill in later */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	mb_w = XmCreateMenuBar (svform_w, "MB", args, n);
	XtManageChild (mb_w);

	/* top toolbar below */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, mb_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 0); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightOffset, 0); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	ttbar_w = XmCreateRowColumn (svform_w, "TToolbar", args, n);
	XtManageChild (ttbar_w);

	/* make a timestamp label across the bottom */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 25); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 75); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	XtSetArg (args[n], XmNrecomputeSize, False); n++;
	dt_w = XmCreateLabel (svform_w, "TimeStamp", args, n);
	wtip (dt_w, "Date and Time for which map is computed");
	XtManageChild(dt_w);

	/* altdec short cuts in lower right corner.
	 * create with XmString because of !XmNrecomputeSize
	 */

	xms = XmStringCreateSimple ("45:00");
	n = 0;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightOffset, 1); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 1); n++;
	XtSetArg (args[n], XmNmarginWidth, 1); n++;
	XtSetArg (args[n], XmNmarginHeight, 1); n++;
	XtSetArg (args[n], XmNrecomputeSize, False); n++;
	XtSetArg (args[n], XmNlabelString, xms); n++;
	altdecsc_w = XmCreatePushButton (svform_w, "ADSC", args, n);
	XmStringFree (xms);
	XtAddCallback (altdecsc_w, XmNactivateCallback, sv_altdecsc_cb, 
							    (XtPointer)45);
	wtip (altdecsc_w, "Press to slide Alt/Dec scale to 45:00");
	XtManageChild (altdecsc_w);

	xms = XmStringCreateSimple ("0:00");
	n = 0;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, altdecsc_w); n++;
	XtSetArg (args[n], XmNrightOffset, 1); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 1); n++;
	XtSetArg (args[n], XmNmarginWidth, 1); n++;
	XtSetArg (args[n], XmNmarginHeight, 1); n++;
	XtSetArg (args[n], XmNrecomputeSize, False); n++;
	XtSetArg (args[n], XmNlabelString, xms); n++;
	altdecsc_w = XmCreatePushButton (svform_w, "ADSC", args, n);
	XmStringFree (xms);
	XtAddCallback (altdecsc_w, XmNactivateCallback, sv_altdecsc_cb, 
							    (XtPointer)0);
	wtip (altdecsc_w, "Press to slide Alt/Dec scale to 0:00");
	XtManageChild (altdecsc_w);

	xms = XmStringCreateSimple ("-45:00");
	n = 0;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, altdecsc_w); n++;
	XtSetArg (args[n], XmNrightOffset, 1); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 1); n++;
	XtSetArg (args[n], XmNmarginWidth, 1); n++;
	XtSetArg (args[n], XmNmarginHeight, 1); n++;
	XtSetArg (args[n], XmNrecomputeSize, False); n++;
	XtSetArg (args[n], XmNlabelString, xms); n++;
	altdecsc_w = XmCreatePushButton (svform_w, "ADSC", args, n);
	XmStringFree (xms);
	XtAddCallback (altdecsc_w, XmNactivateCallback, sv_altdecsc_cb, 
							    (XtPointer)-45);
	wtip (altdecsc_w, "Press to slide Alt/Dec scale to 45:00");
	XtManageChild (altdecsc_w);

	/* FOV short cuts in lower left corner.
	 * create with XmString because of !XmNrecomputeSize
	 */

	xms = XmStringCreateSimple ("90H");
	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 1); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 1); n++;
	XtSetArg (args[n], XmNmarginWidth, 1); n++;
	XtSetArg (args[n], XmNmarginHeight, 1); n++;
	XtSetArg (args[n], XmNrecomputeSize, False); n++;
	XtSetArg (args[n], XmNlabelString, xms); n++;
	fovsc_w = XmCreatePushButton (svform_w, "FOVSC", args, n);
	XtAddCallback (fovsc_w, XmNactivateCallback, sv_fovsc_cb,(XtPointer)0);
	XmStringFree (xms);
	wtip (fovsc_w, "Set FOV scale to 90:00H");
	XtManageChild (fovsc_w);

	xms = XmStringCreateSimple ("1:1");
	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, fovsc_w); n++;
	XtSetArg (args[n], XmNleftOffset, 5); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 1); n++;
	XtSetArg (args[n], XmNmarginWidth, 1); n++;
	XtSetArg (args[n], XmNmarginHeight, 1); n++;
	XtSetArg (args[n], XmNrecomputeSize, False); n++;
	XtSetArg (args[n], XmNlabelString, xms); n++;
	fovsc_w = XmCreatePushButton (svform_w, "FOVSC", args, n);
	XtAddCallback (fovsc_w, XmNactivateCallback, sv_fovsc_cb,(XtPointer)1);
	XmStringFree (xms);
	wtip (fovsc_w, "Resize width to create 1:1 window size");
	XtManageChild (fovsc_w);

	xms = XmStringCreateSimple ("2:1");
	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, fovsc_w); n++;
	XtSetArg (args[n], XmNleftOffset, 1); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 1); n++;
	XtSetArg (args[n], XmNmarginWidth, 1); n++;
	XtSetArg (args[n], XmNmarginHeight, 1); n++;
	XtSetArg (args[n], XmNrecomputeSize, False); n++;
	XtSetArg (args[n], XmNlabelString, xms); n++;
	fovsc_w = XmCreatePushButton (svform_w, "FOVSC", args, n);
	XtAddCallback (fovsc_w, XmNactivateCallback, sv_fovsc_cb,(XtPointer)2);
	XmStringFree (xms);
	wtip (fovsc_w, "Resize width to create 2:1 window size");
	XtManageChild (fovsc_w);

	/* make the three scale value displays above the shortcuts */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 33); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, altdecsc_w); n++;
	XtSetArg (args[n], XmNrecomputeSize, False); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	fovl_w = XmCreateLabel (svform_w, "FOVL", args, n);
	wtip (fovl_w, "Field of View, Deg:Min");
	XtManageChild (fovl_w);

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 34); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 66); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, altdecsc_w); n++;
	XtSetArg (args[n], XmNrecomputeSize, False); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	azral_w = XmCreateLabel (svform_w, "AzRAL", args, n);
	wtip (azral_w, "Azimuth in Deg:Min or RA in Hours:Min");
	XtManageChild (azral_w);

	n = 0;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 67); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, altdecsc_w); n++;
	XtSetArg (args[n], XmNrecomputeSize, False); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	altdecl_w = XmCreateLabel (svform_w, "AltDecL", args, n);
	wtip (altdecl_w, "Altitude or Declination, Deg:Min");
	XtManageChild (altdecl_w);

	/* make the bottom scale above the row of scale values */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, azral_w); n++; /* typical */
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNminimum, 0); n++;
	XtSetArg (args[n], XmNmaximum, 4320-1); n++;	/* 5' steps */
	XtSetArg (args[n], XmNshowValue, False); n++;
	azra_w = XmCreateScale (svform_w, "AzRAScale", args, n);
	wtip (azra_w, "Azimuth or RA scale");
	XtAddCallback (azra_w, XmNdragCallback, sv_scale_cb, (XtPointer)AZRA_S);
	XtAddCallback (azra_w, XmNvalueChangedCallback, sv_scale_cb,
							    (XtPointer)AZRA_S);
	XtManageChild (azra_w);
	sr_reg (azra_w, NULL, skycategory, 0);

	/* make the left toolbar */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, ttbar_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, azra_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 0); n++;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	ltbar_w = XmCreateRowColumn (svform_w, "LToolbar", args, n);
	XtManageChild (ltbar_w);

	/* make the left (fov) scale */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, ttbar_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, ltbar_w); n++;
	XtSetArg (args[n], XmNleftOffset, 0); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, azra_w); n++;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNminimum, 1); n++;
	XtSetArg (args[n], XmNmaximum, 180*(60/FOV_STEP)); n++;
	XtSetArg (args[n], XmNprocessingDirection, XmMAX_ON_BOTTOM); n++;
	XtSetArg (args[n], XmNshowValue, False); n++;
	fov_w = XmCreateScale (svform_w, "FOVScale", args, n);
	wtip (fov_w, "Field of view scale");
	XtAddCallback (fov_w, XmNdragCallback, sv_scale_cb, (XtPointer)FOV_S);
	XtAddCallback (fov_w, XmNvalueChangedCallback, sv_scale_cb, 
							    (XtPointer)FOV_S);
	XtManageChild (fov_w);
	sr_reg (fov_w, NULL, skycategory, 0);

	/* make the right scale */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, ttbar_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, azra_w); n++;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNprocessingDirection, XmMAX_ON_TOP); n++;
	XtSetArg (args[n], XmNminimum, -1080); n++;	/* changed dynamicly */
	XtSetArg (args[n], XmNmaximum, 1080); n++;
	XtSetArg (args[n], XmNshowValue, False); n++;
	altdec_w = XmCreateScale (svform_w, "AltDecScale", args, n);
	wtip (altdec_w, "Altitude or Declination scale");
	XtAddCallback (altdec_w, XmNdragCallback, sv_scale_cb,
							(XtPointer)ALTDEC_S);
	XtAddCallback (altdec_w, XmNvalueChangedCallback, sv_scale_cb,
							(XtPointer)ALTDEC_S);
	XtManageChild (altdec_w);
	sr_reg (altdec_w, NULL, skycategory, 0);

	/* make the sky drawing area in a frame inside the sliders */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, ttbar_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, azra_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, fov_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, altdec_w); n++;
	fr_w = XmCreateFrame (svform_w, "MapF", args, n);
	XtManageChild (fr_w);

	    n = 0;
	    svda_w = XmCreateDrawingArea (fr_w, "Map", args, n);
	    XtAddCallback (svda_w, XmNexposeCallback, sv_da_exp_cb, 0);
	    XtAddCallback (svda_w, XmNinputCallback, sv_da_input_cb, 0);
	    mask = Button1MotionMask | Button2MotionMask | ButtonPressMask
				     | ButtonReleaseMask | PointerMotionMask
				     | PointerMotionHintMask
				     | EnterWindowMask | LeaveWindowMask;
	    XtAddEventHandler (svda_w, mask, False, sv_da_motion_eh, 0);
	    XtManageChild (svda_w);

	/* fill both toolbars */
	svtb_create (ttbar_w, ltbar_w);

	/* create the filter dialog.
	 * it's not managed yet, but its state info is used right off.
	 */
	svf_create(svshell_w);

	/* create the popup */
	sv_create_popup();

	/* make the options menu */
	sv_create_options ();

	/* set up user's defaults.
	 * N.B. need Options by now since sv_set_scale(FOV_S) uses wantamag_w
	 */
	sv_read_scale(AZRA_S);
	sv_set_scale(AZRA_S, 1, 1);
	sv_read_scale(ALTDEC_S);
	sv_set_scale(ALTDEC_S, 1, 1);
	sv_read_scale(FOV_S);
	sv_set_scale(FOV_S, 1, 1);

	/* now fill in the menu bar. this late because "History" pd must follow
	 * creating other stuff since it adds the first item.
	 */
	sv_fillin_mb (mb_w);

	/* init toolbar to match options */
	svtb_sync();

#if XmVersion == 1001
	/* try to connect arrow and page up/down keys to FOV initially */
	XmProcessTraversal (fov_w, XmTRAVERSE_CURRENT);
	XmProcessTraversal (fov_w, XmTRAVERSE_CURRENT); /* yes, twice!! */
#endif

#ifdef XmNinitialFocus
	/* try to connect arrow and page up/down keys to FOV initially */
	/* this approach showed up in Motif 1.2 */
	set_something (svform_w, XmNinitialFocus, (XtArgVal)fov_w);
#endif
}

static void
gridStepLabel()
{
	/* f_showit avoids blinking when updating scope position */
	f_showit (vgridl_w, want_aagrid ? "Alt: " : "Dec: ");
	f_showit (hgridl_w, want_aagrid ? "Az: "  : "RA: ");
}

/* create the Options dialog.
 */
static void
sv_create_options ()
{
	static ViewOpt vopts[] = {
	    {"Just dots",  0, "JustDots",      &justdots,        &justd_w,
	    	"Draw all star types using simple dots, not typed symbols"},
	    {"Flip Left/Right", 0, "FlipLR",   &flip_lr,         &fliplr_w,
	    	"Flip display left-to-right"},
	    {"Flip Top/Bottom",  0, "FlipTB",  &flip_tb,         &fliptb_w,
	    	"Flip display top-to-bottom"},
	    {"Equatorial plane", 0, "Equator",  &want_eq,   	 &eq_w,
	    	"Draw the Celestial Equator as a 1:2 dotted line"},
	    {"Ecliptic plane", 0, "Ecliptic",  &want_ecliptic,   &eclip_w,
	    	"Draw the Ecliptic as a 1:3 dotted line, and mark anti-solar location"},
	    {"Galactic plane", 0, "Galactic",  &want_galactic,   &galac_w,
	    	"Draw the galactic equator as a 1:4 dotted line, and mark the pole"},
	    {"Eyepieces",  0, "Eyepieces",     &want_eyep,       &wanteyep_w,
	    	"Display eyepieces in current view, if any"},
	    {"Magnitude key",  0, "MagScale",  &want_magscale,   &wantmag_w,
	    	"Display table of dot sizes vs. star magnitudes in lower corner"},
	    {"Auto magnitude", 0, "AutoMag",   &want_automag,    &wantamag_w,
                "Maintain a reasonable faint magnitude based on Field of View"},
	    {"Field stars",0, "FieldStars",    &want_fs,         &wantfs_w,
	    	"Auto load and show Field Stars when position changes)"},
	    {"Live dragging",  0, "LiveDrag",  &want_livedrag,   NULL,
		"Update scene while dragging, not just when release"},
	    {"Horizon map",   0, "HznMap",     &want_hznmap,     &hznmap_w,
	    	"Draw horizon map from Az/Alt data in xephem_hzn file"},
	};
	static ViewOpt cnsopts[] = {
	    {"Boundaries", 0, "CnsBoundaries", &want_conbounds,  &conb_w,
	    	"Draw constellation boundaries"},
	    {"Figures",    0, "CnsFigures",    &want_configures, &conf_w,
	    	"Draw constellation figures"},
	    {"Full names", 0, "CnsNames",      &want_connames,   &conn_w,
	    	"Label constellations with full name centered in figure"},
	    {"Abbreviations", 0, "CnsAbbr",    &want_conabbr,    &cona_w,
	       "Label constellations with abbreviation centered in figure"},
	};
	Widget w, f_w, rc_w;
	Widget hcrc_w;
	Arg args[20];
	XmString str;
	int i;
	int n;

	/* create form */

	n = 0;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	svops_w = XmCreateFormDialog (svshell_w, "SkyOps", args, n);
	set_something (svops_w, XmNcolormap, (XtArgVal)xe_cm);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Sky options"); n++;
	XtSetValues (XtParent(svops_w), args, n);

	/* put everything in a RC */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	rc_w = XmCreateRowColumn (svops_w, "RC", args, n);
	XtManageChild (rc_w);

	n = 0;
	w = XmCreateLabel (rc_w, "SOL", args, n);
	set_xmstring (w, XmNlabelString, "Display mode:");
	XtManageChild (w);

	/* simulate a radio boxes for alt-az/ra-dec and spherical/cylindrical */

	n = 0;
	f_w = XmCreateForm (rc_w, "AARDF", args, n);
	XtManageChild (f_w);

	    str = XmStringCreate ("Alt-Az", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    aa_w = XmCreateToggleButton (f_w, "AltAzMode", args, n);
	    XtAddCallback (aa_w, XmNvalueChangedCallback, sv_aa_cb, 0);
	    wtip (aa_w, "Orient display with Az Left-Right, Alt Up-Down");
	    XtManageChild (aa_w);
	    XmStringFree (str);
	    sr_reg (aa_w, NULL, skyocategory, 1);

	    str = XmStringCreate ("RA-Dec", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, aa_w); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    rad_w = XmCreateToggleButton(f_w, "RADecMode", args, n);
	    XtAddCallback (rad_w, XmNvalueChangedCallback, sv_aa_cb, 0);
	    wtip (rad_w, "Orient display with RA Left-Right, Dec Up-Down");
	    XtManageChild (rad_w);
	    XmStringFree (str);

	    aa_mode = XmToggleButtonGetState (aa_w);
	    XmToggleButtonSetState (rad_w, !aa_mode, False); 

	    str = XmStringCreate ("Sphere", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 50); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    sph_w = XmCreateToggleButton (f_w, "SphProj", args, n);
	    XtAddCallback (sph_w, XmNvalueChangedCallback, sv_cyl_cb, 0);
	    wtip (sph_w, "Draw using Spherical projection");
	    XtManageChild (sph_w);
	    XmStringFree (str);

	    str = XmStringCreate ("Cylinder", XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, sph_w); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 50); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    cyl_w = XmCreateToggleButton(f_w, "CylProj", args, n);
	    XtAddCallback (cyl_w, XmNvalueChangedCallback, sv_cyl_cb, 0);
	    wtip (cyl_w, "Draw using Cylindrical projection");
	    XtManageChild (cyl_w);
	    XmStringFree (str);
	    sr_reg (cyl_w, NULL, skyocategory, 1);

	    cyl_proj = XmToggleButtonGetState (cyl_w);
	    XmToggleButtonSetState (sph_w, !cyl_proj, False); 

	/* make all the "grid" controls in their own form */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (w);

	n = 0;
	w = XmCreateLabel (rc_w, "SOL", args, n);
	set_xmstring (w, XmNlabelString, "Grid Control:");
	XtManageChild (w);

	n = 0;
	f_w = XmCreateForm (rc_w, "Grid", args, n);
	XtManageChild (f_w);

	sv_create_grid_option (f_w);

	/* make the collection of options */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (w);

	n = 0;
	w = XmCreateLabel (rc_w, "SOL", args, n);
	set_xmstring (w, XmNlabelString, "View Options:");
	XtManageChild (w);

	for (i = 0; i < XtNumber(vopts); i++) {
	    ViewOpt *vp = &vopts[i];

	    str = XmStringCreate (vp->label, XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateToggleButton (rc_w, vp->name, args, n);
	    XtAddCallback (w, XmNvalueChangedCallback, sv_option_cb,
						    (XtPointer)(vp->flagp));

	    XtManageChild (w);
	    XmStringFree(str);

	    if (vp->flagp)
		*vp->flagp = XmToggleButtonGetState (w);
	    if (vp->wp)
		*vp->wp = w;
	    if (vp->tip)
		wtip (w, vp->tip);
	    sr_reg (w, NULL, skyocategory, 1);
	}

	/* add the Constellations options */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (w);

	n = 0;
	w = XmCreateLabel (rc_w, "SOL", args, n);
	set_xmstring (w, XmNlabelString, "Constellation:");
	XtManageChild (w);

	for (i = 0; i < XtNumber(cnsopts); i++) {
	    ViewOpt *vp = &cnsopts[i];

	    str = XmStringCreate (vp->label, XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateToggleButton (rc_w, vp->name, args, n);
	    XtAddCallback (w, XmNvalueChangedCallback, sv_option_cb,
						    (XtPointer)(vp->flagp));
	    XtManageChild (w);
	    *(vp->flagp) = XmToggleButtonGetState (w);
	    XmStringFree(str);

	    if (vp->wp)
		*vp->wp = w;
	    if (vp->tip)
		wtip (w, vp->tip);
	    sr_reg (w, NULL, skyocategory, 1);
	}

	/* add the Labeling controls */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (w);

	n = 0;
	w = XmCreateLabel (rc_w, "SOL", args, n);
	set_xmstring (w, XmNlabelString, "Labeling:");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNseparatorType, XmNO_LINE); n++;
	XtSetArg (args[n], XmNheight, 2); n++;
	w = XmCreateSeparator (rc_w, "LS", args, n);
	XtManageChild (w);

	sv_create_op_lbl (rc_w);

	/* the help and close buttons in its own rc so it can be centered */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNisAligned, False); n++;
	hcrc_w = XmCreateRowColumn (rc_w, "HCRC", args, n);
	XtManageChild (hcrc_w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (hcrc_w, "Help", args, n);
	    XtAddCallback (w, XmNactivateCallback, sv_helpon_cb,
							    "Sky View - ops");
	    wtip (w, "Additional details");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (hcrc_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, sv_closeopdialog_cb, 0);
	    wtip (w, "Close this dialog");
	    XtManageChild (w);
}

/* helper func to make all the "grid" controls in the given form */
static void
sv_create_grid_option (f_w)
Widget f_w;
{
	Arg args[20];
	int n;

	/* main grid on/off toggle */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	grid_w = XmCreateToggleButton (f_w, "Grid", args, n);
	XtAddCallback (grid_w, XmNvalueChangedCallback, sv_grid_cb,
						    (XtPointer)&want_grid);
	wtip (grid_w, "Overlay map with a coordinate grid");
	XtManageChild (grid_w);
	sr_reg (grid_w, NULL, skyocategory, 1);

	want_grid = XmToggleButtonGetState (grid_w);

	/* grid auto spacing on/off toggle */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 50); n++;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	autogrid_w = XmCreateToggleButton (f_w, "Auto", args, n);
	XtAddCallback (autogrid_w, XmNvalueChangedCallback, sv_grid_cb,
						(XtPointer)&want_autogrid);
	wtip (autogrid_w, "Whether to automatically set grid spacing");
	XtManageChild (autogrid_w);
	sr_reg (autogrid_w, NULL, skyocategory, 1);

	want_autogrid = XmToggleButtonGetState (autogrid_w);

	/* V grid label and text field */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, grid_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNcolumns, GRIDLEN); n++;
	XtSetArg (args[n], XmNmaxLength, GRIDLEN); n++;
	vgrid_w = XmCreateTextField (f_w, "SVGridV", args, n);
	XtAddCallback (vgrid_w, XmNactivateCallback, sv_gridtf_cb, 0);
	wtip (vgrid_w, "Vertical grid step size");
	XtManageChild (vgrid_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, grid_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	vgridl_w = XmCreateLabel (f_w, "SVGridVL", args, n);
	XtManageChild (vgridl_w);

	/* H grid label and text field */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, vgrid_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNcolumns, GRIDLEN); n++;
	XtSetArg (args[n], XmNmaxLength, GRIDLEN); n++;
	hgrid_w = XmCreateTextField (f_w, "SVGridH", args, n);
	XtAddCallback (hgrid_w, XmNactivateCallback, sv_gridtf_cb, 0);
	wtip (hgrid_w, "Horizontal grid step size");
	XtManageChild (hgrid_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, vgrid_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	hgridl_w = XmCreateLabel (f_w, "SVGridHL", args, n);
	XtManageChild (hgridl_w);

	/* fake a left radio box for grid system */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, hgrid_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	aagrid_w = XmCreateToggleButton (f_w, "AltAz", args, n);
	XtAddCallback (aagrid_w, XmNvalueChangedCallback, sv_grid_cb, 
						(XtPointer)&want_aagrid);
	wtip (aagrid_w, "Draw Alt-Az grid");
	set_xmstring (aagrid_w, XmNlabelString, "Alt-Az");
	XtManageChild (aagrid_w);
	sr_reg (aagrid_w, NULL, skyocategory, 1);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, aagrid_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	naagrid_w = XmCreateToggleButton (f_w, "RADec", args, n);
	wtip (naagrid_w, "Draw RA-Dec grid");
	XtAddCallback (naagrid_w, XmNvalueChangedCallback, sv_gt_cb, 
							(XtPointer)aagrid_w);
	set_xmstring (naagrid_w, XmNlabelString, "RA-Dec");
	XtManageChild (naagrid_w);

	want_aagrid = XmToggleButtonGetState (aagrid_w);
	XmToggleButtonSetState (naagrid_w, !want_aagrid, False);
	gridStepLabel();

	/* TB whether want labels */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, hgrid_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 50); n++;
	XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
	XtSetArg (args[n], XmNmarginHeight, 2); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	gridlbl_w = XmCreateToggleButton (f_w, "Labels", args, n);
	XtAddCallback (gridlbl_w, XmNvalueChangedCallback, sv_grid_cb, 
						    (XtPointer)&want_gridlbl);
	want_gridlbl = XmToggleButtonGetState (gridlbl_w);
	wtip (gridlbl_w, "Add labels to grid lines");
	XtManageChild (gridlbl_w);
	sr_reg (gridlbl_w, NULL, skyocategory, 1);
}

/* helper function to create the labeling controls in the Options dialog */
static void
sv_create_op_lbl (rc_w)
Widget rc_w;
{
	static ViewOpt lblnbr[] = {
	    /* these are TB/TB/Scale triples */
	    {"N",          0, "LblStName",     &lbl_lst,    &lbl_nst_w,
	    	"Whether Name is in label for stars"},
	    {"M",          0, "LblStMag",      &lbl_lst,    &lbl_mst_w,
	    	"Whether Magnitude is in label for stars"},
	    {"Stars",    0, "LblNStars",     0,      &lbl_bst_w,
	    	"Number of the brightest stars to label"},

	    {"N",          0, "LblSSName",     &lbl_lss,    &lbl_nss_w,
	    	"Whether Name is in label for Solar System objects"},
	    {"M",          0, "LblSSMag",      &lbl_lss,    &lbl_mss_w,
	    	"Whether Magnitude is in label for Solar System objects"},
	    {"Sol Sys",  0, "LblNSolSys",    0,      &lbl_bss_w,
	    	"Number of the brightest solar system objects to label"},

	    {"N",          0, "LblDSName",     &lbl_lds,    &lbl_nds_w,
	    	"Whether Name is in label for Deep Sky objects"},
	    {"M",          0, "LblDSMag",      &lbl_lds,    &lbl_mds_w,
	    	"Whether Magnitude is in label for Deep Sky objects"},
	    {"Deep Sky", 0, "LblNDeepSky",   0,      &lbl_bds_w,
	    	"Number of the brightest deep sky objects to lablel"},
	};
	Widget n_w, m_w, s_w;
	XmFontList sfl;
	Widget nl_w, ml_w;
	Widget f_w;
	Arg args[20];
	int i;
	int n;

	/* put everything in a form */

	n = 0;
	f_w = XmCreateForm (rc_w, "VOLF", args, n);
	XtManageChild (f_w);

	/* the three stacked controls */

	for (i = 0; i < XtNumber(lblnbr); /* incremented in body */ ) {
	    ViewOpt *vp;

	    vp = &lblnbr[i++];
	    n = 0;
	    if (vp == lblnbr) {
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    } else {
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
		XtSetArg (args[n], XmNtopWidget, *vp[-1].wp); n++;
	    }
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
	    XtSetArg (args[n], XmNindicatorOn, True); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNindicatorSize, LOSTBSZ); n++;
	    XtSetArg (args[n], XmNmarginWidth, 0); n++;
	    n_w = XmCreateToggleButton (f_w, vp->name, args, n);
	    XtAddCallback (n_w, XmNvalueChangedCallback, sv_lbln_cb,
							(XtPointer)vp->flagp);
	    set_xmstring (n_w, XmNlabelString, "");
	    XtManageChild (n_w);
	    *vp->wp = n_w;
	    wtip (n_w, vp->tip);
	    if (XmToggleButtonGetState (n_w))
		*(vp->flagp) |= OBJF_NLABEL;
	    sr_reg (n_w, NULL, skyocategory, 1);

	    vp = &lblnbr[i++];
	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, n_w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNleftWidget, n_w); n++;
	    XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
	    XtSetArg (args[n], XmNindicatorOn, True); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNindicatorSize, LOSTBSZ); n++;
	    XtSetArg (args[n], XmNmarginWidth, 0); n++;
	    m_w = XmCreateToggleButton (f_w, vp->name, args, n);
	    XtAddCallback (m_w, XmNvalueChangedCallback, sv_lblm_cb,
							(XtPointer)vp->flagp);
	    set_xmstring (m_w, XmNlabelString, "");
	    XtManageChild (m_w);
	    *vp->wp = m_w;
	    wtip (m_w, vp->tip);
	    if (XmToggleButtonGetState (m_w))
		*(vp->flagp) |= OBJF_MLABEL;
	    sr_reg (m_w, NULL, skyocategory, 1);

	    vp = &lblnbr[i++];
	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, n_w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNleftWidget, m_w); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	    XtSetArg (args[n], XmNminimum, 0); n++;
	    XtSetArg (args[n], XmNmaximum, MAXBRLBLS); n++;
	    XtSetArg (args[n], XmNshowValue, False); n++;
	    XtSetArg (args[n], XmNscaleMultiple, 1); n++;
	    XtSetArg (args[n], XmNscaleHeight, LOSTBSZ); n++;
	    s_w = XmCreateScale (f_w, vp->name, args, n);
	    XtAddCallback (s_w, XmNvalueChangedCallback, sv_brs_cb, 0);
	    set_xmstring (s_w, XmNtitleString, vp->label);
	    XtManageChild (s_w);
	    *vp->wp = s_w;
	    wtip (s_w, vp->tip);
	    sr_reg (s_w, NULL, skyocategory, 1);
	}

	/* last scale is hooked to bottom */

	set_something (s_w, XmNbottomAttachment, XmATTACH_FORM);

	/* add two more labels in lower left.. match Scale font */

	get_something (s_w, XmNfontList, (XtArgVal)&sfl);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, n_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, m_w); n++;
	XtSetArg (args[n], XmNfontList, sfl); n++;
	XtSetArg (args[n], XmNmarginWidth, 0); n++;
	ml_w = XmCreateLabel (f_w, "M", args, n);
	XtManageChild (ml_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, n_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, n_w); n++;
	XtSetArg (args[n], XmNfontList, sfl); n++;
	XtSetArg (args[n], XmNmarginWidth, 0); n++;
	nl_w = XmCreateLabel (f_w, "N", args, n);
	XtManageChild (nl_w);
}

/* callback from selecting the Close PB in the Options control. */
/* ARGSUSED */
static void
sv_closeopdialog_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (svops_w);
}

/* callback from selecting the Options control. */
/* ARGSUSED */
static void
sv_opdialog_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!XtIsManaged(svops_w))
	    XtManageChild (svops_w);
}

/* make the "Locate" cascade button */
static void
sv_create_find (parent)
Widget parent;
{
	Arg args[20];
	Widget pd_w, cb_w;
	Widget w;
	int n;
	int i;

	/* create pulldown managed by the cascade button */

	n = 0;
	pd_w = XmCreatePulldownMenu (parent, "PointPD", args, n);

	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pd_w); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNmnemonic, 'L'); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    cb_w = XmCreateCascadeButton (parent, "PointCB", args, n);
	    set_xmstring (cb_w, XmNlabelString, "Locate");
	    XtAddCallback (cb_w, XmNcascadingCallback, sv_finding_cb, 0);
	    wtip (cb_w, "Mark Sun, Moon, any planet or user-defined object");
	    XtManageChild (cb_w);

	    /* create the pushbuttons forming the cascade menu.
	     * go ahead and fill in and manage the planet names now.
	     * we do the user objects just as we are cascading.
	     */
	    for (i = 0; i < NOBJ; i++) {
		n = 0;
		w = XmCreatePushButton (pd_w, "PointPB", args, n);
		XtAddCallback (w, XmNactivateCallback, sv_find_cb,
								(XtPointer)i);
		if (i >= MERCURY && i <= MOON) {
		    Obj *op = db_basic (i);
		    set_xmstring (w, XmNlabelString, op->o_name);
		    XtManageChild (w);
		} else if (i == OBJX)
		    find_w[0] = w;
		else if (i == OBJY)
		    find_w[1] = w;
		else if (i == OBJZ)
		    find_w[2] = w;
	    }
}

/* callback from the main Close button */
/* ARGSUSED */
static void
sv_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* let popdown do all the work */
	XtPopdown (svshell_w);
}

/* callback from closing the main shell */
/* ARGSUSED */
static void
sv_popdown_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Display *dsp = XtDisplay(svda_w);

	XtUnmanageChild (svops_w);
	sl_unmanage ();
	svf_unmanage ();
	sf_unmanage ();
	se_unmanage ();
	svh_unmanage();

	if (sv_pm) {
	    XFreePixmap (dsp, sv_pm);
	    sv_pm = (Pixmap)0;
	}
	if (trk_pm) {
	    XFreePixmap (dsp, trk_pm);
	    trk_pm = (Pixmap)0;
	}
	if ((infifowason = want_infifo) != 0) {
	    want_infifo = 0;
	    sky_setup_infifo ();
	}
	if (fs_to != 0) {
	    XtRemoveTimeOut (fs_to);
	    fs_to = 0;
	}
}

/* callback from the overall Help button.
 */
/* ARGSUSED */
static void
sv_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg[] = {
"This displays all database objects currently in memory onto the sky. The view",
"may be Alt/Az or RA/Dec. The three sliders adjust the field of fov, the",
"azimuth (or RA), and the altitude (or Dec). Objects may be filtered out by",
"type and magnitude."
};

	hlp_dialog ("Sky View", msg, sizeof(msg)/sizeof(msg[0]));
}

/* callback from a specific Help button.
 * client is a string to use with hlp_dialog().
 */
/* ARGSUSED */
static void
sv_helpon_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	hlp_dialog ((char *)client, NULL, 0);
}

/* callback when either the Cylindrical or Spherical projection TB changes.
 * distinguish by comparing w to sph_w or cyl_w.
 */
/* ARGSUSED */
static void
sv_cyl_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *sp = (XmToggleButtonCallbackStruct *)call;
	int cyl = (w == cyl_w);
	Now *np = mm_get_now();

	/* set new state */
	cyl_proj = (cyl == sp->set);
	svtb_updateCyl (cyl_proj);

	/* implement radio box */
	if (cyl)
	    XmToggleButtonSetState (sph_w, !cyl_proj, False);
	else
	    XmToggleButtonSetState (cyl_w, cyl_proj, False);

	/* deactivate any fits image */
	sf_off();

	/* discard zoom history */
	zm_noundo();

	/* redraw */
	sv_all(np);
}

/* callback when either the alt/az or ra/dec button changes state.
 * distinguish by comparing w to aa_w or rad_w.
 */
/* ARGSUSED */
static void
sv_aa_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
#define	POLEERR		degrad(90 - 1.0/3600.0)	/* polar ambiguous area */
	XmToggleButtonCallbackStruct *sp = (XmToggleButtonCallbackStruct *)call;
	static double last_az_polar_angle = 0.0;
	static double last_ra_polar_angle = 0.0;
	int aaw = (w == aa_w);
	int wantaa = (aaw == sp->set);
	Now *np = mm_get_now();
	double alt, az, ra, dec;

	/* save the ambiguous polar angle if at the pole for later restore */
	if (fabs(sv_altdec) > POLEERR) {
	    if (aa_mode)
		last_az_polar_angle = sv_azra;
	    else
		last_ra_polar_angle = sv_azra;
	}

	/* prepare for current coords in opposite coord system */
	sv_fullwhere (np, sv_altdec, sv_azra, aa_mode, &alt, &az, &ra, &dec);

	/* engage */
	if (wantaa) {
	    /* change from ra/dec to alt/az mode */
	    sv_altdec = alt;
	    sv_azra = az;
	    aa_mode = 1;

	    /* deactivate any fits image */
	    sf_off();
	} else {
	    /* change from alt/az to ra/dec mode */
	    sv_azra = ra;
	    sv_altdec = dec;
	    aa_mode = 0;
	}

	/* restore the ambiguous polar angle if came back */
	if (fabs(sv_altdec) > POLEERR) {
	    if (aa_mode)
		sv_azra = last_az_polar_angle;
	    else
		sv_azra = last_ra_polar_angle;
	}

	/* simulate radiobox pair.
	 * N.B. only change the _other_ TB
	 */
	if (aaw)
	    XmToggleButtonSetState (rad_w, !aa_mode, False);
	else
	    XmToggleButtonSetState (aa_w, aa_mode, False);

	zm_noundo();
	sv_set_scale(ALTDEC_S, 1, 0);
	sv_set_scale(AZRA_S, 1, 0);

	/* redraw */
	if (track_op) {
	    if (sv_mark (track_op, 1, 1, 1, 1, 0, 0.0) < 0)
		sv_all(np);	/* still need it even if mark failed */
	} else 
	    sv_all(np);
}

/* callback from the filter button.
 * just bring up the filter dialog.
 */
/* ARGSUSED */
static void
sv_filter_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	svf_manage();
}

/* callback from the fits dialog button.
 * just bring up the fits dialog.
 */
/* ARGSUSED */
static void
sv_fits_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	sf_manage();
}

/* callback from the List control.
 * just bring up the list dialog.
 */
/* ARGSUSED */
static void
sv_list_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	sl_manage();
}

/* callback from the two fake radio buttons in the grid control.
 * client is the paired widget to toggle.
 */
/* ARGSUSED */
static void
sv_gt_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Widget pw = (Widget)client;

	XmToggleButtonSetState (pw, !XmToggleButtonGetState(w), True);
}

/* callback from any of the grid control buttons.
 * client is pointer to want_* flag effected.
 * redrawing all keeps the overlays in good order.
 */
/* ARGSUSED */
static void
sv_grid_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int state = XmToggleButtonGetState(w);
	int *wantp = (int *)client;

	/* update state flag and inform toolbar */
	*wantp = state;
	svtb_updateGrid(want_grid);

	/* implement the fake radio box */
	if (w == aagrid_w)
	    XmToggleButtonSetState (naagrid_w, !state, False);

	/* no need to actually redraw if already off or just turning off auto */
	if (wantp != &want_grid && !want_grid)
	    return;
	if (wantp == &want_autogrid && !want_autogrid)
	    return;

	sv_all(mm_get_now());
}

/* callback from any of the N Brightest scale.
 */
/* ARGSUSED */
static void
sv_brs_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	sv_all(mm_get_now());
}

/* callback from RETURN in either grid size TF */
/* ARGSUSED */
static void
sv_gridtf_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* turns off Auto as a side effect */
	XmToggleButtonSetState (autogrid_w, want_autogrid = False, True);

	if (want_grid)
	    sv_all(mm_get_now());
}

/* set up whether to accept pointing coords from xephem_in_fifo based on
 * want_infifo flag.
 */
static void
sky_setup_infifo ()
{
	static char fn[256];		/* fifo name; static for skyinfifo_cb */
	static XtInputId infifo_xid;	/* input id for xephem_in_fifo */
	static int infifo_fd = -1;	/* file descriptor for xephem_in_fifo */

	if (want_infifo) {
	    /* though non-POSIX we open for read/write. this lets open
	     * will never block, that reads (and hence select()) WILL block
	     * if it's empty, and let's processes using it come and go as
	     * they please.
	     */
	    if (fn[0] == '\0')
		(void) sprintf(fn,"%s/fifos/xephem_in_fifo", getShareDir());
	    infifo_fd = openh (fn, 2);
	    if (infifo_fd < 0) {
		char msg[256];
		(void) sprintf (msg, "%s: %s", fn, syserrstr());
		xe_msg (msg, 1);
		XmToggleButtonSetState (infifo_w, False, True);
	    } else
		infifo_xid = XtAppAddInput (xe_app, infifo_fd,
				    (XtPointer)XtInputReadMask, skyinfifo_cb,
								(XtPointer)fn);
	} else {
	    if (infifo_fd >= 0) {
		(void) close (infifo_fd);
		infifo_fd = -1;
	    }
	    if (infifo_xid) {
		XtRemoveInput (infifo_xid);
		infifo_xid = 0;
	    }
	    if (sv_ison())
		sv_all (mm_get_now()); /* erase if up */
	}

}

/* callback from the "In FIFO" toggle button.
 */
/* ARGSUSED */
static void
sv_in_fifo_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	want_infifo = XmToggleButtonGetState(w);
	sky_setup_infifo ();
}

/* called whenever there is input waiting from the xephem_in_fifo.
 * we gulp a lot and use last whole line we see that looks reasonable.
 * N.B. do EXACTLY ONE read -- don't know that more won't block.
 */
/* ARGSUSED */
static void
skyinfifo_cb (client, fdp, idp)
XtPointer client;       /* file name */
int *fdp;               /* pointer to file descriptor */
XtInputId *idp;         /* pointer to input id */
{
	static char fmt[] = "RA:%lf Dec:%lf Epoch:%lf";
	char *fn = (char *)client;
	double ra, dec, y;
	char buf[1025];
	char msg[1024];
	char *bp;
	int nr;

	/* one read -- with room for EOS later */
	nr = read (*fdp, buf, sizeof(buf)-1);
	if (nr < 0) {
	    (void) sprintf (msg, "%s: %s", fn, syserrstr());
	    xe_msg (msg, 1);
	    return;
	}
	if (nr == 0) {
	    /* EOF on a R/W open must mean it's really a plain file.  */
	    (void) sprintf (msg, "%s: Closing for EOF.", fn);
	    xe_msg (msg, 0);
	    XmToggleButtonSetState (infifo_w, False, True); /* closes all too */
	    return;
	}

	/* beware locking up if getting flooded.
	 * N.B. can change want_infifo
	 */
	XCheck(xe_app);

	/* not wanted now or not up -- oh well, suffice to have done the read */
	if (!want_infifo || !sv_ison())
	    return;

	/* look for last good line, starting back a bit */
	buf[nr] = '\0';
	for (bp = &buf[nr-20]; bp >= buf; --bp) {
	    if (sscanf (bp, fmt, &ra, &dec, &y) == 3 &&
			ra >= 0 && ra < 2*PI && dec <= PI/2 && dec >= -PI/2)
		break;
	}

	/* display if found one */
	if (bp >= buf) {
	    Now *np = mm_get_now();
	    double m;
	    Obj obj;

	    /* mark the scope location -- be simple since likely more soon */
	    zero_mem ((void *)&obj, sizeof(obj));
	    (void) strcpy (obj.o_name, "TelMark");
	    obj.o_type = FIXED;
	    obj.f_RA = (float)ra;
	    obj.f_dec = (float)dec;
	    year_mjd (y, &m);
	    obj.f_epoch = (float)m;
	    if (obj_cir (np, &obj) == 0)
		(void) sv_mark (&obj, 0, 1, 1, 0, 1, 0.0);
	} else {
	    (void) sprintf (msg, "Bogus infifo cmd:\n  %s", buf);
	    xe_msg (msg, 0);
	}
}

/* callback from the "Loc FIFO" toggle button.
 */
/* ARGSUSED */
static void
sv_loc_fifo_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	want_locfifo = XmToggleButtonGetState(w);

	if (want_locfifo) {
	    /* report if fifo is ok -- it's ok if it's not because
	     * it's retried on each attempt but we thought we'd be helpful.
	     */
	    (void) skyloc_fifo ((Obj *)0);
	} else {
	    /* erase the history list, which consists of all the PB children 
	     * of telpd_w
	     */
	    WidgetList ch;
	    Cardinal nch;
	    int i;

	    get_something (telpd_w, XmNchildren, (XtArgVal)&ch);
	    get_something (telpd_w, XmNnumChildren, (XtArgVal)&nch);
	    for (i = 0; i < (int)nch; i++)
		if (XmIsPushButton(ch[i]))
		    XtDestroyWidget (ch[i]);
	}
}

/* callback from the Print control.
 */
/* ARGSUSED */
static void
sv_print_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XPSAsk ("Sky View", sv_print);
}

/* proceed to generate a postscript file.
 * call XPSClose() when finished.
 */
static void
sv_print ()
{
	Now *np = mm_get_now();

	if (!sv_ison()) {
	    xe_msg ("Sky View must be open to print.", 1);
	    XPSClose();
	    return;
	}

	watch_cursor(1);

	/* fit view in square across the top and prepare to capture X calls*/
	if (sv_w > sv_h)
	    XPSXBegin (sv_pm, 0, 0, sv_w, sv_h, 1*72, 10*72, (int)(6.5*72));
	else {
	    int pw = (int)(72*6.5*sv_w/sv_h+.5);  /* width on paper as 6.5 hi */
	    XPSXBegin (sv_pm, 0, 0, sv_w, sv_h, (int)((8.5*72-pw)/2), 10*72,pw);
	}

	/* redraw */
	sv_all (np);

	/* no more X captures */
	XPSXEnd();

	/* add some extra info */
	sv_ps_annotate (np);

	/* finished */
	XPSClose();

	watch_cursor(0);
}

static void
sv_ps_annotate (np)
Now *np;
{
	double alt, az, ra, dec;
	double fov;
	char dir[128];
	char buf[128];
	char *bp;
	char *site;
	double tmp;
	int aamode;
	int ctr = 306;	/* = 8.5*72/2 */
	int y;

	/* if displaying an image, start with filename and object name */
	if (sf_ison()) {
	    char fn[1024];

	    sf_getName (fn, buf);
	    if (fn[0] || buf[0]) {
		y = AROWY(15);
		(void) sprintf (dir, "(%s %s) %d %d cstr", fn, buf, ctr, y);
		XPSDirect (dir);
	    }
	}

	sv_getcenter (&aamode, &fov, &alt, &az, &ra, &dec);

	/* caption */
	y = AROWY(13);
	if (aa_mode) {
	    (void) strcpy (dir, "(XEphem Alt/Az Sky View");
	} else {
	    (void) sprintf (dir, "(XEphem %s %s RA/Dec Sky View",
		    pref_get (PREF_EQUATORIAL) == PREF_TOPO ? "Topocentric"
		    					    : "Geocentric",
					epoch == EOD ? "Apparent" : "Mean");
	}
	if (anytrails)
	    (void) strcat (dir, " -- Trail Times are in UTC");
	(void) sprintf (dir+strlen(dir), ") %d %d cstr", ctr, y);
	XPSDirect (dir);

	site = mm_getsite();
	if (site) {
	    y = AROWY(12);
	    (void) sprintf (dir, "(%s) %d %d cstr\n",
	    				XPSCleanStr(site,strlen(site)), ctr, y);
	    XPSDirect (dir);
	}

	/* left column */

	y = AROWY(11);
	fs_sexa (buf, radhr(ra), 2, 36000);
	for (bp = buf; *bp == ' '; bp++) continue;
	(void) sprintf (dir, "(RA:) 124 %d rstr (%s) 134 %d lstr\n", y, bp, y);
	XPSDirect (dir);

	y = AROWY(10);
	fs_sexa (buf, raddeg(dec), 3, 3600);
	for (bp = buf; *bp == ' '; bp++) continue;
	(void) sprintf (dir, "(Declination:) 124 %d rstr (%s) 134 %d lstr\n",
								y, bp, y);
	XPSDirect (dir);

	y = AROWY(9);
	if (epoch == EOD)
	    (void) strcpy (buf, "EOD");
	else {
	    mjd_year (epoch, &tmp);
	    (void) sprintf (buf, "%.2f", tmp);
	}
	(void) sprintf (dir, "(Epoch:) 124 %d rstr (%s) 134 %d lstr\n",y,buf,y);
	XPSDirect (dir);

	y = AROWY(8);
	fs_sexa (buf, raddeg(alt), 3, 3600);
	for (bp = buf; *bp == ' '; bp++) continue;
	(void) sprintf (dir, "(Altitude:) 124 %d rstr (%s) 134 %d lstr\n",
								    y, bp, y);
	XPSDirect (dir);

	y = AROWY(7);
	fs_sexa (buf, raddeg(az), 4, 3600);
	for (bp = buf; *bp == ' '; bp++) continue;
	(void) sprintf (dir, "(Azimuth:) 124 %d rstr (%s) 134 %d lstr\n",
								    y, bp, y);
	XPSDirect (dir);

	y = AROWY(6);
	fs_sexa (buf, raddeg(fov), 3, 60);
	for (bp = buf; *bp == ' '; bp++) continue;
	(void) sprintf (dir,"(Field Width:) 124 %d rstr (%s) 134 %d lstr\n",
								    y, bp, y);
	XPSDirect (dir);

	/* right column */

	y = AROWY(11);
	(void) sprintf (dir,"(Julian Date:) 460 %d rstr (%13.5f) 470 %d lstr\n",
							    y, mjd+MJD0, y);
	XPSDirect (dir);

	y = AROWY(10);
	now_lst (np, &tmp);
	fs_time (buf, tmp);
	for (bp = buf; *bp == ' '; bp++) continue;
	(void)sprintf(dir,"(Sidereal Time:) 460 %d rstr (%s) 470 %d lstr\n",
								    y, bp, y);
	XPSDirect (dir);

	y = AROWY(9);
	fs_date (buf, mjd_day(mjd));
	for (bp = buf; *bp == ' '; bp++) continue;
	(void) sprintf (dir, "(UTC Date:) 460 %d rstr (%s) 470 %d lstr\n",
								    y, bp,y);
	XPSDirect (dir);

	y = AROWY(8);
	fs_time (buf, mjd_hr(mjd));
	for (bp = buf; *bp == ' '; bp++) continue;
	(void) sprintf (dir, "(UTC Time:) 460 %d rstr (%s) 470 %d lstr\n",
								    y, bp,y);
	XPSDirect (dir);

	y = AROWY(7);
	fs_sexa (buf, raddeg(fabs(lat)), 3, 3600);
	for (bp = buf; *bp == ' '; bp++) continue;
	(void) sprintf (dir, "(Latitude:) 460 %d rstr (%s %c) 470 %d lstr\n",
						y, bp, lat < 0 ? 'S' : 'N', y);
	XPSDirect (dir);

	y = AROWY(6);
	fs_sexa (buf, raddeg(fabs(lng)), 4, 3600);
	for (bp = buf; *bp == ' '; bp++) continue;
	(void) sprintf (dir,"(Longitude:) 460 %d rstr (%s %c) 470 %d lstr\n",
						y, bp, lng < 0 ? 'W' : 'E', y);
	XPSDirect (dir);

	/* center column */

	/* show basic directions unless pole is visible */
	if (sv_altdec - sv_vfov/2 > -PI/2 && sv_altdec + sv_vfov/2 < PI/2) {
	    y = AROWY(10);
	    if (aa_mode)
		(void) sprintf (dir, "(Zenith is %s - CW is %s) %d %d cstr\n",
						flip_tb ? "down" : "up",
						flip_lr ? "left" : "right",
									ctr, y);
	    else
		(void) sprintf (dir, "(North is %s - West is %s) %d %d cstr\n",
						flip_tb ? "down" : "up",
						flip_lr ? "left" : "right",
									ctr, y);
	    XPSDirect (dir);
	}

	if (want_grid) {
	    char *hval, *vval;
	    char *nam1, *val1;
	    char *nam2, *val2;

	    hval = XmTextFieldGetString (hgrid_w);
	    vval = XmTextFieldGetString (vgrid_w);

	    if (want_aagrid) {
		nam1 = "Alt";
		nam2 = "Az";
		val1 = vval;
		val2 = hval;
	    } else {
		nam1 = "RA";
		nam2 = "Dec";
		val1 = hval;
		val2 = vval;
	    }

	    y = AROWY(9);
	    (void) sprintf (dir, "(Grid Steps:) %d %d cstr\n", ctr, y);
	    XPSDirect (dir);

	    y = AROWY(8);
	    (void) sprintf (dir,"(%s: %s) %d %d cstr\n", nam1, val1, ctr, y);
	    XPSDirect (dir);

	    y = AROWY(7);
	    (void) sprintf (dir,"(%s: %s) %d %d cstr\n", nam2, val2, ctr, y);
	    XPSDirect (dir);

	    XtFree (hval);
	    XtFree (vval);
	}

	if (want_eyep && largeyepr) {
	    char wbuf[32], *wbp, hbuf[32], *hbp;
	    char *sz;

	    fs_sexa (wbuf, raddeg(largeyepr->eyepw), 3, 60);
	    for (wbp = wbuf; *wbp == ' '; wbp++) continue;
	    fs_sexa (hbuf, raddeg(largeyepr->eyeph), 3, 60);
	    for (hbp = hbuf; *hbp == ' '; hbp++) continue;

	    y = AROWY(6);
	    sz = neyepr > 1 ? "Largest " : "";
	    if (largeyepr->eyepw == largeyepr->eyeph)
		(void) sprintf (dir, "(%sEyepiece: %s) %d %d cstr\n", sz,
								wbp, ctr, y);
	    else
		(void) sprintf (dir, "(%sEyepiece: %s x %s) %d %d cstr\n",
							sz, wbp, hbp, ctr, y);
	    XPSDirect (dir);
	}
}

/* callback when the Control->tracking tb is toggled.
 * N.B. also triggered to implement turning tracking off from the popup too.
 */
/* ARGSUSED */
static void
sv_track_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!XmToggleButtonGetState (w)) {
	    track_op = (Obj *)0;
	    XtSetSensitive (w, False);
	    sv_all (mm_get_now());	/* erase the mark */
	}
}

/* set one of the scales and its labels from the current real values.
 * also possibly reset any fits being shown or unzoom records.
 */
static void
sv_set_scale(which, nofits, cutzoom)
int which;
int nofits, cutzoom;
{
	Arg args[20];
	char buf[64];
	int a, n;

	switch (which) {
	case ALTDEC_S:
	    a = (int)floor(raddeg(sv_altdec)*(60./FOV_STEP) + 0.5);
	    XmScaleSetValue (altdec_w, a);

	    (void) strcpy (buf, aa_mode ? "Alt: " : "Dec: ");
	    fs_dm_angle (buf+5, sv_altdec);
	    set_xmstring (altdecl_w, XmNlabelString, buf);
	    break;

	case AZRA_S:
	    a = (int)floor(raddeg(sv_azra)*(60./FOV_STEP) + 0.5);
	    if (a >= 4320)	/* beware of round-up */
		a -= 4320;

	    n = 0;
	    XtSetArg (args[n], XmNvalue, a); n++;
	    XtSetArg (args[n], XmNprocessingDirection,
				aa_mode ? XmMAX_ON_RIGHT : XmMAX_ON_LEFT); n++;
	    XtSetValues (azra_w, args, n);

	    if (aa_mode) {
		(void) strcpy (buf, "Az: ");
		fs_sexa (buf+4, raddeg(sv_azra), 3, 60);
	    } else {
		(void) strcpy (buf, "RA: ");
		fs_sexa (buf+4, radhr(sv_azra), 2, 3600);
	    }
	    set_xmstring (azral_w, XmNlabelString, buf);
	    break;

	case FOV_S: {
	    char vbuf[32], hbuf[32];

	    a = (int)floor(raddeg(sv_vfov)*(60./FOV_STEP)+0.5);
	    if (a < 1)
		a = 1;
	    if (a > 180*(60/FOV_STEP))
		a = 180*(60/FOV_STEP);
	    XmScaleSetValue (fov_w, a);

	    fs_dm_angle (vbuf, sv_vfov);
	    fs_dm_angle (hbuf, sv_hfov);
	    if (strcmp (vbuf, hbuf))
		sprintf (buf, "FOV: %sW x %sH", hbuf, vbuf);
	    else
		sprintf (buf, "FOV: %s", hbuf);
	    set_xmstring (fovl_w, XmNlabelString, buf);

	    /* turn on automag when changing FOV unless user turned it off */
	    if (!user_automagoff) {
		XmToggleButtonSetState (wantamag_w, want_automag = 1, False);
		svtb_updateAutoMag (1);
		svf_automag(sv_vfov);
	    }
	    }

	    break;

	default:
	    printf ("sv_set_scale Bug! bogus which: %d\n", which);
	    exit(1);
	}

	/* check for desired resets */
	if (nofits)
	    sf_off();
	if (cutzoom) {
	    zm_cutundo();
	    svtb_zoomok(0);
	}
}

/* given a new vertical fov set sv_vfov, sv_hfov and sv_dfov.
 * N.B. might be called before first expose when we don't yet know sv_h
 * N.B. we enforce a minimum fov which matches the fov scale.
 */
static void
sv_set_fov(fov)
double fov;
{
	if (fov < degrad(FOV_STEP/60.0))
	    fov = degrad(FOV_STEP/60.0);
	if (fov > degrad(180.))
	    fov = degrad(180.);
	sv_vfov = fov;
	sv_hfov = sv_h ? sv_vfov*sv_w/sv_h : 0;
	sv_dfov = sqrt((double)(sv_hfov*sv_hfov + sv_vfov*sv_vfov));
}

/* expose event of sky view drawing area.
 * if same size just copy from pixmap, else recompute all (it's resized).
 * N.B. we set bit_gravity to ForgetGravity so we can just use Expose events.
 * N.B. we turn off backing store since we effectively do it in the pixmap.
 */
/* ARGSUSED */
static void
sv_da_exp_cb (w, client,
call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	Display *dsp = XtDisplay(svda_w);
	Window win = XtWindow(svda_w);
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

		if (svh_nhist() == 0)
		    svh_add_current();
	    }

	    /* wait for the last in the series */
	    if (e->count != 0)
		return;
	    break;
	    }
	default:
	    printf ("Unexpected svda_w event. type=%d\n", c->reason);
	    exit(1);
	}

	XGetGeometry (dsp, win, &root, &x, &y, &wid, &hei, &bw, &d);

	if (!sv_pm || wid != sv_w || hei != sv_h) {
	    if (sv_pm) {
		/* user resized, else from within */
		XFreePixmap (dsp, sv_pm);
		sv_pm = (Pixmap) NULL;
		sf_off();
	    }

	    sv_pm = XCreatePixmap (dsp, win, wid, hei, d);
	    XSetForeground (dsp, sv_gc, bg_p);
	    XFillRectangle (dsp, sv_pm, sv_gc, 0, 0, wid, hei);
	    sv_w = wid;
	    sv_h = hei;
	    sv_set_fov(sv_vfov);	/* update angular dimensions */
	    sv_set_scale(FOV_S, 0, 1);
	    zm_noundo();
	    sv_all(mm_get_now());
	} else {
	    /* same size; just copy from the pixmap */
	    sv_copy_sky();
	}
}

/* programmatic resize to [neww, newh], and always do a full update.
 * if aspectok, permit changing size to achieve aspect ratio within screen size.
 */
static void
sv_resize (neww, newh, aspectok)
int neww, newh;
int aspectok;
{
	Display *dsp = XtDisplay(svda_w);
	int maxw = 9*DisplayWidth(dsp, DefaultScreen(dsp))/10;
	Arg args[10];
	Position shx;
	int n;

	/* adjust sizes if should/can */
	if (aspectok && neww > maxw) {
	    newh = maxw*newh/neww;
	    neww = maxw;
	}

	/* see if should move left to keep neww on screen */
	n = 0;
	XtSetArg (args[n], XmNx, &shx); n++;
	XtGetValues (svshell_w, args, n);
	if (shx + neww > maxw) {
	    n = 0;
	    XtSetArg (args[n], XmNx, maxw-neww+10); n++;
	    XtSetValues (svshell_w, args, n);
	}

	/* removing the shadow pixmap tells exp_cb() this resize is internal */
	if (sv_pm) {
	    XFreePixmap (dsp, sv_pm);
	    sv_pm = (Pixmap) NULL;
	}

	/* setting new size will cause expose IF size really changes.
	 * we tell by looking for a new sv_pm.
	 */
	n = 0;
	XtSetArg (args[n], XmNwidth, neww); n++;
	XtSetArg (args[n], XmNheight, newh); n++;
	XtSetValues (svda_w, args, n);

	if (!sv_pm) {
	    /* no expose so force */
	    Window win = XtWindow(svda_w);
	    XExposeEvent e;

	    e.type = Expose;
	    e.display = dsp;
	    e.window = win;
	    e.x = e.y = 0;
	    e.width = neww;
	    e.height = newh;
	    e.count = 0;

	    XSendEvent (dsp, win, False, ExposureMask, (XEvent *)&e);
	}
}

/* draw the magnitude scale, based on the near-sky setting */
static void
sv_magscale (dsp, win)
Display *dsp;
Window win;
{
	int cw, ch;		/* w and h of typical digit */
	int x0, y0;		/* ul corner of box */
	int boxw, boxh;		/* overall box size */
	int stmag, ssmag, dsmag, magstp;
	int faintest;
	char buf[16];
	int dir, asc, dsc;
	XCharStruct all;
	GC gc;
	Obj o;
	int i;

	/* get size of a typical digit -- none have descenders */
	buf[0] = '3';	/* a typical wide digit :-) */
	XTextExtents (sv_pf, buf, 1, &dir, &asc, &dsc, &all);
	cw = all.width;
	ch = asc;	/* just numbers so no descenders */

	/* establish box height.
	 * use 3/2 vertical char spacing per symbol + 1 gap after each plus
	 * another half at the bottom for balance.
	 */
	boxh = (ch*3/2+1)*MAGBOXN + ch/2;

	/* set box width based on longest string to be displayed then add
	 * 1 cw for the dot, 1 for a gap, + 1/2 on each end 
	 */
	svf_getmaglimits (&stmag, &ssmag, &dsmag, &magstp);
	faintest = stmag;
	boxw = 0;
	for (i = 0; i < MAGBOXN; i++) {
	    int sw;

	    (void) sprintf (buf, "%d", faintest - i*magstp);
	    sw = strlen(buf)*cw;
	    if (sw > boxw)
		boxw = sw;
	}
	boxw += 3*cw;

	/* position in lower left corner with a small gap for fun */
	x0 = 5;
	y0 = sv_h - boxh - 5;

	/* draw a solid background with a box along the edge */
	XSetForeground (dsp, sv_gc, sky_p);
	XFillRectangle (dsp, win, sv_gc, x0, y0, boxw, boxh);
	PSFillRectangle (win, 1.0, x0, y0, boxw, boxh);
	XSetForeground (dsp, sv_gc, bg_p);
	XPSDrawRectangle (dsp, win, sv_gc, x0, y0, boxw-1, boxh-1);
	XSetForeground (dsp, sv_gc, fg_p);

	/* ok, draw the scale */
	o.o_type = FIXED;
	o.f_class = 'S';
	o.f_spect[0] = 'G';
	o.s_size = 0;
	obj_pickgc (&o, svda_w, &gc);
	for (i = 0; i < MAGBOXN; i++) {
	    int d, y;

	    set_smag (&o, faintest - i*magstp);
	    d = objdiam(&o);
	    y = y0 + (ch*3/2 + 1)*(i+1);
	    sv_draw_obj (dsp, win, gc, &o, x0 + cw, y - ch/2, d, 1);
	    (void) sprintf (buf, "%g", get_mag(&o));
	    XPSDrawString (dsp, win, gc, x0 + 5*cw/2, y, buf, strlen(buf));
	}
	sv_draw_obj (dsp, win, (GC)0, NULL, 0, 0, 0, 0);	/* flush */
}

/* copy the pixmap to the drawing area
 */
static void
sv_copy_sky()
{
	Display *dsp = XtDisplay (svda_w);
	Window win = XtWindow (svda_w);

	if (sv_pm) {
	    XCopyArea (dsp, sv_pm, win, sv_gc, 0, 0, sv_w, sv_h, 0, 0);
	    if (svtb_iszoomok())
		zm_draw();
	}
}

/* called when there is mouse activity over the drawing area */
/* ARGSUSED */
static void
sv_da_motion_eh (w, client, ev, continue_to_dispatch)
Widget w;
XtPointer client;
XEvent *ev;
Boolean *continue_to_dispatch;
{
	static int inwin;
	Display *dsp = XtDisplay(w);
	Window win = XtWindow(w);
	double altdec, azra;
	Window root, child;
	int rx, ry, wx, wy;
	unsigned mask;
	int evt = ev->type;
	int mo, bp, br, en, lv;
	int m1, b1p, b1r;
	int m2, b2p, b2r;
	int inside;
	int drg;

	/* what happened? */
	en  = evt == EnterNotify;
	lv  = evt == LeaveNotify;
	mo  = evt == MotionNotify;
	bp  = evt == ButtonPress;
	br  = evt == ButtonRelease;
	m1  = mo && ev->xmotion.state  == Button1Mask;
	m2  = mo && ev->xmotion.state  == Button2Mask;
	b1p = bp && ev->xbutton.button == Button1;
	b1r = br && ev->xbutton.button == Button1;
	b2p = bp && ev->xbutton.button == Button2;
	b2r = br && ev->xbutton.button == Button2;
	drg = (b2p || m2 || b2r) && !sf_ison();

	if (en) inwin = 1;
	if (lv) inwin = 0;

	/* where are we? */
	XQueryPointer (dsp, win, &root, &child, &rx, &ry, &wx, &wy, &mask);
	inside = inwin && sv_unloc (wx, wy, &altdec, &azra);

	if (drg && !sf_ison()) {
	    /* handle cursor-based dragging */

	    if (inside) {
		static int lastx, lasty;
		static Cursor hc;

		if (!hc)
		    hc = XCreateFontCursor (dsp, XC_fleur);

		if (b2p) {
		    lastx = wx;
		    lasty = wy;
		    XDefineCursor (dsp, win, hc);
		}

		if (b2r || (want_livedrag && m2)) {
		    /* compute new center */
		    double dar;

		    sv_altdec -= sv_vfov*(lasty - wy)/sv_h;
		    if (sv_altdec > PI/2)
			sv_altdec = PI/2;
		    if (sv_altdec < -PI/2)
			sv_altdec = -PI/2;
		    if (aa_mode && sv_altdec < 0)
			sv_altdec = 0;

		    dar = sv_vfov*(wx - lastx)/sv_h;
		    if (fabs(altdec) < PI/2)
			dar /= cos(altdec);
		    sv_azra -= aa_mode ? dar : -dar;
		    range (&sv_azra, 2*PI);

		    lastx = wx;
		    lasty = wy;

		    /* update scales, redraw */
		    sv_set_scale (ALTDEC_S, 1, 1);
		    sv_set_scale (AZRA_S, 1, 1);
		    sv_all(mm_get_now());
		}
	    }

	    if (b2r)
		XUndefineCursor (dsp, win);
	} else {

	    /* let user draw rectangle around new area of interest.
	     * too confusing with image mag glass too
	     */
	    if (b1p && !sf_ison()) {
		/* erase current, if any */
		if (svtb_iszoomok())
		    zm_draw();

		/* discard subsequent undos, init new location, show hope */
		zm_cutundo();
		zm_x0 = zm_x1 = wx;
		zm_y0 = zm_y1 = wy;
		svtb_zoomok(1);
	    }

	    if (m1 && !sf_ison()) {
		/* erase old, draw new */
		zm_draw();
		zm_x1 = wx;
		zm_y1 = wy;
		zm_draw();
	    }

	    /* display the tracking stuff in the corners */

	    if (inside) {
		/* coords, and maybe glass */
		sv_draw_track_coords (b1p, m1, altdec, azra);
		if ((b1p || m1) && sf_ison())
		    sf_doGlass (dsp, win, b1p, m1, sf_elf ? flip_lr : !flip_lr,
					sf_nup ? flip_tb : !flip_tb, wx, wy);
	    }

	    if (b1r || !inside) {
		/* clean off the glass or tracking stuff */
		if (sf_ison())
		    sv_copy_sky();
		else
		    sv_erase_track_coords ();
	    }

	    if (b1r && !sf_ison()) {
		/* no box no zoom */
		svtb_zoomok (zm_x0 != zm_x1 && zm_y0 != zm_y1);
	    }
	}
}

/* a dot has been picked:
 * find what it is and report stuff about it in a popup.
 */
/* ARGSUSED */
static void
sv_da_input_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
#define	PICKRANGE	10	/* dist allowed from pointer */
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;
	double csep, maxcsep;	/* cos(separation) and max so far */
	double altdec, azra;	/* cursor location */
	double saltdec, caltdec;/* sin/cos altdec */
	double ad, ar;		/* candidate location */
	TSky *mintsp = NULL;	/* set to a TSky if find one close */
	Obj *minop = NULL;	/* set to an Obj if find one closer */
	int wx, wy;		/* window coords of cursor */
	DBScan dbs;
	XEvent *ev;
	TrailObj *top;
	int x, y;
	Obj *op;
	int i;

	/* confirm button 3 press event */
	if (c->reason != XmCR_INPUT)
	    return;
	ev = c->event;
	if (ev->xany.type != ButtonPress || ev->xbutton.button != 3)
	    return;

	/* find world coord of cursor -- don't proceed if outside the svda_w */
	wx = ev->xbutton.x;
	wy = ev->xbutton.y;
	if (!sv_unloc (wx, wy, &altdec, &azra))
	    return;

	/* ok, own up to some work in progress */
	watch_cursor(1);

	/* find object with largest cos from current pos */
	caltdec = cos(altdec);
	saltdec = sin(altdec);
	maxcsep = 0.0;

	/* search the trailed stuff first */
	for (top = trailobj; top; top = top->ntop) {
	    if (!top->on)
		continue;
	    for (i = 0; i < top->nsky; i++) {
		op = &top->sky[i].o;
		ad = aa_mode ? op->s_alt : op->s_dec;
		ar = aa_mode ? op->s_az  : op->s_ra;
		csep = saltdec*sin(ad) + caltdec*cos(ad)*cos(azra-ar);
		if (csep > maxcsep) {
		    mintsp = &top->sky[i];
		    maxcsep = csep;
		}
	    }
	}

	/* search the database too -- might be something closer */
	for (db_scaninit(&dbs, ALLM, want_fs ? fldstars : NULL, nfldstars);
					    (op = db_scan (&dbs)) != NULL; ) {
	    if (!(op->o_flags & OBJF_ONSCREEN))
		continue;
	    ad = aa_mode ? op->s_alt : op->s_dec;
	    ar = aa_mode ? op->s_az  : op->s_ra;
	    csep = saltdec*sin(ad) + caltdec*cos(ad)*cos(azra-ar);
	    if (csep > maxcsep) {
		minop = op;
		maxcsep = csep;
	    }
	}

	/* show info about closest object, if any near, else generic */
	if (minop || mintsp) {
	    op = minop ? minop : &mintsp->o;
	    ad = aa_mode ? op->s_alt : op->s_dec;
	    ar = aa_mode ? op->s_az : op->s_ra;
	    if (sv_loc (ad, ar, &x, &y) && abs(x-wx) <= PICKRANGE
						    && abs(y-wy) <= PICKRANGE)
		sv_popup (ev, minop, minop ? NULL : mintsp);
	    else
		sv_popup (ev, NULL, NULL);
	} else
	    sv_popup (ev, NULL, NULL);

	watch_cursor(0);
}

/* callback when any of the scales change value.
 * client is a SCALES enum to tell us which.
 */
/* ARGSUSED */
static void
sv_scale_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmScaleCallbackStruct *s = (XmScaleCallbackStruct *)call;
	int which = (int) client;

	/* read the widget to set the new value */
	sv_read_scale (which);
	sv_set_scale (which, 1, 1);

	/* only do the map work if this is the final value or want live drag */
	if (want_livedrag || s->reason == XmCR_VALUE_CHANGED) {
	    /* update the map */
	    sv_all(mm_get_now());
	}
}

/* callback when any of the Name label TB's change state.
 * client points to a state variable that should track the new state.
 */
/* ARGSUSED */
static void
sv_lbln_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int *lp = (int *)client;

	/* update flag */
	if (XmToggleButtonGetState(w))
	    *lp |= OBJF_NLABEL;
	else
	    *lp &= ~OBJF_NLABEL;

	/* update tollbar */
	svtb_updateNames ((lbl_lst&OBJF_NLABEL) || (lbl_lss&OBJF_NLABEL)
						    || (lbl_lds&OBJF_NLABEL));

	/* redraw everything to pick up the new option state */
	sv_all(mm_get_now());
}

/* callback when any of the Magnitude label TB's change state.
 * client points to a state variable that should track the new state.
 */
/* ARGSUSED */
static void
sv_lblm_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int *lp = (int *)client;

	if (XmToggleButtonGetState(w))
	    *lp |= OBJF_MLABEL;
	else
	    *lp &= ~OBJF_MLABEL;

	/* redraw everything to pick up the new option state */
	sv_all(mm_get_now());
}

/* callback when any of the options toggle buttons change state that just
 *   causes a redraw.
 * client points to a state variable that should track the new state.
 */
/* ARGSUSED */
static void
sv_option_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int *p = (int *)client;

	/* update the flag for sure */
	*p = XmToggleButtonGetState (w);

	/* load fstars immediately, rather than wait for timer */
	if (p == &want_fs && want_fs)
	    sv_loadfs(0);

	/* update any corresponding toolbar */
	svtb_sync();

	/* capture manual user desire about automag.
	 * also, no need to redraw if just turning it off.
	 */
	if (p == &want_automag) {
	    user_automagoff = !want_automag;
	    if (want_automag)
		svf_automag(sv_vfov);
	    else
		return;
	}

	/* no need to do anything if changing live-dragging option */
	if (p == &want_livedrag)
	    return;

	/* special consideration for turning on eyepieces when there aren't
	 * any now
	 */
	if (p == &want_eyep && want_eyep) {
	    if (se_getlist (NULL) == 0)
		return;
	}

	/* special consideration for making constellation buttons work like
	 * a radio pair but ok if both off
	 */
	if (p == &want_conabbr && *p && want_connames) {
	    XmToggleButtonSetState (conn_w, False, True);	/* draws */
	    return;
	}
	if (p == &want_connames && *p && want_conabbr) {
	    XmToggleButtonSetState (cona_w, False, True);	/* draws */
	    return;
	}

	/* if flipping, tell fits to make a new pixmap, and delete undoes */
	if (p == &flip_lr || p == &flip_tb) {
	    if (sf_ison())
		sf_newPixmap (svda_w, sf_elf ? flip_lr : !flip_lr,
	    					sf_nup ? flip_tb : !flip_tb);
	    zm_noundo();
	}

	/* redraw everything to pick up the new option state */
	sv_all(mm_get_now());
}

/* called to bring up the eyepiece control dialog */
/* ARGSUSED */
static void
sv_eyep_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	se_manage();
}

/* callback when the "Find" cascade button is activated.
 * check the user-defined objects and adjust the cascade buttons accordingly.
 */
/* ARGSUSED */
static void
sv_finding_cb (wid, client, call)
Widget wid;
XtPointer client;
XtPointer call;
{
	int i;

	for (i = 0; i < 3; i++) {
	    Widget w = find_w[i];
	    Obj *op = NULL;

	    switch (i) {
	    case 0: op = db_basic(OBJX); break;
	    case 1: op = db_basic(OBJY); break;
	    case 2: op = db_basic(OBJZ); break;
	    default:
		printf ("findng Bug! bad switch(i)=%d\n", i);
		exit (1);
	    }

	    if (op->o_type == UNDEFOBJ)
		XtUnmanageChild (w);
	    else {
		set_xmstring (w, XmNlabelString, op->o_name);
		XtManageChild (w);
	    }
	}
}

/* callback when one of the "locate" cascade buttons changes state.
 * object dbidx is in client.
 */
/* ARGSUSED */
static void
sv_find_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Obj *op = db_basic((int)client);

	if (want_infifo)
	    XmToggleButtonSetState (infifo_w, False, True);
	(void) sv_mark (op, 0, 1, 1, 1, 0, 0.0);
}

/* given one of the SCALES enums, read it and compute the corresponding 
 * real value.
 */
static void
sv_read_scale (which)
int which;
{
	int i;

	switch (which) {
	case FOV_S:
	    XmScaleGetValue (fov_w, &i);
	    sv_set_fov(degrad(i/(60./FOV_STEP)));
	    break;
	case ALTDEC_S:
	    XmScaleGetValue (altdec_w, &i);
	    sv_altdec = degrad(i/(60./FOV_STEP));
	    break;
	case AZRA_S:
	    XmScaleGetValue (azra_w, &i);
	    sv_azra = degrad(i/(60./FOV_STEP));
	    break;
	default:
	    printf ("sv_read_scale Bug! bad which: %d\n", which);
	    exit (1);
	}
}

/* load fresh set of field stars if new circumstances and yet stable. 
 * base mag limit on value of near-sky mag limit.
 * we are called every FSTO to monitor for change and steady state.
 */
void
sv_loadfs(force)
int force;
{
	static double lcfov, lcazra, lcaltdec;		/* last call */
	static double lsmjd, lsfov, lsra, lsdec;	/* last shown */
	static int lsmag;
	Now *np = mm_get_now();
	double alt, az, ra, dec;
	int chging;
	int stmag, ssmag, dsmag, magstp;
	double epsfov;

	/* check some basic conditions */
	if (!sv_ison())
	    return;

	/* wait until things are not changing and meet basic requirements */
	chging = lcazra!=sv_azra || lcaltdec!=sv_altdec || lcfov!=sv_dfov;
	lcazra = sv_azra;
	lcaltdec = sv_altdec;
	lcfov = sv_dfov;
	if (!force && (chging || !want_fs || sv_dfov > degrad(MAXFSFOV)))
	    return;

	/* get current display values */
	sv_fullwhere (np, sv_altdec, sv_azra, aa_mode, &alt, &az, &ra, &dec);
	svf_getmaglimits (&stmag, &ssmag, &dsmag, &magstp);

	/* skip unless forced if nearly the same as last time displayed */
	epsfov = sv_dfov/10.;				/* "small" fov change */
	if (!force  && fabs(lsdec-dec) < epsfov		/* small move */
		    && fabs(lsfov-sv_dfov) < epsfov	/* small move */
		    && delra(lsra-ra)*cos(dec) < epsfov /* small zoom */
		    && lsmag >= stmag			/* brighter limit */
		    && (!fs_pmon() || fabs(lsmjd-mjd)<MAXPMOK)) /*small propmo*/
	    return;

	/* remove any existing stars */
	if (fldstars) {
	    free ((void *)fldstars);
	    fldstars = NULL;
	    nfldstars = 0;
	    tobj_newdb();	/* in case a FS was tracked or trailed */
	}

	/* get and record new set */
	nfldstars = fs_fetch (np, ra, dec, sv_dfov, (double)stmag, &fldstars);
	lsra = ra;
	lsdec = dec;
	lsfov = sv_dfov;
	lsmag = stmag;
	lsmjd = mjd;

	/* update the state and its PB */
	if (nfldstars < 0) {
	    /* fs_fetch() has already put up the error message */
	    XmToggleButtonSetState (wantfs_w, False, False);
	    want_fs = 0;
	    nfldstars = 0;
	} else if (nfldstars > 0) {
	    XmToggleButtonSetState (wantfs_w, True, False);
	    want_fs = 1;
	}

	/* show */
	sv_all (np);
}

/* make sure trk_pm is at least minw x minh */
static void
sv_track_pm (dsp, win, minw, minh)
Display *dsp;
Window win;
int minw, minh;
{
	Window root;
	unsigned int bw, d;
	unsigned int wid, hei;
	int x, y;

	if (trk_pm) {
	    if ((int)trk_w >= minw && (int)trk_h >= minh)
		return;
	    XFreePixmap (dsp, trk_pm);
	}

	trk_w = (unsigned int) minw;
	trk_h = (unsigned int) minh;
	XGetGeometry (dsp, win, &root, &x, &y, &wid, &hei, &bw, &d);
	trk_pm = XCreatePixmap (dsp, win, trk_w, trk_h, d);
}

/* draw all the stuff in the coord tracking areas in the corners of the drawing
 *   area.
 * if new, reset the angular reference.
 * if sep, show separations from angular reference, else absolute coords.
 * N.B. draw coords directly to svda_w, *NOT* sv_pm, so it can be erased by
 *   just copying sv_pm to svda_w again.
 */
static void
sv_draw_track_coords (new, sep, altdec, azra)
int new, sep;
double altdec, azra;
{
#define	NTLAB		10			/* total number of labels */
	static double start_altdec, start_azra;	/* angular reference */
	static int maxrw;			/* right width, never shrinks */
	Now *np = mm_get_now();
	Window win = XtWindow(svda_w);
	Display *dsp = XtDisplay(svda_w);
	double alt, az, ra, ha, dec;
	char alt_buf[64], zen_buf[64], az_buf[64];
	char ra_buf[64], ha_buf[64], dec_buf[64];
	char uatl_buf[64], matl_buf[64];
	char sep_buf[64];
	Pixel fg, bg;
	char *cns;
	char *strp[NTLAB];
	int maxlw, maxh;
	double ca;
	int asc;
	int i;

	/* save if new reference */
	if (new) {
	    start_altdec = altdec;
	    start_azra = azra;
	}

	/* fill in each string.
	 * N.B. later we assume the left set is always wider than the right
	 */
	sv_fullwhere (np, altdec, azra, aa_mode, &alt, &az, &ra, &dec);
	if (sep) {
	    double salt, saz, sra, sdec, dx;
	    sv_fullwhere (np, start_altdec, start_azra, aa_mode,
						    &salt, &saz, &sra, &sdec);
	    (void) strcpy (alt_buf, "dAlt:");
	    dx = raddeg(alt-salt);
	    if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		fs_sexa (alt_buf+5, dx, 4, 60);
	    else
		fs_sexa (alt_buf+5, dx, 4, 3600);

	    (void) strcpy (zen_buf, "dZD: ");
	    dx = raddeg(salt-alt);
	    if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		fs_sexa (zen_buf+5, dx, 4, 60);
	    else
		fs_sexa (zen_buf+5, dx, 4, 3600);

	    (void) strcpy (az_buf,  "dAz: ");
	    dx = raddeg(az-saz)+180.;
	    range (&dx, 360.0);
	    dx -= 180.;
	    if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		fs_sexa (az_buf+5, dx, 4, 60);
	    else
		fs_sexa (az_buf+5, dx, 4, 3600);

	    (void) strcpy (ra_buf,  "dRA:");
	    dx = radhr(ra-sra)+12.;
	    range (&dx, 24.0);
	    dx -= 12.;
	    if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		fs_sexa (ra_buf+4, dx, 5, 600);
	    else
		fs_sexa (ra_buf+4, dx, 5, 360000);

	    (void) strcpy (ha_buf,  "dHA:");
	    dx = radhr(sra-ra)+12.;
	    range (&dx, 24.0);
	    dx -= 12.;
	    if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		fs_sexa (ha_buf+4, dx, 5, 600);
	    else
		fs_sexa (ha_buf+4, dx, 5, 360000);

	    (void) strcpy (dec_buf, "dDec:");
	    dx = raddeg(dec-sdec);
	    if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		fs_sexa (dec_buf+5, dx, 4, 600);
	    else
		fs_sexa (dec_buf+5, dx, 4, 36000);
	} else {
	    (void) strcpy (alt_buf, "Alt:  ");
	    fs_pangle (alt_buf+6, alt);
	    (void) strcpy (zen_buf, "ZD:   ");
	    fs_pangle (zen_buf+6, PI/2 - alt);
	    (void) strcpy (az_buf,  "Az:   ");
	    fs_pangle (az_buf+6, az);
	    (void) strcpy (ra_buf,  "RA:     ");
	    fs_ra (ra_buf+7, ra);
	    radec2ha (np, ra, dec, &ha);
	    ha = radhr(ha);
	    (void) strcpy (ha_buf,  "HA:    ");
	    if (pref_get(PREF_DPYPREC) == PREF_LOPREC)
		fs_sexa (ha_buf+6, ha, 3, 600);
	    else
		fs_sexa (ha_buf+6, ha, 3, 360000);
	    (void) strcpy (dec_buf, "Dec:   ");
	    fs_prdec (dec_buf+6, dec);
	}
	cns = cns_name(cns_pick (ra, dec, epoch==EOD ? mjd : epoch));
	cns += 5;	/* skip the abbreviation */
	solve_sphere (azra - start_azra, PI/2-start_altdec, sin(altdec),
						    cos(altdec), &ca, NULL);
	(void) strcpy (sep_buf, "Sep:  ");
	fs_pangle (sep_buf+6, acos(ca));
	sprintf (uatl_buf, "Ura: %s", um_atlas (ra, dec));
	sprintf (matl_buf, "Mil: %s", msa_atlas (ra, dec));

	/* colors depend on whether we think we are drawing over the sky */
	if (cyl_proj || sv_dfov < degrad(200)) {
	    /* likely drawing over sky */
	    bg = sky_p;
	    fg = fg_p;
	} else {
	    /* likely drawing mostly over the background */
	    bg = bg_p;
	    fg = bgfg_p;
	}

	/* use the tracking font */
	XSetFont (dsp, sv_strgc, sv_tf->fid);

	/* assign strings, down left side then down right */
	strp[0] = ra_buf;
	strp[1] = ha_buf;
	strp[2] = dec_buf;
	strp[3] = sep_buf;
	strp[4] = cns;

	strp[5] = alt_buf;
	strp[6] = zen_buf;
	strp[7] = az_buf;
	strp[8] = uatl_buf;
	strp[9] = matl_buf;

	/* find largest string in each side */
	maxlw = maxh = 0;
	for (i = 0; i < NTLAB; i++) {
	    char *sp = strp[i];
	    int l = strlen (sp);
	    XCharStruct all;
	    int dir, des;

	    XTextExtents (sv_tf, sp, l, &dir, &asc, &des, &all);
	    if (i < NTLAB/2) {
		if (all.width > maxlw)
		    maxlw = all.width;
	    } else {
		if (all.width > maxrw)
		    maxrw = all.width;
	    }
	    if (asc + des > maxh)
		maxh = asc + des;
	}

	/* insure trk_pm is large enough, (re)creating if necessary.
	 * N.B. we assume the left half is always largest
	 */
	sv_track_pm (dsp, win, maxlw, ((NTLAB+1)/2)*maxh);

	/* erase trk_pm */
	XSetForeground (dsp, sv_strgc, bg);
	XFillRectangle (dsp, trk_pm, sv_strgc, 0, 0, trk_w, trk_h);

	/* draw each string -- half of set to each side */
	XSetForeground (dsp, sv_strgc, fg);
	for (i = 0; i < NTLAB; i++) {
	    char *sp = strp[i];
	    int l = strlen (sp);
	    int y = asc + ((i)%((NTLAB+1)/2))*maxh;

	    XDrawString (dsp, trk_pm, sv_strgc, 0, y, sp, l); 

	    if (i == (NTLAB+1)/2 - 1) {
		/* install left side */
		XCopyArea (dsp,trk_pm,win,sv_strgc,0,0,trk_w,trk_h,TBORD,TBORD);
		XSetForeground (dsp, sv_strgc, bg);
		XFillRectangle (dsp, trk_pm, sv_strgc, 0, 0, trk_w, trk_h);
		XSetForeground (dsp, sv_strgc, fg);
	    }
	}

	/* install right side */
	XCopyArea (dsp,trk_pm,win,sv_gc,0,0,maxrw,trk_h,sv_w-TBORD-maxrw,TBORD);
#undef NTLAB
}

/* erase the tracking coords by copying back from sv_pm */
static void
sv_erase_track_coords ()
{
	Display *dsp = XtDisplay(svda_w);
	Window win = XtWindow(svda_w);
	int x, y;

	/* beware being called after sv_pm is freed during closing */
	if (!sv_pm)
	    return;

	x = TBORD;
	y = TBORD;
	XCopyArea (dsp,sv_pm,win,sv_gc,x,y,trk_w,trk_h,x,y);

	x = sv_w-TBORD-trk_w;
	y = TBORD;
	XCopyArea (dsp,sv_pm,win,sv_gc,x,y,trk_w,trk_h,x,y);
}

/* mark the given object, possibly reaiming depending on the center flags.
 * then if newfov is not 0, change to new fov, silently enforcing limits.
 * return 0 if did all that was asked else generate a message using xe_msg(*,1)
 *   and return -1.
 * N.B. we do *not* update the s_ fields of op.
 */
static int
sv_mark (op, in_center, out_center, mark, below_msg, use_window, newfov)
Obj *op;
int in_center;	/* whether to center an object already within the fov */
int out_center;	/* whether to center an object not already within the fov */
int mark;	/* whether to mark the object if it ends up visible */
int below_msg;	/* whether to issue an xe_msg if object is below horizon */
int use_window;	/* write directly to the window, not the pixmap (for speed) */
double newfov;	/* new fov if other conditions met. leave unchanged if == 0 */
{
	double altdec, azra;
	char msg[128];
	int onscrn;
	int x, y;

	/* see if it's within the current fov.
	 * complain if it's not and we have no permission to recenter.
	 */
	altdec = aa_mode ? op->s_alt : op->s_dec;
	azra   = aa_mode ? op->s_az  : op->s_ra;
	onscrn = sv_loc (altdec, azra, &x, &y);
	if (!onscrn && !out_center) {
	    (void) sprintf (msg, "`%.*s' is outside the field of view.\n",
							    MAXNM, op->o_name);
	    xe_msg (msg, 1);
	    return (-1);
	}

	/* recenter if we can/may */
	if ((onscrn && in_center) || (!onscrn && out_center)) {
	    if (newfov != 0.0) {
		sv_set_fov (newfov);
		sv_set_scale(FOV_S, 1, 1);
	    }
	    sv_altdec = altdec;
	    sv_set_scale (ALTDEC_S, 1, 1);
	    sv_azra = azra;
	    sv_set_scale (AZRA_S, 1, 1);
	    sv_all(mm_get_now());

	    /* get window loc at new view */
	    if (sv_loc (altdec, azra, &x, &y) < 0) {
		printf ("sv_mark bug! object disappeared from view?!\n");
		exit (1);
	    } 
	}

	/* and mark if asked to.
	 * by now we are certainly visible.
	 * show mouse coords too if seems otherwise quiet.
	 */
	if (mark) {
	    Display *dsp = XtDisplay (svda_w);
	    Window win = XtWindow (svda_w);
	    Drawable d = use_window ? win : sv_pm;
	    Window root, child;
	    int rx, ry, wx, wy;
	    unsigned mask;

	    if (use_window) {
		/* erase */
		sv_copy_sky();
	    }

	    XSetForeground (dsp, sv_gc, fg_p);
	    XDrawLine (dsp, d, sv_gc, x-MARKR, y-MARKR, x+MARKR, y+MARKR);
	    XDrawLine (dsp, d, sv_gc, x+MARKR, y-MARKR, x-MARKR, y+MARKR);
	    XDrawArc (dsp, d, sv_gc, x-MARKR, y-MARKR, 2*MARKR, 2*MARKR,
								    0, 64*360);
	    if (!use_window) {
		/* update */
		sv_copy_sky();
	    }

	    XQueryPointer (dsp, win, &root, &child, &rx, &ry, &wx, &wy, &mask);
	    if (!mask && sv_unloc (wx, wy, &altdec, &azra))
		sv_draw_track_coords(0, 0, altdec, azra);
	}

	return (0);
}

/* we are called when the right mouse button is pressed because the user wants
 * to identify and possibly control an object in some way. we fill in the popup
 * with goodies and manage it.
 * three cases:
 *   1) we are called with op set and tsp == 0. this means we were called with
 *      a real db object so just use *op;
 *   2) we are called with op == 0 and tsp set. this means we were called with
 *      a trailed object and so use *tsp->op;
 *   3) we are called with neither op nor tsp set. this means we were called
 *      far from any object; compute the location and display it. also set
 *      pu.op to a static FIXED object with basic info set for possible
 *      pointing.
 * position the popup as indicated by ev and display it.
 * it goes down by itself.
 */
static void
sv_popup (ev, op, tsp)
XEvent *ev;
Obj *op;
TSky *tsp;
{
	char buf[32], buf2[64];
	int noobj;
	int llabel = 0, rlabel = 0;
	int hastrail = 0, trailon = 0;
	int track = 0;
	double jd;

	if (tsp) {
	    /* we were given a trailed Obj.
	     * pu.op is the top object, pu.tsp is tsp.
	     */
	    TrailObj *top;

	    op = &tsp->o;
	    top = tobj_find (op);
	    jd = tsp->trts.t;
	    llabel = !!(tsp->flags & OBJF_LLABEL);
	    rlabel = !!(tsp->flags & OBJF_RLABEL);
	    track = top->op == track_op;
	    hastrail = 1;
	    trailon = 1;
	    pu.op = top->op;
	    pu.tsp = tsp;
	    noobj = 0;
	} else if (op) {
	    /* not called with tsp trail -- just op.
	     * pu.op is op, pu.tsp is NULL.
	     */
	    Now *np = mm_get_now();
	    TrailObj *top = tobj_find(op);

	    jd = mjd;
	    llabel = !!(op->o_flags & OBJF_LLABEL);
	    rlabel = !!(op->o_flags & OBJF_RLABEL);
	    noobj = 0;
	    track = op == track_op;
	    hastrail = top ? 1 : 0;
	    trailon = hastrail && top->on;
	    pu.op = op;
	    pu.tsp = NULL;
	} else {
	    /* nothing -- compute from ev and use svobj
	     * pu.op and op are &svobj, pu.tsp is NULL.
	     */
	    Now *np = mm_get_now();
	    double altdec, azra, alt, az, ra, dec;
	    int wx, wy;

	    wx = ev->xbutton.x;
	    wy = ev->xbutton.y;
	    if (!sv_unloc (wx, wy, &altdec, &azra))
		return;	/* outside the window */
	    sv_fullwhere (np, altdec, azra, aa_mode, &alt, &az, &ra, &dec);
	    jd = mjd;
	    /* some will need current state, some will need definition */
	    svobj.o_type = FIXED;
	    (void) strcpy (svobj.o_name, telAnon);
	    svobj.f_RA = (float)ra;
	    svobj.f_dec = (float)dec;
	    svobj.f_epoch = (float)(epoch == EOD ? mjd : epoch);
	    svobj.s_ra = (float)ra;
	    svobj.s_dec = (float)dec;
	    svobj.s_az = (float)az;
	    svobj.s_alt = (float)alt;
	    pu.op = op = &svobj;
	    pu.tsp = NULL;
	    noobj = 1;
	}

	if (noobj) {
	    XtUnmanageChild (pu.name_w);
	} else {
	    XtManageChild (pu.name_w);
	    (void) sprintf (buf2, "%.20s", op->o_name);
	    set_xmstring (pu.name_w, XmNlabelString, buf2);
	}

	if (noobj) {
	    XtUnmanageChild (pu.desc_w);
	} else {
	    XtManageChild (pu.desc_w);
	    (void) sprintf (buf2, "Type: %.20s", obj_description(op));
	    set_xmstring (pu.desc_w, XmNlabelString, buf2);
	}

	if (!noobj && is_type (op, FIXEDM) && op->f_class != 'G' &&
							    op->f_spect[0]) {
	    (void) sprintf (buf2, "Spect: %.*s", (int)sizeof(op->f_spect),
								op->f_spect);
	    set_xmstring (pu.spect_w, XmNlabelString, buf2);
	    XtManageChild (pu.spect_w);
	} else
	    XtUnmanageChild (pu.spect_w);

	if (!noobj && op->s_size > 0) {
	    if (op->s_size < 60)
		(void) sprintf (buf2, "Size: %.1f\"", op->s_size);
	    else if (op->s_size < 3600)
		(void) sprintf (buf2, "Size: %.1f'", op->s_size/60.0);
	    else
		(void) sprintf (buf2, "Size: %.2f%c",op->s_size/3600.0,
#ifdef XK_degree
							XK_degree);
#else
							'd');
#endif /* XK_degree */
	    set_xmstring (pu.size_w, XmNlabelString, buf2);
	    XtManageChild (pu.size_w);
	} else
	    XtUnmanageChild (pu.size_w);

	if (noobj) {
	    XtUnmanageChild (pu.pa_w);
	} else {
	    double pa = get_pa(op);
	    if (pa) {
		sprintf (buf, "PA: %.0f%c E of N", raddeg(pa),
#ifdef XK_degree
							XK_degree);
#else
							'd');
#endif /* XK_degree */
		set_xmstring (pu.pa_w, XmNlabelString, buf);
		XtManageChild (pu.pa_w);
	    } else
		XtUnmanageChild (pu.pa_w);
	}

	if (noobj) {
	    XtUnmanageChild (pu.ud_w);
	} else {
	    XtManageChild (pu.ud_w);
	    fs_date (buf, mjd_day(jd));
	    (void) sprintf (buf2, "UT Date: %s", buf);
	    set_xmstring (pu.ud_w, XmNlabelString, buf2);
	}

	if (noobj) {
	    XtUnmanageChild (pu.ut_w);
	} else {
	    XtManageChild (pu.ut_w);
	    fs_time (buf, mjd_hr(jd));
	    (void) sprintf (buf2, "UT Time: %s", buf);
	    set_xmstring (pu.ut_w, XmNlabelString, buf2);
	}

	fs_ra (buf, op->s_ra);
	(void) sprintf (buf2, "RA: %s", buf);
	set_xmstring (pu.ra_w, XmNlabelString, buf2);

	fs_prdec (buf, op->s_dec);
	(void) sprintf (buf2, "Dec: %s", buf);
	set_xmstring (pu.dec_w, XmNlabelString, buf2);

	fs_pangle (buf, op->s_alt);
	(void) sprintf (buf2, "Alt: %s", buf);
	set_xmstring (pu.alt_w, XmNlabelString, buf2);

	fs_pangle (buf, op->s_az);
	(void) sprintf (buf2, "Az: %s", buf);
	set_xmstring (pu.az_w, XmNlabelString, buf2);

	if (noobj) {
	    XtUnmanageChild (pu.mag_w);
	} else {
	    XtManageChild (pu.mag_w);
	    (void) sprintf (buf2, "Mag: %.2f", get_mag(op));
	    set_xmstring (pu.mag_w, XmNlabelString, buf2);
	}

	if (tsp) {
	    XtUnmanageChild (pu.rise_w);
	    XtUnmanageChild (pu.trans_w);
	    XtUnmanageChild (pu.transalt_w);
	    XtUnmanageChild (pu.set_w);
	} else {
	    XtManageChild (pu.rise_w);
	    XtManageChild (pu.transalt_w);
	    XtManageChild (pu.set_w);
	    sv_riset (op);
	}

	if (want_locfifo)
	    XtManageChild (pu.locfifo_w);
	else
	    XtUnmanageChild (pu.locfifo_w);

	if (noobj) {
	    XtUnmanageChild (pu.label_w);
	    XtUnmanageChild (pu.assign_w);
	    XtUnmanageChild (pu.dv_w);
	    XtUnmanageChild (pu.track_w);
	    XtUnmanageChild (pu.trail_w);
	    XtUnmanageChild (pu.newtrail_w);
	} else {
	    XtManageChild (pu.label_w);
	    XmToggleButtonSetState (pu.llabel_w, llabel, False);
	    XmToggleButtonSetState (pu.rlabel_w, rlabel, False);
	    if (op->o_type == PLANET) {
		XtUnmanageChild (pu.assign_w);
		XtManageChild (pu.dv_w);
	    } else {
		XtManageChild (pu.assign_w);
		XtUnmanageChild (pu.dv_w);
	    }
	    XtManageChild (pu.track_w);
	    XmToggleButtonSetState (pu.track_w, track, False);
	    if (hastrail) {
		XtManageChild (pu.trail_w);
		XmToggleButtonSetState (pu.trail_w, trailon, False);
		set_xmstring (pu.newtrail_w, XmNlabelString, "Change Trail...");
	    } else {
		XtUnmanageChild (pu.trail_w);
		set_xmstring (pu.newtrail_w, XmNlabelString, "Create Trail...");
	    }
	    XtManageChild (pu.newtrail_w);
	}

	XmMenuPosition (pu.pu_w, (XButtonPressedEvent *)ev);
	XtManageChild (pu.pu_w);
}

/* fill in the pu.rise/trans/set_w widgets from op.
 * we are never called for trailed objects so the time is always just Now.
 */
static void
sv_riset (op)
Obj *op;
{
	Now *np = mm_get_now();
	char buf[64];
	RiseSet rs;

	dm_riset (np, op, &rs);

	(void) strcpy (buf, "Rise: ");
	dm_colFormat (np, op, &rs, RSTIME_ID, &buf[6]);
	set_xmstring (pu.rise_w, XmNlabelString, buf);

	(void) strcpy (buf, "Transit: ");
	dm_colFormat (np, op, &rs, TRTIME_ID, &buf[9]);
	set_xmstring (pu.trans_w, XmNlabelString, buf);

	(void) strcpy (buf, "Transit Alt: ");
	dm_colFormat (np, op, &rs, TRALT_ID, &buf[13]);
	set_xmstring (pu.transalt_w, XmNlabelString, buf);

	(void) strcpy (buf, "Set: ");
	dm_colFormat (np, op, &rs, SETTIME_ID, &buf[5]);
	set_xmstring (pu.set_w, XmNlabelString, buf);
}

/* create the id popup */
static void
sv_create_popup()
{
	static struct {
	    Widget *wp;
	    char *tip;
	} puw[] = {
	    { &pu.name_w, "Object name"},
	    { &pu.desc_w, "Object classification"},
	    { &pu.ud_w,   "UT Date of this information"},
	    { &pu.ut_w,   "UT Time of this information"},
            { &pu.rise_w, "Rise time today, as per time zone Preference"},
	    { &pu.trans_w,"Transit time today, as per time zone Preference"},
	    { &pu.transalt_w, "Transit altitude today"},
	    { &pu.set_w,  "Set time today, as per time zone Preference"},
	    { &pu.ra_w,   "RA of this location or object"},
	    { &pu.dec_w,  "Declination of this location or object"},
	    { &pu.alt_w,  "Local altitude: angle above horizon"},
	    { &pu.az_w,   "Local azimuth: angle E of N"},
	    { &pu.spect_w,"Spectral classification"},
	    { &pu.mag_w,  "Nominal brightness"},
	    { &pu.size_w, "Angular diameter or separation"},
	    { &pu.pa_w,   "Position angle, degs E of N"},
	};
	Arg args[20];
	XmString str;
	Widget w;
	int i, n;

	/* create the outer form */
	n = 0;
	XtSetArg (args[n], XmNisAligned, True); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	XtSetArg (args[n], XmNspacing, 0); n++;
	pu.pu_w = XmCreatePopupMenu (svda_w, "SVPopup", args, n);

	/* create the label widgets */

	for (i = 0; i < XtNumber(puw); i++) {
	    n = 0;
	    w = XmCreateLabel (pu.pu_w, "SVPopValueL", args, n);
	    *(puw[i].wp) = w;
	    XtManageChild (w);
	    wtip (w, puw[i].tip);
	}

	/* add a nice separator */
	n = 0;
	w = XmCreateSeparator (pu.pu_w, "SVSep", args, n);
	XtManageChild(w);

	/* make the command buttons */

	str = XmStringCreateLtoR ("Center", XmSTRING_DEFAULT_CHARSET);
	n = 0;
	XtSetArg (args[n], XmNlabelString, str); n++;
	w = XmCreatePushButton (pu.pu_w, "SVPopPoint", args, n);
	XtAddCallback (w, XmNactivateCallback, sv_pu_activate_cb,
							    (XtPointer)AIM);
	XtManageChild (w);
	wtip (w, "Center the Sky View at this location");
	XmStringFree (str);

	/* make the zoom cascade */

	sv_create_zoomcascade (pu.pu_w);

	/* add a nice separator */
	n = 0;
	w = XmCreateSeparator (pu.pu_w, "SVSep", args, n);
	XtManageChild(w);

	str = XmStringCreateLtoR ("Place eyepiece", XmSTRING_DEFAULT_CHARSET);
	n = 0;
	XtSetArg (args[n], XmNlabelString, str); n++;
	w = XmCreatePushButton (pu.pu_w, "SVPopEyeP", args, n);
	XtAddCallback (w, XmNactivateCallback, sv_pu_activate_cb,
							(XtPointer)EYEPIECE);
	XtManageChild (w);
	wtip (w, "Drop an eyepiece centered at this location");
	XmStringFree(str);

	str = XmStringCreateLtoR ("Set telescope", XmSTRING_DEFAULT_CHARSET);
	n = 0;
	XtSetArg (args[n], XmNlabelString, str); n++;
	pu.locfifo_w = XmCreatePushButton (pu.pu_w, "SVPopLocFifo", args, n);
	XtAddCallback (pu.locfifo_w, XmNactivateCallback, sv_pu_activate_cb,
							    (XtPointer)LOCFIFO);
	wtip (pu.locfifo_w, "Send coordinates of this location to telescope");
	XmStringFree (str);

	str=XmStringCreateLtoR("AAVSO", XmSTRING_DEFAULT_CHARSET);
	n = 0;
	XtSetArg (args[n], XmNlabelString, str); n++;
	pu.av_w = XmCreatePushButton (pu.pu_w, "SVPopAV", args, n);
	wtip (pu.av_w,"Load closest AAVSO star into Tools->AAVSO");
	XtAddCallback (pu.av_w, XmNactivateCallback, sv_pu_activate_cb,
							    (XtPointer)LOADAV);
	XmStringFree (str);
	XtManageChild (pu.av_w);

	str = XmStringCreateLtoR("Show in Data Table",XmSTRING_DEFAULT_CHARSET);
	n = 0;
	XtSetArg (args[n], XmNlabelString, str); n++;
	pu.dv_w = XmCreatePushButton (pu.pu_w, "SVPopDV", args, n);
	XtAddCallback (pu.dv_w, XmNactivateCallback, sv_pu_activate_cb,
							    (XtPointer)ADDP2DV);
	wtip (pu.dv_w, "Enable a row for this object in the Data Table");
	XmStringFree (str);

	/* make the object assignment cascade */
	sv_create_objcascade (pu.pu_w);

	/* make the label l/r casade menu */
	sv_create_labellr (pu.pu_w);

	str = XmStringCreateLtoR ("Track", XmSTRING_DEFAULT_CHARSET);
	n = 0;
	XtSetArg (args[n], XmNlabelString, str); n++;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	pu.track_w = XmCreateToggleButton (pu.pu_w, "SVPopTrack", args, n);
	XtAddCallback (pu.track_w, XmNvalueChangedCallback, sv_pu_track_cb, 0);
	wtip (pu.track_w, "Lock the Sky View center on this object after each Main Update");
	XtManageChild (pu.track_w);
	XmStringFree (str);

	/* the Trail TB: must un/manage and set state on each use */
	str = XmStringCreateLtoR ("Trail", XmSTRING_DEFAULT_CHARSET);
	n = 0;
	XtSetArg (args[n], XmNlabelString, str); n++;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	pu.trail_w = XmCreateToggleButton (pu.pu_w, "SVPopTrail", args, n);
	XtAddCallback(pu.trail_w, XmNvalueChangedCallback, sv_pu_trail_cb,NULL);
	wtip(pu.trail_w,"Whether to display an existing trail for this object");
	XmStringFree(str);

	/* add a nice separator */
	n = 0;
	w = XmCreateSeparator (pu.pu_w, "SVSep", args, n);
	XtManageChild(w);

	/* the Change Trail PB: must un/manage and set labelString each use */
	n = 0;
	pu.newtrail_w = XmCreatePushButton (pu.pu_w, "SVPopNewTrail", args, n);
	XtAddCallback (pu.newtrail_w, XmNactivateCallback, sv_pu_activate_cb,
							(XtPointer)NEWTRAIL);
	wtip (pu.newtrail_w, "Create (or change) a time-sequence trail for this object");
}

/* create the zoom cascade menu off pulldown menu pd_w */
static void
sv_create_zoomcascade (pd_w)
Widget pd_w;
{
	Widget cb_w, zpd_w, w;
	Arg args[20];
	int n;
	int i;

	n = 0;
	zpd_w = XmCreatePulldownMenu (pd_w, "ZPD", args, n);

	n = 0;
	XtSetArg (args[n], XmNsubMenuId, zpd_w); n++;
	cb_w = XmCreateCascadeButton (pd_w, "ZCB", args, n);
	set_xmstring (cb_w, XmNlabelString, " Center + Zoom");
	wtip (cb_w, "Center at this location and zoom in or out");
	XtManageChild (cb_w);

	for (i = 0; i < 6; i++) {
	    int z = 0;
	    char buf[64];

	    /* z is the numerator, 10 is the denominator of zoom-in ratio */
	    switch (i) {
	    case 0: z =  20; break;
	    case 1: z =  50; break;
	    case 2: z = 100; break;
	    case 3: z =   5; break;
	    case 4: z =   2; break;
	    case 5: z =   1; break;
	    default:
		printf ("create_zoom Bug! bad setup %d\n", i);
		exit (1);
	    }

	    if (z >= 10)
		(void) sprintf (buf, "Zoom in %dX", z/10);
	    else
		(void) sprintf (buf, "Zoom out %dX", 10/z);

	    n = 0;
	    w = XmCreatePushButton (zpd_w, "Zoom", args, n);
	    XtAddCallback (w, XmNactivateCallback, sv_pu_zoom_cb, (XtPointer)z);
	    set_xmstring (w, XmNlabelString, buf);
	    XtManageChild (w);
	}
}

/* create the obj assignment cascade menu off pulldown menu pd_w */
static void
sv_create_objcascade (pd_w)
Widget pd_w;
{
	Widget objpd_w, w;
	Arg args[20];
	int n;

	n = 0;
	objpd_w = XmCreatePulldownMenu (pd_w, "ObjPD", args, n);

	n = 0;
	XtSetArg (args[n], XmNsubMenuId, objpd_w); n++;
	pu.assign_w = XmCreateCascadeButton (pd_w, "ObjCB", args, n);
	wtip (pu.assign_w, "Assign this object to a row in the Data Table");
	set_xmstring (pu.assign_w, XmNlabelString, "Assign");

	n = 0;
	w = XmCreatePushButton (objpd_w, "PUObjX", args, n);
	set_xmstring (w, XmNlabelString, "to ObjX");
	XtAddCallback (w, XmNactivateCallback, sv_pu_activate_cb,
							(XtPointer)MK_OBJX);
	wtip (w, "Assign this to user Object X and display in Data Table");
	XtManageChild (w);

	n = 0;
	w = XmCreatePushButton (objpd_w, "PUObjY", args, n);
	set_xmstring (w, XmNlabelString, "to ObjY");
	XtAddCallback (w, XmNactivateCallback, sv_pu_activate_cb,
							(XtPointer)MK_OBJY);
	wtip (w, "Assign this to user Object Y and display in Data Table");
	XtManageChild (w);

	n = 0;
	w = XmCreatePushButton (objpd_w, "PUObjY", args, n);
	set_xmstring (w, XmNlabelString, "to ObjZ");
	XtAddCallback (w, XmNactivateCallback, sv_pu_activate_cb,
							(XtPointer)MK_OBJZ);
	wtip (w, "Assign this to user Object Z and display in Data Table");
	XtManageChild (w);
}

/* create the label l/r cascade menu off pulldown menu pd_w */
static void
sv_create_labellr (pd_w)
Widget pd_w;
{
	Widget lpd_w;
	Arg args[20];
	int n;

	n = 0;
	lpd_w = XmCreatePulldownMenu (pd_w, "LPD", args, n);

	n = 0;
	XtSetArg (args[n], XmNsubMenuId, lpd_w); n++;
	pu.label_w = XmCreateCascadeButton (pd_w, "Persistent Label",
								    args, n);
	wtip (pu.label_w, "Control how/if this object is labeled");

	n = 0;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	pu.llabel_w = XmCreateToggleButton (lpd_w, "on Left", args, n);
	XtAddCallback(pu.llabel_w, XmNvalueChangedCallback, sv_pu_label_cb,
							(XtPointer)OBJF_LLABEL);
	wtip (pu.llabel_w, "Label with name to the left of the symbol");
	XtManageChild (pu.llabel_w);

	n = 0;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	pu.rlabel_w = XmCreateToggleButton (lpd_w, "on Right", args, n);
	XtAddCallback(pu.rlabel_w, XmNvalueChangedCallback, sv_pu_label_cb,
							(XtPointer)OBJF_RLABEL);
	wtip (pu.rlabel_w, "Label with name to the right of the symbol");
	XtManageChild (pu.rlabel_w);
}

/* called when any of the popup's pushbuttons are activated.
 * (the zoom cascade is not done here though).
 * client is a code to indicate which.
 * obtain current state from pu. The current object is at pu.op. If it is on
 *   a trail that specific entry is at pu.tsp->o.
 */
/* ARGSUSED */
static void
sv_pu_activate_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int code = (int)client;
	Obj *op = pu.tsp ? &pu.tsp->o : pu.op;

	switch (code) {
	case AIM:
	    if (want_infifo)
		XmToggleButtonSetState (infifo_w, False, True);
	    (void) sv_mark (op, 1, 1, 0, 1, 0, 0.0);
	    break;

	case EYEPIECE:
	    se_add (op->s_ra, op->s_dec, op->s_alt, op->s_az);
	    XmToggleButtonSetState (wanteyep_w, True, False);
	    want_eyep = 1;
	    sv_all (mm_get_now());
	    break;

	case LOCFIFO:
	    /* send the current object to the SKYLOCFIFO. */
	    (void) skyloc_fifo (op);

	    /* add to Telescope pulldown */
	    sv_addtelpd (op);
	    break;

	case LOADAV:
	    av_load (op);
	    break;

	case ADDP2DV:
	    if (op->o_type != PLANET) {
		printf ("Bug! ADDP2DV but not a planet: %d\n", op->o_type);
		exit (1);
	    }
	    dm_newobj(op->pl.pl_code);
	    dm_update (mm_get_now(), 1);
	    break;

	case MK_OBJX:
	    if (op->o_type == PLANET)
                xe_msg ("User objects can not be of type PLANET.", 1);
	    else {
		sv_ournewobj = 1;
		obj_set (op, OBJX);
		sv_ournewobj = 0;
		break;
	    }
	    break;

	case MK_OBJY:
	    if (op->o_type == PLANET)
                xe_msg ("User objects can not be of type PLANET.", 1);
	    else {
		sv_ournewobj = 1;
		obj_set (op, OBJY);
		sv_ournewobj = 0;
		break;
	    }
	    break;

	case MK_OBJZ:
	    if (op->o_type == PLANET)
                xe_msg ("User objects can not be of type PLANET.", 1);
	    else {
		sv_ournewobj = 1;
		obj_set (op, OBJZ);
		sv_ournewobj = 0;
		break;
	    }
	    break;

	case NEWTRAIL: {
	    TrailObj *top;
	    TrState *sp;

	    sp= pu.tsp && (top = tobj_find (&pu.tsp->o)) != 0
							? &top->trs : &trstate;
	    tr_setup ("xephem Sky Trail setup", pu.op->o_name, sp, sv_mktrail,
							    (XtPointer)pu.op);

	    }
	    break;

	default:
	    printf ("sv_pu_activate_db Bug! code=%d\n", code);
	    exit (1);
	    break;
	}
}

/* called when any of the zoom cascade buttons is activated.
 * client is the zoom-in factor numerator -- the denominator is 10.
 */
/* ARGSUSED */
static void
sv_pu_zoom_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int factor = (int)client;
	double newfov = sv_vfov*10.0/factor;
	Obj *op = pu.tsp ? &pu.tsp->o : pu.op;

	if (want_infifo && newfov < sv_vfov)
	    XmToggleButtonSetState (infifo_w, False, True);
	(void) sv_mark (op, 1, 1, 0, 1, 0, newfov);
}

/* called when the Trail popup toggle button changes.
 * when called, pu.op will point to the base object (which is known to have a
 *   trail, but may not be on)
 */
/* ARGSUSED */
static void
sv_pu_trail_cb (wid, client, call)
Widget wid;
XtPointer client;
XtPointer call;
{
	TrailObj *top = tobj_find (pu.op);

	if (!top) {
	    printf ("sv_pu_trail_cb() Bug! no trail!\n");
	    exit (1);
	}

	if (XmToggleButtonGetState(wid)) {
	    /* trail is being turned on. just display it */
	    top->on = 1;
	    tobj_display_all();
	    sv_copy_sky();
	} else {
	    /* trailing is being turned off. mark it as being off.
	     * it will get discarded at the next update if it's still off.
	     * redraw sky so it disappears.
	     */
	    top->on = 0;
	    sv_all (mm_get_now());
	}
}

/* called when the Label popup toggle button changes.
 * client is one of OBJF_{L,R}LABEL.
 * we get all other context from the pu structure.
 */
/* ARGSUSED */
static void
sv_pu_label_cb (wid, client, call)
Widget wid;
XtPointer client;
XtPointer call;
{
	int set = ((XmToggleButtonCallbackStruct *)call)->set;
	int side = (int)client;
	int oside = (side & OBJF_LLABEL) ? OBJF_RLABEL : OBJF_LLABEL;
	unsigned char *flagsp;

	/* if this is a trailed item then its TSky will be in pu.tsp
	 * otherwise it is a plain db object so use pu.op.
	 */
	flagsp = pu.tsp ? &pu.tsp->flags : &pu.op->o_flags;
	if (set) {
	    *flagsp |= (side|OBJF_PERSLB);
	    *flagsp &= ~oside;
	} else
	    *flagsp &= ~(side|OBJF_PERSLB);

	sv_all (mm_get_now());
}

/* called when the Track popup toggle button changes.
 * we get all context from the pu structure.
 */
/* ARGSUSED */
static void
sv_pu_track_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (XmToggleButtonGetState (w)) {
	    track_op = pu.op;
	    XtSetSensitive (tracktb_w, True);
	    XmToggleButtonSetState (tracktb_w, True, True);
	    (void) sv_mark (track_op, 0, 0, 1, 1, 0, 0.0);
	} else
	    XmToggleButtonSetState (tracktb_w, False, True);
}

/* remove trails which are no longer turned on. */
static void
tobj_rmoff()
{
	TrailObj **topp;	/* address to be changed if we decide to 
				 * remove *topp
				 */
	TrailObj *top;		/* handy *topp */

	for (topp = &trailobj; (top = *topp) != NULL; ) {
	    if (top->on) {
		topp = &top->ntop;
	    } else {
		*topp = top->ntop;
		XtFree ((char *)top);
	    }
	}
}

/* remove the trailobj list that contains the given pointer.
 * we have to search each trail list to find the one with this pointer.
 * it might be the one on TrailObj itself or one of the older ones on
 *    the TSky list.
 * it's no big deal if op isn't really on any trail list.
 */
static void
tobj_rmobj (op)
Obj *op;
{
	TrailObj **topp;	/* address to be changed if we decide to 
				 * remove *topp
				 */
	TrailObj *top;		/* handy *topp */

	for (topp = &trailobj; (top = *topp) != NULL; ) {
	    int i;
	    if (top->op == op)
		goto out;
	    for (i = 0; i < top->nsky; i++)
		if (&top->sky[i].o == op)
		    goto out;
	    topp = &top->ntop;
	}

    out:

	if (!top)
	    return;	/* oh well */

	*topp = top->ntop;
	XtFree ((char *)top);
}

/* add a new TrailObj entry to the trailobj list for db object op.
 * make enough room in the sky array for nsky entries.
 * return a pointer to the new TrailObj.
 * (we never return if there's no memory)
 */
static TrailObj *
tobj_addobj (op, nsky)
Obj *op;
int nsky;
{
	TrailObj *top;
	int nbytes;

	/* don't forget there is inherently room for one TSky in a TrailObj */
	nbytes = sizeof(TrailObj) + (nsky-1)*sizeof(TSky);
	top = (TrailObj *) XtMalloc (nbytes);
	zero_mem ((void *)top, nbytes);
	top->nsky = nsky;	  /* though none are in use now */
	top->op = op;
	top->on = 1;

	/* link directly off trailobj -- order is unimportant */
	top->ntop = trailobj;
	trailobj = top;

	return (top);
}

/* remove each trail that refers to a db object, including field stars, no
 * longer around. also reset the tracked object if it's gone now too.
 * this is done when the db is reduced.
 */
static void
tobj_newdb()
{
	TrailObj *top;
	DBScan dbs;
	Obj *op;

	for (top = trailobj; top; ) {
	    for (db_scaninit (&dbs, ALLM, fldstars, nfldstars);
	    					(op = db_scan (&dbs)) != NULL; )
		if (top->op == op)
		    break;
	    if (op == NULL) {
		/* it's gone -- remove trail and restart */
		tobj_rmobj (top->op);
		top = trailobj;
	    } else
		top = top->ntop;
	}


	if (track_op) {
	    for (db_scaninit (&dbs, ALLM, fldstars, nfldstars);
	    					(op = db_scan (&dbs)) != NULL; )
		if (track_op == op)
		    break;
	    if (op == NULL) {
		/* it's gone */
		XmToggleButtonSetState (tracktb_w, False, False);
		XtSetSensitive (tracktb_w, False);
		track_op = NULL;
	    }
	}
}

/* find the TrailObj that contains op.
 * return NULL if don't find it.
 */
static TrailObj *
tobj_find (op)
Obj *op;
{
	TrailObj *top;

	for (top = trailobj; top; top = top->ntop) {
	    int i;
	    if (top->op == op)
		return (top);
	    for (i = 0; i < top->nsky; i++)
		if (&top->sky[i].o == op)
		    return (top);
	}

	return (NULL);
}

/* display everything in the trailobj list that is marked on onto sv_pm
 * clipped to the current window.
 */
static void
tobj_display_all()
{
	static int aatrailwarn;
	Display *dsp = XtDisplay(svda_w);
	TrailObj *top;

	/* reset trail counter -- increment if actually print any trails.
	 * this is just used when printing to know whether to include a message.
	 */
	anytrails = 0;

	for (top = trailobj; top; top = top->ntop) {
	    int x1 = 0, y1 = 0, x2, y2;
	    GC gc;
	    int i;

	    if (!top->on)
		continue;

	    obj_pickgc(top->op, svda_w, &gc);

	    /* warn about aa trails and topo ra/dec first time only */
	    if (!aatrailwarn && aa_mode && top->nsky > 1) {
		aatwarn_msg();
		aatrailwarn = 1;
	    }

	    for (i = 0; i < top->nsky; i++) {
		TSky *sp = &top->sky[i];
		Obj *op = &sp->o;

		if (sv_trailobjloc (sp, &x2, &y2)) {
		    int d = objdiam(op);

		    if (sp->flags & OBJF_PERSLB) {
			int lflags;
			if (is_deepsky (op))
			    lflags = lbl_lds;
			else if (is_ssobj (op))
			    lflags = lbl_lss;
			else
			    lflags = lbl_lst;
			draw_label (sv_pm, gc, op, lflags|sp->flags, x2, y2, d);
		    }
		}
		if (i > 0) {
		    int sx1, sy1, sx2, sy2;

		    if (segisvis (x1, y1, x2, y2, &sx1, &sy1, &sx2, &sy2)) {
			TrTS *tp = (x2==sx2 && y2==sy2) ? &sp->trts : NULL;
			TrTS *ltp = (i==1 && x1==sx1 && y1==sy1)
						? &top->sky[0].trts : NULL;
			int xwrap, ywrap;
			XSegment xs;

			xs.x1 = sx1;
			xs.y1 = sy1;
			xs.x2 = sx2;
			xs.y2 = sy2;
			split_wrap (&xs, &xwrap, &ywrap);

			if (xwrap || ywrap) {
			    tr_draw (dsp, sv_pm, gc, TICKLEN, tp, ltp,
				    &top->trs, sx1, sy1, sx2+xwrap, sy2+ywrap);
			    tr_draw (dsp, sv_pm, gc, TICKLEN, tp, ltp,
				    &top->trs, sx1-xwrap, sy1-ywrap, sx2, sy2);
			} else
			    tr_draw (dsp, sv_pm, gc, TICKLEN, tp, ltp,
					    &top->trs, sx1, sy1, sx2, sy2);
			anytrails++;
		    }
		}
		x1 = x2;
		y1 = y2;
	    }

	}

	sv_draw_obj (dsp, sv_pm, (GC)0, NULL, 0, 0, 0, 0);	/* flush */
}

static void
aatwarn_msg ()
{
	xe_msg (
"\n\
Please be aware that due to diurnal motion Alt/Az trails can be\n\
very misleading when viewed against the fixed background of stars.\n\
\n\
              This message will only appear one time",
	1);
}

/* determine if the given object is visible and within sv_w/sv_h.
 * if so, return 1 and compute the location in *xp / *yp, else return 0.
 * N.B. only call this for bona fide db objects -- *not* for objects in the
 *   TrailObj lists -- it will destroy their history.
 */
static int
sv_dbobjloc (op, xp, yp)
Obj *op;
int *xp, *yp;
{
	double altdec, azra;

	if (!sv_precheck(op))
	    return (0);

	/* remaining things need accurate s_* fields */
	db_update(op);

	if (!hznOpOk(op))
	    return(0);

	/* persistent labels are immune to faint cutoff */
	if (!(op->o_flags & OBJF_PERSLB)) {
	    int stmag, ssmag, dsmag, magstp;
	    double m = get_mag(op);

	    svf_getmaglimits (&stmag, &ssmag, &dsmag, &magstp);
	    if (m > (is_deepsky(op) ? dsmag : (is_ssobj(op) ? ssmag : stmag)))
		return(0);	/* it's not within mag range after all */
	}

	altdec = aa_mode ? op->s_alt : op->s_dec;
	azra   = aa_mode ? op->s_az  : op->s_ra;
	if (!sv_loc (altdec, azra, xp, yp))
	    return(0); 	/* it's outside the fov after all */

	return (1);	/* yup, really within r */
}

/* given a TSky find its location on a window of size sv_w/sv_h.
 * return 1 if the resulting x/y is in fact within the window, else 0 if it is
 *   outside or otherwise should not be shown now (but return the x/y anyway).
 * N.B. we take care to not change the tsp->o in any way.
 */
static int
sv_trailobjloc (tsp, xp, yp)
TSky *tsp;
int *xp, *yp;
{
	Obj *op = &tsp->o;
	double altdec, azra;

	altdec = aa_mode ? op->s_alt : op->s_dec;
	azra   = aa_mode ? op->s_az  : op->s_ra;
	return (hznOpOk(op) && sv_loc (altdec, azra, xp, yp));
}

/* do as much as possible to pre-check whether op is on screen now
 * WITHOUT computing it's actual coordinates. put another way, we are not to
 * use any s_* fields in these tests.
 * return 0 if we know it's definitely not on screen, or 1 if it might be.
 */
static int
sv_precheck (op)
Obj *op;
{
	if (op->o_type == UNDEFOBJ)
	    return(0);

	/* persistent labels are immune to faint cutoff */
	if (!(op->o_flags & OBJF_PERSLB) && !svf_filter_ok(op))
	    return(0);
	if (!sv_onscrn(op))
	    return(0);
	
	return (1);
}

/* return 1 if the object can potentially be on screen, else 0.
 * N.B. this is meant to be cheap - we only do fixed objects and we don't
 *      precess. most specifically, !! we don't use any s_* fields. !!
 */
static int
sv_onscrn (op)
Obj *op;
{
#define	DELEP	3650		/* maximum epoch difference we dare go without
				 * precessing, days
				 */
#define	MARGIN	degrad(1.0)	/* border around fov still considered "in"
				 * in spite of having not precessed.
				 */
	double ra0, dec0;	/* ra/dec of our center of view */
	Now *np;
	double r;
	int polecap;

	np = mm_get_now();

	if (!is_type (op, FIXEDM))
	    return (1);
	if (fabs (mjd - op->f_epoch) > DELEP)
	    return (1);

	if (aa_mode) {
	    /* compute ra/dec of view center; worth caching too */
	    static double last_lat = 9876, last_alt, last_az;
	    static double last_ha, last_dec0;
	    double ha, lst;

	    if(lat == last_lat && sv_altdec == last_alt && sv_azra == last_az) {
		ha = last_ha;
		dec0 = last_dec0;
	    } else {
		aa_hadec (lat, sv_altdec, sv_azra, &ha, &dec0);
		last_lat = lat;
		last_alt = sv_altdec;
		last_az = sv_azra;
		last_ha = ha;
		last_dec0 = dec0;
	    }

	    /* now_lst() already knows how to cache; others are very cheap */
	    now_lst (np, &lst);
	    ra0 = hrrad(lst) - ha;
	    range (&ra0, 2*PI);
	} else {
	    ra0 = sv_azra;
	    dec0 = sv_altdec;
	}

	r = sv_dfov/2 + MARGIN;
	polecap = fabs(op->f_dec) > PI/2 - (r + fabs(dec0));

	/* just check a surrounding "rectangular" region. */
	return (fabs(op->f_dec - dec0) < r
		&& (polecap || delra(op->f_RA-ra0)*cos(op->f_dec) < r));
}

/* compute x/y loc of a point at azra/altdec as viewed from sv_azra/sv_altdec.
 * if we are displaying a fits image, use it directly.
 * always set *x/yp, but return value is whether it is really on screen now.
 */
static int
sv_loc (altdec, azra, xp, yp)
double altdec;	/* angle up from spherical equator, such as alt or dec; rads */
double azra;	/* angle around spherical pole, such as az or ra; rads */
int *xp, *yp;	/* return X coords within sv_w/h window */
{
	if (sf_ison()) {
	    /* use FITS image */
	    double x, y;
	    FImage *fip;

	    fip = sf_getFImage();
	    if (!fip) {
		printf ("sv_loc Bug! FITS disappeared\n");
		exit (1);
	    }

	    (void) RADec2xy (fip, azra, altdec, &x, &y);
	    *xp = (int)floor (x + 0.5);
	    *yp = (int)floor (y + 0.5);
	    if (sf_elf ? flip_lr : !flip_lr)
		*xp = fip->sw - 1 - *xp;
	    if (sf_nup ? flip_tb : !flip_tb)
		*yp = fip->sh - 1 - *yp;
	} else if (cyl_proj) {
	    /* cylindrical:
	     * lines of constant altdec are horizontal,
	     * lines of constant azra are vertical.
	     * nothing shows beyond the poles.
	     */
	    double scale = sv_h/sv_vfov;
	    int fullwide = (int)(2*PI*scale+.5);
	    int halfwide = (int)(1*PI*scale+.5);

	    /* looks nice to space evenly with sv_azra in center */
	    *xp = (int)floor((sv_azra - azra)*scale + sv_w/2 + .5);
	    if (*xp < (int)sv_w/2-halfwide)
		*xp += fullwide;
	    if (*xp >= (int)sv_w/2+halfwide)
		*xp -= fullwide;
	    *yp = (int)floor((sv_altdec  - altdec)*scale + sv_h/2 + .5);

	    /* flipping */
	    if (aa_mode != flip_lr)	/* a/a opposite of r/d */
		*xp = (int)sv_w - *xp;
	    if (flip_tb)
		*yp = (int)sv_h - *yp;

	} else {
	    /* spherical */
#define	LOCEPS	(1e-6)	/* an angle too small to see on screen, rads */
	    static double last_sv_altdec = 123.0, last_sa, last_ca;
	    double a,sa,ca;	/* angle from viewpoint to pole */
	    double b,sb,cb;	/* angle from object to pole */
	    double c,sc,cc;	/* difference in polar angles of obj and vwpt */
	    double d,sd,cd;	/* angular separation of object and viewpoint */
	    double r;		/* proportion of d to desired field of view */
	    double se, ce;	/* angle between (vwpt,pole) and (vwpt,obj) */


	    a = PI/2 - sv_altdec;
	    if (sv_altdec == last_sv_altdec) {
		sa = last_sa;
		ca = last_ca;
	    } else {
		last_sv_altdec = sv_altdec;
		last_sa = sa = sin(a);
		last_ca = ca = cos(a);
	    }

	    b = PI/2 - altdec;
	    sb = sin(b);
	    cb = cos(b);

	    c = aa_mode ? azra - sv_azra : sv_azra - azra;
	    cc = cos(c);

	    cd = ca*cb + sa*sb*cc;
	    if (cd >  1.0) cd =  1.0;
	    if (cd < -1.0) cd = -1.0;
	    d = acos(cd);

	    if (d < LOCEPS) {
		*xp = sv_w/2;
		*yp = sv_h/2;
		return (1);
	    }

	    r = d/(sv_vfov/2.0);

	    sc = sin(c);
	    sd = sin(d);
	    se = sc*sb/sd;
	    *xp = (int)floor ((sv_w + sv_h*r*se)/2 + 0.5);
	    if (flip_lr)
		*xp = sv_w - *xp;
	    if (a < LOCEPS) {
		/* as viewpoint approaches N pole, e approaches PI - c */
		ce = -cc;
	    } else if (a > PI - LOCEPS) {
		/* as viewpoint approaches S pole, e approaches c */
		ce = cc;
	    } else {
		/* ok (we've already checked for small d) */
		ce = (cb - cd*ca)/(sd*sa);
	    }
	    *yp = (int)floor ((sv_h - sv_h*r*ce)/2 + 0.5);
	    if (flip_tb)
		*yp = (int)sv_h - *yp;

	    /* return false if outside the fov circle */
	    /* TODO */
	    return (d < PI/2);
	    if (d > PI/2)
		return (0);
	}

	/* return whether in the window */
	return(*xp >= 0 && *xp < (int)sv_w && *yp >= 0 && *yp < (int)sv_h);
}

/* compute azra/altdec loc of a point at x/y as viewed from sv_azra/sv_altdec.
 * if displaying a fits image, use it directly.
 * return true if x/y is valid, else 0.
 */
static int
sv_unloc (x, y, altdecp, azrap)
int x, y;	/* X coords within window */
double *altdecp;/* angle up from spherical equator, such as alt or dec; rad */
double *azrap;	/* angle around spherical pole, such as az or ra; rad */
{
	/* basic bounds check */
	if (x < 0 || x >= (int)sv_w || y < 0 || y >= (int)sv_h)
	    return (0);

	if (sf_ison()) {
	    FImage *fip;

	    fip = sf_getFImage();
	    if (!fip) {
		printf ("sv_unloc Bug! FITS disappeared\n");
		exit (1);
	    }

	    if (sf_elf ? flip_lr : !flip_lr)
		x = fip->sw - 1 - x;
	    if (sf_nup ? flip_tb : !flip_tb)
		y = fip->sh - 1 - y;
	    (void) xy2RADec (fip, (double)x, (double)y, azrap, altdecp);
	} else if (cyl_proj) {
	    double scale = sv_vfov/sv_h;

	    if (aa_mode != flip_lr)	/* a/a opposite of r/d */
		x = sv_w - x;
	    if (flip_tb)
		y = sv_h - y;

	    *altdecp = sv_altdec  - scale*(y - (int)sv_h/2);
	    *azrap = sv_azra - scale*(x - (int)sv_w/2);
	    if (*altdecp > PI/2) {
		*altdecp = PI - *altdecp;
		*azrap += PI;
	    }
	    if (*altdecp < -PI/2) {
		*altdecp = -PI - *altdecp;
		*azrap += PI;
	    }
	    range (azrap, 2*PI);
	} else {
#define	UNLOCEPS (1e-4)	/* sufficiently close to pole to not know az/ra; rads */
	    double a,sa,ca;	/* angle from viewpoint to pole */
	    double r;	/* distance from center to object, pixels */
	    double d,sd,cd;	/* distance from center to object, rads */
	    double   se,ce;	/* angle between (vwpt,pole) and (vwpt,obj) */
	    double b,sb,cb;	/* angle from object to pole */
	    double c,   cc;	/* difference in polar angles of obj and vwpt */
	    int x0, y0;	/* center of screen, pixels */


	    /* check the center -- avoids cases where r == 0 */
	    if (x == (int)sv_w/2 && y == (int)sv_h/2) {
		*altdecp = sv_altdec;
		*azrap = sv_azra;
		return (1);
	    }

	    /* undo flipping */
	    if (flip_lr)
		x = sv_w - x;
	    if (flip_tb)
		y = sv_h - y;

	    a = PI/2 - sv_altdec;
	    sa = sin(a);
	    ca = cos(a);

	    x0 = sv_w/2;
	    y0 = sv_h/2;
	    r = sqrt ((double)((x-x0)*(x-x0) + (y-y0)*(y-y0)));
	    d = r*sv_vfov/sv_h;
	    if (fabs(d) >= PI/2)
		return (0);	/* outside sphere */
	    sd = sin(d);
	    cd = cos(d);
	    ce = (y0 - y)/r;
	    se = (x - x0)/r;
	    cb = ca*cd + sa*sd*ce;
	    b = acos(cb);
	    *altdecp = PI/2 - b;

	    /* find c, the polar angle between viewpoint and object */
	    if (a < UNLOCEPS) {
		/* as viewpoint approaches N pole, c approaches PI - e */
		c = acos(-ce);
	    } else if (a > PI - UNLOCEPS) {
		/* as viewpoint approaches S pole, c approaches e */
		c = acos(ce);
	    } else if (b < UNLOCEPS || b > PI - UNLOCEPS) {
		/* as object approaches either pole, c becomes arbitary */
		c = 0.0;
	    } else {
		sb = sin(b);
		cc = (cd - ca*cb)/(sa*sb);
		if (cc < -1.0) cc = -1.0;	/* heh man, it happens */
		if (cc >  1.0) cc =  1.0;	/* yah man, it happens */
		c = acos (cc);		/* 0 .. PI; next step checks if c
					     * should be > PI
					     */
	    }
	    if (se < 0.0) 		/* if e > PI */
		c = PI + (PI - c);	/*     so is c */

	    if (aa_mode)
		*azrap = sv_azra + c;
	    else
		*azrap = sv_azra - c;
	    range (azrap, 2*PI);

#undef UNLOCEPS
	}

	return (1);
}

/* if aa
 *    altdec/azra are alt/az and return dec/ra
 * else
 *    altdec/azra are dec/ra and return alt/az
 */
static void
sv_other (altdec, azra, aa, altdecp, azrap)
double altdec, azra;
int aa;
double *altdecp, *azrap;
{
	Now *np = mm_get_now();
	double tmp;

	if (aa)
	    sv_fullwhere (np, altdec, azra, aa, &tmp, &tmp, azrap, altdecp);
	else
	    sv_fullwhere (np, altdec, azra, aa, altdecp, azrap, &tmp, &tmp);
}

/* given an altdec/azra pair (decided by aa), find all coords for our current
 * location. all values will be topocentric if we are currently in Alt/Az
 * display mode, else all values will be geocentric.
 */
static void
sv_fullwhere (np, altdec, azra, aa, altp, azp, rap, decp)
Now *np;
double altdec, azra;
int aa;
double *altp, *azp;
double *rap, *decp;
{
	double ha;
	double lst;

	now_lst (np, &lst);
	lst = hrrad(lst);

	if (aa) {
	    /* need to make the ra/dec entries */
	    *altp = altdec;
	    *azp = azra;
	    unrefract (pressure, temp, altdec, &altdec);
	    aa_hadec (lat, altdec, azra, &ha, decp);
	    *rap = lst - ha;
	    range (rap, 2*PI);
	    if (epoch != EOD)
		ap_as (np, epoch, rap, decp);
	} else {
	    /* need to make the alt/az entries */
	    double ra, dec;

	    ra = *rap = azra;
	    dec = *decp = altdec;
	    if (epoch != EOD)
		as_ap (np, epoch, &ra, &dec);
	    ha = lst - ra;
	    hadec_aa (lat, ha, dec, altp, azp);
	    refract (pressure, temp, *altp, altp);
	}
}

/* draw all visible objects */
static void
draw_allobjs (dsp, win)
Display *dsp;
Drawable win;
{
	typedef struct {
	    Obj *op;
	    int x, y, d;
	    GC gc;
	} LabelSummary;
	LabelSummary stl[MAXBRLBLS];
	LabelSummary ssl[MAXBRLBLS];
	LabelSummary dsl[MAXBRLBLS];
	int nstl = 0, nssl = 0, ndsl = 0;
	int maxstl, maxssl, maxdsl;
	DBScan dbs;
	Obj *op;
	int i;

	/* get N Brightest labels in each category -- be wary of too many */
	XmScaleGetValue (lbl_bst_w, &maxstl);
	if (maxstl > MAXBRLBLS)
	    maxstl = MAXBRLBLS;
	XmScaleGetValue (lbl_bss_w, &maxssl);
	if (maxssl > MAXBRLBLS)
	    maxssl = MAXBRLBLS;
	XmScaleGetValue (lbl_bds_w, &maxdsl);
	if (maxdsl > MAXBRLBLS)
	    maxdsl = MAXBRLBLS;

	/* go through the database and display what we want.
	 * also build the brightest object lists for labeling if desired.
	 */
	for (db_scaninit(&dbs, ALLM, want_fs ? fldstars : NULL, nfldstars);
					    (op = db_scan(&dbs)) != NULL; ) {
	    LabelSummary *lsp = 0;
	    int *nlsp = 0;
	    int *maxlsp = 0;
	    int x, y;

	    if (!sv_dbobjloc(op, &x, &y))
		op->o_flags &= ~OBJF_ONSCREEN;
	    else {
		int lflags;
		int d;
		GC gc;

		/* yup, it's really supposed to be on the screen */
		op->o_flags |= OBJF_ONSCREEN;

		/* find object's size and location, and draw symbol.
		 * N.B. we assume sv_dbobjloc() called db_update()
		 */
		d = objdiam(op);
		obj_pickgc(op, svda_w, &gc);
		sv_draw_obj (dsp, win, gc, op, x, y, d, justdots);

		/* check if want to draw label, set its list and count */
		if (is_deepsky (op)) {
		    lsp = dsl;
		    nlsp = &ndsl;
		    maxlsp = &maxdsl;
		    lflags = lbl_lds;
		} else if (is_ssobj (op)) {
		    lsp = ssl;
		    nlsp = &nssl;
		    maxlsp = &maxssl;
		    lflags = lbl_lss;
		} else {
		    lsp = stl;
		    nlsp = &nstl;
		    maxlsp = &maxstl;
		    lflags = lbl_lst;
		}

		/* always draw label if persistent */
		if (op->o_flags & OBJF_PERSLB) {
		    /* flush pending graphics so label more likely on top */
		    sv_draw_obj (dsp, win, (GC)0, NULL, 0, 0, 0, 0);
		    draw_label (win, gc, op, lflags|op->o_flags, x, y, d);
		}

		/* insert op into its list by decreasing brightness
		 * (incr mag). optimized assuming most objects are too dim.
		 */
		if (lsp) {
		    for (i = *nlsp; --i >= 0 && op->s_mag < lsp[i].op->s_mag; )
			if (i < (*maxlsp)-1)
			    lsp[i+1] = lsp[i];
		    if (++i < (*maxlsp)) {
			lsp[i].op = op;
			lsp[i].gc = gc;
			lsp[i].x  = x;
			lsp[i].y  = y;
			lsp[i].d  = d;
			if (*nlsp < (*maxlsp))
			    (*nlsp) += 1;
		    }
		}
	    }
	}

	/* flush */
	sv_draw_obj (dsp, win, (GC)0, NULL, 0, 0, 0, 0);

	/* draw the Brightest labels */
	for (i = 0; i < nstl; i++) {
	    LabelSummary *lp = &stl[i];
	    draw_label (win, lp->gc, lp->op, lbl_lst|lp->op->o_flags,
							lp->x, lp->y, lp->d);
	}
	for (i = 0; i < nssl; i++) {
	    LabelSummary *lp = &ssl[i];
	    draw_label (win, lp->gc, lp->op, lbl_lss|lp->op->o_flags,
							lp->x, lp->y, lp->d);
	}
	for (i = 0; i < ndsl; i++) {
	    LabelSummary *lp = &dsl[i];
	    draw_label (win, lp->gc, lp->op, lbl_lds|lp->op->o_flags,
							lp->x, lp->y, lp->d);
	}
}

/* choose a nice step size for about MAXGRID steps for angular range a.
 * *dp will be step size, *np will be number of steps to include full range.
 * all angles in rads.
 */
static void
niceStep (a, dp, np)
double a;
double *dp;
int *np;
{
	static int nicesecs[] = {
	       1,    2,     5,    10,    20,     30,
	      60,  120,   300,   600,  1200,   1800,
	    3600, 7200, 18000, 36000, 72000, 108000,
	    1296000 /* safety net */
	};
	double as = raddeg(a)*3600.0;
	double d;
	int i;

	for (i = 0; i < XtNumber(nicesecs); i++)
	    if ((int)floor(as/nicesecs[i]) < MAXGRID)
		break;

	d = degrad(nicesecs[i]/3600.0);
	*np = (int)ceil(a/d);
	*dp = d;
}

/* convert [x,y] to grid coords, [*adp,*arp].
 * return 1 if on-screen, else 0.
 */
static int
xy2grid (x, y, samesys, adp, arp)
int x, y;		/* X Win coords */
int samesys;		/* whether grid is in same coord system as display */
double *adp, *arp;	/* grid coords, altdec/azra */
{
	if (!sv_unloc(x, y, adp, arp))
	    return (0);
	if (!samesys)
	    sv_other (*adp, *arp, aa_mode, adp, arp);
	return (1);
}

/* convert altdec/azra grid coord to screen coord.
 * return 1 if on-screen, else 0.
 */
static int
grid2xy (ad, ar, samesys, xp, yp)
double ad, ar;		/* grid coord, altdec/azra */
int samesys;		/* whether grid is in same coord system as display */
int *xp, *yp;		/* X Win coords */
{
	if (!samesys)
	    sv_other (ad, ar, want_aagrid, &ad, &ar);
	return (sv_loc (ad, ar, xp, yp));
}

/* draw label on grid near [x,y].
 * return 0 of ok, else -1 if can't find any place visible.
 */
static int
draw_grid_label (dsp, win, gc, x, y, dx, dy, samesys, arlabel, dv, dh)
Display *dsp;
Window win;
GC gc;
int x, y;		/* initial window location */
int dx, dy;		/* -1/0/1 for each traveling direction */
int samesys;		/* whether grid is in same coord system as display */
int arlabel;		/* whether want azra or altdec label */
double dv, dh;		/* altdec and azra step sizes, rads */
{
	char buf[32];		/* coord string */
	double ad, ar;		/* grid loc at [x,y] */
	double a;		/* text rotation angle */
	int xstep, ystep;	/* change in x and y during walk */
	int x0, y0;		/* one end */
	int x1, y1;		/* other end */
	int xc, yc;		/* center pos */
	int i;

	/* break into MAXGRID steps */
	xstep = ((int)sv_w)*dx/MAXGRID;
	ystep = ((int)sv_h)*dy/MAXGRID;

	/* hunt in the step direction for first entry which is
	 * entirely visible on both ends.
	 */
	for (i = 0; i < MAXGRID; i++) {
	    x += xstep;
	    y += ystep;

	    /* see if visible at all */
	    if (!xy2grid (x, y, samesys, &ad, &ar))
		continue;

	    /* move to a near grid intersection */
	    ad = dv*floor(ad/dv);
	    ar = dh*floor(ar/dh);

	    /* if still visible, check both ends */
	    if (grid2xy (ad, ar, samesys, &x0, &y0)) {
		if (arlabel) {
		    if (grid2xy (ad+dv, ar, samesys, &x1, &y1))
			break;
		} else {
		    if (grid2xy (ad, ar+dh, samesys, &x1, &y1))
			break;
		}
	    }
	}
	if (i == MAXGRID)
	    return (-1);

	/* center label between parallels */
	a = raddeg(atan2((double)y0-y1, (double)(x1==x0?1:x1-x0)));
	if (a > 90)
	    a -= 180;
	if (a < -90)
	    a += 180;
	xc = (x0+x1)/2;
	yc = (y0+y1)/2;
	if (arlabel)
	    fs_sexa (buf, want_aagrid ? raddeg(ar) : radhr(ar), 3, 3600);
	else
	    fs_sexa (buf, raddeg(ad), 3, 3600);
	i = ((a>=0 && (xc > GMARG && yc > GMARG))
			|| (a<=0 && (xc < (int)sv_w-GMARG && yc > GMARG)))
		    ? BCENTRE : TCENTRE;
	XPSRotDrawAlignedString (dsp, sv_rf, a, 1.0, win, gc, xc, yc, buf, i);

	return (0);
}

/* draw a nice grid, with labels */
static void
draw_grid(dsp, win, gc)
Display *dsp;
Window win;
GC gc;
{
	XSegment xsegs[50], *xs;/* segments cache */
	int samesys;		/* whether grid is in same coord sys as dsp */
	double altdec, azra;	/* center in grid coords */
	double dv, dh;		/* v and h step size in grid coords */
	double pangle;		/* grid coord polar angle */
	int seepole;		/* whether can see pole in grid coord sys */
	double polegap;		/* don't crowd closer than this to pole */
	int nvt, nht;		/* num v and h steps */
	char msg[128];
	int i, j;

	/* decide whether grid is in different coord system than display mode */
	samesys = (!!want_aagrid == !!aa_mode);

	/* find center and whether pole is visible in grid system */
	if (samesys) {
	    altdec = sv_altdec;
	    azra = sv_azra;
	    seepole = sv_loc (altdec >= 0.0 ? PI/2 : -PI/2, 0.0, &i, &j);
	} else {
	    double h, v;
	    sv_other (sv_altdec, sv_azra, aa_mode, &altdec, &azra);
	    sv_other (PI/2, 0.0, want_aagrid, &v, &h);
	    seepole = sv_loc (v, h, &i, &j);
	    if (!seepole) {
		sv_other (-PI/2, 0.0, want_aagrid, &v, &h);
		seepole = sv_loc (v, h, &i, &j);
	    }
	}

	/* grid's polar angle: 2*PI if pole visible, else scales by 1/cos */
	if (seepole || fabs(altdec) >= PI/2)
	    pangle = 2*PI;
	else
	    pangle = sv_dfov/cos(altdec) * 1.2;	/* fudge */
	if (pangle > 2*PI)
	    pangle = 2*PI;

	/* pick size and number of steps, either from user or automatically */
	if (want_autogrid) {
	    if (want_aagrid)
		niceStep (pangle, &dh, &nht);
	    else {
		/* do RA in hours */
		niceStep (pangle/15.0, &dh, &nht);
		dh *= 15.0;
	    }
	    niceStep (sv_dfov, &dv, &nvt);
	} else {
	    char *str;

	    str = XmTextFieldGetString (hgrid_w);
	    f_scansex (0.0, str, &dh);
	    XtFree (str);
	    dh = want_aagrid ? degrad(dh) : hrrad(dh);
	    if (dh < pangle/MAXGRID || dh > pangle/2) {
		xe_msg ("Horizontal spacing too small or too large", 1);
		return;
	    }
	    nht = (int)floor(pangle/dh + 0.5) + 1; /* inclusive */

	    str = XmTextFieldGetString (vgrid_w);
	    f_scansex (0.0, str, &dv);
	    XtFree (str);
	    dv = degrad(dv);
	    if (dv < sv_dfov/MAXGRID || dv > sv_dfov/2) {
		xe_msg ("Vertical spacing too small or too large", 1);
		return;
	    }
	    nvt = (int)floor(sv_dfov/dv + 0.5) + 1; /* inclusive */
	}

	/* round center to nearest whole multiple of step size */
	altdec -= fmod (altdec, dv);
	azra -= fmod (azra, dh);

	/* report */
	fs_sexa (msg, raddeg(dv), 3, 3600);
	XmTextFieldSetString (vgrid_w, msg);
	fs_sexa(msg, want_aagrid ? raddeg(dh) : radhr(dh), 3, 3600);
	XmTextFieldSetString (hgrid_w, msg);
	gridStepLabel();

	/* set up max eq dist, dv down then seg size up towards PI/2 */
	polegap = dv*floor(PI/2/dv - dv/NGSEGS/2) + dv/NGSEGS/2;

	/* do the vertical lines (constant ra or az):
	 * for each horizontal tick mark
	 *   for each vertical tick mark, by NGSEGS
	 *     compute coord on screen
	 *     if we've at least 2 pts now
	 *       connect the points with what is visible within the circle.
	 */
	nht*=2;
	nvt*=2;
	for (i = -nht/2; i <= nht/2; i++) {
	    double h0 = azra + i*dh;
	    int before = 0;
	    int vis1 = 0, vis2;
	    int x1 = 0, y1 = 0, x2, y2;
	    xs = xsegs;
	    for (j = -NGSEGS*nvt/2; j <= NGSEGS*nvt/2; j++) {
		double h = h0, v = altdec + j*dv/NGSEGS;
		if (fabs(v) > polegap)
		    continue;
		if (!samesys)
		    sv_other (v, h0, want_aagrid, &v, &h);
		vis2 = sv_loc(v,h,&x2,&y2);	/* hzn done with clipping */
		if (before++ && (vis1 || vis2)) {
		    int sx1, sy1, sx2, sy2;
		    if (segisvis(x1, y1, x2, y2, &sx1, &sy1, &sx2, &sy2)) {
			xs->x1 = sx1; xs->y1 = sy1;
			xs->x2 = sx2; xs->y2 = sy2;
			if (++xs == &xsegs[XtNumber(xsegs)]) {
			    split_segs (dsp, win, gc, xsegs, xs - xsegs);
			    xs = xsegs;
			}
		    }
		}
		x1 = x2;
		y1 = y2;
		vis1 = vis2;
	    }
	    if (xs > xsegs)
		split_segs (dsp, win, gc, xsegs, xs - xsegs);
	}

	/* do the horizontal lines (constant dec or alt):
	 * for each vertical tick mark
	 *   for each horizontal tick mark, by NGSEGS
	 *     compute coord on screen
	 *     if we've at least 2 pts now
	 *       connect the points with what is visible within the circle.
	 */
	for (j = -nvt/2; j <= nvt/2; j++) {
	    double v0 = altdec + j*dv;
	    int before = 0;
	    int vis1 = 0, vis2;
	    int x1 = 0, y1 = 0, x2, y2;
	    xs = xsegs;
	    for (i = -NGSEGS*nht/2; i <= NGSEGS*nht/2; i++) {
		double v = v0, h = azra + i*dh/NGSEGS;
		if (fabs(v) > polegap)
		    continue;
		if (!samesys)
		    sv_other (v, h, want_aagrid, &v, &h);
		vis2 = sv_loc(v,h,&x2,&y2);	/* hzn down with clipping */
		if (before++ && (vis1 || vis2)) {
		    int sx1, sy1, sx2, sy2;
		    if (segisvis(x1, y1, x2, y2, &sx1, &sy1, &sx2, &sy2)) {
			xs->x1 = sx1; xs->y1 = sy1;
			xs->x2 = sx2; xs->y2 = sy2;
			if (++xs == &xsegs[XtNumber(xsegs)]) {
			    split_segs (dsp, win, gc, xsegs, xs - xsegs);
			    xs = xsegs;
			}
		    }
		}
		x1 = x2;
		y1 = y2;
		vis1 = vis2;
	    }
	    if (xs > xsegs)
		split_segs (dsp, win, gc, xsegs, xs - xsegs);
	}

	/* add labels */
	if (want_gridlbl) {
	    draw_grid_label(dsp,win,gc,   0,        0,  1,  1, samesys,1,dv,dh);
	    draw_grid_label(dsp,win,gc,7*sv_w/8, sv_h,  0, -1, samesys,1,dv,dh);
	    draw_grid_label(dsp,win,gc,sv_w,        0, -1,  1, samesys,0,dv,dh);
	    draw_grid_label(dsp,win,gc,sv_w/4,   sv_h,  0, -1, samesys,0,dv,dh);
	}
}

static void
clampShort (sp)
int *sp;
{
	if (*sp > SHRT_MAX)
	    *sp = SHRT_MAX;
	else if (*sp < SHRT_MIN)
	    *sp = SHRT_MIN;
}

/* draw the polygon defined by xpts[npts].
 * this is specifically intended for draw_hznmap().
 */
static void
hznPoly (dsp, win, gc, xpts, npts)
Display *dsp;
Window win;
GC gc;
XPoint xpts[];
int npts;
{
#if 0
{ int i;
for (i=0; i<npts; i++)
printf ("%4d: [%5d %5d]\n", i, xpts[i].x, xpts[i].y);
}
#endif
	/* polygon */
	if (XPSDrawing() && !XPSInColor()) {
	    /* use white mountains on the paper, black on screen */
	    XSetForeground (dsp, gc, WhitePixel (dsp, DefaultScreen (dsp)));
	    XPSFillPolygon (dsp, win, gc, xpts, npts, Complex, CoordModeOrigin);
	    XSetForeground (dsp, gc, BlackPixel (dsp, DefaultScreen (dsp)));
	    XFillPolygon (dsp, win, gc, xpts, npts, Complex, CoordModeOrigin);
	} else {
	    XSetForeground (dsp, gc, BlackPixel (dsp, DefaultScreen (dsp)));
	    XPSFillPolygon (dsp, win, gc, xpts, npts, Complex, CoordModeOrigin);
	}
}

/* fill xpts[] with an arc of n points with center at [sv_w/2,sv_h/2], radius
 * r, from the angle defined by [x0,y0] from center to that defined by [x1,y1].
 */
static void
hznArc (r, x0, y0, x1, y1, xpts, npts)
int r;
int x0, y0, x1, y1;
XPoint xpts[];
int npts;
{
	int xc = (int)sv_w/2;
	int yc = (int)sv_h/2;
	double a0, a1, da;
	int i;

printf ("Arc from [%4d %4d] to [%4d %4d]\n", x0, y0, x1, y1);

	/* arc from a0..a1 and step size */
	a0 = atan2 ((double)yc-y0, (double)x0-xc);
	a1 = atan2 ((double)yc-y1, (double)x1-xc);
	if (a1 < a0)
	    a1 += 2*PI;
	da = (a1-a0);

	/* if nearly all the way around, suspect we really want the short way */
	if (da > 19*PI/10) {
	    double tmp = a0;
	    a0 = a1;
	    a1 = tmp;
	    if (a1 < a0)
		a1 += 2*PI;
	    da = (a1-a0);
	}

	/* step a0..a1 */
	da /= npts;
	for (i = 0; i < npts; i++)  {
	    int x = xc + (int)floor(r*cos(a0 + i*da) + .5);
	    int y = yc - (int)floor(r*sin(a0 + i*da) + .5);
	    clampShort (&x);
	    clampShort (&y);
	    xpts->x = (short)x;
	    xpts->y = (short)y;
	    xpts++;
	}
}

/* draw the horizon contour.
 * also blacken beneath in altaz mode.
 */
static void
draw_hznmap (np, dsp, win, gc)
Now *np;
Display *dsp;
Window win;
GC gc;
{
	XPoint xpts[NHZNPTS+10];	/* extra pad for end cap */
	double step = 2*PI/NHZNPTS;	/* az step size */
	int fovr = PI/2*sv_h/sv_vfov;	/* fov radius, pixels */
	int lx = 0, ly = 0;		/* last x,y */
	int lvis = 0, vis;		/* last and current inside fov */
	int ninvis = 0;			/* number pts invisible */
	int fi, li;			/* indices of first and last in fov */
	int i;

	/* walk path around entire horizon.
	 * as we go, look for maxes.
	 */
	fi = li = -1;
	for (i = 0; i < NHZNPTS; i++) {
	    double altdec, azra;
	    double alt, az;
	    int tmp, x, y;

	    /* find x,y from az/alt */
	    az = i*step;
	    alt = hznAlt (az);
	    if (aa_mode) {
		altdec = alt;
		azra = az;
	    } else 
		sv_fullwhere (np, alt, az, 1, &alt, &az, &azra, &altdec);
	    vis = sv_loc (altdec, azra, &x, &y) || cyl_proj;
	    clampShort (&x);
	    clampShort (&y);
	    /* TODO vis = cyl_proj ? 1 : (x-w2)*(x-w2) + (y-h2)*(y-h2) <= fovr*fovr; */


	    /* always clip to fov circle (_not_ to window edge) */
	    if (!vis) {
		if (lvis) {
		    /* just exited fov circle */
		    segisvis (lx, ly, x, y, &tmp, &tmp, &x, &y);
		    ninvis = 0;
		}
		ninvis++;
	    } else {
		if (fi < 0)
		    fi = i;		/* first index that is visible */
		li = i;			/* last index that is visible */
		if (!lvis && ninvis > 0) {
		    /* back inside .. connect to where it exited with arc */
		    segisvis (x, y, lx, ly, &tmp, &tmp, &x, &y);
		    hznArc (fovr, xpts[i-ninvis].x, xpts[i-ninvis].y,
						x, y, xpts+i-ninvis, ninvis);
		}
	    }

	    /* add, and look for peak */
	    xpts[i].x = (short)x;
	    xpts[i].y = (short)y;

	    /* history */
	    lx = x;
	    ly = y;
	    lvis = vis;
	}

	if (cyl_proj) {
	    /* fill from profile to bottom (or top) edge of window. */
	    XPoint full[NHZNPTS+10];		/* path + pad for world box */
	    short below = flip_tb ? SHRT_MIN : SHRT_MAX;
	    int maxxi, pivot;
	    int halfw = sv_h/2;
	    int j;

	    /* pivot around point furthest to the right */
	    maxxi = 0;
	    for (j = 1; j < NHZNPTS; j++)
		if (xpts[j].x < xpts[maxxi].x)
		    maxxi = j;
	    pivot = flip_lr ? maxxi : maxxi+1;

	    /* shift so right-most x is at end of array */
	    memcpy (full, xpts+pivot, (NHZNPTS-pivot)*sizeof(XPoint));
	    memcpy (full+(NHZNPTS-pivot), xpts, pivot*sizeof(XPoint));

	    /* clip off any wraps */
	    for (j = 1; j < NHZNPTS; j++)
		if (abs(full[j].x - full[j-1].x) > halfw)
		    full[j] = full[j-1];

	    /* connect ends by going all the way to the "below" side */
	    full[i].x = full[i-1].x; full[i].y = below; i++;
	    full[i].x = full[0].x;   full[i].y = below; i++;

	    /* fill "below" horizon */
	    hznPoly (dsp, win, gc, full, i);

	    /* add a nice edge */
	    XSetForeground (dsp, gc, hzn_p);
	    XPSDrawLines (dsp, win, gc, full, NHZNPTS, CoordModeOrigin);

	} else if (ninvis == NHZNPTS) {
	    /* it was _never_ visible.. just fill whole fov circle */
	    XPSFillArc (dsp, win, gc, sv_w/2-fovr, sv_h/2-fovr, 2*fovr, 2*fovr,
								    0, 360*64);

	    /* add a nice edge */
	    XSetForeground (dsp, gc, hzn_p);
	    XPSDrawArc (dsp, win, gc, sv_w/2-fovr, sv_h/2-fovr, 2*fovr, 2*fovr,
								    0, 360*64);
	} else {
	    /* start at peak, go all the way around, then go clear up to top
	     * and around edge of the world and back to surround the outside.
	     */
	    XPoint anti[NHZNPTS+10];		/* path + pad for world box */
	    short lef = flip_lr ? SHRT_MAX : SHRT_MIN;
	    short rit = flip_lr ? SHRT_MIN : SHRT_MAX;
	    short lastx, lasty;
	    int j, maxyi;

	    /* connect last to first points */
	    if (fi >= 0 && li >= 0) {
		int nend = NHZNPTS-li-1;
		int nout = fi + nend;
printf ("fi=%4d li=%4d %4d from [%4d %4d] to [%4d %4d]\n", fi, li, nout+1,
xpts[li].x, xpts[li].y, xpts[fi].x, xpts[fi].y);
		hznArc (fovr, xpts[li].x, xpts[li].y, xpts[fi].x, xpts[fi].y,
								anti, nout+1);
		memcpy (xpts, anti+nend, fi*sizeof(XPoint));
		memcpy (xpts+li+1, anti, nend*sizeof(XPoint));
	    }

	    maxyi = 0;
	    for (j = 1; j < NHZNPTS; j++)
		if (xpts[j].y < xpts[maxyi].y)
		    maxyi = j;

	    /* draw around from the top, then around the world and back */
	    memcpy (anti, xpts+maxyi, (NHZNPTS-maxyi)*sizeof(XPoint));
	    memcpy (anti+(NHZNPTS-maxyi), xpts, maxyi*sizeof(XPoint));
	    lastx = anti[NHZNPTS-1].x;
	    lasty = anti[NHZNPTS-1].y;
	    anti[i].x = lastx; anti[i].y = SHRT_MIN; i++;
	    anti[i].x = rit;   anti[i].y = SHRT_MIN; i++;
	    anti[i].x = rit;   anti[i].y = SHRT_MAX; i++;
	    anti[i].x = lef;   anti[i].y = SHRT_MAX; i++;
	    anti[i].x = lef;   anti[i].y = SHRT_MIN; i++;
	    anti[i].x = lastx; anti[i].y = SHRT_MIN; i++;
	    anti[i].x = lastx; anti[i].y = lasty;    i++;
	    hznPoly (dsp, win, gc, anti, i);

	    /* add a nice edge */
	    XSetForeground (dsp, gc, hzn_p);
	    XPSDrawLines (dsp, win, gc, anti, NHZNPTS, CoordModeOrigin);
	}
}

/* draw the ecliptic */
static void
draw_ecliptic(np, dsp, win, gc)
Now *np;
Display *dsp;
Window win;
GC gc;
{
#define	ECL_CACHE_SZ	100
	XPoint ptcache[ECL_CACHE_SZ];
        double elat0, elng0;    /* ecliptic lat and long at center of fov */
	double elngmin, elngmax;/* ecliptic long limits */
	double ra, dec;
	double altdec, azra;
	double elng;
	double lst;
	int ncache;
	int x, y;
	int on, n;

	now_lst (np, &lst);

	/* find equatorial coords of center of view */
	if (aa_mode) {
	    double ha0;		/* local hour angle */
	    aa_hadec (lat, sv_altdec, sv_azra, &ha0, &dec);
	    ra = hrrad(lst) - ha0;
	} else {
	    ra = sv_azra;
	    dec = sv_altdec;
	}
	eq_ecl (mjd, ra, dec, &elat0, &elng0);

	/* no ecliptic visible if ecliptic latitude at center of view 
	 * is more than the window diagonal.
	 */
	if (fabs(elat0) >= sv_dfov)
	    return;

	/* worst-case elong limits is center elong += half size unless cyl */
	if (cyl_proj) {
	    elngmin = elng0 - PI;
	    elngmax = elng0 + PI;
	} else {
	    elngmin = elng0 - sv_dfov/2.0;
	    elngmax = elng0 + sv_dfov/2.0;
	}

	/* draw dashed line */
	ncache = 0;
	on = 1;
	n = 0;
	for (elng = elngmin; elng <= elngmax; elng += sv_vfov/sv_h) {

	    /* dashed pattern */
	    if (on && n == ECL_NON)
		on = 0, n = 0;
	    else if (!on && n == ECL_NOFF)
		on = 1, n = 0;
	    n++;
	    if (!on)
		continue;

	    /* convert longitude along the ecliptic to ra/dec */
	    ecl_eq (mjd, 0.0, elng, &azra, &altdec);

	    /* if in aa mode, we need it in alt/az */
	    if (aa_mode)
		sv_other (altdec, azra, 0, &altdec, &azra);

	    /* if visible, display point */
	    if (sv_loc (altdec, azra, &x, &y)) {
		XPoint *xp = &ptcache[ncache++];
                xp->x = x;
		xp->y = y;

		if (ncache == XtNumber(ptcache)) {
		    XPSDrawPoints(dsp,win,gc,ptcache,ncache,CoordModeOrigin);
		    ncache = 0;
		}
	    }
	}

	if (ncache > 0)
	    XPSDrawPoints (dsp, win, gc, ptcache, ncache, CoordModeOrigin);
#undef ECL_CACHE_SZ
}

/* draw the [pe]numbra, and throw in the anti-solar point too */
static void
draw_umbra(np, dsp, win, gc)
Now *np;
Display *dsp;
Window win;
GC gc;
{
	Obj *sop, *mop;
	double altdec, azra;
	int x, y;

	/* mark the far-field anti-solar point */
	sop = db_basic (SUN);
	mop = db_basic (MOON);
	azra = sop->s_gaera + PI;
	altdec = -sop->s_gaedec;
	if (aa_mode)
	    sv_other (altdec, azra, 0, &altdec, &azra);
	if (sv_loc (altdec, azra, &x, &y))
	    XPSDrawArc (dsp, win, gc, x-ASR, y-ASR, 2*ASR+1, 2*ASR+1, 0,64*360);

	/* mark the umbra/penumbra @ moon dist.
	 * flip geo then correct for parallax
	 */
	azra = sop->s_gaera + PI - (mop->s_gaera - mop->s_ra);
	altdec = -sop->s_gaedec - (mop->s_gaedec - mop->s_dec);
	if (aa_mode)
	    sv_other (altdec, azra, 0, &altdec, &azra);
	if (sv_loc (altdec, azra, &x, &y)) {
	    double rsn = sop->s_edist;
	    double rmn = mop->s_edist;
	    double shadow0 = ERAD + rmn / rsn * (ERAD - SRAD); /* umbra */
	    double shadow1 = ERAD + rmn / rsn * (ERAD + SRAD); /* penumbra */
	    double urad = asin(shadow0/MAU/rmn);
	    double prad = asin(shadow1/MAU/rmn);
	    int upix = (int)floor(urad/(sv_vfov/sv_h) + 0.5);
	    int ppix = (int)floor(prad/(sv_vfov/sv_h) + 0.5);

	    XPSDrawArc (dsp, win, gc, x-upix,y-upix,2*upix+1,2*upix+1,0,64*360);
	    XPSDrawArc (dsp, win, gc, x-ppix,y-ppix,2*ppix+1,2*ppix+1,0,64*360);
	}
}

/* draw the equator */
static void
draw_equator(np, dsp, win, gc)
Now *np;
Display *dsp;
Window win;
GC gc;
{
#define	EQ_CACHE_SZ	100
	XPoint ptcache[EQ_CACHE_SZ];
	double alt0, az0, ra0, dec0;
	double ramin, ramax;	/* display limits */
	double ra;
	double lst;
	int ncache;
	int on, n;

	now_lst (np, &lst);

	/* no equator visible if dec at center of view 
	 * is not less than half the window diagonal.
	 */
	sv_fullwhere (np, sv_altdec, sv_azra, aa_mode, &alt0, &az0, &ra0,&dec0);
	if (fabs(dec0) >= sv_dfov/2.0)
	    return;

	/* worst-case limits is center += half diag size unless cyl */
	if (cyl_proj) {
	    ramin = ra0 - PI;
	    ramax = ra0 + PI;
	} else {
	    ramin = ra0 - sv_dfov/2.0;
	    ramax = ra0 + sv_dfov/2.0;
	}

	/* draw dashed line */
	ncache = 0;
	on = 1;
	n = 0;
	for (ra = ramin; ra <= ramax; ra += sv_vfov/sv_h) {
	    double altdec = 0.0, azra = ra;
	    int x, y;

	    /* dashed pattern */
	    if (on && n == EQ_NON)
		on = 0, n = 0;
	    else if (!on && n == EQ_NOFF)
		on = 1, n = 0;
	    n++;
	    if (!on)
		continue;

	    /* if in aa mode, we need it in alt/az */
	    if (aa_mode)
		sv_other (altdec, azra, 0, &altdec, &azra);

	    /* if visible, display point */
	    if (sv_loc (altdec, azra, &x, &y)) {
		XPoint *xp = &ptcache[ncache++];
                xp->x = x;
		xp->y = y;

		if (ncache == XtNumber(ptcache)) {
		    XPSDrawPoints(dsp,win,gc,ptcache,ncache,CoordModeOrigin);
		    ncache = 0;
		}
	    }
	}

	if (ncache > 0)
	    XPSDrawPoints (dsp, win, gc, ptcache, ncache, CoordModeOrigin);
#undef EQ_CACHE_SZ
}

/* draw the galactic plane and poles */
static void
draw_galactic(np, dsp, win, gc)
Now *np;
Display *dsp;
Window win;
GC gc;
{
#define	GAL_CACHE_SZ	100
	XPoint ptcache[GAL_CACHE_SZ];
	double e = epoch == EOD ? mjd : epoch;
        double glat0, glng0;    /* galactic lat and long at center of fov */
	double glngmin, glngmax;/* galactic long limits */
	double altdec, azra;
	double ra, dec;
	double glng;
	double lst;
	int x, y;
	int ncache;
	int on, n;

	now_lst (np, &lst);

	/* first the poles as little crosses */
	gal_eq (e, PI/2, 0.0, &azra, &altdec);
	if (aa_mode)
	    sv_other (altdec, azra, 0, &altdec, &azra);
	if (sv_loc (altdec, azra, &x, &y)) {
	    XPSDrawLine(dsp, win, gc, x-GAL_W, y, x+GAL_W, y);
	    XPSDrawLine(dsp, win, gc, x, y-GAL_W, x, y+GAL_W);
	}
	gal_eq (e, -PI/2, 0.0, &azra, &altdec);
	if (aa_mode)
	    sv_other (altdec, azra, 0, &altdec, &azra);
	if (sv_loc (altdec, azra, &x, &y)) {
	    XPSDrawLine(dsp, win, gc, x-GAL_W, y, x+GAL_W, y);
	    XPSDrawLine(dsp, win, gc, x, y-GAL_W, x, y+GAL_W);
	}

	/* now the equator */

	/* find ecliptic coords of center of view */
	if (aa_mode) {
	    double ha0;		/* local hour angle */
	    aa_hadec (lat, sv_altdec, sv_azra, &ha0, &dec);
	    ra = hrrad(lst) - ha0;
	} else {
	    ra = sv_azra;
	    dec = sv_altdec;
	}
	eq_gal (e, ra, dec, &glat0, &glng0);

	/* no galactic eq visible if galactic latitude at center of view 
	 * is not less than diagonal window radius.
	 */
	if (fabs(glat0) >= sv_dfov/2.0)
	    return;

	/* worst-case glng limits is center glng += half win size unless cyl */
	if (cyl_proj) {
	    glngmin = glng0 - PI;
	    glngmax = glng0 + PI;
	} else {
	    glngmin = glng0 - sv_dfov/2;
	    glngmax = glng0 + sv_dfov/2;
	}

	/* draw dashed line */
	ncache = 0;
	on = 1;
	n = 0;
	for (glng = glngmin; glng <= glngmax; glng += sv_vfov/sv_h) {

	    /* dashed pattern */
	    if (on && n == GAL_NON)
		on = 0, n = 0;
	    else if (!on && n == GAL_NOFF)
		on = 1, n = 0;
	    n++;
	    if (!on)
		continue;

	    /* convert longitude along the galactic eq to ra/dec */
	    gal_eq (e, 0.0, glng, &azra, &altdec);

	    /* if in aa mode, we need it in alt/az */
	    if (aa_mode)
		sv_other (altdec, azra, 0, &altdec, &azra);

	    /* if visible, display point */
	    if (sv_loc (altdec, azra, &x, &y)) {
		XPoint *xp = &ptcache[ncache++];
                xp->x = x;
		xp->y = y;

		if (ncache == XtNumber(ptcache)) {
		    XPSDrawPoints(dsp,win,gc,ptcache,ncache,CoordModeOrigin);
		    ncache = 0;
		}
	    }
	}

	if (ncache > 0)
	    XPSDrawPoints (dsp, win, gc, ptcache, ncache, CoordModeOrigin);
#undef GAL_CACHE_SZ
}

/* draw the constellation lines */
static void
draw_cnsbounds(np, dsp, win)
Now *np;
Display *dsp;
Window win;
{
#define	NCONSEGS    23	/* draw with this fraction of r (primes look best) */
	double alt, az, ra, dec;
	double e = epoch == EOD ? mjd : epoch;
	double segsize = sv_dfov/NCONSEGS;
	double cdec, sdec;
	double *era0, *edec0, *era1, *edec1;
	double lst;
	int nedges;

	/* get all the edges, precessed to e.
	 */
	nedges = cns_edges (e, &era0, &edec0, &era1, &edec1);
	if (nedges <= 0) {
	    xe_msg ("Can't find constellation edges :-(", 1);
	    return;
	}

	/* prepare for drawing */
	XSetForeground (dsp, sv_cnsgc, cnsbnd_p);
	sv_fullwhere (np, sv_altdec, sv_azra, aa_mode, &alt, &az, &ra, &dec);
	now_lst (np, &lst);
	cdec = cos(dec);
	sdec = sin(dec);

	/* for each edge.. break into smaller segments
	 * and draw any that are even partially visible.
	 */
	while (--nedges >= 0) {
	    double ra0  = era0[nedges];
	    double dec0 = edec0[nedges];
	    double ra1  = era1[nedges];
	    double dec1 = edec1[nedges];
	    XPoint xpts[NCONSEGS+10];
	    int lastvis, lastx, lasty;
	    double dra, ddec;
	    double sep, csep;
	    int see0, see1;
	    int nsegs;
	    int npts;
	    int j;

	    /* cull segments that aren't even close */
	    solve_sphere (ra-ra0, PI/2-dec0, sdec, cdec, &csep, NULL);
	    see0 = (acos(csep) < sv_dfov);
	    solve_sphere (ra-ra1, PI/2-dec1, sdec, cdec, &csep, NULL);
	    see1 = (acos(csep) < sv_dfov);
	    if (!see0 && !see1)
		continue;

	    /* find number of segments with which to draw this edge */
	    solve_sphere (ra1-ra0, PI/2-dec1, sin(dec0), cos(dec0), &csep,NULL);
	    sep = acos(csep);
	    nsegs = (int)(sep/segsize) + 1;

	    /* find step sizes.
	     * N.B. watch for RA going the long way through 0
	     */
	    dra = ra1 - ra0;
	    if (dra < -PI)
		dra += 2*PI;
	    else if (dra > PI)
		dra -= 2*PI;
	    dra /= nsegs;
	    ddec = (dec1 - dec0)/nsegs;

	    /* step along the segment ends */
	    lastvis = -1; /* illegal return value from sv_loc() */
	    lastx = lasty = 0;
	    npts = 0;
	    for (j = 0; j <= nsegs; j++) {
		int vis, x, y;
		double ad, ar;

		ad = dec0 + j*ddec;
		ar = ra0 + j*dra;

		/* need alt/az when are in aa mode */
		if (aa_mode)
		    sv_other (ad, ar, 0, &ad, &ar);

		vis = sv_loc (ad, ar, &x, &y);	/* hzn down with clipping */

		if (lastvis >= 0 && vis != lastvis) {
		    int x1, y1, x2, y2;

		    /* at edge of circle -- find crossing point */
		    if (segisvis (lastx, lasty, x, y, &x1, &y1, &x2, &y2)) {
			if (vis) {
			    xpts[npts].x = x1;
			    xpts[npts].y = y1;
			    npts++;
			} else {
			    xpts[npts].x = x2;
			    xpts[npts].y = y2;
			    npts++;
			    break;	/* end of visible portion */
			}
		    }
		}
		if (vis) {
		    xpts[npts].x = x;
		    xpts[npts].y = y;
		    npts++;
		}

		lastvis = vis;
		lastx = x;
		lasty = y;
	    }

	    if (npts > XtNumber(xpts)) {
		/* stack has already overflowed but try to show how much */
		printf ("cnsbounds Bug! npts=%d N=%d\n", npts, XtNumber(xpts));
		exit(1);
	    }

	    if (npts > 1)
		split_lines(dsp, win, sv_cnsgc, xpts, npts);
	}
}

/* like XPSDrawLine but for use when cyl_proj to split around left or right
 *   edge if longer than half a screen width.
 * return 1 if had to split, else 0.
 */
static int
split_line (dsp, win, gc, x1, y1, x2, y2)
Display *dsp;
Window win;
GC gc;
int x1, y1;
int x2, y2;
{
	if (cyl_proj) {
	    XSegment xs;

	    xs.x1 = x1;
	    xs.y1 = y1;
	    xs.x2 = x2;
	    xs.y2 = y2;
	    return (split_segs (dsp, win, gc, &xs, 1));
	} else {
	    XPSDrawLine (dsp, win, gc, x1, y1, x2, y2);
	    return (0);
	}
}

/* like XPSDrawLines but for use when in cyl_proj to split around left or
 *   right edge if any leg longer than half a screen width.
 * return the number of legs that were split.
 * N.B. we assume np >= 2.
 */
static int
split_lines (dsp, win, gc, xp, np)
Display *dsp;
Window win;
GC gc;
XPoint xp[];
int np;
{
	if (cyl_proj) {
	    int ns = np-1;
	    XSegment *sp0 = (XSegment *) XtMalloc (ns * sizeof(XSegment));
	    XSegment *sp = sp0;
	    XPoint *lxp;
	    int nsplit;

	    for (lxp = xp+ns; xp<lxp; sp++, xp++) {
		sp->x1 = xp->x;
		sp->y1 = xp->y;
		sp->x2 = xp[1].x;
		sp->y2 = xp[1].y;
	    }
	    nsplit = split_segs (dsp, win, gc, sp0, ns);
	    XtFree ((void*)sp0);
	    return (nsplit);
	} else {
	    XPSDrawLines (dsp, win, gc, xp, np, CoordModeOrigin);
	    return (0);
	}
}

/* like XPSDrawSegments but for use when in cyl_proj to split around edge.
 * we know that both ends of each segment are visible.
 * return the number of segments that were split.
 * N.B. unlike XPSDrawSegments we may modify sp[] IN PLACE
 */
static int
split_segs (dsp, win, gc, xsp, ns)
Display *dsp;
Window win;
GC gc;
XSegment xsp[];
int ns;
{
	int nsplit = 0;

	/* check for probably wraps */
	if (cyl_proj) {
	    XSegment *sp = xsp, *lsp;
	    int xwrap, ywrap;

	    for (lsp = sp+ns; sp<lsp; sp++) {
		split_wrap (sp, &xwrap, &ywrap);
		if (xwrap || ywrap) {
		    /* draw one end here, modify other end for array draw */
		    XPSDrawLine (dsp, win, gc, sp->x1, sp->y1,
						    sp->x2+xwrap, sp->y2+ywrap);
		    sp->x1 -= xwrap;
		    sp->y1 -= ywrap;
		    nsplit++;
		}
	    }
	}

	XPSDrawSegments (dsp, win, gc, xsp, ns);

	return (nsplit);
}

/* given a segment, determine if wraps around in the cyl projection.
 * if so, return how far to wrap in each direction, or 0 if none.
 */
static void
split_wrap (sp, xwp, ywp)
XSegment *sp;
int *xwp, *ywp;
{
	double scale = sv_h/sv_vfov;
	int diff;

	if (abs(diff = sp->x2 - sp->x1) > PI*scale)
	    *xwp = (int)floor((2*PI)*scale * (diff > 1 ? -1 : 1) + .5);
	else
	    *xwp = 0;

	if (abs(diff = sp->y2 - sp->y1) > 2*scale)
	    *ywp = (int)floor(PI*scale * (diff > 1 ? -1 : 1) + .5);
	else
	    *ywp = 0;
}

/* draw the constellation figures and/or names */
static void
draw_cns(np, dsp, win)
Now *np;
Display *dsp;
Window win;
{
#define	BBOX(x1,y1,x2,y2)   {				\
	    if (want_connames || want_conabbr) {	\
		if (!begun || x1 < minx) minx = x1;	\
		if (!begun || x1 > maxx) maxx = x1;	\
		if (!begun || y1 < miny) miny = y1;	\
		if (!begun || y1 > maxy) maxy = y1;	\
		if (x2 < minx) minx = x2;		\
		if (x2 > maxx) maxx = x2;		\
		if (y2 < miny) miny = y2;		\
		if (y2 > maxy) maxy = y2;		\
		begun = 1;				\
	    }						\
	}
	double alt, az, ra, dec;
	double altdec, azra;
	double e = epoch == EOD ? mjd : epoch;
	double fra[40], fdec[40];
	double lst;
	int dcodes[40];
	int conids[89];
	int ncns, ndc;
	int x, lastx = 0, y, lasty = 0;
	int vis, lastvis = 0;
	int sx1, sy1, sx2, sy2;
	int minx = 0, maxx = 0, miny = 0, maxy = 0;
	int begun;
	int split;
	int i;

	XSetForeground (dsp, sv_cnsgc, cnsfig_p);
	sv_fullwhere (np, sv_altdec, sv_azra, aa_mode, &alt, &az, &ra, &dec);
	ncns = cns_list (ra, dec, e, sv_dfov/2, conids);
	now_lst (np, &lst);

	while (--ncns >= 0) {
	    ndc = cns_figure (conids[ncns], e, fra, fdec, dcodes);
	    if (ndc <= 0) {
		printf ("cns Bug! Bogus id:%d\n", conids[ncns]);
		exit (1);
	    }
	    split = begun = 0;
	    for (i = 0; i < ndc; i++) {
		/* need alt/az if in alt/az mode */
		if (aa_mode) {
		    sv_other (fdec[i], fra[i], 0, &altdec, &azra);
		} else {
		    altdec = fdec[i];
		    azra = fra[i];
		}
		vis =  sv_loc (altdec, azra, &x, &y); /* hzn uses clipping */

		if (dcodes[i]) {
		    if (want_configures)
			sv_set_dashed_cnsgc (dsp, dcodes[i] == 2);
		    if (vis != lastvis) {
			if (segisvis(lastx,lasty,x,y,&sx1,&sy1,&sx2,&sy2)) {
			    if (want_configures)
				split += split_line (dsp, win, sv_cnsgc, sx1,
								sy1, sx2, sy2);
			    BBOX(sx1, sy1, sx2, sy2);
			}
		    } else if (vis) {
			if (want_configures)
			    split += split_line (dsp, win, sv_cnsgc, lastx,
								lasty, x, y);
			BBOX(lastx, lasty, x, y);
		    }
		}
		lastx = x;
		lasty = y;
		lastvis = vis;
	    }

	    if ((want_connames || want_conabbr) && begun && !split)
		draw_cnsname (dsp, win, conids[ncns], minx, miny, maxx, maxy);
	}

	sv_set_dashed_cnsgc (dsp, 0);
#undef	BBOX
}

/* draw the name of the given constellation centered in the bounding box */
static void
draw_cnsname (dsp, win, conid, minx, miny, maxx, maxy)
Display *dsp;
Window win;
int conid;
int minx, miny, maxx, maxy;
{
	char *name = cns_name (conid);
	XCharStruct all;
	int len;
	int dir, asc, des;
	int x, y;

	if (want_connames) {
	    name += 5;				/* skip "XXX: " */
	    len = strlen (name);
	} else
	    len = 3;				/* just XXX: */
	XTextExtents (sv_cf, name, len, &dir, &asc, &des, &all);

	x = minx + (maxx - minx - all.rbearing)/2;
	y = miny + (maxy - miny - (all.ascent + all.descent))/2 + all.ascent;

	XSetFont (dsp, sv_strgc, sv_cf->fid);
	XSetForeground (dsp, sv_strgc, cnsnam_p);
	XPSDrawString (dsp, win, sv_strgc, x, y, name, len); 
}

/* draw a label for an object that is located at [x,y] with symbol diam d.
 * the label consists OBJF_* in flags.
 * label may contain greek names with superscripts.
 */
static void
draw_label (win, gc, op, flags, x, y, d)
Window win;
GC gc;
Obj *op;
int flags;	/* mask of OBJF_{L,R,N,M,PERSLB}LABEL set */
int x, y;	/* center of object we are labeling, pixels */
int d;		/* diam of object we are labeling, pixels */
{
	Display *dsp = XtDisplay (svda_w);
	char *name = op->o_name;
	int gw = 0;	/* greek width, pixels */
	int sw = 0;	/* superscript width */
	int sa = 0;	/* superscript ascent */
	int nw = 0;	/* regular name width (might include mag too) */
	int gl = 0;	/* n chars in greek name, if any */
	char g = '\0';	/* char code to drawn greek character, if any */
	XCharStruct xcs;
	char buf[64];
	int dir, asc, des;
	int sx, sy;
	int tw;

	/* check the trivial case of not drawing anything :-) */
	if (!name[0] || !(flags & (OBJF_NLABEL|OBJF_MLABEL|OBJF_PERSLB)))
	    return;

	/* default to name if persistent and neither name or mag are set */
	if (!(flags & (OBJF_NLABEL|OBJF_MLABEL))) {
	    flags |= OBJF_NLABEL;
	    if (!(flags & (OBJF_LLABEL|OBJF_RLABEL)))
		flags |= OBJF_RLABEL;
	}

	/* deal with name portion first */
	if (flags & OBJF_NLABEL) {
	    if (sv_ggc && chk_greeklabel (name, &gl, &g)) {
		XTextExtents (sv_gf, &g, 1, &dir, &asc, &des, &xcs);
		gw = xcs.width;
		if (isdigit(name[4+gl])) {
		    /* don't crowd the superscript */
		    XTextExtents (sv_pf, name+gl+4, 1, &dir, &asc, &des, &xcs);
		    gw += 1;
		    sw = xcs.width + 1;
		    sa = xcs.ascent;
		    (void) strcpy (buf, name+gl+5);
		} else
		    (void) strcpy (buf, name+gl+4);
	    } else
		(void) strcpy (buf, name);
	}

	/* set (or append) mag in buf (too) if enabled */
	if (flags & OBJF_MLABEL) {
	    int m = (int)floor(get_mag(op)*10.0 + 0.5);
	    if (flags & OBJF_NLABEL)
		(void) sprintf (buf+strlen(buf), "(%d)", m);
	    else
		(void) sprintf (buf, "%d", m);
	}

	XTextExtents (sv_pf, buf, strlen(buf), &dir, &asc, &des, &xcs);
	nw = xcs.width;

	/* find total offset from x to be drawn then starting x and y */
	tw = gw + sw + nw;
	sx = (flags & OBJF_LLABEL) ? x - tw - d/3 - 1 : x + d/3 + 2;
	sy = y - d/4 - 4;

	/* draw everything, starting at sx,sy */
	if (gw) {
	    unsigned long gcm;
	    XGCValues gcv;

	    /* use same color in ggc as in gc */
	    gcm = GCForeground;
	    (void) XGetGCValues (dsp, gc, gcm, &gcv);
	    XSetForeground (dsp, sv_ggc, gcv.foreground);

	    XPSDrawString (dsp, win, sv_ggc, sx, sy, &g, 1);
	    sx += gw;
	    if (sw) {
		XPSDrawString (dsp, win, gc, sx, sy-sa/2, name+gl+4, 1);
		sx += sw;
	    }
	}
	XPSDrawString (dsp, win, gc, sx, sy, buf, strlen(buf));
}

/* see if the given name is of the form "Cns BayerN-Flams". If so, find the
 * number of chars in the Bayer (greek) part and the greek font code and ret 1.
 * else return 0.
 */
static int
chk_greeklabel (name, glp, gcodep)
char name[];	/* name */
int *glp;	/* number of chars in the greek name part */
char *gcodep;	/* code to use for drawing the greek character */
{
	static char *greeks[] = {
	    "Alpha", "Beta",    "Gamma",   "Delta", "Epsilon", "Zeta",
	    "Eta",   "Theta",   "Iota",    "Kappa", "Lambda",  "Mu", 
	    "Nu",    "Xi",      "Omicron", "Pi",    "Rho",     "Sigma",
	    "Tau",   "Upsilon", "Phi",     "Chi",   "Psi",     "Omega",
	};
	static char grfontidx[] = "abgdezhqiklmnxoprstufcjw";
	static int glen[XtNumber(greeks)];
	int strl;
	int gl;
	int i;

	/* forget it if no greek gc available */
	if (!sv_ggc)
	    return (0);

	/* init glen array the first time */
	if (glen[0] == 0)
	    for (i = 0; i < XtNumber(greeks); i++)
		glen[i] = strlen(greeks[i]);

	/* fast preliminary checks */
	strl = strlen (name);
	if (strl < 6)	/* shortest greek entry is "Cns Pi" */
	    return (0);
	if (name[3] != ' ')
	    return (0);

	/* find length of potentionally greek portion */
	for (gl = 0; ; gl++)
	    if (!isalpha(name[4+gl]))
		break;
	if (gl < 2)	/* shortest greek name is 2 chars */
	    return (0);

	/* scan for greek name -- it may be truncated */
	for (i = 0; i < XtNumber(greeks); i++) {
	    if (gl <= glen[i] && strncmp (name+4, greeks[i], gl) == 0) {
		*gcodep = grfontidx[i];
		*glp = gl;
		return (1);
	    }
	}

	return (0);
}

/* draw all visible eyepieces */
static void
draw_eyep (dsp, win, gc)
Display *dsp;
Window win;
GC gc;
{
	int cir = 360*64;/* 360 degrees in X */
	EyePiece *eyep;
	int neyep;
	int i;

	/* gather the list */
	neyep = se_getlist (&eyep);
	if (neyep == 0)
	    return;

	/* use ep color unless it matches the sky then use fg color */
	XSetForeground (dsp, gc, eyep_p == sky_p ? fg_p : eyep_p);

	/* set this to the largest eyep we actually draw -- just used for the
	 * print label.
	 */
	largeyepr = NULL;
	neyepr = 0;

	/* draw each eyepiece */
	for (i = 0; i < neyep; i++) {
	    EyePiece *ep = &eyep[i];
	    double altdec, azra;
	    int epw, eph;	/* eyepiece half width, height, pixels */
	    int x, y;

	    epw = (int)floor(ep->eyepw*sv_h/sv_vfov/2 + 0.5);
	    eph = (int)floor(ep->eyeph*sv_h/sv_vfov/2 + 0.5);

	    /* forget it if too small */
	    if (epw < MINEPR && eph < MINEPR)
		continue;

	    if (aa_mode) {
		altdec = ep->alt;
		azra = ep->az;
	    } else {
		azra = ep->ra;
		altdec = ep->dec;
	    }

	    /* only draw if center is on screen */
	    if (sv_loc (altdec, azra, &x, &y)) {
		if (ep->round) {
		    if (!ep->solid)
			XPSDrawArc (dsp,win,gc,x-epw,y-eph,2*epw,2*eph,0,cir);
		    else {
			if (XPSInColor())
			   XPSFillArc(dsp,win,gc,x-epw,y-eph,2*epw,2*eph,0,cir);
			else {
			   XFillArc (dsp,win,gc,x-epw,y-eph,2*epw,2*eph,0,cir);
			   XPSDrawArc(dsp,win,gc,x-epw,y-eph,2*epw,2*eph,0,cir);
			}
		    }
		} else {
		    XPoint e[5];

		    e[0].x = x - epw; e[0].y = y - eph;
		    e[1].x = 2*epw;   e[1].y = 0;
		    e[2].x = 0;       e[2].y = 2*eph;
		    e[3].x = -2*epw;  e[3].y = 0;
		    e[4].x = 0;       e[4].y = -2*eph;

		    if (!ep->solid)
			XPSDrawLines (dsp,win,gc,e,5,CoordModePrevious);
		    else {
			if (XPSInColor())
			    XPSFillPolygon(dsp, win, gc, e, 5, Convex,
							    CoordModePrevious);
			else {
			    XFillPolygon(dsp, win, gc, e, 5, Convex,
							    CoordModePrevious);
			    XPSDrawLines (dsp, win, gc, e, 5,
							    CoordModePrevious);
			}
		    }
		}
		if (!largeyepr || largeyepr->eyepw < ep->eyepw
						|| largeyepr->eyeph < ep->eyeph)
		    largeyepr = ep;
		neyepr++;
	    }
	}
}

/* given an object return its desired diameter, in pixels.
 * the size is the larger of the actual size at the current window scale or
 *   a size designed to be proportional to the objects visual magnitude.
 *   but we also force deep sky objects to be at least MIND.
 * N.B. we assume we already know op is at least as bright as dsmag or fsmag
 *   (depending on is_deepsky(), respectively).
 */
static int
objdiam(op)
Obj *op;
{
	int stmag, ssmag, dsmag, magstp;
	int isdeep = is_deepsky(op);
	int faint, d;

	svf_getmaglimits (&stmag, &ssmag, &dsmag, &magstp);
	faint = isdeep ? dsmag : (is_ssobj(op) ? ssmag : stmag);
	d = magdiam (faint, magstp, sv_vfov/sv_h, get_mag(op),
					    degrad(op->s_size/3600.0));

	if (isdeep && d < MIND)
	    d = MIND;

	return (d);
}

/* make the GCs, load the fonts and gather the colors.
 * TODO: clean up previous stuff if called more than once.
 */
static void
sv_mk_gcs()
{
	Display *dsp = XtDisplay(toplevel_w);
	Window win = XtWindow(toplevel_w);
	unsigned long gcm;
	XGCValues gcv;
	Pixel xo;

	get_something (svda_w, XmNbackground, (XtArgVal)&bg_p);

	(void) get_color_resource (svda_w, "SkyColor", &sky_p);
	(void) get_color_resource (svda_w, "HorizonColor", &hzn_p);
	(void) get_color_resource (svda_w, "SkyCnsFigColor", &cnsfig_p);
	(void) get_color_resource (svda_w, "SkyCnsBndColor", &cnsbnd_p);
	(void) get_color_resource (svda_w, "SkyCnsNamColor", &cnsnam_p);
	(void) get_color_resource (svda_w, "SkyGridColor", &grid_p);
	(void) get_color_resource (svda_w, "SkyEqColor", &eq_p);
	(void) get_color_resource (svda_w, "SkyEyePColor", &eyep_p);
	(void) get_color_resource (svda_w, "SkyTrackColor", &fg_p);

	/* use a label's color since that will surely be visible */
	get_something (aa_w, XmNforeground, (XtArgVal)&bgfg_p);

	sv_rf = getXResFont ("SkyGridFont");
	gcm = GCFont;
	gcv.font = sv_rf->fid;
	sv_gc = XCreateGC (dsp, win, gcm, &gcv);

	gcm = 0L;
	sv_cnsgc = XCreateGC (dsp, win, gcm, &gcv);

	gcm = 0L;
	sv_strgc = XCreateGC (dsp, win, gcm, &gcv);

	/* load the tracking and constellation fonts */
	get_tracking_font (dsp, &sv_tf);
	sv_cf = getXResFont ("CnsFont");

	/* load the greek font */
	loadGreek (dsp, win, &sv_ggc, &sv_gf);

	/* get font used by all gcs from obj_pickgc() */
	get_views_font (dsp, &sv_pf);

	/* zoom box gc */
	zm_xorgc = XCreateGC (dsp, win, 0L, NULL);
	xo = sky_p ^ WhitePixel (dsp, DefaultScreen (dsp));
	XSetForeground (dsp, zm_xorgc, xo);
	XSetFunction (dsp, zm_xorgc, GXxor);
}

/* set line_style of sv_cnsgc to LineSolid if dashed==0 else to LineOnOffDash.
 * keep a cache so we don't mess with it any more than necessary.
 */
static void
sv_set_dashed_cnsgc (dsp, dashed)
Display *dsp;
int dashed;
{
	static int last_dashed = -1243;	/* anything bogus */
	XGCValues xgcv;
	unsigned long mask;

	if (last_dashed == dashed)
	    return;
	last_dashed = dashed;

	mask = GCLineStyle;
	xgcv.line_style = dashed ? LineOnOffDash : LineSolid;
	XChangeGC (dsp, sv_cnsgc, mask, &xgcv);
}

/* if a circular edge is showing clip, else just copy.
 * return 1 if any part MAY be visible, else 0.
 */ 
static int
segisvis (x1, y1, x2, y2, cx1, cy1, cx2, cy2)
int x1, y1, x2, y2;		/* original segment */
int *cx1, *cy1, *cx2, *cy2;	/* clipped segment */
{
	int vis;

	if (cyl_proj || sv_dfov < PI) {
	    /* no round edge so just check for rectangle vis.
	     */
	    *cx1 = x1;
	    *cy1 = y1;
	    *cx2 = x2;
	    *cy2 = y2;

	    vis = (x1>=0 || x2>=0) && (x1<(int)sv_w || x2<(int)sv_w)
			&& (y1>=0 || y2>=0) && (y1<(int)sv_h || y2<=(int)sv_h);
	} else {
	    int w = (int)(PI*sv_h/sv_vfov);
	    int x = (sv_w - w)/2;
	    int y = (sv_h - w)/2;

	    vis = lc (x, y, w, x1, y1, x2, y2, cx1, cy1, cx2, cy2) < 0 ? 0 : 1;
	}

	return (vis);
}

/* add the current settings in a new zoom undo entry and install.
 * if successful all deeper entries are discarded.
 * return -1 with no change if zoom box center is not in map, else 0.
 */
static int
zm_addundo()
{
	ZM_Undo *zp;
	double ad, ar;
	double dx, dy;
	double ratio;
	double newfov;

	/* assert */
	if (zm_cundo < 0) {
	    printf ("addundo Bug! zm_cundo=%d zm_nundo=%d\n",zm_cundo,zm_nundo);
	    exit (1);
	}

	/* always add to end of list */
	zm_cutundo();

	/* compute new center -- beware off image */
	if (!sv_unloc ((zm_x0+zm_x1)/2, (zm_y0+zm_y1)/2, &ad, &ar))
	    return (-1);

	/* make room for 1 new undo entry */
	zm_undo = (ZM_Undo *) XtRealloc ((void *)zm_undo,
					    (++zm_nundo)*sizeof(ZM_Undo));
	zp = &zm_undo[zm_cundo++];

	/* save current settings in new entry */
	zp->ad = sv_altdec;
	zp->ar = sv_azra;
	zp->fov = sv_vfov;
	zp->x0 = zm_x0;
	zp->y0 = zm_y0;
	zp->x1 = zm_x1;
	zp->y1 = zm_y1;

	/* compute new fov */
	dx = fabs(zm_x0 - zm_x1);
	dy = fabs(zm_y0 - zm_y1);
	ratio = (dx>dy?dx:dy)/sv_h;
	newfov = ratio*sv_vfov;

	/* install */
	sv_altdec = ad;
	sv_set_scale(ALTDEC_S, 1, 0);
	sv_azra = ar;
	sv_set_scale(AZRA_S, 1, 0);
	sv_set_fov (newfov);
	sv_set_scale(FOV_S, 1, 0);

	return (0);
}

/* install the zm_cundo entry of the zoom undo stack */
static void
zm_installundo()
{
	ZM_Undo *zp = &zm_undo[zm_cundo];

	/* set new center and fov */
	sv_altdec = zp->ad;
	sv_set_scale(ALTDEC_S, 1, 0);
	sv_azra = zp->ar;
	sv_set_scale(AZRA_S, 1, 0);
	sv_set_fov (zp->fov);
	sv_set_scale(FOV_S, 1, 0);

	/* set to new aoi box */
	zm_x0 = zp->x0;
	zm_y0 = zp->y0;
	zm_x1 = zp->x1;
	zm_y1 = zp->y1;
}

/* discard zoom undo stack deeper than current setting */
static void
zm_cutundo()
{
	zm_nundo = zm_cundo;
}

/* disable and forget all unzooms */
static void
zm_noundo()
{
	/* disable for later sv_all's */
	svtb_zoomok(0);

	if (zm_undo) {
	    XtFree ((void *)zm_undo);
	    zm_undo = NULL;
	    zm_nundo = zm_cundo = 0;
	    svtb_unzoomok(0);
	}
}

/* draw rectangle defined by zm_[xy][01] using XOR.
 * this is used both to draw and erase.
 */
static void
zm_draw()
{
	Display *dsp = XtDisplay(svda_w);
	Window win = XtWindow(svda_w);
	XPoint xp[5];

	/* TODO: leaves bit turd if have 0 thickness */
	xp[0].x = zm_x0; xp[0].y = zm_y0;
	xp[1].x = zm_x1; xp[1].y = zm_y0;
	xp[2].x = zm_x1; xp[2].y = zm_y1;
	xp[3].x = zm_x0; xp[3].y = zm_y1;
	xp[4].x = zm_x0; xp[4].y = zm_y0;
	XPSDrawLines (dsp, win, zm_xorgc, xp, 5, CoordModeOrigin);
}

/* respond to fov shortcut PB.
 * client is secret code.. see various XtAdds
 */
static void
sv_fovsc_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int code = (int)client;

	switch (code) {
	case 0:		/* 90:00 FOV */
	    sv_set_fov (degrad(90));
	    sv_set_scale (FOV_S, 1, 1);
	    sv_all (mm_get_now());
	    break;

	case 1:		/* 1:1 */
	    sv_resize (sv_h, sv_h, 1);
	    break;

	case 2:		/* 2:1 */
	    sv_resize (2*sv_h, sv_h, 1);
	    break;

	default:
	    printf ("Bug! Bogus fovsc code: %d\n", code);
	    exit(1);
	}
}

/* respond to an altdec shortcut PB.
 * client is desired position, in degrees.
 */
static void
sv_altdecsc_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	sv_altdec = degrad((int)client);
	sv_set_scale (ALTDEC_S, 1, 1);
	sv_all (mm_get_now());
}

/* callback when a Telescope history item is destroyed.
 * XtFree userData.
 */
/* ARGSUSED */
static void
sv_dtelpd_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtPointer ud;

	get_something (w, XmNuserData, (XtArgVal)&ud);
	XtFree ((char *)ud);
}

/* callback when a Telescope history item is activated.
 * userData is a pointer to a malloced copy of the Obj to which we are to point.
 */
/* ARGSUSED */
static void
sv_telpd_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtPointer ud;
	Obj *op;

	get_something (w, XmNuserData, (XtArgVal)&ud);
	op = (Obj *)ud;
	(void) skyloc_fifo (op);
}

/* add op to the Telescope pulldown as a history entry.
 * we add a new PB and store a copy of op in its userData.
 */
static void
sv_addtelpd (op)
Obj *op;
{
	char label[64];
	XtPointer ud;
	Widget w;
	Arg args[20];
	int n;
	WidgetList ch;
	Cardinal nch;
	int i;

	/* create the label for the button */
	if (strcmp (op->o_name, telAnon))
	    strcpy (label, op->o_name);
	else {
	    char rastr[32], decstr[32];
	    fs_ra (rastr, op->f_RA);
	    fs_prdec (decstr, op->f_dec);
	    sprintf (label, "%s %s", rastr, decstr);
	}

	/* check for already in list */
	get_something (telpd_w, XmNchildren, (XtArgVal)&ch);
	get_something (telpd_w, XmNnumChildren, (XtArgVal)&nch);
	for (i = 0; i < (int)nch; i++) {
	    char *chlabel;
	    int found;

	    if (!XmIsPushButton(ch[i]))
		continue;
	    get_xmstring (ch[i], XmNlabelString, &chlabel);
	    found = !strcmp (chlabel, label);
	    XtFree (chlabel);
	    if (found)
		return;
	}

	n = 0;
	w = XmCreatePushButton (telpd_w, "TELH", args, n);
	XtAddCallback (w, XmNactivateCallback, sv_telpd_cb, NULL);
	XtAddCallback (w, XmNdestroyCallback, sv_dtelpd_cb, NULL);
	wtip (w, "Send telescope to this object or place again");
	set_xmstring (w, XmNlabelString, label);
	ud = (XtPointer) XtMalloc (sizeof(Obj));
	memcpy ((void *)ud, (void *)op, sizeof(Obj));
	set_something (w, XmNuserData, (XtArgVal)ud);
	XtManageChild (w);
}

/* called periodically to decide whether to reload field stars.
 */
static void
chkFS_to (client, id)
XtPointer client;
XtIntervalId *id;
{
	sv_loadfs(0);
	fs_to = XtAppAddTimeOut (xe_app, FSTO, chkFS_to, 0);
}

/* return whether op is visible with respect to horizon choices */
static int
hznOpOk (op)
Obj *op;
{
	return (!want_hznmap || !aa_mode || op->s_alt >= hznAlt(op->s_az));
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: svhzn.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
