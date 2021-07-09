/* this file contains code to manage the widget tips facility.
 *
 * N.B. this code assumes the strings registerd with wtip() live for the
 * lifetime of the widget. If this is not the case, malloced copies must be
 * kept here. This is performed if #define MALLOC_COPIES is set. This is not
 * the default to avoid needlessly peppering the heap with long-lived copies.
 */

#include <stdio.h>
#include <stdlib.h>
typedef const void *qsort_arg;

#include <Xm/Xm.h> /* easy way to bring in Xt stuff is all */

#include "xtools.h"

/* keep a list of the tips for each registered widget.
 * w == 0 is used to mark a recycled entry.
 */
typedef enum
{
    T_DOWN,
    T_ARMED,
    T_UP
} TState;
typedef struct
{
    Widget w;     /* the widget for this tip */
    String s;     /* its tip text */
    TState state; /* down, armed or up */
} Tip;

static void tip_init(Widget w);
static void tip_new(Widget w, String s);
static void tip_reclaim(Widget w);
static void tip_eh(Widget w, XtPointer c, XEvent *e, Boolean *cont);
static void tip_startup(Tip *tp);
static void timer_cb(XtPointer client, XtIntervalId *id);
static void tip_up(Tip *tp);
static void tip_down(Tip *tp);
static void tip_destroy_cb(Widget w, XtPointer client, XtPointer call);
static Tip *tip_find(Widget w);
static int tip_sf(qsort_arg a1, qsort_arg a2);
static void tip_sort(void);

#define GAP 5     /* gap around text, pixels */
#define BW 1      /* window border width */
#define VOFFSET 4 /* dist from event button to window, pixels */
#define TIPTO 500 /* time to wait before raising tip, ms */

static Tip *tips;              /* all current tips -- w==0 when not in use */
static int ntips;              /* total tips in tips[] */
static Window the_tipw;        /* tips window */
static GC the_tipgc;           /* tips gc */
static XFontStruct *the_tipfs; /* tips font */
static Pixel tip_fg, tip_bg;   /* foreground and background colors */
static int tips_sorted;        /* flag set when tips array is sorted */
static int tip_silent;         /* don't show tips if this is != 0 */

/* connect the given string as the tip for the given widget */
void wtip(w, tip) Widget w;
char *tip;
{
    /* gadgets don't have windows and hence no events or even a XtDisplay */
    if (XmIsGadget(w))
    {
        fprintf(stderr, "Widget is really a Gadget. tip=%s\n", tip);
        return;
    }

    /* one-time initialization */
    if (!the_tipw)
        tip_init(w);

    /* connect handlers to this widget.
     * we tried using translations but it interfered too much
     */
    XtAddEventHandler(w, EnterWindowMask, False, tip_eh, (XtPointer)1);
    XtAddEventHandler(w, LeaveWindowMask, False, tip_eh, (XtPointer)0);
    XtAddEventHandler(w, ButtonPressMask, False, tip_eh, (XtPointer)0);
    XtAddEventHandler(w, ButtonReleaseMask, False, tip_eh, (XtPointer)0);
    XtAddCallback(w, XmNdestroyCallback, tip_destroy_cb, NULL);

    /* add info to the master list */
    tip_new(w, tip);
}

/* return true if tips will be used else false */
int tip_geton()
{
    return (!tip_silent);
}

/* set whether we want tips */
void tip_seton(int whether)
{
    tip_silent = !whether;
}

/* one-time initialization stuff: create a window, font and gc */
static void tip_init(w) Widget w;
{
    Display *dsp = XtDisplay(w);
    Window root = RootWindow(dsp, DefaultScreen(dsp));
    XSetWindowAttributes wa;
    unsigned long mask;
    XGCValues gcv;

    /* create the tips window -- use same one for all */
    tip_fg = getColor(w, "#11f");
    tip_bg = getColor(w, "white");
    the_tipw = XCreateSimpleWindow(dsp, root, 0, 0, 10, 10, BW, tip_fg, tip_bg);

    /* prevent window manager from decorating */
    mask = CWOverrideRedirect;
    wa.override_redirect = True;
    XChangeWindowAttributes(dsp, the_tipw, mask, &wa);

    /* pick a font and make a GC */
    the_tipfs = XLoadQueryFont(dsp, "fixed");
    if (!the_tipfs)
    {
        printf("Tips: no fixed font ?!?\n");
        exit(1);
    }
    gcv.font = the_tipfs->fid;
    mask = GCFont;
    the_tipgc = XCreateGC(dsp, root, mask, &gcv);
}

/* add a new tip to tips[].
 * these tend to occur in bunches so just add to the end and mark the
 * array as unsorted. it will be sorted before being used.
 */
static void tip_new(w, s) Widget w;
String s;
{
    Tip *tp, *ltp;

    /* find an empty spot, or grow if none */
    for (tp = tips, ltp = &tips[ntips]; tp < ltp; tp++)
        if (tp->w == 0)
            break;
    if (tp == ltp)
    {
        /* nope -- make a new spot */
        tips = (Tip *)XtRealloc((void *)tips, (ntips + 1) * sizeof(Tip));
        tp = &tips[ntips++];
    }

    /* save widget */
    tp->w = w;

    /* save text */
#ifdef MALLOC_COPIES
    tp->s = XtNewString(s);
#else
    tp->s = s;
#endif

    /* init state */
    tp->state = T_DOWN;

    /* mark array as being unsorted now */
    tips_sorted = 0;
}

