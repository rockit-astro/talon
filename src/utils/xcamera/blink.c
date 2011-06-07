/* code to handle blinking */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/Scale.h>
#include <Xm/Frame.h>
#include <Xm/DrawingA.h>

#include "P_.h"
#include "astro.h"
#include "xtools.h"
#include "fits.h"
#include "telenv.h"
#include "strops.h"
#include "wcs.h"
#include "fieldstar.h"
#include "camera.h"

#define	MINSZ		50		/* min AOI for ref blink, pixels */
#define	MAXLAB		128		/* max label string */

extern Widget toplevel_w;
extern char myclass[];

static void blink_to (XtPointer client, XtIntervalId *idp);
static void loadFirst(void);
static void loadAnother(void);
static void createBlinkw (void);
static void daExpCB (Widget w, XtPointer client, XtPointer call);
static void addCB (Widget w, XtPointer client, XtPointer call);
static void addAllCB (Widget w, XtPointer client, XtPointer call);
static void delCB (Widget w, XtPointer client, XtPointer call);
static void runCB (Widget w, XtPointer client, XtPointer call);
static void newCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);
static void unmapCB (Widget w, XtPointer client, XtPointer call);
static void speedCB (Widget w, XtPointer client, XtPointer call);
static void posCB (Widget w, XtPointer client, XtPointer call);
static void setRefAOI(void);
static void startBlinking(void);
static void stopBlinking(void);
static void newDA (void);
static void nextStep (void);
static void resetPM (void);
static void growPM (void);
static void shrinkPM (void);
static void fillPM (void);
static void showPM(void);
static int blinkRef0(void);
static void blinkRef1(void);
static int bb2AOI(FImage *fip, double er, double nd, double wr, double sd,
    AOI *ap);
static void forceAOI (AOI *ap);
static int findRefImage (char *obj, double *era, double *ndec, double *wra,
    double *sdec, char *fn);

/* widgets */
static Widget blink_w;			/* main form dialog */
static Widget blink_da_w;		/* drawing area -- changes often */
static Widget blinklabel_w;		/* label for which image is in window */
static Widget frame_w;			/* frame for drawing area */
static Widget speed_w;			/* speed scale */
static Widget pos_w;			/* position scale */
static Widget cyclic_w;			/* TB: set to choose cyclic pattern */
static Widget ss_w;			/* Start/Stop PB */

/* info for each pixmap/frame in movie */
typedef struct {
    Pixmap pm;
    char title[MAXLAB];
} PMInfo;
static PMInfo *pminfo;			/* malloced array of PMInfo */
static int npminfo;			/* entries in pminfo */
static int currentpm = -1;			/* index into pminfo currently up */
static int cyc_backwards;		/* if cyclic, which way now */

static XtIntervalId bl_id;		/* timer id .. 0 when not blinking */

/* the aoi to use when adding more images */
typedef struct {
    int wx, wy, ww, wh;			/* ul location/size, in window pixels*/
} RefAOI;
static RefAOI refaoi;

/* flag and support to tell expose we want to load and blink with reference */
static int want_ref;
static double ref_era, ref_ndec, ref_wra, ref_sdec;
static char ref_fn[1024];

/* called from main to toggle movie dialog */
/* ARGSUSED */
void
blinkCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
    extern MTB mtb[];
    int default_zoom = 2;  /* 1/2 zoom */

    /* KMI 10/19/03 - change default zoom to make things fit on screen */
    XmToggleButtonSetState(mtb[default_zoom].w, True, True);

	if (!blink_w) {
	    createBlinkw();
	    if (state.fimage.image)
		loadFirst();
	    /* expose will do the rest */
	}

	if (XtIsManaged(blink_w))
	    raiseShell (blink_w);
	else
	    XtManageChild (blink_w);
}

/* called from main to load and prep for blinking a reference object */
/* ARGSUSED */
void
blinkRefCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* not reentrant */
	if (want_ref)
	    return;

	/* first phase does not need blink_w at all */
	if (blinkRef0() < 0)
	    return;

	/* if blink_w is up we can do the work here, else we arrange
	 * for it in the expose handler
	 */
	if (blink_w && XtIsManaged(blink_w)) {
	    loadFirst();
	    blinkRef1();
	} else {
	    want_ref = 1;
	    blinkCB (w, client, call);
	}
}

