/* dialog to take and control applying bias/thermal/flat corrections.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/TextF.h>

#include "fits.h"
#include "fitscorr.h"
#include "fieldstar.h"
#include "telenv.h"
#include "strops.h"
#include "camera.h"
#include "xtools.h"

static Widget createSep (Widget top);
static Widget createApply(void);
static Widget createDir(Widget top);
static Widget createBadMap (Widget top);
static Widget createBias (Widget top);
static Widget createTherm (Widget top);
static Widget createFlat (Widget top);
static Widget createFileControls (Widget top, void (*takeCB)(),
    Widget *newFN_wp, void (*scanNewCB)(), Widget *FN_wp, void
    (*scanLastCB)(), Widget *auto_wp);
static void createClose (Widget top);

static void scanDirCB (Widget w, XtPointer client, XtPointer call);
static void takeBiasCB (Widget w, XtPointer client, XtPointer call);
static void scanNewBiasCB (Widget w, XtPointer client, XtPointer call);
static void scanLastBiasCB (Widget w, XtPointer client, XtPointer call);
static void takeThermCB (Widget w, XtPointer client, XtPointer call);
static void scanNewThermCB (Widget w, XtPointer client, XtPointer call);
static void scanLastThermCB (Widget w, XtPointer client, XtPointer call);
static void takeFlatCB (Widget w, XtPointer client, XtPointer call);
static void mkFakeFlat (char *ffn, char filter, int n, double d);
static void scanNewFlatCB (Widget w, XtPointer client, XtPointer call);
static void scanLastFlatCB (Widget w, XtPointer client, XtPointer call);
static void scanLastBadCol (void);
static void closeCB (Widget w, XtPointer client, XtPointer call);

static void scanAllFiles(void);
static void getFilename (char fn[], Widget w);
static void getString (char s[], Widget w);
static int getInt (Widget w);
static double getReal (Widget w);

static Widget apply_w;		/* whether to automatically apply corrections */
static Widget dirFN_w;		/* directory for all files */

static Widget badColMapFN_w;	/* bad column map file to use */

static Widget biasFN_w;		/* current bias file to use */
static Widget newBiasFN_w;	/* new bias file to create */
static Widget newBiasN_w;	/* number of bias frames to average */
static Widget autoBias_w;	/* True when automatically choose bias name */

static Widget thermFN_w;	/* current therm file to use */
static Widget newThermFN_w;	/* new thermal file to create */
static Widget newThermN_w;	/* number of thermal frames to average */
static Widget newThermD_w;	/* duration of each thermal frame */
static Widget autoTherm_w;	/* True when automatically choose therm name */

static Widget flatFN_w;		/* current flat file to use */
static Widget newFlatFN_w;	/* new flat file to create */
static Widget newFlatF_w;	/* filter type of new flat file */
static Widget newFlatD_w;	/* duration of each flat file */
static Widget newFlatN_w;	/* number of flats */
static Widget autoFlat_w;	/* True when automatically choose flat name */
static Widget fakeFlat_w;	/* True when we really want a fake flat */

static Widget corr_w;	/* main correction dialog */

void
manageCorr ()
{
	if (!corr_w)
	    createCorr();

	if (XtIsManaged(corr_w))
	    raiseShell (corr_w);
	else
	    XtManageChild (corr_w);
}

void
createCorr()
{
	Widget w;
	Arg args[20];
	int n;

	/* create form */
	
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 5); n++;
	XtSetArg (args[n], XmNmarginWidth, 5); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	corr_w = XmCreateFormDialog (toplevel_w, "Corr", args,n);
	
	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Corrections Control"); n++;
	XtSetValues (XtParent(corr_w), args, n);
	XtVaSetValues (corr_w, XmNcolormap, camcm, NULL);

	w = createApply ();
	w = createSep (w);
	w = createDir (w);
	w = createBadMap(w);
	w = createBias (w);
	w = createTherm (w);
	w = createFlat (w);
	createClose (w);
}

/* if enabled, get the current correction filenames and apply them to the
 * current image
 */
