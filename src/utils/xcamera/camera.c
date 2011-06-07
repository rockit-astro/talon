/* code to operate any camera using the ccdcamera.h interface.
 * use msg() to talk to the operator.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <Xm/Xm.h>
#include <X11/keysym.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/ToggleB.h>
#include <Xm/TextF.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/Separator.h>
#include <Xm/ArrowB.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "strops.h"
#include "xtools.h"
#include "fits.h"
#include "fitscorr.h"
#include "configfile.h"
#include "fieldstar.h"
#include "telenv.h"
#include "ccdcamera.h"
#include "telstatshm.h"
#include "telfits.h"

#include "camera.h"

#define	BEEP_TIME	30	/* beep when finished when dur this long */


// from ccdcamera.h
extern int TIME_SYNC_DELAY;

/* info needed when we get pixels from the camera.
 * we save all that we need to take simple exposures, or make bias, thermal
 * and flat reference images. some fields are no used for each exposure use.
 */

typedef enum {
    NO_EXP = 0, CONT_EXP, BIAS_EXP, THERM_EXP, FLAT_EXP, CDSCAN_EXP
} ExpState;

typedef struct {
    ExpState state;		/* current goal -- all cases */
    XtInputId iid;		/* camera driver -- all cases */
    int npixels;		/* total pixels in image -- all cases */
    FImage fimage;		/* temporary image info -- all cases */
    char newfn[1024];		/* name of file to create -- all cases */
    int n, ntot;		/* images so far and total goal -- all cases */
    float *fpmem;		/* intermediate fp accumulator -- all cases */
    CamPixel ** mdmem;    /* buffer space allocated for median computations */
    XtIntervalId countTID;	/* used for exposure countdown timer */
    XtIntervalId tempTID;	/* used to monitor temperature periodically */
    time_t ctime;		/* estimated time of exposure completion */
    char biasfn[1024];		/* just THERM_EXP and FLAT_EXP */
    char thermfn[1024];		/* just FLAT_EXP */
    int filter;			/* just FLAT_EXP */
} ExpInfo;

#define	CD_MS	1000		/* exp countdown interval, ms */
#define	TEMP_MS	2000		/* temperature polling interval, ms */

static ExpInfo expinfo;
static CCDExpoParams expP;

/* drift scan info */
typedef enum { XT_NF, XT_CDS } ExpType;
static ExpType xtype;           /* type of drift scan exposure */
static Widget xtnf_w, xtcds_w;  /* TB's selecting type of exposure */
static Widget cdsri_w;          /* TF of row interval */
static int canDS;		/* 1 if we can drift scan, else 0 */

/* orientation and scale info from config file */
static char ccfn[] = "archive/config/camera.cfg";
static double HPIXSZ, VPIXSZ;	/* "/pix */
static int DEFTEMP;		/* default intial cooler temp, C */
static int LRFLIP, TBFLIP;	/* 1 to flip */
static int RALEFT, DECUP;	/* raw increase */
static char tele_kw[128];	/* TELESCOP keyword */
static char orig_kw[128];	/* ORIGIN keyword */

static void camInit(void);
static void camFullFrame(void);
static void fullFrameCB (Widget w, XtPointer client, XtPointer call);
static void AOIcursorCB (Widget w, XtPointer client, XtPointer call);
static void xtCB (Widget w, XtPointer client, XtPointer call);
static void cdsriCB (Widget w, XtPointer client, XtPointer call);
static void countCB (Widget w, XtPointer client, XtPointer call);
static void count1CB (Widget w, XtPointer client, XtPointer call);
static void durArrowCB (Widget w, XtPointer client, XtPointer call);
static void dur1CB (Widget w, XtPointer client, XtPointer call);
static void binArrowCB (Widget w, XtPointer client, XtPointer call);
static void bin11CB (Widget w, XtPointer client, XtPointer call);
static void shutterCB (Widget w, XtPointer client, XtPointer call);
static void ishutterCB (Widget w, XtPointer client, XtPointer call);
static void coolerCB (Widget w, XtPointer client, XtPointer call);
static void goalCB (Widget w, XtPointer client, XtPointer call);
static void countTO (XtPointer client, XtIntervalId *idp);
static void startTempTimer(void);
static void stopTempTimer(void);
static void showCoolerTemp(void);
static void camGoCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);
static void unmapCB (Widget w, XtPointer client, XtPointer call);
static void resetExpInfo(void);
static void gatherExpParams (void);
static int readPixels (void);
static void flipImg(void);
static void initCfg(void);

static Widget cam_w;		/* main camera setup dialog */

static Widget dur_w;		/* duration text field widget */
static Widget count_w;		/* exposure count text field widget */
static Widget sx_w;		/* subimage x text field widget */
static Widget sy_w;		/* subimage y text field widget */
static Widget sw_w;		/* subimage w text field widget */
static Widget sh_w;		/* subimage h text field widget */
static Widget bx_w;		/* binning x text field widget */
static Widget by_w;		/* binning y text field widget */
static Widget name_w;		/* label for camera id */
static Widget shutterO_w;	/* shutter Open toggle button */
static Widget shutterC_w;	/* shutter Closed toggle button */
static Widget shutterD_w;	/* shutter Dbl option toggle button */
static Widget shutterM_w;	/* shutter Multi option toggle button */
static Widget cooler_w;		/* label to display cooler temp */
static Widget coolgoal_w;	/* cooler goal temp TF */
static Widget con_w;		/* cooler on TB */
static Widget fliplr_w;		/* flip l-r TB */
static Widget fliptb_w;		/* flip t-b TB */

static int expCancel;		/* set to cancel exposure(s) */
static char cam_id[64];		/* camera id string -- also inited flag */

static TelStatShm *telstatshmp;	/* shared system info, or NULL */
static void init_shm(void);

static char driver[1024];	/* driver or control script */
static int auxcam;		/* whether driver is real or script */

/* fairly generic camera control function interfaces */
static void startExpose(ExpState state, char *fn, int makefpmem, int ntot);
static int getCameraID(char *buf);
static void setCoolerTemp(CCDTempInfo *tp);
static int getCoolerTemp(CCDTempInfo *tp);

/* return 0 if looks like we have a camera, else -1 */
int
camDevFileRW()
{
	char msg[1024];

	initCfg();
	return (setPathCCD (driver, auxcam, msg));
}

void
camManage()
{
	if (XtIsManaged(cam_w)) {
	    raiseShell (cam_w);
	} else {
	    /* try initting again if no name yet */
	    if (cam_id[0] == '\0')
		camInit();

	    XtManageChild (cam_w);
	    startTempTimer();
	}
}

void
camTake1()
{
	if (expinfo.state != NO_EXP) {
	    msg ("Camera is busy -- Cancel first");
	    return;
	}
	XmTextFieldSetString (count_w, "1");
	startExpose(CONT_EXP, NULL, 0, 1);
}

void
camLoop()
{
	if (expinfo.state != NO_EXP) {
	    msg ("Camera is busy -- Cancel first");
	    return;
	}
	XmTextFieldSetString (count_w, "99999");
	startExpose(CONT_EXP, NULL, 0, 99999);
}

void
camCancel()
{
	switch (expinfo.state) {
	case NO_EXP:
	    break;
	case CDSCAN_EXP:
	    expinfo.state = NO_EXP;
	    dsStop();
	    break;
	default:
	    expCancel = 1;
	    break;
	}
}

