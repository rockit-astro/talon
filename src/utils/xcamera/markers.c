/* dialog to manage markers.. maybe annotation someday. */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/SelectioB.h>
#include <Xm/Form.h>
#include <Xm/ToggleB.h>
#include <Xm/Separator.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/TextF.h>

#include "P_.h"
#include "astro.h"
#include "xtools.h"
#include "fits.h"
#include "telenv.h"
#include "fieldstar.h"
#include "wcs.h"

#include "camera.h"
#include "ps.h"

#define	NMARKERS	6	/* total number of markers, any type */
#define	MSZ		5	/* radius of marker circle */

typedef struct {
    Widget ontb;		/* on-off TB */
    Widget xytb;		/* on for X/Y off for RA/Dec */
    Widget xtf, ytf;		/* X/Y (or RA/Dec) text fields */
    Widget ctf;			/* color text field */
    Widget camtb;		/* cam or image coords TB */
} Marker;

static Marker markers[NMARKERS];

static Widget markers_w;	/* overall Form Dialog */
static GC marker_gc;		/* GC for drawing */

static void createMarkers(void);
static void okCB (Widget w, XtPointer client, XtPointer call);
static void applyCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);
static void apply(void);
static void drawMarker(Marker *mp);
static void drawMark (int x, int y);
static void makeGC (void);

/* bring up the markers dialog */
void
markersCB (Widget w, XtPointer client, XtPointer call)
{
	if (!markers_w)
	    createMarkers();

	if (XtIsManaged(markers_w))
	    raiseShell (markers_w);
	else
	    XtManageChild (markers_w);
}

void
drawMarkers(void)
{
	int i;

	if (!markers_w)
	    createMarkers();

	if (!marker_gc)
	    makeGC();

	msg ("");
	for (i = 0; i < NMARKERS; i++) {
	    Marker *mp = &markers[i];
	    if (XmToggleButtonGetState(mp->ontb))
		drawMarker (mp);
	}
}

/* create the widgets and set up the defaults from them.
 */
static void
createMarkers()
{
	Widget w, t_w;
	Widget rc_w;
	Arg args[20];
	int i, n;

	/* the main form */

	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 5); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	XtSetArg (args[n], XmNmarginHeight, 5); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	markers_w = XmCreateFormDialog (toplevel_w, "Markers", args, n);

	n = 0;
	XtSetArg (args[n], XmNtitle, "Markers"); n++;
	XtSetValues (XtParent(markers_w), args, n);
	XtVaSetValues (markers_w, XmNcolormap, camcm, NULL);

	/* title label */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	t_w = XmCreateLabel (markers_w, "Title", args, n);
	wlprintf (t_w, "Define X/Y and RA/Dec markers in Camera or Image coordinates");
	XtManageChild (t_w);

	/* put the table in a RC */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, t_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg (args[n], XmNnumColumns, NMARKERS+1); n++;
	XtSetArg (args[n], XmNisAligned, FALSE); n++;
	rc_w = XmCreateRowColumn (markers_w, "RC1", args, n);
	XtManageChild (rc_w);

	/* first row is for titles */

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	w = XmCreateLabel (rc_w, "OnOff", args, n);
	wlprintf (w, "On(Off)");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	w = XmCreateLabel (rc_w, "Type", args, n);
	wlprintf (w, "X/Y(R/D)");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	w = XmCreateLabel (rc_w, "CI", args, n);
	wlprintf (w, "Cam(Img)");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	w = XmCreateLabel (rc_w, "C", args, n);
	wlprintf (w, "Color");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	w = XmCreateLabel (rc_w, "XRA", args, n);
	wlprintf (w, "X/RA");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	w = XmCreateLabel (rc_w, "YDec", args, n);
	wlprintf (w, "Y/Dec");
	XtManageChild (w);

	/* add the markers, per row */

	for (i = 0; i < NMARKERS; i++) {
	    Marker *mp = &markers[i];
	    char buf[32];

	    n = 0;
	    sprintf (buf, "On%d", i+1);
	    XtSetArg (args[n], XmNmarginWidth, 30); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    mp->ontb = XmCreateToggleButton (rc_w, buf, args, n);
	    wlprintf (mp->ontb, " ");
	    XtManageChild (mp->ontb);

	    n = 0;
	    sprintf (buf, "XY%d", i+1);
	    XtSetArg (args[n], XmNmarginWidth, 30); n++;
	    mp->xytb = XmCreateToggleButton (rc_w, buf, args, n);
	    wlprintf (mp->xytb, " ");
	    XtManageChild (mp->xytb);

	    n = 0;
	    sprintf (buf, "CamCoords%d", i+1);
	    XtSetArg (args[n], XmNmarginWidth, 30); n++;
	    mp->camtb = XmCreateToggleButton (rc_w, buf, args, n);
	    wlprintf (mp->camtb, " ");
	    XtManageChild (mp->camtb);

	    n = 0;
	    sprintf (buf, "Color%d", i+1);
	    XtSetArg (args[n], XmNcolumns, 9); n++;
	    mp->ctf = XmCreateTextField (rc_w, buf, args, n);
	    XtManageChild (mp->ctf);

	    n = 0;
	    sprintf (buf, "X%d", i+1);
	    XtSetArg (args[n], XmNcolumns, 9); n++;
	    mp->xtf = XmCreateTextField (rc_w, buf, args, n);
	    XtManageChild (mp->xtf);

	    n = 0;
	    sprintf (buf, "Y%d", i+1);
	    XtSetArg (args[n], XmNcolumns, 9); n++;
	    mp->ytf = XmCreateTextField (rc_w, buf, args, n);
	    XtManageChild (mp->ytf);
	}

	/* bottom controls */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 10); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 30); n++;
	w = XmCreatePushButton (markers_w, "Ok", args, n);
	XtAddCallback (w, XmNactivateCallback, okCB, NULL);
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 40); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 60); n++;
	w = XmCreatePushButton (markers_w, "Apply", args, n);
	XtAddCallback (w, XmNactivateCallback, applyCB, NULL);
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, rc_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 5); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 70); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 90); n++;
	w = XmCreatePushButton (markers_w, "Close", args, n);
	XtAddCallback (w, XmNactivateCallback, closeCB, NULL);
	XtManageChild (w);
}