/* load image under current AOI as first frame of movie.
 * also establish a new reference area for subsequent frames.
 */
static void
loadFirst()
{
	setRefAOI();
	newDA();
	resetPM();
    if (!state.daGC)
        refreshScene(0,0,0,0);

	growPM();
	fillPM();
}

/* load next image, forcing AOI to refaoi if desired */
static void
loadAnother()
{
	/* add a new PM, fill and show */
	growPM();
	fillPM();
	showPM();
}

/* PB to start/stop movie */
/* ARGSUSED */
static void
runCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* fresh message */
	msg ("");

	if (bl_id) {
	    stopBlinking();
	} else {
	    if (npminfo < 2) {
		msg ("Need at least 2 images for a movie.");
		XmToggleButtonSetState (w, False, True);
	    } else {
		nextStep();
		showPM();
		startBlinking();
	    }
	}
}

static void
startBlinking()
{
	int interval;

	if (bl_id) {
	    XtRemoveTimeOut (bl_id);
	    bl_id = (XtIntervalId)0;
	} else
	    wlprintf (ss_w, "Stop");

	XmScaleGetValue (speed_w, &interval);
	bl_id = XtAppAddTimeOut (app, interval, blink_to, NULL);
}

static void
stopBlinking()
{
	wlprintf (ss_w, "Run");

	if (bl_id) {
	    XtRemoveTimeOut (bl_id);
	    bl_id = (XtIntervalId)0;
	}
}

/* PB to reset */
/* ARGSUSED */
static void
newCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	stopBlinking();
	if (!state.fimage.image) {
	    msg ("No image");
	    return;
	}
	loadFirst();
	showPM();
}

/* PB to add */
/* ARGSUSED */
static void
addCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	stopBlinking();
	if (!state.fimage.image) {
	    msg ("No image");
	    return;
	}
	if (npminfo == 0) {
	    loadFirst();
	    showPM();
	} else
	    loadAnother();
}

/* PB to addAll */
/* KMI 10/19/03 */
static void
addAllCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
    FILE *fp;
    char linebuf[1024];
    char errmsg[1024];

	stopBlinking();

    fp = fopen("files.list", "r");
    if (!fp) {
        msg("Cannot open files.list");
        return;
    }


    fgets(linebuf, 1024, fp);
    while (strlen(linebuf) > 0) {
        linebuf[strlen(linebuf)-1] = '\0'; /* Trim newline */
        printf("Opening '%s'\n", linebuf);
        if (openFile(linebuf) == -1) {
            sprintf(errmsg, "Cannot open %s", linebuf);
            msg (errmsg);
            fclose(fp);
            return;
        }

        if (!state.fimage.image) {
       	    msg ("No image");
            fclose(fp);
       	    return;
       	}
       	if (npminfo == 0) {
       	    loadFirst();
       	    showPM();
       	} else {
       	    loadAnother();
        }
        fgets(linebuf, 1024, fp);
    }
    fclose(fp);
}

/* PB to delete current */
/* ARGSUSED */
static void
delCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	stopBlinking();

	/* remove current, display next */
	shrinkPM();
	showPM();
}

/* PB to close */
/* ARGSUSED */
static void
closeCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (blink_w);
}

/* unmap */
/* ARGSUSED */
static void
unmapCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	stopBlinking();
}

/* blink timeout -- display the next pixmap and repeat */
static void
blink_to (client, idp)
XtPointer client;
XtIntervalId *idp;
{
	/* show next pm */
	nextStep();
	showPM();

	/* repeat */
	startBlinking();
}

/* create a new blink control dialog, save it in blink_w.
 */
