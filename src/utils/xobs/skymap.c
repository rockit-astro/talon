#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Shell.h>
#include <Xm/BulletinB.h>
#include <Xm/CascadeB.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/MainW.h>
#include <Xm/MessageB.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/ScrollBar.h>
#include <Xm/SelectioB.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/Xm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "misc.h"
#include "strops.h"
/* #include "db.h" */
#include "cliserv.h"
#include "telenv.h"
#include "telstatshm.h"
#include "xtools.h"

#include "xobs.h"

static void skyExpCB(Widget w, XtPointer client, XtPointer call);
static void aa2xy(double alt, double az, int *xp, int *yp);

#define SKYSZ 100    /* size of sky map, pixels */
#define TLSZ 8       /* size of tel symbol */
#define TGSZ 6       /* size of target symbol */
#define SUNSZ 8      /* size of sun */
#define MOONSZ SUNSZ /* size of moon */

static Widget skyda_w; /* sky symbol DA */
static Pixmap sky_pm;  /* pixmap to make it update cleanly */

static GC skyGC;        /* GC for skyda_w */
static Pixel skybg_p;   /* color for overall background */
static Pixel sky_p;     /* color for sky */
static Pixel skytel_p;  /* color for telescope */
static Pixel skytarg_p; /* color for target */
static Pixel skygrid_p; /* color for coord grid */
static Pixel skysun_p;  /* color for sun */
static Pixel skymoon_p; /* color for moon */

Widget mkSky(Widget main_w)
{
    Widget fr_w;

    fr_w = XtVaCreateManagedWidget("LFr", xmFrameWidgetClass, main_w, NULL);

    skyda_w = XtVaCreateManagedWidget("SDA", xmDrawingAreaWidgetClass, fr_w, XmNresizePolicy, XmRESIZE_NONE, XmNwidth,
                                      SKYSZ, XmNheight, SKYSZ, NULL);
    XtAddCallback(skyda_w, XmNexposeCallback, skyExpCB, NULL);

    return (fr_w);
}

/* draw the sky map.
 * N.B. do nothing gracefully if called before window is known
 */
void showSkyMap()
{
    Display *dsp = XtDisplay(skyda_w);
    Window win = XtWindow(skyda_w);
    int x, y;

    if (!win || !sky_pm)
        return;

    if (!skyGC)
    {
        skyGC = XCreateGC(dsp, win, 0L, NULL);
        sky_p = getColor(toplevel_w, "#334");
        skygrid_p = getColor(toplevel_w, "#777");
        skytel_p = getColor(toplevel_w, "#ccf");
        skytarg_p = getColor(toplevel_w, "green");
        skysun_p = getColor(toplevel_w, "yellow");
        skymoon_p = getColor(toplevel_w, "#ccc");
        XtVaGetValues(skyda_w, XmNbackground, &skybg_p, NULL);
    }

    /* background */
    XSetForeground(dsp, skyGC, skybg_p);
    XFillRectangle(dsp, sky_pm, skyGC, 0, 0, SKYSZ, SKYSZ);
    XSetForeground(dsp, skyGC, sky_p);
    XFillArc(dsp, sky_pm, skyGC, 0, 0, SKYSZ, SKYSZ, 0, 360 * 64);
    XDrawString(dsp, sky_pm, skyGC, 0, 10, "NW", 2);

    /* 30-degree grid */
    XSetForeground(dsp, skyGC, skygrid_p);
    XDrawArc(dsp, sky_pm, skyGC, SKYSZ / 6, SKYSZ / 6, 2 * SKYSZ / 3, 2 * SKYSZ / 3, 0, 360 * 64);
    XDrawArc(dsp, sky_pm, skyGC, SKYSZ / 3, SKYSZ / 3, SKYSZ / 3, SKYSZ / 3, 0, 360 * 64);
    XDrawPoint(dsp, sky_pm, skyGC, SKYSZ / 2, SKYSZ / 2);

    /* crescent moon */
    if (moonobj.s_alt >= 0)
    {
        aa2xy(moonobj.s_alt, moonobj.s_az, &x, &y);
        XSetForeground(dsp, skyGC, skymoon_p);
        XFillArc(dsp, sky_pm, skyGC, x - MOONSZ / 2, y - MOONSZ / 2, MOONSZ, MOONSZ, 0, 360 * 64);
        XSetForeground(dsp, skyGC, sky_p);
        XFillArc(dsp, sky_pm, skyGC, x - MOONSZ, y - MOONSZ / 2, MOONSZ, MOONSZ, 0, 360 * 64);
    }

    /* sun */
    if (sunobj.s_alt >= 0)
    {
        aa2xy(sunobj.s_alt, sunobj.s_az, &x, &y);
        XSetForeground(dsp, skyGC, skysun_p);
        XFillArc(dsp, sky_pm, skyGC, x - SUNSZ / 2, y - SUNSZ / 2, SUNSZ, SUNSZ, 0, 360 * 64);
    }

    /* target */
    switch (telstatshmp->telstate)
    {
    case TS_SLEWING: /* FALLTHRU */
    case TS_HUNTING: /* FALLTHRU */
    case TS_TRACKING:
        aa2xy(telstatshmp->Dalt, telstatshmp->Daz, &x, &y);
        XSetForeground(dsp, skyGC, skytarg_p);
        XDrawLine(dsp, sky_pm, skyGC, x - TGSZ / 2, y - TGSZ / 2, x + TGSZ / 2, y + TGSZ / 2);
        XDrawLine(dsp, sky_pm, skyGC, x + TGSZ / 2, y - TGSZ / 2, x - TGSZ / 2, y + TGSZ / 2);
        break;
    default:
        break;
    }

    /* telescope */
    aa2xy(telstatshmp->Calt, telstatshmp->Caz, &x, &y);
    XSetForeground(dsp, skyGC, skytel_p);
    XDrawArc(dsp, sky_pm, skyGC, x - TLSZ / 2, y - TLSZ / 2, TLSZ, TLSZ, 0, 360 * 64);

    /* copy pixmap to screen */
    XCopyArea(dsp, sky_pm, win, skyGC, 0, 0, SKYSZ, SKYSZ, 0, 0);
}

/* callback for the sky drawing area expose */
static void skyExpCB(Widget w, XtPointer client, XtPointer call)
{
    if (!sky_pm)
    {
        Display *dsp = XtDisplay(w);
        Window win = XtWindow(w);
        Window root;
        unsigned int bw, d;
        unsigned int wid, hei;
        int x, y;

        XGetGeometry(dsp, win, &root, &x, &y, &wid, &hei, &bw, &d);
        sky_pm = XCreatePixmap(dsp, win, wid, hei, d);
    }

    showSkyMap();
}

static void aa2xy(double alt, double az, int *xp, int *yp)
{
    double tmp = PI / 2 - alt;
    *xp = SKYSZ / 2.0 * (1 + tmp * sin(az) / (PI / 2));
    *yp = SKYSZ / 2.0 * (1 - tmp * cos(az) / (PI / 2));
}
