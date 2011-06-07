/* main program.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#ifdef DO_EDITRES
#include <X11/Xmu/Editres.h>
#endif

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/ScrolledW.h>
#include <Xm/Label.h>
#include <Xm/Form.h>
#include <Xm/ToggleB.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#include <Xm/MessageB.h>
#include <Xm/Separator.h>

#include "P_.h"
#include "astro.h"
#include "strops.h"
#include "xtools.h"
#include "fits.h"
#include "fieldstar.h"
#include "telenv.h"

#include "camera.h"
#include "patchlevel.h"

#define FIND_LINEAR_FEATURE 1   // 1 to enable, 0 to disable streak finding option
#define FIND_SMEAR_FEATURE  1   // 1 to enable, 0 to disable smear finding / solving option

#define NUM_MTB 6

Widget toplevel_w;
#define	XtD	XtDisplay(toplevel_w)
XtAppContext app;
char myclass[] = "Camera";
Colormap camcm;
State state;

typedef struct {
    char *name;		/* widget name */
    char *label;	/* widget label */
    void (*cb)();	/* callback */
    XtPointer call;	/* client data */
    Widget *wp;		/* where to save widget, if desired */
} MenuItem;

typedef struct {
    char *name;
    char *label;
    MenuItem *items;
    int nitems;
    Widget *wp;		/* where to save the pulldown widget, if desired */
} Menu;

#define	DCS	XmSTRING_DEFAULT_CHARSET

static void chk_args (void);
static void onSig (int sig);
static Widget makeMenuBar (Widget par, Menu *menus, int nmenus);
static void makeMain(void);
static void addOptionsMenu(Widget mb);
static void initState(void);

static void openCB (Widget w, XtPointer client, XtPointer call);
static void saveCB (Widget w, XtPointer client, XtPointer call);
static void delCB (Widget w, XtPointer client, XtPointer call);
static void quitCB (Widget w, XtPointer client, XtPointer call);
static void winCB (Widget w, XtPointer client, XtPointer call);
static void basicCB (Widget w, XtPointer client, XtPointer call);
static void glassCB (Widget w, XtPointer client, XtPointer call);
static void zoominCB (Widget w, XtPointer client, XtPointer call);
static void zoomoutCB (Widget w, XtPointer client, XtPointer call);
static void doCropCB (Widget w, XtPointer client, XtPointer call);
static void fitsCB (Widget w, XtPointer client, XtPointer call);
static void measureCB (Widget w, XtPointer client, XtPointer call);
static void gsclimCB (Widget w, XtPointer client, XtPointer call);
static void epochCB (Widget w, XtPointer client, XtPointer call);
static void corrCB (Widget w, XtPointer client, XtPointer call);
static void camSetupCB (Widget w, XtPointer client, XtPointer call);
static void listenCB (Widget w, XtPointer client, XtPointer call);
static void resetAOICB (Widget w, XtPointer client, XtPointer call);
static void roamCB (Widget w, XtPointer client, XtPointer call);
static void privcm(void);

extern void resetCB (Widget w, XtPointer client, XtPointer call);

static Widget msg_w;	/* general purpose message line */
static Widget fn_w;	/* file name label */