void
camCreate()
{
	typedef struct {
	    char *lname, *lvalue;	/* label name and initial value */
	    char *tname;		/* text field name */
	    int columns;		/* text field columns */
	    Widget *wp;			/* where to save the text field widget*/
	} Prompt;
	static Prompt subi_p[] = {
	    {"SXL", "X:", "SX", 4, &sx_w},
	    {"SYL", "Y:", "SY", 4, &sy_w},
	    {"SWL", "W:", "SW", 4, &sw_w},
	    {"SHL", "H:", "SH", 4, &sh_w},
	};
	static Prompt bin_p[] = {
	    {"BXL", "H:", "BX", 4, &bx_w},
	    {"BYL", "V:", "BY", 4, &by_w},
	};
	Arg args[20];
	Widget rc_w;
	Widget hrc_w;
	Widget f_w;
	Widget w;
	int n;
	int i;

	/* first see whether we support drift scanning */
	canDS = !haveDS();

	/* create form */

	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_GROW); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	cam_w = XmCreateFormDialog (toplevel_w, "Cam", args,n);
	XtAddCallback (cam_w, XmNunmapCallback, unmapCB, 0);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Exposure Setup"); n++;
	XtSetValues (XtParent(cam_w), args, n);
	XtVaSetValues (cam_w, XmNcolormap, camcm, NULL);

	/* make everything in a vertical rowcolumn.
	 * each row is then a horizontal rowcolumn with it.
	 */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNspacing, 5); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	rc_w = XmCreateRowColumn (cam_w, "RC", args, n);
	XtManageChild (rc_w);

	/* camera id at the top */

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	name_w = XmCreateLabel (rc_w, "ID", args, n);
	XtManageChild (name_w);

	n = 0;
	w = XmCreateLabel (rc_w, "GAP", args, n);
	XtManageChild (w);
	wlprintf (w, "%s", "  ");

	/* top portion is normal still images */

	n = 0;
	XtSetArg (args[n], XmNseparatorType, XmDOUBLE_LINE); n++;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	if (canDS)
	    XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	XtSetArg (args[n], XmNset, True); n++;
	xtnf_w = XmCreateToggleButton (rc_w, "XTN", args, n);
	XtAddCallback (xtnf_w, XmNvalueChangedCallback, xtCB, (XtPointer)XT_NF);
	wlprintf (xtnf_w, "Normal Frame:");
	if (canDS)
	    XtManageChild (xtnf_w);
	xtype = XT_NF;

	/* make the subimage controls */

	n = 0;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	hrc_w = XmCreateRowColumn (rc_w, "SubIRC", args, n);
	XtManageChild (hrc_w);

	n = 0;
	w = XmCreateLabel (hrc_w, "SL", args, n);
	XtManageChild (w);
	wlprintf (w, "Subimage:");

	for (i = 0; i < XtNumber(subi_p); i++) {
	    Prompt *p = &subi_p[i];

	    n = 0;
	    w = XmCreateLabel (hrc_w, p->lname, args, n);
	    XtManageChild (w);
	    wlprintf (w, p->lvalue);

	    n = 0;
	    XtSetArg (args[n], XmNcolumns, p->columns); n++;
	    *p->wp = XmCreateTextField (hrc_w, p->tname, args, n);
	    XtManageChild (*p->wp);
	}

	/* make the binning controls */

	n = 0;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	hrc_w = XmCreateRowColumn (rc_w, "BinRC", args, n);
	XtManageChild (hrc_w);

	n = 0;
	w = XmCreateLabel (hrc_w, "BL", args, n);
	XtManageChild (w);
	wlprintf (w, " Binning:");

	for (i = 0; i < XtNumber(bin_p); i++) {
	    Prompt *p = &bin_p[i];

	    n = 0;
	    w = XmCreateLabel (hrc_w, p->lname, args, n);
	    XtManageChild (w);
	    wlprintf (w, p->lvalue);

	    n = 0;
	    XtSetArg (args[n], XmNcolumns, p->columns); n++;
	    *p->wp = XmCreateTextField (hrc_w, p->tname, args, n);
	    XtManageChild (*p->wp);
	}

	n = 0;
	XtSetArg (args[n], XmNarrowDirection, XmARROW_UP); n++;
	w = XmCreateArrowButton (hrc_w, "UP", args, n);
	XtAddCallback (w, XmNactivateCallback, binArrowCB, (XtPointer)0);
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNarrowDirection, XmARROW_DOWN); n++;
	w = XmCreateArrowButton (hrc_w, "DOWN", args, n);
	XtAddCallback (w, XmNactivateCallback, binArrowCB, (XtPointer)1);
	XtManageChild (w);

	n = 0;
	w = XmCreatePushButton (hrc_w, "DOWN", args, n);
	XtAddCallback (w, XmNactivateCallback, bin11CB, (XtPointer)0);
	wlprintf (w, " 1:1 ");
	XtManageChild (w);

	/* subimage shortcuts */

	n = 0;
	XtSetArg (args[n], XmNfractionBase, 11); n++;
	f_w = XmCreateForm (rc_w, "SIF", args, n);
	XtManageChild (f_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 1); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 5); n++;
	    w = XmCreatePushButton (f_w, "SF", args, n);
	    XtAddCallback (w, XmNactivateCallback, fullFrameCB, 0);
	    wlprintf (w, "Set Full Frame");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 6); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 10); n++;
	    w = XmCreatePushButton (f_w, "FC", args, n);
	    XtAddCallback (w, XmNactivateCallback, AOIcursorCB, NULL);
	    wlprintf (w, "Set from AOI");
	    XtManageChild (w);

	/* make the duration control */

	n = 0;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	hrc_w = XmCreateRowColumn (rc_w, "DurRC", args, n);
	XtManageChild (hrc_w);

	    n = 0;
	    w = XmCreateLabel (hrc_w, "DurL", args, n);
	    XtManageChild (w);
	    wlprintf (w, "Duration:");

	    n = 0;
	    XtSetArg (args[n], XmNcolumns, 7); n++;
	    dur_w = XmCreateTextField (hrc_w, "Dur", args, n);
	    XtAddCallback (dur_w, XmNactivateCallback, camGoCB, NULL);
	    XtManageChild (dur_w);

	    n = 0;
	    w = XmCreateLabel (hrc_w, "Secs", args, n);
	    wlprintf (w, "Second(s)  ");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNarrowDirection, XmARROW_UP); n++;
	    w = XmCreateArrowButton (hrc_w, "SUP", args, n);
	    XtAddCallback (w, XmNactivateCallback, durArrowCB, (XtPointer)0);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNarrowDirection, XmARROW_DOWN); n++;
	    w = XmCreateArrowButton (hrc_w, "SDW", args, n);
	    XtAddCallback (w, XmNactivateCallback, durArrowCB, (XtPointer)1);
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (hrc_w, "S1", args, n);
	    XtAddCallback (w, XmNactivateCallback, dur1CB, (XtPointer)0);
	    wlprintf (w, " 1 ");
	    XtManageChild (w);

	/* make the count control */

	n = 0;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	hrc_w = XmCreateRowColumn (rc_w, "CntRC", args, n);
	XtManageChild (hrc_w);

	    n = 0;
	    w = XmCreateLabel (hrc_w, "CntL", args, n);
	    XtManageChild (w);
	    wlprintf (w, "   Count:");

	    n = 0;
	    XtSetArg (args[n], XmNcolumns, 7); n++;
	    XtSetArg (args[n], XmNvalue, "1"); n++;
	    count_w = XmCreateTextField (hrc_w, "NExp", args, n);
	    XtAddCallback (count_w, XmNactivateCallback, camGoCB, NULL);
	    XtManageChild (count_w);

	    n = 0;
	    w = XmCreateLabel (hrc_w, "NExpL", args, n);
	    wlprintf (w, "Exposure(s)");
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNarrowDirection, XmARROW_UP); n++;
	    w = XmCreateArrowButton (hrc_w, "CUP", args, n);
	    XtAddCallback (w, XmNactivateCallback, countCB, (XtPointer)1);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNarrowDirection, XmARROW_DOWN); n++;
	    w = XmCreateArrowButton (hrc_w, "CDW", args, n);
	    XtAddCallback (w, XmNactivateCallback, countCB, (XtPointer)0);
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (hrc_w, "C1", args, n);
	    XtAddCallback (w, XmNactivateCallback, count1CB, (XtPointer)0);
	    wlprintf (w, " 1 ");
	    XtManageChild (w);

	/* make the exposure shutter controls */

	n = 0;
	XtSetArg (args[n], XmNadjustMargin, False); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	hrc_w = XmCreateRowColumn (rc_w, "ShutterRC", args, n);
	XtManageChild (hrc_w);

	    n = 0;
	    w = XmCreateLabel (hrc_w, "ShutterL", args, n);
	    XtManageChild (w);
	    wlprintf (w, " Shutter:");

	    n = 0;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    XtSetArg (args[n], XmNset, True); n++;
	    shutterO_w = XmCreateToggleButton (hrc_w, "ShutterOpen", args, n);
	    XtAddCallback (shutterO_w, XmNvalueChangedCallback, shutterCB,
						    (XtPointer) CCDSO_Open);
	    XtManageChild (shutterO_w);
	    wlprintf (shutterO_w, "Open");

	    n = 0;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    XtSetArg (args[n], XmNset, False); n++;
	    shutterC_w = XmCreateToggleButton (hrc_w, "ShutterClosed", args, n);
	    XtAddCallback (shutterC_w, XmNvalueChangedCallback, shutterCB,
						    (XtPointer) CCDSO_Closed);
	    XtManageChild (shutterC_w);
	    wlprintf (shutterC_w, "Closed");

	    n = 0;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    XtSetArg (args[n], XmNset, False); n++;
	    shutterD_w = XmCreateToggleButton (hrc_w, "ShutterDbl",args, n);
	    XtAddCallback (shutterD_w, XmNvalueChangedCallback, shutterCB,
						    (XtPointer) CCDSO_Dbl);
	    XtManageChild (shutterD_w);
	    wlprintf (shutterD_w, "Dbl");

	    n = 0;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    XtSetArg (args[n], XmNset, False); n++;
	    shutterM_w = XmCreateToggleButton (hrc_w, "ShutterMulti",args, n);
	    XtAddCallback (shutterM_w, XmNvalueChangedCallback, shutterCB,
						    (XtPointer) CCDSO_Multi);
	    XtManageChild (shutterM_w);
	    wlprintf (shutterM_w, "Mult");

	    n = 0;
	    w = XmCreatePushButton (hrc_w, "O", args, n);
	    XtAddCallback (w, XmNactivateCallback, ishutterCB, (XtPointer)1);
	    XtManageChild (w);

	    n = 0;
	    w = XmCreatePushButton (hrc_w, "C", args, n);
	    XtAddCallback (w, XmNactivateCallback, ishutterCB, (XtPointer)0);
	    XtManageChild (w);

	/* make the image flip controls */

	n = 0;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	hrc_w = XmCreateRowColumn (rc_w, "FlipRC", args, n);
	XtManageChild (hrc_w);

	    n = 0;
	    w = XmCreateLabel (hrc_w, "FlipL", args, n);
	    XtManageChild (w);
	    wlprintf (w, "    Flip:");

	    n = 0;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    fliptb_w = XmCreateToggleButton (hrc_w, "FlipTB", args, n);
	    XtManageChild (fliptb_w);
	    wlprintf (fliptb_w, "Rows");

	    n = 0;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    fliplr_w = XmCreateToggleButton (hrc_w, "FlipLR", args, n);
	    XtManageChild (fliplr_w);
	    wlprintf (fliplr_w, "Columns");

	/* Continuous drift scan */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep2", args, n);
	if (canDS)
	    XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	xtcds_w = XmCreateToggleButton (rc_w, "XTN", args, n);
	XtAddCallback (xtcds_w, XmNvalueChangedCallback,xtCB,(XtPointer)XT_CDS);
	wlprintf (xtcds_w, "Continuous Drift Scan:");
	if (canDS)
	    XtManageChild (xtcds_w);

	/* continuous drift timing */

	n = 0;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	hrc_w = XmCreateRowColumn (rc_w, "CDSRC", args, n);
	if (canDS)
	    XtManageChild (hrc_w);

	    n = 0;
	    w = XmCreateLabel (hrc_w, "CDSL1", args, n);
	    XtManageChild (w);
	    wlprintf (w, "  Row dT:", XK_mu);

	    n = 0;
	    XtSetArg (args[n], XmNcolumns, 7); n++;
	    XtSetArg (args[n], XmNvalue, "66000"); n++;
	    cdsri_w = XmCreateTextField (hrc_w, "CDSRI", args, n);
	    XtAddCallback (cdsri_w, XmNactivateCallback, cdsriCB, NULL);
	    XtManageChild (cdsri_w);

	    n = 0;
	    w = XmCreateLabel (hrc_w, "CDSL2", args, n);
	    XtManageChild (w);
	    wlprintf (w, "%cs", XK_mu);

	n = 0;
	XtSetArg (args[n], XmNseparatorType, XmDOUBLE_LINE); n++;
	w = XmCreateSeparator (rc_w, "Sep", args, n);
	if (canDS)
	    XtManageChild (w);

	/* make the cooler controls */

	n = 0;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNspacing, 5); n++;
	hrc_w = XmCreateRowColumn (rc_w, "CoolerRC", args, n);
	XtManageChild (hrc_w);

	    n = 0;
	    w = XmCreateLabel (hrc_w, "CoolerL", args, n);
	    XtManageChild (w);
	    wlprintf (w, "  Cooler:");

	    n = 0;
	    con_w = XmCreateToggleButton (hrc_w, "CoolerOnOff", args, n);
	    XtAddCallback (con_w, XmNvalueChangedCallback, coolerCB, 0);
	    XtManageChild (con_w);
	    wlprintf (con_w, "On");

	    n = 0;
	    w = XmCreateLabel (hrc_w, "CoolerL", args, n);
	    XtManageChild (w);
	    wlprintf (w, " Goal:");

	    n = 0;
	    XtSetArg (args[n], XmNcolumns, 4); n++;
	    coolgoal_w = XmCreateTextField (hrc_w, "CoolerGoal", args, n);
	    XtAddCallback (coolgoal_w, XmNactivateCallback, goalCB, 0);
	    XtManageChild (coolgoal_w);

	    n = 0;
	    w = XmCreateLabel (hrc_w, "CoolerC", args, n);
	    XtManageChild (w);
	    wlprintf (w, " Now:");

	    n = 0;
	    cooler_w = XmCreateLabel (hrc_w, "CoolerTemp", args, n);
	    XtManageChild (cooler_w);

	/* separator */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep2", args, n);
	XtManageChild (w);

	/* control buttons */

	n = 0;
	XtSetArg (args[n], XmNfractionBase, 16); n++;
	f_w = XmCreateForm (rc_w, "CF", args, n);
	XtManageChild (f_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomOffset, 8); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 1); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 5); n++;
	    w = XmCreatePushButton (f_w, "Start", args, n);
	    XtAddCallback (w, XmNactivateCallback, camGoCB, NULL);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomOffset, 8); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 6); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 10); n++;
	    w = XmCreatePushButton (f_w, "Stop", args, n);
	    XtAddCallback (w, XmNactivateCallback, camStopCB, NULL);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomOffset, 8); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 11); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 15); n++;
	    w = XmCreatePushButton (f_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, closeCB, NULL);
	    XtManageChild (w);

	/* set up from real camera info */
	camInit();
}

