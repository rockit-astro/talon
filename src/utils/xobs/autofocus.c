/* this dialog allows the operator to automatically focus.
 * the idea is to maximum the overall SNR of the image.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <Xm/Label.h>
#include <X11/keysym.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/TextF.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telstatshm.h"
#include "ccdcamera.h"
#include "xtools.h"
#include "configfile.h"
#include "cliserv.h"
#include "telenv.h"
#include "fits.h"
#include "telfits.h"
#include "strops.h"

#include "fitscorr.h"
#include "focustemp.h"

#include "xobs.h"

#define TRACE_ON 0
#if TRACE_ON
#define TRACE fprintf(stderr,
#else
void TRACE_EAT(char * fmt, ...)
{
	va_list ap;
	va_start (ap, fmt);
	va_end (ap);
}
#define TRACE TRACE_EAT(
#endif


// STO: define this as 1 if you want to take out some of the real-world
// sanity checks that make testing the code harder
#define	IGNORE_SANITY_CHECK	0

#define	AFPOLL_PERIOD	567	/* auto focus polling period, ms */
#define	MAXHIST		10	/* last images for which we display stats */
#define	TFW		10	/* field width, chars */
#define	GAP		32	/* gap around image when finding stats */
#define	BORD		20	/* border around graph */
#define	BOXSZ		5	/* size of "points" in graph */
#define	NDIV		5	/* number of divisions in grid */
#define	MINDT		5	/* min temp between 2 entries, C */

#define BADFWHM     9999.0

static int afoc_ison(void);
static void mkAfGUI(void);
static void makeGap (Widget p_w);
static void makePrompt (Widget p_w, char *prompt, Widget *tfp,
    XtCallbackProc cb);
static void makeLabelPair (Widget p_w, Widget *pp, Widget *lp);
static void showStats(void);
static void showGraph(void);
static void pollCB (XtPointer client, XtIntervalId *id);
static void goCB (Widget w, XtPointer client, XtPointer call);
static void stopCB (Widget w, XtPointer client, XtPointer call);
static void installCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);
static void sdaCB (Widget w, XtPointer client, XtPointer call);
static void unmapCB (Widget w, XtPointer client, XtPointer call);
static void doStop(void);
static int startExpose (void);
static void camCB (void);
static int pos_qf (const void *p1, const void *p2);
static int nextMove (FImage *fip, double *movep);
static double findNew(void);
static double findBest(void); // STO 3/15/07
static int getExpTime(CCDExpoParams *cp);
static int getBinning (CCDExpoParams *cp);
static void nextName (char *fn);
static double focusTemp(void);
static void finishFITS (char fn[], int fd, FImage *fip);

static Widget top_w;		/* main form shell */
static Widget cur_w;		/* current position Label */
static Widget temp_w;		/* current temperature Label */
static Widget filt_w;		/* current filter Label */
static Widget dur_w;		/* duration TF, secs */
static Widget bin_w;		/* binning TF, pixels */
static Widget tol_w;		/* tolerance, TF */
static Widget msg_w;		/* messages label */
static Widget sda_w;		/* stats drawing area */
static Pixel graph_p;		/* stats pixel */
static Pixel grid_p;		/* grid pixel */
static Pixel label_p;		/* label pixel */
static GC graph_gc;		/* GC to use for the graph */

/* overall notion of current activity */
typedef enum {
    FS_IDLE = 0,		/* nothing going on */
    FS_WORKING,			/* auto-focusing procedure in progress */
} FocusState;
static FocusState fstate;

/* images taken so far and what we know of them.
 */
typedef struct {
    double pos;			/* focus position, microns */
    double sd;			/* stddev */
    double fwhm;        /* combined fwhm, or 0 if not available (STO 3/15/07) */
} ImgInfo;
static ImgInfo imghist[MAXHIST];/* what we know of the total set */
static Widget pos_w[MAXHIST];	/* Label for positions */
static Widget sd_w[MAXHIST];	/* Label for std devs */
static int nimg;		/* entries in use */

/* from the config files */
static char fcfn[] = "archive/config/focus.cfg";
static double OFIRSTSTEP;
static double OSTOPSTEP;
static double OEXPTIM;
static double OMINSTD;
static int OTRACK;

/* STO: my additions for controlling autofocus corrections */
static int OFIXBADCOL; 	// 1 to apply bad column correction, 0 or not here = no bad column fix
static int OUSECORIMGS;	// 1 for bias/thermal/flat corrections, 0 or not here = no corrections
static char OBADCOLMAP[1024];
static char OBIASIMG[1024];
static char OTHERMIMG[1024];
static char OFLATIMG[1024];

static char tcfn[] = "archive/config/telsched.cfg";
static int DEFBIN;

static char ccfn[] = "archive/config/camera.cfg";
static double HPIXSZ, VPIXSZ;
/* what kind of driver */
static char driver[1024];
static int auxcam;

// STO
int autofocus_done = 0;

/* polling id */
static XtIntervalId pollId;

