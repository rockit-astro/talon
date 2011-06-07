/* code to manage and perform arithmetic operations.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>

#ifdef DO_EDITRES
#include <X11/Xmu/Editres.h>
#endif

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/FileSB.h>
#include <Xm/Label.h>
#include <Xm/Form.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>

#include "P_.h"
#include "astro.h"
#include "strops.h"
#include "telenv.h"
#include "xtools.h"
#include "fits.h"
#include "fitscorr.h"
#include "fieldstar.h"

#include "camera.h"

static void create_arith_w(void);
static void initFSB (Widget fsb_w);
static void okCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);
static void flatCB (Widget w, XtPointer client, XtPointer call);
static void thermCB (Widget w, XtPointer client, XtPointer call);
static int useConstant (double *cp);
static int useFile (void);
static int getFile (FImage *fip, char fn[]);
static int doAdd (void);
static int doSub (void);
static int doMult (void);
static int doDiv (void);
static int doFlat (void);
static int doTherm (void);
static void ac_add (FImage *fip, double c);
static void ac_mult (FImage *fip, double c);
static void ai_add (FImage *fip1, FImage *fip2);
static void ai_sub (FImage *fip1, FImage *fip2);
static void ai_mult (FImage *fip1, FImage *fip2);
static void ai_div (FImage *fip1, FImage *fip2);
static void ai_flat (FImage *fip1, FImage *fip2, double mean);
static void ai_therm (FImage *fip1, FImage *fip2, double factor);

static Widget arith_w;		/* main dialog */
static Widget fsb_w;		/* FSB for a file operand */
static Widget constTB_w;	/* constant operand TB */
static Widget constTF_w;	/* constant operand text field */
static Widget fileTB_w;		/* file operand TB */

typedef struct {
    char *prompt;	/* prompt */
    int (*f)(void);	/* call when find widget set */
    void (*cb)();	/* callback, if interested */
    Widget tb;		/* TB widget */
} RadBox;

static RadBox radbox[] = {
    {"Add",  		 doAdd},
    {"Subtract",	 doSub},
    {"Multiply",	 doMult},
    {"Divide",		 doDiv},
    {"Divide by flat",	 doFlat,  flatCB},
    {"Subtract thermal", doTherm, thermCB},
};

static char hist_fld[] = "HISTORY";	/* handy FITS field name */

/* called when user wants to toggle the arithmetic dialog */
void
arithCB (Widget w, XtPointer client, XtPointer call)
{
	if (!arith_w)
	    create_arith_w();

	if (XtIsManaged(arith_w))
	    raiseShell (arith_w);
	else
	    XtManageChild (arith_w);
}

static void
create_arith_w()
{
	Arg args[20];
	Widget rb_w, rc_w;
	Widget w;
	int i;
	int n;

	/* create form */
	
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNallowResize, True); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	arith_w = XmCreateFormDialog (toplevel_w, "Arith", args,n);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Image Arithmetic"); n++;
	XtSetValues (XtParent(arith_w), args, n);
	XtVaSetValues (arith_w, XmNcolormap, camcm, NULL);

	/* pile up most stuff  in a r/c */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	rc_w = XmCreateRowColumn (arith_w, "RC", args, n);
	XtManageChild (rc_w);

	/* operator label */
	n = 0;
	w = XmCreateLabel (rc_w, "Opr", args, n);
	wlprintf (w, "Operator:");
	XtManageChild (w);

	/* operator choices in a RB */
	n = 0;
	XtSetArg (args[n], XmNmarginWidth, 30); n++;
	XtSetArg (args[n], XmNnumColumns, 3); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	rb_w = XmCreateRadioBox (rc_w, "RB", args, n);
	XtManageChild (rb_w);

	    for (i = 0; i < XtNumber(radbox); i++) {
		RadBox *rp = &radbox[i];

		n = 0;
		if (i == 0) {
		    XtSetArg (args[n], XmNset, True); n++;
		}
		w = XmCreateToggleButton (rb_w, "TB", args, n);
		wlprintf (w, "%s", rp->prompt);
		XtManageChild (w);
		rp->tb = w;

		if (rp->cb)
		    XtAddCallback (w, XmNvalueChangedCallback, rp->cb, NULL);
	    }

	/* operand label */
	n = 0;
	w = XmCreateLabel (rc_w, "Opr", args, n);
	wlprintf (w, "Operand:");
	XtManageChild (w);

	/* operand choices in a RB */
	n = 0;
	XtSetArg (args[n], XmNmarginWidth, 30); n++;
	XtSetArg (args[n], XmNnumColumns, 2); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	rb_w = XmCreateRadioBox (rc_w, "RB2", args, n);
	XtManageChild (rb_w);

	XtVaSetValues (rb_w, XmNisHomogeneous, False, NULL);

	    n = 0;
	    XtSetArg (args[n], XmNset, True); n++;
	    constTB_w = XmCreateToggleButton (rb_w, "Constant", args, n);
	    wlprintf (constTB_w, "Constant:");
	    XtManageChild (constTB_w);

	    n = 0;
	    XtSetArg (args[n], XmNcolumns, 15); n++;
	    constTF_w = XmCreateTextField (rb_w, "FN", args, n);
	    wtprintf (constTF_w, "0");
	    XtManageChild (constTF_w);

	    n = 0;
	    fileTB_w = XmCreateToggleButton (rb_w, "File", args, n);
	    wlprintf (fileTB_w, "File:");
	    XtManageChild (fileTB_w);

	/* FSB -- attach to form so it grows with */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	fsb_w = XmCreateFileSelectionBox (arith_w, "FSB", args, n);
	initFSB (fsb_w);
	XtManageChild (fsb_w);
}