static String fallbacks[] = {
    "Camera*Arith*pattern: *.ft[sh]",		/* arith pattern */
    "Camera*AutoReadPhotCalTB.set: True",	/* whether to read photcal.out*/
    "Camera*Basic*Mag1.set: True",		/* set default mag to 1x */
    "Camera*Cam*BX.value: 1",			/* exp binning x */
    "Camera*Cam*BY.value: 1",			/* exp binning y */
    "Camera*Cam*Dur.value: 1",			/* exp duration, seconds */
    "Camera*Cam*SX.value: 0",			/* exp subframe x */
    "Camera*Cam*SY.value: 0",			/* exp subframe y */
    "Camera*Corr*Apply.set: False",		/* auto apply corrections */
    "Camera*Corr*AutoScan.set: True",		/* auto scan for crctn names */
    "Camera*Corr*BiasN.value: 1",		/* default num bias frames */
    "Camera*Corr*FlatD.value: 10",		/* default flat exp time */
    "Camera*Corr*FlatF.value: C",		/* default flat filter code */
    "Camera*Corr*FlatN.value: 1",		/* default num flat frames */
    "Camera*Corr*Sep.topOffset: 5",		/* nice gap above seps */
    "Camera*Corr*ThermD.value: 30",		/* default thermal exp time */
    "Camera*Corr*ThermN.value: 1",		/* default num thermal frames */
    "Camera*Equinox.textString: 2000",		/* default display equinox */ 
    "Camera*FITS*RO.rows:20",			/* number of FITS lines */
    "Camera*FITS*RW.rows:1",			/* number of comment lines */
    "Camera*GSCSetup*MagLimit.value: 20",	/* default GSC limiting mag */
    "Camera*GSCSetup*MatchRadius.value: 0.2",	/* GSC match search rad, degs */
    "Camera*GSCSetup*WantUSNO.set: False",	/* TB whether to use USNO */
    "Camera*Glass*Factor.4x.set: true",		/* default glass mag is 4x */
    "Camera*Glass*Size.32x32.set: true",	/* default glass size 32x32 */
    "Camera*OpenFSB*pattern: *.ft[sh]",		/* FSB search pattern */
    "Camera*OpenFSB.directory: ",		/* default images dir */
    "Camera*Options.Listen.set: False",		/* listen for image names */
    "Camera*Options.ResetAOI.set: True",	/* reset AOI on each open */
    "Camera*Options.RoamReport.set: False",	/* report cursor as roam */
    "Camera*Options.Field.set: False",		/* label field stars */
    "Camera*Photometry*RAperture.value: 1",	/* aperature size, pixels */
    "Camera*Photometry*RSearch.value: 10",	/* max star search radius */
    "Camera*Print*Grayscale.set: True",		/* print gray (aot color) */
    "Camera*Print*PrintCmd.value: lpr",		/* host os print command */
    "Camera*Print*Save.set: True",		/* save to file (aot print) */
    "Camera*SW.spacing: 4",			/* scroll bar needs more space*/
    "Camera*Save*Dir.value: .",			/* default save dir name */
    "Camera*Save*Template.value: ####.fts",	/* default save template */
    "Camera*Win*AutoWin.set: True",		/* use histo to set window */
    "Camera*Win*BWLut.set: True",		/* start with the B/W lut */
    "Camera*Win*Histogram.Rpt.foreground: #080",/* histogram report color */
    "Camera*Win*Histogram.foreground: #f00",	/* histogram color */
    "Camera*Win*Histogram.height: 150",		/* initial height of histo */
    "Camera*Win*LOHIOnly.set: True",		/* lo..hi histo graph */
    "Camera*Win*Wide.set: True",		/* wide contrast */
    "Camera*XmTextField.marginHeight: 2",	/* text margin */
    "Camera*XmTextField.marginWidth: 2",	/* text margin */
    "Camera*XmTextField.shadowThickness: 1",	/* text shadows */
    "Camera*background: lightgrey",		/* overall background */
    "Camera*fontList: -*-lucidatypewriter-bold-r-*-*-12-*-*-*-m-*-*-*",
    "Camera*foreground: navy",			/* overall foreground */
    "Camera*highlightThickness: 0",		/* no highlighting evident */
    "Camera*marginHeight: 1",			/* label gaps */
    "Camera*spacing: 4",			/* toggle button gaps */
    "Camera.AOIColor: #ff0",			/* color of permanent AOI */
    "Camera.FifoName: comm/CameraFilename",	/* path to fifo, with telenv */
    "Camera.GSCColor: #0fc",			/* color of GSC star markers */
    "Camera.GlassBorderColor: #f00",		/* border around mag glass */
    "Camera.GlassGaussColor: #080",		/* glass gaussian color */
    "Camera.GlassGridColor: #888",		/* glass grid color */
    "Camera.PhotomColor: #f66",			/* color of photom stars */
    "Camera.RefImagesFile: archive/config/blink.idx", /* blink reference info */
    "Camera.ScreenEdge: 150",			/* grow no larger thn scr-this*/
    "Camera.StarMarkColor: #f66",		/* color of star markers */
    "Camera.viewsFont: -*-helvetica-medium-r-*-*-14-*-*-*-*-*-*-*",
    "Camera.StreakMarkColor: #0c0",

    NULL
};

#define	NPRECM	30		/* colors to retain when making private cmap */


static XrmOptionDescRec options[] = {
    {"-install", ".install", XrmoptionIsArg, NULL},
    {"-prfb",    ".prfb",    XrmoptionIsArg, NULL},
    {"-help",    ".help",    XrmoptionIsArg, NULL},
    {"-listen",  ".listen",  XrmoptionSepArg, NULL},
};

