/* code to manage the stuff on the "objx/y" menu.
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#if defined(__STDC__)
#include <stdlib.h>
typedef const void * qsort_arg;
#else
typedef void * qsort_arg;
#endif

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/ScrollBar.h>
#include <Xm/SelectioB.h>
#include <Xm/Separator.h>
#include <Xm/TextF.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "preferences.h"

extern Widget toplevel_w;
extern Colormap xe_cm;

extern Now *mm_get_now P_((void));
extern Obj *db_basic P_((int id));
extern Obj *db_scan P_((DBScan *sp));
extern char *obj_description P_((Obj *op));
extern int db_n P_((void));
extern int db_set_field P_((char bp[], int id, PrefDateFormat pref, Obj *op));
extern int isUp P_((Widget w));
extern int obj_cir P_((Now *np, Obj *op));
extern int skyloc_fifo P_((Obj *op));
extern void all_newobj P_((int dbidx));
extern void buttonAsButton P_((Widget w, int whether));
extern void db_scaninit P_((DBScan *sp, int mask, ObjF *op, int nop));
extern void db_setuserobj P_((int id, Obj *op));
extern void f_prdec P_((Widget w, double a));
extern void f_double P_((Widget w, char *fmt, double f));
extern void f_ra P_((Widget w, double ra));
extern void f_string P_((Widget w, char *s));
extern void fs_date P_((char out[], double jd));
extern void get_something P_((Widget w, char *resource, XtArgVal value));
extern void get_xmstring P_((Widget w, char *resource, char **txtp));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void prompt_map_cb P_((Widget w, XtPointer client, XtPointer call));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void sv_id P_((Obj *op));
extern void sv_point P_((Obj *op));
extern void watch_cursor P_((int want));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *msg, int app_modal));
extern void zero_mem P_((void *loc, unsigned len));

/* info about each field for a given type of object.
 * N.B. the prompt field is used to decide whether a button should be created
 *   to let this field be changed. Some prompts are determined at runtime
 *   though based on preference. For these, we need at least a dummy.
 *   the runtime setting is done in obj_change_cb().
 * N.B. the altp entries must match the values of the PREF_XX enums.
 */
typedef struct {
    int id;		/* field id */
    char *label;	/* label */
    char *prompt;	/* prompt if changeable, or 0 if it can't be */
    char *tip;		/* widget tip */
    char *altp[3];	/* alternate prompts for fields effected by prefs */
    Widget pb_w;	/* PushButton if it can be changed, else Label */
} Field;

static void init_working_obj P_((void));
static void obj_unsetmenu P_((void));
static void obj_set_from_db P_((Obj *op));
static void obj_set_db P_((void));
static void obj_setmenu P_((void));
static void obj_setxyz P_((void));
static void obj_setobjinfo P_((Field *fp, int nf));
static void obj_create_shell P_((void));
static void obj_create_type_rc_form P_((Widget fw, Field *fp));
static void obj_change_cb P_((Widget w, XtPointer client, XtPointer call));
static void obj_ctl_cb P_((Widget w, XtPointer client, XtPointer call));
static void do_help P_((void));
static void obj_xyz_cb P_((Widget w, XtPointer client, XtPointer call));
static void obj_type_cb P_((Widget w, XtPointer client, XtPointer call));
static void obj_select_cb P_((Widget w, XtPointer client, XtPointer call));
static void obj_scroll_cb P_((Widget w, XtPointer client, XtPointer call));
static void obj_srch_cb P_((Widget w, XtPointer client, XtPointer call));
static int srch_match P_((char *n1, char *n2));
static int nl_cmp P_((const void *nl1, const void *nl2));
static void new_namelist P_((void));
static void load_namelist P_((void));
static void obj_setnames P_((int nli));
static void prompt_ok_cb P_((Widget w, XtPointer client, XtPointer call));
static void prompt P_((Field *fp));
static void obj_set_button P_((Widget w, int id));
static void epoch_as_decimal P_((Widget w, double e));
static void epoch_as_mdy P_((Widget w, double e));

static Widget objshell_w;	/* overall shell */
static Widget fldrc_w;		/* overall field info rc */
static Widget scroll_w;		/* name scroll widget */
static Widget srch_w;		/* the search text field */

static Widget objx_w;		/* ObjX selection TB */
static Widget objy_w;		/* ObjY selection TB */
static Widget objz_w;		/* ObjZ selection TB */

static Field fixed_fields[] = {
    {O_NAME,  "Name:",  "Object name:", "Name of Object"},
    {F_CLASS, "Class:",	NULL, "Database type classifcation"},	/* TODO */
    {F_RA,    "RA:",	"RA (h:m:s):", "Right Ascension (epoch below)"},
    {F_DEC,   "Dec:",	"Dec (d:m:s):", "Declination (epoch below)"},
    {F_MAG,   "Mag:",	"Magnitude:", "Nominal magnitude"},
    {F_EPOCH, "Epoch:", "dummy", "Epoch of RA and Dec", {
	"Reference epoch\n(UT Date as m/d.d/y or year.d):",
	"Reference epoch\n(UT Date as y/m/d.d or year.d):",
	"Reference epoch\n(UT Date as d.d/m/y or year.d):"
	}
    },
    {F_SIZE,  "Size:",  "Angular Size (arc secs):", "Angular size"},
};

static Field ellip_fields[] = {
    {O_NAME, "Name:", "Object name:", "Name of Object"},
    {E_INC, "Inclination:", "Inclination (degs):", "Orbit inclination"},
    {E_LAN, "Long of Asc Nod:", "Longitude of ascending node (degs):",
    	"Longitude of ascending node"},
    {E_AOP, "Arg of Peri:", "Argument of Perihelion (degs):",
    	"Argument of Perihelion"},
    {E_A, "Semi-Axis:", "Semi-Major axis (AU):", "Semi-major axis of orbit"},
    {E_E, "Eccentricity:", "Eccentricty (<1):", "Orbital eccentricity"},
    {E_M, "Mean Anomaly:", "Mean Anomaly (degs):",
    	"Mean anomaly at moment of MA Epoch"},
    {E_CEPOCH, "MA Epoch:", "dummy", "Date of Mean Anomaly value", {
	"Epoch date of MA\n(UT Date as m/d.d/y or year.d):",
	"Epoch date of MA\n(UT Date as y/m/d.d or year.d):",
	"Epoch date of MA\n(UT Date as d.d/m/y or year.d):"
	}
    },
    {E_EPOCH, "Equinox:", "dummy", "Reference date of orbital parameters", {
	"Equinox year\n(UT Date as m/d.d/y or year.d):",
	"Equinox year\n(UT Date as y/m/d.d or year.d):",
	"Equinox year\n(UT Date as d.d/m/y or year.d):"
	}
    },
    {E_M1, "Mag coeff 1:", "Magnitude coefficient 1\n(g#, H# or just #):",
    	"First coefficient in magnitude model: g or H"},
    {E_M2, "Mag coeff 2:", "Magnitude coefficient 2\n(k#, G# or just #):",
    	"Second coefficient in magnitude model: k or G"},
    {E_SIZE, "Size:", "Angular Size @ 1 AU (arc secs):","Angular size at 1 AU"},
};