void
applyCorr()
{
	char errmsg[1024];
	char caldir[1024];
	char biasfn[1024];
	char thermfn[1024];
	char flatfn[1024];
	
	if (!XmToggleButtonGetState (apply_w))
	    return;

	getString (caldir, dirFN_w);

	/* gather correction file names; can be manual or automatic */
	if (XmToggleButtonGetState (autoBias_w)) {
	    if (findBiasFN (&state.fimage, caldir, biasfn, errmsg) < 0) {
		msg ("Bias file error: %s", errmsg);
		XmToggleButtonSetState (apply_w, False, True);
		return;
	    }
	    XmTextFieldSetString (biasFN_w, basenm(biasfn));
	} else 
	    getFilename (biasfn, biasFN_w);

	if (XmToggleButtonGetState (autoTherm_w)) {
	    if (findThermFN (&state.fimage, caldir, thermfn, errmsg) < 0) {
		msg ("Thermal file error: %s", errmsg);
		XmToggleButtonSetState (apply_w, False, True);
		return;
	    }
	    XmTextFieldSetString (thermFN_w, basenm(thermfn));
	} else 
	    getFilename (thermfn, thermFN_w);

	if (XmToggleButtonGetState (autoFlat_w)) {
	    char fkw[100];
	    int f;

	    /* pick filter.
	     * use the current user setting if image has none
	     */
	    if (getStringFITS (&state.fimage, "FILTER", fkw) < 0)
		getString (fkw, newFlatF_w);
	    f = fkw[0];
	    if (findFlatFN (&state.fimage, f, caldir, flatfn, errmsg) < 0) {
		msg ("Flat file error: %s", errmsg);
		XmToggleButtonSetState (apply_w, False, True);
		return;
	    }
	    XmTextFieldSetString (flatFN_w, basenm(flatfn));
	} else 
	    getFilename (flatfn, flatFN_w);
	
	/* Do corrections */
	if (correctFITS (&state.fimage, biasfn, thermfn, flatfn, errmsg) < 0) {
	    msg ("Correction error: %s", errmsg);
	}
	else {
		msg ("Corrections applied");	// put this up in case we aren't correcting bad columns
		if(0 == fixBadColumns(&state.fimage)) {		
			msg ("Corrections applied");	// refresh this message on success, else leave error message standing
		}
	}
}	

// external read of correction state
Bool getAutoCorrectState(void)
{
	return XmToggleButtonGetState(apply_w);
}
// external set of correction state
void setAutoCorrectState(Bool state)
{
	XmToggleButtonSetState(apply_w,state,True);
}	

/* Apply bad column correction.
 * return 0 if we fixed column, else return < 0 and display message if any relevant
*/
int fixBadColumns(FImage * fip)
{
	char errmsg[1024];
	char caldir[1024];
	char badmapfn[1024];
	char mapnameused[1024];
	BADCOL *badColMap;
	int  rt;

	if (!XmToggleButtonGetState (apply_w))
	    return -1;
		
	getString (caldir, dirFN_w);

	/* get name of bad column map. Use the name in edit box, but if one not there, scan for it and put it there. */
	getFilename(badmapfn, badColMapFN_w);
		
    if(!basenm(badmapfn) || !strcmp(basenm(badmapfn),"")) {
	  if(findMapFN(caldir,badmapfn,errmsg) < 0) {
//	    msg("Bad Column Map file error: %s", errmsg);
//	    XmToggleButtonSetState(apply_w,False,True);
	    return -1;
	  }
	  XmTextFieldSetString(badColMapFN_w,basenm(badmapfn));
	}	
	
	msg("Removing bad columns");
		
	// correct bad columns
	rt = readMapFile(NULL, badmapfn, &badColMap,mapnameused,errmsg);
	if(rt > 0) {
	  rt = removeBadColumns(fip,badColMap,mapnameused,errmsg);
	  free(badColMap);
	}
	if(rt < 0) {
	    msg ("Bad Column Fix error: %s", errmsg);
	}
	return rt;
}

/* return the current Filter setting */
void
getCorrFilter (char *buf)
{
	getString (buf, newFlatF_w);
	if (islower(buf[0]))
	    buf[0] = toupper(buf[0]);
	buf[1] = '\0';
}

static Widget
createSep (top)
Widget top;
{
	Widget sep_w;
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (corr_w, "Sep", args, n);
	XtManageChild (sep_w);

	return (sep_w);
}

static Widget
createApply()
{
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNtopOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	apply_w = XmCreateToggleButton (corr_w, "Apply", args, n);
	XtManageChild (apply_w);
	wlprintf (apply_w, "Automatically apply corrections to new images");

	return (apply_w);
}