static void
createBlinkw ()
{
#define	UEACH	5
#define	FRBASE	(1+UEACH*XtNumber(ctrls))
	typedef struct {
	    char *name;		/* widget name */
	    char *label;	/* label, use name if null */
	    XtCallbackProc cb;	/* callback */
	    Widget *wp;		/* widget, or NULL */
	} Ctrl;
	static Ctrl ctrls[] = {
	    {"Run", NULL, runCB, &ss_w},
	    {"Add", NULL, addCB},
	    {"Add list", NULL, addAllCB},
	    {"Delete", NULL, delCB},
	    {"New", NULL, newCB},
	    {"Close", NULL, closeCB},
	};
	Arg args[20];
	int i, n;

	/* create form */

	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNdefaultPosition, False); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_GROW); n++;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 5); n++;
	XtSetArg (args[n], XmNfractionBase, FRBASE); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	blink_w = XmCreateFormDialog (toplevel_w, "Blink", args,n);
	XtAddCallback (blink_w, XmNunmapCallback, unmapCB, NULL);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "camera Movie"); n++;
	XtSetValues (XtParent(blink_w), args, n);
	XtVaSetValues (blink_w, XmNcolormap, camcm, NULL);

	/* the controls along the bottom */
	for (i = 0; i < XtNumber(ctrls); i++) {
	    Ctrl *cp = &ctrls[i];
	    Widget w;

	    n = 0;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 1+UEACH*i); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, UEACH+UEACH*i); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    w = XmCreatePushButton (blink_w, cp->name, args, n);
	    XtAddCallback (w, XmNactivateCallback, cp->cb, NULL);
	    XtManageChild (w);
	    wlprintf (w, cp->label ? cp->label : cp->name);

	    if (cp->wp)
		*cp->wp = w;
	}

	/* position scale */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, ss_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, UEACH); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, UEACH+(FRBASE-UEACH)/2); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNscaleMultiple, 1); n++;
	XtSetArg (args[n], XmNmaximum, 1); n++; /* max must be > min*/
	XtSetArg (args[n], XmNminimum, 0); n++;
	pos_w = XmCreateScale (blink_w, "Position", args, n);
	XtAddCallback (pos_w, XmNdragCallback, posCB, NULL);
	XtAddCallback (pos_w, XmNvalueChangedCallback, posCB, NULL);
	XtManageChild (pos_w);
	set_xmstring (pos_w, XmNtitleString, "Index");

	/* speed scale */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, ss_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, UEACH+(FRBASE-UEACH)/2+1); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNminimum, 50); n++;
	XtSetArg (args[n], XmNmaximum, 1000); n++;
	XtSetArg (args[n], XmNvalue, 525); n++;
	XtSetArg (args[n], XmNprocessingDirection, XmMAX_ON_LEFT); n++;
	speed_w = XmCreateScale (blink_w, "Speed", args, n);
	XtAddCallback (speed_w, XmNdragCallback, speedCB, NULL);
	XtManageChild (speed_w);
	set_xmstring (speed_w, XmNtitleString, "Speed");

	/* label for blink header */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, speed_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	blinklabel_w = XmCreateLabel (blink_w, "N", args, n);
	XtManageChild (blinklabel_w);

	/* reciprocating TB */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, blinklabel_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNmarginWidth, 0); n++;
	cyclic_w = XmCreateToggleButton (blink_w, "Cyc", args, n);
	wlprintf (cyclic_w, "Recip");
	XtManageChild (cyclic_w);

	/* frame for drawing area */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNtopOffset, 10); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, blinklabel_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	frame_w = XmCreateFrame (blink_w, "F", args, n);
	XtManageChild (frame_w);
}

/* drag and value changed callback from position scale */
static void
posCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int pos;

	/* stop blinking */
	stopBlinking();

	/* show this pm.
	 * N.B. scale can't have min==max so can't have 0=0 when 1 image.
	 */
	XmScaleGetValue (w, &pos);
	if (pos < npminfo) {
	    currentpm = pos;
	    showPM();
	}
}

/* drag callback from speed scale */
static void
speedCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (bl_id) {
	    /* feels better with an immediate effect */
	    stopBlinking();
	    startBlinking();
	}
}

/* expose callback for blink_da_w */
static void
daExpCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *cp = (XmDrawingAreaCallbackStruct *)call;
	Display *dsp = XtDisplay (w);
	Window win = XtWindow (w);
	XExposeEvent *ep = &cp->event->xexpose;

	if (pminfo) {
	    PMInfo *pip = &pminfo[currentpm];
	    Pixmap pm = pip->pm;

	    /* might be called just to finished reference blinking */
	    if (want_ref) {
		blinkRef1();
		showPM();
		want_ref = 0;
		return;
	    }

	    wlprintf (blinklabel_w, pip->title);
	    XCopyArea (dsp, pm, win, state.daGC, ep->x, ep->y, ep->width,
						    ep->height, ep->x, ep->y);
	} else {
	    wlprintf (blinklabel_w, "");
	    XClearWindow (dsp, win);
	}
}

