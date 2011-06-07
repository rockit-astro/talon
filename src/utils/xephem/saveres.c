/* code to manage current resource.
 * Terminology: "resource" =  "name":"value"
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#if defined(__STDC__)
#include <stdlib.h>
typedef const void * qsort_arg;
#else
typedef void * qsort_arg;
#endif

#if defined (_POSIX_SOURCE)
#include <unistd.h>
#else
#define	X_OK	1
#endif

#include <X11/IntrinsicP.h>	/* to define struct _WidgetRec */
#include <X11/Shell.h>
#include <X11/cursorfont.h>
#include <Xm/CascadeB.h>
#include <Xm/CascadeBG.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/List.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/RowColumn.h>
#include <Xm/Scale.h>
#include <Xm/ScrollBar.h>
#include <Xm/ScrolledW.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/ToggleBG.h>

#include "P_.h"

extern Widget toplevel_w;
extern Colormap xe_cm;
extern String fallbacks[];
extern char myclass[];

extern char prefcategory[];
extern char helpcategory[];

extern char *expand_home P_((char *path));
extern char *getXRes P_((char *name, char *def));
extern char *syserrstr P_((void));
extern char *userResFile P_((void));
extern FILE *fopenh P_((char *name, char *how));
extern int isUp P_((Widget w));
extern void calm_newres P_((void));
extern void e_newres P_((void));
extern void get_something P_((Widget w, char *resource, XtArgVal value));
extern void get_xmstring P_((Widget w, char *resource, char **txtp));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void jm_newres P_((void));
extern void m_newres P_((void));
extern void make_objgcs P_((void));
extern void mars_newres P_((void));
extern void mm_newres P_((void));
extern void ng_newres P_((void));
extern void setButtonInfo P_((void));
extern void all_selection_mode P_((int whether));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_tracking_font P_((Display *dsp, XFontStruct *fsp));
extern void set_views_font P_((Display *dsp, XFontStruct *fsp));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sm_newres P_((void));
extern void ss_newres P_((void));
extern void sv_newres P_((void));
extern void toHSV (double r, double g, double b, double *hp, double *sp,
    double *vp);
extern void toRGB (double h, double s, double v, double *rp, double *gp,
    double *bp);
extern void tr_newres P_((void));
extern void um_newres P_((void));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *tip));
extern void wtip_init P_((void));
extern void xe_msg P_((char *msg, int app_modal));

/* the general idea is to keep a list of each widget registered with
 * sr_reg() in a Resource and a separate list of unique Categories.
 * sr_refresh() updates the current value of each registered widget.
 * sr_display() displays each Resource grouped by Categories in a window.
 * sr_save() writes all Resources which have save True.
 */

typedef struct {
    char *name;			/* cat name. N.B. must be permanent memory */
    int exp;			/* whether to show expanded view */
    int nanew;			/* n new autosave resources in this category */
    int ntnew;			/* n new transient resources in this category */
} Category;

typedef struct {
    Widget live_w;		/* live widget to monitor, else getXRes(fb) */
    char *fb;			/* entry in fallbacks[] or getXRes name */
    char *val;			/* current value, malloced */
    char *lsv;			/* last-saved value, malloced */
    int cati;			/* index into catlist (N.B. no ptr: realloced)*/
    int new : 1;		/* set if val != lsv */
    int save : 1;		/* whether to save, regardless of new */
    int autosav : 1;		/* set if want to mark for save whenevr chngs */
} Resource;

/* one of these to describe each color or font we control */
typedef struct {
    int gap;			/* 0=button, 1=title */
    char *title;		/* name to show user */
    char *res;			/* resource pattern */
    char reg;			/* whether to register in Save list */
    char autosav;		/* if reg, whether as autosav */
    char *res2;			/* 2nd resource pattern if any, or *_newres() */
    char reg2;			/* whether to register in Save list */
    char autosav2;		/* if reg2, whether as autosav */
    XtCallbackProc cb;		/* callback */
    int (*isf)();		/* class comparitor function */
    char *tip;			/* help tip text */
    Widget w;			/* PB control (once created that is) */
} Choice;

static void create_srshell P_((void));
static void sr_save_cb P_((Widget w, XtPointer client, XtPointer call));
static void sr_refresh_cb P_((Widget w, XtPointer client, XtPointer call));
static void sr_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void sr_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void sr_asel_cb P_((Widget w, XtPointer client, XtPointer call));
static void sr_display P_((void));
static void sr_createpms P_((void));
static void sr_init P_((void));
static void sr_setnnew P_((int nanew, int ntnew));
static int crackNam P_((char *res, char *nam));
static int crackVal P_((char *res, char *val));
static void getCurVal P_((Resource *rp, char *val));
static void getGeometry P_((Widget w, int *xp, int *yp));
static void fmtRes P_((char *res, char *nam, char *val));
static void cpyNoWS P_((char *to, char *from));
static char *findWFB P_((Widget w));
static char *findRFB P_((char *res));
static int findCat P_((char *cat));
static Resource *findRes P_((char *findnam));
static int cmpRes P_((Resource *r1, Resource *r2));
static char *fullWName P_((Widget w, char *buf));
static void loadArgsChildren P_((Widget w, int (*isf)()));
static void updateHelp P_((void));

static Arg *loadargs;		/* list for loadArgsChildren() */
static int nloadargs;		/* length of loadargs[] */

/* pixmaps for the "directory" listing */

#define more_width 16
#define more_height 16
static unsigned char more_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
   0x80, 0x01, 0xf8, 0x1f, 0xf8, 0x1f, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01,
   0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define nomore_width 16
#define nomore_height 16
static unsigned char nomore_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0xf8, 0x1f, 0xf8, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define majorres_width 16
#define majorres_height 16
static unsigned char majorres_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x01,
   0xe0, 0x03, 0xf0, 0x07, 0xf0, 0x07, 0xf0, 0x07, 0xe0, 0x03, 0xc0, 0x01,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define minorres_width 16
#define minorres_height 16
static unsigned char minorres_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x01,
   0x20, 0x02, 0x10, 0x04, 0x10, 0x04, 0x10, 0x04, 0x20, 0x02, 0xc0, 0x01,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define blankres_width 16
#define blankres_height 16
static unsigned char blankres_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static Category *catlist;	/* malloced list of Categorys */
static int ncatlist;		/* entries in catlist[] */
static Resource *reslist;	/* malloced list of Resources */
static int nreslist;		/* entries in reslist[] */
static Pixmap more_pm;		/* more stuff */
static Pixmap nomore_pm;	/* no more stuff */
static Pixmap majorres_pm;	/* new autosav */
static Pixmap minorres_pm;	/* new non-autosav */
static Pixmap blankres_pm;	/* not new */
static int nmyclass;		/* handy length of myclass[] */

/* name of default per-user dir and name of file that can override */
static char mydirdef[] = "~/XEphem";
static char mdovride[] = "~/.xephemrc";
static char mdres[] = "XEphem.PrivateDir";

#define	RESWID	45		/* columns for resource name, if possible */
#define	PGWID	75		/* overall number of default columns */
#define	MRNAM	128		/* max chars in resource name (not critical) */
#define	MRVAL	128		/* max chars in resource value (not critical) */
#define	MLL	256		/* max line length (not critical) */

/* Save window info */
static Widget srshell_w;	/* main shell */
static Widget srsw_w;		/* XmScrolledWindow to hold report */
static Widget asav_w;		/* autosave TB */
static Widget asel_w;		/* autoselect TB */
static Widget close_w;		/* close PB */
static Widget majorres_w;	/* sample majorres_pm label */
static Widget minorres_w;	/* sample minorres_pm label */
static Widget majorn_w;		/* count of changed major resources */
static Widget minorn_w;		/* count of changed minor resources */
static int pendingexit;		/* set when saving just before exiting */

/* a family of functions that checks for an exact class match.
 * (can't use xm*Class in static initialization!!)
 */
#define	CLASSCHKF(f,c1, c2)			\
    static int f(w) Widget w; { 		\
	WidgetClass wc = XtClass(w);		\
	return (wc == c1 || wc == c2);		\
    }
CLASSCHKF (isLabel, xmLabelWidgetClass, xmLabelGadgetClass)
CLASSCHKF (isPB,    xmPushButtonWidgetClass, xmPushButtonGadgetClass)
CLASSCHKF (isTB,    xmToggleButtonWidgetClass, xmToggleButtonGadgetClass)
CLASSCHKF (isCB,    xmCascadeButtonWidgetClass, xmCascadeButtonGadgetClass)
CLASSCHKF (isList,  xmListWidgetClass, 0)
CLASSCHKF (isText,  xmTextWidgetClass, 0)
CLASSCHKF (isTextF, xmTextFieldWidgetClass, 0)
CLASSCHKF (isScale, xmScaleWidgetClass, 0)
static int isAny(w) Widget w; { return (1); }

/* Font window info */
static Widget srfshell_w;	/* main shell */
static Widget srfhl_w;		/* font history scrolled list */
static Widget srftf_w;		/* font text field */
static Widget srfaf_w;		/* list of all fonts */
static Widget srfsample_w;	/* label to show sample */
static Widget fappto_w;		/* Apply-to TB */
static Widget fsetd_w;		/* Set-default TB */
static Widget fgetc_w;		/* Get-current TB */
static Widget fgetd_w;		/* Get-default TB */
static void create_srfshell P_((void));
static XFontStruct * srf_install P_((char *res, char *res2, char *xlfd));
static void srf_go_cb P_((Widget w, XtPointer client, XtPointer call));
static void srf_tracking_cb P_((Widget w, XtPointer client, XtPointer call));
static void srf_views_cb P_((Widget w, XtPointer client, XtPointer call));
static void srf_moons_cb P_((Widget w, XtPointer client, XtPointer call));
static void srf_appres_cb P_((Widget w, XtPointer client, XtPointer call));

/* N.B. see create_buttons() for grouping rules */
static Choice fchoices[] = {
    {1, "Controls:"},
    {1},
    {1},
    {1},
    {0, "Labels",
	"XEphem*XmLabel.fontList", 1, 1,
	"XEphem*XmLabelGadget.fontList", 1, 1,
	srf_go_cb, isLabel, "Font for all passive labels"},
    {0, "Push buttons",
	"XEphem*XmPushButton.fontList", 1, 1,
	"XEphem*XmPushButtonGadget.fontList", 1, 1,
	srf_go_cb, isPB, "Font for all push buttons"},
    {0, "Toggle buttons",
	"XEphem*XmToggleButton.fontList", 1, 1,
	"XEphem*XmToggleButtonGadget.fontList", 1, 1,
	srf_go_cb, isTB, "Font for all toggle buttons"},
    {0, "Cascade buttons",
	"XEphem*XmCascadeButton.fontList", 1, 1,
	"XEphem*XmCascadeButtonGadget.fontList", 1, 1,
	srf_go_cb, isCB, "Font for all buttons that spring pulldown menus"},

    {1, "Text:"},
    {1},
    {1},
    {1},
    {0, "Text fields",
	"XEphem*XmTextField.fontList", 1, 1,
	0, 0, 0,
	srf_go_cb, isTextF, "Font for all 1-line text fields"},
    {0, "Text boxes",
	"XEphem*XmText.fontList", 1, 1,
	0, 0, 0,
	srf_go_cb, isText, "Font for all multiline text fields"},
    {0, "Lists",
	"XEphem*XmList.fontList", 1, 1,
	0, 0, 0,
	srf_go_cb, isList, "Font for text presented in lists"},
    {0, "Tips",
	"XEphem.tipFont", 1, 1,
	(char *)wtip_init, 0, 0,
	srf_appres_cb, NULL, "Font for the help tip balloons"},

    {1, "Other:"},
    {1},
    {1},
    {1},
    {0, "Sky constel",
	"XEphem.CnsFont", 1, 1,
	(char *)sv_newres, 0, 0,
	srf_appres_cb, NULL, "Font to draw constellation names in Sky View"},
    {0, "Sky grid",
	"XEphem.SkyGridFont", 1, 1,
	(char *)sv_newres, 0, 0,
	srf_appres_cb, NULL, "Font to draw the grid labels in Sky View"},
    {0, "Greek",
	"XEphem.viewsGreekFont", 1, 1,
	(char *)sv_newres, 0, 0,
	srf_appres_cb, NULL,
	    "Font for Greek portion of Bayer names in map views"},
    {0, "Cursor data",
	"XEphem.cursorTrackingFont", 1, 1,
	0, 0, 0,
	srf_tracking_cb,NULL,
	    "Font to show the cursor tracking coordinates in maps"},
    {0, "Map trails",
	"XEphem.trailsFont", 1, 1,
	(char *)tr_newres, 0, 0,
	srf_appres_cb, NULL, "Font for time trails in maps"},
    {0, "Map labels",
	"XEphem.viewsFont", 1, 1,
	0, 0, 0,
	srf_views_cb, NULL, "Font to label objects in map views"},
    {0, "Moons labels",
	"XEphem.moonsFont", 1, 1,
	0, 0, 0,
	srf_moons_cb, NULL, "Font to label moons in map views"},
    {0, "Scales",
	"XEphem*XmScale.fontList", 1, 1,
	0, 0, 0,
	srf_go_cb, isScale, "Font for scales using standard Motif labeling"},
};

/* Color window info */
static Widget srcshell_w;	/* main shell */
static Widget srcsl_w;		/* color history scrolled list */
static Widget cappto_w;		/* Apply-to TB */
static Widget csetd_w;		/* Set-default TB */
static Widget cgetc_w;		/* Get-current TB */
static Widget cgetd_w;		/* Get-default TB */
static Widget srctf_w;		/* color text field */
static Widget srcRH_w;		/* R/H scale */
static Widget srcGS_w;		/* G/S scale */
static Widget srcBV_w;		/* B/V scale */
static Widget srcrgb_w;		/* rgb TB scale */
static Widget srcda_w;		/* sample drawing area */
static Widget cpicker_w;	/* color picker TB */
static void create_srcshell P_((void));
static void src_fg_cb P_((Widget w, XtPointer client, XtPointer call));
static void src_bg_cb P_((Widget w, XtPointer client, XtPointer call));
static void src_modal_cb P_((Widget w, XtPointer client, XtPointer call));
static void src_appres_cb P_((Widget w, XtPointer client, XtPointer call));
static void src_obj_cb P_((Widget w, XtPointer client, XtPointer call));
static void src_setmodal P_((Pixel bg, char *cnam, int (*isf)()));
static void src_install P_((char *res, char *res2, char *cnam));
static void src_setbg P_((Widget w, Pixel bg, int (*isf)()));
static void src_showcolor P_((char *name, int scalestoo));
static int src_hasShadow P_((Widget w));

