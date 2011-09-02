/* main() for telsched
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include <math.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <Xm/Xm.h>
#include <X11/Shell.h>
#include <X11/cursorfont.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/Form.h>
#include <Xm/Separator.h>
#include <Xm/FileSB.h>
#include <Xm/MainW.h>
#include <Xm/RowColumn.h>
#include <Xm/ScrollBar.h>
#include <Xm/ToggleB.h>
#include <Xm/BulletinB.h>
#include <Xm/MessageB.h>
#include <Xm/SelectioB.h>
#include <Xm/TextF.h>
#include <Xm/DrawingA.h>
#include <Xm/Frame.h>
#include <Xm/ScrolledW.h>

#include <X11/xpm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "strops.h"
#include "telenv.h"
#include "configfile.h"
#include "preferences.h"
#include "xtools.h"
#include "misc.h"
#include "scan.h"
#include "telsched.h" // KMI 11/11/02

#define SG_W 1400
#define SG_H 400

//extern void sg_create_form (void);
extern void sg_da_exp_cb (Widget w, XtPointer client, XtPointer call);
extern void sg_close_cb (Widget w, XtPointer client, XtPointer call);
extern void sg_all (void);
//extern void sg_stats (Obs *op, int nop);
//extern void make_gcs (Display *dsp, Window win);

//extern Widget sgform_w;	/* overall form dialog */
extern Widget sgda_w;	/* drawing area */
extern Widget stats_w;  /* label to hold the stats */
//extern Widget stats_w;	/* label to hold the stats */

//extern GC c_gc, o_gc,w_gc,fg_gc,bg_gc;	/* GCs */

static FILE * dbfp;
void start_dblog(void);
void end_dblog(void);
void dblog(char * dbmsg);

#define	SDC	XmSTRING_DEFAULT_CHARSET

/* these are used to describe and semi-automate making the main pulldown menus
 */
typedef struct {
    char *name;		/* button name, or separator name if !cb */
    char *label;	/* button label (use name if 0) */
    char *acc;		/* button accelerator, if any */
    char *acctext;	/* button accelerator text, if any */
    char mne;		/* button mnemonic */
    void (*cb)();	/* button callback, or 0 to indicate a separator */
    XtPointer client;	/* button callback client data */
} ButtonInfo;
typedef struct {
    char *cb_name;	/* cascade button name */
    char *cb_label;	/* cascade button label (use name if 0) */
    char cb_mne;	/* cascade button mnemonic */
    char *pd_name;	/* pulldown menu name */
    ButtonInfo *bip;	/* array of ButtonInfos, one per button in pulldown */
    int nbip;		/* number of entries in bip[] */
} PullDownMenu;

/* globally visible variables */
Widget toplevel_w;
XtAppContext telsched_app;
char *myclass = "TelSched";
Now now;		/* when we consider now */
char imdir[1024];	/* default image directory */

/* variables set from the config file -- see init_cfg() */
static char telschedcfg[] = "archive/config/telsched.cfg";
static char cameracfg[]   = "archive/config/camera.cfg";
static char filtercfg[]   = "archive/config/filter.cfg";
double MAXHA;
double MAXDEC;
double MINALT;
double MAXALT;
double LSTDELTADEF;
int COMPRESSH;
int DEFBIN;
int DEFIMW, DEFIMH;
double SUNDOWN;
int IGSUN;
int NBIAS;
int NTHERM;
int NFLAT;
double THERMDUR;
double CAMDIG_MAX;
FilterInfo *filterp;
int nfilt;
char FDEFLT;
char MESHFILTER;
double MESHEXPTIME;
int MESHCOMP;
double PTGRAD;
double PHOTBDUR;
double PHOTVDUR;
double PHOTRDUR;
double PHOTIDUR;

static void mkTopLevel (int *argcp, char *argv[]);
static void makeMainWindow(Widget top);
static void fillInMenuBar (Widget mb);
static void createMain (Widget p);
static Widget makePulldown (Widget mb_w, PullDownMenu *pdmp);
static void init_cfg(void);
static void init_now(void);
static void quit_cb (Widget w, XtPointer client, XtPointer call);
static void mb_cb (Widget w, XtPointer client, XtPointer call);
static void createScrolledList(Widget rc);
static void logo_exp_cb (Widget w, XtPointer client, XtPointer call);
static Widget createLogo (Widget f_w);
static void setScrolledListLoc(int i);
static void off_cb (Widget w, XtPointer client, XtPointer call);
static void edit_cb (Widget w, XtPointer client, XtPointer call);
static void sl_scroll_cb (Widget w, XtPointer client, XtPointer call);
static void sort_cb (Widget w, XtPointer client, XtPointer call);
static void newdate_cb (Widget w, XtPointer client, XtPointer call);
static void newscan_cb (Widget w, XtPointer client, XtPointer call);
static void createColumnHeading (Widget rc);
static void destroyEditMenus(void);
static void readNewSchedFile (char *filename);
static void readNewCatFile (char *filename);
static int eligible(Obs *op);
static void manageSchedFileMenu(void);
static void createSchedFSB(void);
static void createNewDateDialog(void);
static void date_ok_cb (Widget w, XtPointer client, XtPointer call);
static void date_today_cb (Widget w, XtPointer client, XtPointer call);
static void date_cancel_cb (Widget w, XtPointer client, XtPointer call);
static void opensched_cb (Widget w, XtPointer client, XtPointer call);
static void manageSaveMenu(void);
static void createListFN(void);
static void createSaveMenu(void);
static void manageImDirMenu(void);
static void createImDirMenu(void);
static void imdir_cb (Widget w, XtPointer client, XtPointer call);
static void save_cb (Widget w, XtPointer client, XtPointer call);
static void set_newdate (double Mjd);
static void setElig(int start);

#define	NSL		25	/* rows in the scrolled list */

static Widget dusk_w;	/* label to display Dusk */
static Widget dawn_w;	/* label to display Dawn */
static Widget msg_w;	/* label to display misc messages  */
static Widget ban_w;	/* label to display banner  */

static Widget sfname_w;	/* current schedule scan count label widget */
static Widget slsb_w;	/* the home-brew "scrolled list" scroll bar widget */
static Widget schedd_w;	/* the schedule file selection dialog */
static Widget imdir_w;	/* the image directory prompt dialog */

static Widget newdate_w;/* dialog to set new date */

static Widget saved_w;	/* save dialog */
static Widget listfn_w;	/* listing TF in saved_w */

static ObsDsp slod[NSL];/* ObsDsps in the scrolled list */
static int slopi = -1;	/* index into workop[] that is in first slod row */
static Obs *workop;	/* malloced list of Obs in the scrolled list */
static int nworkop;	/* number of Obs in workop[] */
static double mjddawn, mjddusk; /* mjd of dawn and dusk today */

/* constants for mb_cb() */
enum {
    OPEN_SCHED, DEL_SCHED, OPEN_CAT, OPEN_SLS, SET_COOKED_DIR, SAVE
};