static void
nextStep()
{
	if (XmToggleButtonGetState(cyclic_w)) {
	    if (cyc_backwards) {
		if (--currentpm < 0) {
		    cyc_backwards = 0;
		    currentpm = 1;
		}
	    } else {
		if (++currentpm == npminfo) {
		    cyc_backwards = 1;
		    currentpm = npminfo - 2;
		}
	    }
	} else {
	    currentpm = (cyc_backwards ? (currentpm-1 + npminfo)
	    			       : (currentpm+1)) % npminfo;
	}
}

/* establish current as first aoi */
static void
setRefAOI()
{
	AOI *ap = &state.aoi;
	int x = ap->x;
	int y = ap->y;

	image2window (&x, &y);
	refaoi.wx = x;
	refaoi.wy = y;
	refaoi.ww = ap->w*state.mag/MAGDENOM;
	refaoi.wh = ap->h*state.mag/MAGDENOM;
}

/* discard pminfo list, if any */
static void
resetPM()
{
	if (pminfo) {
	    Display *dsp = XtDisplay (blink_w);
	    while (--npminfo >= 0)
		XFreePixmap (dsp, pminfo[npminfo].pm);
	    XtFree ((void *)pminfo);
	    pminfo = NULL;
	    npminfo = 0;
	}

	currentpm = 0;
}

/* insert a new pixmap into pminfo of size refaoi.w/h after currentpm */
static void
growPM()
{
	Display *dsp = XtDisplay (blink_w);
	Window win = RootWindow (dsp, DefaultScreen(dsp));
	int w = refaoi.ww;
	int h = refaoi.wh;
	Pixmap pm;
	int i;

	pm = XCreatePixmap (dsp, win, w, h, DefaultDepth (dsp, 0));
	XFillRectangle (dsp, pm, state.daGC, 0, 0, w, h);

	npminfo++;
	pminfo = (PMInfo *)XtRealloc ((void *)pminfo, npminfo*sizeof(PMInfo));
	currentpm = (currentpm+1)%npminfo;
	for (i = npminfo; --i > currentpm; )
	    pminfo[i] = pminfo[i-1];

	pminfo[currentpm].pm = pm;
}

/* remove currentpm from pminfo and move down 1 */
static void
shrinkPM()
{
	Display *dsp = XtDisplay (blink_w);
	int i;

	if (!pminfo)
	    return;

	XFreePixmap (dsp, pminfo[currentpm].pm);
	npminfo -= 1;
	for (i = currentpm; i < npminfo; i++)
	    pminfo[i] = pminfo[i+1];
	if (npminfo == 0) {
	    XFree ((void *)pminfo);
	    pminfo = NULL;
	    currentpm = 0;
	} else {
	    pminfo = (PMInfo *)XtRealloc((void *)pminfo,npminfo*sizeof(PMInfo));
	    if (currentpm >= npminfo)
		currentpm = npminfo-1;
	}
}

/* fill current PM with stuff under the current AOI.
 */
static void
fillPM()
{
	Display *dsp = XtDisplay (blink_w);
	Pixmap pm = pminfo[currentpm].pm;
	XImage *ip = state.ximagep;
	AOI *ap = &state.aoi;
	char hdr[MAXLAB];
	int x, y, w, h;

	/* get current AOI in window units */
	x = ap->x;
	y = ap->y;
	image2window (&x, &y);
	w = ap->w*state.mag/MAGDENOM;
	h = ap->h*state.mag/MAGDENOM;

	/* ul corners will align, caveat emptor about sizes */
	XPutImage (dsp, pm, state.daGC, ip, x, y, x-refaoi.wx, y-refaoi.wy,w,h);

	/* do the title while we are at it */
	mkHeader (hdr);
	strcpy (pminfo[currentpm].title, hdr);
}