/* called to toggle the auto focus dialog */
void
afoc_manage ()
{
	if (!OMOT->have) {
	    msg ("No focus motor configured.");
	    return;
	}

	if (!top_w) {
	    mkAfGUI();
	    afoc_initCfg();
	}

	if (XtIsManaged(top_w)) {
	    XtUnmanageChild (top_w);
	} else {
	    /* up we go */

#ifdef FAKE_RUN
    nimg = 5;
    imghist[1].pos = 300; imghist[1].sd = 29.7;  fwhm = 3.5;
    imghist[2].pos = 400; imghist[2].sd = 12.6;  fwhm = BADFWHM;
    imghist[0].pos = 200; imghist[0].sd = 10.5;  fwhm = BADFWHM;
    imghist[3].pos = 350; imghist[3].sd = 24.4;  fwhm = 6.7;
    imghist[4].pos = 325; imghist[4].sd = 33.3;  fwhm = 2.8;
    qsort ((void *)imghist, nimg, sizeof(ImgInfo), pos_qf);
    printf ("%g\n", findNew());
#endif

	    showStats();
	    XtManageChild (top_w);

	    /* start monitor */
	    pollId = XtAppAddTimeOut (app, AFPOLL_PERIOD, pollCB, 0);
	}
}

/* respond to info from the focus response */
void
afoc_foc_cb (int code, char msg[])
{
	if (!afoc_ison())
	    return;

	if (code < 0) {
	    wlprintf (msg_w, "Stopping: %s", msg);
	    fstate = FS_IDLE;
	} else if (startExpose () < 0)
	    fstate = FS_IDLE;
}

/* called when camerad says something */
void
afoc_cam_cb (int code, char msg[])
{
	/* ignore if not interested */
	if (!afoc_ison())
	    return;
                
	/* looking for 0. >0 is FYI, <0 is trouble */
	if (code == 0)
	    camCB();
	else {
	    if (code < 0)
		fstate = FS_IDLE;
	    wlprintf (msg_w, "%s", msg);
	}

}

/* (re)read the config files */
void
afoc_initCfg()
{
#define	NFCFG	(sizeof(fcfg)/sizeof(fcfg[0]))
#define	NFCFG2	(sizeof(fcfg2)/sizeof(fcfg2[0]))
#define	NCCFG	(sizeof(ccfg)/sizeof(ccfg[0]))
#define	NOCCFG	(sizeof(occfg)/sizeof(occfg[0]))
	static CfgEntry fcfg[] = {
	    {"OFIRSTSTEP", CFG_DBL, &OFIRSTSTEP},
	    {"OSTOPSTEP",  CFG_DBL, &OSTOPSTEP},
	    {"OEXPTIM",    CFG_DBL, &OEXPTIM},
	    {"OMINSTD",    CFG_DBL, &OMINSTD},
	    {"OTRACK",     CFG_INT, &OTRACK},
	};
	// sto: the new, optional stuff
	static CfgEntry fcfg2[] = {
	    {"OFIXBADCOL", CFG_INT, &OFIXBADCOL},
	    {"OUSECORIMGS", CFG_INT, &OUSECORIMGS},
	    {"OBADCOLMAP", CFG_STR, OBADCOLMAP, sizeof(OBADCOLMAP)},
	    {"OBIASIMG", CFG_STR, OBIASIMG, sizeof(OBIASIMG)},
	    {"OTHERMIMG", CFG_STR, OTHERMIMG, sizeof(OTHERMIMG)},
	    {"OFLATIMG", CFG_STR, OFLATIMG, sizeof(OFLATIMG)},
	};
	// ---
	static CfgEntry ccfg[] = {
	    {"HPIXSZ",     CFG_DBL, &HPIXSZ},
	    {"VPIXSZ",     CFG_DBL, &VPIXSZ},
	};
	static CfgEntry occfg[] = {
	    {"DRIVER",     CFG_STR,	driver, sizeof(driver)},
	    {"AUXCAM",	   CFG_INT,	&auxcam},
	};
	int n;
	
	// init optional stuff before reading
	OFIXBADCOL = 0;
	OUSECORIMGS = 0;
		
	/* read in everything */
	n = readCfgFile (1, fcfn, fcfg, NFCFG);
	if (n != NFCFG) {
	    cfgFileError (fcfn, n, NULL, fcfg, NFCFG);
	    die();
	}
	// (sto:) opt correction values
	n = readCfgFile (1, fcfn, fcfg2, NFCFG2);
	
	//--
	n = readCfgFile (1, ccfn, ccfg, NCCFG);
	if (n != NCCFG) {
	    cfgFileError (ccfn, n, NULL, ccfg, NCCFG);
	    die();
	}

	//-- optional values from camera.cfg
	n = readCfgFile (1, ccfn, occfg, NOCCFG);
	
	if(OUSECORIMGS) {
		// STO 2002-10-07: Read the correction config values
		readCorrectionCfg(1, ccfn);
	}
	
	if (read1CfgEntry (1, tcfn, "DEFBIN", CFG_INT, &DEFBIN, 0) < 0) {
	    printf ("%s: %s\n", tcfn, "DEFBIN");
	    die();
	}
	
	if (top_w) {
	    wtprintf (dur_w, "%*.2f", TFW, OEXPTIM);
	    wtprintf (tol_w, "%*.1f", TFW, OSTOPSTEP);
	    wtprintf (bin_w, "%*d",   TFW, DEFBIN);
	}
	
}