static Field hyper_fields[] = {
    {O_NAME, "Name:", "Object name:", "Name of Object"},
    {H_EP, "Ep of Peri:", "dummy", "Date of Perihelion", {
	"Epoch of perihelion\n(UT Date as m/d.d/y or year.d):",
	"Epoch of perihelion\n(UT Date as y/m/d.d or year.d):",
	"Epoch of perihelion\n(UT Date as d.d/m/y or year.d):"
	}
    },
    {H_INC, "Inclination:", "Inclination (degs):", "Orbital inclination"},
    {H_LAN, "Long of Asc Nod:", "Longitude of ascending node (degs):",
    	"Longitude of Ascending Node"},
    {H_AOP, "Arg of Peri:", "Argument of perihelion (degs):",
    	"Argument if Perihelion"},
    {H_E, "Eccentricity:", "Eccentricity (>1):", "Oribital eccentricty"},
    {H_QP, "Peri Dist:", "Perihelion distance (AU):", "Distance at perihelion"},
    {H_EPOCH, "Epoch:", "dummy", "Reference date of orbital parameters", {
	"Reference epoch\n(UT Date as m/d.d/y or year.d):",
	"Reference epoch\n(UT Date as y/m/d.d or year.d):",
	"Reference epoch\n(UT Date as d.d/m/y or year.d):"
	}
    },
    {H_G, "g:", "g, as in m = g + 5*log(delta) + 2.5*k*log(r):", 
    	"g magnitude model, as in m = g + 5*log(delta) + 2.5*k*log(r)"},
    {H_K, "k:", "k, as in m = g + 5*log(delta) + 2.5*k*log(r):",
    	"k magnitude model, as in m = g + 5*log(delta) + 2.5*k*log(r)"},
    {H_SIZE, "Size:", "Angular Size @ 1 AU (arc secs):","Angular size at 1 AU"},
};

static Field para_fields[] = {
    {O_NAME, "Name:", "Object name:", "Name of Object"},
    {P_EP, "Ep of Peri:", "dummy", "Date of perihelion", {
	"Epoch of perihelion\n(UT Date as m/d.d/y or year.d):",
	"Epoch of perihelion\n(UT Date as y/m/d.d or year.d):",
	"Epoch of perihelion\n(UT Date as d.d/m/y or year.d):"
	}
    },
    {P_INC, "Inclination:", "Inclination (degs):", "Oribital inclination"},
    {P_AOP, "Arg of Peri:", "Argument of perihelion (degs):",
    	"Argument of perihelion"},
    {P_QP, "Peri Dist:", "Perihelion distance (AU):", "Distance at perihelion"},
    {P_LAN, "Long of Asc Nod:", "Longitude of ascending node (degs):",
    	"Longitude of ascending node"},
    {P_EPOCH, "Epoch:", "dummy", "Date of orbital parameters", {
	"Reference epoch\n(UT Date as m/d.d/y or year.d):",
	"Reference epoch\n(UT Date as y/m/d.d or year.d):",
	"Reference epoch\n(UT Date as d.d/m/y or year.d):"
	}
    },
    {P_G, "g:", "g, as in m = g + 5*log(delta) + 2.5*k*log(r):",
    	"g magnitude model, as in m = g + 5*log(delta) + 2.5*k*log(r)"},
    {P_K, "k:", "k, as in m = g + 5*log(delta) + 2.5*k*log(r):",
    	"k magnitude model, as in m = g + 5*log(delta) + 2.5*k*log(r)"},
    {P_SIZE, "Size:", "Angular Size @ 1 AU (arc secs):","Angular size at 1 AU"},
};

static Field esat_fields[] = {
    {O_NAME, "Name:", "Object name:", "Name of Object"},
    {ES_EPOCH, "Ep of Perigee:", "dummy", "Epoch of perigee", {
	"Epoch of perigee\n(UT Date as m/d.d/y or year.d):",
	"Epoch of perigee\n(UT Date as y/m/d.d or year.d):",
	"Epoch of perigee\n(UT Date as d.d/m/y or year.d):"
	}
    },
    {ES_INC, "Inclination:", "Inclination (degs):", "Orbital inclination"},
    {ES_RAAN, "RA of Node:", "RA of Ascending Node (degs):",
    	"Right Ascension of Ascending Node"},
    {ES_E, "Eccentricty:", "Eccentricty (<1):", "Orbital eccentricity"},
    {ES_AP, "Arg of Perigee:", "Argument of Perigee (degs):",
    	"Argument of perigee"},
    {ES_M, "Mean Anomaly:", "Mean Anomaly (degs):", "Mean anomaly"},
    {ES_N, "Daily motion:", "Daily motion (revs/day):", "Revolutions per day"},
    {ES_DECAY, "Orbit decay:", "Orbital decay rate (revs/day^2):",
    	"Revolutions per day^2"},
    {ES_ORBIT, "Orbit reference:", "Orbit number at epoch (integer):",
    	"Orbit number at epoch"},
    {ES_DRAG, "Drag coefficient:", "Drag coefficient, (earth radii)^-1:",
    	"Drag coefficient"},
};

typedef struct {
    char *name;		/* name of type toggle button */
    Field *fp;		/* ptr to Fields array defining the info, if any */
    int nfields;	/* number of Fields in fp[] */
    char *tip;		/* widget tip text */
    Widget tb_w;	/* type toggle button widget */
    Widget frc_w;	/* Field rc widget */
} TypeInfo;

/* names of each type.
 * N.B. must match order of enum in circum.h.
 */
