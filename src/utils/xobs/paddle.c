/* code to display a joystick for manually overriding pointing control.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <Xm/Xm.h>
#include <X11/keysym.h>
#include <Xm/RowColumn.h>
#include <Xm/ToggleB.h>
#include <Xm/Scale.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/Form.h>
#include <Xm/Separator.h>
#include <Xm/ArrowB.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "misc.h"
#include "configfile.h"
#include "xtools.h"
#include "telstatshm.h"
#include "cliserv.h"

#include "buildcfg.h"

#include "xobs.h"

#ifndef WINDSCREEN
    #define WINDSCREEN 0
#endif
#ifndef COVER
    #define COVER 0
#endif

#if WINDSCREEN
	#define	ARROWSZ	60
#else
	#define ARROWSZ 30
#endif

static void mkPadGUI(void);
static Widget mkArrows (Widget p_w);
static Widget mkButtons (Widget p_w);
static void armArrow (Widget w);
static void disarmArrow (Widget w);
static void buttonCB (Widget w, XtPointer client, XtPointer call);
static void arrowArmCB (Widget w, XtPointer client, XtPointer call);
static void arrowDisarmCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);

static Widget paddle_w;			/* the main shell */
static Widget n_w, s_w, e_w, w_w;	/* the 4 direction buttons */
static Widget nl_w, sl_w, el_w, wl_w;	/* the 4 direction button labels */
static Widget roof_w, oi_w, coarse_w, fine_w;	/* the 4 control buttons */
#if WINDSCREEN
static Widget screenE_w, screenW_w, screenS_w;
#endif
#if COVER
static Widget cover_w;
#endif

/* toggle the paddle */
void
pad_manage ()
{
	if (!paddle_w)
	    mkPadGUI();

	if (XtIsManaged (paddle_w))
	    XtUnmanageChild (paddle_w);
	else
	    XtManageChild (paddle_w);
}

/* return to basic coarse slew state */
void
pad_reset()
{
	if (paddle_w)
	    XmToggleButtonSetState (coarse_w, True, True);
}

static void
kpEH (w, client, ep, dispatch)
Widget w;
XtPointer client;
XEvent *ep;
Boolean *dispatch;
{
	XKeyEvent *kp = (XKeyEvent *)ep;
	KeySym ks = XLookupKeysym (kp, 0);
	Widget dir_w = 0;

	/* see which arrow key, if any */
	switch (ks) {
	case XK_Up:    dir_w = n_w; break;
	case XK_Down:  dir_w = s_w; break;
	case XK_Left:  dir_w = w_w; break;
	case XK_Right: dir_w = e_w; break;
	}

	/* pretend to be the arrow button */
	if (dir_w) {
	    int press = ep->type == KeyPress;
	    XKeyboardControl kbc;

	    /* turning off autorepeat on individual keycodes did not work */
	    if (press) {
		kbc.auto_repeat_mode = AutoRepeatModeOff;
		XChangeKeyboardControl (XtDisplay(w), KBAutoRepeatMode, &kbc);
		armArrow (dir_w);
	    } else {
		kbc.auto_repeat_mode = AutoRepeatModeOn;
		XChangeKeyboardControl (XtDisplay(w), KBAutoRepeatMode, &kbc);
		disarmArrow (dir_w);
	    }
	}
}

/* create the pad GUI */
static void
mkPadGUI()
{
	Widget arrowbox_w, buttonbox_w;
	Widget close_w;
	Arg args[20];

	int n;

	n = 0;
	XtSetArg (args[n], XmNkeyboardFocusPolicy, XmEXPLICIT); n++;
	XtSetArg (args[n], XmNtraversalOn, False); n++;
	XtSetArg (args[n], XmNnavigationType, XmNONE); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 10); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_GROW); n++;
	XtSetArg (args[n], XmNtitle, "Software Hand Paddle"); n++;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	paddle_w = XmCreateFormDialog (toplevel_w, "MainF", args, n);

	/* connect up arrow keys as controls too */
	XtAddEventHandler (paddle_w,KeyPressMask|KeyReleaseMask,False,kpEH,0);

	/* arrows to left */

	arrowbox_w = mkArrows (paddle_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 45); n++;
	XtSetValues (arrowbox_w, args, n);

	/* close at the bottom */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, arrowbox_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 40); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 60); n++;
	close_w = XmCreatePushButton (paddle_w, "Close", args, n);
	XtAddCallback (close_w, XmNactivateCallback, closeCB, NULL);
	XtManageChild (close_w);

	/* control buttons to the right */

	buttonbox_w = mkButtons (paddle_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, close_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
#if WINDSCREEN	
	XtSetArg (args[n], XmNleftPosition, 65); n++;
#else
	XtSetArg (args[n], XmNleftPosition, 55); n++;
#endif
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetValues (buttonbox_w, args, n);

	/* pick one for default */
	XmToggleButtonSetState (coarse_w, True, True);
}