static Widget listen_w;	/* STO: reference to listener widget checkbox so we can set option */

int 
main (ac, av)
int ac;
char *av[];
{
	char *pname = av[0];
	char title[64];

	(void) sprintf (title, "Camera %s", PATCHLEVEL);

	toplevel_w = XtVaAppInitialize (&app, myclass,
		    options, XtNumber(options), &ac, av, fallbacks,
		    XmNallowShellResize, True,
		    XmNiconName, myclass,
		    XmNtitle, title,
		    NULL);

#ifdef DO_EDITRES
	XtAddEventHandler (toplevel_w, (EventMask)0, True,
					    _XEditResCheckMessages, NULL);
#endif

	camcm = DefaultColormap (XtD, DefaultScreen(XtD));

	/* scan for local options */
	chk_args ();

	/* connect to log file */
	telOELog (pname);

	/* handle some signals */
	signal (SIGFPE, SIG_IGN);
	signal (SIGTERM, onSig);
	signal (SIGINT, onSig);
	signal (SIGQUIT, onSig);
	signal (SIGHUP, onSig);

	makeMain();
	initState();
	atexit (dsStop);

	/* work right to left adding files to history, opening the first */
	while (--ac > 0)
	    if (ac == 1)
		(void) openFile (av[ac]);
	    else
		addHistory (av[ac]);

	XtRealizeWidget(toplevel_w);
	
	/* if we specified this on the cmd line, set it ON now */
	if(getFilenameFifo() != NULL ) {
		if( 0 >= listenFifoSet(1)) {
		    XmToggleButtonSetState (listen_w, True, True);
		}
	}
	
	XtAppMainLoop(app);

	printf ("XtAppMainLoop() returned ?!\n");
	return (1);
}

static void
onSig (int sig)
{
	exit(0);
}
	
/* ARGSUSED */
static void
chk_args ()
{	
	char *p = NULL;
		
	if (getXRes (toplevel_w, "prfb", NULL)) {
	    String *fbp;
	    for (fbp = fallbacks; *fbp != NULL; fbp++)
		printf ("%s\n", *fbp);
	    exit (0);
	}

	if (getXRes (toplevel_w, "help", NULL)) {
	    fprintf (stderr, "camera: [options] [files]\n");
	    fprintf (stderr, "  -help:    this\n");
	    fprintf (stderr, "  -install: install a private colormap\n");
	    fprintf (stderr, "  -prfb:    print fallback resources and exit\n");
	    fprintf (stderr, "  -listen <fifoname>: specify a different name for listener fifo\n");
	    fprintf (stderr, "remaining args are files: first is shown, remaining are put on history list\n");
	    exit (0);
	}

	if (getXRes (toplevel_w, "install", NULL)) {
	    privcm();
	    XtVaSetValues (toplevel_w, XmNcolormap, camcm, NULL);
	}
	
	if ( NULL != (p = getXRes (toplevel_w, "listen", NULL))) {		
		setFilenameFifo(p);
	}		
}