static Widget
createDir(top)
Widget top;
{
	Widget l_w, pb_w;
	char buf[1024];
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	l_w = XmCreateLabel (corr_w, "DirL", args, n);
	XtManageChild (l_w);
	wlprintf (l_w, "Directory:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	pb_w = XmCreatePushButton (corr_w, "DirPB", args, n);
	XtAddCallback (pb_w, XmNactivateCallback, scanDirCB, NULL);
	XtManageChild (pb_w);
	wlprintf (pb_w, "Rescan All");

	telfixpath (buf, "archive/calib");
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, pb_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, l_w); n++;
	XtSetArg (args[n], XmNcolumns, strlen(buf)); n++;
	XtSetArg (args[n], XmNvalue, buf); n++;
	dirFN_w = XmCreateTextField (corr_w, "DirFN", args, n);
	XtAddCallback (dirFN_w, XmNactivateCallback, scanDirCB, NULL);
	XtManageChild (dirFN_w);

	return (dirFN_w);
}

/* STO20010420 */
/* Add support for bad column fix in camera corrections dialog */
static Widget
createBadMap (top)
Widget top;
{
  	Widget t_w,xw;
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top); n++;
	XtSetArg (args[n], XmNtopOffset, 20); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	t_w = XmCreateLabel (corr_w, "BadMapL", args, n);
	XtManageChild (t_w);
	wlprintf (t_w, "Bad Column Map:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 45); n++;
	xw = XmCreateLabel (corr_w, "BadMapLN", args, n);
	XtManageChild (xw);
	wlprintf (xw, "Current file name:");
	
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, xw); n++;
	XtSetArg (args[n], XmNcolumns, 20); n++;
	XtSetArg (args[n], XmNmaxLength,20); n++;
	badColMapFN_w = XmCreateTextField (corr_w, "BadColN", args, n);
	XtManageChild (badColMapFN_w);


	return (badColMapFN_w);
}
/*****************/