/* return 1 if we are up and running, else 0 */
static int
afoc_ison()
{
	return (top_w && XtIsManaged(top_w) && fstate == FS_WORKING);
}

/* build the user interface */
static void
mkAfGUI()
{
	Widget install_w;
	Widget stop_w;
	Widget go_w;
	Widget rc_w;
	Widget sf_w;
	Widget src_w;
	Widget w, w2;
	char buf[64];
	Arg args[20];
	int i;
	int n;

	n = 0;
	XtSetArg (args[n], XmNhorizontalSpacing, 10); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_GROW); n++;
	XtSetArg (args[n], XmNfractionBase, 13); n++;
	XtSetArg (args[n], XmNtitle, "Auto Focus"); n++;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	top_w = XmCreateFormDialog (toplevel_w, "F", args, n);
	XtAddCallback (top_w, XmNunmapCallback, unmapCB, 0);

	/* top stuff in a RC */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNisAligned, True); n++;
	XtSetArg (args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	rc_w = XmCreateRowColumn (top_w, "RC", args, n);
	XtManageChild (rc_w);

	n = 0;
	msg_w = XmCreateLabel (rc_w, "Msg", args, n);
	wlprintf (msg_w, "Press \"Start\" to begin auto focus");
	XtManageChild (msg_w);

	makeGap (rc_w);

	makeLabelPair (rc_w, &w, &cur_w);
	wlprintf (w, "Current position, %cm:", XK_mu);
	makeLabelPair (rc_w, &w, &temp_w);
	wlprintf (w, "Temperature, %cC:", XK_degree);
	makeLabelPair (rc_w, &w, &filt_w);
	wlprintf (w, "Filter:");

	makePrompt (rc_w, "Exposure time, secs:", &dur_w, NULL);
	wtip (dur_w, "Duration of each exposure during autofocusing");
	makePrompt (rc_w, "Binning, pixels:", &bin_w, NULL);
	wtip (bin_w, "Camera pixel binning, same in each dimenion");
	sprintf (buf, "Depth of field, %cm:", XK_mu);
	makePrompt (rc_w, buf, &tol_w, NULL);
	wtip(tol_w,"Autofocusing will stop when last change is less than this");

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (w);

	/* make a form to split stats table on left and graph on right */

	sf_w = XmCreateForm (rc_w, "SF", args, n);
	XtManageChild (sf_w);

	    /* make a rc for the left-half table */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 48); n++;
	    src_w = XmCreateRowColumn (sf_w, "SRC", args, n);
	    XtManageChild (src_w);

	    makeLabelPair (src_w, &w, &w2);
	    wlprintf (w, "Position");
	    wlprintf (w2, "%*s", TFW, "SD (FWHM)");
	    for (i = 0; i < MAXHIST; i++)
		makeLabelPair (src_w, &pos_w[i], &sd_w[i]);

	    /* make a graphing area on the right half */

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 52); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    sda_w = XmCreateDrawingArea (sf_w, "SRC", args, n);
	    wtip (sda_w, "Plot of Std Dev vs. focus position, in microns");
	    XtAddCallback (sda_w, XmNexposeCallback, sdaCB, NULL);
	    XtManageChild (sda_w);

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	XtManageChild (w);

	/* put the close and other buttons across the bottom */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 1); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 3); n++;
	go_w = XmCreatePushButton (top_w, "Start", args, n);
	wtip (go_w, "Start the autofocus procedure");
	XtAddCallback (go_w, XmNactivateCallback, goCB, 0);
	XtManageChild (go_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 4); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 6); n++;
	stop_w = XmCreatePushButton (top_w, "Stop", args, n);
	wtip (stop_w, "Stop the autofocus procedure");
	XtAddCallback (stop_w, XmNactivateCallback, stopCB, 0);
	XtManageChild (stop_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 7); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 9); n++;
	install_w = XmCreatePushButton (top_w, "Install", args, n);
	wtip (install_w, "Move to and save this result in filter.cfg");
	XtAddCallback (install_w, XmNactivateCallback, installCB, 0);
	XtManageChild (install_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 10); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 12); n++;
	w = XmCreatePushButton (top_w, "Close", args, n);
	XtAddCallback (w, XmNactivateCallback, closeCB, 0);
	wtip(w, "Stop Autofocus and Close this dialog");
	XtManageChild (w);

	/* get the pixels for the graph */
	graph_p = getColor (toplevel_w, "#f00");
	grid_p = getColor (toplevel_w, "#222");
	label_p = getColor (toplevel_w, "#292");
}

/* make a separator as a gap */
static void
makeGap (Widget p_w)
{
	Widget w = XmCreateSeparator (p_w, "Gap", NULL, 0);
	XtVaSetValues (w,
	    XmNseparatorType, XmNO_LINE,
	    XmNheight, 10,
	    NULL);
	XtManageChild (w);
}

/* a label and textfield side-by-side as prompt pair to the given RC.
 */