/* create the arrow button control panel and return the outside manager */
static Widget
mkArrows (Widget p_w)
{
	typedef struct {
	    char *name;
	    int x, y;
	    XtArgVal dir;
	    Widget *wp;
	} ArrowW;
	
	static ArrowW arrows[] = {
	  {"N",   ARROWSZ,         0, XmARROW_UP,    &n_w},
	  {"S",   ARROWSZ, 2*ARROWSZ, XmARROW_DOWN,  &s_w},
	  {"W",         0,   ARROWSZ, XmARROW_LEFT,  &w_w},
	  {"E", 2*ARROWSZ,   ARROWSZ, XmARROW_RIGHT, &e_w},
	};
	
	Arg args[20];
	Widget f_w, bb_w;
	int i, n;

	/* outter form */

	n = 0;
	f_w = XmCreateForm (p_w, "AF", args, n);
	XtManageChild (f_w);

	/* top and right labels */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	wl_w = XmCreateLabel (f_w, "T", args, n);
	XtManageChild (wl_w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	el_w = XmCreateLabel (f_w, "R", args, n);
	XtManageChild (el_w);

	/* left and right labels */
	

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, wl_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, el_w); n++;
	nl_w = XmCreateLabel (f_w, "L", args, n);
	XtManageChild (nl_w);
	
	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, wl_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, el_w); n++;
	sl_w = XmCreateLabel (f_w, "R", args, n);
	XtManageChild (sl_w);

	/* arrows in a BB  */

	n = 0;
	XtSetArg (args[n], XmNwidth, 3*ARROWSZ); n++;
	XtSetArg (args[n], XmNheight, 3*ARROWSZ); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNmarginWidth, 0); n++;
	bb_w = XmCreateBulletinBoard (f_w, "Pad", args, n);
	XtManageChild (bb_w);

	for (i = 0; i < XtNumber(arrows); i++) {
	  ArrowW *awp = &arrows[i];
	  Widget w;
	  
	  n = 0;
	  XtSetArg (args[n], XmNx, awp->x); n++;
	  XtSetArg (args[n], XmNy, awp->y); n++;
	  XtSetArg (args[n], XmNwidth, ARROWSZ); n++;
	  XtSetArg (args[n], XmNheight, ARROWSZ); n++;
	  XtSetArg (args[n], XmNarrowDirection, awp->dir); n++;
	  w = XmCreateArrowButton (bb_w, awp->name, args, n);
	  XtAddCallback (w, XmNarmCallback, arrowArmCB, NULL);
	  XtAddCallback (w, XmNdisarmCallback, arrowDisarmCB, NULL);
	  XtManageChild(w);
	  *(awp->wp) = w;
	}
	
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, nl_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, sl_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, wl_w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNrightWidget, el_w); n++;
	XtSetValues (bb_w, args, n);

	return (f_w);
}

/* make the control buttons and return the outside manager */
static Widget
mkButtons (Widget p_w)
{
	typedef struct {
	    char *name;
	    char *label;
	    int pos;
	    Widget *wp;
	    char *tip;
	} ButtonW;
	static ButtonW buttons[] = {
	    {"C", "Coarse scope", 10, &coarse_w, "Coarse telescope control"},
#if WINDSCREEN	
	    {"D", "Fine scope",   20, &fine_w,   "Fine telescope control"},
	    {"B", "Focus/Filter", 30, &oi_w,     "Focus and Filter control"},
	    {"A", "Roof/Dome",	  40, &roof_w,   "Roof and Dome control"},
	    {"E", "East wind screen",  50, &screenE_w, \
	     "East wind screen control"},
	    {"F", "West wind screen",  60, &screenW_w, \
	     "West wind screen control"},
	    {"G", "South wind screen",  70, &screenS_w, \
	     "South wind screen control"},
#elif COVER
	    {"D", "Fine scope",   25, &fine_w,   "Fine telescope control"},
	    {"B", "Focus/Filter", 40, &oi_w,     "Focus and Filter control"},
	    {"A", "Roof/Dome",	  55, &roof_w,   "Roof and Dome control"},	
	    {"H", "Cover",        70, &cover_w,  "Mirror cover control"},	
#else
	    {"D", "Fine scope",   30, &fine_w,   "Fine telescope control"},
	    {"B", "Focus/Filter", 50, &oi_w,     "Focus and Filter control"},
	    {"A", "Roof/Dome",	  70, &roof_w,   "Roof and Dome control"},	
#endif
	
	};

	Widget f_w;
	Arg args[20];
	int i, n;

	n = 0;
	f_w = XmCreateForm (p_w, "BF", args, n);
	XtManageChild (f_w);

	for (i = 0; i < XtNumber(buttons); i++) {
	    ButtonW *bwp = &buttons[i];
	    Widget w;

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNtopPosition, bwp->pos); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    XtSetArg (args[n], XmNindicatorOn, True); n++;
	    XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	    w = XmCreateToggleButton (f_w, bwp->name, args, n);
	    wlprintf (w, "%s", bwp->label);
	    XtAddCallback (w, XmNvalueChangedCallback, buttonCB, NULL);
	    XtManageChild (w);
	    wtip (w, bwp->tip);
	    *(bwp->wp) = w;
	}

	return (f_w);
}