static Widget
createBias (top)
Widget top;
{
	Widget t_w, l_w;
	Widget w;
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	t_w = XmCreateLabel (corr_w, "BiasL", args, n);
	XtManageChild (t_w);
	wlprintf (t_w, "Bias:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 20); n++;
	l_w = XmCreateLabel (corr_w, "BiasNL", args, n);
	XtManageChild (l_w);
	wlprintf (l_w, "Count:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, l_w); n++;
	XtSetArg (args[n], XmNcolumns, 4); n++;
	XtSetArg (args[n], XmNmaxLength, 4); n++;
	newBiasN_w = XmCreateTextField (corr_w, "BiasN", args, n);
	XtManageChild (newBiasN_w);

	w = createFileControls (newBiasN_w, takeBiasCB, &newBiasFN_w,
			scanNewBiasCB, &biasFN_w, scanLastBiasCB, &autoBias_w);

	return (w);
}

static Widget
createTherm (top)
Widget top;
{
	Widget t_w, l_w;
	Widget w;
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	t_w = XmCreateLabel (corr_w, "ThermL", args, n);
	XtManageChild (t_w);
	wlprintf (t_w, "Thermal:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 20); n++;
	l_w = XmCreateLabel (corr_w, "ThermNL", args, n);
	XtManageChild (l_w);
	wlprintf (l_w, "Count:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, l_w); n++;
	XtSetArg (args[n], XmNcolumns, 4); n++;
	XtSetArg (args[n], XmNmaxLength, 4); n++;
	newThermN_w = XmCreateTextField (corr_w, "ThermN", args, n);
	XtManageChild (newThermN_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, newThermN_w); n++;
	l_w = XmCreateLabel (corr_w, "ThermDL", args, n);
	XtManageChild (l_w);
	wlprintf (l_w, "Duration:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, l_w); n++;
	XtSetArg (args[n], XmNcolumns, 4); n++;
	XtSetArg (args[n], XmNmaxLength, 4); n++;
	newThermD_w = XmCreateTextField (corr_w, "ThermD", args, n);
	XtManageChild (newThermD_w);

	w = createFileControls (newThermN_w, takeThermCB, &newThermFN_w,
		    scanNewThermCB, &thermFN_w, scanLastThermCB, &autoTherm_w);

	return (w);
}

static Widget
createFlat (top)
Widget top;
{
	Widget t_w, l_w;
	Widget w;
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	t_w = XmCreateLabel (corr_w, "FlatLL", args, n);
	XtManageChild (t_w);
	wlprintf (t_w, "Flat:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 20); n++;
	l_w = XmCreateLabel (corr_w, "FlatNL", args, n);
	XtManageChild (l_w);
	wlprintf (l_w, "Count:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, l_w); n++;
	XtSetArg (args[n], XmNcolumns, 4); n++;
	XtSetArg (args[n], XmNmaxLength, 4); n++;
	newFlatN_w = XmCreateTextField (corr_w, "FlatN", args, n);
	XtManageChild (newFlatN_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, newFlatN_w); n++;
	l_w = XmCreateLabel (corr_w, "FlatDL", args, n);
	XtManageChild (l_w);
	wlprintf (l_w, "Duration:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, l_w); n++;
	XtSetArg (args[n], XmNcolumns, 4); n++;
	XtSetArg (args[n], XmNmaxLength, 4); n++;
	newFlatD_w = XmCreateTextField (corr_w, "FlatD", args, n);
	XtManageChild (newFlatD_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, newFlatD_w); n++;
	l_w = XmCreateLabel (corr_w, "FlatFL", args, n);
	XtManageChild (l_w);
	wlprintf (l_w, "Filter:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, l_w); n++;
	XtSetArg (args[n], XmNcolumns, 1); n++;
	XtSetArg (args[n], XmNmaxLength, 1); n++;
	newFlatF_w = XmCreateTextField (corr_w, "FlatF", args, n);
	XtManageChild (newFlatF_w);

	w = createFileControls (newFlatD_w, takeFlatCB, &newFlatFN_w,
			scanNewFlatCB, &flatFN_w, scanLastFlatCB, &autoFlat_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, newFlatFN_w); n++;
	fakeFlat_w = XmCreateToggleButton (corr_w, "Fake", args, n);
	XtManageChild (fakeFlat_w);

	return (w);
}

static Widget
createFileControls (top, takeCB, newFN_wp, scanNewCB, FN_wp, scanLastCB,auto_wp)
Widget top;		/* widget to attach to on top */
void (*takeCB)();	/* callback for the "Take" button */
Widget *newFN_wp;	/* text field widget that holds new file name */
void (*scanNewCB)();	/* callback for the "scan" button for new file */
Widget *FN_wp;		/* text field widget that holds the current file name */
void (*scanLastCB)();	/* callback for the "auto" toggle button */
Widget *auto_wp;	/* toggle widget whose state reads true for auto */
{
	Widget tpb, tfl, tspb;	/* top pushb, file and scan pb */
	Widget cfl, apb;	/* current file and auto pb */
	Widget sep_w;
	Arg args[20];
	int n;

	/* make the row for the new image info */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftOffset, 20); n++;
	tpb = XmCreatePushButton (corr_w, "TPB", args, n);
	XtManageChild (tpb);
	XtAddCallback (tpb, XmNactivateCallback, takeCB, NULL);
	wlprintf (tpb, " Take new ");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, tpb); n++;
	tfl = XmCreateLabel (corr_w, "TFL", args, n);
	XtManageChild (tfl);
	wlprintf (tfl, "File name:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	tspb = XmCreatePushButton (corr_w, "TSPB", args, n);
	XtManageChild (tspb);
	XtAddCallback (tspb, XmNactivateCallback, scanNewCB, NULL);
	wlprintf (tspb, "Scan Dir");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, tfl); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, tspb); n++;
	*newFN_wp = XmCreateTextField (corr_w, "NewTF", args, n);
	XtManageChild (*newFN_wp);

	/* save text field to put next row of things under */

	sep_w = *newFN_wp;

	/* make a row for the current filename info */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, *newFN_wp); n++;
	cfl = XmCreateLabel (corr_w, "CFL", args, n);
	XtManageChild (cfl);
	wlprintf (cfl, "Current file name:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, *newFN_wp); n++;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	apb = XmCreateToggleButton (corr_w, "AutoScan", args, n);
	XtManageChild (apb);
	XtAddCallback (apb, XmNvalueChangedCallback, scanLastCB, NULL);
	wlprintf (apb, "Auto");
	*auto_wp = apb;

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, tfl); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, tspb); n++;
	*FN_wp = XmCreateTextField (corr_w, "LastTF", args, n);
	XtManageChild (*FN_wp);

	/* now that the file name text widget exists, we can do the
	 * actual work of searching and setting it if initial state is auto.
	 */
	if (XmToggleButtonGetState (apb))
	    XmToggleButtonSetState (apb, True, True);

	return (*FN_wp);
}

static void
createClose (top)
Widget top;
{
	Widget pb_w;
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, top); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 40); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 60); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 5); n++;
	pb_w = XmCreatePushButton (corr_w, "Close", args, n);
	XtAddCallback (pb_w, XmNactivateCallback, closeCB, NULL);
	XtManageChild (pb_w);
}