static TypeInfo typeinfo[NOBJTYPES-1 /*No PLANET*/] = {
    {"Undefined",  NULL, 	  0,
    	"Current object is, or force it to be, Undefined"},
    {"Fixed",	   fixed_fields,  XtNumber(fixed_fields),
    	"Current object is, or force it to be, of type Fixed"},
    {"Elliptical", ellip_fields,  XtNumber(ellip_fields),
    	"Current object is, or force it to be, of type Sol Sys Elliptical"},
    {"Hyperbolic", hyper_fields,  XtNumber(hyper_fields),
    	"Current object is, or force it to be, of type Sol Sys Hyperbolic"},
    {"Parabolic",  para_fields,   XtNumber(para_fields),
    	"Current object is, or force it to be, of type Sol Sys Parabolic"},
    {"Earth Sat",  esat_fields,   XtNumber(esat_fields),
    	"Current object is, or force it to be, of type Earth Satellite"},
};

/* working storage - set when Reset, made permanent in database with Ok/Apply
 */
static Obj objx;			/* local working copy */
static Obj objy;			/* local working copy */
static Obj objz;			/* local working copy */
static int objidx = OBJX;		/* which one is on-screen now */
#define	objp	(objidx == OBJX ? &objx : objidx == OBJY ? &objy : &objz)

/* used to maintain mapping between object selection buttons and database
 * objects. the userData of each object selection button is set to the index of
 * a namelist[].
 */
typedef struct {
	Obj *op;
} NameList;

#define	NBTNS	20		/* number of buttons in name list */
static NameList *namelist;	/* malloced array of db_n() NameLists*/
static int nnamelist;		/* number of entries in namelist actually used*/
static Widget namepb_w[NBTNS];	/* each name selection button */

#define	SCROLLBAR_WIDTH		20

/* bottom control panel buttons */
enum {
    OK, APPLY, RESET, POINT, IDENT, SETTEL, CANCEL, HELP
};

static int newdb;		/* set when db is updated while we are down */
static char objcategory[] = "Date base -- Search";	/* Save category */

/* called by the main menu pick.
 * create the main form, if this is the first time we've been called.
 * then we go for it.
 */
void
obj_manage()
{
	if (!objshell_w) {
	    obj_create_shell();
	    init_working_obj();
	    newdb = 1;	/* force a fresh list */
	}
	
	if (newdb) {
	    new_namelist();
	    newdb = 0;
	}
	obj_setxyz();
	obj_setmenu();
	XtPopup (objshell_w, XtGrabNone);
	set_something (objshell_w, XmNiconic, (XtArgVal)False);
}

/* update our view if we are up */
/* ARGSUSED */
void
obj_update (np, howmuch)
Now *np;
int howmuch;
{
	if (!isUp(objshell_w))
	    return;

	obj_setmenu();
}

/* make the given arbitrary db object the current value of ObjXYZ.
 * update our working copy, the real db object, all the stuff in the menu (if
 * we've been managed!) and inform all others.
 */
void
obj_set (op, dbidx)
Obj *op;
int dbidx;
{
	static char me[] = "obj_set()";

	/* TODO: we don't support planets */
	if (op->o_type == PLANET) {
	    printf ("%s: planets not supported\n", me);
	    exit (1);
	}

	if (dbidx != OBJX && dbidx != OBJY && dbidx != OBJZ) {
	    printf ("%s: not one of XYZ: %d\n", me, dbidx);
	    exit(1);
	}

	obj_unsetmenu();
	objidx = dbidx;		/* effectively changes current objp */
	obj_set_from_db(op);	/* *objp <- *op */
	obj_setxyz();		/* set the correct objxyz_w TB to match */
	obj_setmenu();		/* load menu items */
	obj_set_db();		/* db_setuserobj(objp) */
	all_newobj (objidx);	/* tell all */
}

/* called when the db has changed (beyond NOBJ that is).
 * we don't care whether it was appended or deleted because we need to resort
 * and rebuild the whole list either way.
 */
/* ARGSUSED */
void
obj_newdb(appended)
int appended;
{
	if (!isUp(objshell_w)) {
	    newdb = 1;
	    return;
	}

	new_namelist();
}