/* N.B. see create_buttons() for grouping rules */
static Choice cchoices[] = {
    {1, "Text:"},
    {1},
    {1},
    {1},

    {0, "Labels",
	"XEphem*XmLabel.foreground", 1, 1,
	"XEphem*XmLabelGadget.foreground", 1, 1,
	src_fg_cb, isLabel, "Color of passive labels"},
    {0, "Push buttons",
	"XEphem*XmPushButton.foreground", 1, 1,
	"XEphem*XmPushButtonGadget.foreground", 1, 1,
	src_fg_cb, isPB, "Color of push buttons"},
    {0, "Toggle buttons",
	"XEphem*XmToggleButton.foreground", 1, 1,
	"XEphem*XmToggleButtonGadget.foreground", 1, 1,
	src_fg_cb, isTB, "Color of toggle buttons"},
    {0, "Cascade buttons",
	"XEphem*XmCascadeButton.foreground", 1, 1,
	"XEphem*XmCascadeButtonGadget.foreground", 1, 1,
	src_fg_cb, isCB, "Color of buttons that pull down menus"},

    {0, "Text fields",
	"XEphem*XmTextField.foreground", 1, 1,
	0, 0, 0,
	src_fg_cb, isTextF, "Color of 1-line text fields"},
    {0, "Text boxes",
	"XEphem*XmText.foreground", 1, 1,
	0, 0, 0,
	src_fg_cb, isText, "Color of multiline text fields"},
    {0, "Lists",
	"XEphem*XmList.foreground", 1, 1,
	0, 0, 0,
	src_fg_cb, isList, "Color of text presented in lists"},
    {0, "Scales",
	"XEphem*XmScale.foreground", 1, 1,
	0, 0, 0,
	src_fg_cb, isScale, "Color of text presented in lists"},

    {0, "Moon overlay",
	"XEphem.MoonAnnotColor", 1, 1,
	(char *)m_newres, 0, 0,
	src_appres_cb, NULL,
	    "Color of annotation and other overlay on Moon view"},
    {0, "Mars overlay",
	"XEphem.MarsAnnotColor", 1, 1,
	(char *)mars_newres, 0, 0,
	src_appres_cb, NULL,
	    "Color of annotation and other overlay on Mars view"},
    {0, "Tips",
	"XEphem.tipForeground", 1, 1,
	(char *)wtip_init, 0, 0,
	src_appres_cb, NULL, "Color of text in bubble tips"},
    {1},

    {1, "Backgrounds:"},
    {1},
    {1},
    {1},
    {0, "Text fields",
	"XEphem*XmTextField.background", 1, 1,
	0, 0, 0,
	src_bg_cb, isTextF, "Color of 1-line text field backgrounds"},
    {0, "Text boxes",
	"XEphem*XmText.background", 1, 1,
	0, 0, 0,
	src_bg_cb, isText, "Color of multiline text field backgrounds"},
    {0, "Lists",
	"XEphem*XmList.background", 1, 1,
	0, 0, 0,
	src_bg_cb, isList, "Color of lists background"},
    {0, "Tips",
	"XEphem.tipBackground", 1, 1,
	(char *)wtip_init, 0, 0,
	src_appres_cb, NULL, "Color of background in bubble tips"},

    {0, "Prompt dialogs",
	0, 0, 0,
	0, 0, 0,
	src_modal_cb, isAny,
	    "Color of prompt and modal dialog window backgrounds"},
    {0, "Moon",
	"XEphem.MoonBackground", 1, 1,
	(char *)m_newres, 0, 0,
	src_appres_cb, NULL, "Color of background in Moon window"},
    {0, "Mars",
	"XEphem.MarsBackground", 1, 1,
	(char *)mars_newres, 0, 0,
	src_appres_cb, NULL, "Color of background in Mars windows"},
    {0, "Jupiter",
	"XEphem.JupiterBackground", 1, 1,
	(char *)jm_newres, 0, 0,
	src_appres_cb, NULL, "Color of background in Jupiter window"},

    {0, "Saturn",
	"XEphem.SaturnBackground", 1, 1,
	(char *)sm_newres, 0, 0,
	src_appres_cb, NULL, "Color of background in Saturn window"},
    {0, "Uranus",
	"XEphem.UranusBackground", 1, 1,
	(char *)um_newres, 0, 0,
	src_appres_cb, NULL, "Color of background in Uranus window"},
    {0, "- All -",
	"XEphem.normalBGColor", 1, 1,
	"XEphem*background", 1, 1,
	src_bg_cb, isAny, "Color of overall background"},
    {0, "- Night -",
	"XEphem.nightBGColor", 1, 1,
	"XEphem*background", 0, 0,	/* once in All will do */
	src_bg_cb, isAny, "Night background color"},

    {1, "Sky view:"},
    {1},
    {1},
    {1},

    {0, "Background sky",
	"XEphem.SkyColor", 1, 1,
	(char *)sv_newres, 0, 0,
	src_appres_cb, NULL, "Color of Sky View sky background"},
    {0, "Cns bounds",
	"XEphem.SkyCnsBndColor", 1, 1,
	(char *)sv_newres, 0, 0,
	src_appres_cb, NULL, "Color of Sky View constellation boundaries"},
    {0, "Cns figures",
	"XEphem.SkyCnsFigColor", 1, 1,
	(char *)sv_newres, 0, 0,
	src_appres_cb, NULL, "Color of Sky View constellation figures"},
    {0, "Cns names",
	"XEphem.SkyCnsNamColor", 1, 1,
	(char *)sv_newres, 0, 0,
	src_appres_cb, NULL, "Color of Sky View constellation names"},

    {0, "Eyepieces",
	"XEphem.SkyEyePColor", 1, 1,
	(char *)sv_newres, 0, 0,
	src_appres_cb, NULL, "Color of Sky View eyepieces"},
    {0, "Grid",
	"XEphem.SkyGridColor", 1, 1,
	(char *)sv_newres, 0, 0,
	src_appres_cb, NULL, "Color of Sky View grid and labels"},
    {0, "Coord planes",
	"XEphem.SkyEqColor", 1, 1,
	(char *)sv_newres, 0, 0,
	src_appres_cb, NULL,
	    "Color of Sky View equatorial, ecliptic and galactic planes"},
    {0, "Horizon",
	"XEphem.HorizonColor", 1, 1,
	(char *)sv_newres, 0, 0,
	src_appres_cb, NULL, "Color of Sky View horizon profile"},

    {0, "Cursor data",
	"XEphem.SkyTrackColor", 1, 1,
	(char *)sv_newres, 0, 0,
	src_appres_cb, NULL, "Color of Sky View cursor tracking coordinates"},
    {1},
    {1},
    {1},

    {1, "Earth view:"},
    {1},
    {1},
    {1},

    {0, "Object 1",
	"XEphem.EarthObj1Color", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw Object defined on row 1"},
    {0, "Object 2",
	"XEphem.EarthObj2Color", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw Object defined on row 2"},
    {0, "Object 3",
	"XEphem.EarthObj3Color", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw Object defined on row 3"},
    {0, "Sun light",
	"XEphem.EarthSunColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color of sunlit portion of Earth surface"},

    {0, "Grid",
	"XEphem.EarthGridColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color of coordinate grid"},
    {0, "Sites",
	"XEphem.EarthSiteColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color of dots which denote each pickable Site"},
    {0, "Totality",
	"XEphem.EarthEclipseColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL,
	    "Color of mark denoting location of totality during solar eclipse"},
    {0, "Continents",
	"XEphem.EarthBorderColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw continent outlines"},

    {0, "Here cross",
	"XEphem.EarthHereColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color of cross marking focus position"},
    {0, "Background",
	"XEphem.EarthBackground", 1, 1,
	(char *)e_newres, 0, 0,
	src_appres_cb, NULL, "Color of background in Earth window"},
    {1},
    {1},

    {1, "Targets:"},
    {1},
    {1},
    {1},
    {0, "Mercury",
	"XEphem.mercuryColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw Mercury on maps"},
    {0, "Venus",
	"XEphem.venusColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw Venus on maps"},
    {0, "Mars",
	"XEphem.marsColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw Mars on maps"},
    {0, "Jupiter",
	"XEphem.jupiterColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw Jupiter on maps"},

    {0, "Saturn",
	"XEphem.saturnColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw Saturn on maps"},
    {0, "Uranus",
	"XEphem.uranusColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw Uranus on maps"},
    {0, "Neptune",
	"XEphem.neptuneColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw Neptune on maps"},
    {0, "Pluto",
	"XEphem.plutoColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw Pluto on maps"},

    {0, "Hot stars",
	"XEphem.hotStarColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL,
	    "Color used to draw spectral class O, B, A or W stars on maps"},
    {0, "Warm stars",
	"XEphem.mediumStarColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL,
	    "Color used to draw spectral class F, G or K stars on maps"},
    {0, "Cool stars",
	"XEphem.coolStarColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL,
	    "Color used to draw spectral class M, N, R or C stars on maps"},
    {0, "Other stars",
	"XEphem.otherStellarColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw other stellar objects on maps"},

    {0, "Sun",
	"XEphem.sunColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw the Sun on maps"},
    {0, "Moon",
	"XEphem.moonColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw the Moon on maps"},
    {0, "Asteroids",
	"XEphem.solSysColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw asteroids and comets on maps"},
    {0, "Other objs",
	"XEphem.otherObjColor", 1, 1,
	0, 0, 0,
	src_obj_cb, NULL, "Color used to draw objects of unknown type on maps"},
};

/* list of resources we control for shadow thickness */
static char *shadRes[] = {
    "XEphem*XmPushButton.shadowThickness",
    "XEphem*XmPushButtonGadget.shadowThickness",
    "XEphem*XmCascadeButton.shadowThickness",
    "XEphem*XmCascadeButtonGadget.shadowThickness",
    "XEphem*XmScale.shadowThickness",
    "XEphem*XmTextField.shadowThickness",
    "XEphem*XmText.shadowThickness",
    "XEphem*XmList.shadowThickness",
    "XEphem*XmScrollBar.shadowThickness",
    "XEphem*XmRowColumn.shadowThickness",
};

/* list of resources for Prompt and Modal dialog backgrounds */
static char *modalRes[] = {
    "XEphem*MainPrompt*background",
    "XEphem*ModalMessage*background",
    "XEphem*NetStop*background",
    "XEphem*ObjPrompt*background",
    "XEphem*Print*background",
    "XEphem*Query*background",
};

/* list of resources for the transient Help system */
static char *helpRes[] = {
    "XEphem*HelpWindow.x",
    "XEphem*HelpWindow.y",
};

static char prefshadcategory[] = "Main -- Preferences -- Shadows";
static char preffontcategory[] = "Main -- Preferences -- Fonts";
static char prefcolrcategory[] = "Main -- Preferences -- Colors";
static char prefsavecategory[] = "Main -- Preferences -- Save";

#define	MAXSCALE	100	/* max value in color scales */

/* register the given widget and/or resource to the collection we use for Save,
 *   assigning it to the given category.
 * if !w then res is a resource to monitor purely via Xrm.
 * if !res then w is the widget to follow and has a real entry in fallbacks[].
 * if both then w is the widget to follow but fallbacks[] has a resource.
 * if neither then this is illegal!
 * autosav indicates whether this resource is automatically marked for save
 *   when its value changes.
 * N.B. memory at res and cat must be permanent.
 * N.B. we assume this is called before app changes anything from its initial
 *   value.
 */
void
sr_reg (w, res, cat, autosav)
Widget w;
char *res;
char *cat;
int autosav;
{
	Resource *rp, newr, *newrp = &newr;
	char val[MRVAL];

	/* need at least one */
	if (!w && !res) {
	    printf ("Bug! Nothing for sr_reg()\n");
	    exit(1);
	}

	/* one-time setup */
	sr_init();

	/* init new Resource */
	memset (newrp, 0, sizeof(*newrp));
	newrp->cati = findCat (cat);
	newrp->fb = res ? findRFB(res) : findWFB (w);
	newrp->live_w = w;
	getCurVal (newrp, val);
	newrp->val = XtNewString (val);
	newrp->lsv = XtNewString (val);
	newrp->new = newrp->save = 0;
	newrp->autosav = autosav;

	/* expand list of resources */
	reslist = (Resource *) XtRealloc ((char *)reslist,
						(nreslist+1)*sizeof(Resource));

	/* bubble rp to new position */
	for (rp=&reslist[nreslist++]; rp>reslist && cmpRes(newrp,rp-1)<0; --rp)
	    memcpy (rp, rp-1, sizeof(*rp));

	/* insert */
	memcpy (rp, &newr, sizeof(newr));
}

/* unregister the given widget */
void
sr_unreg (w)
Widget w;
{
	Resource *rp, *endrp;

	/* search for matching live_w */
	endrp = &reslist[nreslist];
	for (rp = reslist; rp < endrp; rp++)
	    if (rp->live_w == w)
		break;
	if (rp == endrp) {
	    printf ("Bug! sr_unreg can not find %s\n", XtName(w));
	    exit(1);
	}

	/* reclaim value memory */
	XtFree (rp->val);
	XtFree (rp->lsv);

	/* copy down to remove from reslist */
	memmove (rp, rp+1, sizeof(Resource)*(endrp-(rp+1)));
	nreslist--;
}

/* called to put up or remove the watch cursor on any of the Save windows  */
void
sr_cursor (c)
Cursor c;
{
	Window win;

	if (srshell_w && (win = XtWindow(srshell_w)) != 0) {
	    Display *dsp = XtDisplay(srshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}

	if (srfshell_w && (win = XtWindow(srfshell_w)) != 0) {
	    Display *dsp = XtDisplay(srfshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}

	if (srcshell_w && (win = XtWindow(srcshell_w)) != 0) {
	    Display *dsp = XtDisplay(srcshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* return 1/0 whether either autosave is on */
int
sr_autosaveon()
{
	sr_init();
	return (XmToggleButtonGetState (asav_w));
}

/* update our knowledge of each Resource's value.
 * return the total number of enabled autosav entries that do not match their
 * last-saved value.
 */
int
sr_refresh()
{
	char val[MRVAL];
	Category *lastcp;
	Resource *rp;
	int wantasel;
	int totntnew, totnanew;

	sr_init();
	watch_cursor(1);

	updateHelp();
	wantasel = XmToggleButtonGetState (asel_w);

	totntnew = totnanew = 0;
	lastcp = NULL;
	for (rp = reslist; rp < &reslist[nreslist]; rp++) {
	    Category *cp = &catlist[rp->cati];
	    if (lastcp != cp) {
		lastcp = cp;
		cp->ntnew = 0;
		cp->nanew = 0;
	    }
	    getCurVal (rp, val);
	    if ((rp->new = !!strcmp (val, rp->lsv))) {
		if (rp->autosav) {
		    cp->nanew++;
		    totnanew++;
		} else {
		    cp->ntnew++;
		    totntnew++;
		}
	    }
	    rp->save = rp->new && rp->autosav && wantasel;
	    if (strcmp (val, rp->val)) {
		XtFree (rp->val);
		rp->val = XtNewString (val);
	    }
	}

	sr_setnnew (totnanew, totntnew);

	watch_cursor(0);
	return (totnanew);
}

/* compute and display resources that have changed since last saved.
 */
void
sr_manage()
{
	/* create if first time */
	sr_init();
	watch_cursor(1);

	/* fresh report */
	sr_refresh();
	sr_display();
	watch_cursor(0);

	/* show */
	XtPopup (srshell_w, XtGrabNone);
	set_something (srshell_w, XmNiconic, (XtArgVal)False);
}

/* just like sr_manage() but exits when saving is completed */
void
sr_xmanage()
{
	pendingexit = 1;
	sr_manage();
}

/* bring up the font management window */
void
srf_manage()
{
	sr_init();
	XtPopup (srfshell_w, XtGrabNone);
	set_something (srfshell_w, XmNiconic, (XtArgVal)False);
}

/* bring up the color management window */
void
src_manage()
{
	sr_init();
	XtPopup (srcshell_w, XtGrabNone);
	set_something (srcshell_w, XmNiconic, (XtArgVal)False);
}

/* return 1/0 whether Save window is currently up */
int
sr_isUp()
{
	return (isUp (srshell_w));
}

/* save the selected resources to the local file.
 * when finished all resources as "up to date".
 */
int
sr_save(talk)
int talk;
{
	char nam[MRNAM];	/* resource name */
	char buf[1024];		/* handy buffer */
	char *resfn;		/* full path to user's resource file */
	FILE *oldfp;		/* existing resource file, if any */
	FILE *newfp;		/* new resource file */
	int nnew, nrepl;	/* count of entries added and replaced */
	Resource *rp;

	/* start */
	sr_init();
	watch_cursor(1);

	/* open existing if possible, rename since about to create new */
	resfn = userResFile();
	oldfp = fopen (resfn, "r");
	if (oldfp) {
	    sprintf (buf, "%s.bak", resfn);
	    if (rename (resfn, buf) < 0) {
		sprintf (buf, "Can not backup %s:\n%s", resfn, syserrstr());
		xe_msg (buf, 1);
		fclose (oldfp);
		watch_cursor(0);
		return (-1);
	    }
	}

	/* create new resource file */
	newfp = fopen (resfn, "w");
	if (!newfp) {
	    if (oldfp)
		fclose (oldfp);
	    sprintf (buf, "Can not create %s:\n%s", resfn, syserrstr());
	    xe_msg (buf, 1);
	    watch_cursor(0);
	    return (-1);
	}

	/* scan for matching selected reslist[] entries in oldfp, replace
	 * in-place and mark so don't save again.
	 */
	nrepl = 0;
	if (oldfp) {
	    while (fgets (buf, sizeof(buf), oldfp)) {
		buf[strlen(buf)-1] = '\0';
		if (!crackNam (buf, nam) && (rp = findRes (nam)) && rp->save) {
		    fmtRes (buf, nam, rp->val);
		    rp->save = 0;
		    nrepl++;
		}
		fprintf (newfp, "%s\n", buf);
	    }
	    fclose (oldfp);
	}

	/* append all remaining selected entries and mark all as current */
	nnew = 0;
	for (rp = reslist; rp < &reslist[nreslist]; rp++) {
	    if (rp->save) {
		crackNam (rp->fb, nam);
		fmtRes (buf, nam, rp->val);
		rp->save = 0;
		nnew++;
		fprintf (newfp, "%s\n", buf);
	    }

	    /* no longer out of date */
	    if (rp->new) {
		XtFree (rp->lsv);
		rp->lsv = XtNewString (rp->val);
		rp->new = 0;
	    }
	}
	fclose (newfp);

	/* possibly inform and done */
	if (talk) {
	    sprintf (buf, "%s:\n%3d replaced\n%3d added", userResFile(),
								nrepl, nnew);
	    xe_msg (buf, 1);
	}
	watch_cursor(0);
	return (0);
}

/* return full path of per-user working directory,
 * allowing for possible override.
 */
char *
getPrivateDir()
{
	static char *mydir;

	if (!mydir) {
	    /* try mdovride else use default */
	    FILE *ofp = fopenh (mdovride, "r");
	    char *vhome, *vp = NULL;
	    char nam[MRNAM], val[MRVAL], buf[MLL];

	    if (ofp) {
		while (fgets (buf, sizeof(buf), ofp)) {
		    if (!crackNam (buf, nam) && !strcmp (nam, mdres) &&
						    !crackVal (buf, val)) {
			vp = val;
			break;
		    } 
		}
		fclose(ofp);
		if (!vp)
		    fprintf (stderr, "%s: %s not found. Using %s\n",
						    mdovride, mdres, mydirdef);
	    }
	    if (!vp)
		vp = mydirdef;
	    vhome = expand_home(vp);
	    mydir = XtNewString (vhome);	/* macro! */
	    if (access (mydir, X_OK) < 0 && mkdir (mydir, 0744) < 0) {
		sprintf (buf, "%s: %s", mydir, syserrstr());
		xe_msg (buf, 1);
		XtFree (mydir);
		mydir = "?";
	    }
	}

	return (mydir);
}

/* return full path of of per-user resource file.
 */
char *
userResFile ()
{
	static char *myres;

	if (!myres) {
	    char *pd = getPrivateDir();
	    myres = malloc (strlen(pd) + strlen(myclass) + 2); /* '/'+'\0' */
	    sprintf (myres, "%s/%s", pd, myclass);
	}

	return (myres);
}

static void
create_srshell()
{
	Widget srform_w;
	Widget t_w, w;
	char buf[256];
	Arg args[20];
	int n;

	/* create shell and form */

	n = 0;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle, "xephem Save"); n++;
	XtSetArg (args[n], XmNiconName, "Save"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	srshell_w = XtCreatePopupShell ("SaveRes", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (srshell_w, XmNcolormap, (XtArgVal)xe_cm);
	sr_reg (srshell_w, "XEphem*SaveRes.width", prefsavecategory, 0);
	sr_reg (srshell_w, "XEphem*SaveRes.height", prefsavecategory, 0);
	sr_reg (srshell_w, "XEphem*SaveRes.x", prefsavecategory, 0);
	sr_reg (srshell_w, "XEphem*SaveRes.y", prefsavecategory, 0);

	n = 0;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNfractionBase, 13); n++;
	srform_w = XmCreateForm (srshell_w, "SRForm", args, n);
	XtAddCallback (srform_w, XmNhelpCallback, sr_help_cb, 0);
	XtManageChild (srform_w);

	/* controls at bottom */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 1); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 3); n++;
	w = XmCreatePushButton (srform_w, "Save", args, n);
	XtAddCallback (w, XmNactivateCallback, sr_save_cb, NULL);
	wtip (w, "Write the selected resources to disk");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 4); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 6); n++;
	w = XmCreatePushButton (srform_w, "Refresh", args, n);
	wtip (w, "Mark resources which now differ from those last saved");
	XtAddCallback (w, XmNactivateCallback, sr_refresh_cb, NULL);
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 7); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 9); n++;
	close_w = XmCreatePushButton (srform_w, "Close", args, n);
	wtip (close_w, "Close this window");
	XtAddCallback (close_w, XmNactivateCallback, sr_close_cb, NULL);
	XtManageChild (close_w);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 10); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 12); n++;
	w = XmCreatePushButton (srform_w, "Help", args, n);
	wtip (w, "More information about this window");
	XtAddCallback (w, XmNactivateCallback, sr_help_cb, 0);
	XtManageChild (w);

	/* title */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	t_w = XmCreateLabel (srform_w, "Title", args, n);
	sprintf (buf, "Save Selected Resources to\n%s", userResFile());
	set_xmstring (t_w, XmNlabelString, buf);
	XtManageChild (t_w);

	/* Major count */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNlabelType, XmPIXMAP); n++;
	XtSetArg (args[n], XmNmarginTop, 0); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	/* labelPixmap is set later */
	majorres_w = XmCreateLabel (srform_w, "ChgChk", args, n);
	XtManageChild (majorres_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, majorres_w); n++;
	majorn_w = XmCreateLabel (srform_w, "ChgL", args, n);
	/* label is set later */
	XtManageChild (majorn_w);

	    /* Autoselect TB */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, majorn_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 2); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftOffset, 30); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    asel_w = XmCreateToggleButton (srform_w, "AutoSel", args, n);
	    XtAddCallback (asel_w, XmNvalueChangedCallback, sr_asel_cb, NULL);
	    set_xmstring (asel_w, XmNlabelString, " Autoselect Major resources when they get modified");
	    wtip (asel_w,
	      "Whether modified Major resources are selected for Saving");
	    XtManageChild (asel_w);

	    /* Autosave TB */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNtopWidget, asel_w); n++;
	    XtSetArg (args[n], XmNtopOffset, 2); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftOffset, 30); n++;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    asav_w = XmCreateToggleButton (srform_w, "AutoSave", args, n);
	    set_xmstring (asav_w, XmNlabelString, " Autosave changed Major resources when Quitting");
	    wtip (asav_w,
		"Silently save all selected Major resources when Quit");
	    XtManageChild (asav_w);
	
	/* minor count */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, asav_w); n++;
	XtSetArg (args[n], XmNtopOffset, 4); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNlabelType, XmPIXMAP); n++;
	XtSetArg (args[n], XmNmarginTop, 0); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	/* labelPixmap is set later */
	minorres_w = XmCreateLabel (srform_w, "ChgPChk", args, n);
	XtManageChild (minorres_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, asav_w); n++;
	XtSetArg (args[n], XmNtopOffset, 4); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, minorres_w); n++;
	minorn_w = XmCreateLabel (srform_w, "ChgL", args, n);
	/* label is set later */
	XtManageChild (minorn_w);

	/* the big scrolled window for all the resources */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, minorres_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, close_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNscrollingPolicy, XmAUTOMATIC); n++;
	srsw_w = XmCreateScrolledWindow (srform_w, "SaveSW", args, n);
	XtManageChild (srsw_w);

	    /* add one dummy until first display */
	    n = 0;
	    w = XmCreateRowColumn (srsw_w, "RCD", args, n);
	    XtManageChild (w);
}