static String fallbacks[] = {
    "TelSched*CtrlTbl*StopL.background: #aa4040",
    "TelSched*CtrlTbl*StopL.foreground: white",
    "TelSched*CurrentRun*foreground: #000065",
    "TelSched*PhotStd*nFields.value: 6",
    "TelSched*PhotStd*priority.value: 0",
    "TelSched*PointMesh*priority.value: 0",
    "TelSched*SchedFSB.cancelLabelString: Close",
    "TelSched*SchedFSB.okLabelString: Read",
    "TelSched*SchedFSB.pattern: *.sch",
    "TelSched*ShutterF*ShS.background: #aa4040",
    "TelSched*ShutterF*ShS.foreground: white",
    "TelSched*XmTextField.marginHeight: 0",
    "TelSched*XmTextField.marginWidth: 0",
    "TelSched*Header*foreground: #2a2",
    "TelSched*background: cornsilk3",
    //"TelSched*fontList: -*-fixed-bold-r-normal-*-14-*-*-*-c-*-*-*",
    "TelSched*fontList: -*-lucidatypewriter*medium*-12-*",
    "TelSched*foreground: navy",
    "TelSched*highlightThickness: 0",
    "TelSched.doneColor: #808080",
    "TelSched.eligibleColor: #006500",
    "TelSched.notEligibleColor: #e61000",

    NULL
};

int
main(argc, argv)
int argc;
char *argv[];
{
//	printf("Staring telsched KMI\n");
	telOELog (argv[0]);
	fprintf(stderr, "Starting telsched\n");

	mkTopLevel (&argc, argv);

	makeMainWindow (toplevel_w);

	/* create these to pick up their defaults */
	createImDirMenu();

	XtRealizeWidget (toplevel_w);

	init_cfg();
	fprintf(stderr, "Calling init_now()\n");
	init_now();

	XtAppMainLoop(telsched_app);

	return (1);
}

/* called to set or unset the watch cursor on the main window.
 * allow for nested requests.
 */
void
watch_cursor(want)
int want;
{
	static Cursor wc;
	static int nreqs;
	Window win = XtWindow(toplevel_w);
	Display *dsp = XtDisplay(toplevel_w);

	if (!wc)
	    wc = XCreateFontCursor (dsp, XC_watch);

	if (want) {
	    if (nreqs++ > 0)
		return;
	    XDefineCursor (dsp, win, wc);
	} else {
	    if (--nreqs > 0)
		return;
	    XUndefineCursor (dsp, win);
	}

	XFlush (dsp);
	XmUpdateDisplay (toplevel_w);
}

void
get_obsarray (opp, nopp)
Obs **opp;
int *nopp;
{
	*opp = workop;
	*nopp = nworkop;
}

/* return 0 if s is a date string that can fall between the current dusk and
 * dawn, else return -1.
 * the only mismatch we tolerate is s == NULL or empty.
 */
int
dateistoday (s)
char *s;
{
	int sday, dawnday, duskday;
	double d, datemjd;
	int m, y;

	if (!s || s[0] == '\0')
	    return (0);

	if (sscanf (s,   "%d/%lf/%d", &m, &d, &y) != 3)
	    return (-1);
	cal_mjd (m, d, y, &datemjd);
	sday = (int)floor(mjd_day(datemjd));

	duskday = (int)floor(mjd_day(mjddusk));
	dawnday = (int)floor(mjd_day(mjddawn));

	return (duskday == sday || dawnday == sday ? 0 : -1);
}

/* return our cached mjd of dawn and dusk today.
 */
void
dawnduskToday (mjddawnp, mjdduskp)
double *mjddawnp, *mjdduskp;
{
	*mjddawnp = mjddawn;
	*mjdduskp = mjddusk;
}

/* called by any code which wants to add to the current Obs list.
 * caller may free newobs when we return.
 */
void
addSchedEntries (newobs, nnewobs)
Obs *newobs;
int nnewobs;
{
	char *new;
	int nnew;

	/* grow the workop list, and add new items at the end */
	nnew = (nworkop + nnewobs)*sizeof(Obs);
	new = workop ? realloc ((char *)workop, nnew) : malloc (nnew);
	if (!new) {
	    msg ("No memory for %d more scans", nnewobs);
	    return;
	}
	workop = (Obs *)new;
	memcpy ((char *)(&workop[nworkop]), (char *)newobs,nnewobs*sizeof(Obs));
	nworkop += nnewobs;

	Obs* pw = &workop[nworkop];
	strcpy(pw->scan.extcmd, newobs->scan.extcmd);

	/* update display */
	setElig(nworkop - nnewobs);
	setScrolledListLoc (nworkop-nnewobs);	/* nice to show new items */
	updateScrolledList ();
	updateDeleteSchedMenu();
	sg_update();
	wlprintf (sfname_w, "%4d", nworkop);

	/* acknowledge */
	msg ("Added %d scan%s from %s", nnewobs, nnewobs == 1 ? "" : "s",
							newobs->scan.schedfn);
}

/* called from the schedule delete dialog.
 * remove all scans that came from the given schedule file.
 */
void
deleteSchedEntries (schedfn)
char *schedfn;
{
	Obs *newop;
	int old, new;

	destroyEditMenus();

	newop = (Obs *) calloc (nworkop, sizeof(Obs));
	if (!newop) {
	    printf ("No more memory\n");
	    exit (1);
	}

	for (old = new = 0; old < nworkop; old++)
	    if (strcmp (workop[old].scan.schedfn, schedfn) != 0)
		newop[new++] = workop[old];
	free ((void *)workop);
	workop = newop;
	nworkop = new;

	setScrolledListLoc (-1);
	updateScrolledList ();
	sg_update();

	msg ("Deleted %d scans from %s", old - new, schedfn);
	wlprintf (sfname_w, "%4d", nworkop);
}

/* remove all scans */
void
deleteAllSchedEntries()
{
	if (nworkop == 0) {
	    msg ("No scans to delete");
	    return;
	}

	destroyEditMenus();

	free ((char *)workop);
	workop = NULL;
	nworkop = 0;

	setScrolledListLoc (0);
	updateScrolledList ();
	sg_update();

	msg ("Deleted all scans");
	wlprintf (sfname_w, "%4d", nworkop);
}

/* called when the Ok button on the catalog file selection box is pressed. */
/* ARGSUSED */
void
opencat_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmFileSelectionBoxCallbackStruct *s
				= (XmFileSelectionBoxCallbackStruct  *)call;
	char *filename;

	XmStringGetLtoR (s->value, SDC, &filename);

	if (strlen(basenm(filename)) == 0) {
	    wlprintf (msg_w, "No filename");
	} else {
	    watch_cursor(1);
	    destroyEditMenus();
	    readNewCatFile (filename);
	    watch_cursor(0);
	}

	XtFree (filename);
}

void
computeCir (op)
Obs *op;
{
	Scan *sp = &op->scan;
	Now n = now;

	/* just skip camera cal scans -- locations are irrelevant */
	if (sp->ccdcalib.data == CD_NONE)
	    return;

	/* compute circumstances, using utcstart if set else now */
	if (op->utcstart != NOTIME)
	    n.n_mjd = mjd_day(n.n_mjd) + op->utcstart/24.0;
	(void) obj_cir (&n, &sp->obj);

	/* can always compute today's rise/set */
	riset_cir (&n, &sp->obj, -MINALT, &op->rs);
}

static void
mkTopLevel (int *argcp, char *argv[])
{
	char title[128];
	Arg args[20];
	char *vp;
	int n;

	vp = strchr ("$Revision: 1.5 $", ':') + 1;
	sprintf (title, "Telescope Scheduler -- Version %.5s", vp);

	n = 0;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	XtSetArg (args[n], XmNtitle, title); n++;
	toplevel_w = XtAppInitialize (&telsched_app, myclass, NULL, 0,
					    argcp, argv, fallbacks, args, n);
}