/* called to put up or remove the watch cursor.  */
void
obj_cursor (c)
Cursor c;
{
	Window win;

	if (objshell_w && (win = XtWindow(objshell_w)) != 0) {
	    Display *dsp = XtDisplay(objshell_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* like strcmp() but take care to sort numerics by value and ignore case.
 */
int
strnncmp (s1, s2)
char *s1, *s2;
{
	int p;
	int q;

	for (;;) {
	    p = *s1++;
	    if (islower(p))
		p = toupper (p);
	    q = *s2++;
	    if (islower(q))
		q = toupper (q);
	    if (isdigit(p) && isdigit(q)) {
		int np = 0, nq = 0;
		do {
		    np = np*10 + p - '0';
		    p = *s1;
		} while (isdigit(p) && s1++);
		do {
		    nq = nq*10 + q - '0';
		    q = *s2;
		} while (isdigit(q) && s2++);
		if (np != nq)
		    return (np - nq);
	    }
	    if (!p || !q || p != q)
		return (p - q);
	}
}

/* this was added to support backward compatability with the ability to
 * set OBJXYZ from the ephem.cfg file.
 */
static void
init_working_obj()
{
	Obj *op;

	op = db_basic(OBJX);
	objx = *op;
	op = db_basic(OBJY);
	objy = *op;
	op = db_basic(OBJZ);
	objz = *op;
}

/* turn off menu info about the current object.
 */
static void
obj_unsetmenu()
{
	if (!objshell_w)
	    return;

	XtUnmanageChild (typeinfo[objp->o_type].frc_w);
	XmToggleButtonSetState (typeinfo[objp->o_type].tb_w, False, False);
}

/* set the xyz toggle buttons to match objidx */
static void
obj_setxyz()
{
	if (!objshell_w)
	    return;

	XmToggleButtonSetState (objx_w, False, False);
	XmToggleButtonSetState (objy_w, False, False);
	XmToggleButtonSetState (objz_w, False, False);

	switch (objidx) {
	case OBJX: XmToggleButtonSetState (objx_w, True, False); break;
	case OBJY: XmToggleButtonSetState (objy_w, True, False); break;
	case OBJZ: XmToggleButtonSetState (objz_w, True, False); break;
	default: printf ("Objsetxyz: impossible objidx: %d\n", objidx); exit(1);
	}
}

/* copy the arbitrary database object, op, into the working copy of the
 * current object.
 * TODO: N.B. don't allow planets -- YET!
 */
static void
obj_set_from_db (op)
Obj *op;
{
	static char me[] = "obj_set_from_db()";

	if (op->o_type == PLANET) {
	    printf ("%s: PLANET not supported\n", me);
	    exit (1);
	}

	/* *objp = *op; */
	(void) memcpy ((void *)objp, (void *)op, sizeof(Obj));
}

/* make the current working object officially in the database.
 */
static void
obj_set_db ()
{
	db_setuserobj(objidx, objp);
}

/* set up the menu according to the current working object
 */
static void
obj_setmenu()
{
	TypeInfo *tp;

	if (!objshell_w)
	    return;

	tp = &typeinfo[objp->o_type];
	XmToggleButtonSetState (tp->tb_w, True, False);
	obj_setobjinfo(tp->fp, tp->nfields);
	XtManageChild (tp->frc_w);
}

/* set each field in the rc for the current object.
 */
static void
obj_setobjinfo(fp, nf)
Field *fp;
int nf;
{
	while (--nf >= 0) {
	    obj_set_button (fp->pb_w, fp->id);
	    fp++;
	}
}

/* called once to build the basic shell and form.
 * N.B. we could not manage to connect the right side of NameF to the left
 * side of ScrollB without complaints from the Obj form. So, to make it look
 * attached, both NameF and ScrollB are attach to the right side of the form
 * but the NameF has an offset equal to the width of ScrollB. hey -- it works!
 */
static void
obj_create_shell ()
{
	struct Btns {	/* to streamline creation of control buttons */
	    int id;
	    char *name;
	    char *tip;
	};
	static struct Btns ctlbtns[] = {
	    {OK,    "Ok",    "Define the current Object and close this dialog"},
	    {APPLY, "Apply", "Define the current Object and leave dialog up"},
	    {RESET, "Reset", "Restore current Object"},
	    {CANCEL,"Close", "Close this dialog without changing anything"},
	    {HELP,  "Help",  "Display detailed help information"}
	};
	static struct Btns skybtns[] = {
	    {POINT, "Sky Point",
	    	"Center Sky View on this Object and mark"},
	    {IDENT, "Sky Mark",
		"Mark this Object on Sky View, if already in view"},
	    {SETTEL, "Set Tel",
		"Send coordinates of this Object to the telescope, if up"},
	};
	Widget typrbf_w, typrb_w;
	Widget fldrcf_w;
	Widget xyzrbf_w, xyzrb_w;
	Widget namerc_w, namercf_w;
	Widget objform_w;
	Widget ctlfr_w, ctlf_w;
	Widget skyfr_w, skyf_w;
	Widget srchf_w, srchrc_w;
	Widget w;
	Arg args[20];
	int n;
	int i;

	/* create shell */
	n = 0;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle, "xephem Search and ObjXYZ"); n++;
	XtSetArg (args[n], XmNiconName, "Objects"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	objshell_w = XtCreatePopupShell ("Objects", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (objshell_w, XmNcolormap, (XtArgVal)xe_cm);
	sr_reg (objshell_w, "XEphem*Objects.x", objcategory, 0);
	sr_reg (objshell_w, "XEphem*Objects.y", objcategory, 0);

	/* create form */
	n = 0;
	XtSetArg (args[n], XmNallowOverlap, False); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	objform_w = XmCreateForm (objshell_w, "ObjF", args, n);
	set_something (objform_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (objform_w, XmNhelpCallback, obj_ctl_cb, (XtPointer)HELP);
	XtManageChild (objform_w);

	/* make the type control radio box in a frame.
	 * we save the widget ids so we can force the type to be what
	 * was selected from the database.
	 */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	typrbf_w = XmCreateFrame (objform_w, "TypeRBFrame", args, n);
	XtManageChild (typrbf_w);

	n = 0;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg (args[n], XmNnumColumns, 2); n++;
	typrb_w = XmCreateRadioBox (typrbf_w, "TypeRB", args, n);
	XtManageChild (typrb_w);

	    /* N.B. we assume the enum ObjType form indeces into typeinfo[] */
	    for (i = UNDEFOBJ; i < PLANET; i++) {
		TypeInfo *tp = &typeinfo[i];
		n = 0;
		w = tp->tb_w = XmCreateToggleButton (typrb_w, tp->name, args,n);
		XtAddCallback (w, XmNvalueChangedCallback, obj_type_cb,
								(XtPointer)i);
		if (tp->tip)
		    wtip (w, tp->tip);
		XtManageChild (w);
	    }

	/* make the xyz selection radio box in a frame */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, typrbf_w); n++;
	XtSetArg (args[n], XmNorientation, XmVERTICAL); n++;
	xyzrbf_w = XmCreateFrame (objform_w, "XYRBF", args, n);
	XtManageChild (xyzrbf_w);

	n = 0;
	xyzrb_w = XmCreateRadioBox (xyzrbf_w, "XYRBox", args, n);
	XtManageChild (xyzrb_w);

	    n = 0;
	    XtSetArg (args[n], XmNset, True); n++; /* N.B. must = objidx init */
	    objx_w = XmCreateToggleButton (xyzrb_w, "ObjX", args, n);
	    XtAddCallback (objx_w, XmNvalueChangedCallback, obj_xyz_cb,
							    (XtPointer)OBJX);
	    wtip (objx_w, "Display, or define, user-defined Object X");
	    XtManageChild (objx_w);

	    n = 0;
	    objy_w = XmCreateToggleButton (xyzrb_w, "ObjY", args, n);
	    XtAddCallback (objy_w, XmNvalueChangedCallback, obj_xyz_cb,
							    (XtPointer)OBJY);
	    wtip (objy_w, "Display, or define, user-defined Object Y");
	    XtManageChild (objy_w);

	    n = 0;
	    objz_w = XmCreateToggleButton (xyzrb_w, "ObjZ", args, n);
	    XtAddCallback (objz_w, XmNvalueChangedCallback, obj_xyz_cb,
							    (XtPointer)OBJZ);
	    wtip (objz_w, "Display, or define, user-defined Object Z");
	    XtManageChild (objz_w);

	/* make a rc in a frame to hold the name buttons and srch field */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, xyzrbf_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightOffset, SCROLLBAR_WIDTH); n++;
	namercf_w = XmCreateFrame (objform_w, "NameF", args, n);
	XtManageChild (namercf_w);

	n = 0;
	namerc_w = XmCreateRowColumn (namercf_w, "NameRC", args, n);
	XtManageChild (namerc_w);

	    for (i = 0; i < NBTNS; i++) {
		n = 0;
		w = XmCreatePushButton (namerc_w, "NamePB", args, n);
		XtAddCallback (w, XmNactivateCallback, obj_select_cb,
								(XtPointer)i);
		XtManageChild (w);
		wtip (w, "Select to view this Object's definition");
		namepb_w[i] = w;
	    }

	/* make the search pb and text field in a frame */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, namercf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, xyzrbf_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	srchf_w = XmCreateFrame (objform_w, "SrchF", args, n);
	XtManageChild (srchf_w);


	    n = 0;
	    XtSetArg (args[n], XmNisAligned, False); n++;
	    srchrc_w = XmCreateRowColumn (srchf_w, "SrchRS", args, n);
	    XtManageChild (srchrc_w);

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	    w = XmCreatePushButton (srchrc_w, "SrchL", args, n);
	    set_xmstring (w, XmNlabelString, "Search:");
	    XtAddCallback (w, XmNactivateCallback, obj_srch_cb, NULL);
	    wtip (w,"Scan for next Object in memory with name described below");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNmaxLength, MAXNM-1); n++;
	    srch_w = XmCreateTextField (srchrc_w, "SrchTF", args, n);
	    XtManageChild (srch_w);
	    wtip (srch_w, "Enter all or partial name, hit RETURN to search");
	    XtAddCallback (srch_w, XmNactivateCallback, obj_srch_cb, NULL);

#if XmVersion >= 1001
	    /* init kb focus here if possible */
	    XmProcessTraversal (w, XmTRAVERSE_CURRENT);
	    XmProcessTraversal (w, XmTRAVERSE_CURRENT);
#endif

	/* make the scrollbar attached to the right edge */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, namercf_w); n++;
	XtSetArg (args[n], XmNpageIncrement, NBTNS-1); n++;
	XtSetArg (args[n], XmNwidth, SCROLLBAR_WIDTH); n++;
	scroll_w = XmCreateScrollBar (objform_w, "ScrollB", args, n);
	XtAddCallback (scroll_w, XmNdragCallback, obj_scroll_cb, 0);
	XtAddCallback (scroll_w, XmNvalueChangedCallback, obj_scroll_cb, 0);
	XtManageChild (scroll_w);

	/* make a form in a frame to hold the bottom control buttons */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, srchf_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	ctlfr_w = XmCreateFrame (objform_w, "CtlFrame", args, n);
	XtManageChild (ctlfr_w);

	n = 0;
	XtSetArg (args[n], XmNfractionBase, 21); n++;
	ctlf_w = XmCreateForm (ctlfr_w, "CtlForm", args, n);
	XtManageChild (ctlf_w);

	    /* make the control buttons */

	    for (i = 0; i < XtNumber(ctlbtns); i++) {
		n = 0;
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNtopOffset, 10); n++;
		XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNbottomOffset, 10); n++;
		XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
		XtSetArg (args[n], XmNleftPosition, 1 + i*4); n++;
		XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
		XtSetArg (args[n], XmNrightPosition, 4 + i*4); n++;
		w = XmCreatePushButton (ctlf_w, ctlbtns[i].name, args, n);
		XtAddCallback (w, XmNactivateCallback, obj_ctl_cb,
						    (XtPointer)ctlbtns[i].id);
		if (ctlbtns[i].tip)
		    wtip (w, ctlbtns[i].tip);
		XtManageChild (w);
	    }

	/* make the overall field id rc in a frame.
	 * the field id rc is just used as a manager to hold the several
	 *   various type rcs; only one is ever on at once.
	 */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, typrbf_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, namercf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, namercf_w); n++;
	fldrcf_w = XmCreateFrame (objform_w, "FldFrame", args, n);
	XtManageChild (fldrcf_w);

	n = 0;
	fldrc_w = XmCreateRowColumn (fldrcf_w, "FldInfoRC", args, n);
	XtManageChild (fldrc_w);

	    /* make one row/col for each possible Obj type.
	     * they are managed later according to which type is displayed.
	     * N.B. we assume the enum ObjType form indeces into typeinfo[]
	     */
	    for (i = UNDEFOBJ; i < PLANET; i++) {
		TypeInfo *tp = &typeinfo[i];
		Field *fp;

		n = 0;
		tp->frc_w = XmCreateRowColumn (fldrc_w, "TypeRC", args, n);

		for (fp = tp->fp; fp < tp->fp + tp->nfields; fp++) {
		    Widget fw;
		    n = 0;
		    fw = XmCreateForm (tp->frc_w, "ObjFldForm", args, n);
		    obj_create_type_rc_form (fw, fp);
		    XtManageChild (fw);
		}
	    }

	/* make a form in a frame for the sky point/id buttons */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, srchf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, xyzrbf_w); n++;
	skyfr_w = XmCreateFrame (objform_w, "SkyFr", args, n);
	XtManageChild (skyfr_w);

	n = 0;
	XtSetArg (args[n], XmNfractionBase, 1+10*XtNumber(skybtns)); n++;
	skyf_w = XmCreateForm (skyfr_w, "SkyF", args, n);
	XtManageChild (skyf_w);

	    for (i = 0; i < XtNumber(skybtns); i++) {
		n = 0;
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNtopOffset, 10); n++;
		XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNbottomOffset, 10); n++;
		XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
		XtSetArg (args[n], XmNleftPosition, 1 + i*10); n++;
		XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
		XtSetArg (args[n], XmNrightPosition, 10 + i*10); n++;
		w = XmCreatePushButton (skyf_w, skybtns[i].name, args, n);
		XtAddCallback (w, XmNactivateCallback, obj_ctl_cb,
						    (XtPointer)skybtns[i].id);
		if (skybtns[i].tip)
		    wtip (w, skybtns[i].tip);
		XtManageChild (w);
	    }
}