static void
armArrow (Widget w)
{
	if (XmToggleButtonGetState(oi_w)) {
	    if (w == n_w)
		fifoMsg (Focus_Id, "j-");
	    if (w == s_w)
		fifoMsg (Focus_Id, "j+");
	    if (w == e_w)
		fifoMsg (Filter_Id, "j-");
	    if (w == w_w)
		fifoMsg (Filter_Id, "j+");
	}
	if (XmToggleButtonGetState(coarse_w)) {
	    if (w == n_w)
		fifoMsg (Tel_Id, "jN");
	    if (w == s_w)
		fifoMsg (Tel_Id, "jS");
	    if (w == e_w)
		fifoMsg (Tel_Id, "jE");
	    if (w == w_w)
		fifoMsg (Tel_Id, "jW");
	}
	if (XmToggleButtonGetState(fine_w)) {
	    if (w == n_w)
		fifoMsg (Tel_Id, "jn");
	    if (w == s_w)
		fifoMsg (Tel_Id, "js");
	    if (w == e_w)
		fifoMsg (Tel_Id, "je");
	    if (w == w_w)
		fifoMsg (Tel_Id, "jw");
	}
	if (XmToggleButtonGetState(roof_w)) {
	    if (w == n_w)
		fifoMsg (Dome_Id, "Open");
	    if (w == s_w)
		fifoMsg (Dome_Id, "Close");
	    if (w == e_w)
		fifoMsg (Dome_Id, "j+");
	    if (w == w_w)
		fifoMsg (Dome_Id, "j-");
	}
#if WINDSCREEN	
	if (XmToggleButtonGetState(screenE_w)) {
	   if (w == n_w)
	     system("windscreen 1 open");
	   if (w == s_w)
	     system("windscreen 1 close");
	}
	if (XmToggleButtonGetState(screenW_w)) {
	   if (w == n_w)
	     system("windscreen 2 open");
	   if (w == s_w)
	     system("windscreen 2 close");
	}
	if (XmToggleButtonGetState(screenS_w)) {
	   if (w == n_w)
	     system("windscreen 3 open");
	   if (w == s_w)
	     system("windscreen 3 close");
	}
#endif	
#if COVER
    if(XmToggleButtonGetState(cover_w)) {
        if(w == n_w) {
            fifoMsg(Tel_Id, "OpenCover");
        }
        if(w == s_w) {
            fifoMsg(Tel_Id, "CloseCover");
        }
    }
#endif
}

static void
disarmArrow (Widget w)
{
	if (XmToggleButtonGetState(oi_w)) {
	    if (w == n_w || w == s_w)
		fifoMsg (Focus_Id, "j0");
	    if (w == e_w || w == w_w)
		fifoMsg (Filter_Id, "j0");
	}
	if (XmToggleButtonGetState(coarse_w))
	    fifoMsg (Tel_Id, "j0");
	if (XmToggleButtonGetState(fine_w))
	    fifoMsg (Tel_Id, "j0");
	if (XmToggleButtonGetState(roof_w)) {
	    if (w == e_w || w == w_w)
		fifoMsg (Dome_Id, "j0");
	}
}

/* called when an arrow button is armed */
/* ARGSUSED */
static void
arrowArmCB (Widget w, XtPointer client, XtPointer call)
{
	armArrow (w);
}

/* called when an arrow button is disarmed */
/* ARGSUSED */
static void
arrowDisarmCB (Widget w, XtPointer client, XtPointer call)
{
	disarmArrow (w);
}