/* the main container is a form.
 * it contains a menu bar, a rowcolumn for everything else including a
 * home-brew scrolled list.
 */
static void
makeMainWindow(top)
Widget top;
{
	Arg args[20];
	Widget f;
	Widget rc;
	Widget mb;
	int n;

	n = 0;
	f = XtVaCreateManagedWidget ("Form", xmFormWidgetClass, top, NULL);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	mb = XmCreateMenuBar (f, "MB", args, n);
	XtManageChild (mb);
	fillInMenuBar (mb);

	/* make a rowcolumn for the bulk of the rest of the main window.
	 */
	rc = XtVaCreateManagedWidget ("MRC", xmRowColumnWidgetClass, f,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, mb,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNisAligned, False,
	    XmNmarginHeight, 0,
	    XmNmarginWidth, 0,
	    XmNspacing, 0,
	    NULL);

	/* make and init everything else
	 */
	createMain (rc);
	createScrolledList(rc);
	setScrolledListLoc(0);
}

static void
fillInMenuBar (mb)
Widget mb;
{
	static ButtonInfo file_buttons[] = {
	    {"Read Schedule file...", 0, 0, 0,'S',mb_cb,(XtPointer)OPEN_SCHED},
	    {"Read Catalog file...", 0, 0, 0,'C',mb_cb,(XtPointer)OPEN_CAT},
	    {"Read Scanlist file...", 0, 0, 0,'l',mb_cb,(XtPointer)OPEN_SLS},
	    {0, 0, 0, 0, 0, 0, 0},
	    {"List or Delete loaded files...", 0, 0, 0, 'L', mb_cb,
							(XtPointer)DEL_SCHED},
	    {"Set Default Image directory...",0,0,0,'D',
					    mb_cb, (XtPointer)SET_COOKED_DIR},
	    {"Save...", 0, 0, 0,'v',mb_cb,(XtPointer)SAVE},
	    {0, 0, 0, 0, 0, 0, 0},
	    {"Quit", 0, 0, 0, 'Q', quit_cb, 0},
	};
	static PullDownMenu file_pd = {
	    "File", 0, 'F', "file_pd", file_buttons, XtNumber(file_buttons),
	};
	static ButtonInfo option_btns[] = {
	    {"Sort scans", 0, 0, 0, 'o', sort_cb},
	    {"Set new date...", 0, 0, 0, 'd', newdate_cb},
	    {"Display timeline...", 0, 0, 0, 't', sg_manage},
	    {0, 0, 0, 0, 0, 0, 0},
	    {"Add one New scan...", 0, 0, 0, 'N', newscan_cb},
	    {"Add Photometric scans...", 0,0, 0, 'P', stdp_cb},
	    {"Add Pointing Mesh scans...", 0,0, 0, 'M', pting_cb},
	    {"Add Camera Calibration scans...", 0,0, 0, 'C', camcal_cb},
	};
	static PullDownMenu option_pd = {
	    "Options", 0, 'O', "option_pd", option_btns, XtNumber(option_btns),
	};

	(void) makePulldown (mb, &file_pd);
	(void) makePulldown (mb, &option_pd);
}

/* create/manage a cascade button with a pulldown menu off a menu bar.
 * return the cascade button.
 */
static Widget
makePulldown (mb_w, pdmp)
Widget mb_w;
PullDownMenu *pdmp;
{
	Widget pulldown_w;
	Widget button;
	Widget cascade;
	XmString accstr = 0, labstr = 0;
	Widget sep;
	Arg args[20];
	int n;
	int i;

	/* make the pulldown menu */

	n = 0;
	pulldown_w = XmCreatePulldownMenu (mb_w, pdmp->pd_name, args, n);

	/* fill it with buttons and/or separators */

	for (i = 0; i < pdmp->nbip; i++) {
	    ButtonInfo *bip = &pdmp->bip[i];

	    if (!bip->cb) {
		sep = XmCreateSeparator (pulldown_w, bip->name, args, n);
		XtManageChild (sep);
		continue;
	    }

	    n = 0;
	    if (bip->acctext && bip->acc) {
		accstr = XmStringCreate(bip->acctext, SDC);
		XtSetArg (args[n], XmNacceleratorText, accstr); n++;
		XtSetArg (args[n], XmNaccelerator, bip->acc); n++;
	    }
	    if (bip->label) {
		labstr = XmStringCreate (bip->label, SDC);
		XtSetArg (args[n], XmNlabelString, labstr); n++;
	    }
	    XtSetArg (args[n], XmNmnemonic, bip->mne); n++;
	    button = XmCreatePushButton (pulldown_w, bip->name, args, n);
	    XtManageChild (button);
	    XtAddCallback (button, XmNactivateCallback, bip->cb,
							(XtPointer)bip->client);
	    if (bip->acctext && bip->acc)
		XmStringFree (accstr);
	    if (bip->label)
		XmStringFree (labstr);
	}

	/* create a cascade button and glue them together */

	n = 0;
	if (pdmp->cb_label) {
	    labstr = XmStringCreate (pdmp->cb_label, SDC);
	    XtSetArg (args[n], XmNlabelString, labstr);  n++;
	}
	XtSetArg (args[n], XmNsubMenuId, pulldown_w);  n++;
	XtSetArg (args[n], XmNmnemonic, pdmp->cb_mne); n++;
	cascade = XmCreateCascadeButton (mb_w, pdmp->cb_name, args, n);
	if (pdmp->cb_label)
	    XmStringFree (labstr);
	XtManageChild (cascade);

	return (cascade);
}

/* create all of main window but the scrolled list and the menu bar.
 * p is a vertical row column.
 */