/* load pminfo[currentpm] into blink_da_w, and update position scale */
static void
showPM()
{
	Display *dsp = XtDisplay (blink_da_w);
	Window win = XtWindow (blink_da_w);
	XmString xmstr;
	char str[1024];
	int w = refaoi.ww;
	int h = refaoi.wh;
	int max;

	if (pminfo) {
	    PMInfo *pip = &pminfo[currentpm];

	    wlprintf (blinklabel_w, pip->title);
	    XCopyArea (dsp, pip->pm, win, state.daGC, 0, 0, w, h, 0, 0);

	    max = npminfo-1;
	    if (max == 0)
		max = 1;
	    sprintf (str, "Index: %2d of %-2d", currentpm+1, npminfo);
	} else {
	    wlprintf (blinklabel_w, "");
	    XClearWindow (dsp, win);

	    max = 1;
	    currentpm = 0;
	    sprintf (str, "Index:  0 of 0 ");
	}

	/* setting all at once avoids jumps -- and scale can't have min==max */
	xmstr = XmStringCreateSimple (str);
	XtVaSetValues (pos_w,
	    XmNmaximum, max,
	    XmNvalue, currentpm,
	    XmNtitleString, xmstr,
	    NULL);
	XmStringFree (xmstr);
}

/* (re)create the drawing area the size of aoi0.w/h within frame_w */
static void
newDA()
{
	Arg args[20];
	int n;

	if (blink_da_w)
	    XtDestroyWidget (blink_da_w);

	n = 0;
	XtSetArg (args[n], XmNwidth, refaoi.ww); n++;
	XtSetArg (args[n], XmNheight, refaoi.wh); n++;
	blink_da_w = XmCreateDrawingArea (frame_w, "DA", args, n);
	XtAddCallback (blink_da_w, XmNexposeCallback, daExpCB, NULL);
	XtManageChild (blink_da_w);
}

/* do as much towards loading and blinking a reference to the current object
 * as we can without actually needing the blink_w (actually, blink_da_w's
 * window for showPM()). this amounts to looking up the reference image and
 * reading and putting its AOI into effect into the current image, which is
 * the "test" image. if we return ok, the next step is to load this as the 
 * first frame of the movie with loadFirst(), then load up the reference image.
 * return 0 if ok, else -1.
 */
static int
blinkRef0()
{
	FImage *fip = &state.fimage;
	char obj[80];
	double tmp, tmp2;
	AOI aoi;

	/* make sure we even have an image */
	if (!fip->image) {
	    msg ("No image");
	    return (-1);
	}

	/* make sure we even have WCS headers */
	if (xy2RADec (fip, 0.0, 0.0, &tmp, &tmp2) < 0) {
	    msg ("No WCS headers");
	    return (-1);
	}

	/* get the OBJECT */
	if (getStringFITS (fip, "OBJECT", obj) < 0) {
	    msg ("Current image has no OBJECT field.");
	    return (-1);
	}

	/* lookup the reference image and its bounding box */
	if (findRefImage (obj,&ref_era,&ref_ndec,&ref_wra,&ref_sdec,ref_fn) < 0)
	    return (-1);
	
	/* find bounding box as an AOI in current (test) image */
	if (bb2AOI (fip, ref_era, ref_ndec, ref_wra, ref_sdec, &aoi) < 0)
	    return (-1);

	/* hold the current contrast */
	noAutoWin();

	/* force it into effect */
	forceAOI (&aoi);

	/* next step is loadFirst() but we don't know whether we have
	 * blink_da_w with a window for showPM yet, so that and the rest
	 * get defferred to the expose handler.
	 */

	/* ok so far */
	return (0);
}

/* do the remaining work of blinking with reference.
 * the test image has already been loaded as the first movie frame.
 * now we do the reference image.
 * this requires blink_w because we need blink_da_w's window.
 * just bale out if trouble.
 */
static void
blinkRef1()
{
	FImage *fip = &state.fimage;
	AOI aoi;

	/* display the reference image */
	if (openFile (ref_fn) < 0)
	    return;

	/* find bounding box as an AOI in current (reference) image */
	if (bb2AOI (fip, ref_era, ref_ndec, ref_wra, ref_sdec, &aoi) < 0)
	    return;

	/* force it into effect */
	forceAOI (&aoi);

	/* ok!! */
}

/* find the image AOI for the given bounding box the given image.
 * return 0 if any of it is within the given image, else -1.
 */