/* init widgets from real camera info */
static void
camInit()
{
	CCDTempInfo tinfo;

	/* get id */
	if (getCameraID(cam_id) < 0)
	    wlprintf (name_w, "%s", "Unknown camera");
	else
	    wlprintf (name_w, "%s", cam_id);

	/* set to full-frame exposure */
	camFullFrame();

	/* gather config file info. not fatal if not present */
	initCfg();

	/* we want degrees */
	VPIXSZ /= 3600.0;
	HPIXSZ /= 3600.0;

	/* set flips */
	XmToggleButtonSetState (fliplr_w, LRFLIP, False);
	XmToggleButtonSetState (fliptb_w, TBFLIP, False);

	/* set to default temp unless already cooler */
	if (getCoolerTemp (&tinfo)<0 || tinfo.s==CCDTS_OFF || tinfo.t>DEFTEMP)
	    tinfo.t = DEFTEMP;
	tinfo.s = CCDTS_SET;
	setCoolerTemp (&tinfo);
	wtprintf (coolgoal_w, "%d", tinfo.t);

	/* try to connect to shared status area */
	init_shm();
	if (telstatshmp)
	    telstatshmp->camstate = CAM_IDLE;

#undef	NCCFG
}

static void
initCfg()
{
#define	NCCFG	(sizeof(ccfg)/sizeof(ccfg[0]))
	static CfgEntry ccfg[] = {
	    {"TELE",	CFG_STR, tele_kw, sizeof(tele_kw)},
	    {"ORIG",	CFG_STR, orig_kw, sizeof(tele_kw)},
	    {"LRFLIP",	CFG_INT, &LRFLIP},
	    {"TBFLIP",	CFG_INT, &TBFLIP},
	    {"RALEFT",	CFG_INT, &RALEFT},
	    {"DECUP",	CFG_INT, &DECUP},
	    {"DEFTEMP",	CFG_INT, &DEFTEMP},
	    {"VPIXSZ",	CFG_DBL, &VPIXSZ},
	    {"HPIXSZ",	CFG_DBL, &HPIXSZ},
	};

	readCfgFile (0, ccfn, ccfg, NCCFG);

	// Set the optional Correction settings (see fitscorr.c)
	readCorrectionCfg(0, ccfn);

	// read optional item(s)
	if(read1CfgEntry(1, ccfn, "TIME_SYNC_DELAY", CFG_INT, &TIME_SYNC_DELAY, sizeof(TIME_SYNC_DELAY)) < 0) {
		TIME_SYNC_DELAY = 0;
	}
	if(read1CfgEntry(1, ccfn, "DRIVER", CFG_STR, &driver, sizeof(driver)) < 0) {
		strcpy(driver,"");
	}
	if(read1CfgEntry(1, ccfn, "AUXCAM", CFG_INT, &auxcam, sizeof(auxcam)) < 0) {
		auxcam = 0;
	}
	// Note: Also see ccdcamera.c for SIGNALCMD option
}

/* try to connect to shared status area.
 * don't fuss if can't.
 */