static void
createMain (p)
Widget p;
{
	typedef struct {
	    char *name;		/* label widget name */
	    char *label;	/* initial label string */
	    Widget *wp;		/* pointer to widget to save, or NULL */
	} FormItem;
	static FormItem infobar[] = {
	    {"DUSKL", "Dusk:", NULL},
	    {"DUSKV", " ",     &dusk_w},
	    {"GAP",   " ",    NULL},
	    {"GAP",   " ",    NULL},
	    {"DAWNL", "Dawn:", NULL},
	    {"DAWNV", " ",     &dawn_w},
	    {"GAP",   " ",    NULL},
	};
	Widget f, fr_w;
	Widget w;
	Widget sw_w; // REMOVE ME, MAYBE
	//Widget sgda_w; // REMOVE ME, MAYBE
	int i;
	int n = 0; // REMOVE ME
	Arg args[20]; // REMOVE ME

	/* top label is for the BANNER */
	ban_w = XtVaCreateManagedWidget ("Banner", xmLabelWidgetClass, p,
	    XmNalignment, XmALIGNMENT_CENTER,
	    NULL);

	/* make a form to hold the message row and the logo */

	f = XtVaCreateManagedWidget ("MsgLgoForm", xmFormWidgetClass, p,
	    NULL);

	    /* make a frame to hold the logo */

	    //fr_w = createLogo (f);

	    /* make the message row label */

	    msg_w = XtVaCreateManagedWidget ("Msg", xmLabelWidgetClass, f,
			XmNtopAttachment, XmATTACH_FORM,
			XmNbottomAttachment, XmATTACH_FORM,
			XmNleftAttachment, XmATTACH_FORM,
			//XmNrightAttachment, XmATTACH_WIDGET,
			//XmNrightWidget, fr_w,
			XmNalignment, XmALIGNMENT_BEGINNING,
			NULL);
	    wlprintf (msg_w, "Welcome");

		/* MODIFIED - EXAMINE CLOSELY */
		n = 0;
		//XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
		//XtSetArg (args[n], XmNtopWidget, msg_w); n++;
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
		//XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
		//XtSetArg (args[n], XmNbottomWidget, sw_w); n++;
		XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
		stats_w = XmCreateLabel (p, "SL", args, n);
		wlprintf (stats_w,
		    "? Scans      ? Off\n"
		    "? NvrUp      ? CirPol\n"
		    "? LSTSTAR\n");
		XtManageChild (stats_w);

		n = 0;
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
		XtSetArg (args[n], XmNtopWidget, stats_w); n++;
		//XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
		//XtSetArg (args[n], XmNbottomWidget, c_w); n++;
		XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNscrollingPolicy, XmAUTOMATIC); n++;
		XtSetArg (args[n], XmNheight, 160); n++;
		sw_w = XmCreateScrolledWindow (p, "SW", args, n);
		XtManageChild(sw_w);

		n = 0;
	    XtSetArg (args[n], XmNwidth, SG_W); n++;
	    XtSetArg (args[n], XmNheight, SG_H); n++;
	    XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	    sgda_w = XmCreateDrawingArea (sw_w, "SGDA", args, n);
	    XtManageChild (sgda_w);
		XtAddCallback (sgda_w, XmNexposeCallback, sg_da_exp_cb, NULL);

		/* END MODIFIED */

	/* make label for UTC notice */

	w = XtVaCreateManagedWidget ("UTCL", xmLabelWidgetClass, p, NULL);
	set_xmstring (w, XmNlabelString, "All Times are UTC");

	(void) XtVaCreateManagedWidget ("Sep1", xmSeparatorWidgetClass, p,NULL);

	/* make the info bar */

	f = XtVaCreateManagedWidget ("DDTbl", xmFormWidgetClass, p,
	    XmNfractionBase, XtNumber(infobar),
	    NULL);

	    for (i = 0; i < XtNumber(infobar); i++) {
		FormItem *fip = &infobar[i];
		XmString str = XmStringCreateLtoR (fip->label, SDC);
		Arg args[20];
		int n;

		w = XtVaCreateManagedWidget (fip->name, xmLabelWidgetClass, f,
		    XmNtopAttachment, XmATTACH_FORM,
		    XmNbottomAttachment, XmATTACH_FORM,
		    XmNlabelString, str,
		    NULL);
		XmStringFree (str);

		n = 0;
		if (i & 1) {
		    XtSetArg(args[n],XmNleftAttachment, XmATTACH_POSITION); n++;
		    XtSetArg(args[n],XmNleftPosition, i); n++;
		} else {
		    XtSetArg(args[n],XmNrightAttachment, XmATTACH_POSITION);n++;
		    XtSetArg(args[n],XmNrightPosition, i+1); n++;
		}
		XtSetValues (w, args, n);

		if (fip->wp)
		    *fip->wp = w;
	    }

	(void) XtVaCreateManagedWidget ("Sep2", xmSeparatorWidgetClass, p,NULL);

	createColumnHeading (p);
}

/* create the home-brew scrolled list.
 * build it in a form. put the vertical scroll bar on the right edge, and
 * connect a rowcolumn to it with NSL ObsDsp control sets in it.
 * p is a row column.
 */
static void
createScrolledList (p)
Widget p;
{
	Widget f;
	Widget rc;
	int i;

	f = XtVaCreateManagedWidget ("SLF", xmFormWidgetClass, p, NULL);

	rc = XtVaCreateManagedWidget ("SLRC", xmRowColumnWidgetClass, f,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNmarginWidth, 0,
	    XmNmarginHeight, 0,
	    XmNspacing, 0,
	    NULL);

	    for (i = 0; i < NSL; i++) {
		ObsDsp *odp = &slod[i];
		initObsDsp (odp, rc, "SLOD");
		XtAddCallback (odp->off, XmNvalueChangedCallback, off_cb,
								(XtPointer)i);
		set_xmstring (odp->off, XmNlabelString, "Off");
		XtAddCallback (odp->edit, XmNactivateCallback, edit_cb,
								(XtPointer)i);
		set_xmstring (odp->edit, XmNlabelString, "Edit");
	    }

	slsb_w = XtVaCreateManagedWidget ("SLSB", xmScrollBarWidgetClass, f,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, rc,
	    XmNpageIncrement, NSL-1,
	    XmNsliderSize, NSL,
	    XmNminimum, 0,
	    NULL);

	XtAddCallback (slsb_w, XmNdragCallback, sl_scroll_cb, 0);
	XtAddCallback (slsb_w, XmNvalueChangedCallback, sl_scroll_cb, 0);
}

/* open the logo file, logo.xpm, and put in a drawing area.
 * f_w is the overall form widget.
 * return the frame.
 */
static Widget
createLogo (f_w)
Widget f_w;
{
	static Pixmap pm;
	Display *dsp = XtDisplay(f_w);
	Window root = RootWindow(dsp, 0);
	char fn[1024];
	Widget da_w;
	Widget fr_w;
	Arg args[20];
	XpmAttributes xpma;
	int xpms;
	int n;

	strcpy (fn, "archive/config/logo.xpm");
	telfixpath (fn, fn);

	xpma.valuemask = 0;
	xpms = XpmReadFileToPixmap(dsp, root, fn, &pm, NULL, &xpma);
	if (xpms != XpmSuccess) {
	    xpma.width = 100;
	    xpma.height = 100;
	}

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNtopOffset, 3); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightOffset, 3); n++;
	fr_w = XmCreateFrame (f_w, "LogoFrame", args, n);
	//XtManageChild (fr_w);

	    n = 0;
	    XtSetArg (args[n], XmNwidth, xpma.width); n++;
	    XtSetArg (args[n], XmNheight, xpma.height); n++;
	    da_w = XmCreateDrawingArea (fr_w, "Logo", args, n);
	    if (xpms == XpmSuccess)
		XtAddCallback(da_w, XmNexposeCallback, logo_exp_cb,
								(XtPointer)&pm);
	    XtManageChild (da_w);

	return (fr_w);
}

/* callback for the logo drawing area expose.
 * client is a pointer to the Pixmap to use.
 */
/* ARGSUSED */
static void
logo_exp_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static GC gc;
	Pixmap *pmp = (Pixmap *)client;
	XmDrawingAreaCallbackStruct *s = (XmDrawingAreaCallbackStruct *)call;
	XExposeEvent *evp = &s->event->xexpose;

	if (!gc)
	    gc = XCreateGC (evp->display, evp->window, 0, NULL);

	XCopyArea (evp->display, *pmp, evp->window, gc, evp->x, evp->y,
				   evp->width, evp->height, evp->x, evp->y);
}

/* called when the scroll bar is changed.
 * set slopi and fill slod with NSL objects starting with sp->value.
 */