static void
makePrompt (Widget p_w, char *prompt, Widget *tfp, XtCallbackProc cb)
{
	Widget f_w, tf_w, l_w;
	Arg args[20];
	int n;

	n = 0;
	f_w = XmCreateForm (p_w, "PF", args, n);
	XtManageChild (f_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 50); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	l_w = XmCreateLabel (f_w, "PL", args, n);
	XtManageChild (l_w);
	wlprintf (l_w, "%s", prompt);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, l_w); n++;
	XtSetArg (args[n], XmNleftOffset, 10); n++;
	XtSetArg (args[n], XmNcolumns, TFW); n++;
	XtSetArg (args[n], XmNmarginHeight, 2); n++;
	tf_w = XmCreateTextField (f_w, "PTF", args, n);
	if (cb)
	    XtAddCallback (tf_w, XmNactivateCallback, cb, NULL);
	XtManageChild (tf_w);
	*tfp = tf_w;
}

/* make a pair of labels off the given RC
 */
static void
makeLabelPair (Widget par_w, Widget *pp, Widget *lp)
{
	Widget f_w, p_w, l_w;
	Arg args[20];
	int n;

	n = 0;
	f_w = XmCreateForm (par_w, "LP", args, n);
	XtManageChild (f_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 50); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	p_w = XmCreateLabel (f_w, "PP", args, n);
	XtManageChild (p_w);
	*pp = p_w;

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, p_w); n++;
	XtSetArg (args[n], XmNleftOffset, 10); n++;
	l_w = XmCreateLabel (f_w, "PL", args, n);
	XtManageChild (l_w);
	*lp = l_w;
}

/* display the history of positions and their quality.
 * N.B. we assume the array is already sorted by increasing position.
 */
static void
showStats()
{
	int i;

	for (i = 0; i < MAXHIST; i++) {
	    ImgInfo *ip = &imghist[i];

	    if (i < nimg) {
		wlprintf (pos_w[i], "%*.1f", TFW, ip->pos);
		wlprintf (sd_w[i], "%*.1f (%.1f)", TFW-5, ip->sd, ip->fwhm == BADFWHM ? 0 : ip->fwhm);
	    } else {
		wlprintf (pos_w[i], " ");
		wlprintf (sd_w[i], " ");
	    }
	}
}

/* draw the stats so far as a graph.
 * N.B. we assume the array is already sorted by increasing position.
 */
static void
showGraph()
{
	Display *dsp = XtDisplay (sda_w);
	Window win = XtWindow (sda_w);
	double hticks[NDIV+2], vticks[NDIV+2];
	int nhticks, nvticks;
	Dimension wid, hei;
	double minpos, maxpos;
	double minsd, maxsd;
	int lastx, lasty;
	char buf[64];
	int x, y;
	int i;

	/* beware being called before up */
	if (!win)
	    return;

	/* insure we have our GC */
	if (!graph_gc)
	    graph_gc = XCreateGC (dsp, win, 0L, NULL);

	/* erase */
	XClearWindow (dsp, win);

	/* find size of graph, allow for border */
	XtVaGetValues (sda_w,
	    XmNwidth, &wid,
	    XmNheight, &hei,
	    NULL);
	wid -= 2*BORD;
	hei -= 2*BORD;

	/* just draw a blank square until we have 2 points */
	if (nimg < 2) {
	    XSetForeground (dsp, graph_gc, grid_p);
	    XDrawRectangle (dsp, win, graph_gc, BORD, BORD, wid, hei);
	    return;
	}

	/* find extremes to scale graph */
	minpos = maxpos = imghist[0].pos;
	minsd = maxsd = imghist[0].sd;
	for (i = 1; i < nimg; i++) {
	    ImgInfo *ip = &imghist[i];
	    if (ip->pos < minpos) minpos = ip->pos;
	    if (ip->pos > maxpos) maxpos = ip->pos;
	    if (ip->sd < minsd) minsd = ip->sd;
	    if (ip->sd > maxsd) maxsd = ip->sd;
	}

	/* find tickmarks to use for grid, then readjust min/max */
	nhticks = tickmarks (minpos, maxpos, NDIV, hticks);
	nvticks = tickmarks (minsd, maxsd, NDIV, vticks);
	minpos = hticks[0];
	maxpos = hticks[nhticks-1];
	minsd = vticks[0];
	maxsd = vticks[nvticks-1];

	/* draw background grid */
	XSetForeground (dsp, graph_gc, grid_p);
	for (i = 0; i < nhticks; i++) {
	    x = BORD + wid*(hticks[i] - minpos)/(maxpos - minpos);
	    XDrawLine (dsp, win, graph_gc, x, BORD, x, BORD+hei);
	}
	for (i = 0; i < nvticks; i++) {
	    y = BORD + hei-hei*(vticks[i] - minsd)/(maxsd - minsd);
	    XDrawLine (dsp, win, graph_gc, BORD, y, BORD+wid, y);
	}

	/* label a few lines */
	XSetForeground (dsp, graph_gc, label_p);
	x = BORD/2;
	y = 2*BORD + hei;
	sprintf (buf, "%g", minpos);
	XDrawString (dsp, win, graph_gc, x, y, buf, strlen(buf));
	sprintf (buf, "%g", maxpos);
	x = BORD + wid - BORD/2;
	XDrawString (dsp, win, graph_gc, x, y, buf, strlen(buf));
	x = 0;
	y = BORD;
	sprintf (buf, "%g", maxsd);
	XDrawString (dsp, win, graph_gc, x, y, buf, strlen(buf));
	y = BORD + hei;
	sprintf (buf, "%g", minsd);
	XDrawString (dsp, win, graph_gc, x, y, buf, strlen(buf));

	/* connect the dots */
	XSetForeground (dsp, graph_gc, graph_p);
	lastx = lasty = 0;
	for (i = 0; i < nimg; i++) {
	    ImgInfo *ip = &imghist[i];
	    int x = BORD + wid*(ip->pos - minpos)/(maxpos - minpos);
	    int y = BORD + hei-hei*(ip->sd - minsd)/(maxsd - minsd);

	    XDrawRectangle (dsp, win, graph_gc,x-BOXSZ/2,y-BOXSZ/2,BOXSZ,BOXSZ);
	    if (i > 0)
		XDrawLine (dsp, win, graph_gc, x, y, lastx, lasty);

	    lastx = x;
	    lasty = y;
	}
}