/* set majorn_w and minorn_w with message of counts */
static void
sr_setnnew (nanew, ntnew)
int nanew;
int ntnew;
{
	char buf[128];

	sprintf (buf, "%3d Major resource%s been modified since last Save",
				    nanew, nanew == 1 ? " has" : "s have");
	set_xmstring (majorn_w, XmNlabelString, buf);
	sprintf (buf, "%3d Minor resource%s been modified since last Save",
				    ntnew, ntnew == 1 ? " has" : "s have");
	set_xmstring (minorn_w, XmNlabelString, buf);
}

/* one-time stuff */
static void
sr_init()
{
	int i;

	/* be harmless if called more than once */
	if (srshell_w)
	    return;

	/* create GUI */
	create_srshell();
	create_srcshell();
	create_srfshell();

	/* register the shadow resources we control */
	for (i = 0; i < XtNumber(shadRes); i++)
	    sr_reg (NULL, shadRes[i], prefshadcategory, 1);

	/* register the modal and prompt background resources we control */
	for (i = 0; i < XtNumber(modalRes); i++)
	    sr_reg (NULL, modalRes[i], prefcolrcategory, 1);

	/* register the Help resources we control */
	for (i = 0; i < XtNumber(helpRes); i++)
	    sr_reg (NULL, helpRes[i], helpcategory, 0);

	/* create the pixmaps */
	sr_createpms();

	/* handy */
	nmyclass = strlen (myclass);

	/* register our own stuff too */
	sr_reg (asav_w, NULL, prefsavecategory, 1);
	sr_reg (asel_w, NULL, prefsavecategory, 1);
	for (i = 0; i < XtNumber(fchoices); i++) {
	    Choice *fcp = &fchoices[i];
	    if (fcp->gap)
		continue;
	    if (fcp->res && fcp->reg)
		sr_reg((Widget)0, fcp->res, preffontcategory, fcp->autosav);
	    if (fcp->res2 && fcp->reg2)
		sr_reg((Widget)0, fcp->res2, preffontcategory, fcp->autosav2);
	}
	for (i = 0; i < XtNumber(cchoices); i++) {
	    Choice *ccp = &cchoices[i];
	    if (ccp->gap)
		continue;
	    if (ccp->res && ccp->reg)
		sr_reg((Widget)0, ccp->res, prefcolrcategory, ccp->autosav);
	    if (ccp->res2 && ccp->reg2)
		sr_reg((Widget)0, ccp->res2, prefcolrcategory, ccp->autosav2);
	}

	/* init resource counts */
	sr_setnnew (0, 0);
}