static void
makeMain()
{
	static MenuItem filem[] = {
	    {"Open", "Open ...", openCB, NULL, NULL},
	    {"Print", "Print ...", printCB, NULL, NULL},
	    {"Save", "Save ...", saveCB, NULL, NULL},
	    {"Delete", "Delete ...", delCB, NULL, NULL},
	    {0, 0, 0, 0, NULL},
	    {"Quit", "Quit", quitCB, NULL, NULL},
	};
	static MenuItem viewm[] = {
	    {"Mag",     "Basics: Mag, AOI, Stats ...", basicCB, NULL, NULL},
	    {"Window",  "Contrast and Histogram ...", winCB, NULL, NULL},
	    {"Glass",   "Magnifying Glass, FWHM ...", glassCB,NULL,NULL},
	    {"Measure", "Rubber Band section ...", measureCB, NULL,NULL},
	    {"Arith",   "Image Arithmetic ...", arithCB, NULL, NULL},
	    {"PhotomPB","Photometry ...", photomCB, NULL, NULL},
	    {"Movie",   "Movie loop ...", blinkCB, NULL, NULL},
	    {"Markers", "Markers ...", markersCB, NULL, NULL},
	    {"FITS",    "FITS Header ...", fitsCB, NULL, NULL},
	};
	static MenuItem exposem[] = {
	    {"Setup", "Setup ...", camSetupCB, NULL, NULL},
	    {"Corrections", "Corrections ...", corrCB, NULL, NULL},
	    {0, 0, 0, 0, NULL},
	    {"One", "Take 1", cam1CB, NULL, NULL},
	    {"Continous", "Continuous", camContCB, NULL, NULL},
	    {"Cancal", "Stop", camStopCB, NULL, NULL},
	};
	static Widget filepdm;
	static Menu menus[] = {
	    {"File", "File", filem, XtNumber(filem), &filepdm},
	    {"Controls", "Tools", viewm, XtNumber(viewm)},
	    {"Expose", "Expose", exposem, XtNumber(exposem)}, /* N.B. Last!! */
	};
    
    /* KMI 10/19/03 */
    typedef struct {
        char *name;     /* widget name */
        char *label;    /* label, use name if null */
        XtCallbackProc cb; /* callback */
        Widget *wp;     /* widget, or NULL */
    } Ctrl;
    static Ctrl ctrls[] = {
        {"Zoom out", NULL, zoomoutCB},
        {"Zoom in", NULL, zoominCB},
        {"Glass", NULL, glassCB},
        {"Mark Tgt", NULL, markHdrRADecCB},
        {"Crop", NULL, doCropCB},
        {"Reset AOI", NULL, resetCB},
    };
    /* End KMI 10/19/03 */

	Widget f;
	Widget rc;
	Widget tbrc; /* Toolbar RowColumn - KMI 10/19/03 */
	Widget mb;
	Arg args[20];
	int n, i;

	n = 0;
	f = XmCreateForm (toplevel_w, "MainForm", args, n);
	XtManageChild (f);

	/* create a row column for most stuff */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	rc = XmCreateRowColumn (f, "MainRC", args, n);
	XtManageChild (rc);

	    n = XtNumber(menus);
	    if (camDevFileRW() < 0)
		n--;	/* no Expose */
	    mb = makeMenuBar (rc, menus, n);
	    addOptionsMenu (mb);
	    createHistoryMenu (filepdm);
	    XtManageChild (mb);

        /* KMI 10/19/03 */
        n = 0;
        XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
        XtSetArg (args[n], XmNnumColumns, XtNumber(ctrls)+1); n++;
        XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
        tbrc = XmCreateRowColumn(rc, "ToolbarRC", args, n);
        XtManageChild(tbrc);

        for (i = 0; i < XtNumber(ctrls); i++) {
            Ctrl *cp = &ctrls[i];
            Widget w;

            n = 0;
            w = XmCreatePushButton(tbrc, cp->name, args, n);
            XtAddCallback(w, XmNactivateCallback, cp->cb, NULL);
            XtManageChild(w);

            /* Fill the remaining space. N.B. I'm sure this isn't the 
             * correct way to do this!! */
            if (i+1 == XtNumber(ctrls)) {
                w = XmCreateLabel(tbrc, "", args, n);
                XtManageChild(w);
            }
        }

        /* End KMI 10/19/03 */

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    XtSetArg (args[n], XmNrecomputeSize, False); n++;
	    msg_w = XmCreateLabel (rc, "Msg", args, n);
	    XtManageChild (msg_w);
	    wlprintf (msg_w, "Welcome");

	    n = 0;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	    fn_w = XmCreateLabel (rc, "File", args, n);
	    XtManageChild (fn_w);
	    wlprintf (fn_w, " ");

	/* create the scolledWindow now, but we don't manage it until we
	 * load an image.
	 */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNscrollingPolicy, XmAUTOMATIC); n++;
	state.imageSW = XmCreateScrolledWindow (f, "SW", args, n);
}

/* given an array of Menu items, build and return a MenuBar widget.
 */