static void
init_shm()
{
	int shmid;
	long addr;

	/* flag as not available until succeed */
	telstatshmp = NULL;

	shmid = shmget (TELSTATSHMKEY, sizeof(TelStatShm), 0);
	if (shmid < 0)
	    return;
	addr = (long) shmat (shmid, (void *)0, 0);
	if (addr == -1)
	    return;

	/* ok */
	telstatshmp = (TelStatShm *) addr;
}

/* init to full frame */
static void
camFullFrame()
{
	CCDExpoParams ce;
	char errmsg[1024];

	if (getSizeCCD (&ce, errmsg) < 0) {
	    msg ("Can't get camera sizes: %s", errmsg);
	    return;
	}

	wtprintf (sw_w, "%d", ce.sw);
	wtprintf (sh_w, "%d", ce.sh);
	wtprintf (sx_w, "%d", 0);
	wtprintf (sy_w, "%d", 0);
}

/* called from any of the several exposure type toggle buttons.
 * client is one of ExpType.
 */
/* ARGSUSED */
static void
xtCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	ExpType xt = (ExpType) client;

	if (!XmToggleButtonGetState(w))
	    return;

	xtype = xt;
	switch (xt) {
	case XT_NF:
	    XmToggleButtonSetState (xtcds_w, False, False);
	    break;
	case XT_CDS:
	    XmToggleButtonSetState (xtnf_w, False, False);
	    break;
	}
}

/* called from the Up and Down exposure count arrow buttons.
 * client is 1 to go up, 0 down
 */
/* ARGSUSED */
static void
countCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int up = (int)client == 1;
	char *str;
	int n;

	str = XmTextFieldGetString (count_w);
	n = atoi (str);
	XtFree (str);

	if (up)
	    n *= 2;
	else if (n > 1)
	    n /= 2;

	wtprintf (count_w, "%d", n);
}

/* called by the count = 1 shortcut */
/* ARGSUSED */
static void
count1CB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	wtprintf (count_w, "%d", 1);
}

/* called from either of the duration arrows.
 * client is 0 for up, 1 for down.
 */
/* ARGSUSED */
static void
durArrowCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int up = (int)client == 0;
	char *str;
	double t;

	str = XmTextFieldGetString (dur_w);
	t = atof(str);
	XtFree (str);

	if (up) {
	    if (t == 0)
		t = 1;
	    else
		t *= 2.0;
	} else if (t >= .02)
	    t /= 2.0;
	else
	    t = 0;
	msg ("");

	wtprintf (dur_w, "%.4g", t);
}

/* called by the 1-sec duration shortcut */
/* ARGSUSED */
static void
dur1CB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	wtprintf (dur_w, "%.4g", 1.0);
}

/* called from either of the binning arrows.
 * client is 0 for up, 1 for down.
 */
/* ARGSUSED */
static void
binArrowCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int up = (int)client == 0;
	CCDExpoParams ce;
	char errmsg[1024];
	char *str;
	int b;

	if (getSizeCCD (&ce, errmsg) < 0) {
	    msg ("Can't get camera max sizes: %s", errmsg);
	    return;
	}

	str = XmTextFieldGetString (bx_w);
	b = atoi (str);
	XtFree (str);
	if (up && b < ce.bx)
	    b++;
	else if (!up && b > 1)
	    --b;
	wtprintf (bx_w, "%d", b);

	str = XmTextFieldGetString (by_w);
	b = atoi (str);
	XtFree (str);
	if (up && b < ce.by)
	    b++;
	else if (!up && b > 1)
	    --b;
	wtprintf (by_w, "%d", b);
}

/* called from the 1:1 binning shortcut */
/* ARGSUSED */
static void
bin11CB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	wtprintf (bx_w, "1");
	wtprintf (by_w, "1");
}

/* called when want to set the subimage using the AOI */
/* ARGSUSED */
static void
AOIcursorCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	FImage *fip = &state.fimage;
	int hbin = fip ? fip->bx : 1;
	int vbin = fip ? fip->by : 1;
	int xoff = fip ? fip->sx : 0;
	int yoff = fip ? fip->sy : 0;

	wtprintf (sw_w, "%d", state.aoi.w * hbin);
	wtprintf (sh_w, "%d", state.aoi.h * vbin);
	wtprintf (sx_w, "%d", state.aoi.x * hbin + xoff);
	wtprintf (sy_w, "%d", state.aoi.y * vbin + yoff);
}

/* called when the full frame PB is activate */
/* ARGSUSED */
static void
fullFrameCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	camFullFrame();
}

/* called when any of the shutter toggle buttons values changes.
 * client is an CCDShutterOptions.
 */
/* ARGSUSED */
static void
shutterCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmToggleButtonCallbackStruct *s = (XmToggleButtonCallbackStruct *)call;

	if (s->set) {
	    /* we have been turned on, turn off all others */
	    if (w != shutterO_w)
		XmToggleButtonSetState(shutterO_w, 0, False);
	    if (w != shutterC_w)
		XmToggleButtonSetState(shutterC_w, 0, False);
	    if (w != shutterD_w)
		XmToggleButtonSetState(shutterD_w, 0, False);
	    if (w != shutterM_w)
		XmToggleButtonSetState(shutterM_w, 0, False);
	} else {
	    /* we have been turned off, turn on Open as a default */
	    XmToggleButtonSetState(shutterO_w, 1, False);
	}
}

/* called when either of the immediate shutter push buttons are pushed.
 * client is 1 to open, 0 to close.
 */
/* ARGSUSED */
static void
ishutterCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char errmsg[1024];

	if (setShutterNow ((int)client, errmsg) < 0)
	    msg ("Shutter: %s", errmsg);
}

/* called when the cooler state tb changes state.  */
/* ARGSUSED */
static void
coolerCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	CCDTempInfo tinfo;

	if (XmToggleButtonGetState(w)) {
	    char *str = XmTextFieldGetString (coolgoal_w);

	    tinfo.t = atoi (str);
	    tinfo.s = CCDTS_SET;
	    XtFree (str);
	} else {
	    tinfo.s = CCDTS_OFF;
	}

	setCoolerTemp (&tinfo);
	showCoolerTemp();
}

/* called when RETURN is hit on the goal TF */
/* ARGSUSED */
static void
goalCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char *str = XmTextFieldGetString (coolgoal_w);
	CCDTempInfo tinfo;

	tinfo.t = atoi (str);
	XtFree (str);
	tinfo.s = CCDTS_SET;

	setCoolerTemp (&tinfo);
	showCoolerTemp();
}

static void
showCoolerTemp()
{
	CCDTempInfo tinfo;

	if (getCoolerTemp (&tinfo) < 0) {
	    wlprintf (cooler_w, "?????");
	    XmToggleButtonSetState (con_w, False, False);
	} else {
	    char buf[100];
	    int on = 0;

	    (void) sprintf (buf, "%3d C ", tinfo.t);
	    switch (tinfo.s) {
	    case CCDTS_AT:	on = 1; strcat (buf, "AtTarg"); break;
	    case CCDTS_UNDER:	on = 1; strcat (buf, "<Targ"); break;
	    case CCDTS_OVER:	on = 1; strcat (buf, ">Targ"); break;
	    case CCDTS_OFF:	on = 0; strcat (buf, "Off"); break;
	    case CCDTS_RDN:	on = 1; strcat (buf, "Ramping"); break;
	    case CCDTS_RUP:	on = 0; strcat (buf, "ToAmb"); break;
	    case CCDTS_STUCK:	on = 1; strcat (buf, "Floor"); break;
	    case CCDTS_MAX:	on = 1; strcat (buf, "Ceiling"); break;
	    case CCDTS_AMB:	on = 0; strcat (buf, "AtAmb"); break;
	    default:    	on = 0; strcat (buf, "Error"); break;
	    }

	    if (tinfo.s == CCDTS_STUCK) {
		/* back off 2 degrees if hit floor */
		char *str = XmTextFieldGetString (coolgoal_w);

		tinfo.t = atoi (str) + 2;
		tinfo.s = CCDTS_SET;
		XtFree (str);
		setCoolerTemp (&tinfo);

		wtprintf (coolgoal_w, "%d", tinfo.t);
		msg ("Hit cooler floor -- Shifting goal up 2%c", XK_degree);
	    }

	    wlprintf (cooler_w, "%s", buf);
	    XmToggleButtonSetState (con_w, on, False);
	}
}