static void
sdaCB (Widget w, XtPointer client, XtPointer call)
{
	showGraph();
}

/* periodic callback */
static void
pollCB (XtPointer client, XtIntervalId *id)
{
	MotorInfo *mip = OMOT;
	double pos;
	int i;

	/* process pending events if may have been waiting for camera */
	XCheck(app);

	/* report current focus position, air temp and filter */
	pos = mip->cpos*mip->step/(2*PI*mip->focscale);
	wlprintf (cur_w, "%*.1f", TFW, pos);

	if (time(NULL) - telstatshmp->wxs.updtime < 30)
	    wlprintf (temp_w, "%*.1f", TFW, telstatshmp->now.n_temp);
	else
	    wlprintf (temp_w, "   (%*.1f)", TFW-5, telstatshmp->now.n_temp);

	if (IMOT->have) {
	    for (i = 0; i < nfilt; i++)
		if (filtinfo[i].name[0] == telstatshmp->filter)
		    break;
	    wlprintf (filt_w, "%*s", TFW, i < nfilt ? filtinfo[i].name : "???");
	} else
	    wlprintf (filt_w, "%*s", TFW, filtinfo[deffilt].name);

	/* repeat */
	pollId = XtAppAddTimeOut (app, AFPOLL_PERIOD, pollCB, 0);
}

/* called from the Start PB */
static void
goCB (Widget w, XtPointer client, XtPointer call)
{
	/* sanity-check initial conditions */
	if (fstate != FS_IDLE) {
	    wlprintf (msg_w, "Stop first");
	    return;
	}
	
#if !(IGNORE_SANITY_CHECK)	
	if (OTRACK && telstatshmp->telstate != TS_TRACKING) {
	    wlprintf (msg_w, "Telescope must be tracking");
	    return;
	}
#endif

	/* start a new round of images */
    autofocus_done = 0;
	nimg = 0;
	showStats();
	showGraph();
	if (startExpose() == 0) {
	    fstate = FS_WORKING;
	    wlprintf (msg_w, "Starting auto focus");
	}
}

/* called from the Stop PB */
static void
stopCB (Widget w, XtPointer client, XtPointer call)
{
	doStop();
}


/* called from the Install PB */
static void
installCB (Widget w, XtPointer client, XtPointer call)
{
	MotorInfo *mip = OMOT;
	char buf[1024];
	FilterInfo *fip;
	double t;
	double p;
	int maxi;
	int i;

	/* must be idle */
	if (fstate != FS_IDLE) {
	    wlprintf (msg_w, "Can not install while searching");
	    return;
	}

	/* must have at least 1 point */
	if (nimg < 1) {
	    wlprintf (msg_w, "No data taken yet");
	    return;
	}

	/* find the position with the max sd */
	maxi = 0;
	for (i = 1; i < nimg; i++)
	    if (imghist[i].fwhm < imghist[maxi].fwhm)
		maxi = i;
	p = imghist[maxi].pos;

	/* find filtinfo for current filter */
	if (IMOT->have) {
	    for (i = 0; i < nfilt; i++)
		if (filtinfo[i].name[0] == telstatshmp->filter)
		    break;
	} else
	    i = deffilt;
	if (i >= nfilt) {
	    wlprintf (msg_w, "Bogus filter!! %c", telstatshmp->filter);
	    return;
	}
	fip = &filtinfo[i];

	/* get temperature */
	t = focusTemp();

	/* confirm */
	sprintf (buf, "Save and move to %.1fum @ %.1fC for filter %s",
							    p, t, fip->name);
	if (!rusure (toplevel_w, buf))
	    return;

	focusPositionReadData();

	/* ok, save in filter file */
	if (fip->dflt0 || fabs(fip->t0 - t) < MINDT) {
	    fip->f0 = p;
	    fip->t0 = t;
	    fip->dflt0 = 0;
	} else {
	    fip->f1 = p;
	    fip->t1 = t;
	    fip->dflt1 = 0;
	}
	if (writeFilterCfg (icfn, filtinfo, nfilt, fip-filtinfo, buf) < 0) {
	    wlprintf (msg_w, "%s: %s", basenm(icfn), buf);
	    return;
	}

	focusPositionAdd(fip->name[0],t,p);
	
	if(focusPositionWriteData() >=0) {
		wlprintf (msg_w, "New focus installed");
		msg ("New focus installed");
	} else {
		wlprintf (msg_w, "Error installing new focus");
		msg ("Error installing new focus");
	}		
	/* inform the Filter fifo to reread the config file */
	fifoMsg (Filter_Id, "%s", "Reset");

	/* move to new position */
	msg ("Moving filter to %.1fum", p);
	fifoMsg (Focus_Id, "%g", p - mip->cpos*mip->step/(2*PI*mip->focscale));
}

