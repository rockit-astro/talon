#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <Xm/CascadeB.h>
#include <Xm/MessageB.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/Xm.h>

#include "xtools.h"

/* handy way to set one resource for a widget.
 * shouldn't use this if you have several things to set for the same widget.
 */
void set_something(w, resource, value) Widget w;
char *resource;
char *value;
{
    Arg a[1];

    if (!w)
    {
        printf("set_something (%s) called with w==0\n", resource);
        exit(1);
    }

    XtSetArg(a[0], resource, value);
    XtSetValues(w, a, 1);
}

/* handy way to get one resource for a widget.
 * shouldn't use this if you have several things to get for the same widget.
 */
void get_something(w, resource, value) Widget w;
char *resource;
char *value;
{
    Arg a[1];

    if (!w)
    {
        printf("get_something (%s) called with w==0\n", resource);
        exit(1);
    }

    XtSetArg(a[0], resource, value);
    XtGetValues(w, a, 1);
}

/* return the given XmString resource from the given widget as a char *.
 *   N.B. OUR caller should always  XtFree (*txtp).
 */
void get_xmstring(w, resource, txtp) Widget w;
char *resource;
char **txtp;
{
    static char me[] = "get_xmstring()";

    if (!w)
    {
        printf("%s: called for %s with w==0\n", me, resource);
        exit(1);
    }
    else
    {
        XmString str;
        get_something(w, resource, (char *)&str);

        // KMI - XmStringGetLtoR is deprecated and causes crash in Redhat > 9.0
        //       Replace with XmStringUnparse according to
        //       http://www.faqs.org/faqs/motif-faq/part6/section-59.html
        if (str != NULL)
        {
            // printf("Trying to unparse %s\n", (char *)str);
            *txtp = XmStringUnparse(str, NULL, 0, XmCHARSET_TEXT, NULL, 0, 0);
            // printf("Unparsed '%s'\n", *txtp);
            XmStringFree(str);
            // printf("Freed\n");
        }
        else
        {
            printf("Warning: tried to unparse null\n");
            *txtp = (char *)malloc(3);
            strcpy(*txtp, "??");
        }
    }
}

void set_xmstring(w, resource, txt) Widget w;
char *resource;
char *txt;
{
    XmString str;

    if (!w)
    {
        printf("set_xmstring called for %s with w==0\n", resource);
        return;
    }

    str = XmStringCreateLtoR(txt, XmSTRING_DEFAULT_CHARSET);
    set_something(w, resource, (char *)str);
    XmStringFree(str);
}

/* print something to the labelString XmString resource of the given widget.
 * take care not to do the i/o if it's the same string again.
 */
void wlprintf(Widget w, char *fmt, ...)
{
    va_list ap;
    char newbuf[512], *txtp;

    va_start(ap, fmt);
    vsprintf(newbuf, fmt, ap);
    va_end(ap);

    get_xmstring(w, XmNlabelString, &txtp);
    if (strcmp(txtp, newbuf))
        set_xmstring(w, XmNlabelString, newbuf);
    XtFree(txtp);
}

/* print something to the value String resource of the given text widget.
 * take care not to do the i/o if it's the same string again.
 */
void wtprintf(Widget w, char *fmt, ...)
{
    va_list ap;
    char newbuf[512];
    String txtp;

    va_start(ap, fmt);
    vsprintf(newbuf, fmt, ap);
    va_end(ap);

    get_something(w, XmNvalue, (char *)&txtp);
    if (strcmp(txtp, newbuf))
        set_something(w, XmNvalue, newbuf);
    XtFree(txtp);
}

/* given any widget, walk up the hierachy to find its shell ancestor */
Widget getShell(Widget w)
{
    while (!XtIsShell(w))
        w = XtParent(w);
    return (w);
}

/* given any widget, raise its ancestor shell to the front of stacking order.
 * also move so ul corner is on same screen as toplevel shell.
 * until proven with other window managers, provide an escape hatch.
 */
void raiseShell(Widget w)
{
    static int tried_env, move_ok;
    Widget shell = getShell(w);
    Display *dsp = XtDisplay(shell);
    Window win = XtWindow(shell);

    if (!tried_env)
    {
        char *nsm = getenv("NOSHELLMOVE");
        move_ok = !nsm;
        tried_env = 1;
    }

    if (move_ok)
    {
        Widget top = XtParent(shell);
        Position sx, sy;
        Position tx, ty;
        int scrw, scrh;

        XtVaGetValues(shell, XmNx, &sx, XmNy, &sy, NULL);

        XtVaGetValues(top, XmNx, &tx, XmNy, &ty, NULL);

        scrw = DisplayWidth(dsp, DefaultScreen(dsp));
        scrh = DisplayHeight(dsp, DefaultScreen(dsp));

        tx += 10 * scrw;
        ty += 10 * scrh;
        sx += 10 * scrw;
        sy += 10 * scrh;

        XtVaSetValues(shell, XmNx, ((tx / scrw) * scrw) + (sx % scrw) - 10 * scrw, XmNy,
                      ((ty / scrh) * scrh) + (sy % scrh) - 10 * scrh, NULL);
    }

    XRaiseWindow(dsp, win);
}