/* called when a control toggle button is pressed/released */
/* ARGSUSED */
static void
buttonCB (Widget w, XtPointer client, XtPointer call)
{
	int have;

	/* only do something when coming on */
	if (!XmToggleButtonGetState(w))
	    return;

	/* implement radio box behavior */
	if (w != roof_w)   XmToggleButtonSetState (roof_w,   False, True);
	if (w != oi_w)     XmToggleButtonSetState (oi_w,     False, True);
	if (w != coarse_w) XmToggleButtonSetState (coarse_w, False, True);
	if (w != fine_w)   XmToggleButtonSetState (fine_w,   False, True);
#if WINDSCREEN	
	if (w != screenE_w) XmToggleButtonSetState (screenE_w, False, True);
	if (w != screenW_w) XmToggleButtonSetState (screenW_w, False, True);
	if (w != screenS_w) XmToggleButtonSetState (screenS_w, False, True);
#endif
#if COVER
	if (w != cover_w) XmToggleButtonSetState (cover_w, False, True);
#endif

	/* then set up new labels and sensitivity according to context */
	if (w == fine_w || w == coarse_w) {
	    if (telstatshmp->telstate == TS_TRACKING) {
		wlprintf (nl_w, "N");
		wlprintf (sl_w, "S");
		wlprintf (el_w, " E ");
		wlprintf (wl_w, " W ");
	    } else {
		wlprintf (nl_w, "+Y");
		wlprintf (sl_w, "-Y");
		wlprintf (el_w, " +X");
		wlprintf (wl_w, "-X ");
	    }

	    have = HMOT->have;
	    XtSetSensitive (e_w, have);
	    XtSetSensitive (w_w, have);
	    have = DMOT->have;
	    XtSetSensitive (n_w, have);
	    XtSetSensitive (s_w, have);


	    /* coarse doubles as a panic stop on the real paddle but not
	     * here..
	    if (w == coarse_w && telstatshmp->telstate != TS_SLEWING)
		stop_all_devices();
	     */
	}

	if (w == roof_w) {
	    have = telstatshmp->shutterstate != SH_ABSENT;
	    wlprintf (nl_w, "Open");
	    wlprintf (sl_w, "Close");
	    XtSetSensitive (n_w, have);
	    XtSetSensitive (s_w, have);

	    have = telstatshmp->domestate != DS_ABSENT;
	    wlprintf (el_w, "CW ");
	    wlprintf (wl_w, "CCW");
	    XtSetSensitive (e_w, have);
	    XtSetSensitive (w_w, have);
	}
	
#if WINDSCREEN	
	if (w == screenW_w) {
	  have = 1;
	  wlprintf (nl_w, "Up");
	  wlprintf (sl_w, "Down");
	  XtSetSensitive (n_w, have);
	  XtSetSensitive (s_w, have);
	  
	  have = 0;
	  wlprintf (el_w, "   ");
	  wlprintf (wl_w, "   ");
	  XtSetSensitive (e_w, have);
	  XtSetSensitive (w_w, have);
	}
	if (w == screenE_w) {
	  have = 1;
	  wlprintf (nl_w, "Up");
	  wlprintf (sl_w, "Down");
	  XtSetSensitive (n_w, have);
	  XtSetSensitive (s_w, have);
	  
	  have = 0;
	  wlprintf (el_w, "   ");
	  wlprintf (wl_w, "   ");
	  XtSetSensitive (e_w, have);
	  XtSetSensitive (w_w, have);
	}
	if (w == screenS_w) {
	  have = 1;
	  wlprintf (nl_w, "Up");
	  wlprintf (sl_w, "Down");
	  XtSetSensitive (n_w, have);
	  XtSetSensitive (s_w, have);
	  
	  have = 0;
	  wlprintf (el_w, "   ");
	  wlprintf (wl_w, "   ");
	  XtSetSensitive (e_w, have);
	  XtSetSensitive (w_w, have);
	}
#endif	
#if COVER
	if (w == cover_w) {
	    wlprintf (nl_w, "Open");
	    wlprintf (sl_w, "Close");
	    XtSetSensitive (n_w, 1);
	    XtSetSensitive (s_w, 1);
	    XtSetSensitive (e_w, 0);
	    XtSetSensitive (w_w, 0);
    }
#endif

	if (w == oi_w) {
	    have = OMOT->have;
	    wlprintf (nl_w, "Out");
	    wlprintf (sl_w, "In");
	    XtSetSensitive (n_w, have);
	    XtSetSensitive (s_w, have);

	    have = IMOT->have;
	    wlprintf (el_w, "CW ");
	    wlprintf (wl_w, "CCW");
	    XtSetSensitive (e_w, have);
	    XtSetSensitive (w_w, have);
	}
}

/* called when Close is pressed */
/* ARGSUSED */
static void
closeCB (Widget w, XtPointer client, XtPointer call)
{
	XtUnmanageChild (paddle_w);
}