/* given a Form widget and a Field, fill in the widgets in the Form.
 */
static void
obj_create_type_rc_form (fw, fp)
Widget fw;
Field *fp;
{
	XmString str;
	Widget w;
	Arg args[20];
	int n;

	/* create a pushbutton if this field can be changed, else a label */

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	if (fp->prompt) {
	    w = XmCreatePushButton (fw, "ObjFldPB", args, n);
	    XtAddCallback (w, XmNactivateCallback, obj_change_cb,
								(XtPointer)fp);
	} else
	    w = XmCreateLabel (fw, "ObjFldInfo", args, n);
	fp->pb_w = w;
	if (fp->tip)
	    wtip (w, fp->tip);
	XtManageChild (w);

	/* make the label to the left of w */

	str = XmStringCreateLtoR (fp->label, XmSTRING_DEFAULT_CHARSET);
	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, w); n++;
	XtSetArg (args[n], XmNlabelString, str); n++;
	w = XmCreateLabel (fw, "ObjFldLabel", args, n);
	XtManageChild (w);
	XmStringFree(str);
}

/* callback from any of the obj field info buttons being activated.
 * client is the Field pointer for this item.
 * N.B. some fields' prompts are set from preferences.
 */
/* ARGSUSED */
static void
obj_change_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Field *fp = (Field *)client;

	switch (fp->id) {
	case F_EPOCH: 
	case E_CEPOCH:
	case E_EPOCH:
	case H_EP:
	case H_EPOCH:
	case P_EP:
	case P_EPOCH:
	case ES_EPOCH:
	    fp->prompt = fp->altp[pref_get(PREF_DATE_FORMAT)];
	    break;
	}

	prompt (fp);
}