static void
closeCB (Widget w, XtPointer client, XtPointer call)
{
	XtUnmanageChild (top_w);
}

static void
unmapCB (Widget w, XtPointer client, XtPointer call)
{
	doStop();

	if (pollId) {
	    XtRemoveTimeOut (pollId);
	    pollId = (XtIntervalId) 0;
	}
}

static void
doStop()
{
	switch (fstate) {
	case FS_IDLE:
	    break;

	case FS_WORKING:
	    fifoMsg (Cam_Id, "Stop");
	    fifoMsg (Focus_Id, "Stop");
	    wlprintf (msg_w, "Focus procedure stopped");
	    fstate = FS_IDLE;
	    break;
	}
}

/* start a full-frame exposure using camerad.
 * afoc_cam_cb calls us when camerad talks to us.
 * return 0 if all ok else use msg_w and always reset everything and return -1.
 */
static int
startExpose ()
{
	MotorInfo *mip = OMOT;
	CCDExpoParams cep;
	char buf[1024];
	char fn[128];
	char obj[64];
	double pos;
	double dur;
	
	if (setPathCCD (driver, auxcam, buf) < 0)
	{
	    wlprintf (msg_w, "Can't setup camera: %s", buf);
	    return (-1);
	}
	
	/* determine the exposure params */
	if (getSizeCCD (&cep, buf) < 0) {
	    wlprintf (msg_w, "No cam info: %s", buf);
	    return (-1);
	}
	if (getBinning(&cep) < 0)
	    return (-1);
	if (getExpTime(&cep) < 0)
	    return (-1);
	cep.sx = cep.sy = 0;
	cep.shutter = 1;
	
	/* tell camerad to start the exposure */
	nextName (fn);
	pos = mip->step*mip->cpos/mip->focscale/(2*PI);
	sprintf (obj, "Focus@%.1f", pos);
	dur = cep.duration / 1000.0;
	if (fifoMsg (Cam_Id,
		    "Expose %d+%dx%dx%d %dx%d %g %d %d %s\n%s\n%s\n%s\n%s\n",
			0, 0, cep.sw, cep.sh, cep.bx, cep.by, dur, 1, 100, fn,
			obj,
			"Auto Focus via xobs",
			"Auto Focus",
			"Operator") < 0) {
	    wlprintf (msg_w, "Can not talk to camerad");
	    return (-1);
	}

	/* ok! */
	return (0);
}