/* search for the named X resource from all the usual places.
 * this looks in more places than XGetDefault().
 * "any" can be any widget in the program.
 * we just return it as a string -- caller can do whatever.
 * return def if can't find it anywhere.
 * N.B. memory returned is _not_ malloced so leave it be.
 */
char *getXRes(Widget any, char *name, char *def)
{
    static char notfound[] = "_Not_Found_";
    char *res = NULL;
    Widget shell;
    XtResource xr;

    xr.resource_name = name;
    xr.resource_class = "AnyClass";
    xr.resource_type = XmRString;
    xr.resource_size = sizeof(String);
    xr.resource_offset = 0;
    xr.default_type = XmRImmediate;
    xr.default_addr = (XtPointer)notfound;

    shell = getShell(any);
    XtGetApplicationResources(shell, (void *)&res, &xr, 1, NULL, 0);
    if (!res || strcmp(res, notfound) == 0)
        res = def;

    return (res);
}

/* get a pixel for the given name.
 * use display and colormap from w.
 */
Pixel getColor(Widget w, char *name)
{
    Display *dsp = XtDisplay(w);
    XColor defxc, dbxc;
    Colormap cm;
    Pixel p;

    XtVaGetValues(w, XmNcolormap, &cm, NULL);

    if (XAllocNamedColor(dsp, cm, name, &defxc, &dbxc))
        p = defxc.pixel;
    else
        p = WhitePixel(dsp, DefaultScreen(dsp));

    return (p);
}

/* given any widget built from an XmLabel return pointer to the first
 * XFontStruct in its XmFontList.
 */
void get_xmlabel_font(Widget w, XFontStruct **f)
{
    static char me[] = "get_xmlable_font";
    XmFontList fl;
    XmFontContext fc;
    XmStringCharSet charset;

    get_something(w, XmNfontList, (char *)&fl);
    if (XmFontListInitFontContext(&fc, fl) != True)
    {
        printf("%s: No Font context!\n", me);
        exit(1);
    }
    if (XmFontListGetNextFont(fc, &charset, f) != True)
    {
        printf("%s: no font!\n", me);
        exit(1);
    }
    XmFontListFreeFontContext(fc);
}

/* explicitly handle pending X events when otherwise too busy */
void XCheck(XtAppContext app)
{
    while ((XtAppPending(app) & XtIMXEvent) == XtIMXEvent)
        XtAppProcessEvent(app, XtIMXEvent);
}

/* make a pusbutton look like a label */
void lookLikeLabel(pb) Widget pb;
{
    Pixel bg;

    get_something(pb, XmNbackground, (char *)&bg);
    XtVaSetValues(pb, XmNtopShadowColor, bg, XmNbottomShadowColor, bg, XmNfillOnArm, False, XmNtraversalOn, False,
                  NULL);
}

/* a little dialog to let the user stop a lengthy action.
 * call stopchk_up() once to bring up the dialog.
 * call stopchk() occasionally.
 * call stopchk_down() when finished (stopped or not).
 */

static void stopchkCB(Widget w, XtPointer client, XtPointer call);

static Widget stopchk_w;
static int stopchk_stopped;

/* pop up a modal dialog to allow aborting a length op */
void stopchk_up(Widget shell_w, char *title, char *message)
{
    if (!stopchk_w)
    {
        Arg args[20];
        Colormap cm;
        int n;

        XtVaGetValues(shell_w, XmNcolormap, &cm, NULL);

        n = 0;
        XtSetArg(args[n], XmNcolormap, cm);
        n++;
        XtSetArg(args[n], XmNautoUnmanage, False);
        n++;
        XtSetArg(args[n], XmNdialogStyle, XmDIALOG_APPLICATION_MODAL);
        n++;
        stopchk_w = XmCreateInformationDialog(shell_w, "UStop", args, n);
        XtVaSetValues(stopchk_w, XmNcolormap, cm, NULL);
        XtAddCallback(stopchk_w, XmNokCallback, stopchkCB, (XtPointer)0);
        XtUnmanageChild(XmMessageBoxGetChild(stopchk_w, XmDIALOG_CANCEL_BUTTON));
        XtUnmanageChild(XmMessageBoxGetChild(stopchk_w, XmDIALOG_HELP_BUTTON));
        set_xmstring(stopchk_w, XmNokLabelString, "Stop");
    }

    set_xmstring(stopchk_w, XmNmessageString, message);
    XtVaSetValues(XtParent(stopchk_w), XmNtitle, title, NULL);

    XtManageChild(stopchk_w);
    stopchk_stopped = 0;
}

/* bring down the user stop dialog.
 */
void stopchk_down()
{
    if (stopchk_w)
        XtUnmanageChild(stopchk_w);
    stopchk_stopped = 0;
}

/* poll whether the user wants to stop.
 * return 1 to stop, else 0.
 */
int stopchk()
{
    /* check for user button presses */
    XCheck(XtWidgetToApplicationContext(stopchk_w));

    return (stopchk_stopped);
}

/* called when the user presses the Stop button */
/* ARGSUSED */
static void stopchkCB(w, client, call) Widget w;
XtPointer client;
XtPointer call;
{
    stopchk_stopped = 1;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid,
                         "@(#) $RCSfile: xtools.c,v $ $Date: 2006/05/28 01:07:19 $ $Revision: 1.2 $ $Name:  $"};