/* callback from any of the botton control buttons.
 * id is in client.
 */
/* ARGSUSED */
static void
obj_ctl_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int id = (int) client;

	switch (id) {
	case OK:
	    obj_set_db();
	    all_newobj(objidx);
	    XtPopdown (objshell_w);
	    break;
	case APPLY:
	    obj_set_db();
	    all_newobj(objidx);
	    break;
	case RESET:
	    obj_unsetmenu();
	    obj_set_from_db(db_basic(objidx));
	    obj_setmenu();
	    break;
	case POINT:
	    if (objp->o_type != UNDEFOBJ) {
		if (obj_cir (mm_get_now(), objp) < 0) {
		    printf ("obj_ctl_cb: can't get obj_cir for POINT\n");
		    exit (1);
		}
		sv_point (objp);
	    } else
		xe_msg ("No current object", 1);
	    break;
	case IDENT:
	    if (objp->o_type != UNDEFOBJ) {
		if (obj_cir (mm_get_now(), objp) < 0) {
		    printf ("obj_ctl_cb: can't get obj_cir for IDENT\n");
		    exit (1);
		}
		sv_id (objp);
	    } else
		xe_msg ("No current object", 1);
	    break;
	case SETTEL:
	    if (objp->o_type != UNDEFOBJ) {
		if (obj_cir (mm_get_now(), objp) < 0) {
		    printf ("obj_ctl_cb: can't get obj_cir for IDENT\n");
		    exit (1);
		}
		(void) skyloc_fifo (objp);
	    } else
		xe_msg ("No current object", 1);
	    break;
	case CANCEL:
	    XtPopdown (objshell_w);
	    break;
	case HELP:
	    do_help();
	    break;
	}
}

static void
do_help()
{
	static char *msg[] = {
"Search for an object by name;",
"edit it;",
"or assign it to a User-defined object"
};

	switch (objp->o_type) {
	case UNDEFOBJ:
	    hlp_dialog ("Object", msg, sizeof(msg)/sizeof(msg[0]));
	    break;
	case FIXED:
	    hlp_dialog ("Fixed Object", msg, sizeof(msg)/sizeof(msg[0]));
	    break;
	case ELLIPTICAL:
	    hlp_dialog ("Elliptical Object", msg, sizeof(msg)/sizeof(msg[0]));
	    break;
	case HYPERBOLIC:
	    hlp_dialog ("Hyperbolic Object", msg, sizeof(msg)/sizeof(msg[0]));
	    break;
	case PARABOLIC:
	    hlp_dialog ("Parabolic Object", msg, sizeof(msg)/sizeof(msg[0]));
	    break;
	case EARTHSAT:
	    hlp_dialog ("Earth satellite", msg, sizeof(msg)/sizeof(msg[0]));
	    break;
	default:
	    printf ("objmenu: no help for type %d\n", objp->o_type);
	    exit (1);
	}
}

/* callback for which object (xyz) toggle.
 * client is one of OBJXYZ.
 */
/* ARGSUSED */
static void
obj_xyz_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *t = (XmToggleButtonCallbackStruct *) call;
	int xyz = (int)client;

	if (t->set && xyz != objidx) {
	    obj_unsetmenu();
	    objidx = xyz;
	    obj_setmenu();
	}
}

/* callback for what type of object toggles.
 * client data is the Obj.o_type code.
 */
/* ARGSUSED */
static void
obj_type_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *t = (XmToggleButtonCallbackStruct *) call;
	int type = (int)client;

	if (t->set && objp->o_type != type) {
	    obj_unsetmenu();
	    zero_mem ((void *)objp, sizeof(Obj));
	    objp->o_type = (ObjType_t) type;
	    obj_setmenu();
	}
}

/* callback when an item is selected from the list.
 * client is the button number, userData is the namelist index.
 */
/* ARGSUSED */
static void
obj_select_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char me[] = "obj_select_cb()";
	int nameidx;
	Obj *op;

	get_something (w, XmNuserData, (XtArgVal)&nameidx);
	if (nameidx < 0)
	    return;	/* marked as unused */
	if (nameidx >= nnamelist) {
	    printf ("%s: nameidx=%d but nnamelist=%d\n", me, nameidx,
								nnamelist);
	    exit (1);
	}

	op = namelist[nameidx].op;
	obj_unsetmenu();
	obj_set_from_db(op);
	obj_setmenu();
}

/* callback when the scroll bar is moved.
 * fill name buttons with next NB names starting with namelist[sp->value].
 */
/* ARGSUSED */
static void
obj_scroll_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmScrollBarCallbackStruct *sp = (XmScrollBarCallbackStruct *)call;
	obj_setnames (sp->value);
}

/* callback when CR is hit in the Srch text field or when the Search: PB is
 * activated.
 * N.B. Since we are used by two different kinds of widgets don't use w or call.
 */
/* ARGSUSED */
static void
obj_srch_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char last_str[MAXNM];	/* last string we searched for */
	static int last_idx = -1;	/* index to start */
	char *str;
	int i;
	
	str = XmTextFieldGetString (srch_w);

	/* start from top if new string or smaller list */
	if (strcmp (str, last_str) || last_idx >= nnamelist) {
	    last_idx = -1;
	    (void) strcpy (last_str, str);
	}
	i = last_idx;

	/* check list starting at i+1, wrapping back to last_idx */
	for (;;) {

	    i++;
	    if (last_idx < 0 && i >= nnamelist) {
		char msg[1024];
		(void) sprintf (msg, "%s: not found", str);
		xe_msg (msg, 1);
		break;
	    }
	    if (i >= nnamelist)
		i = 0;

	    if (srch_match (str, namelist[i].op->o_name)) {
		int idx, max, slider;

		/* find index to use for setting names and the scroll bar */
		get_something (scroll_w, XmNmaximum, (XtArgVal)&max);
		get_something (scroll_w, XmNsliderSize, (XtArgVal)&slider);
		idx = i < max-slider ? i : max-slider;

		/* set buttons so idxth entry appears on top */
		obj_setnames (idx);

		/* set slider to match */
		set_something (scroll_w, XmNvalue, (XtArgVal)idx);

		/* set up type menu with info about the selected item */
		obj_unsetmenu();
		obj_set_from_db(namelist[i].op);
		obj_setmenu();

		last_idx = i;
		(void) strcpy (last_str, str);
		break;
	    }

	    if (i == last_idx) {
		last_idx = -1;
		break;
	    }
	}

	XtFree (str);
}