/* called when the Start button is activated.  */
/* ARGSUSED */
static void
camGoCB (Widget w, XtPointer client, XtPointer call)
{
	char *str;
	int n;

	if (expinfo.state != NO_EXP) {
	    msg ("Camera is busy -- Cancel first");
	    return;
	}

	switch (xtype) {
	case XT_NF:
	    str = XmTextFieldGetString (count_w);
	    n = atoi(str);
	    XtFree (str);
	    if (n < 1)
		msg ("Count must be at least 1");
	    else
		startExpose(CONT_EXP, NULL, 0, n);
	    break;

	case XT_CDS:
	    str = XmTextFieldGetString (cdsri_w);
	    startCDS(atoi(str));
	    XtFree (str);
	    expinfo.state = CDSCAN_EXP;
	    break;
	}
}

/* called when the close button is activated.  */
/* ARGSUSED */
static void
closeCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (cam_w);
}

/* called when the main setup dialog is unmapped */
/* ARGSUSED */
static void
unmapCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* stop the mindless timer */
	stopTempTimer();

	/* clean up and close driver */
	resetExpInfo();
}

/* count-down timer callback -- we also check for user cancel */
/* ARGSUSED */
static void
countTO (client, idp)
XtPointer client;
XtIntervalId *idp;
{
	time_t t = time(NULL);
	int dt = expinfo.ctime - t;

	/* check for cancel */
	XCheck(app);
	if (expCancel) {
	    resetExpInfo();
	    expCancel = 0;
	    msg ("Cancelled");
	    if (XtIsManaged(cam_w))
		startTempTimer();
	    return;
	}

	/* restart count down, but not worth it if nearly finished */
	if (expP.duration < 0) {
	    msg ("Triggered exposure %d of %d ... ", expinfo.n+1,
							    expinfo.ntot);
	    expinfo.countTID = XtAppAddTimeOut (app, CD_MS, countTO, 0);
	} else if (dt > 0) {
	    msg ("Exposure %d of %d ... %3d secs remaining", expinfo.n+1,
							    expinfo.ntot, dt);
	    expinfo.countTID = XtAppAddTimeOut (app, CD_MS, countTO, 0);
	} else {
	    if (telstatshmp)
		telstatshmp->camstate = CAM_READ;	/* presumably */
	    msg ("Exposure %d of %d complete. Waiting for pixels...",
	    					expinfo.n+1, expinfo.ntot);
	    expinfo.countTID = (XtIntervalId)0;
	}
}

/* temp polling timer callback */
/* ARGSUSED */
static void
tempTO (client, idp)
XtPointer client;
XtIntervalId *idp;
{
	showCoolerTemp();

	/* repeat */
	expinfo.tempTID = XtAppAddTimeOut (app, TEMP_MS, tempTO, 0);
}

/* start temp timer */
static void
startTempTimer()
{
	/* show now, and later */
	showCoolerTemp();

	if (expinfo.tempTID)
	    XtRemoveTimeOut (expinfo.tempTID);
	expinfo.tempTID = XtAppAddTimeOut (app, TEMP_MS, tempTO, 0);
}

/* stop temp timer */
static void
stopTempTimer()
{
	if (expinfo.tempTID) {
	    XtRemoveTimeOut (expinfo.tempTID);
	    expinfo.tempTID = (XtIntervalId)0;
	}
}

static int fileQual (char *fn, int (*qfp)(), char *type);
static void camInputCB (XtPointer client, int *fdp, XtInputId *idp);
static void nextExpose (void);
static int setExpCharacteristics (void);
static void setupNewFITS (void);
static void saveFITSInfo(void);
static void showNewFITS (void);

/* start taking the first bias frame and save it with the given name and make
 * it the current image. actually we take n of them and average them.
 */
void
startBias (bfn, n)
char *bfn;
int n;
{
	if (expinfo.state != NO_EXP) {
	    msg ("Camera is busy -- Cancel first");
	    return;
	}

	/* override some exposure params that control a bias before
	 * gathering exposure params.
	 */
	XmTextFieldSetString (dur_w, "0");
	XmToggleButtonSetState (shutterC_w, True, True);

	/* go */
	startExpose(BIAS_EXP, bfn, 1, n);
}

/* set up for next intermediate bias and return -1 or finish up and return 0 */
static int
nextBias ()
{
    if(useMeanBias)
    {
        /* accumulate into fpmem */
        nc_accumulate (expinfo.npixels, expinfo.fpmem,
                            (CamPixel *)expinfo.fimage.image);

        /* more? */
        if (expinfo.n < expinfo.ntot)
            return (-1);

        /* this was the last image -- find the average */
        nc_accdiv (expinfo.npixels, expinfo.fpmem, expinfo.ntot);

        /* copy to mem */
        nc_acc2im (expinfo.npixels, expinfo.fpmem,
                            (CamPixel *)expinfo.fimage.image);

    }
    else
    {
        // BiasList allocations/deallocations are handled in startExpose and resetExpImage
        int n, mid;
        CamPixel ** biasList = expinfo.mdmem;
        if(!biasList) {
            msg("Unexpected error -- NULL biasList");
            return 0; //?? Yikes!
        }

        /* do insertion sort of values among bias list arrays per pixel */
        for(n=0; n<expinfo.npixels; n++) {
            int j,k;
            int i = expinfo.n-1; // current image of series
            CamPixel *cp = (CamPixel *) expinfo.fimage.image;
            CamPixel val = cp[n];

            for(j=0; j<i; j++) {
                if((unsigned int) biasList[j][n] > (unsigned int) val)
                    break;
            }
            for(k=i; k>j; k--) {
                if(k<expinfo.ntot) {
                    biasList[k][n] = biasList[k-1][n];
                }
            }
            if(j<expinfo.ntot) {
                biasList[j][n] = val;
            }
        }

        /* more? */
        if (expinfo.n < expinfo.ntot)
            return (-1);

        /* this was the last image -- find the median */
        mid = expinfo.ntot > 2 ? expinfo.ntot / 2 : 0;

        // we now have median values arranged into the middle biasList array
        // move these values into the image
        if(biasList[mid] != NULL) {
            memcpy(expinfo.fimage.image,biasList[mid],expinfo.npixels * sizeof(CamPixel));
        }

    }

    /* add the bias keywords */
    nc_biaskw (&expinfo.fimage, expinfo.ntot);

    /* make it our current display image */
    showNewFITS();

    /* save the new bias image */
    writeImage();

    /* finished */
    return (0);
}

/* create a thermal frame with the given file name and make it the current
 *   image. we subtract off the given bias frame as well.
 * actually we take n of them, each d seconds long, and average them.
 */
void
startThermal (tfn, bfn, n, d)
char *tfn;
char *bfn;
int n;
double d;
{
	char buf[1024];

	if (expinfo.state != NO_EXP) {
	    msg ("Camera is busy -- Cancel first");
	    return;
	}

	/* override some exposure params that control a thermal before
	 * gathering exposure params.
	 */
	XmToggleButtonSetState (shutterC_w, True, True);
	sprintf (buf, "%g", d);
	XmTextFieldSetString (dur_w, buf);

	/* insure we are offered a good bias file */
	if (fileQual (bfn, biasQual, "bias") < 0)
	    return;

	/* save bfn in expinfo */
	strcpy (expinfo.biasfn, bfn);

	/* go */
	startExpose(THERM_EXP, tfn, 1, n);
}

/* set up for next intermediate therm and return -1 or finish up and return 0 */
static int
nextTherm ()
{
    char buf[1024];

    if(useMeanTherm)
    {
        /* accumate into fpmem */
        nc_accumulate (expinfo.npixels, expinfo.fpmem,
                            (CamPixel *)expinfo.fimage.image);

        /* more? */
        if (expinfo.n < expinfo.ntot)
            return (-1);

        /* last image -- find the average */
        nc_accdiv (expinfo.npixels, expinfo.fpmem, expinfo.ntot);
    }
    else
    {
        // ThermList allocations/deallocations are handled in startExpose and resetExpImage
        int n, mid;
        CamPixel ** thermList = expinfo.mdmem;
        if(!thermList) {
            msg("Unexpected error -- NULL thermList");
            return 0; //?? Yikes!
        }

 printf("Insertion\n");
        /* do insertion sort of values among therm list arrays per pixel */
        for(n=0; n<expinfo.npixels; n++) {
            int j,k;
            int i = expinfo.n-1; // current image of series
            CamPixel *cp = (CamPixel *) expinfo.fimage.image;
            CamPixel val = cp[n];

            for(j=0; j<i; j++) {
                if((unsigned int) thermList[j][n] > (unsigned int) val)
                    break;
            }
            for(k=i; k>j; k--) {
                if(k<expinfo.ntot) {
                    thermList[k][n] = thermList[k-1][n];
                }
            }
            if(j<expinfo.ntot) {
                thermList[j][n] = val;
            }
        }

        /* more? */
        if (expinfo.n < expinfo.ntot)
            return (-1);

 printf("Median\n");

        /* this was the last image -- find the median */
        mid = expinfo.ntot > 2 ? expinfo.ntot / 2 : 0;

        // we now have median values arranged into the middle thermList array
        // move these values into the image
        if(thermList[mid] != NULL) {
            memcpy(expinfo.fimage.image,thermList[mid],expinfo.npixels * sizeof(CamPixel));
        }
printf("TO FP\n");
        // Convert to FP so we can use existing code support hereout
        nc_accumulate (expinfo.npixels, expinfo.fpmem,
                            (CamPixel *)expinfo.fimage.image);
    }
printf("Apply Bias\n");
    /* subtract bias */
    msg ("Applying bias correction");
    if (nc_applyBias (expinfo.npixels,expinfo.fpmem,expinfo.biasfn,buf)<0){
        msg ("%s: %s", expinfo.biasfn, buf);
        return (0); /* 0 as in quit */
    }

printf("Copy to Im\n");
    /* copy to mem */
    nc_acc2im (expinfo.npixels, expinfo.fpmem,
                        (CamPixel *)expinfo.fimage.image);
printf("Fits fixup\n");

    /* add the thermal keywords */
    nc_thermalkw (&expinfo.fimage, expinfo.ntot, expinfo.biasfn);

    /* make it our current display image */
    showNewFITS();

    /* save the new thermal image */
    writeImage();

    /* finished */
    return (0);
}