static Widget
makeMenuBar (par, menus, nmenus)
Widget par;
Menu menus[];
int nmenus;
{
	Widget mb, cb, pdm;
	Widget w;
	XmString str;
	Menu *mp;
	MenuItem *mip;
	Arg args[20];
	int n;

	n = 0;
	mb = XmCreateMenuBar (par, "MB", args, n);

	for (mp = menus; mp < menus+nmenus; mp++) {
	    n = 0;
	    pdm = XmCreatePulldownMenu (mb, "PDM", args, n);
	    for (mip = mp->items; mip < mp->items + mp->nitems; mip++) {
		if (!mip->name) {
		    n = 0;
		    w = XmCreateSeparator (pdm, "Sep", args, n);
		    XtManageChild (w);
		} else {
		    str = XmStringCreateLtoR (mip->label, DCS);
		    n = 0;
		    XtSetArg (args[n], XmNmarginHeight, 0); n++;
		    XtSetArg (args[n], XmNlabelString, str); n++;
		    w = XmCreatePushButton (pdm, mip->name, args, n);
		    XtManageChild (w);
		    XtAddCallback (w, XmNactivateCallback, mip->cb, mip->call);
		    XmStringFree (str);
		}
		if (mip->wp)
		    *mip->wp = w;
	    }
	    if (mp->wp)
		*(mp->wp) = pdm;
	    str = XmStringCreateLtoR (mp->label, DCS);
	    n = 0;
	    XtSetArg (args[n], XmNsubMenuId, pdm); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    cb = XmCreateCascadeButton (mb, mp->name, args, n);
	    XtManageChild (cb);
	    XmStringFree (str);
	}

	return (mb);
}

/* get the named color in *p.
 * return 0 if the color was found, else -1.
 */
int
get_color_resource (dsp, myclass, cname, p)
Display *dsp;
char *myclass;
char *cname;
Pixel *p;
{
	XColor defxc, dbxc;
	char *cval;

	cval = getXRes (toplevel_w, cname, NULL);

	if (!cval || !XAllocNamedColor (dsp, camcm, cval, &defxc, &dbxc)) {
	    return (-1);
	} else {
	    *p = defxc.pixel;
	    return (0);
	}
}

/* add the Options pulldown we need to have the widgets for.
 */
static void
addOptionsMenu (mb)
Widget mb;	/* menuBar */
{
	Widget pdm;
	Widget w;
	Widget cb;
	Arg args[20];
	XmString str;
	int n;

	/* build the pulldown with the toggle buttons in it */
	n = 0;
	pdm = XmCreatePulldownMenu (mb, "Options", args, n);

	    str = XmStringCreateLtoR ("Auto listen", DCS);
	    n = 0;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateToggleButton (pdm, "Listen", args, n);
	    listen_w = w;
	    XtManageChild (w);
	    XtAddCallback (w, XmNvalueChangedCallback, listenCB, NULL);
	    if (XmToggleButtonGetState (w)) {
		if (listenFifoSet(1) < 0)
		    XmToggleButtonSetState (w, False, False);
	    }
	    XmStringFree (str);

	    str = XmStringCreateLtoR ("Reset AOI on each open", DCS);
	    n = 0;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateToggleButton (pdm, "ResetAOI", args, n);
	    XtManageChild (w);
	    XtAddCallback (w, XmNvalueChangedCallback, resetAOICB, NULL);
	    state.resetaoi = XmToggleButtonGetState (w);
	    XmStringFree (str);

	    str = XmStringCreateLtoR ("Roaming cursor report", DCS);
	    n = 0;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateToggleButton (pdm, "RoamReport", args, n);
	    XtManageChild (w);
	    XtAddCallback (w, XmNvalueChangedCallback, roamCB, NULL);
	    state.roam = XmToggleButtonGetState (w);
	    XmStringFree (str);

	    str = XmStringCreateLtoR ("Label Field stars", DCS);
	    n = 0;
	    XtSetArg (args[n], XmNmarginHeight, 0); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreateToggleButton (pdm, "Field", args, n);
	    XtManageChild (w);
	    XtAddCallback (w, XmNvalueChangedCallback, markGSCCB, NULL);
	    state.showgsc = XmToggleButtonGetState (w);
	    XmStringFree (str);

	    n = 0;
	    w = XmCreateSeparator (pdm, "Sep", args, 1);
	    XtManageChild (w);

	    str = XmStringCreateLtoR ("(Re)Compute WCS fields", DCS);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreatePushButton (pdm, "WCS", args, n);
	    XtManageChild (w);
	    XtAddCallback (w, XmNactivateCallback, setWCSCB, NULL);
	    XmStringFree (str);

	    str = XmStringCreateLtoR ("(Re)Compute FWHM fields", DCS);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreatePushButton (pdm, "FWHM", args, n);
	    XtManageChild (w);
	    XtAddCallback (w, XmNactivateCallback, setFWHMCB, NULL);
	    XmStringFree (str);

	    str = XmStringCreateLtoR ("Blink reference image", DCS);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreatePushButton (pdm, "BlRef", args, n);
	    XtManageChild (w);
	    XtAddCallback (w, XmNactivateCallback, blinkRefCB, NULL);
	    XmStringFree (str);

	    str = XmStringCreateLtoR ("Mark Header RA/DEC once", DCS);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreatePushButton (pdm, "HdrRADec", args, n);
	    XtManageChild (w);
	    XtAddCallback (w, XmNactivateCallback, markHdrRADecCB, NULL);
	    XmStringFree (str);

	    str = XmStringCreateLtoR ("Mark stars once", DCS);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreatePushButton (pdm, "Stars", args, n);
	    XtManageChild (w);
	    XtAddCallback (w, XmNactivateCallback, markStarsCB, NULL);
	    XmStringFree (str);

#if FIND_LINEAR_FEATURE
	    str = XmStringCreateLtoR ("Identify elongated objects", DCS);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreatePushButton (pdm, "Streaks", args, n);
	    XtManageChild (w);
	    XtAddCallback (w, XmNactivateCallback, markStreaksCB, NULL);
	    XmStringFree (str);
#endif

#if FIND_SMEAR_FEATURE
	    str = XmStringCreateLtoR ("Solve untracked image", DCS);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreatePushButton (pdm, "Smears", args, n);
	    XtManageChild (w);
	    XtAddCallback (w, XmNactivateCallback, markSmearsCB, NULL);
	    XmStringFree (str);
#endif

	    n = 0;
	    w = XmCreateSeparator (pdm, "Sep", args, 1);
	    XtManageChild (w);

	    str = XmStringCreateLtoR ("Setup Field stars ...", DCS);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreatePushButton (pdm, "GSCLimPB", args, n);
	    XtManageChild (w);
	    XtAddCallback (w, XmNactivateCallback, gsclimCB, NULL);
	    XmStringFree (str);

	    str = XmStringCreateLtoR ("Setup Epoch ...", DCS);
	    n = 0;
	    XtSetArg (args[n], XmNlabelString, str); n++;
	    w = XmCreatePushButton (pdm, "EpochPB", args, n);
	    XtManageChild (w);
	    XtAddCallback (w, XmNactivateCallback, epochCB, NULL);
	    XmStringFree (str);

	/* connect to a cascade button in the menu bar */
	n = 0;
	XtSetArg (args[n], XmNsubMenuId, pdm); n++;
	cb = XmCreateCascadeButton (mb, "Options", args, n);
	XtManageChild (cb);
}