/* return 1 if n1 is anywhere in n2, ignoring case and whitespace;
 * else return 0.
 * N.B. we only use the first MAXNM-1 chars from each string and assume \0.
 */
static int
srch_match (n1, n2)
char *n1, *n2;
{
	char n1c[MAXNM];	/* n1 all upper case, no whitespace */
	char n2c[MAXNM];	/* n2 all upper case, no whitespace */
	int n1l, n2l;		/* lengths of n1c and n2c */
	char c, *cp;
	int i;

	for (n1l = 0, cp = n1; (c = *cp++) != '\0'; ) {
	    if (islower(c))
		c = toupper (c);
	    if (!isspace(c))
		n1c[n1l++] = c;
	}
	n1c[n1l] = '\0';

	for (n2l = 0, cp = n2; (c = *cp++) != '\0'; ) {
	    if (islower(c))
		c = toupper (c);
	    if (!isspace(c))
		n2c[n2l++] = c;
	}
	n2c[n2l] = '\0';

	/* if n1 started with ^ it must match the start of n2 */
	if (n1c[0] == '^')
	    return (strncmp (n1c+1, n2c, n1l-1) == 0 ? 1 : 0);

	for (i = 0; i <= n2l-n1l; i++)
	    if (strncmp (n1c, n2c+i, n1l) == 0)
		return (1);

	return (0);
}

/* compare two NameList entries as required qsort(3).
 * take care to sort numerics by value.
 * ignore case.
 */
static int
nl_cmp (nl1, nl2)
qsort_arg nl1, nl2;
{
	char *s1 = ((NameList *)nl1)->op->o_name;
	char *s2 = ((NameList *)nl2)->op->o_name;
	return (strnncmp (s1, s2));
}

/* set up namelist for a new list of db objects.
 */
static void
new_namelist()
{
	Arg args[20];
	int n;

	watch_cursor(1);

	load_namelist();

	if (nnamelist > 0) {
	    int slider = nnamelist < NBTNS ? nnamelist : NBTNS;
	    n = 0;
	    XtSetArg (args[n], XmNmaximum, nnamelist); n++;
	    XtSetArg (args[n], XmNsliderSize, slider); n++;
	    XtSetArg (args[n], XmNvalue, 0); n++;
	    XtSetValues (scroll_w, args, n);
	    XtManageChild(scroll_w);
	} else
	    XtUnmanageChild(scroll_w);

	obj_setnames(0);

	watch_cursor(0);
}

/* make the master list of db name objects in sorted order.
 */
static void
load_namelist ()
{
	DBScan dbs;
	NameList *nlp;
	int mask = ALLM & ~PLANETM;
	Obj *op;
	int nobj;

	if (namelist) {
	    XtFree ((char *)namelist);
	    namelist = NULL;
	    nnamelist = 0;
	}

	nobj = db_n();
	if (nobj <= 0)
	    return;

	nlp = namelist = (NameList *) XtMalloc (nobj*sizeof(NameList));

	/* put everything except the basic objects on the list */
	for (db_scaninit(&dbs, mask, NULL, 0); (op = db_scan (&dbs)) != 0; ) {
	    nlp->op = op;
	    nlp++;
	}

	nnamelist = nlp - namelist;

	qsort ((void *)namelist, nnamelist, sizeof(NameList), nl_cmp);
}

/* fill the name buttons with the next NBTNS names starting at index nli 
 * within namelist[];
 */
static void
obj_setnames(nli)
int nli;
{
	static char noname[MAXNM];
	XmString str;
	Arg args[20];
	char *namep;
	int bi;		/* namepb_w[] index */
	int nb;		/* number of good buttons to set */

	/* noname is string of MAXNM-1 blanks for use when btn not used */
	if (noname[0] == '\0')
	    (void) sprintf (noname, "%*s", MAXNM-1, "");

	nb = (nnamelist - nli < NBTNS) ? nnamelist - nli : NBTNS;
	for (bi = 0; bi < NBTNS; bi++) {
	    int n = 0;
	    if (bi < nb) {
		namep = namelist[nli].op->o_name;
		XtSetArg (args[n], XmNuserData, nli); n++;
		buttonAsButton(namepb_w[bi], 1);
		nli++;
	    } else {
		namep = noname;
		XtSetArg (args[n], XmNuserData, -1); n++;
		buttonAsButton(namepb_w[bi], 0);
	    }
	    str = XmStringCreateLtoR (namep, XmSTRING_DEFAULT_CHARSET);
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    XtSetValues (namepb_w[bi], args, n);
	    XmStringFree (str);
	}
}

/* user typed OK to a prompt for fp. get his new value and use it.
 * get fp from userData.
 */
/* ARGSUSED */
static void
prompt_ok_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmSelectionBoxCallbackStruct *s = (XmSelectionBoxCallbackStruct *)call;
	Field *fp;
	char *text;
	
	switch (s->reason) {
	case XmCR_OK:
	    get_xmstring(w, XmNtextString, &text);
	    get_something (w, XmNuserData, (XtArgVal)&fp);
	    (void) db_set_field (text, fp->id,
			    (PrefDateFormat) pref_get(PREF_DATE_FORMAT), objp);
	    obj_set_button (fp->pb_w, fp->id);
	    XtFree (text);
	    break;
	}

	XtUnmanageChild (w);
}

/* put up a prompt dialog near the cursor to ask about fp.
 * make it app modal since we don't want the object type to change under us.
 */