/* create a flat frame with the given file name and make it the current
 *   image.
 * actually we take n of them, each d seconds long, and average them and
 *   correct them with the named biad and thermal files.
 */
void
startFlat (ffn, tfn, bfn, filter, n, d)
char *ffn;	/* name of thermal file to create */
char *tfn;	/* name of thermal to use */
char *bfn;	/* name of bias to use */
int filter;	/* really a char but prototype doesn't work :-Z */
int n;		/* number of images to take */
double d;	/* duration of each, seconds */
{
	char buf[1024];

	if (expinfo.state != NO_EXP) {
	    msg ("Camera is busy -- Cancel first");
	    return;
	}

	/* override some exposure params that control a flat before
	 * gathering exposure params.
	 */
	XmToggleButtonSetState (shutterO_w, True, True);
	sprintf (buf, "%g", d);
	XmTextFieldSetString (dur_w, buf);

	/* save filter */
	expinfo.filter = filter;

	/* insure were offered a good bias file */
	if (fileQual (bfn, biasQual, "bias") < 0)
	    return;

	/* save bfn in expinfo */
	strcpy (expinfo.biasfn, bfn);

	/* insure were offered a good thermal file */
	if (fileQual (tfn, thermQual, "thermal") < 0)
	    return;

	/* save tfn in expinfo */
	strcpy (expinfo.thermfn, tfn);

	/* go */
	startExpose(FLAT_EXP, ffn, 1, n);
}

/* set up for next intermediate flat and return -1 or finish up and return 0 */
static int
nextFlat ()
{
    char buf[1024];
    int s;

    if(useMeanFlat)
    {
        /* accumate into fpmem */
        nc_accumulate (expinfo.npixels, expinfo.fpmem,
                            (CamPixel *)expinfo.fimage.image);

        /* more? */
        if (expinfo.n < expinfo.ntot)
            return (-1);

        /* this was the last image -- find the average */
        nc_accdiv (expinfo.npixels, expinfo.fpmem, expinfo.ntot);
    }
    else
    {
        // FlatList allocations/deallocations are handled in startExpose and resetExpImage
        int n, mid;
        CamPixel ** flatList = expinfo.mdmem;
        if(!flatList) {
            msg("Unexpected error -- NULL flatList");
            return 0; //?? Yikes!
        }

        /* do insertion sort of values among flat list arrays per pixel */
        for(n=0; n<expinfo.npixels; n++) {
            int j,k;
            int i = expinfo.n-1; // current image of series
            CamPixel *cp = (CamPixel *) expinfo.fimage.image;
            CamPixel val = cp[n];

            for(j=0; j<i; j++) {
                if((unsigned int) flatList[j][n] > (unsigned int) val)
                    break;
            }
            for(k=i; k>j; k--) {
                if(k<expinfo.ntot) {
                    flatList[k][n] = flatList[k-1][n];
                }
            }
            if(j<expinfo.ntot) {
                flatList[j][n] = val;
            }
        }

        /* more? */
        if (expinfo.n < expinfo.ntot)
            return (-1);

        /* this was the last image -- find the median */
        mid = expinfo.ntot > 2 ? expinfo.ntot / 2 : 0;

        // we now have median values arranged into the middle thermList array
        // move these values into the image
        if(flatList[mid] != NULL) {
            memcpy(expinfo.fimage.image,flatList[mid],expinfo.npixels * sizeof(CamPixel));
        }
        // Convert to FP so we can use existing code support hereout
        nc_accumulate (expinfo.npixels, expinfo.fpmem,
                            (CamPixel *)expinfo.fimage.image);
    }
	/* subtract bias */
	msg ("Applying bias correction");
	s = nc_applyBias (expinfo.npixels, expinfo.fpmem, expinfo.biasfn, buf);
	if (s < 0){
	    msg ("%s: %s", expinfo.biasfn, buf);
	    return (0);	/* 0 as in quit */
	}

	/* subtract thermal */
	msg ("Applying thermal correction");
	s = nc_applyThermal (expinfo.npixels, expinfo.fpmem,
				    expP.duration/1000.0, expinfo.thermfn, buf);
	if (s < 0) {
	    msg ("%s: %s", expinfo.thermfn, buf);
	    return (0);	/* 0 as in quit */
	}

	/* load into mem */
	nc_acc2im (expinfo.npixels, expinfo.fpmem,
					    (CamPixel *)expinfo.fimage.image);

	/* add the flat keywords */
	nc_flatkw (&expinfo.fimage, expinfo.ntot, expinfo.biasfn,
					    expinfo.thermfn, expinfo.filter);

	/* make it our current display image */
	showNewFITS();

	/* save the new flat image */
	writeImage ();

	/* finished */
	return (0);
}

/* set up for next continuous and return -1 or finish up and return 0 */
static int
nextCont()
{
	/* save if enabled */
	setSaveName();
	if (saveAuto() == 0)
	    writeImage();

	/* more? */
	if (expinfo.n < expinfo.ntot) {
	    if (saveAuto() == 0) {
		if (mkTemplateName (expinfo.newfn) == 0)
		    setSaveName();
	    }
	    return (-1);
	}

	/* finished */
	return (0);
}

/* start an exposure according to the current widget settings and expinfo.
 * arrange for camInputCB to be called when the pixels are ready.
 * we return void but we still use msg() if error and always reset everything.
 */
static void
startExpose (state, fn, makefpmem, ntot)
ExpState state;
char *fn;
int makefpmem;
int ntot;
{
	/* send exposure characteristics to driver for validation.
	 * it will remember them for us.
	 */
	if (setExpCharacteristics() < 0)
	    return;

	/* create a new FITS array to read raw pixels into */
	expinfo.npixels = (expP.sw/expP.bx) * (expP.sh/expP.by);
	if (expinfo.npixels == 0) {
	    msg ("No pixels in that size image!");
	    return;
	}
	setupNewFITS();

	/* save the new filename, if given, else fetch from save system */
	if (fn)
	    (void) strncpy (expinfo.newfn, fn, sizeof(expinfo.newfn)-1);
	else if (saveAuto() == 0) {
	    if (mkTemplateName (expinfo.newfn) < 0) {
		resetExpInfo();
		return;	/* already explained */
	    }
	} else {
	    if (getSaveName (expinfo.newfn) < 0) {
		resetExpInfo();
		return; /* already explained */
	    }
	}

	/* make the fpmem array if desired */
	if(!useMeanBias && state == BIAS_EXP) {
		CamPixel ** biasList;
		// allocate the biasList
		biasList = (CamPixel **) calloc(ntot,sizeof(CamPixel **));
		if(biasList) {
			int i;
			expinfo.mdmem = biasList;
			// Make the individual allocations to collect sorted pixel values
			for(i=0; i<ntot; i++) {
				biasList[i] = NULL; // clear first
			}
			for(i=0; i<ntot; i++) {
				biasList[i] = (CamPixel *) calloc(expinfo.npixels,sizeof(CamPixel));
				if(!biasList[i]) {
					msg("Failed allocating pixel memory for bias image %d",i);
					resetExpInfo();
					return;
				}
			}
		}
		else {
			msg("Can not malloc biasList allocation");
			resetExpInfo();
			return;
		}
	}
	else
	{
        if (makefpmem && !(expinfo.fpmem = nc_makeFPAccum(expinfo.npixels))) {
            msg ("Can not malloc temp fp accumulator");
            resetExpInfo();
            return;
        }

        if(!useMeanTherm && state == THERM_EXP) {
            CamPixel ** thermList;
            // allocate the thermList
            thermList = (CamPixel **) calloc(ntot,sizeof(CamPixel **));
            if(thermList) {
                int i;
                expinfo.mdmem = thermList;
                // Make the individual allocations to collect sorted pixel values
                for(i=0; i<ntot; i++) {
                    thermList[i] = NULL; // clear first
                }
                for(i=0; i<ntot; i++) {
                    thermList[i] = (CamPixel *) calloc(expinfo.npixels,sizeof(CamPixel));
                    if(!thermList[i]) {
                        msg("Failed allocating pixel memory for therm image %d",i);
                        resetExpInfo();
                        return;
                    }
                }
            }
            else {
                msg("Can not malloc thermList allocation");
                resetExpInfo();
                return;
            }
        }
        if(!useMeanFlat && state == FLAT_EXP) {
            CamPixel ** flatList;
            // allocate the flatList
            flatList = (CamPixel **) calloc(ntot,sizeof(CamPixel **));
            if(flatList) {
                int i;
                expinfo.mdmem = flatList;
                // Make the individual allocations to collect sorted pixel values
                for(i=0; i<ntot; i++) {
                    flatList[i] = NULL; // clear first
                }
                for(i=0; i<ntot; i++) {
                    flatList[i] = (CamPixel *) calloc(expinfo.npixels,sizeof(CamPixel));
                    if(!flatList[i]) {
                        msg("Failed allocating pixel memory for flat image %d",i);
                        resetExpInfo();
                        return;
                    }
                }
            }
            else {
                msg("Can not malloc flatList allocation");
                resetExpInfo();
                return;
            }
        }
	}
	/* init state and count */
	expinfo.state = state;
	expinfo.ntot = ntot;
	expinfo.n = 0;

	/* stop polling the temp */
	stopTempTimer();

	/* go */
	nextExpose();
}

