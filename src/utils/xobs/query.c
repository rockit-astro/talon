/* general purpose way to ask a question, in X.
 */

#include <stdio.h>
#if defined(__STDC__)
#include <stdlib.h>
#endif
#include <X11/Xlib.h>
#include <Xm/MessageB.h>
#include <Xm/Xm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "telstatshm.h"
#include "xtools.h"

#include "xobs.h"

static void query_create P_((Widget toplevel));
static void query_cb P_((Widget w, XtPointer client, XtPointer call));

static Widget q_w;
static void (*funcs[3])();

/* put up an app-modal query message with up to three buttons.
 * all args can safely be NULL; buttons without labels will be turned off.
 */
void query(tw, msg, label0, label1, label2, func0, func1, func2) Widget tw; /* toplevel widget */
char *msg;                                                                  /* query message */
char *label0;                                                               /* label for button 0 */
char *label1;                                                               /* label for button 1 */
char *label2;                                                               /* label for button 2 */
void (*func0)();                                                            /* func to call if button 0 is pushed */
void (*func1)();                                                            /* func to call if button 1 is pushed */
void (*func2)();                                                            /* func to call if button 2 is pushed */
{
    if (!q_w)
        query_create(tw);

    funcs[0] = func0;
    funcs[1] = func1;
    funcs[2] = func2;

    if (label0)
    {
        set_xmstring(q_w, XmNokLabelString, label0);
        XtManageChild(XmMessageBoxGetChild(q_w, XmDIALOG_OK_BUTTON));
    }
    else
        XtUnmanageChild(XmMessageBoxGetChild(q_w, XmDIALOG_OK_BUTTON));

    if (label1)
    {
        set_xmstring(q_w, XmNcancelLabelString, label1);
        XtManageChild(XmMessageBoxGetChild(q_w, XmDIALOG_CANCEL_BUTTON));
    }
    else
        XtUnmanageChild(XmMessageBoxGetChild(q_w, XmDIALOG_CANCEL_BUTTON));

    if (label2)
    {
        set_xmstring(q_w, XmNhelpLabelString, label2);
        XtManageChild(XmMessageBoxGetChild(q_w, XmDIALOG_HELP_BUTTON));
    }
    else
        XtUnmanageChild(XmMessageBoxGetChild(q_w, XmDIALOG_HELP_BUTTON));

    if (msg)
        set_xmstring(q_w, XmNmessageString, msg);
    else
        set_xmstring(q_w, XmNmessageString, "?message?");

    XtManageChild(q_w);
}

static int rusure_called = 0;
static int rusure_yes = 0;
static int rusure_silent = 0;

static void rusure_yes_f()
{
    rusure_called = 1;
    rusure_yes = 1;
}

static void rusure_no_f()
{
    rusure_called = 1;
    rusure_yes = 0;
}

/* set whether we want rusure dialog or just assume yes */
void rusure_seton(int whether)
{
    rusure_silent = !whether;
}

/* return true if dialog will be used, false if we just assume yes to all q's */
int rusure_geton()
{
    return (!rusure_silent);
}

/* handy wrapper over query() to ask yes/no question, block waiting,
 * then return 1 if user picked Yes, 0 if No.
 * always just return 1 if rusure_silent.
 */
int rusure(Widget tw, char *msg)
{
    char buf[1024];

    if (rusure_silent)
        return (1);

    rusure_called = 0;
    rusure_yes = 0;

    sprintf(buf, "Are you sure you want to\n%s?", msg);
    query(tw, buf, "Yes", "No", NULL, rusure_yes_f, rusure_no_f, NULL);

    do
    {
        XCheck(app);
    } while (!rusure_called);

    return (rusure_yes);
}

static void query_create(tw) Widget tw;
{
    Arg args[20];
    int n;

    n = 0;
    XtSetArg(args[n], XmNdefaultPosition, True);
    n++;
    XtSetArg(args[n], XmNdialogStyle, XmDIALOG_APPLICATION_MODAL);
    n++;
    XtSetArg(args[n], XmNtitle, "XObs Query");
    n++;
    q_w = XmCreateQuestionDialog(tw, "Query", args, n);
    XtAddCallback(q_w, XmNokCallback, query_cb, (XtPointer)0);
    XtAddCallback(q_w, XmNcancelCallback, query_cb, (XtPointer)1);
    XtAddCallback(q_w, XmNhelpCallback, query_cb, (XtPointer)2);
}

/* ARGSUSED */
static void query_cb(w, client, call) Widget w;
XtPointer client;
XtPointer call;
{
    void (*f)() = funcs[(int)client];

    if (f)
        (*f)();
    XtUnmanageChild(w);
}