/* called when the take-bias button is activated
 */
/* ARGSUSED */
static void
takeBiasCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char bfn[2048];
	int n;

	/* STO: Fixed scan UI a bit */

	scanLastBadCol();
	scanNewBiasCB(NULL,NULL,NULL);
	
	getFilename (bfn, newBiasFN_w);
	n = getInt (newBiasN_w);

	if (strlen (bfn) == 0)
	    msg ("No bias filename");
	else if (n <= 0)
	    msg ("No count");
	else {
	    //XmToggleButtonSetState (apply_w, False, True);
	    startBias (bfn, n);
	}
}

/* called when the take-thermal button is activated
 */
/* ARGSUSED */
static void
takeThermCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char bfn[2048];
	char tfn[2048];
	double d;
	int n;

	/* STO: fixed scan UI a bit */
	
	scanLastBadCol();
	scanLastBiasCB(NULL,NULL,NULL);
	
	
	getString(tfn, newThermFN_w);
	if(strlen(tfn) == 0) {
		scanNewThermCB(NULL,NULL,NULL);
	}
		
	getString(bfn, biasFN_w);
	if(strlen(bfn) == 0) {
		msg ("No bias filename");
		return;
	}
	
	getFilename (tfn, newThermFN_w);
	getFilename (bfn, biasFN_w);
	n = getInt (newThermN_w);
	d = getReal (newThermD_w);

	if (strlen(tfn) == 0)
	    msg ("No thermal filename");
/*	
	else if (strlen(bfn) == 0)
	    msg ("No bias filename");
*/	
	else if (n <= 0)
	    msg ("No count");
	else if (d <= 0)
	    msg ("Duration must be > 0");
	else {
	    //XmToggleButtonSetState (apply_w, False, True);
	    startThermal (tfn, bfn, n, d);
	}
}

/* called when the take-flat button is activated
 */
/* ARGSUSED */
static void
takeFlatCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char ffn[2048];
	char tfn[2048];
	char bfn[2048];
	char f[10], filter;
	double d;
	int n;

	/* STO: fixed scan UI a bit */
	
	scanLastBadCol();
	scanLastBiasCB(NULL,NULL,NULL);
	scanLastThermCB(NULL,NULL,NULL);
	
	getString(ffn, newFlatFN_w);
	if(strlen(ffn) == 0) {
		scanNewFlatCB(NULL,NULL,NULL);
	}
		
	getString(tfn, thermFN_w);
	if(strlen(tfn) == 0) {
	    msg ("No thermal filename");
	    return;
	}
	
	getString(bfn, biasFN_w);
	if(strlen(bfn) == 0) {
	    msg ("No bias filename");
	    return;
	}	
	
	getFilename (ffn, newFlatFN_w);
	getFilename (bfn, biasFN_w);
	getFilename (tfn, thermFN_w);
	getString (f, newFlatF_w);
	filter = f[0];
	if (islower(filter))
	    filter = toupper(filter);
	d = getReal (newFlatD_w);
	n = getInt (newFlatN_w);

	if (strlen(ffn) == 0)
	    msg ("No flat filename");
/*	
	else if (strlen(bfn) == 0)
	    msg ("No bias filename");
	else if (strlen(tfn) == 0)
	    msg ("No thermal filename");
*/	
	else if (n <= 0)
	    msg ("No count");
	else if (strncmp (ffn, "cf", 2) == 0 && ffn[2] != filter)
	    msg ("Filename filter code does not match");
	else if (d < 0)
	    msg ("Duration must be >= 0");
	else if (XmToggleButtonGetState (fakeFlat_w))
	    mkFakeFlat(ffn, filter, n, d);
	else {
	    //XmToggleButtonSetState (apply_w, False, True);
	    startFlat (ffn, tfn, bfn, filter, n, d);
	}
}