/* ARGSUSED */
static void
sl_scroll_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmScrollBarCallbackStruct *sp = (XmScrollBarCallbackStruct *)call;

	if (nworkop > 0) {
	    watch_cursor(1);
	    slopi = sp->value;
	    updateScrolledList();
	    watch_cursor(0);
	}
}

static void
init_cfg()
{
#define	NTCFG	(sizeof(tcfg)/sizeof(tcfg[0]))
#define	NCCFG	(sizeof(ccfg)/sizeof(ccfg[0]))
	static char BANNER[132];
	static double LONGITUDE, LATITUDE, TEMPERATURE, PRESSURE, ELEVATION;
	static CfgEntry tcfg[] = {
	    {"MINALT",		CFG_DBL, &MINALT},
	    {"MAXALT",		CFG_DBL, &MAXALT},
	    {"MAXHA",		CFG_DBL, &MAXHA},
	    {"MAXDEC",		CFG_DBL, &MAXDEC},
	    {"SUNDOWN",		CFG_DBL, &SUNDOWN},
	    {"LSTDELTADEF",	CFG_DBL, &LSTDELTADEF},
	    {"COMPRESS",	CFG_INT, &COMPRESSH},
	    {"DEFBIN",		CFG_INT, &DEFBIN},
	    {"DEFIMW",		CFG_INT, &DEFIMW},
	    {"DEFIMH",		CFG_INT, &DEFIMH},
	    {"IGSUN",		CFG_INT, &IGSUN},
	    {"LONGITUDE",	CFG_DBL, &LONGITUDE},
	    {"LATITUDE",	CFG_DBL, &LATITUDE},
	    {"TEMPERATURE",	CFG_DBL, &TEMPERATURE},
	    {"PRESSURE",	CFG_DBL, &PRESSURE},
	    {"ELEVATION",	CFG_DBL, &ELEVATION},
	    {"MESHFILTER",	CFG_STR, &MESHFILTER, 1},
	    {"MESHCOMP",	CFG_INT, &MESHCOMP},
	    {"MESHEXPTIME",	CFG_DBL, &MESHEXPTIME},
	    {"PTGRAD",		CFG_DBL, &PTGRAD},
	    {"PHOTBDUR",	CFG_DBL, &PHOTBDUR},
	    {"PHOTVDUR",	CFG_DBL, &PHOTVDUR},
	    {"PHOTRDUR",	CFG_DBL, &PHOTRDUR},
	    {"PHOTIDUR",	CFG_DBL, &PHOTIDUR},
	    {"BANNER",		CFG_STR, BANNER, sizeof(BANNER)},
	};
	static CfgEntry ccfg[] = {
	    {"NBIAS",		CFG_INT, &NBIAS},
	    {"NTHERM",		CFG_INT, &NTHERM},
	    {"NFLAT",		CFG_INT, &NFLAT},
	    {"THERMDUR",	CFG_DBL, &THERMDUR},
	    {"CAMDIG_MAX",	CFG_DBL, &CAMDIG_MAX},
	};
	char buf[1024];
	char *vp;
	int deff;
	int n;

	/* read file */
	n = readCfgFile (1, telschedcfg, tcfg, NTCFG);
	if (n != NTCFG) {
	    cfgFileError (telschedcfg, n, NULL, tcfg, NTCFG);
	    exit(1);
	}

	/* some get loaded into now */
	now.n_lng = -LONGITUDE;		/* we want rads +E */
	now.n_lat = LATITUDE;
	now.n_temp = TEMPERATURE;
	now.n_pressure = PRESSURE;
	now.n_elev = ELEVATION/ERAD;	/* we want earth radii */
	now.n_tz = -floor(radhr(now.n_lng) + .5);	/* TODO: ok guess? */
	wlprintf (ban_w, "%s", BANNER);

	/* read camera.cfg */
	n = readCfgFile (1, cameracfg, ccfg, NCCFG);
	if (n != NCCFG) {
	    cfgFileError (cameracfg, n, NULL, ccfg, NCCFG);
	    exit(1);
	}

	/* read filter.cfg */
	nfilt = readFilterCfg (1, filtercfg, &filterp, &deff, buf);
	if (nfilt < 0) {
	    fprintf (stderr, "%s: %s\n", filtercfg, buf);
	    exit (1);
	}

	/* get default filter from filter.cfg */
	if (read1CfgEntry (1, filtercfg, "FDEFLT", CFG_STR, &FDEFLT, 1) < 0) {
	    fprintf (stderr, "%s: %s\n", filtercfg, "FDEFLT");
	    exit (1);
	}

	vp = &FDEFLT;
	if (legalFilters(1, &vp) < 0)
	    exit(1);
}

/* set the time to now and compute dawn/dusk times.
 * we actually set it to the next full day allowing for dusk and longitude.
 */
static void
init_now()
{
	time_t t;

	/* UNIX t is seconds since 00:00:00 1/1/1970 UTC on UNIX systems;
	 * mjd was 25567.5 then.
	 */
	t = time(NULL);
	now.n_mjd = mjd_day (25567.5 + 1.0 + (t-86400/2)/SPD);
	now.n_epoch = EOD;

	set_newdate (now.n_mjd);

	/* start at dusk */
	now.n_mjd += mjd_hr(mjddusk)/24.0;
}

/* called from the Quit button on the File pulldown with client == 0.
 * called from this "Are you sure" question dialog with client == 1.
 */
/* ARGSUSED */
static void
quit_cb (Widget w, XtPointer client, XtPointer call)
{
	static Widget rusure;

	if (!rusure) {
	    Arg args[20];
	    int n;

	    n = 0;
	    rusure = XmCreateQuestionDialog (toplevel_w, "RUSure", args, n);
	    set_xmstring (rusure, XmNokLabelString, "Yes -- quit");
	    set_xmstring (rusure, XmNcancelLabelString, "No -- resume");
	    set_xmstring (rusure, XmNmessageString, "Really quit telsched?");
	    set_xmstring (rusure, XmNdialogTitle, "Telsched Quit");
	    XtUnmanageChild(XmMessageBoxGetChild(rusure, XmDIALOG_HELP_BUTTON));
	    XtAddCallback (rusure, XmNokCallback, quit_cb, (XtPointer)1);
	}

	if ((int)client == 1) {
	    XtCloseDisplay (XtDisplay(w));
	    exit(0);
	} else
	    XtManageChild (rusure);
}

/* callback from some of the pushbuttons on the main menu bar.
 * client is an enum to distinguish.
 */
/* ARGSUSED */
static void
mb_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	switch ((int)client) {
	case OPEN_SCHED:
	    manageSchedFileMenu();
	    break;
	case DEL_SCHED:
	    manageDeleteSchedMenu();
	    break;
	case OPEN_CAT:
	    manageCatFileMenu();
	    break;
	case OPEN_SLS:
	    manageSLSFileMenu();
	    break;
	case SET_COOKED_DIR:
	    manageImDirMenu();
	    break;
	case SAVE:
	    manageSaveMenu();
	    break;
	}
}