/* reclaim storage for tip info associated with w */
static void tip_reclaim(w) Widget w;
{
    Tip *tp;

    tp = tip_find(w);
    if (tp)
    {
        tp->w = (Widget)0;
#ifdef MALLOC_COPIES
        XtFree(tp->s);
#endif
        tips_sorted = 0;
        return;
    }

    /* ha? */
    fprintf(stderr, "Tips: stranger reclaimed: %s\n", XtName(w));
}

/* event handler -- c is 1 if want up, 0 for down */
static void tip_eh(w, c, e, cont) Widget w;
XtPointer c;
XEvent *e;
Boolean *cont;
{
    Tip *tp;
    int wantup;

    tp = tip_find(w);
    if (!tp)
        return;

    wantup = ((int)c == 1) && !tip_silent;
    if (wantup)
    {
        if (tp->state == T_DOWN)
        {
            tip_startup(tp);
            tp->state = T_ARMED;
        }
    }
    else
    {
        if (tp->state == T_UP)
            tip_down(tp);
        tp->state = T_DOWN;
    }
}

/* start a timer which, if it springs, will pop up the given tip */
static void tip_startup(tp) Tip *tp;
{
    XtAppContext ac = XtWidgetToApplicationContext(tp->w);

    /* N.B. tp can move if realloced, index can move if sorted. */
    (void)XtAppAddTimeOut(ac, TIPTO, timer_cb, (XtPointer)(tp->w));
}

/* timer expired so pop up tip for widget in client unless went DOWN again */
/* ARGSUSED */
static void timer_cb(client, id) XtPointer client;
XtIntervalId *id;
{
    Widget w = (Widget)client;
    Tip *tp = tip_find(w);

    /* conceivable it was destroyed */
    if (!tp)
        return;

    if (tp->state == T_ARMED)
    {
        tip_up(tp);
        tp->state = T_UP;
    }
}

/* pop up the given tip */
static void tip_up(tp) Tip *tp;
{
    Display *dsp = XtDisplay(tp->w);
    int dw = DisplayWidth(dsp, 0);
    int dh = DisplayHeight(dsp, 0);
    int l = strlen(tp->s);
    int dir, asc, des;
    int wid, hei;
    XCharStruct oa;
    Position x, y;
    Dimension ww, wh;
    Arg a[2];
    XSetWindowAttributes wa;
    unsigned long mask;

    /* compute size of window to wrap nicely around text.
     * location should be below and offset a bit, unless near screen edge.
     */
    XTextExtents(the_tipfs, tp->s, l, &dir, &asc, &des, &oa);
    wid = oa.width + 2 * GAP;
    hei = oa.ascent + oa.descent + 2 * GAP;
    XtSetArg(a[0], XmNwidth, (XtArgVal)&ww);
    XtSetArg(a[1], XmNheight, (XtArgVal)&wh);
    XtGetValues(tp->w, a, XtNumber(a));
    XtTranslateCoords(tp->w, (int)ww / 3, (int)wh + VOFFSET, &x, &y);
    if (x + wid + 2 * BW > dw)
        x = dw - wid - 2 * BW;
    if (y + hei + 2 * BW > dh)
        y -= VOFFSET + wh + VOFFSET + hei + 2 * BW;
    XMoveResizeWindow(dsp, the_tipw, x, y, wid, hei);

    /* set colors to match source */
    mask = CWBackPixel | CWBorderPixel;
    wa.background_pixel = tip_bg;
    wa.border_pixel = tip_fg;
    XChangeWindowAttributes(dsp, the_tipw, mask, &wa);
    XClearWindow(dsp, the_tipw);
    XMapRaised(dsp, the_tipw);
    XSync(dsp, 0); /* so we don't need to wait for Expose elsewhere */

    /* draw the text */
    XSetForeground(dsp, the_tipgc, tip_fg);
    XDrawString(dsp, the_tipw, the_tipgc, GAP, oa.ascent + GAP, tp->s, l);
}

/* pop down the tip window */
static void tip_down(tp) Tip *tp;
{
    XUnmapWindow(XtDisplay(tp->w), the_tipw);
}

/* w is being destroyed so reclaim its tip info */
/* ARGSUSED */
static void tip_destroy_cb(w, client, call) Widget w;
XtPointer client;
XtPointer call;
{
    tip_reclaim(w);
}

/* find the Tip for the given widget, else NULL.
 * use a binary search. sort the array first if necessary.
 */
static Tip *tip_find(w) Widget w;
{
    long wl = (long)w;
    long s;
    int t, b, m;

    if (!tips_sorted)
        tip_sort();

    /* binary search */
    t = ntips - 1;
    b = 0;
    while (b <= t)
    {
        m = (t + b) / 2;
        s = wl - (long)(tips[m].w);
        if (s == 0)
            return (&tips[m]);
        if (s < 0)
            t = m - 1;
        else
            b = m + 1;
    }

    return (NULL);
}

/* qsort-style function to sort the tips array by widget */
static int tip_sf(a1, a2) qsort_arg a1, a2;
{
    Tip *t1 = (Tip *)a1;
    Tip *t2 = (Tip *)a2;
    long d = (long)(t1->w) - (long)(t2->w);

    if (d < 0)
        return (-1);
    if (d > 0)
        return (1);
    return (0);
}

/* sort the tips array by widget id, then mark it sorted */
static void tip_sort()
{
    if (ntips > 0)
        qsort((void *)tips, ntips, sizeof(Tip), tip_sf);
    tips_sorted = 1;
}