/* (re)create the pixmaps */
static void
sr_createpms()
{
	Display *dsp = XtDisplay(toplevel_w);
	Window win = RootWindow(dsp, DefaultScreen(dsp));
	Pixel fg, bg;
	int d;

	/* sneak info from Close button.
	 * turns out Motif 1.2 can not handle a Bitmap and 2.x can, but we
	 * go the conservative route.
	 */
	get_something (close_w, XmNforeground, (XtArgVal)&fg);
	get_something (close_w, XmNbackground, (XtArgVal)&bg);
	get_something (close_w, XmNdepth, (XtArgVal)&d);

	/* create the pixmaps for the directory layout */

#if 0	/* TODO: freeing these confuses their widgets when destroyed */
	if (more_pm)
	    XFreePixmap (dsp, more_pm);
	if (nomore_pm)
	    XFreePixmap (dsp, nomore_pm);
	if (majorres_pm)
	    XFreePixmap (dsp, majorres_pm);
	if (minorres_pm)
	    XFreePixmap (dsp, minorres_pm);
	if (blankres_pm)
	    XFreePixmap (dsp, blankres_pm);
#endif

	more_pm = XCreatePixmapFromBitmapData (dsp, win, more_bits,
				    more_width, more_height, fg, bg, d);
	nomore_pm = XCreatePixmapFromBitmapData (dsp, win, nomore_bits,
				    nomore_width, nomore_height, fg, bg, d);
	majorres_pm = XCreatePixmapFromBitmapData (dsp, win, majorres_bits,
				    majorres_width, majorres_height, fg, bg, d);
	minorres_pm = XCreatePixmapFromBitmapData (dsp, win, minorres_bits,
				minorres_width, minorres_height, fg, bg, d);
	blankres_pm = XCreatePixmapFromBitmapData (dsp, win, blankres_bits,
				blankres_width, blankres_height, fg, bg, d);

	/* now can set sample checkmark */
	set_something (majorres_w, XmNlabelPixmap, (XtArgVal)majorres_pm);
	set_something (minorres_w, XmNlabelPixmap, (XtArgVal)minorres_pm);
}

/* ARGSUSED */
static void
sr_save_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	watch_cursor(1);

	/* save then exit if pending */
	if (pendingexit) {
	    if (!sr_save(0))
		exit(0);
	    /* trouble saving, so stand down */
	    pendingexit = 0;
	} else {
	    (void) sr_save(1);
	    sr_refresh();
	    sr_display();
	}

	watch_cursor(0);
}

/* ARGSUSED */
static void
sr_refresh_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	watch_cursor(1);
        sr_refresh();
        sr_display();
	watch_cursor(0);
}

/* ARGSUSED */
static void
sr_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	pendingexit = 0;
        XtPopdown (srshell_w);
}

/* ARGSUSED */
static void
sr_asel_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	watch_cursor(1);
	sr_refresh();
	sr_display();
	watch_cursor(0);
}

/* callback from the Help button.
 */
/* ARGSUSED */
static void
sr_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
static char *help_msg[] = {
"Display resources changed since last Save and allowing saving.",
};
	hlp_dialog ("Save", help_msg, sizeof(help_msg)/sizeof(help_msg[0]));
}

/* callback from a Category expand TB.
 * client is pointer to Resource.
 */
/* ARGSUSED */
static void
sr_catexp_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Resource *rp = (Resource *)client;

	catlist[rp->cati].exp = XmToggleButtonGetState(w);
	sr_display();
}

/* callback from a Resource Save TB.
 * client is pointer to Resource.
 */
/* ARGSUSED */
static void
sr_ressav_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Resource *rp = (Resource *)client;
	rp->save = XmToggleButtonGetState(w);
}

/* compare two Resources, in qsort fashion.
 * first sort by category, then by fb, pushing all !autosav after all autosav.
 */
static int
cmpRes (r1, r2)
Resource *r1, *r2;
{
	Category *c1 = &catlist[r1->cati];
	Category *c2 = &catlist[r2->cati];
	int c = strcmp (c1->name, c2->name);

	if (c)
	    return (c);
	if (r1->autosav == r2->autosav)
	    return (strcmp (r1->fb, r2->fb));
	if (r1->autosav)
	    return (-1);
	return (1);
}

/* add one entry to rc_w for the Category used by rp */
static void
sr_1cat (rc_w, rp)
Widget rc_w;
Resource *rp;
{
	Widget s_w, x_w, l_w, fo_w;
	Category *cp = &catlist[rp->cati];
	Pixmap pm;
	Arg args[20];
	int n;

	/* form */
	n = 0;
	XtSetArg (args[n], XmNhorizontalSpacing, 4); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	fo_w = XmCreateForm (rc_w, "CatF", args, n);
	XtManageChild (fo_w);

	/* set pixmap depending on totals */
	if (cp->nanew > 0)
	    pm = majorres_pm;
	else if (cp->ntnew > 0)
	    pm = minorres_pm;
	else
	    pm = blankres_pm;

	/* label to show whether any are new */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNlabelType, XmPIXMAP); n++;
	XtSetArg (args[n], XmNlabelPixmap, pm); n++;
	s_w = XmCreateLabel (fo_w, "CatC", args, n);
	wtip (s_w, "Checked if this category has any new unsaved resources");
	XtManageChild (s_w);

	/* expand TB */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, s_w); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNmarginWidth, 0); n++;
	XtSetArg (args[n], XmNlabelType, XmPIXMAP); n++;
	XtSetArg (args[n], XmNlabelPixmap, more_pm); n++;
	XtSetArg (args[n], XmNselectPixmap, nomore_pm); n++;
	XtSetArg (args[n], XmNindicatorOn, False); n++;
	XtSetArg (args[n], XmNset, cp->exp); n++;
	x_w = XmCreateToggleButton (fo_w, "CatD", args, n);
	wtip (x_w, "Show or Hide specific resources for this category");
	XtAddCallback (x_w, XmNvalueChangedCallback,sr_catexp_cb,(XtPointer)rp);
	XtManageChild (x_w);

	/* category name label */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, x_w); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	l_w = XmCreateLabel (fo_w, "CatL", args, n);
	set_xmstring (l_w, XmNlabelString, cp->name);
	XtManageChild (l_w);
}

/* add one entry to rc_w for the Resource rp */
static void
sr_1res (rc_w, rp, center)
Widget rc_w;
Resource *rp;
int center;
{
	Widget s_w, l_w, v_w, fo_w;
	Arg args[20];
	Pixmap pm;
	char nam[MRNAM];
	int n;

	/* form */
	n = 0;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	fo_w = XmCreateForm (rc_w, "ResF", args, n);
	XtManageChild (fo_w);

	/* determine pixmap */
	if (rp->new)
	    pm = rp->autosav ? majorres_pm : minorres_pm;
	else
	    pm = blankres_pm;

	/* "new" label */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 44); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNlabelType, XmPIXMAP); n++;
	XtSetArg (args[n], XmNlabelPixmap, pm); n++;
	l_w = XmCreateLabel (fo_w, "ResC", args, n);
	XtManageChild (l_w);

	/* save TB */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, l_w); n++;
	XtSetArg (args[n], XmNleftOffset, 4); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
	XtSetArg (args[n], XmNindicatorOn, True); n++;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	XtSetArg (args[n], XmNspacing, 4); n++;
	XtSetArg (args[n], XmNset, !!rp->save); n++;	/* bitfield */
	s_w = XmCreateToggleButton (fo_w, "ResTB", args, n);
	XtAddCallback (s_w, XmNvalueChangedCallback, sr_ressav_cb,
								(XtPointer)rp);
	XtManageChild (s_w);
	wtip (s_w, "Whether to Save this resource");

	/* value "label" -- use TB just to get same color */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, center); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNindicatorOn, False); n++;
	v_w = XmCreateToggleButton (fo_w, "ResV", args, n);
	XtManageChild (v_w);

	/* label with current name and val */
	crackNam (rp->fb, nam);
	set_xmstring (s_w, XmNlabelString, nam);
	set_xmstring (v_w, XmNlabelString, rp->val);
}

/* make a non-autosave header */
static void
sr_1nonas (rc_w)
Widget rc_w;
{
	Widget w, fo_w;
	Arg args[20];
	int n;

	/* form */
	n = 0;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	fo_w = XmCreateForm (rc_w, "ResHF", args, n);
	XtManageChild (fo_w);

	/* "Not saved!" label */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, blankres_width + 25 + 4); n++;
	XtSetArg (args[n], XmNmarginHeight, 2); n++;
	w = XmCreateLabel (fo_w, "ResHL", args, n);
	set_xmstring (w, XmNlabelString,
	    "The following Minor Resources are never automaticaly selected for Saving:");
	XtManageChild (w);
}

/* build a report from reslist[] and replace as workwindow in srsw_w.
 * N.B. reslist[] assumed to be in cmpRes() sorted order.
 */
static void
sr_display()
{
	Widget ww;
	Arg args[20];
	Resource *rp;
	Category *lastcp;
	Dimension size;
	int lastas;
	int n;

	/* admit it is some work */
	watch_cursor(1);

	/* get location for values */
	get_something (srshell_w, XmNwidth, (XtArgVal)&size);
	size = 2*size/3;

	/* replace workWindow */
	get_something (srsw_w, XmNworkWindow, (XtArgVal)&ww);
	XtDestroyWidget (ww);
	n = 0;
	XtSetArg (args[n], XmNspacing, 0); n++;
	ww = XmCreateRowColumn (srsw_w, "SRRC", args, n);
	set_something (srsw_w, XmNworkWindow, (XtArgVal)ww);

	/* fill with each category and resource */
	lastcp = NULL;
	lastas = 0;
	for (rp = reslist; rp < &reslist[nreslist]; rp++) {
	    if (lastcp != &catlist[rp->cati]) {
		lastcp = &catlist[rp->cati];
		sr_1cat (ww, rp);
		lastas = 1;
	    } 
	    if (lastcp->exp) {
		if (!rp->autosav && lastas) {
		    /* put header over first non-autosav */
		    sr_1nonas (ww);
		    lastas = 0;
		}
		sr_1res (ww, rp, size);
	    }
	}

	/* show it and done */
	XtManageChild (ww);
	watch_cursor (0);
}

/* given a full resource spec, extract name, sans any surrounding white space.
 * return 0 if ok, or -1 if doesn't even look like a resource.
 */
static int
crackNam (res, nam)
char *res, *nam;
{
	char *colp = strchr (res, ':');
	char wsnam[MRNAM];

	if (!colp || strncmp (res, myclass, nmyclass))
	    return (-1);
	sprintf (wsnam, "%.*s", colp-res, res);
	cpyNoWS (nam, wsnam);
	return(0);
}

/* given a full resource spec, extract value, sans any surrounding white space.
 * return 0 if ok, or -1 if doesn't even look like a resource.
 */
static int
crackVal (res, val)
char *res, *val;
{
	char *colp = strchr (res, ':');

	if (!colp || strncmp (res, myclass, nmyclass))
	    return (-1);
	cpyNoWS (val, colp+1);
	return (0);
}

/* find the fallback that corresponds to w.
 * TODO: using XtNameToWidget precludes cases of multiple instances, eg plot.
 */
static char *
findWFB (findw)
Widget findw;
{
	char wnam[MRNAM];
	Widget w;
	char *fb, **fbp;
	char *findn;
	char *dot;

	/* fallback[] must at least contain root name */
	findn = XtName(findw);

	/* scan fallbacks */
	for (fbp = fallbacks; (fb = *fbp) != NULL; fbp++) {
	    if (!strstr(fb, findn))
		continue;
	    crackNam (fb, wnam);
	    /* chop off widget's own resource, if any */
	    dot = strrchr (wnam, '.');
	    if (dot && (!strcmp (dot, ".value") || !strcmp (dot, ".set")
					    || !strcmp (dot, ".labelString")))
		*dot = '\0';
	    w = XtNameToWidget (toplevel_w, wnam+nmyclass);
	    if (w == findw)
		return (fb);
	}

	printf ("Bug! No fallback for widget %s\n", fullWName(findw, wnam));
	exit (1);
}

/* find the fallback that corresponds to res.
 */
static char *
findRFB (res)
char *res;
{
	char fbnam[MRNAM];
	char resnam[MRNAM];
	char **fbp;

	/* create full res name */
	if (strncmp (res, myclass, nmyclass))
	    sprintf (resnam, "%s.%s", myclass, res);
	else
	    strcpy (resnam, res);

	/* scan fallbacks */
	for (fbp = fallbacks; *fbp; fbp++) {
	    crackNam (*fbp, fbnam);
	    if (!strcmp (fbnam, resnam))
		return (*fbp);
	}

	printf ("Bug! No fallback for resource %s\n", res);
	exit (1);
}

/* print "nam":"val" into res; try to look nice */
static void
fmtRes (res, nam, val)
char *res, *nam, *val;
{
	int rl;

	rl = sprintf (res, "%s: ", nam);
	if (rl < RESWID)
	    sprintf (res+rl, "%*s%s", RESWID-rl, "", val);
	else
	    sprintf (res+rl, "%s", val);
}

/* copy from[] to to[], sans any white space on either end.
 */
static void
cpyNoWS (to, from)
char *to;
char *from;
{
	char *lastnwsp;		/* last non w/s char in to not counting '\0' */

	while (isspace(*from))
	    from++;
	for (lastnwsp = NULL; (*to = *from) != '\0'; to++, from++)
	    if (!isspace(*to))
		lastnwsp = to;
	if (lastnwsp)
	    *++lastnwsp = '\0';
}

/* search for resource with the given name.
 * if find, return pointer else NULL.
 * TODO: speed this up somehow making use of knowing it is in cmpRes() order?
 */
static Resource *
findRes (findnam)
char *findnam;
{
	char nam[MRNAM];
	int i;

	for (i = 0; i < nreslist; i++) {
	    (void) crackNam (reslist[i].fb, nam);
	    if (!strcmp (findnam, nam))
		return (&reslist[i]);
	}

	/* nope */
	return (NULL);
}

/* put full name of w into buf[] */
static char *
fullWName (w, buf)
Widget w;
char *buf;
{
	if (w == toplevel_w)
	    sprintf (buf, "%s", XtName(w));
	else {
	    Widget pw = XtParent (w);
	    buf = fullWName (pw, buf);
	    sprintf (buf+strlen(buf), ".%s", XtName(w));
	}
	return (buf);
}

/* scan catlist[] and return index of entry with name of cat, making a new
 * entry if none currently exist.
 */
static int
findCat (cat)
char *cat;
{
	Category *cp;

	for (cp = catlist; cp < &catlist[ncatlist]; cp++)
	    if (!strcmp (cat, cp->name))
		return (cp - catlist);

	catlist = (Category *) XtRealloc ((char *)catlist,
						(ncatlist+1)*sizeof(Category));
	cp = &catlist[ncatlist++];
	memset (cp, 0, sizeof(*cp));
	cp->name = cat;
	return (cp - catlist);
}

/* called once for each resource in entire database.
 * if find a match to _xrmNam copy value to xrmVal and return True,
 * else return False.
 */