static void
okCB (Widget w, XtPointer client, XtPointer call)
{
	apply();
	XtUnmanageChild (markers_w);
}

static void
applyCB (Widget w, XtPointer client, XtPointer call)
{
	apply();
}

static void
closeCB (Widget w, XtPointer client, XtPointer call)
{
	XtUnmanageChild (markers_w);
}

static void
apply(void)
{
	FImage *fip = &state.fimage;
	if (!fip->image)
	    return;
	refreshScene (0, 0, fip->sw, fip->sh);
}

static void
drawMarker (Marker *mp)
{
	Display *dsp = XtDisplay(state.imageDA);
	FImage *fip = &state.fimage;
	XColor screen, exact;
	int x, y;
	char *str;

	/* only if really is an image now */
	if (!fip->image) {
	    msg ("No image");
	    return;
	}

	/* get the color */
	str = XmTextFieldGetString (mp->ctf);
	if (!XAllocNamedColor (dsp, camcm, str, &screen, &exact))
	    screen.pixel = WhitePixel (dsp, DefaultScreen(dsp));
	XtFree (str);
	XSetForeground (dsp, marker_gc, screen.pixel);

	/* figure x/y */
	if (XmToggleButtonGetState(mp->xytb)) {
	    /* image x/y */

	    str = XmTextFieldGetString (mp->xtf);
	    x = atoi (str);
	    XtFree (str);
	    str = XmTextFieldGetString (mp->ytf);
	    y = atoi (str);
	    XtFree (str);

	    /* change to camera coords if desired */
	    if (XmToggleButtonGetState(mp->camtb)) {
		x -= fip->sx;
		y -= fip->sy;
		x /= fip->bx;
		y /= fip->by;

		if (state.lrflip)
		    x = fip->sw-1-x;
		if (state.tbflip)
		    y = fip->sh-1-y;
	    }

	} else {
	    /* image ra/dec */
	    double ra, dec;
	    double rx, ry;

	    str = XmTextFieldGetString (mp->xtf);
	    if (scansex (str, &ra) < 0) {
		msg ("RA/Dec Marker error: Bad RA format: %s", str);
		XtFree (str);
		return;
	    }
	    XtFree (str);
	    ra = hrrad (ra);

	    str = XmTextFieldGetString (mp->ytf);
	    if (scansex (str, &dec) < 0) {
		msg ("RA/Dec Marker error: Bad Dec format: %s", str);
		XtFree (str);
		return;
	    }
	    XtFree (str);
	    dec = degrad (dec);

	    if (rd2kxy (fip, ra, dec, &rx, &ry) < 0) {
		msg ("RA/Dec Marker error: No WCS headers");
		return;
	    }
	    x = (int)floor(rx + 0.5);
	    y = (int)floor(ry + 0.5);
	}

	drawMark (x, y);
	return;
}

/* the mark at the given image location */
static void
drawMark (int x, int y)
{
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);

	image2window(&x,&y);
	XPSDrawArc (dsp, win, marker_gc, x-MSZ, y-MSZ, 2*MSZ, 2*MSZ, 0, 360*64);
	XPSDrawLine (dsp, win, marker_gc, x, y-2*MSZ, x, y-MSZ);
	XPSDrawLine (dsp, win, marker_gc, x, y+MSZ, x, y+2*MSZ);
	XPSDrawLine (dsp, win, marker_gc, x-2*MSZ, y, x-MSZ, y);
	XPSDrawLine (dsp, win, marker_gc, x+MSZ, y, x+2*MSZ, y);
}

static void
makeGC(void)
{
	Display *dsp = XtDisplay(state.imageDA);
	Window win = XtWindow(state.imageDA);

	/* make a GC to use */
	marker_gc = XCreateGC (dsp, win, 0L, NULL);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: markers.c,v $ $Date: 2001/04/19 21:12:00 $ $Revision: 1.1.1.1 $ $Name:  $"};