/* callback from the Sort button */
/* ARGSUSED */
static void
sort_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int i;

	watch_cursor(1);

	/* first reset all the Offs back on unless the date is wrong. */
	for (i = 0; i < nworkop; i++) {
	    Obs *op = &workop[i];
	    op->off = dateistoday (op->date) == 0 ? 0 : 1;
	    if (op->off)
		ACPYZ (op->yoff, "Wrong date");
	    else
		ACPYZ (op->yoff, "");	/* reset, let sort fill in */
	}

	sortscans (&now, workop, nworkop);
	destroyEditMenus();
	setElig(0);
	setScrolledListLoc (-1);
	updateScrolledList();
	sg_update();

	watch_cursor(0);
}

/* callback from the New Scan option button */
/* ARGSUSED */
static void
newscan_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	newRun();
}

/* callback from the New Date option button */
/* ARGSUSED */
static void
newdate_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!newdate_w)
	    createNewDateDialog();

	if (XtIsManaged(newdate_w))
	    XtUnmanageChild (newdate_w);
	else
	    XtManageChild (newdate_w);
}

/* callback from a OFF togglebutton in the scrolled list.
 * client is an index into the slod[] array.
 */
/* ARGSUSED */
static void
off_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int n = (int) client;
	ObsDsp *odp = &slod[n];
	int opi = n + slopi;
	Obs *op = &workop[opi];

	op->off = XmToggleButtonGetState (w);
	if (op->off && op->yoff[0] == '\0')
	    ACPYZ (op->yoff, "Operator's choice");

	setObsDspColor (odp, op);

	/* update count */
	sg_update();
}

/* callback from an EDIT button in the scrolled list.
 * client is an index into the slod[] array.
 */
/* ARGSUSED */
static void
edit_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int n = (int) client;
	int opi = n + slopi;
	Obs *op = &workop[opi];

	if (op->editsh)
	    msg ("Please use the edit window already open for this entry.");
	else
	    editRun (op);
}

/* create the column heading.
 * we don't need to keep the widget info around.
 */
static void
createColumnHeading (rc)
Widget rc;
{
	ObsDsp hdr;

	initObsDsp (&hdr, rc, "Header");
	lookLikeLabel (hdr.off);
	lookLikeLabel (hdr.edit);

	/* this is really the scan counts */
	wlprintf (hdr.off, "N: ");
	wlprintf (hdr.edit, "   0");
	sfname_w = hdr.edit;

	/* enter the column headings */
	wtprintf (hdr.f[OD_NAME], "Source");
	wtprintf (hdr.f[OD_RA], "RA");
	wtprintf (hdr.f[OD_DEC], "Dec");
	wtprintf (hdr.f[OD_EPOCH], "Epoch");
	wtprintf (hdr.f[OD_HA], "HA");
	wtprintf (hdr.f[OD_ALT], "El");
	wtprintf (hdr.f[OD_AZ], "Az");
	wtprintf (hdr.f[OD_DUR], "Dur");
	wtprintf (hdr.f[OD_FILT], "F");
	wtprintf (hdr.f[OD_RISETM], "Rises");
	wtprintf (hdr.f[OD_TRANSTM], "Trans");
	wtprintf (hdr.f[OD_SETTM], "Sets");
	wtprintf (hdr.f[OD_UTCSTART], "Start");
}

/* go through the workop array and destroy all Edit menus that are up now.
 */
static void
destroyEditMenus()
{
	int i;

	for (i = 0; i < nworkop; i++)
	    if (workop[i].editsh) {
		XtDestroyWidget (workop[i].editsh);
		workop[i].editsh = (Widget)0;
	    }
}

/* read a new schedule file and add to the current list.
 * don't mess with the old one until we find some good stuff in the new one.
 */
static void
readNewSchedFile (name)
char *name;
{
	Obs *tmpop;
	int tmpnop;

	tmpnop = readObsFile (name, &tmpop);

	if (tmpnop > 0) {
	    addSchedEntries (tmpop, tmpnop);
	    free ((char *)tmpop);
	}
}

/* read a new catalog file and add the entries in the scrolled list.
 * don't discard the old one until we find some good stuff in the new one.
 */
static void
readNewCatFile (name)
char *name;
{
	Obs *tmpop;
	int tmpnop;

	tmpnop = readCatFile (name, &tmpop);

	if (tmpnop > 0) {
	    addSchedEntries (tmpop, tmpnop);
	    free ((void *)tmpop);
	}
}

/* write a new message to the msg area
 * N.B. don't end with a \n
 */
void
msg (char *fmt, ...)
{
	va_list ap;
	char buf[1024];
    static int dblock = 0;

    if(!dblock || buf[0] == '!') {
	va_start (ap, fmt);
	vsprintf (buf, fmt, ap);
	va_end (ap);
	wlprintf (msg_w, "%s", buf);
    }

    if(buf[0] == '!') dblock = 1;

	XmUpdateDisplay (toplevel_w);
}

/* set slopi and the slsb_w resources to make entry i at the top, but
 * also try to keep as much of the list visible as possible.
 * if i < 0, then adjust automatically so first good one is on top.
 */
static void
setScrolledListLoc (i)
int i;
{
	/* <0 means put first good one at top */
	if (i < 0) {
	    for (i = 0; i < nworkop; i++)
		if (wantinsls(&workop[i]))
		    break;
	    if (i == nworkop)
		i = 0;	/* none are any good! might as well scroll to top */
	}

	/* use i unless it is so far down it wastes entries */
	if (nworkop - i < NSL) {
	    slopi = nworkop - NSL;
	    if (slopi < 0)
		slopi = 0;
	} else
	    slopi = i;

	/* slider stays at NSL, minimum stays at 0.
	 * value callback is always in the range min .. max-slider.
	 */
	XtVaSetValues (slsb_w,
	    XmNmaximum, nworkop <= NSL ? NSL : nworkop,
	    XmNvalue, slopi,
	    NULL);
}

/* update the scrolled list.
 */
void
updateScrolledList()
{
	Now *np = &now;
	int i;

	for (i = 0; i < NSL; i++) {
	    ObsDsp *odp = &slod[i];
	    int opi = slopi + i;

	    if (opi >= 0 && opi < nworkop) {
		Obs *op = &workop[opi];
		XtSetSensitive (odp->off, True);
		XtSetSensitive (odp->edit, True);
		XmToggleButtonSetState (odp->off, op->off, False);
		fillTimeDObsDsp (odp, op, np);
		fillFixedObsDsp (odp, op);	/* in case it's edited */
		setObsDspColor (odp, op);
	    } else {
		setObsDspEmpty (odp);
		XtSetSensitive (odp->off, False);
		XtSetSensitive (odp->edit, False);
	    }
	}
}

/* return 1 if op can be run now else 0.
 * also set op->ynot if returning 0.
 * N.B. we assume computeCir() has been called recently.
 */