/* create a fake flat and store it at ffn */
static void
mkFakeFlat (char *ffn, char filter, int n, double d)
{
	FImage *fip = &state.fimage;
	CamPixel *cp;
	int npix;
	int i;
	int switchState;

	/* need a pattern to go by */
	if (!fip->image) {
	    msg ("No current image to match against");
	    return;
	}

	/* fill image with a very flat image */
	npix = fip->sw * fip->sh;
	cp = (CamPixel *)fip->image;
	for (i = 0; i < npix; i++)
	    *cp++ = 1;

	/* fresh basic header */
	free ((char *)fip->var);
	fip->var = NULL;
	fip->nvar = 0;
	fip->dur = (int)floor(d*1000+0.5);
	setSimpleFITSHeader (fip);

	/* add stuff for flats */
	timeStampFITS (fip, /*time(NULL)*/0, "When fake was created");
	nc_flatkw (fip, n, "<Fake>", "<Fake>", (int)filter);

	/* show it */
	// Don't correct a fake flat!
	switchState = getAutoCorrectState();
	setAutoCorrectState(False);
	
	strcpy (state.fname, ffn);
	presentNewImage();
	
	setAutoCorrectState(switchState);
	
	/* and save it */
	writeImage ();
}

/* called when the scan button is activated or a CR is typed in the dir
 * filename text widget.
 */
/* ARGSUSED */
static void
scanDirCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	scanAllFiles();
}

/* called when the scan button is activated for the new bias file name */
/* N.B. we can get called manually -- DON'T USE ANY OF THE ARGS!!! */
/* ARGSUSED */
static void
scanNewBiasCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char dir[1024];
	char fn[1024];
	char errmsg[1024];

	getString (dir, dirFN_w);
	if (findNewBiasFN (dir, fn, errmsg) < 0) {
	    msg ("Error scanning for bias: %s", errmsg);
	    return;
	}

	XmTextFieldSetString (newBiasFN_w, basenm(fn));
}

/* called when the scan button is activated for the new thermal file name */
/* N.B. we can get called manually -- DON'T USE ANY OF THE ARGS!!! */
/* ARGSUSED */
static void
scanNewThermCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char dir[1024];
	char fn[1024];
	char errmsg[1024];

	getString (dir, dirFN_w);
	if (findNewThermFN (dir, fn, errmsg) < 0) {
	    msg ("Error scanning for thermal: %s", errmsg);
	    return;
	}

	XmTextFieldSetString (newThermFN_w, basenm(fn));
}

/* called when the scan button is activated for the new flat file name */
/* N.B. we can get called manually -- DON'T USE ANY OF THE ARGS!!! */
/* ARGSUSED */
static void
scanNewFlatCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char dir[1024];
	char fn[1024];
	char f, filter[1024];
	char errmsg[1024];

	getString (filter, newFlatF_w);
	f = filter[0];
	if (isupper(f))
	    f = tolower(f);
	getString (dir, dirFN_w);
	if (findNewFlatFN (f, dir, fn, errmsg) < 0) {
	    msg ("Error scanning for flat: %s", errmsg);
	    return;
	}

	XmTextFieldSetString (newFlatFN_w, basenm(fn));
}

/* called when the scan toggle button changes for the last bias file name */
/* N.B. we can get called manually -- DON'T USE ANY OF THE ARGS!!! */
/* ARGSUSED */
static void
scanLastBiasCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char errmsg[1024];
	char dir[1024];
	char fn[1024];

	/* we might get called at build time before the filename widget */
	if (!biasFN_w)
	    return;

	if (XmToggleButtonGetState (autoBias_w)) {
	    if (!state.fimage.image) {
		msg ("No current image to match against");
		return;
	    }
	    getString (dir, dirFN_w);
	    if (findBiasFN (&state.fimage, dir, fn, errmsg) < 0) {
		msg ("Error scanning for bias: %s", errmsg);
		XmTextFieldSetString (biasFN_w, "");
		return;
	    }

	    XmTextFieldSetString (biasFN_w, basenm(fn));
	}
}