/* write a new message to the msg area */
void
msg (char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	char *nl;

	va_start (ap, fmt);
	vsprintf (buf, fmt, ap);
	va_end (ap);

	/* 0-length lines cause the label to shrink strangely */
	if (buf[0] == '\0')
	    strcpy (buf, " ");

	/* newlines cause label widget to bunch up */
	nl = strchr (buf, '\n');
	if (nl)
	    *nl = '\0';

	/* just in case we are called very early */
	if (msg_w) {
	    wlprintf (msg_w, "%s", buf);
	    XmUpdateDisplay (msg_w);
	} else
	    fprintf (stderr, "%s\n", buf);
}

/* display a header line for state.fname/fimage */
void
showHeader ()
{
	char hdr[1024];

	mkHeader (hdr);
	wlprintf (fn_w, "%s", hdr);
}

/* create a header lines for state.fname/fimage */
void
mkHeader (char *hdr)
{
	FImage *fip = &state.fimage;
	char *fn = state.fname;
	char buf[1024];
	int d, m, y;
	double tmp;
	double fh, fv;
	int nhdr;

	hdr[nhdr = 0] = '\0';

	if ((int)strlen(basenm(fn)) > 0)
	    nhdr = sprintf (hdr, "%-14s: ", basenm (fn));

	if (!getStringFITS (fip, "OBJECT", buf))
	    nhdr += sprintf (hdr+nhdr, "\"%s\" ", buf);

	if (!getStringFITS (fip, "DATE-OBS", buf) &&
			    (sscanf (buf, "%d/%d/%d", &d, &m, &y) == 3 ||
			    sscanf (buf, "%d-%d-%d", &y, &m, &d) == 3))
	    nhdr += sprintf (hdr+nhdr, "%2d-%s-%d ", d, monthName(m), y);

	if (!getStringFITS (fip, "TIME-OBS", buf))
	    nhdr += sprintf (hdr+nhdr, "%s ", buf);

	if (!getRealFITS (fip, "EXPTIME", &tmp))
	    nhdr += sprintf (hdr+nhdr, "%gS ", tmp);

	if (!getStringFITS (fip, "FILTER", buf))
	    nhdr += sprintf (hdr+nhdr, "%.1s ", buf);

	if (!getStringFITS (fip, "ELEVATION", buf) && !scansex (buf, &tmp)
								&& tmp != 0.0)
	    nhdr += sprintf (hdr+nhdr, "Z=%4.2f ", 1.0/sin(degrad(tmp)));

	if (!getRealFITS(fip,"FWHMH",&fh) && !getRealFITS (fip,"FWHMV",&fv)) {
	    double hs, vs;

	    if (!getRealFITS (fip, "CDELT1", &hs)
				    && !getRealFITS (fip, "CDELT2", &vs)) {
		fh *= fabs(hs)*3600.;
		fv *= fabs(vs)*3600.;
		nhdr += sprintf (hdr+nhdr, "FWHM=%.1fx%.1f\" ", fh, fv);
	    } else {
		nhdr += sprintf (hdr+nhdr, "FWHM=%.1fx%.1f ", fh, fv);
	    }
	}
}