static char *_xrmNam;
static char *_xrmVal;
/* stolen from appres.c and Xlib Xrm.c */
/*ARGSUSED*/
static Bool
DumpEntry(db, bindings, quarks, type, value, data)
XrmDatabase *db;
XrmBindingList bindings;
XrmQuarkList quarks;
XrmRepresentation *type;
XrmValuePtr value;
XPointer data;
{
	static XrmQuark qstring;
	char buf[1024];
	int l = 0;
	Bool firstNameSeen;

	if (!qstring)
	    qstring = XrmPermStringToQuark("String");

	if (*type != qstring)
	    return (False);

	/* build name */
	for (firstNameSeen = False; *quarks; bindings++, quarks++) {
	    if (*bindings == XrmBindLoosely) {
		l += sprintf (buf+l, "*");
	    } else if (firstNameSeen) {
		l += sprintf (buf+l, ".");
	    }
	    firstNameSeen = True;
	    l += sprintf (buf+l, "%s", XrmQuarkToString(*quarks));
	}

	/* compare to _xrmNam */
	if (!strcmp (buf, _xrmNam)) {
	    /* bingo! */
	    sprintf (_xrmVal, "%.*s", (int)value->size, value->addr);
	    return (True);
	}

	return (False);
}

/* search entire display resource database for entry matching nam[].
 * if find copy value to val[] and return 0, else return -1.
 */
static int
getXrmDB (nam, val)
char *nam, *val;
{
	XrmDatabase db = XtDatabase(XtDisplay(toplevel_w));
	XrmName names[101];
	XrmClass classes[101];
	Bool r;

	XrmStringToNameList("xephem", names);
	XrmStringToClassList(myclass, classes);

	/* call DumpEntry for each entry in display database */
	_xrmNam = nam;
	_xrmVal = val;
	r = XrmEnumerateDatabase(db, names, classes, XrmEnumAllLevels,
								DumpEntry, 0);

	return (r == True ? 0 : -1);
}

/* given a Shell widget, dig up to the root and find the x/y location of
 * the window manager's window. This is want is wanted in the *.x and *.y
 * resources (which are similar to *.geometry, hence the name).
 * if we run into trouble, we just return .x and .y from w.
 */
static void
getGeometry (w, xp, yp)
Widget w;
int *xp, *yp;
{
	Display *dsp = XtDisplay(w);
	Window win = XtWindow (w);
	XWindowAttributes xwa;;

	/* in case we get called before we are realized */
	if (!win) {
	    Position x, y;
    punt:
	    get_something (w, XmNx, (XtArgVal)&x);
	    get_something (w, XmNy, (XtArgVal)&y);
	    *xp = x;
	    *yp = y;
	    return;
	}

	/* move win up until win is child of root */
	while (1) {
	    Window *children = NULL;
	    unsigned int nchildren;
	    Window root, parent;

	    if (!XQueryTree (dsp, win, &root, &parent, &children, &nchildren))
		goto punt;
	    if (children)
		XFree((char *)children);
	    if (parent == root)
		break;
	    win = parent;
	}

	/* use win's location as shell's location */
	if (!XGetWindowAttributes(dsp, win, &xwa))
	    goto punt;
	*xp = xwa.x;
	*yp = xwa.y;
}

/* dig out the current value of rp as a string and fill val[].
 * exit if fails.
 */
static void
getCurVal (rp, val)
Resource *rp;
char val[];
{
	if (rp->live_w) {
	    Widget w = rp->live_w;

	    if (XmIsToggleButton(w)) {
		strcpy (val, XmToggleButtonGetState(w) ? "True" : "False");
	    } else if (XmIsLabel(w)) {
		char *txtp;
		get_xmstring (w, XmNlabelString, &txtp);
		cpyNoWS(val, txtp);
		XtFree(txtp);
	    } else if (XmIsTextField(w) || XmIsText(w)) {
		if (strstr (rp->fb, ".value")) {
		    char *txtp = XmTextGetString (w);
		    cpyNoWS (val, txtp);
		    XtFree(txtp);
		} else if (strstr (rp->fb, ".rows")) {
		    short n;
		    get_something (w, XmNrows, (XtArgVal)&n);
		    sprintf (val, "%d", n);
		} else if (strstr (rp->fb, ".columns")) {
		    short n;
		    get_something (w, XmNcolumns, (XtArgVal)&n);
		    sprintf (val, "%d", n);
		} else {
		    printf ("Bug! %s not supported from %s\n", rp->fb,
							    fullWName(w,val));
		    exit(1);
		}
	    } else if (XmIsScale(w)) {
		int i;
		XmScaleGetValue (w, &i);
		sprintf (val, "%d", i);
	    } else if (XtIsShell(w)) {
		Dimension d;
		int i;

		if (strstr (rp->fb, ".width")) {
		    get_something (w, XmNwidth, (XtArgVal)&d);
		    i = d;
		} else if (strstr (rp->fb, ".height")) {
		    get_something (w, XmNheight, (XtArgVal)&d);
		    i = d;
		} else if (strstr (rp->fb, ".x")) {
		    int tmp;
		    getGeometry (w, &i, &tmp);
		} else if (strstr (rp->fb, ".y")) {
		    int tmp;
		    getGeometry (w, &tmp, &i);
		} else {
		    printf ("Bug! Only w/h from Shell %s\n", fullWName(w,val));
		    exit(1);
		}
		sprintf (val, "%d", i);
	    } else {
		printf ("Bug! Unsupported Save type for %s\n",fullWName(w,val));
		exit (1);
	    }
	} else {
	    /* try app resource, then whole display db */
	    char *vp, nam[MRNAM];

	    crackNam (rp->fb, nam);
	    if (nam[nmyclass]=='.' && (vp= getXRes(nam+nmyclass+1,NULL))!=NULL)
		strcpy (val, vp);
	    else if (getXrmDB (nam, val) < 0) {
		printf ("Bug! No Save value for %s\n", rp->fb);
		exit(1);
	    }
	}
}

/* starting with w, load it and all children for which (*isf)(w) is true
 * with loadargs[]
 */
static void
loadArgsChildren (w, isf)
Widget w;
int (*isf)();
{
	if (XtIsComposite (w)) {
	    WidgetList children;
	    Cardinal numChildren;
	    int i;

	    get_something (w, XmNchildren, (XtArgVal)&children);
	    get_something (w, XmNnumChildren, (XtArgVal)&numChildren);
	    for (i = 0; i < (int)numChildren; i++)
		loadArgsChildren (children[i], isf);
	    for (i = 0; i < (int)w->core.num_popups; i++)
		loadArgsChildren (w->core.popup_list[i], isf);
	}

	if ((*isf)(w))
	    XtSetValues (w, loadargs, nloadargs);
}

/* Font control */

/* display the named font in srfsample_w */
static void
srf_showsample (xlfd)
char *xlfd;
{
	Display *dsp = XtDisplay (toplevel_w);
	XFontStruct *fsp;
	XmFontList fl;
	Arg args[20];
	int n;

	fsp = XLoadQueryFont (dsp, xlfd);
	fl = XmFontListCreate (fsp, XmSTRING_DEFAULT_CHARSET);
	n = 0;
	XtSetArg (args[n], XmNfontList, fl); n++;
	XtSetValues (srfsample_w, args, n);
	XmFontListFree (fl);
}

/* add the given xlfd to the scrolled history list if not already present */
static void
srf_addhistory (xlfd)
char *xlfd;
{
	XmString xms = XmStringCreateSimple (xlfd);
	if (!XmListItemExists (srfhl_w, xms))
	    XmListAddItem (srfhl_w, xms, 0);
	XmStringFree (xms);
}

/* perform the Get-and-show current or default actions for the Choice */
static void
srf_getshow (fcp)
Choice *fcp;
{
	Resource *rp = findRes (fcp->res);
	char buf[MLL];
	char *xlfd;

	if (XmToggleButtonGetState (fgetc_w)) {
	    /* get current */
	    getCurVal (rp, buf);
	    xlfd = buf;
	} else {
	    /* get default */
	    xlfd = rp->lsv;
	}

	/* show */
	XmTextFieldSetString (srftf_w, xlfd);
	srf_showsample (xlfd);
	srf_addhistory (xlfd);
}

/* install xlfd in res and maybe res2 in database and our history list.
 * return XFontStruct if ok else NULL if xlfd is bogus.
 * N.B. caller should not XFreeFont with returned value, even if it does not
 *   need it immediately; doing so will blow when the using widget runs;
 *   evidently it must remain loaded.
 */
static XFontStruct *
srf_install (res, res2, xlfd)
char *res, *res2;
char *xlfd;
{
	Display *dsp = XtDisplay(toplevel_w);
	XrmDatabase db = XrmGetDatabase (dsp);
	XFontStruct *fsp = XLoadQueryFont (dsp, xlfd);
	Resource *rp;
	char buf[MLL];

	/* check that xlfd is real */
	if (!fsp) {
	    sprintf (buf, "%s:\nfont not found", xlfd);
	    xe_msg (buf, 1);
	    return(NULL);
	}

	/* install in db for new */
	fmtRes (buf, res, xlfd);
	XrmPutLineResource (&db, buf);
	if (res2) {
	    fmtRes (buf, res2, xlfd);
	    XrmPutLineResource (&db, buf);
	}

	/* add lsv to history if new */
	rp = findRes (res);
	srf_addhistory (rp->lsv);
	if (res2) {
	    rp = findRes (res2);
	    srf_addhistory (rp->lsv);
	}

	/* add to history if new */
	srf_addhistory (xlfd);

	/* ok */
	return (fsp);
}

/* called when a class button is activated for a font.
 * client is pointer to a fchoice
 */
/* ARGSUSED */
static void
srf_go_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Choice *fcp = (Choice *)client;
	int setdef;

	if ((setdef = XmToggleButtonGetState (fsetd_w))) {
	    Resource *rp = findRes (fcp->res);
	    XmTextFieldSetString (srftf_w, rp->lsv);
	}

	if (setdef || XmToggleButtonGetState (fappto_w)) {
	    char *xlfd = XmTextFieldGetString (srftf_w);
	    XFontStruct *fsp;

	    /* save in db and show in history */
	    fsp = srf_install (fcp->res, fcp->res2, xlfd);
	    if (fsp) {
		XmFontList fl = XmFontListCreate (fsp,XmSTRING_DEFAULT_CHARSET);

		/* distribute to existing widgets */
		loadargs = (Arg *) XtMalloc (sizeof(Arg));
		nloadargs = 0;
		XtSetArg (loadargs[nloadargs], XmNfontList, fl); nloadargs++;
		loadArgsChildren (toplevel_w, fcp->isf);
		XtFree ((char *)loadargs);
		XmFontListFree (fl);

		/* spread the word */
		ng_newres();
	    }

	    /* done */
	    XtFree (xlfd);
	} else
	    srf_getshow (fcp);
}

/* called to change any one app defined font resource.
 * client is pointer to a Choice
 * ccp->res2 is really a pointer to the *_newres() function to call.
 */
/* ARGSUSED */
static void
srf_appres_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Choice *fcp = (Choice *)client;
	int setdef;

	if ((setdef = XmToggleButtonGetState (fsetd_w))) {
	    Resource *rp = findRes (fcp->res);
	    XmTextFieldSetString (srftf_w, rp->lsv);
	}

	if (setdef || XmToggleButtonGetState (fappto_w)) {
	    char *fnam = XmTextFieldGetString (srftf_w);

	    /* save in db and show in history, then tell */
	    if (srf_install (fcp->res, NULL, fnam) && fcp->res2)
		((void (*)())fcp->res2)();

	    /* done */
	    XtFree (fnam);
	} else
	    srf_getshow (fcp);
}

/* called when viewsFont is to be set.
 * client is pointer to a Choice
 */
/* ARGSUSED */
static void
srf_views_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Choice *fcp = (Choice *)client;
	int setdef;

	if ((setdef = XmToggleButtonGetState (fsetd_w))) {
	    Resource *rp = findRes (fcp->res);
	    XmTextFieldSetString (srftf_w, rp->lsv);
	}

	if (setdef || XmToggleButtonGetState (fappto_w)) {
	    char *xlfd = XmTextFieldGetString (srftf_w);
	    XFontStruct *fsp;

	    /* save in db and show in history, then install and tell all */
	    fsp = srf_install (fcp->res, fcp->res2, xlfd);
	    if (fsp) {
		/* install */
		set_views_font(XtDisplay(w), fsp);

		/* spread the word */
		ng_newres();
		m_newres();
		mars_newres();
		ss_newres();
		sv_newres();
	    }

	    /* done */
	    XtFree (xlfd);
	} else
	    srf_getshow (fcp);
}

/* called when cursorTrackingFont is to be set.
 * client is pointer to a Choice
 */
/* ARGSUSED */
static void
srf_tracking_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Choice *fcp = (Choice *)client;
	int setdef;

	if ((setdef = XmToggleButtonGetState (fsetd_w))) {
	    Resource *rp = findRes (fcp->res);
	    XmTextFieldSetString (srftf_w, rp->lsv);
	}

	if (setdef || XmToggleButtonGetState (fappto_w)) {
	    char *xlfd = XmTextFieldGetString (srftf_w);
	    XFontStruct *fsp;

	    /* save in db and show in history */
	    fsp = srf_install (fcp->res, fcp->res2, xlfd);
	    if (fsp) {
		/* install */
		set_tracking_font(XtDisplay(w), fsp);

		/* spread the word */
		e_newres();
		sv_newres();
	    }

	    /* done */
	    XtFree (xlfd);
	} else
	    srf_getshow (fcp);
}

/* called when moonsFont is to be set.
 * client is pointer to a fchoice
 */
/* ARGSUSED */
static void
srf_moons_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Choice *fcp = (Choice *)client;
	int setdef;

	if ((setdef = XmToggleButtonGetState (fsetd_w))) {
	    Resource *rp = findRes (fcp->res);
	    XmTextFieldSetString (srftf_w, rp->lsv);
	}

	if (setdef || XmToggleButtonGetState (fappto_w)) {
	    char *xlfd = XmTextFieldGetString (srftf_w);
	    XFontStruct *fsp;

	    /* save in db and show in history */
	    fsp = srf_install (fcp->res, fcp->res2, xlfd);
	    if (fsp) {
		/* install */
		set_views_font(XtDisplay(w), fsp);

		/* spread the word */
		mars_newres();
		sm_newres();
		jm_newres();
		um_newres();
	    }

	    /* done */
	    XtFree (xlfd);
	} else
	    srf_getshow (fcp);
}