static int
eligible(op)
Obs *op;
{
	if (dateistoday (op->date) < 0) {
	    ACPYZ (op->yoff, "Wrong date");
	    return (0);
	}

	if (op->scan.ccdcalib.data == CD_NONE)
	    return (1);	/* always */

	if (op->utcstart != NOTIME) {
	    /* specific run time is known so can compute circumstances */
	    Now startnow = now;
	    Obj startobj = op->scan.obj;
	    double lst, alt, ha, dec;
	    Now *np;
	    Obj *objp;

	    startnow.n_mjd = mjd_day(startnow.n_mjd) + op->utcstart/24.0;
	    obj_cir (&startnow, &startobj);

	    np = &startnow;
	    objp = &startobj;

	    now_lst (np, &lst);
	    ha = hrrad(lst) - objp->s_ra;
	    haRange (&ha);

	    dec = objp->s_dec;

	    alt = objp->s_alt;

	    if (fabs(ha) > MAXHA) {
		ACPYZ (op->yoff, "Outside MAXHA limit");
		return(0);
	    }
	    if (alt > MAXALT) {
		ACPYZ (op->yoff, "Above MAXALT limit");
		return(0);
	    }
	    if (fabs(dec) > MAXDEC) {
		ACPYZ (op->yoff, "Beyond MAXDEC limit");
		return(0);
	    }
	    if (alt < MINALT) {
		ACPYZ (op->yoff, "Below MINALT limit");
		return(0);
	    }
	    if (!IGSUN && !at_night (op->utcstart)) {
		ACPYZ (op->yoff, "Not night then");
		return(0);
	    }
	} else {
	    /* just check for general "observability" */
	    if (!((op->rs.rs_flags & RS_CIRCUMPOLAR) || is_evening(op) ||
					    is_night(op) || is_morning(op))) {
		ACPYZ (op->yoff, "Never up");
		return (0);
	    }
	}

	/* evidently ok */
	return (1);
}

static void
manageSchedFileMenu()
{
	if (!schedd_w)
	    createSchedFSB();

	if (!XtIsManaged(schedd_w))
	    XtManageChild (schedd_w);
	else
	    XtUnmanageChild (schedd_w);
}

/* create the dialog to ask for a new date.
 * save overall widget in newdate_w.
 */
static void
createNewDateDialog()
{
	Arg args[20];
	char buf[64];
	struct tm *tmp;
	time_t t;
	int n;

	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	newdate_w = XmCreatePromptDialog (toplevel_w, "NewDateF", args, n);
	set_xmstring (newdate_w, XmNselectionLabelString,
					    "Enter new date (mm/dd/yyyy):");
	XtAddCallback (newdate_w, XmNokCallback, date_ok_cb, NULL);
	set_xmstring (newdate_w, XmNokLabelString, "Apply");
	XtManageChild(XmSelectionBoxGetChild(newdate_w,XmDIALOG_APPLY_BUTTON));
	XtAddCallback (newdate_w, XmNapplyCallback, date_today_cb, NULL);
	set_xmstring (newdate_w, XmNapplyLabelString, "Tonight");
	XtAddCallback (newdate_w, XmNcancelCallback, date_cancel_cb, NULL);
	XtUnmanageChild(XmSelectionBoxGetChild(newdate_w,XmDIALOG_HELP_BUTTON));

	n = 0;
	XtSetArg (args[n], XmNtitle, "New Date"); n++;
	XtSetValues (XtParent(newdate_w), args, n);

	/* set today as an example */
	t = time(NULL);
	tmp = gmtime (&t);
	sprintf (buf, "%d/%d/%d",tmp->tm_mon+1,tmp->tm_mday,tmp->tm_year+1900);
	set_xmstring (newdate_w, XmNtextString, buf);

}

/* called from the OK button on the new date dialog */
/* ARGSUSED */
static void
date_ok_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmSelectionBoxCallbackStruct *s = (XmSelectionBoxCallbackStruct *)call;
	String str;
	int m, y;
	double d;
	double Mjd;

	XmStringGetLtoR (s->value, XmSTRING_DEFAULT_CHARSET, &str);
	mjd_cal (now.n_mjd, &m, &d, &y);
	f_sscandate (str, PREF_MDY, &m, &d, &y);
	XtFree (str);

	if (m < 1 || m > 12 || d < 1 || d > 31 || y < 1950 || y > 2050) {
	    wlprintf (msg_w, "Preposterous date. Use mm/dd/yyyy");
	    return;
	}

	cal_mjd (m, d, y, &Mjd);
	set_newdate(Mjd);
}

/* called from the Apply button on the new date dialog.
 * this is really a way to set the date to "Tonight".
 */
/* ARGSUSED */
static void
date_today_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	init_now();
}

/* called from the Cancel button on the new date dialog.
 */
/* ARGSUSED */
static void
date_cancel_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (newdate_w);
}

/* called to close the schedd_w dialog */
/* ARGSUSED */
static void
closesched_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (schedd_w);
}

static void
createSchedFSB()
{
	char buf[1024];
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	schedd_w= XmCreateFileSelectionDialog (toplevel_w, "SchedFSB", args, n);
	XtAddCallback (schedd_w, XmNcancelCallback, closesched_cb, NULL);

	n = 0;
	XtSetArg (args[n], XmNtitle, "Schedule File Selection"); n++;
	XtSetValues (XtParent(schedd_w), args, n);

	/* Ok is used for Append */
	set_xmstring (schedd_w, XmNokLabelString, "Append");
	XtAddCallback (schedd_w, XmNokCallback, opensched_cb, (XtPointer)0);

	/* Help is used as Edit */
	set_xmstring (schedd_w, XmNhelpLabelString, "Edit");
	XtAddCallback (schedd_w, XmNhelpCallback, opensched_cb, (XtPointer)1);

	/* apply TELHOME */
	telfixpath (buf, "user/schedin");
	set_xmstring (schedd_w, XmNdirectory, buf);
}

/* called when the Ok or Help button on the sched file selection box is pressed.
 * client is 0 for the former, 1 for the latter.
 * the Ok button really means Append; Help really means invoke an editor.
 * w is the FileSelectionBox.
 */
/* ARGSUSED */
static void
opensched_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmFileSelectionBoxCallbackStruct *s
				= (XmFileSelectionBoxCallbackStruct *)call;
	int helpbutton = (int)client;
	char filename[1024], *base;
	char *str;

	/* get filename */
	XmStringGetLtoR (s->value, SDC, &str);
	(void) strcpy (filename, str);
	XtFree (str);
	base = basenm(filename);

	/* check for absent */
	if (strlen(base) == 0) {
	    wlprintf (msg_w, "No filename");
	    return;
	}

	if (helpbutton) {
	    /* really Edit */
	    char *editor = getenv ("EDITOR");
	    char cmd[1024];

	    if (!editor || strcmp (editor, "xedit") == 0)
		(void) sprintf (cmd, "xedit -title \"xedit %s\" %s &",
								base, filename);
	    else
		(void) sprintf (cmd, "xterm -title \"%s %s\" -e %s %s &",
						editor, base, editor, filename);
	    system (cmd);
	} else {
	    /* really Append */
	    watch_cursor(1);
	    destroyEditMenus();
	    readNewSchedFile (filename);
	    watch_cursor(0);
	}
}

static void
manageImDirMenu()
{
	if (!imdir_w)
	    createImDirMenu();

	if (XtIsManaged(imdir_w))
	    XtUnmanageChild (imdir_w);
	else
	    XtManageChild (imdir_w);
}

static void
manageSaveMenu()
{
	if (!saved_w)
	    createSaveMenu();

	if (XtIsManaged(saved_w))
	    XtUnmanageChild (saved_w);
	else {
	    createListFN();
	    XtManageChild (saved_w);
	}
}

/* make up a name for the listing file in listfn_w.
 * just use the current time I guess.
 */
static void
createListFN()
{
	char buf[1024];
	struct tm *tmp;
	time_t t;

	t = time(NULL);
	tmp = gmtime (&t);

	(void) sprintf (buf, "user/logs/summary/%02d%02d%02d%02d.sum",
		    tmp->tm_mon+1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min);

	telfixpath (buf, buf);

	XmTextFieldSetString (listfn_w, buf);
}