static int
bb2AOI (FImage *fip, double er, double nd, double wr, double sd, AOI *ap)
{
	double sw = fip->sw;
	double sh = fip->sh;
	double x0, y0, x1, y1;
	double w, h;

	/* get corners in pixels */
	if(RADec2xy(fip,er,nd,&x0,&y0)<0 || RADec2xy(fip,wr,sd,&x1,&y1)<0) {
	    msg ("WCS headers disappeared!!");
	    return (-1);
	}

	/* AOI wants ul corner */
	if (x0 > x1) {double tmp = x0; x0 = x1; x1 = tmp;}
	if (y0 > y1) {double tmp = y0; y0 = y1; y1 = tmp;}

	/* confine to image size */
	if (x0 < 0) x0 = 0;
	if (x1 >= sw) x1 = sw;
	if (y0 < 0) y0 = 0;
	if (y1 >= sh) y1 = sh;

	/* anything left? */
	w = x1 - x0 + 1;		/* inclusive */
	h = y1 - y0 + 1;		/* inclusive */
	if (w < MINSZ || h < MINSZ) {
	    msg ("Reference AOI is not well within image.");
	    return (-1);
	}

	/* load ap */
	ap->x = (int)floor(x0 + 0.5);
	ap->y = (int)floor(y0 + 0.5);
	ap->w = (int)floor(w + 0.5);
	ap->h = (int)floor(h + 0.5);

	/* ok */
	return (0);
}

/* set state.aoi to *ap and make it so.
 * N.B. ap is already in image coords and we assume within the current image.
 */
static void
forceAOI (AOI *ap)
{
	/* TODO: what if currently cropped? */
	state.aoi.x = ap->x;
	state.aoi.y = ap->y;
	state.aoi.w = ap->w;
	state.aoi.h = ap->h;

	drawAOI (True, &state.aoi);
	updateAOI();
	if (state.aoistats || state.crop) {
	    computeStats();
	    setWindow();
	    updateWin();
	    newXImage();
	}
}

/* look up obj in index. if find it return ref file name and bounding box.
 * return 0 if ok, else -1.
 */
static int
findRefImage (char *obj, double *era, double *ndec, double *wra, double *sdec,
char *fn)
{
	char line[1024];
	char fileobj[128];
	char fileera[128];
	char filendec[128];
	char filewra[128];
	char filesdec[128];
	char *idxfn, *bi;
	FILE *fp;

	/* fetch name of index file */
	idxfn = getXRes (toplevel_w,"RefImagesFile","archive/config/blink.idx");
	bi = basenm(idxfn);

	/* open the file listing the references */
	fp = telfopen (idxfn, "r");
	if (!fp) {
	    msg ("%s: %s", bi, strerror(errno));
	    return (-1);
	}

	/* search for the line for obj */
	while (fgets (line, sizeof(line), fp)) {
	    if (sscanf (line, "%s %s %s %s %s %s", fileobj, fileera, filendec,
		    filewra, filesdec, fn) == 6 && !strcwcmp (obj, fileobj))
		break;
	}
	if (feof(fp) || ferror(fp)) {
	    msg ("%s not in %s", obj, bi);
	    fclose (fp);
	    return (-1);
	}
	fclose (fp);

	/* sanity check the fn -- avoids a blank movie window later */
	fp = telfopen (fn, "r");
	if (!fp) {
	    msg ("%s in %s but %s", basenm(fn), bi, strerror(errno));
	    return (-1);
	}
	fclose (fp);

	/* found the reference image -- now crack the bounding box */
	if (scansex (fileera, era) < 0) {
	    msg ("%s: Bad Eastern RA: %s", fn, fileera);
	    return (-1);
	}
	*era = hrrad(*era);
	if (scansex (filendec, ndec) < 0) {
	    msg ("%s: Bad Northern Dec: %s", fn, filendec);
	    return (-1);
	}
	*ndec = degrad(*ndec);
	if (scansex (filewra, wra) < 0) {
	    msg ("%s: Bad Western RA: %s", fn, filewra);
	    return (-1);
	}
	*wra = hrrad(*wra);
	if (scansex (filesdec, sdec) < 0) {
	    msg ("%s: Bad Southern Dec: %s", fn, filesdec);
	    return (-1);
	}
	*sdec = degrad(*sdec);

	/* ok! */
	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: blink.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $"};