/* called when the scan button is activated for the last thermal file name */
/* N.B. we can get called manually -- DON'T USE ANY OF THE ARGS!!! */
/* ARGSUSED */
static void
scanLastThermCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char errmsg[1024];
	char dir[1024];
	char fn[1024];

	/* we might get called at build time before the filename widget */
	if (!thermFN_w)
	    return;

	if (XmToggleButtonGetState (autoTherm_w)) {
	    if (!state.fimage.image) {
		msg ("No current image to match against");
		return;
	    }

	    getString (dir, dirFN_w);
	    if (findThermFN (&state.fimage, dir, fn, errmsg) < 0) {
		msg ("Error scanning for thermal: %s", errmsg);
		XmTextFieldSetString (thermFN_w, "");
		return;
	    }

	    XmTextFieldSetString (thermFN_w, basenm(fn));
	}
}

/* called when the scan button is activated for the last flat file name */
/* N.B. we can get called manually -- DON'T USE ANY OF THE ARGS!!! */
/* ARGSUSED */
static void
scanLastFlatCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char errmsg[1024];
	char dir[1024];
	char fn[1024];

	/* we might get called at build time before the filename widget */
	if (!flatFN_w)
	    return;

	if (XmToggleButtonGetState (autoFlat_w)) {
	    char kwf[100];
	    int f;

	    if (!state.fimage.image) {
		msg ("No current image to match against");
		return;
	    }

	    /* pick filter.
	     * use the current user setting if image has none
	     */
	    if (getStringFITS (&state.fimage, "FILTER", kwf) < 0)
		getString (kwf, newFlatF_w);
	    f = kwf[0];

	    getString (dir, dirFN_w);
	    if (findFlatFN (&state.fimage, f, dir, fn, errmsg) < 0) {
		msg ("Error scanning for flat: %s", errmsg);
		XmTextFieldSetString (flatFN_w, "");
		return;
	    }

	    XmTextFieldSetString (flatFN_w, basenm(fn));
	}
}

/* called during scan all */
/* ARGSUSED */
static void scanLastBadCol (void)
{
	char errmsg[1024];
	char dir[1024];
	char fn[1024];

	/* we might get called at build time before the filename widget */
	if (badColMapFN_w) {

	    getString (dir, dirFN_w);
	    if (findMapFN (dir, fn, errmsg) < 0) {
	//		msg ("Error scanning for bad column map: %s", errmsg);
			XmTextFieldSetString (badColMapFN_w, "");
			return;
	    }

	    XmTextFieldSetString (badColMapFN_w, basenm(fn));
	}
}

/* called when the close button is activated.
 */
/* ARGSUSED */
static void
closeCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (corr_w);
}

/* scan and fill in all the filename fields.
 */
static void
scanAllFiles()
{
	/* ARGHHH! we just invoke each of the scan callbacks :-Z */
	scanNewBiasCB (NULL, NULL, NULL);
	scanLastBiasCB (NULL, NULL, NULL);
	scanNewThermCB (NULL, NULL, NULL);
	scanLastThermCB (NULL, NULL, NULL);
	scanNewFlatCB (NULL, NULL, NULL);
	scanLastFlatCB (NULL, NULL, NULL);

	scanLastBadCol();
}

/* fill fn with string from text field widget fn.
 * if it does not begin with '/' we also prepend the string in dirFN_w.
 *   if dirFN_w does not end with '/', we add one before appending w.
 */
static void
getFilename (fn, w)
char fn[];
Widget w;
{
	int l;

	getString (fn, w);
	if (fn[0] == '/')
	    return;

	/* w didn't start with / so do again this time after dirFN_w */
	getString (fn, dirFN_w);
	l = strlen (fn);
	if (l > 0 && fn[l-1] != '/') {
	    strcat (fn, "/");
	    l++;
	}
	getString (fn+l, w);
}

static void
getString (s, w)
char s[];
Widget w;
{
	String str;

	str = XmTextFieldGetString (w);
	strcpy (s, str);
	XtFree (str);
}

static int
getInt (w)
Widget w;
{
	String s;
	int n;

	s = XmTextFieldGetString (w);
	n = atoi (s);
	XtFree (s);

	return (n);
}

static double
getReal (w)
Widget w;
{
	String s;
	double n;

	s = XmTextFieldGetString (w);
	n = atof (s);
	XtFree (s);

	return (n);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: corr.c,v $ $Date: 2002/12/21 00:32:49 $ $Revision: 1.7 $ $Name:  $"};