/* called when an item in the font history list is selected */
/* ARGSUSED */
static void
srf_hist_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        XmListCallbackStruct *lp = (XmListCallbackStruct *)call;
	char *txt;

	XmStringGetLtoR (lp->item, XmSTRING_DEFAULT_CHARSET, &txt);
	XmTextFieldSetString (srftf_w, txt);
	srf_showsample (txt);
	XtFree (txt);
}

/* called to clear font text field */
/* ARGSUSED */
static void
srf_clear_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        XmTextFieldSetString (srftf_w, "");
}

/* called to Search for the font named in the text field.
 * N.B. used by a PushButton and a TextField so don't use call.
 */
/* ARGSUSED */
static void
srf_search_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int nfonts;
	Display *dsp = XtDisplay (toplevel_w);
        char *xlfd = XmTextFieldGetString (srftf_w);
	char **fonts = XListFonts (dsp, xlfd, 1, &nfonts);

	if (nfonts > 0) {
	    /* show in All list */
	    XmString str = XmStringCreateSimple (fonts[0]);
	    XmListSelectItem (srfaf_w, str, False);
	    XmListSetItem (srfaf_w, str);
	    XmStringFree (str);

	    /* show sample and add to history list */
	    srf_showsample (xlfd);
	    srf_addhistory (xlfd);
	} else {
	    char buf[1024];
	    sprintf (buf, "%s:\nfont not found", xlfd);
	    xe_msg (buf, 1);
	}

	if (fonts)
	    XFreeFontNames (fonts);
	XtFree (xlfd);
}

/* called to clear font history list */
/* ARGSUSED */
static void
srf_clrhistory_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        XmListDeleteAllItems (srfhl_w);
}

/* called to close font chooser */
/* ARGSUSED */
static void
srf_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        XtPopdown (srfshell_w);
}

/* callback from the font Help button.
 */
/* ARGSUSED */
static void
srf_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
static char *fhelp_msg[] = {
"Type a font, then install into the specified widgets.",
};
	hlp_dialog ("Save - fonts", fhelp_msg,
				    sizeof(fhelp_msg)/sizeof(fhelp_msg[0]));
}

static int
cmpStr (p1, p2)
qsort_arg p1;
qsort_arg p2;
{
	return (strcmp (*(char **)p1, *(char **)p2));
}

/* fill the scrolled list with names of all available fonts */
static void
srf_fillall (sl_w)
Widget sl_w;
{
	Display *dsp = XtDisplay (toplevel_w);
	char **names, **namescpy;
	int i, nnames;

	/* get all font names */
	names = XListFonts (dsp, "*", 99999, &nnames);
	if (!nnames) {
	    xe_msg ("No fonts!", 1);
	    return;
	}

	/* seems XFreeFontNames doesn't like the names to be rearranged
	 * so must sort a copy.
	 */
	namescpy = (char **)XtMalloc (nnames*sizeof(char *));
	for (i = 0; i < nnames; i++)
	    namescpy[i] = XtNewString (names[i]);
	qsort ((void *)namescpy, nnames, sizeof(char *), cmpStr);
	for (i = 0; i < nnames; i++) {
	    XmString str = XmStringCreateSimple (namescpy[i]);
	    XmListAddItem (sl_w, str, 0);
	}
	for (i = 0; i < nnames; i++)
	    XtFree(namescpy[i]);
	XtFree ((char *)namescpy);
	XFreeFontNames (names);
}

/* called when an item in the font list is selected */
/* ARGSUSED */
static void
srf_sel_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        XmListCallbackStruct *lp = (XmListCallbackStruct *)call;
	char *txt;

	XmStringGetLtoR (lp->item, XmSTRING_DEFAULT_CHARSET, &txt);
	XmTextFieldSetString (srftf_w, txt);
	srf_showsample (txt);
	XtFree (txt);
}

/* build the collection of Choice buttons in the given form. */
static void
create_buttons (f_w, cp, ncp)
Widget f_w;
Choice cp[];
int ncp;
{
	Widget w, rc_w;
	Choice *end;
	Arg args[20];
	int n;

	/* build in 1 RC */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNspacing, 3); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNadjustLast, False); n++;
	XtSetArg (args[n], XmNnumColumns, ncp/4); n++;
	rc_w = XmCreateRowColumn (f_w, "SCRC", args, n);
	XtManageChild (rc_w);

	for (end = cp + ncp; cp < end; cp++) {
	    if (cp->gap) {
		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		w = XmCreateLabel (rc_w, "Fill", args, n);
		set_xmstring (w, XmNlabelString, cp->title ? cp->title : "");
		XtManageChild (w);
	    } else {
		n = 0;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
		w = XmCreatePushButton (rc_w, "SCPB", args, n);
		set_xmstring (w, XmNlabelString, cp->title);
		XtAddCallback (w, XmNactivateCallback, cp->cb, (XtPointer)cp);
		wtip (w, cp->tip);
		XtManageChild (w);
		cp->w = w;
	    }
	}
}

static void
create_srfshell()
{
	Widget mf_w, ht_w, bf_w, w;
	Widget tl_w, al_w;
	Widget rb_w;
	Arg args[20];
	int n;

	/* create shell and form */

	n = 0;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle, "xephem Fonts"); n++;
	XtSetArg (args[n], XmNiconName, "Fonts"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	srfshell_w = XtCreatePopupShell ("Fonts", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (srfshell_w, XmNcolormap, (XtArgVal)xe_cm);
	sr_reg (srfshell_w, "XEphem*Fonts.x", preffontcategory, 0);
	sr_reg (srfshell_w, "XEphem*Fonts.y", preffontcategory, 0);

	/* master form */

	n = 0;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	mf_w = XmCreateForm (srfshell_w, "SRFMF", args, n);
	XtAddCallback (mf_w, XmNhelpCallback, srf_help_cb, 0);
	XtManageChild (mf_w);

	/* start with controls at bottom so font list grows with window */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 20); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 40); n++;
	w = XmCreatePushButton (mf_w, "Close", args, n);
	wtip (w, "Close this window");
	XtAddCallback (w, XmNactivateCallback, srf_close_cb, NULL);
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 60); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 80); n++;
	w = XmCreatePushButton (mf_w, "Help", args, n);
	wtip (w, "More info about this window");
	XtAddCallback (w, XmNactivateCallback, srf_help_cb, NULL);
	XtManageChild (w);

	/* form for overall button collection */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, w); n++;
	XtSetArg (args[n], XmNbottomOffset, 20); n++;
	bf_w = XmCreateForm (mf_w, "SFRC", args, n);
	XtManageChild (bf_w);

	create_buttons (bf_w, fchoices, XtNumber(fchoices));

	/* "target" label next up */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, bf_w); n++;
	XtSetArg (args[n], XmNbottomOffset, 4); n++;
	tl_w = XmCreateLabel (mf_w, "TFL", args, n);
	set_xmstring (tl_w, XmNlabelString, "Then choose target(s):");
	XtManageChild (tl_w);

	/* RC of action TBs next up */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 20); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, tl_w); n++;
	XtSetArg (args[n], XmNspacing, 8); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	rb_w = XmCreateRadioBox (mf_w, "SCRB", args, n);
	XtManageChild (rb_w);

	    n = 0;
	    XtSetArg (args[n], XmNset, True); n++;
	    XtSetArg (args[n], XmNspacing, 4); n++;
	    fgetc_w = XmCreateToggleButton (rb_w, "Get", args, n);
	    set_xmstring (fgetc_w, XmNlabelString, "Get current");
	    wtip (fgetc_w,
	    		"Pressing a button below retrieves that current font");
	    XtManageChild (fgetc_w);

	    n = 0;
	    XtSetArg (args[n], XmNspacing, 4); n++;
	    fgetd_w = XmCreateToggleButton (rb_w, "GDef", args, n);
	    set_xmstring (fgetd_w, XmNlabelString, "Get default");
	    wtip (fgetd_w,"Pressing a button below retrieves its default font");
	    XtManageChild (fgetd_w);

	    n = 0;
	    XtSetArg (args[n], XmNspacing, 4); n++;
	    fappto_w = XmCreateToggleButton (rb_w, "Set", args, n);
	    set_xmstring (fappto_w, XmNlabelString, "Set");
	    wtip (fappto_w, "Pressing a button below sets that font");
	    XtManageChild (fappto_w);

	    n = 0;
	    XtSetArg (args[n], XmNspacing, 4); n++;
	    fsetd_w = XmCreateToggleButton (rb_w, "SDef", args, n);
	    set_xmstring (fsetd_w, XmNlabelString, "Restore default");
	    wtip (fsetd_w, "Pressing a button below restores its default font");
	    XtManageChild (fsetd_w);

	/* "action" label next up */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, rb_w); n++;
	XtSetArg (args[n], XmNbottomOffset, 4); n++;
	al_w = XmCreateLabel (mf_w, "TFL", args, n);
	set_xmstring (al_w, XmNlabelString, "Set action first:");
	XtManageChild (al_w);

	/* label for sample */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, al_w); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	srfsample_w = XmCreateLabel (mf_w, "TFL", args, n);
	set_xmstring (srfsample_w, XmNlabelString,
"\
ABCDEFGHIJKLMNOPQRSTUVWXYZ\n\
abcdefghijklmnopqrstuvwxyz\n\
01234567890 ±²³´µ¶·¸¹°");

	XtManageChild (srfsample_w);

	/* TF for new font */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, srfsample_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	srftf_w = XmCreateTextField (mf_w, "Font", args, n);
	XtAddCallback (srftf_w, XmNactivateCallback, srf_search_cb, NULL);
	wtip (srftf_w, "Entered or retrieved font name");
	XtManageChild (srftf_w);

	/* font instructions next up */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, srftf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	w = XmCreateLabel (mf_w, "SRFST", args, n);
	set_xmstring (w, XmNlabelString, "Font:");
	XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNbottomWidget, srftf_w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNleftWidget, w); n++;
	    XtSetArg (args[n], XmNleftOffset, 10); n++;
	    w = XmCreatePushButton (mf_w, "Clear", args, n);
	    XtAddCallback (w, XmNactivateCallback, srf_clear_cb, NULL);
	    wtip (w, "Erase font text field below");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNbottomWidget, srftf_w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNleftWidget, w); n++;
	    XtSetArg (args[n], XmNleftOffset, 10); n++;
	    w = XmCreatePushButton (mf_w, "Search", args, n);
	    XtAddCallback (w, XmNactivateCallback, srf_search_cb, NULL);
	    wtip (w, "Search all fonts for one matching pattern");
	    XtManageChild (w);

	/* history scrolled list */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNvisibleItemCount, 8); n++;
	XtSetArg (args[n], XmNselectionPolicy, XmBROWSE_SELECT); n++;
	srfhl_w = XmCreateScrolledList (mf_w, "CBox", args, n);
	XtAddCallback (srfhl_w, XmNbrowseSelectionCallback, srf_hist_cb, 0);
	wtip (srfhl_w, "Click on an entry to copy to font text field below");
	XtManageChild (srfhl_w);

	/* history title */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, srfhl_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	w = XmCreateLabel (mf_w, "Title", args, n);
	set_xmstring (w, XmNlabelString, "Font history:");
	XtManageChild (w);

	    /* PB to clear list */

	    n = 0;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNbottomWidget, srfhl_w); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNleftWidget, w); n++;
	    XtSetArg (args[n], XmNleftOffset, 10); n++;
	    ht_w = XmCreatePushButton (mf_w, "Clear", args, n);
	    XtAddCallback (ht_w, XmNactivateCallback, srf_clrhistory_cb, 0);
	    wtip (ht_w, "Erase the font history");
	    XtManageChild (ht_w);

	/* the top title */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	w = XmCreateLabel (mf_w, "Title", args, n);
	set_xmstring (w, XmNlabelString, "All available fonts:");
	XtManageChild (w);

	/* scrolled list of all fonts */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, ht_w); n++;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNvisibleItemCount, 8); n++;
	XtSetArg (args[n], XmNselectionPolicy, XmBROWSE_SELECT); n++;
	srfaf_w = XmCreateScrolledList (mf_w, "SRFALL", args, n);
	XtAddCallback (srfaf_w, XmNbrowseSelectionCallback, srf_sel_cb, 0);
	wtip (srfaf_w, "Click on an entry to copy to font text field below");
	srf_fillall (srfaf_w);
	XtManageChild (srfaf_w);

}

/* Color control */

/* display the named color in the sample area and maybe set scales to match */
static void
src_showcolor (name, scalestoo)
char *name;
int scalestoo;
{
	Display *dsp = XtDisplay(toplevel_w);
	Window win = XtWindow (srcda_w);
	XColor defxc, dbxc;
	int rh, gs, bv;

	if (!win)
	    return;
	if (!XAllocNamedColor (dsp, xe_cm, name, &defxc, &dbxc)) {
	    xe_msg ("Can not find that color", 1);
	    return;
	}
	XSetWindowBackground (dsp, win, defxc.pixel);
	XClearWindow (dsp, win);
	XFreeColors (dsp, xe_cm, &defxc.pixel, 1, 0);

	if (!scalestoo)
	    return;

	if (XmToggleButtonGetState (srcrgb_w)) {
	    rh = MAXSCALE*defxc.red/65535;
	    gs = MAXSCALE*defxc.green/65535;
	    bv = MAXSCALE*defxc.blue/65535;
	} else {
	    double h, s, v;
	    toHSV (defxc.red/65535.0, defxc.green/65535.0, defxc.blue/65535.0,
								    &h, &s, &v);
	    rh = (int)(h*MAXSCALE+.5);
	    gs = (int)(s*MAXSCALE+.5);
	    bv = (int)(v*MAXSCALE+.5);
	}

	XmScaleSetValue (srcRH_w, rh);
	XmScaleSetValue (srcGS_w, gs);
	XmScaleSetValue (srcBV_w, bv);
}