/* called when pixels are ready.
 * what we do depends on expinfo.
 */
/* ARGSUSED */
static void
camInputCB (client, fdp, idp)
XtPointer client;
int *fdp;		/* pointer to file descriptor */
XtInputId *idp;		/* pointer to what AddInput returned */
{
	int switchState;
	switchState = getAutoCorrectState();

	/* turn on watch .. don't forget to turn back off :-) */
	watch_cursor(1);

	/* turn off count-down timer */
	if (expinfo.countTID) {
	    XtRemoveTimeOut (expinfo.countTID);
	    expinfo.countTID = (XtIntervalId)0;
	}

	/* check for user cancel */
	XCheck(app);
	if (expCancel) {
	    expCancel = 0;
	    expinfo.state = NO_EXP;
	    msg ("Cancelled");
	} else {
	    int i;

	    /* grab the time and other info now right at end of exposure */
	    saveFITSInfo();

	    /* read pixels from and close driver, remove from X input list */
	    msg ("Reading image %d of %d ...", expinfo.n + 1, expinfo.ntot);
	    if (readPixels () < 0) {
		resetExpInfo();
		watch_cursor(0);
		return;
	    }
	    XtRemoveInput (expinfo.iid);
	    expinfo.iid = (XtInputId)0;

		// now we need to hack a little... we don't want to allow applyCorr
		// to do corrections on the correction images... this used to be handled
		// by turning off the 'auto apply corrections' switch, but I've changed that
		// so this stays on... so we need to turn it off for correction images
		// and then back on again...
		if(expinfo.state == BIAS_EXP
		|| expinfo.state == THERM_EXP
		|| expinfo.state == FLAT_EXP)
		{
			setAutoCorrectState(False);
		}

	    /* increment count */
	    expinfo.n += 1;

	    /* show, and beep if this was a long one */
	    setFName (expinfo.newfn);
	    showNewFITS();
	    if (expP.duration/1000 >= BEEP_TIME)
		XBell (XtDisplay(toplevel_w), 0);

	    /* process and repeat as required */
	    i = 0;
	    switch (expinfo.state) {
	    case NO_EXP:
		break;
	    case CONT_EXP:
		i = nextCont();
		break;
	    case BIAS_EXP:
		i = nextBias();
		break;
	    case THERM_EXP:
		i = nextTherm();
		break;
	    case FLAT_EXP:
		i = nextFlat();
		break;
	    case CDSCAN_EXP:
		printf ("Pixel during drift scan!\n");
		exit (1);
		break;
	    }

	    /* finished, or do another */
	    if (i == 0)
		expinfo.state = NO_EXP;
	    else
		nextExpose();
	}

	/* when finished, reset temp info and start cooler loop if up */
	if (expinfo.state == NO_EXP) {
	    resetExpInfo();
	    if (XtIsManaged(cam_w))
		startTempTimer();
	}

	// put it back and play nice..
	setAutoCorrectState(switchState);

	/* watch back off for now */
	watch_cursor(0);
}

/* start the real exposure.
 * we don't return anything but we use msg() and resetExpInfo() if we fail.
 */
static void
nextExpose()
{
	char errmsg[1024];
	int ccd_fd;

	/* start the actual exposure now */
	msg ("Starting exposure %d of %d ...", expinfo.n+1, expinfo.ntot);

	(void) sprintf (errmsg, "%d", expinfo.ntot - expinfo.n);
	XmTextFieldSetString (count_w, errmsg);
	XSync (XtDisplay(toplevel_w), 0);
	if (telstatshmp)
	    telstatshmp->camstate = CAM_EXPO;
	if (startExpCCD (errmsg) < 0) {
	    msg ("CCD driver setup error: %s", errmsg);
	    resetExpInfo();
	}

	/* arrange to read pixels from ccd_fd when they are ready */
	ccd_fd = selectHandleCCD(errmsg);
	if (ccd_fd < 0) {
	    msg ("Camera handle: %s", errmsg);
	    resetExpInfo();
	    return;
	}
	expinfo.iid = XtAppAddInput (app, ccd_fd, (XtPointer)XtInputReadMask,
							    camInputCB, 0);

	/* start count-down timer */
	expinfo.ctime = time(NULL) + expP.duration/1000;
	if (expinfo.countTID)
	    XtRemoveTimeOut (expinfo.countTID);
	expinfo.countTID = XtAppAddTimeOut (app, 0, countTO, 0);
}

static void
resetExpInfo()
{
	if (expinfo.iid) {
	    XtRemoveInput (expinfo.iid);
	    expinfo.iid = (XtInputId)0;
	}
	if (expinfo.mdmem) {

        CamPixel ** cp = expinfo.mdmem;
        int i;
        for(i=0; i<expinfo.ntot; i++) {
            if(cp[i]) free(cp[i]);
        }
	    free ((char *)expinfo.mdmem);
	    expinfo.mdmem = NULL;
	}
	if (expinfo.fpmem) {
        free ((char *)expinfo.fpmem);
        expinfo.fpmem = NULL;
	}
	resetFImage (&expinfo.fimage);
	expinfo.n = 0;
	expinfo.ntot = 0;
	expinfo.npixels = 0;
	expinfo.state = NO_EXP;

	/* close driver */
	abortExpCCD();
	if (telstatshmp)
	    telstatshmp->camstate = CAM_IDLE;
}

/* using qfp, see if fn can be used to correct the format described in expP.
 * if not give msg and return -1, else 0 if ok.
 */
static int
fileQual (fn, qfp, type)
char *fn;	/* file to check */
int (*qfp)();	/* qualifier function to use */
char *type;	/* name of goal, for error report */
{
	char errmsg[1024];
	int fd;
	FImage im;
	FImage expim;
	int result;

	fd = open (fn, O_RDONLY);
	if (fd < 0) {
	    msg ("%s: %s", fn, strerror(errno));
	    return (-1);
	}
	if (readFITSHeader (fd, &im, errmsg) < 0) {
	    msg ("%s: %s", fn, errmsg);
	    (void) close (fd);
	    return (-1);
	}
	(void) close (fd);

	/* conjure up an FImage from expP so we can use the qual function */
	initFImage (&expim);
	expim.bitpix = 16;
	expim.sw = expP.sw/expP.bx;	/* wants net image size */
	expim.sh = expP.sh/expP.by;	/* wants net image size */
	expim.sx = expP.sx;
	expim.sy = expP.sy;
	expim.bx = expP.bx;
	expim.by = expP.by;
	expim.dur = abs(expP.duration);
	setSimpleFITSHeader(&expim);

	/* run the qual function */
	result = (*qfp)(&expim, &im, errmsg);
	resetFImage (&expim);
	resetFImage (&im);
	if (result < 0) {
	    msg ("%s: Bad %s: %s", basenm(fn), type, errmsg);
	    return (-1);
	}

	return (0);
}