static void
createImDirMenu()
{
	XmString str;
	Arg args[20];
	int n;

	n = 0;
	str = XmStringCreate ("Enter desired image directory:",
						XmSTRING_DEFAULT_CHARSET);
	XtSetArg (args[n], XmNselectionLabelString, str); n++;
	XtSetArg (args[n], XmNwidth, 350); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	imdir_w = XmCreatePromptDialog (toplevel_w, "ImDir", args, n);
	XmStringFree (str);
	XtAddCallback (imdir_w, XmNokCallback, imdir_cb, 0);
	XtUnmanageChild(XmSelectionBoxGetChild(imdir_w, XmDIALOG_HELP_BUTTON));

	/* init default dir and place in menu */
	telfixpath (imdir, "user/images");
	set_xmstring (imdir_w, XmNtextString, imdir);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Images Directory"); n++;
	XtSetValues (XtParent(imdir_w), args, n);
}

/* callback when the Ok button is pressed on the image directory prompt */
/* ARGSUSED */
static void
imdir_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	String defdir;

	/* apply TELHOME, save in imdir[] and back in widget */
	get_xmstring (imdir_w, XmNtextString, &defdir);
	strncpy (imdir, defdir, sizeof(imdir)-1);
	XtFree (defdir);
	telfixpath (imdir, imdir);
	set_xmstring (imdir_w, XmNtextString, imdir);
}

/* callback when the Ok button is pressed on the save dialog */
/* ARGSUSED */
static void
save_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmSelectionBoxCallbackStruct *s = (XmSelectionBoxCallbackStruct *)call;
	String filename;
	int i;
	Arg args[5];
	int n = 0;
	static Widget dialog;
	XmString str;

	watch_cursor(1);

	/* make sure all info is current before printing anything */
	for (i = 0; i < nworkop; i++) {
	    computeCir (&workop[i]);
	    workop[i].elig = eligible(&workop[i]) ? 1 : 0;
	}

	/* listfn_w has the listing filename */
	filename = XmTextFieldGetString (listfn_w);
	i = print_summary (&now, workop, nworkop, filename);
	XtFree (filename);

        if (i == -1) { // Error saving summary - display an error dialog
            n = 0;
	    str = XmStringCreate("Error saving summary file", XmSTRING_DEFAULT_CHARSET);
            XtSetArg(args[n], XmNdialogStyle, XmDIALOG_APPLICATION_MODAL); n++;
            XtSetArg(args[n], XmNdialogTitle, str); n++;
            XtSetArg(args[n], XmNmessageString, str); n++;
	    dialog = XmCreateErrorDialog(toplevel_w, "error", args, n);
	    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
	    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
	    XtManageChild(dialog);
	    XmStringFree(str);
	}

	/* sls too if listing ok; sls last so affirming message persists */
	if (i == 0) {
	    /* dialog has scan name */
	    XmStringGetLtoR (s->value, XmSTRING_DEFAULT_CHARSET, &filename);
	    i = print_sls (&now, workop, nworkop, imdir, filename);
	    XtFree (filename);

	    if (i == -1) { // Error saving sls file - display error dialog
                n = 0;
	        str = XmStringCreate("Error saving SLS file", XmSTRING_DEFAULT_CHARSET);
                XtSetArg(args[n], XmNdialogStyle, XmDIALOG_APPLICATION_MODAL); n++;
                XtSetArg(args[n], XmNdialogTitle, str); n++;
                XtSetArg(args[n], XmNmessageString, str); n++;
	        dialog = XmCreateErrorDialog(toplevel_w, "error", args, n);
	        XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
	        XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
	        XtManageChild(dialog);
	        XmStringFree(str);
	    }
	}

	watch_cursor(0);
}

static void
createSaveMenu()
{
	Widget l_w, rc_w;
	Arg args[20];
	char buf[1024];
	int n;

	/* create prompt dialog for the scan filename */
	n = 0;
	XtSetArg (args[n], XmNwidth, 450); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	saved_w = XmCreatePromptDialog (toplevel_w, "SavePD", args, n);
	XtAddCallback (saved_w, XmNokCallback, save_cb, 0);
	XtUnmanageChild(XmSelectionBoxGetChild(saved_w,XmDIALOG_HELP_BUTTON));
	set_xmstring (saved_w, XmNselectionLabelString, "Scan filename:");

	/* apply TELHOME */
	telfixpath (buf, "archive/telrun/telrun.sls");
	set_xmstring (saved_w, XmNtextString, buf);

	/* create a place for the listing file prompt/value too */
	n = 0;
	rc_w = XmCreateRowColumn (saved_w, "ListRC", args, n);
	XtManageChild (rc_w);

	    n = 0;
	    l_w = XmCreateLabel (rc_w, "SL", args, n);
	    set_xmstring (l_w, XmNlabelString, "Listing filename:");
	    XtManageChild (l_w);

	    n = 0;
	    listfn_w = XmCreateTextField (rc_w, "STF", args, n);
	    XtManageChild (listfn_w);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Save"); n++;
	XtSetValues (XtParent(saved_w), args, n);
}

/* set now.n_mjd to mjd, update dawn/dusk and recompute all obs info.
 */
static void
set_newdate(Mjd)
double Mjd;
{
	char buf[64], buf1[64];
	int rsstatus;

	now.n_mjd = Mjd;

	/* twilight cir always works on today; but we want dawn tomorrow */
	twilight_cir (&now, SUNDOWN, &mjddawn, &mjddusk, &rsstatus);
	if (mjddawn < mjddusk) {
	    double tmp;
	    Now tomorrow = now;
	    tomorrow.n_mjd += 1;
	    twilight_cir (&tomorrow, SUNDOWN, &mjddawn, &tmp, &rsstatus);
	}

	/* round to match display precision */
	mjddawn = floor(mjddawn*24.*60. + .5)/(24.*60.);
	mjddusk = floor(mjddusk*24.*60. + .5)/(24.*60.);

	fs_sexa (buf, mjd_hr(mjddawn), 2, 60);
	fs_date (buf1, mjd_day(mjddawn));
	wlprintf (dawn_w, "%s %s", buf, buf1);

	fs_sexa (buf, mjd_hr(mjddusk), 2, 60);
	fs_date (buf1, mjd_day(mjddusk));
	wlprintf (dusk_w, "%s %s", buf, buf1);

	updateScrolledList();
}

static void
setElig(int start)
{
	int i;

	/* compute circumstances so we can set elig, unless already off */
	for (i = start; i < nworkop; i++) {
	    Obs *op = &workop[i];
	    if (!op->off) {
		computeCir (op);
		op->elig = eligible(op);	/* sets op->yoff if returns 0 */
		if (!op->elig)
		    op->off = 1;
	    }
	}
}

void start_dblog(void)
{
	dbfp = fopen("/usr/local/telescope/logs/telsched.db.log","w");
}

void end_dblog(void)
{
	fclose(dbfp);
}

void dblog(char * dbmsg)
{
	fprintf(dbfp,"%s\n",dbmsg);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: telsched.c,v $ $Date: 2007/03/11 05:30:28 $ $Revision: 1.5 $ $Name:  $"};