/* perform the Get-and-show current or default actions for the cchoice */
static void
src_getshow (ccp)
Choice *ccp;
{
	Resource *rp = findRes (ccp->res);
	XmString xms;
	char buf[MLL];
	char *cnam;

	if (XmToggleButtonGetState (cgetc_w)) {
	    /* get current */
	    getCurVal (rp, buf);
	    cnam = buf;
	} else {
	    /* get default */
	    cnam = rp->lsv;
	}

	/* show */
	XmTextFieldSetString (srctf_w, cnam);
	src_showcolor (cnam, 1);
	xms = XmStringCreateSimple (cnam);
	if (!XmListItemExists (srcsl_w, xms))
	    XmListAddItem (srcsl_w, xms, 0);
	XmStringFree (xms);
}

/* load w and its children with the background colors assoc with bg. */
static void
src_setbg (w, bg, isf)
Widget w;
Pixel bg;
int (*isf)();
{
	Display *dsp = XtDisplay(w);
	Screen *scr = DefaultScreenOfDisplay(dsp);
	Pixel fg, ts, bs, sl;

	/* get corresponding 3d colors */
	XmGetColors (scr, xe_cm, bg, &fg, &ts, &bs, &sl);

	/* distribute to existing widgets */
	loadargs = (Arg *) XtMalloc (20 * sizeof(Arg));
	nloadargs = 0;
	XtSetArg (loadargs[nloadargs], XmNbackground, bg); nloadargs++;
	XtSetArg (loadargs[nloadargs], XmNtopShadowColor, ts); nloadargs++;
	XtSetArg (loadargs[nloadargs], XmNbottomShadowColor,bs);nloadargs++;
	XtSetArg (loadargs[nloadargs], XmNselectColor, sl); nloadargs++;
	XtSetArg (loadargs[nloadargs], XmNtroughColor, sl); nloadargs++;
	loadArgsChildren (w, isf);
	XtFree ((char *)loadargs);
}

/* install colorname in res{,2} in database and our history list */
static void
src_install (res, res2, cnam)
char *res, *res2;
char *cnam;
{
	Display *dsp = XtDisplay(toplevel_w);
	XrmDatabase db = XrmGetDatabase (dsp);
	Resource *rp;
	XmString xms;
	char buf[MLL];

	/* install in db for new */
	fmtRes (buf, res, cnam);
	XrmPutLineResource (&db, buf);
	if (res2) {
	    fmtRes (buf, res2, cnam);
	    XrmPutLineResource (&db, buf);
	}

	/* add lsv to history if new */
	rp = findRes (res);
	xms = XmStringCreateSimple (rp->lsv);
	if (!XmListItemExists (srcsl_w, xms))
	    XmListAddItem (srcsl_w, xms, 0);
	XmStringFree (xms);
	if (res2) {
	    rp = findRes (res2);
	    xms = XmStringCreateSimple (rp->lsv);
	    if (!XmListItemExists (srcsl_w, xms))
		XmListAddItem (srcsl_w, xms, 0);
	    XmStringFree (xms);
	}

	/* add cnam to history if new */
	xms = XmStringCreateSimple (cnam);
	if (!XmListItemExists (srcsl_w, xms))
	    XmListAddItem (srcsl_w, xms, 0);
	XmStringFree (xms);

	/* make sure showing on scales */
	src_showcolor (cnam, 1);
}

/* set backgrounds in each existing modal shell in existing widgets for which
 * (*isf)() is true and update database for new.
 */
static void
src_setmodal (bg, cnam, isf)
Pixel bg;
char *cnam;
int (*isf)();
{
	int i;

	for (i = 0; i < XtNumber(modalRes); i++) {
	    char *mri = modalRes[i];
	    char *bgstr = strstr (mri, "background");
	    if (bgstr) {
		char nam[MRNAM];
		Widget mw;

		sprintf (nam, "%.*s", bgstr-1-mri-nmyclass, mri+nmyclass);
		mw = XtNameToWidget(toplevel_w, nam);
		if (mw)
		    src_setbg (mw, bg, isf);
		src_install (mri, NULL, cnam);
	    }
	}
}

/* return 1 if w should have its shadowThickess set, else 0.
 * e.g. TBs should not because they use them creatively.
 * N.B. register the resources with sr_reg() first.
 */
static int
src_hasShadow (w)
Widget w;
{
	WidgetClass wc = XtClass(w);

	if (wc == xmPushButtonWidgetClass || wc == xmCascadeButtonWidgetClass
					  || wc == xmScaleWidgetClass
					  || wc == xmTextFieldWidgetClass
					  || wc == xmTextWidgetClass
					  || wc == xmListWidgetClass
					  /* xmFrame causes SRCFM bailout if
					   * change shadow thickness a few
					   * times with earth open (!)
					   */
					  || wc == xmScrollBarWidgetClass) {
	    return (1);
	}

	/* TB only if already using it */
	if (wc == xmToggleButtonWidgetClass) {
	    Dimension thick;

	    get_something (w, XmNshadowThickness, (XtArgVal)&thick);
	    return (thick > 0);
	}

	/* RC only if not Work Area */
	if (wc == xmRowColumnWidgetClass) {
	    unsigned char type;

	    get_something (w, XmNrowColumnType, (XtArgVal)&type);
	    return (type != XmWORK_AREA);
	}
		
	return (0);
}

/* called when a Background color button is activated.
 * we compute all the related colors too if installing.
 * client is pointer to a cchoice[]
 */
/* ARGSUSED */
static void
src_bg_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Choice *ccp = (Choice *)client;
	int setdef;

	if ((setdef = XmToggleButtonGetState (csetd_w))) {
	    Resource *rp = findRes (ccp->res);
	    XmTextFieldSetString (srctf_w, rp->lsv);
	}

	if (setdef || XmToggleButtonGetState (cappto_w)) {
	    Display *dsp = XtDisplay(w);
	    char *cnam = XmTextFieldGetString (srctf_w);
	    XColor defxc, dbxc;
	    Pixel bg;

	    /* set new bg color */
	    if (!XAllocNamedColor (dsp, xe_cm, cnam, &defxc, &dbxc)) {
		xe_msg ("Can not find that color", 1);
		XtFree (cnam);
		return;
	    }
	    bg = defxc.pixel;
	    src_setbg (toplevel_w, bg, ccp->isf);

	    /* save in db and show in history.
	     * include all bg-related resources too if we are "All"
	     */
	    src_install (ccp->res, ccp->res2, cnam);
	    if (ccp->isf == isAny) {
		Choice *acp;
		for (acp= cchoices; acp < &cchoices[XtNumber(cchoices)]; acp++){
		    if (acp->gap)
			continue;
		    if (acp->cb == src_bg_cb && acp->isf != isAny)
			src_install (acp->res, acp->res2, cnam);
		    if (acp->cb == src_modal_cb)
			src_setmodal (bg, cnam, acp->isf);
		}
		
		/* and some really special cases */
		src_install ("XEphem.EarthBackground",  NULL,cnam);  e_newres();
		src_install ("XEphem.MoonBackground",   NULL,cnam);  m_newres();
		src_install ("XEphem.MarsBackground", NULL,cnam); mars_newres();
		src_install ("XEphem.JupiterBackground",NULL,cnam); jm_newres();
		src_install ("XEphem.SaturnBackground", NULL,cnam); sm_newres();
		src_install ("XEphem.UranusBackground", NULL,cnam); um_newres();
	    }

	    /* other special needs */
	    setButtonInfo();
	    all_selection_mode (0);	/* TODO: what if selecting? */
	    sr_createpms();
	    sr_display();
	    calm_newres();
	    mm_newres();
	    ss_newres();
	    sv_newres();

	    /* done */
	    XtFree (cnam);
	    XFreeColors (dsp, xe_cm, &bg, 1, 0);
	} else
	    src_getshow (ccp);

}

/* called when a class button is activated for a foreground color.
 * client is pointer to a Choice
 */
/* ARGSUSED */
static void
src_fg_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Choice *ccp = (Choice *)client;
	int setdef;

	if ((setdef = XmToggleButtonGetState (csetd_w))) {
	    Resource *rp = findRes (ccp->res);
	    XmTextFieldSetString (srctf_w, rp->lsv);
	}

	if (setdef || XmToggleButtonGetState (cappto_w)) {
	    Display *dsp = XtDisplay(w);
	    char *cnam = XmTextFieldGetString (srctf_w);
	    XColor defxc, dbxc;
	    Pixel fg;

	    /* get new color */
	    if (!XAllocNamedColor (dsp, xe_cm, cnam, &defxc, &dbxc)) {
		xe_msg ("Can not find that color", 1);
		XtFree (cnam);
		return;
	    }
	    fg = defxc.pixel;

	    /* distribute to existing widgets */
	    loadargs = (Arg *) XtMalloc (20 * sizeof(Arg));
	    nloadargs = 0;
	    XtSetArg (loadargs[nloadargs], XmNforeground, fg); nloadargs++;
	    loadArgsChildren (toplevel_w, ccp->isf);
	    XtFree ((char *)loadargs);

	    /* save in db and show in history */
	    src_install (ccp->res, ccp->res2, cnam);

	    /* tell others too */
	    calm_newres();
	    mm_newres();
	    sv_newres();

	    /* done */
	    XtFree (cnam);
	    XFreeColors (dsp, xe_cm, &fg, 1, 0);
	} else
	    src_getshow(ccp);
}

/* called when a single object (planet, star) resource is to be changed then
 * call make_objgcs() then inform all the other views.
 * client is pointer to a Choice
 */
/* ARGSUSED */
static void
src_obj_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Choice *ccp = (Choice *)client;
	int setdef;

	if ((setdef = XmToggleButtonGetState (csetd_w))) {
	    Resource *rp = findRes (ccp->res);
	    XmTextFieldSetString (srctf_w, rp->lsv);
	}

	if (setdef || XmToggleButtonGetState (cappto_w)) {
	    char *cnam = XmTextFieldGetString (srctf_w);

	    /* save in db and show in history */
	    src_install (ccp->res, ccp->res2, cnam);

	    /* make the new GCs */
	    make_objgcs();

	    /* tell others */
	    sv_newres();
	    e_newres();
	    ng_newres();
	    ss_newres();
	    sm_newres();
	    jm_newres();
	    mars_newres();
	    um_newres();

	    /* done */
	    XtFree (cnam);
	} else
	    src_getshow (ccp);
}

/* called to change any one app defined resource.
 * client is pointer to a Choice
 * ccp->res2 is really a pointer to the *_newres() function to call.
 */
/* ARGSUSED */
static void
src_appres_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Choice *ccp = (Choice *)client;
	int setdef;

	if ((setdef = XmToggleButtonGetState (csetd_w))) {
	    Resource *rp = findRes (ccp->res);
	    XmTextFieldSetString (srctf_w, rp->lsv);
	}

	if (setdef || XmToggleButtonGetState (cappto_w)) {
	    char *cnam = XmTextFieldGetString (srctf_w);

	    /* save in db and show in history */
	    src_install (ccp->res, NULL, cnam);

	    /* inform by calling the proper *_newres() */
	    if (ccp->res2)
		((void (*)())ccp->res2)();

	    /* done */
	    XtFree (cnam);
	} else
	    src_getshow (ccp);
}

/* called when an item in the color history list is selected */
/* ARGSUSED */
static void
src_hist_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        XmListCallbackStruct *lp = (XmListCallbackStruct *)call;
	char *txt;

	XmStringGetLtoR (lp->item, XmSTRING_DEFAULT_CHARSET, &txt);
	XmTextFieldSetString (srctf_w, txt);
	src_showcolor (txt, 1);
	XtFree (txt);
}

/* called when shadow thickness TB changes */
/* ARGSUSED */
static void
src_shadow_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Display *dsp = XtDisplay(toplevel_w);
	XrmDatabase db = XrmGetDatabase (dsp);
	int thick = XmToggleButtonGetState(w) ? 1 : 2;
	char tbuf[32];
	char res[MLL];
	int i;

	/* distribute to existing */
	loadargs = (Arg *) XtMalloc (20 * sizeof(Arg));
	nloadargs = 0;
	XtSetArg (loadargs[nloadargs], XmNshadowThickness, thick); nloadargs++;
	loadArgsChildren (toplevel_w, src_hasShadow);
	XtFree ((char *)loadargs);

	/* install in db for new */
	sprintf (tbuf, "%d", thick);
	for (i = 0; i < XtNumber(shadRes); i++) {
	    fmtRes (res, shadRes[i], tbuf);
	    XrmPutLineResource (&db, res);
	}
}

/* called when the Prompt BG changes.
 * client is pointer into cchoices[] for our entry.
 */
/* ARGSUSED */
static void
src_modal_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Choice *ccp = (Choice *)client;
	int setdef;

	if ((setdef = XmToggleButtonGetState (csetd_w))) {
	    Resource *rp = findRes (ccp->res);
	    XmTextFieldSetString (srctf_w, rp->lsv);
	}

	if (setdef || XmToggleButtonGetState (cappto_w)) {
	    Display *dsp = XtDisplay(w);
	    char *cnam = XmTextFieldGetString (srctf_w);
	    XColor defxc, dbxc;
	    Pixel bg;

	    /* get new bg color */
	    if (!XAllocNamedColor (dsp, xe_cm, cnam, &defxc, &dbxc)) {
		xe_msg ("Can not find that color", 1);
		XtFree (cnam);
		return;
	    }
	    bg = defxc.pixel;

	    /* set current and future */
	    src_setmodal(bg, cnam, ccp->isf);

	} else {
	    ccp->res = modalRes[0];	/* typical */
	    src_getshow (ccp);
	}
}

/* called to clear color text list */
/* ARGSUSED */
static void
src_clrl_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        XmListDeleteAllItems (srcsl_w);
}

/* called to close color chooser */
/* ARGSUSED */
static void
src_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        XtPopdown (srcshell_w);
}

/* called when ENTER  is typed in the color text field */
/* ARGSUSED */
static void
src_enter_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        char *txt = XmTextFieldGetString (w);
	src_showcolor (txt, 1);
	XtFree (txt);
}

/* called when RGB TB changes */
/* ARGSUSED */
static void
src_rgb_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char *rgbstr;

	/* just redo scales.. nothing else changes */
	rgbstr = XmTextFieldGetString (srctf_w);
	src_showcolor (rgbstr, 1);
	XtFree (rgbstr);
}