static void
initFSB (Widget fsb_w)
{
	Widget w;
	char buf[1024];

	/* change some button labels */
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_OK_BUTTON);
	set_xmstring (w, XmNlabelString, "Compute");
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_CANCEL_BUTTON);
	set_xmstring (w, XmNlabelString, "Close");

	/* connect an Compute handler */
	XtAddCallback (fsb_w, XmNokCallback, okCB, NULL);

	/* connect a Close handler */
	XtAddCallback (fsb_w, XmNcancelCallback, closeCB, NULL);

	/* no Help handler */
	XtUnmanageChild(XmFileSelectionBoxGetChild(fsb_w,XmDIALOG_HELP_BUTTON));

	/* set default directory */
	telfixpath (buf, "archive/calib");
	set_xmstring (fsb_w, XmNdirectory, buf);
}

/* Ok on the FSB means do it */
static void
okCB (Widget w, XtPointer client, XtPointer call)
{
	int i;

	for (i = 0; i < XtNumber(radbox); i++) {
	    RadBox *rp = &radbox[i];

	    if (XmToggleButtonGetState (rp->tb)) {
		watch_cursor(1);
		if ((*rp->f)() == 0) {
		    newStats();
		    updateAOI();
		    updateFITS();
		    msg ("");
		}
		watch_cursor(0);
		return;
	    }
	}

	msg ("No operator selected");
}

/* Cancel on the FSB means close down */
static void
closeCB (Widget w, XtPointer client, XtPointer call)
{
	XtUnmanageChild (arith_w);
}

/* called when Divide by Flat TB changes */
static void
flatCB (Widget w, XtPointer client, XtPointer call)
{
	if (XmToggleButtonGetState (w))
	    XmToggleButtonSetState (fileTB_w, True, True);
}

/* called when Subtract thermal TB changes */
static void
thermCB (Widget w, XtPointer client, XtPointer call)
{
	if (XmToggleButtonGetState (w))
	    XmToggleButtonSetState (fileTB_w, True, True);
}

/* check if user wants operand to be a constant.
 * if so, return 1 and put it in *cp, else return 0.
 */
static int
useConstant (double *cp)
{
	char *v;

	if (!XmToggleButtonGetState (constTB_w))
	    return (0);
	    
	v =  XmTextFieldGetString (constTF_w);
	*cp = atof (v);
	XtFree (v);
	return (1);
}

/* check if user wants operand to be a FITS file.
 * if so return 1, else return 0
 */
static int
useFile ()
{
	return (XmToggleButtonGetState (fileTB_w));
}

/* open the FITS file currently selected in the FSB.
 * if ok fill in fip and base name and return 0, else write msg() and return -1.
 */