/* get the exposure params from the X widgets into expP. */
static void
gatherExpParams()
{
	typedef struct {
	    Widget *wp;		/* pointer to widget to get text string from */
	    int *vp;		/* pointer to where to put the atoi value */
	} WIParams;
	static WIParams wiparams[] = {
	    {&sx_w, &expP.sx},
	    {&sy_w, &expP.sy},
	    {&sw_w, &expP.sw},
	    {&sh_w, &expP.sh},
	    {&bx_w, &expP.bx},
	    {&by_w, &expP.by},
	};
	char *str;
	int i;

	/* gather the integer parameters */

	for (i = 0; i < XtNumber(wiparams); i++) {
	    WIParams *wip = &wiparams[i];
	    str = XmTextFieldGetString (*wip->wp);
	    *wip->vp = atoi(str);
	    XtFree (str);
	}

	/* guard against 0 binning values here */

	if (expP.bx < 1) {
	    expP.bx = 1;
	    XmTextFieldSetString (bx_w, "1");
	}
	if (expP.by < 1) {
	    expP.by = 1;
	    XmTextFieldSetString (by_w, "1");
	}

	/* gather the remaining odd-ball parameters */

	str = XmTextFieldGetString (dur_w);
	expP.duration = (int)(atof(str) * 1000.0);	/* secs to ms */
	XtFree (str);

	expP.shutter = XmToggleButtonGetState (shutterC_w) ? CCDSO_Closed :
		       XmToggleButtonGetState (shutterD_w) ? CCDSO_Dbl :
		       XmToggleButtonGetState (shutterM_w) ? CCDSO_Multi :
		       CCDSO_Open;
}

/* send expP to the driver.
 * return 0 if ok else tell user via msg() and return -1.
 */
static int
setExpCharacteristics()
{
	char errmsg[1024];

	gatherExpParams();
	if (setExpCCD (&expP, errmsg) < 0) {
	    msg ("%s", errmsg);
	    return (-1);
	}

	return (0);
}

/* read the real pixels.
 * return 0 if else use msg() and return -1.
 */
static int
readPixels()
{
	char errmsg[1024];
	int nbytes;
	int s;

	nbytes = expinfo.npixels * sizeof(CamPixel);
	if (telstatshmp)
	    telstatshmp->camstate = CAM_READ;
	s = readPixelCCD (expinfo.fimage.image, nbytes, errmsg);
	if (telstatshmp)
	    telstatshmp->camstate = CAM_IDLE;
	if (s < 0) {
	    msg ("Camera read error: %s", errmsg);
	    return (-1);
	}

	/* affect any desired flipping */
	flipImg ();

	return (0);
}

/* implement any required/desired image row and/or column flipping.
 * N.B. for germal eq in flip mode, we add an extra 180 rot via 2 flips
 */
static void
flipImg()
{
	FImage *fip = &expinfo.fimage;
	int geflip;
	int flip;

	if (telstatshmp)
	    geflip = telstatshmp->tax.GERMEQ && telstatshmp->tax.GERMEQ_FLIP;
	else
	    geflip = 0;

	flip = XmToggleButtonGetState (fliplr_w);
	if (geflip)
	    flip = !flip;
	if (flip)
	    flipImgCols ((CamPixel *)fip->image, fip->sw, fip->sh);
	flip = XmToggleButtonGetState (fliptb_w);
	if (geflip)
	    flip = !flip;
	if (flip)
	    flipImgRows ((CamPixel *)fip->image, fip->sw, fip->sh);
}

/* reset state.fimage with expinfo.fimage and show */
static void
showNewFITS()
{
	FImage *sfip = &state.fimage;
	FImage *efip = &expinfo.fimage;
	char *newpix;
	char *newvar;
	int nbytes;

	/* clean out the current first */
	resetFImage (sfip);

	/* copy the pixels and var so we're not tied to expinfo */
	nbytes = expinfo.npixels * sizeof(CamPixel);
	newpix = malloc (nbytes);
	if (!newpix) {
	    printf ("Can't malloc %d for new image\n", nbytes);
	    exit(1);
	}
	memcpy (newpix, (char *)efip->image, nbytes);
	nbytes = efip->nvar * sizeof(FITSRow);
	newvar = malloc (nbytes);
	if (!newvar) {
	    printf ("Can't malloc %d for new image variables\n", nbytes);
	    exit(1);
	}
	memcpy (newvar, (char *)efip->var, nbytes);

	/* copy fields from exp, but then use new copies */
	*sfip = *efip;
	sfip->var = (FITSRow *)newvar;
	sfip->image = newpix;

	/* show it */
	presentNewImage();
}

/* save all current info in expinfo.fimage */
static void
saveFITSInfo()
{
	FImage *fip = &expinfo.fimage;
	CCDTempInfo tinfo;

	timeStampFITS (fip, /*time(NULL)*/0, "Time at end of exposure");
	if (telstatshmp)
	    addShmFITS (fip, telstatshmp);
	else {
	    char buf[10];
	    getCorrFilter (buf);
	    setStringFITS (fip, "FILTER", buf, "Filter code");
	}

	if (getCoolerTemp (&tinfo) == 0) {
	    setIntFITS (fip, "CAMTEMP", tinfo.t, "Camera temp, C");
	    showCoolerTemp();	/* show it too */
	}
}

/* set up expinfo.fimage for a new run */
static void
setupNewFITS()
{
	FImage *fip = &expinfo.fimage;
	int nbytes;

	resetFImage (fip);

	nbytes = expinfo.npixels * sizeof(CamPixel);
	fip->image = calloc (nbytes, 1);	/* 0 to reveal count bugs */
	if (!fip->image) {
	    printf ("Can not get %d bytes for temp image\n", nbytes);
	    exit(1);
	}

	fip->bitpix = 16;
	fip->sw = expP.sw/expP.bx;
	fip->sh = expP.sh/expP.by;
	fip->sx = expP.sx;
	fip->sy = expP.sy;
	fip->bx = expP.bx;
	fip->by = expP.by;
	fip->dur = abs(expP.duration);

	/* fill in some FITS header fields  */
	setSimpleFITSHeader (fip);
	if (orig_kw[0] != '\0')
	    setStringFITS (fip, "ORIGIN", orig_kw, NULL);
	if (tele_kw[0] != '\0')
	    setStringFITS (fip, "TELESCOP", tele_kw, NULL);

	if (VPIXSZ > 0 && HPIXSZ > 0) {
	    int sign;

	    sign = XmToggleButtonGetState (fliplr_w) == RALEFT ? 1 : -1;
	    setRealFITS (fip, "CDELT1", HPIXSZ*expP.bx * sign, 10,
						"RA step right, degrees/pixel");

	    sign = XmToggleButtonGetState (fliptb_w) == DECUP  ? 1 : -1;
	    setRealFITS (fip, "CDELT2", VPIXSZ*expP.by * sign, 10,
						"Dec step down, degrees/pixel");
	}
	if (cam_id[0] != '\0')
	    setStringFITS (fip, "INSTRUME", cam_id, NULL);
}

/* fetch the camera id string */
static int
getCameraID(char buf[])
{
	char errmsg[1024];

	if (getIDCCD (buf, errmsg) < 0) {
	    msg ("%s", errmsg);
	    return (-1);
	}
	return (0);
}

/* set cooler info and report state */
static void
setCoolerTemp(CCDTempInfo *tp)
{
	char errmsg[1024];

	if (setTempCCD (tp, errmsg) < 0)
	    msg ("%s", errmsg);
	else if (tp->s == CCDTS_SET) {
	    msg ("Cooling to %d", tp->t);
	    if (telstatshmp)
		telstatshmp->camtarg = tp->t;
	} else
	    msg ("Cooler turned off");
}

/* fetch cooler info.
 * return 0 if ok, else -1.
 * put into shared memory if available.
 */
static int
getCoolerTemp(CCDTempInfo *tp)
{
	char errmsg[1024];

	if (getTempCCD (tp, errmsg) < 0) {
	    msg ("%s", errmsg);
	    return (-1);
	}
	if (telstatshmp) {
	    telstatshmp->camtemp = tp->t;
	    telstatshmp->coolerstatus = tp->s;
	}
	return (0);
}

/* called when RETURN is hit over drift scan rate text field */
/* ARGSUSED */
static void
cdsriCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char *str;

	/* don't do anything if not currently scanning */
	if (expinfo.state != CDSCAN_EXP)
	    return;

	/* set new row interval */
	str = XmTextFieldGetString (w);
	dsNewInterval(atoi(str));
	XtFree (str);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: camera.c,v $ $Date: 2002/12/21 00:32:49 $ $Revision: 1.4 $ $Name:  $"};