/* debug aid */
void
printState()
{
	printf ("\nState:\n");
	printf ("fname: %s\n", state.fname);
	printf ("fimage.sw: %d\n", state.fimage.sw);
	printf ("fimage.sh: %d\n", state.fimage.sh);
	printf ("ximagep: %d\n", (unsigned)state.ximagep);
	printf ("stats.mean: %d\n", state.stats.mean);
	printf ("stats.min: %d\n", state.stats.min);
	printf ("stats.max: %d\n", state.stats.max);
	printf ("stats.sd: %g\n", state.stats.sd);
	printf ("aoi.x: %d\n", state.aoi.x);
	printf ("aoi.y: %d\n", state.aoi.y);
	printf ("aoi.w: %d\n", state.aoi.w);
	printf ("aoi.h: %d\n", state.aoi.h);
	printf ("state.mag: %d\n", state.mag);
	printf ("state.hi: %d\n", state.hi);
	printf ("state.lo: %d\n", state.lo);
	printf ("state.crop: %d\n", state.crop);
	printf ("state.aoistats: %d\n", state.aoistats);
	printf ("state.inverse: %d\n", state.inverse);
	printf ("state.lrflip: %d\n", state.lrflip);
	printf ("state.tbflip: %d\n", state.tbflip);
}

static void
initState()
{
	initDepth();		/* establishes bits per pixel for images */

	createOpen();		/* create open FSB */
	createSave();		/* default save file name */

	state.mag = MAGDENOM;	/* default mag is 1 but ... */
	createBasic();		/* set state.mag, crop */

	createGSCLimit();	/* sets gsclimit */
	createEpoch();		/* sets display epoch */

	createCorr();		/* sets state.{bias,dark,flat} */
	createWin();		/* sets state.{inverse,aoistats,xpxls,luttype}*/
	createGlass();		/* set mag glass sizes */
	if (!camDevFileRW())
	    camCreate();	/* creates the camera dialog */
}

/* ARGSUSED */
static void openCB (Widget w, XtPointer client, XtPointer call)
{
	msg("");
	manageOpen();
}

/* ARGSUSED */
static void saveCB (Widget w, XtPointer client, XtPointer call)
{
	msg("");
	manageSave();
}

/* ARGSUSED */
static void delCB (Widget w, XtPointer client, XtPointer call)
{
	msg("");
	manageDel();
}

/* called from the Quit button on the File pulldown with client == 0.
 * called from the "Are you sure" question dialog with client == 1.
 */
/* ARGSUSED */
static void quitCB (Widget w, XtPointer client, XtPointer call)
{
	static Widget rusure;

	if (!rusure) {
	    Arg args[20];
	    int n;

	    n = 0;
	    rusure = XmCreateQuestionDialog (toplevel_w, "RUSure", args, n);
	    set_xmstring (rusure, XmNokLabelString, "Yes -- quit");
	    set_xmstring (rusure, XmNcancelLabelString, "No -- resume");
	    set_xmstring (rusure, XmNmessageString, "Really quit Camera?");
	    set_xmstring (rusure, XmNdialogTitle, "Camera Quit");
	    XtUnmanageChild(XmMessageBoxGetChild(rusure, XmDIALOG_HELP_BUTTON));
	    XtAddCallback (rusure, XmNokCallback, quitCB, (XtPointer)1);
	}

	if ((int)client == 1)
	    exit (0);
	else
	    XtManageChild (rusure);
}