static void
prompt (fp)
Field *fp;
{
	static Widget dw;
	Widget w;
	
	if (!dw) {
	    XmString title;
	    Arg args[20];
	    int n;

	    title = XmStringCreate("xephem Object Prompt",
						    XmSTRING_DEFAULT_CHARSET);
	    n = 0;
	    XtSetArg(args[n], XmNdialogStyle, XmDIALOG_APPLICATION_MODAL);  n++;
	    XtSetArg(args[n], XmNdialogTitle, title);  n++;
	    XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	    XtSetArg(args[n], XmNmarginWidth, 10);  n++;
	    XtSetArg(args[n], XmNmarginHeight, 10);  n++;
	    dw = XmCreatePromptDialog(toplevel_w, "ObjPrompt", args, n);
	    set_something (dw, XmNcolormap, (XtArgVal)xe_cm);
	    XtAddCallback (dw, XmNmapCallback, prompt_map_cb, NULL);
	    XtAddCallback (dw, XmNokCallback, prompt_ok_cb, NULL);
	    XtAddCallback (dw, XmNcancelCallback, prompt_ok_cb, NULL);
	    XmStringFree (title);

	    w = XmSelectionBoxGetChild (dw, XmDIALOG_HELP_BUTTON);
	    XtUnmanageChild (w);
	}

	set_something (dw, XmNuserData, (XtArgVal)fp);
	set_xmstring (dw, XmNselectionLabelString, fp->prompt);

	/* preload with current string -- and skip leading blanks */
	if (pref_get (PREF_PRE_FILL) == PREF_PREFILL) {
	    char *txt, *txt0;

	    get_xmstring (fp->pb_w, XmNlabelString, &txt0);
	    for (txt = txt0; *txt == ' '; txt++)
		continue;
	    set_xmstring(dw, XmNtextString, txt);
	    XtFree (txt0);
	} else
	    set_xmstring(dw, XmNtextString, "");

	XtManageChild (dw);

#if XmVersion >= 1001
	w = XmSelectionBoxGetChild (dw, XmDIALOG_TEXT);
	XmProcessTraversal (w, XmTRAVERSE_CURRENT);
	XmProcessTraversal (w, XmTRAVERSE_CURRENT); /* yes, twice!! */
#endif
}

/* set the label for the given label widget according to the info in the
 * given field of the current working object.
 */
static void
obj_set_button (w, id)
Widget w;
int id;
{
	static char me[] = "obj_set_button";

	switch (id) {
	case O_NAME:
	    f_string (w, objp->o_name);
	    break;
	case F_CLASS:
	    f_string (w, obj_description(objp));
	    break;
	case F_RA:
	    f_ra (w, objp->f_RA);
	    break;
	case F_DEC:
	    f_prdec (w, objp->f_dec);
	    break;
	case F_MAG:
	    f_double (w, "%g", get_mag(objp));
	    break;
	case F_EPOCH:
	    epoch_as_decimal (w, objp->f_epoch);
	    break;
	case F_SIZE:
	    f_double (w, "%g", objp->f_size);
	    break;

	case E_INC:
	    f_double (w, "%g", objp->e_inc);
	    break;
	case E_LAN:
	    f_double (w, "%g", objp->e_Om);
	    break;
	case E_AOP:
	    f_double (w, "%g", objp->e_om);
	    break;
	case E_A:
	    f_double (w, "%g", objp->e_a);
	    break;
	case E_N:
	    /* retired */
	    break;
	case E_E:
	    f_double (w, "%g", objp->e_e);
	    break;
	case E_M:
	    f_double (w, "%g", objp->e_M);
	    break;
	case E_CEPOCH:
	    epoch_as_mdy (w, objp->e_cepoch);
	    break;
	case E_EPOCH:
	    epoch_as_decimal (w, objp->e_epoch);
	    break;
	case E_M1: {
	    char buf[64];
	    (void) sprintf (buf, "%c%g",
			    objp->e_mag.whichm == MAG_HG ? 'H' : 'g',
			    objp->e_mag.m1);
	    f_string (w, buf);
	    break;
	}
	case E_M2: {
	    char buf[64];
	    (void) sprintf (buf, "%c%g",
			    objp->e_mag.whichm == MAG_HG ? 'G' : 'k',
			    objp->e_mag.m2);
	    f_string (w, buf);
	    break;
	}
	case E_SIZE:
	    f_double (w, "%g", objp->e_size);
	    break;

	case H_EP:
	    epoch_as_mdy (w, objp->h_ep);
	    break;
	case H_INC:
	    f_double (w, "%g", objp->h_inc);
	    break;
	case H_LAN:
	    f_double (w, "%g", objp->h_Om);
	    break;
	case H_AOP:
	    f_double (w, "%g", objp->h_om);
	    break;
	case H_E:
	    f_double (w, "%g", objp->h_e);
	    break;
	case H_QP:
	    f_double (w, "%g", objp->h_qp);
	    break;
	case H_EPOCH:
	    epoch_as_decimal (w, objp->h_epoch);
	    break;
	case H_G:
	    f_double (w, "%g", objp->h_g);
	    break;
	case H_K:
	    f_double (w, "%g", objp->h_k);
	    break;
	case H_SIZE:
	    f_double (w, "%g", objp->h_size);
	    break;

	case P_EP:
	    epoch_as_mdy (w, objp->p_ep);
	    break;
	case P_INC:
	    f_double (w, "%g", objp->p_inc);
	    break;
	case P_AOP:
	    f_double (w, "%g", objp->p_om);
	    break;
	case P_QP:
	    f_double (w, "%g", objp->p_qp);
	    break;
	case P_LAN:
	    f_double (w, "%g", objp->p_Om);
	    break;
	case P_EPOCH:
	    epoch_as_decimal (w, objp->p_epoch);
	    break;
	case P_G:
	    f_double (w, "%g", objp->p_g);
	    break;
	case P_K:
	    f_double (w, "%g", objp->p_k);
	    break;
	case P_SIZE:
	    f_double (w, "%g", objp->p_size);
	    break;


	case ES_EPOCH:
	    epoch_as_decimal (w, objp->es_epoch);
	    break;
	case ES_INC:
	    f_double (w, "%g", objp->es_inc);
	    break;
	case ES_RAAN:
	    f_double (w, "%g", objp->es_raan);
	    break;
	case ES_E:
	    f_double (w, "%g", objp->es_e);
	    break;
	case ES_AP:
	    f_double (w, "%g", objp->es_ap);
	    break;
	case ES_M:
	    f_double (w, "%g", objp->es_M);
	    break;
	case ES_N:
	    f_double (w, "%g", objp->es_n);
	    break;
	case ES_DECAY:
	    f_double (w, "%g", objp->es_decay);
	    break;
	case ES_ORBIT:
	    f_double (w, "%g", (double)objp->es_orbit);
	    break;
	case ES_DRAG:
	    f_double (w, "%g", objp->es_drag);
	    break;

	default:
	    printf ("%s: bad id: %d\n", me, id);
	    exit (1);
	}
}

static void
epoch_as_decimal (w, e)
Widget w;
double e;
{
	double y;
	mjd_year (e, &y);
	f_double (w, "%g", y);
}

static void
epoch_as_mdy (w, e)
Widget w;
double e;
{
	char buf[100];

	fs_date (buf, e);
	f_string (w, buf);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: objmenu.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