/* called when camerad says the exposure is complete */
/* ARGSUSED */
static void
camCB ()
{
	FImage fimage, *fip = &fimage;
	char *tolstr;
	char buf[1024];
	char fn[128];
	double move, stop;
	int fd;
	char caldir[1024];
	char errmsg[1024];
	char mapnameused[1024];
	BADCOL *badColMap;
	int rt;

	/* read the new file */
	nextName (fn);
	fd = open (fn, O_RDWR);
	if (fd < 0) {
	    wlprintf (msg_w, "%s: %s", fn, strerror(errno));
	    return;
	}
	initFImage (fip);
	if (readFITS (fd, fip, buf) < 0) {
	    wlprintf (msg_w, "%s: %s", fn, buf);
	    close (fd);
	    return;
	}
	
/* STO: 5/8/01  Apply corrections here during autofocus
	Make this contingent upon the presence of config flags, because
	it's not entirely clear that this is what we really want to do always...
*/
	// including the scanned file names
	telfixpath (caldir, "archive/calib");
	
	// if we want corrections, and didn't supply our own, use defaults
	if(OFIXBADCOL) {
		if(!OBADCOLMAP[0]) {
			if (findMapFN (caldir, OBADCOLMAP, errmsg) < 0) {
			    printf ("Warning: Disabling Autofocus bad column corrections (OFIXBADCOL); %s\n", errmsg);
			    wlprintf (msg_w, "Bad Column corrections DISABLED");
			    OFIXBADCOL = 0;
			}
		}
	}
	if(OUSECORIMGS) {
		if(!OBIASIMG[0]) {
			if (findBiasFN (fip, caldir, OBIASIMG, errmsg) < 0) {
		    	printf ("Warning: Disabling Autofocus Bias/Therm/Flat corrections(OUSECORIMGS); %s\n", errmsg);
			    wlprintf (msg_w, "Bias/Therm/Flat Corrections DISABLED");
			    OUSECORIMGS = 0;
			}
		}
		if(OUSECORIMGS && !OTHERMIMG[0]) {
			if (findThermFN (fip, caldir, OTHERMIMG, errmsg) < 0) {
		    	printf ("Warning: Disabling Autofocus Bias/Therm/Flat corrections(OUSECORIMGS); %s\n", errmsg);
			    wlprintf (msg_w, "Corrections for Autofocus DISABLED");
			    OUSECORIMGS = 0;
			}
		}
		if(OUSECORIMGS && !OFLATIMG[0]) {
			if (findFlatFN (fip, 0, caldir, OFLATIMG, errmsg) < 0) {
		    	printf ("Warning: Disabling Autofocus Bias/Therm/Flat corrections(OUSECORIMGS); %s\n", errmsg);
			    wlprintf (msg_w, "Corrections for Autofocus DISABLED");
			    OUSECORIMGS = 0;
			}
		}
	}

	/* Do corrections */
	if(OUSECORIMGS) {
	    wlprintf (msg_w, "Applying image corrections");
		if (correctFITS (fip, OBIASIMG, OTHERMIMG, OFLATIMG, errmsg) < 0) {
	    	wlprintf(msg_w, "Correction error: %s", errmsg);
	    	close(fd);
	    }
	}
	/* correct bad columns */	
	if(OFIXBADCOL) {
		rt = readMapFile("", OBADCOLMAP,&badColMap,mapnameused,errmsg);
		if(rt > 0) {
		  wlprintf (msg_w, "Removing bad columns");
		  rt = removeBadColumns(fip,badColMap,mapnameused,errmsg);
		  free(badColMap);
		}
		if(rt < 0) {
			printf("Bad column file %s: %s", OBADCOLMAP, errmsg);
		    wlprintf (msg_w, "Bad Column correction DISABLED\n");
		    OFIXBADCOL = 0;
			close(fd);
		}
	}
/*---*/	

    if(autofocus_done)
    {
        finishFITS(fn, fd, fip);
        fstate = FS_IDLE;
        wlprintf (msg_w, "Focus complete");
        return;
    }
    
	/* find stats and compute next position, then move on if necessary */
	if (nextMove (fip, &move) < 0) {
	    /* nextMove already explained */
	    fstate = FS_IDLE;
	} else {
	    /* issue move and repeat unless step was sufficiently small */
	    tolstr = XmTextFieldGetString (tol_w);
	    stop = atof (tolstr);
	    XtFree (tolstr);
	    if (fabs(move) <= stop) {
          autofocus_done = 1;
          double pos = OMOT->cpos*OMOT->step/(2*PI*OMOT->focscale);
          move = findBest(); // STO 3/15/07
          wlprintf (msg_w, "Best Focus = %g", move);
          (void) fifoMsg (Focus_Id, "%g", move-pos);
//		  fstate = FS_IDLE;
//		  wlprintf (msg_w, "Focus complete");
	    } else {
		  wlprintf (msg_w, "Next move = %g", move);
		  (void) fifoMsg (Focus_Id, "%g", move);
		  /* focus response will repeat and goose us again */
	    }
	}

	/* update image with more goodies in any case */
	finishFITS (fn, fd, fip);
}

/* qsort-style comparison function to sort by increasing position */
static int
pos_qf (const void *p1, const void *p2)
{
	double pos1 = ((ImgInfo *)p1)->pos;
	double pos2 = ((ImgInfo *)p2)->pos;
	double diff = pos1 - pos2;

	if (diff < 0)
	    return (-1);
	if (diff > 0)
	    return (1);
	return (0);
}

/* given a new fip, compute and return the next focus move, in microns.
 * return 0 if ok to proceed with next, else emit msg and return -1.
 */
static int
nextMove (FImage *fip, double *movep)
{
	MotorInfo *mip = OMOT;
	double thispos, thissd;
	AOIStats aoi;
	int w, h;
	int i;
    // STO 3/15/07
    double thisfwhm;
    double FWh, FWhs, FWv, FWvs;

    char whynot[1024];

	/* get stats */
	w = fip->sw;
	h = fip->sh;
	aoiStatsFITS (fip->image, w, GAP, GAP, w-2*GAP, h-2*GAP, &aoi);
    if (fwhmFITS (fip->image,fip->sw,fip->sh,&FWh,&FWhs,&FWv,&FWvs,whynot) < 0)
    {
        // can't get fwhm, record as NA
        thisfwhm = BADFWHM;
    }
    else
    {
        // record the fwhm for this position
        thisfwhm = sqrt(FWh*HPIXSZ*FWv*VPIXSZ);
    }
	/* record info from this image */
	thispos = mip->step*mip->cpos/mip->focscale/(2*PI);
	thissd = aoi.sd;
	
	/* add to FITS header too */
	setRealFITS (fip, "FOCSD", thissd, 6, "Std Dev of Focus image");
	setRealFITS (fip, "FOCPOS", thispos, 6, "Position of Focus image, um");

#if !(IGNORE_SANITY_CHECK)
	/* make some overall judgements */
	if (aoi.max > .9*MAXCAMPIX) {
	    wlprintf (msg_w, "Stopping: Image is too bright");
	    return (-1);
	}
	if (aoi.sd < OMINSTD) {
	    wlprintf (msg_w, "Stopping: StdDev = %.1f (must be >= %.1f)",
							    aoi.sd, OMINSTD);
	    return (-1);
	}
#endif

	/* add to list, or overwrite worst if full */
	if (nimg < MAXHIST) {
	    i = nimg++;
	} else {
	    int worsti = 0;
	    for (i = 1; i < MAXHIST; i++)
		if (imghist[i].sd < imghist[worsti].sd)
		    worsti = i;
	    i = worsti;
	}
	imghist[i].pos = thispos;
	imghist[i].sd = thissd;
    imghist[i].fwhm = thisfwhm;

	/* sort by position */
	qsort ((void *)imghist, nimg, sizeof(ImgInfo), pos_qf);

	/* show the new list */
	showStats();
	showGraph();

	/* decide the next move */
	*movep = nimg == 1 ? OFIRSTSTEP : findNew() - thispos;
	return (0);
}