/* ARGSUSED */
static void winCB (Widget w, XtPointer client, XtPointer call)
{
	manageWin();
}


/* ARGSUSED */
static void basicCB (Widget w, XtPointer client, XtPointer call)
{
	manageBasic();
}


/* ARGSUSED */
static void glassCB (Widget w, XtPointer client, XtPointer call)
{
	manageGlass();
}

/* KMI 10/19/03 */
static void zoominCB (Widget w, XtPointer client, XtPointer call)
{

    extern MTB mtb[];
    int i;
    int selected = 0;

    for (i = 0; i < NUM_MTB; i++) {
        if (XmToggleButtonGetState(mtb[i].w)) {
            selected = i;
        }
    }

    if (selected < NUM_MTB-1) {
        selected = selected+1;
    }
    
    XmToggleButtonSetState (mtb[selected].w, True, True);
    newXImage();
}

/* KMI 10/19/03 */
static void zoomoutCB (Widget w, XtPointer client, XtPointer call)
{

    extern MTB mtb[];
    int i;
    int selected = 0;

    for (i = 0; i < NUM_MTB; i++) {
        if (XmToggleButtonGetState(mtb[i].w)) {
            selected = i;
        }
    }

    if (selected > 0) {
        selected = selected-1;
    }
    
    XmToggleButtonSetState (mtb[selected].w, True, True);
    newXImage();
}

/* KMI 10/20/03 */
static void doCropCB (Widget w, XtPointer client, XtPointer call)
{
    extern Widget crop_w;

    XmToggleButtonSetState (crop_w, True, True);
}

/* ARGSUSED */
static void fitsCB (Widget w, XtPointer client, XtPointer call)
{
	manageFITS();
}

/* ARGSUSED */
static void measureCB (Widget w, XtPointer client, XtPointer call)
{
	manageMeasure();
}

/* ARGSUSED */
static void gsclimCB (Widget w, XtPointer client, XtPointer call)
{
	manageGSCLimit();
}

/* ARGSUSED */
static void epochCB (Widget w, XtPointer client, XtPointer call)
{
	manageEpoch();
}


/* ARGSUSED */
static void corrCB (Widget w, XtPointer client, XtPointer call)
{
	manageCorr();
}

/* ARGSUSED */
static void camSetupCB (Widget w, XtPointer client, XtPointer call)
{
	msg("");
	camManage();
}


/* ARGSUSED */
void cam1CB (Widget w, XtPointer client, XtPointer call)
{
	msg("");
	camTake1();
}

/* ARGSUSED */
void camContCB (Widget w, XtPointer client, XtPointer call)
{
	msg("");
	camLoop();
}

/* ARGSUSED */
void camStopCB (Widget w, XtPointer client, XtPointer call)
{
	msg("");
	camCancel();
#if JSF_VERSION
	system("pvmstr -C");
#endif
		
}

/* ARGSUSED */
static void listenCB (Widget w, XtPointer client, XtPointer call)
{
	if (listenFifoSet (XmToggleButtonGetState (w)) < 0)
	    XmToggleButtonSetState (w, False, False);
}

/* ARGSUSED */
static void resetAOICB (Widget w, XtPointer client, XtPointer call)
{
	msg("");
	state.resetaoi = XmToggleButtonGetState (w);
}

/* ARGSUSED */
static void roamCB (Widget w, XtPointer client, XtPointer call)
{
	msg("");
	state.roam = XmToggleButtonGetState (w);
}


/* build a private colormap for toplevel_w and save in camcm */
static void
privcm()
{
	XColor preload[NPRECM];
	Display *dsp = XtDisplay (toplevel_w);
	Window win = RootWindow (dsp, DefaultScreen(dsp));
	Colormap defcm = DefaultColormap (dsp, DefaultScreen(dsp));
	Visual *v = DefaultVisual (dsp, DefaultScreen(dsp));
	int i;

	/* get some existing colors */
	for (i = 0; i < NPRECM; i++)
	    preload[i].pixel = (unsigned long) i;
	XQueryColors (dsp, defcm, preload, NPRECM);

	/* make a new colormap, and preload with some existing colors */
	camcm = XCreateColormap (dsp, win, v, AllocNone);
	for (i = 0; i < NPRECM; i++)
	    (void) XAllocColor (dsp, camcm, &preload[i]);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: main.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.8 $ $Name:  $"};
