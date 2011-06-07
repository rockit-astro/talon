/* this file contains the code to put up misc messages: xe_msg().
 * everything goes to the message dialog.
 * the latter can be toggled on/off from the main menu.
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#if defined(__STDC__)
#include <stdlib.h>
#endif
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include <Xm/Text.h>
#include <Xm/MessageB.h>

#include "P_.h"
#include "preferences.h"


extern Widget toplevel_w;
extern Colormap xe_cm;

extern int isUp P_((Widget w));
extern void prompt_map_cb P_((Widget w, XtPointer client, XtPointer call));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void wtip P_((Widget w, char *tip));

static void msg_on_top P_((void));
static void msg_create_w P_((void));
static void msg_erase_cb P_((Widget w, XtPointer client, XtPointer call));
static void msg_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void msg_add P_((char *msg));
static void msg_scroll_down P_((void));
static void xe_msg_modal P_((char *p));


#define	MINBELLSECS	3	/* min time between bell rings, seconds */

static Widget msg_w;		/* main form dialog widget */
static Widget txt_w;		/* scrolled text widget */
static int txtl;		/* current length of text in txt_w */

/* called to force the scrolling message window to be up.
 */
void
msg_manage()
{
	if (!msg_w)
	    msg_create_w();

	XtManageChild (msg_w);
	if (XtWindow(msg_w))
	    XMapRaised (XtDisplay(msg_w), XtWindow(msg_w));
}

/* ring the bellm but avoid overdoing a lot of them */
void
msg_bell()
{
	static long lastbellt;
	long t;

	t = time (NULL);

	if (t - lastbellt >= MINBELLSECS) {
	    XBell (XtDisplay(toplevel_w), 0);
	    lastbellt = t;
	}
}

/* add the msg[] to the message list.
 * if app_modal also show in an obnoxious app modal box.
 */
void
xe_msg (msg, app_modal)
char *msg;
int app_modal;
{
	/* if this is the first message, create the message box */
	if (!msg_w)
	    msg_create_w();

	/* add to message list */
	msg_add (msg);

	/* if model, display directly too, and that's all */
	if (app_modal && isUp(toplevel_w)) {
	    xe_msg_modal (msg);
	    return;
	}

	/* if we are managed we bully right under the cursor. */
	if (XtIsManaged(msg_w))
	    msg_on_top();

	/* and ring the bell if turned on */
	if (pref_get (PREF_MSG_BELL) == PREF_MSGBELL)
	    msg_bell();
}

/* called to put up or remove the watch cursor.  */
void
msg_cursor (c)
Cursor c;
{
	Window win;

	if (msg_w && (win = XtWindow(msg_w)) != 0) {
	    Display *dsp = XtDisplay(msg_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* do everything we can to make sure the message dialog is visible.
 */
static void
msg_on_top()
{
#ifdef BULLY
	Window win;

	/* bring to the top right under the cursor
	 * N.B. we can't if this is the first time but don't need to in that
	 *   case anyway because that means nothing is on top of us yet.
	 */
	win = XtWindow(XtParent(msg_w));
	if (win) {
	    XRaiseWindow(XtDisplay(XtParent(msg_w)), win);
	    XtCallCallbacks (msg_w, XmNmapCallback, NULL);
	}

	XFlush (XtDisplay(toplevel_w));
#endif
}

/* create the message dialog */
static void
msg_create_w()
{
	static struct {
	    char *name;
	    void (*cb)();
	    char *tip;
	} cb[] = {
	    {"Erase", msg_erase_cb, "Erase messages"},
	    {"Close", msg_close_cb, "Close this dialog"},
	};
	Widget w;
	Arg args[20];
	int i, n;

	/* make the help shell form-dialog widget */

	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNfractionBase, 9); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	msg_w = XmCreateFormDialog (toplevel_w, "Message", args, n);
	set_something (msg_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (msg_w, XmNmapCallback, prompt_map_cb, NULL);
	set_something (XtParent(msg_w),XmNtitle,(XtArgVal)"xephem Message Box");

	/* make the control buttons */

	for (i = 0; i < XtNumber(cb); i++) {
	    n = 0;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomOffset, 10); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 1 + 5*i); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 3 + 5*i); n++;
	    w = XmCreatePushButton (msg_w, cb[i].name, args, n);
	    XtAddCallback (w, XmNactivateCallback, cb[i].cb, NULL);
	    wtip (w, cb[i].tip);
	    XtManageChild (w);
	}

	/* make the scrolled text area to hold the messages */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomOffset, 10); n++;
	XtSetArg (args[n], XmNbottomWidget, w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	XtSetArg (args[n], XmNeditable, False); n++;
	XtSetArg (args[n], XmNcursorPositionVisible, False); n++;
	txt_w = XmCreateScrolledText (msg_w, "Messages", args, n);
	XtManageChild (txt_w);
}

/* callback from the erase pushbutton */
/* ARGSUSED */
static void
msg_erase_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmTextReplace (txt_w, 0, txtl, "");
	txtl = 0;
	XFlush (XtDisplay(toplevel_w));
}

/* callback from the close pushbutton */
/* ARGSUSED */
static void
msg_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (msg_w);
}

/* add msg to the txt_w widget.
 * Always set the vertical scroll bar to the extreme bottom.
 */
static void
msg_add (msg)
char *msg;
{
	int l;

	l = strlen(msg);
	if (l == 0)
	    return;

	XmTextReplace (txt_w, txtl, txtl, msg);
	txtl += l;

	if (msg[l-1] != '\n')
	    msg_add ("\n");
	else
	    msg_scroll_down();
}

/* make sure the text is scrolled to the bottom */
static void
msg_scroll_down()
{
	XmTextSetInsertionPosition (txt_w, txtl);
}

/* print a message, p, in an app-modal dialog. */
static void
xe_msg_modal (p)
char *p;
{
	static Widget apmsg_w;
	Arg args[20];
	int n;

	if (!apmsg_w) {
	    XmString button_string;
	    XmString title_string;
	    Widget w;

	    button_string = XmStringCreate("Ok", XmSTRING_DEFAULT_CHARSET);
	    title_string = XmStringCreate ("xephem Modal Message",
						XmSTRING_DEFAULT_CHARSET);

	    /* Create MessageBox dialog. */
	    n = 0;
	    XtSetArg (args[n], XmNtitle, "xephem Message");  n++;
	    XtSetArg (args[n], XmNokLabelString, button_string);  n++;
	    XtSetArg (args[n], XmNdialogStyle, XmDIALOG_APPLICATION_MODAL); n++;
	    XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	    apmsg_w = XmCreateWarningDialog (toplevel_w, "ModalMessage",args,n);
	    set_something (apmsg_w, XmNcolormap, (XtArgVal)xe_cm);
	    XtAddCallback (apmsg_w, XmNmapCallback, prompt_map_cb, NULL);

	    XmStringFree (title_string);
	    XmStringFree (button_string);

	    w = XmMessageBoxGetChild (apmsg_w, XmDIALOG_CANCEL_BUTTON);
	    XtUnmanageChild (w);
	    w = XmMessageBoxGetChild (apmsg_w, XmDIALOG_HELP_BUTTON);
	    XtUnmanageChild (w);
	}

	set_xmstring (apmsg_w, XmNmessageString, p);

	/* Display help window. rely on autoUnmanage to bring back down. */
	XtManageChild (apmsg_w);

	/* make sure it's up because we are occasionally used when the main
	 * loop might now run for a while
	 */
	XmUpdateDisplay (toplevel_w);
	XSync (XtDisplay(toplevel_w), False);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: msgmenu.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