/* analyse the imghist[] array and compute next best place to take an image.
 * N.B. we assume the array is already sorted by increasing position.
 * N.B. we assume nimg > 1.
 */
static double
findNew()
{
	int besti, bestnbr;
	int i;

	/* find the best position so far */
	besti = 0;
	for (i = 1; i < nimg; i++)
    {
        if(imghist[besti].fwhm != BADFWHM)
        {
            if(imghist[i].fwhm < imghist[besti].fwhm)
            {
                besti = i;
            }
        }
	    else if (imghist[i].sd > imghist[besti].sd)
        {
		  besti = i;
        }
    }
    
	/* if at either end, just go more off that side */
	if (besti == 0)
	    return (2*imghist[0].pos - imghist[1].pos);
	if (besti == nimg-1)
	    return (2*imghist[nimg-1].pos - imghist[nimg-2].pos);
    
	/* else split the difference between the best and its best neighbor */
	bestnbr = imghist[besti-1].sd > imghist[besti+1].sd ? besti-1 : besti+1;
	return ((imghist[besti].pos + imghist[bestnbr].pos)/2.0);
}

/* Find the best, but don't interpolate a new position.  Use for final position */
static double
findBest()
{
    int besti;
    int i;

    /* find the best position so far */
    besti = 0;
    for (i = 1; i < nimg; i++)
    {
        if(imghist[besti].fwhm != BADFWHM)
        {
            if(imghist[i].fwhm < imghist[besti].fwhm)
            {
                besti = i;
            }
        }
        else if (imghist[i].sd > imghist[besti].sd)
        {
          besti = i;
        }
    }
    return (imghist[besti].pos);
}

/* read the TF and fill in cp->duration with ms duration.
 * complain and return -1 if trouble.
 */
static int
getExpTime(CCDExpoParams *cp)
{
	char *str;
	double s;
	int ms;

	str = XmTextFieldGetString (dur_w);
	s = atof (str);
	XtFree (str);
	ms = (int)(1000.0*s + 0.5);

	if (s < 0.0 || ms <= 0) {
	    wlprintf (msg_w, "Exp time must be > 0");
	    return (-1);
	}
	cp->duration = ms;
	return (0);
}

/* read the TF and fill in cp->bx and by.
 * complain and return -1 if trouble.
 */
static int
getBinning (CCDExpoParams *cp)
{
	char *str;
	int b;

	str = XmTextFieldGetString (bin_w);
	b = atoi (str);
	XtFree (str);

	if (b <= 0) {
	    wlprintf (msg_w, "Binning must be > 0");
	    return (-1);
	}
	cp->bx = cp->by = b;
	return (0);
}

/* create a name for the next image */
static void
nextName (char *fn)
{
	(void) sprintf (fn, "/tmp/Focus%02d.fts", nimg);
}
	    
/* get the temp to use for the focus reference point.
 * first aux sensor takes priority over ambient
 */
static double
focusTemp()
{
	double newtemp = telstatshmp->now.n_temp;
	WxStats *wxp = &telstatshmp->wxs;
	int i;

	for (i = MAUXTP; --i >= 0; ) {
	    if (wxp->auxtmask & (1<<i)) {
		newtemp = wxp->auxt[i];
		break;
	    }
	}

	return (newtemp);
}

/* add FWHM, rewrite fip over fd, tell camera -- ignore any errors.
 * N.B. we close fd and reset fip
 */
static void
finishFITS (char fn[], int fd, FImage *fip)
{
	char buf[1024];
	int s;

	(void) setFWHMFITS (fip, buf);
	lseek (fd, 0L, 0);
	s = writeFITS (fd, fip, buf, 0);
	close (fd);
	resetFImage (fip);
	if (s == 0) {
	    /* tell camera */
	    int fd = telopen ("comm/CameraFilename", O_WRONLY|O_NONBLOCK);
	    if (fd >= 0) {
		(void) write (fd, fn, strlen(fn));
		close (fd);
	    }
	}
}
	
/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: autofocus.c,v $ $Date: 2007/06/09 10:09:57 $ $Revision: 1.10 $ $Name:  $"};