/* called while any RGB/HSV scale is being dragged */
/* ARGSUSED */
static void
src_scale_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int r, g, b;
	char buf[32];

	XmScaleGetValue (srcRH_w, &r);
	XmScaleGetValue (srcGS_w, &g);
	XmScaleGetValue (srcBV_w, &b);

	if (!XmToggleButtonGetState (srcrgb_w)) {
	    double rd, gd, bd;
	    toRGB ((double)r/MAXSCALE, (double)g/MAXSCALE, (double)b/MAXSCALE,
								&rd, &gd, &bd);
	    r = (int)(MAXSCALE*rd+.5);
	    g = (int)(MAXSCALE*gd+.5);
	    b = (int)(MAXSCALE*bd+.5);
	}

	sprintf(buf, "#%02x%02x%02x", 255*r/MAXSCALE, 255*g/MAXSCALE,
							    255*b/MAXSCALE);
	XmTextFieldSetString (srctf_w, buf);
	src_showcolor (buf, 0);
}

/* callback from the color Help button */
/* ARGSUSED */
static void
src_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
static char *chelp_msg[] = {
"Type a color name, then install into the specified widgets.",
};
	hlp_dialog ("Save - colors", chelp_msg,
				    sizeof(chelp_msg)/sizeof(chelp_msg[0]));
}

/* called to start color picker */
/* ARGSUSED */
static void
cpk_grab_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static Cursor wc;
	Display *dsp = XtDisplay(w);
	Window root = RootWindow(dsp, DefaultScreen(dsp));
	Window rw, cw;
	int rx, ry;
	unsigned int m;
	int x, y;
	XColor xcol;
	Pixel pix;
	XImage *xim;
	char cnam[32];

	/* make roaming cursor */
	if (!wc)
	    wc = XCreateFontCursor (dsp, XC_crosshair);

	/* grab pointer and show color until see button1 */
	if (XGrabPointer (dsp, root, False, ButtonPressMask, GrabModeAsync,
		    GrabModeSync, None, wc, CurrentTime) != GrabSuccess) {
	    xe_msg ("Could not grab pointer", 1);
	    return;
	}
	xcol.pixel = 0;
	do {
	    if (!XQueryPointer (dsp, root, &rw, &cw, &rx, &ry, &x, &y, &m)){
		xe_msg ("XQueryPointer error", 1);
		break;
	    }

	    /* pull out pixel */
	    xim = XGetImage (dsp, root, x, y, 1, 1, ~0, ZPixmap);
	    if (!xim) {
		xe_msg ("XGetImage error", 1);
		break;
	    }
	    pix = XGetPixel (xim, 0, 0);
	    XDestroyImage (xim);

	    /* avoid jitters while same color */
	    if (pix == xcol.pixel)
		continue;
	    xcol.pixel = pix;

	    /* convert to rgb */
	    XQueryColor (dsp, xe_cm, &xcol);
	    sprintf (cnam, "#%02x%02x%02x", xcol.red>>8, xcol.green>>8,
								xcol.blue>>8);
	    XmTextFieldSetString (srctf_w, cnam);
	    src_showcolor (cnam, 1);
	} while (!(m & Button1Mask));

	XUngrabPointer (dsp, CurrentTime);
}

static void
create_srcshell()
{
	Display *dsp = XtDisplay(toplevel_w);
	Widget mf_w, ti_w, fr_w, top_w, ttl_w, rb_w;
	Widget tl_w, al_w;
	Widget bf_w;
	Widget w;
	char val[MRVAL];
	Arg args[20];
	int n;

	/* create shell and form */

	n = 0;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle, "xephem Colors"); n++;
	XtSetArg (args[n], XmNiconName, "Colors"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	srcshell_w = XtCreatePopupShell ("Colors", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (srcshell_w, XmNcolormap, (XtArgVal)xe_cm);
	sr_reg (srcshell_w, "XEphem*Colors.x", prefcolrcategory, 0);
	sr_reg (srcshell_w, "XEphem*Colors.y", prefcolrcategory, 0);

	/* master form */

	n = 0;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	mf_w = XmCreateForm (srcshell_w, "SRCMF", args, n);
	XtAddCallback (mf_w, XmNhelpCallback, src_help_cb, 0);
	XtManageChild (mf_w);

	/* top title */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	top_w = XmCreateLabel (mf_w, "Title", args, n);
	set_xmstring (top_w, XmNlabelString, "Color history:");
	XtManageChild (top_w);

	/* font history in a scrolled list. want it to grow with window
	 * build remaining widget bottom-to-top and attach in middle.
	 */

	/* controls at bottom */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 20); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 40); n++;
	w = XmCreatePushButton (mf_w, "Close", args, n);
	wtip (w, "Close this window");
	XtAddCallback (w, XmNactivateCallback, src_close_cb, NULL);
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 60); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 80); n++;
	w = XmCreatePushButton (mf_w, "Help", args, n);
	wtip (w, "More info about this window");
	XtAddCallback (w, XmNactivateCallback, src_help_cb, NULL);
	XtManageChild (w);

	/* form for overall button collection */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, w); n++;
	XtSetArg (args[n], XmNbottomOffset, 20); n++;
	bf_w = XmCreateForm (mf_w, "BF", args, n);
	XtManageChild (bf_w);

	create_buttons (bf_w, cchoices, XtNumber(cchoices));

	/* "Target" label next up */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, bf_w); n++;
	XtSetArg (args[n], XmNbottomOffset, 4); n++;
	tl_w = XmCreateLabel (mf_w, "TL", args, n);
	set_xmstring (tl_w, XmNlabelString, "Then choose target(s):");
	XtManageChild (tl_w);

	/* RC of TBs next up */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 20); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, tl_w); n++;
	XtSetArg (args[n], XmNspacing, 4); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	rb_w = XmCreateRadioBox (mf_w, "SCRB", args, n);
	XtManageChild (rb_w);

	    n = 0;
	    XtSetArg (args[n], XmNset, True); n++;
	    XtSetArg (args[n], XmNspacing, 4); n++;
	    cgetc_w = XmCreateToggleButton (rb_w, "Get", args, n);
	    set_xmstring (cgetc_w, XmNlabelString, "Get current");
	    wtip (cgetc_w,
	    		"Pressing a button below retrieves that current color");
	    XtManageChild (cgetc_w);

	    n = 0;
	    XtSetArg (args[n], XmNspacing, 4); n++;
	    cgetd_w = XmCreateToggleButton (rb_w, "GDef", args, n);
	    set_xmstring (cgetd_w, XmNlabelString, "Get default");
	    wtip (cgetd_w,
	    		"Pressing a button below retrieves its default color");
	    XtManageChild (cgetd_w);

	    n = 0;
	    XtSetArg (args[n], XmNspacing, 4); n++;
	    cappto_w = XmCreateToggleButton (rb_w, "Set", args, n);
	    set_xmstring (cappto_w, XmNlabelString, "Set");
	    wtip (cappto_w, "Pressing a button below sets that color");
	    XtManageChild (cappto_w);

	    n = 0;
	    XtSetArg (args[n], XmNspacing, 4); n++;
	    csetd_w = XmCreateToggleButton (rb_w, "SDef", args, n);
	    set_xmstring (csetd_w, XmNlabelString, "Restore default");
	    wtip (csetd_w,"Pressing a button below restores its default color");
	    XtManageChild (csetd_w);

	/* "Action" label next up */

	n = 0;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, rb_w); n++;
	XtSetArg (args[n], XmNbottomOffset, 4); n++;
	al_w = XmCreateLabel (mf_w, "TL", args, n);
	set_xmstring (al_w, XmNlabelString, "Set action first:");
	XtManageChild (al_w);

	/* TB for shadow thickness on the right */

	getXrmDB (shadRes[0], val);	/* typical */

	n = 0;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, al_w); n++;
	XtSetArg (args[n], XmNbottomOffset, 0); n++;
	XtSetArg (args[n], XmNset, atoi(val) == 1); n++;
	w = XmCreateToggleButton (mf_w, "SCSH", args, n);
	set_xmstring (w, XmNlabelString, "Shadow Thickness 1");
	wtip (w, "Shadow thickness is 1 when set, else 2");
	XtAddCallback (w, XmNvalueChangedCallback, src_shadow_cb, 0);
	XtManageChild (w);

	/* TF for new color */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, al_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 45); n++;
	srctf_w = XmCreateTextField (mf_w, "Color", args, n);
	XtAddCallback (srctf_w, XmNactivateCallback, src_enter_cb, NULL);
	XmTextFieldSetString (srctf_w, "#000000"); /* match scales */
	wtip (srctf_w, "Entered or retrieved color name or value");
	XtManageChild (srctf_w);

	/* color instructions next up */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, srctf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	ti_w = XmCreateLabel (mf_w, "SRCST", args, n);
	set_xmstring (ti_w, XmNlabelString, "Color name or spec:");
	XtManageChild (ti_w);

	/* color Picker PB */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, srctf_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 45); n++;
	cpicker_w = XmCreatePushButton (mf_w, "Grab", args, n);
	wtip (cpicker_w,"Grab color under cursor until press Button 1");
	XtAddCallback (cpicker_w, XmNactivateCallback, cpk_grab_cb, 0);
	XtManageChild (cpicker_w);

	/* history scrolled list in top left */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, ti_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 45); n++;
	XtSetArg (args[n], XmNvisibleItemCount, 8); n++;
	XtSetArg (args[n], XmNselectionPolicy, XmBROWSE_SELECT); n++;
	srcsl_w = XmCreateScrolledList (mf_w, "CBox", args, n);
	XtAddCallback (srcsl_w, XmNbrowseSelectionCallback, src_hist_cb, 0);
	wtip (srcsl_w, "Click on an entry to copy to color text field below");
	XtManageChild (srcsl_w);

	    /* sneak in a clear */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 45); n++;
	    w = XmCreatePushButton (mf_w, "Clear", args, n);
	    XtAddCallback (w, XmNactivateCallback, src_clrl_cb, 0);
	    wtip (w, "Erase the color history");
	    XtManageChild (w);

	/* scales, toggle and DA in middle too */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 55); n++;
	ttl_w = XmCreateLabel (mf_w, "ST", args, n);
	set_xmstring (ttl_w, XmNlabelString, "Color setting:");
	XtManageChild(ttl_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNspacing, 4); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_TIGHT); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	rb_w = XmCreateRadioBox (mf_w, "RHRB", args, n);
	XtManageChild(rb_w);

	    n = 0;
	    XtSetArg (args[n], XmNset, True); n++;
	    XtSetArg (args[n], XmNspacing, 3); n++;
	    srcrgb_w = XmCreateToggleButton (rb_w, "RGB", args, n);
	    XtAddCallback (srcrgb_w, XmNvalueChangedCallback, src_rgb_cb, 0);
	    wtip (srcrgb_w, "Scales are in RGB");
	    XtManageChild (srcrgb_w);

	    n = 0;
	    XtSetArg (args[n], XmNset, False); n++;
	    XtSetArg (args[n], XmNspacing, 3); n++;
	    w = XmCreateToggleButton (rb_w, "HSV", args, n);
	    wtip (w, "Scales are in HSV");
	    XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, srcsl_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, al_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 55); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	fr_w = XmCreateFrame (mf_w, "DAF", args, n);
	XtManageChild (fr_w);

	    /* create black drawing area to match initial scale settings */
	    n = 0;
	    XtSetArg (args[n], XmNbackground, BlackPixel(dsp,
						    DefaultScreen(dsp))); n++;
	    srcda_w = XmCreateDrawingArea (fr_w, "DA", args, n);
	    wtip (srcda_w, "Sample color patch of above setting");
	    XtManageChild (srcda_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, ttl_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, fr_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 55); n++;
	XtSetArg (args[n], XmNshowValue, True); n++;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNminimum, 0); n++;
	XtSetArg (args[n], XmNmaximum, MAXSCALE); n++;
	XtSetArg (args[n], XmNvalue, 0); n++;
	XtSetArg (args[n], XmNscaleMultiple, 1); n++;
	srcRH_w = XmCreateScale (mf_w, "RHScale", args, n);
	XtAddCallback (srcRH_w, XmNdragCallback, src_scale_cb, NULL);
	XtAddCallback (srcRH_w, XmNvalueChangedCallback, src_scale_cb, NULL);
	wtip (srcRH_w, "Slide to adjust Red or Hue");
	XtManageChild(srcRH_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, ttl_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, fr_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 70); n++;
	XtSetArg (args[n], XmNshowValue, True); n++;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNminimum, 0); n++;
	XtSetArg (args[n], XmNmaximum, MAXSCALE); n++;
	XtSetArg (args[n], XmNvalue, 0); n++;
	XtSetArg (args[n], XmNscaleMultiple, 1); n++;
	srcGS_w = XmCreateScale (mf_w, "GSScale", args, n);
	XtAddCallback (srcGS_w, XmNdragCallback, src_scale_cb, NULL);
	XtAddCallback (srcGS_w, XmNvalueChangedCallback, src_scale_cb, NULL);
	wtip (srcGS_w, "Slide to adjust Green or Saturation");
	XtManageChild(srcGS_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, ttl_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, fr_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 85); n++;
	XtSetArg (args[n], XmNshowValue, True); n++;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNminimum, 0); n++;
	XtSetArg (args[n], XmNmaximum, MAXSCALE); n++;
	XtSetArg (args[n], XmNvalue, 0); n++;
	XtSetArg (args[n], XmNscaleMultiple, 1); n++;
	srcBV_w = XmCreateScale (mf_w, "BVScale", args, n);
	XtAddCallback (srcBV_w, XmNdragCallback, src_scale_cb, NULL);
	XtAddCallback (srcBV_w, XmNvalueChangedCallback, src_scale_cb, NULL);
	wtip (srcBV_w, "Slide to adjust the Blue or Brightness");
	XtManageChild(srcBV_w);
}

/* update the Help location db entry.
 * use current live widget, if any, else leave unchanged.
 */
static void
updateHelp()
{
	Widget helpw = XtNameToWidget (toplevel_w, "*HelpWindow");
	XrmDatabase db;
	char val[MRVAL];
	char buf[MLL];
	int x, y;

	/* if none open now, just reuse last */
	if (!helpw)
	    return;

	db = XrmGetDatabase (XtDisplay(toplevel_w));

	getGeometry (helpw, &x, &y);
	sprintf (val, "%d", x);
	fmtRes (buf, "XEphem*HelpWindow.x", val);
	XrmPutLineResource (&db, buf);
	sprintf (val, "%d", y);
	fmtRes (buf, "XEphem*HelpWindow.y", val);
	XrmPutLineResource (&db, buf);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: saveres.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