static int
getFile (FImage *fip, char fn[])
{
	Widget w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_TEXT);
	char *v = XmTextGetString (w);
	char buf[1024];
	int fd;

	(void) strcpy (buf, v);
	XtFree (v);

	if (buf[0] == '\0') {
	    msg ("No file selected");
	    return (-1);
	}

	(void) strcpy (fn, basenm (buf));

	fd = open (buf, O_RDONLY);
	if (fd < 0) {
	    msg ("%s: %s", fn, strerror(errno));
	    return (-1);
	}
	if (readFITS (fd, fip, buf) < 0) {
	    msg ("%s: %s", fn, buf);
	    return (-1);
	}
	(void) close (fd);

	return (0);
}

/* check that the two images are the same size.
 * if not write msg() and return -1, else return 0.
 */
static int
chkSize (FImage *fip1, FImage *fip2, char f2name[])
{
	if (fip1->sw != fip2->sw || fip1->sh != fip2->sh) {
	    msg ("%s: must be %d x %d", f2name, fip1->sw, fip1->sh);
	    return (-1);
	}

	return (0);
}



static int
doAdd ()
{
	FImage *fip1 = &state.fimage;
	char record[128];
	double c;

	if (!fip1->image) {
	    msg ("No image");
	    return (-1);
	}

	if (useConstant(&c)) {
	    ac_add (fip1, c);
	    (void) sprintf (record, "Add %g", c);
	    setCommentFITS (fip1, hist_fld, record);
	} else if (useFile ()) {
	    FImage f, *fip2 = &f;
	    char fn[64];

	    initFImage (fip2);
	    if (getFile (fip2, fn) < 0)
		return (-1);
	    if (chkSize (fip1, fip2, fn) < 0) {
		resetFImage (fip2);
		return (-1);
	    }
	    ai_add (fip1, fip2);
	    resetFImage (fip2);
	    (void) sprintf (record, "Add %s", fn);
	    setCommentFITS (fip1, hist_fld, record);
	} else {
	    msg ("No operand");
	    return (-1);
	}

	return (0);
}

static int
doSub ()
{
	FImage *fip1 = &state.fimage;
	char record[128];
	double c;

	if (!fip1->image) {
	    msg ("No image");
	    return (-1);
	}

	if (useConstant(&c)) {
	    ac_add (fip1, -c);
	    (void) sprintf (record, "Subtract %g", c);
	    setCommentFITS (fip1, hist_fld, record);
	} else if (useFile ()) {
	    FImage f, *fip2 = &f;
	    char fn[64];

	    initFImage (fip2);
	    if (getFile (fip2, fn) < 0)
		return (-1);
	    if (chkSize (fip1, fip2, fn) < 0) {
		resetFImage (fip2);
		return (-1);
	    }
	    ai_sub (fip1, fip2);
	    resetFImage (fip2);
	    (void) sprintf (record, "Subtract %s", fn);
	    setCommentFITS (fip1, hist_fld, record);
	} else {
	    msg ("No operand");
	    return (-1);
	}

	return (0);
}

static int
doMult ()
{
	FImage *fip1 = &state.fimage;
	char record[128];
	double c;

	if (!fip1->image) {
	    msg ("No image");
	    return (-1);
	}

	if (useConstant(&c)) {
	    ac_mult (fip1, c);
	    (void) sprintf (record, "Multiply by %g", c);
	    setCommentFITS (fip1, hist_fld, record);
	} else if (useFile ()) {
	    FImage f, *fip2 = &f;
	    char fn[64];

	    initFImage (fip2);
	    if (getFile (fip2, fn) < 0)
		return (-1);
	    if (chkSize (fip1, fip2, fn) < 0) {
		resetFImage (fip2);
		return (-1);
	    }
	    ai_mult (fip1, fip2);
	    resetFImage (fip2);
	    (void) sprintf (record, "Multiply by %s", fn);
	    setCommentFITS (fip1, hist_fld, record);
	} else {
	    msg ("No operand");
	    return (-1);
	}

	return (0);
}

static int
doDiv ()
{
	FImage *fip1 = &state.fimage;
	char record[128];
	double c;

	if (!fip1->image) {
	    msg ("No image");
	    return (-1);
	}

	if (useConstant(&c)) {
	    if (c == 0) {
		msg ("Can not divide by 0");
		return (-1);
	    }
	    ac_mult (fip1, 1/c);
	    (void) sprintf (record, "Divide by %g", c);
	    setCommentFITS (fip1, hist_fld, record);
	} else if (useFile ()) {
	    FImage f, *fip2 = &f;
	    char fn[64];

	    initFImage (fip2);
	    if (getFile (fip2, fn) < 0)
		return (-1);
	    if (chkSize (fip1, fip2, fn) < 0) {
		resetFImage (fip2);
		return (-1);
	    }
	    ai_div (fip1, fip2);
	    resetFImage (fip2);
	    (void) sprintf (record, "Divide by %s", fn);
	    setCommentFITS (fip1, hist_fld, record);
	} else {
	    msg ("No operand");
	    return (-1);
	}

	return (0);
}

static int
doFlat ()
{
	FImage *fip1 = &state.fimage;
	double flatmean;
	char record[128];
	char fn[64];
	FImage f, *fip2 = &f;

	if (!fip1->image) {
	    msg ("No image");
	    return (-1);
	}

	initFImage (fip2);
	if (getFile (fip2, fn) < 0)
	    return (-1);
	if (chkSize (fip1, fip2, fn) < 0) {
	    resetFImage (fip2);
	    return (-1);
	}
	if (getStringFITS(fip2, "FLATFR", record) < 0) {
	    msg ("%s: no FLATFR keyword", fn);
	    resetFImage (fip2);
	    return (-1);
	}
	if (getRealFITS (fip2, "FLATMEAN", &flatmean) < 0)
	    computeMeanFITS (fip2, &flatmean);

	ai_flat (fip1, fip2, flatmean);
	(void) sprintf (record, "Divide by flat %s with mean %g", fn, flatmean);
	setCommentFITS (fip1, hist_fld, record);

	resetFImage (fip2);

	return (0);
}

static int
doTherm ()
{
	FImage *fip1 = &state.fimage;
	double fip1exp, fip2exp;
	char record[128];
	char fn[64];
	FImage f, *fip2 = &f;

	if (!fip1->image) {
	    msg ("No image");
	    return (-1);
	}
	if (getRealFITS (fip1, "EXPTIME", &fip1exp) < 0) {
	    msg ("No EXPTIME");
	    return (-1);
	}

	initFImage (fip2);
	if (getFile (fip2, fn) < 0)
	    return (-1);
	if (getStringFITS(fip2, "THERMFR", record) < 0) {
	    msg ("%s: no THERMFR keyword", fn);
	    resetFImage (fip2);
	    return (-1);
	}
	if (getRealFITS (fip2, "EXPTIME", &fip2exp) < 0) {
	    msg ("%s: no EXPTIME keyword", fn);
	    resetFImage (fip2);
	    return (-1);
	}
	if (fip2exp <= 0) {
	    msg ("%s: EXPTIME must be > 0: %g", fn, fip2exp);
	    resetFImage (fip2);
	    return (-1);
	}

	ai_therm (fip1, fip2, fip1exp/fip2exp);
	(void) sprintf (record, "Subract thermal %s with EXPTIME %g", fn,
								    fip2exp);
	setCommentFITS (fip1, hist_fld, record);

	resetFImage (fip2);

	return (0);
}


/* basic arithemetic operations */

/* add constant to an image */
static void
ac_add (FImage *fip, double c)
{
	CamPixel *ip = (CamPixel *)fip->image;
	CamPixel *lp = ip + fip->sw*fip->sh;

	while (ip < lp) {
	    double p = c + (double)(*ip);
	    if (p > MAXCAMPIX)
		p = MAXCAMPIX;
	    if (p < 0)
		p = 0;
	    *ip++ = (CamPixel) floor(p + 0.5);
	}
}

/* mult image by a constant */
static void
ac_mult (FImage *fip, double c)
{
	CamPixel *ip = (CamPixel *)fip->image;
	CamPixel *lp = ip + fip->sw*fip->sh;

	while (ip < lp) {
	    double p = c * (double)(*ip);
	    if (p > MAXCAMPIX)
		p = MAXCAMPIX;
	    if (p < 0)
		p = 0;
	    *ip++ = (CamPixel) floor(p + 0.5);
	}
}

/* mult image 1 by 2 */
static void
ai_mult (FImage *fip1, FImage *fip2)
{
	CamPixel *ip1 = (CamPixel *)fip1->image;
	CamPixel *lp1 = ip1 + fip1->sw*fip1->sh;
	CamPixel *ip2 = (CamPixel *)fip2->image;

	while (ip1 < lp1) {
	    double p = (double)(*ip1) * (double)(*ip2++);
	    if (p > MAXCAMPIX)
		p = MAXCAMPIX;
	    if (p < 0)
		p = 0;
	    *ip1++ = (CamPixel) floor(p + 0.5);
	}
}

/* div image 1 by 2 */
static void
ai_div (FImage *fip1, FImage *fip2)
{
	CamPixel *ip1 = (CamPixel *)fip1->image;
	CamPixel *lp1 = ip1 + fip1->sw*fip1->sh;
	CamPixel *ip2 = (CamPixel *)fip2->image;

	while (ip1 < lp1) {
	    double p1 = (double)(*ip1);
	    double p2 = (double)(*ip2++);
	    double p;

	    if (p1 != 0.0) {
		p = p1 / p2;
		if (p > MAXCAMPIX)
		    p = MAXCAMPIX;
		if (p < 0)
		    p = 0;
	    } else
		p = MAXCAMPIX;
		
	    *ip1++ = (CamPixel) floor(p + 0.5);
	}
}

/* add image 2 to 1 */
static void
ai_add (FImage *fip1, FImage *fip2)
{
	CamPixel *ip1 = (CamPixel *)fip1->image;
	CamPixel *lp1 = ip1 + fip1->sw*fip1->sh;
	CamPixel *ip2 = (CamPixel *)fip2->image;

	while (ip1 < lp1) {
	    double p = (double)(*ip1) + (double)(*ip2++);
	    if (p > MAXCAMPIX)
		p = MAXCAMPIX;
	    if (p < 0)
		p = 0;
	    *ip1++ = (CamPixel) floor(p + 0.5);
	}
}

/* subtract image 2 from 1 */
static void
ai_sub (FImage *fip1, FImage *fip2)
{
	CamPixel *ip1 = (CamPixel *)fip1->image;
	CamPixel *lp1 = ip1 + fip1->sw*fip1->sh;
	CamPixel *ip2 = (CamPixel *)fip2->image;

	while (ip1 < lp1) {
	    double p = (double)(*ip1) - (double)(*ip2++);
	    if (p > MAXCAMPIX)
		p = MAXCAMPIX;
	    if (p < 0)
		p = 0;
	    *ip1++ = (CamPixel) floor(p + 0.5);
	}
}

/* apply flat fip2 to fip1 */
static void
ai_flat (FImage *fip1, FImage *fip2, double mean)
{
	CamPixel *ip1 = (CamPixel *)fip1->image;
	CamPixel *lp1 = ip1 + fip1->sw*fip1->sh;
	CamPixel *ip2 = (CamPixel *)fip2->image;

	while (ip1 < lp1) {
	    double p = (double)(*ip1) / (double)(*ip2++) * mean;
	    if (p > MAXCAMPIX)
		p = MAXCAMPIX;
	    if (p < 0)
		p = 0;
	    *ip1++ = (CamPixel) floor(p + 0.5);
	}
}

/* apply thermal fip2 to fip1 */
static void
ai_therm (FImage *fip1, FImage *fip2, double factor)
{
	CamPixel *ip1 = (CamPixel *)fip1->image;
	CamPixel *lp1 = ip1 + fip1->sw*fip1->sh;
	CamPixel *ip2 = (CamPixel *)fip2->image;

	while (ip1 < lp1) {
	    double p = (double)(*ip1) - (double)(*ip2++) * factor;
	    if (p > MAXCAMPIX)
		p = MAXCAMPIX;
	    if (p < 0)
		p = 0;
	    *ip1++ = (CamPixel) floor(p + 0.5);
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: arith.c,v $ $Date: 2001/04/19 21:11:59 $ $Revision: 1.1.1.1 $ $Name:  $"};
