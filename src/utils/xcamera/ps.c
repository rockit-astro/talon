/* code to support a set of functions which are called like X functions but
 * which may in fact be creating postscript.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>

#include "P_.h"
#include "patchlevel.h"
#include "xtools.h"
#include "fits.h"
#include "wcs.h"
#include "fieldstar.h"
#include "camera.h"

#include "ps.h"

extern Widget toplevel_w;
extern Colormap camcm;
extern void msg (char *fmt, ...);
extern void query P_((Widget tw, char *msg, char *label1, char *label2,
    char *label3, void (*func1)(), void (*func2)(), void (*func3)()));

typedef enum {
    CLOSED=0,	/* not in use */
    ASKING,	/* in process of asking user where to save */
    OPEN, 	/* FILE * is open and ready for writing */
    XDRAWING	/* xforms set up and ready for X coords */
} XPSState;

typedef struct {
    Font fid;		/* Font ID */
    char *name;		/* name of PS font to use */
    int h;		/* height of font, in points */
} PSFontInfo;

typedef struct {
    XPSState state;	/* what's happ'nin' */
    void (*go)();	/* user's function to call when ok to proceed */
    FILE *fp;		/* file accumulating PS commands */

    char *prfile;	/* malloced name of temp file used for printing */
    int wantpr;		/* set when want to print (as opposed to save) */
    int wantcolor;	/* set when want to print in color */
    int wantinvert;	/* set when want to print inverse video */
    int overimg;	/* 0: use wantcolor. 1: white text 2: black text */

    Window win;		/* window we are capturing */
    double scale;	/* PS's points / X's pixels */

    PSFontInfo *fi;	/* malloced array info about registered fonts */
    int nfi;		/* number of entries malloced in fi */
} XPSContext;

/* here is the implied state behind most of the XPS* functions */
static XPSContext xpsc;

/* handy reference to xpsc.fp */
#define	FP	xpsc.fp 

static Widget print_w;			/* overall print setup dialog */
static Widget color_w;			/* TB set when want to print in color */
static Widget gray_w;			/* TB set when want to print in gray */
static Widget invert_w;			/* TB set when want to print inverse */
static Widget filetb_w;			/* TB set when want to print to file */
static Widget prtb_w;			/* TB set when want to print */
static Widget filename_w;		/* text field holding file name */
static Widget prcmd_w;			/* text field holding print command */
static Widget title_w;			/* text field holding title */

static char deffont[] = "Helvetica";	/* default font */

static int XPSOpen P_((char *fn));
static void setLineStyle P_((Display *dsp, GC gc));
static void setColor P_((Display *dsp, GC gc, int dogray));
static void doArc P_((XArc *xap, int fill));
static void doPoly P_((XPoint xp[], int nxp, int mode, int fill));
static void doRect P_((int x, int y, int w, int h, int fill));
static void doSegment P_((int x1, int y1, int x2, int y2));
static void checkState P_((char *funcname, XPSState s));
static void printTime P_((char tmstr[]));

static void create_print_w P_((void));
static void ok_cb P_((Widget w, XtPointer client, XtPointer call));
static void cancel_cb P_((Widget w, XtPointer client, XtPointer call));
static void help_cb P_((Widget w, XtPointer client, XtPointer call));
static void toggle_cb P_((Widget w, XtPointer client, XtPointer call));
static void saveas_confirm P_((void));
static void xpsc_close P_((void));
static void no_go P_((void));
static void call_go P_((void));
static void x_fill_circle P_((Display *dsp, Drawable win, GC gc, int x, int y,
    int diam));

/* ask user to save or print. if ok, call his go() after we've called XPSOpen()
 */
void
XPSAsk (apname, go)
char *apname;
void (*go)();
{
	char buf[1024];
	Arg args[10];
	int n;

	if (print_w && XtIsManaged(print_w)) {
	    raiseShell (print_w);
	    return;
	}

	if (xpsc.state != CLOSED) {
	    msg ("Only one print/save request may be active at a time.");
	    return;
	}

	/* set up state */
	if (!go) {
	    printf ("XPSAsk: !go\n");
	    exit(1);
	}
	xpsc.go = go;
	xpsc.state = ASKING;

	/* bring up print dialog -- call go() if user says Ok */
	if (!print_w)
	    create_print_w();
	(void) sprintf (buf, "%s Print", apname);
	n = 0;
	XtSetArg (args[n], XmNtitle, buf); n++;
	XtSetValues (XtParent(print_w), args, n);
	XtManageChild (print_w);
}

/* return 1 if currently drawing in color, else 0 */
int
XPSInColor()
{
	return (xpsc.state == XDRAWING && xpsc.wantcolor);
}

/* return 1 if currently drawing, else 0 */
int
XPSDrawing()
{
	return (xpsc.state == XDRAWING);
}

/* given a string to draw in postscript, look it over for characters to be
 * escaped. Return a malloced copy with any and all such characters escaped.
 * N.B. caller should make a copy of the returned sttring before calling again.
 */
char *
XPSCleanStr (s, l)
char *s;
int l;
{
	static char *lasts;
	int i, o;

	lasts = XtRealloc (lasts, l*2 + 1); /* worst case is each char escped */

	for (i = o = 0; i < l; i++) {
	    char c = s[i];
	    if (c == '(' || c == ')' || c == '\\')
		lasts[o++] = '\\';
	    lasts[o++] = c;
	}
	lasts[o] = '\0';

	return (lasts);
}

/* called to put up or remove the watch cursor.  */
void
XPS_cursor (c)
Cursor c;
{
	Window win;

	if (print_w && (win = XtWindow(print_w)) != 0) {
	    Display *dsp = XtDisplay(print_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* remember the fid/fname pair in xpsc */
void
XPSRegisterFont (fid, fname)
Font fid;
char *fname;
{
	Display *dsp = XtDisplay(toplevel_w);
	PSFontInfo *fi;
	int dir, asc, des;
	XCharStruct all;
	int i;

	/* quietly do nothing if not currently building a PS file */
	if (xpsc.state == CLOSED)
	    return;

	/* avoid dups */
	for (i = 0; i < xpsc.nfi; i++)
	    if (fid == xpsc.fi[i].fid)
		return;

	/* grow fi[] and save the new info */
	xpsc.fi = (PSFontInfo *)
		XtRealloc ((void *)xpsc.fi, (xpsc.nfi+1)*sizeof(PSFontInfo));
	fi = &xpsc.fi[xpsc.nfi++];
	fi->fid = fid;
	fi->name = XtNewString (fname);
	XQueryTextExtents (dsp, fid, "?", 1, &dir, &asc, &des, &all);
	fi->h = asc + des;
}

/* set up to convert subsequent X drawing calls to the given X window
 * into Postscript and send to xpsc.fp. [Xx0,Yy0] is the X coordinates of the
 * upper left corner of a window Xw pixels wide to be mapped to a rectangle Pw
 * points wide whose upper left corner is at [Px0,Py0], also specified in
 * points. This allows any X rectangle to me mapped into any Postscript
 * rectangle, scaled but with the same aspect ratio.
 * we also define some handy functions.
 */
void
XPSXBegin (win, Xx0, Xy0, Xw, Xh, Px0, Py0, Pw, Ph)
Window win;	/* what window we are capturing */
int Xx0, Xy0;	/* ul origin of X rectangle */
int Xw;		/* width of X rectangle -- clipped if Xh > 0*/
int Xh;		/* height of X rectangle -- > 0 means to clip */
int Px0, Py0;	/* ul origin of Page rectangle */
int Pw, Ph;	/* size of usable Page rectangle */
{
	double xscale, yscale;
	int px0, py0;

	checkState ("Begin", OPEN);

	xpsc.win = win;

	/* Postscript */
	fprintf (FP, "%%!\n\n");

	/* define function to set solid or nominal dashed line.
	 * By: C. P. Price <price@sedona.uafcns.alaska.edu>
	 */
	fprintf (FP, "%% Define solid and dashed\n");
	fprintf (FP, "/dashed { [4 6] 0 setdash } def\n");
	fprintf (FP, "/solid { [] 0 setdash } def\n");
	fprintf (FP, "\n");

	/* define function Spline to draw an array of points as a nice curve */
	fprintf (FP, "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
	fprintf (FP, "%% Spline to Bezier curve\n");
	fprintf (FP, "%%   [x0 y0 x1 y1 x2 y2 ... ] Spline\n");
	fprintf (FP, "%% Chris Beecroft (chrisb@netcom.com)\n");
	fprintf (FP, "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
	fprintf (FP, "\n");
	fprintf (FP, "/SplineDict 7 dict def\n");
	fprintf (FP, "\n");
	fprintf (FP, "SplineDict begin\n");
	fprintf (FP, "\n");
	fprintf (FP, "/CPointDict 8 dict def\n");
	fprintf (FP, "/ComputeCPDict 21 dict def\n");
	fprintf (FP, "\n");
	fprintf (FP, "%% Some constants\n");
	fprintf (FP, "/Tension 0.65 def  %% 0 (loose) to 1 (tight)\n");
	fprintf (FP, "/SQR2 2 sqrt def\n");
	fprintf (FP, "/recipSQR2 1 SQR2 div def\n");
	fprintf (FP, "\n");
	fprintf (FP, "/CPoint    %% calculate control bezier points\n");
	fprintf (FP, "{          %% stack:  x y len1 len2 angle1 angle2 \n");
	fprintf (FP, "	   %% return: xa ya xb yb\n");
	fprintf (FP, "CPointDict begin\n");
	fprintf (FP, "    /theta2 exch def\n");
	fprintf (FP, "    /theta1 exch def\n");
	fprintf (FP, "\n");
	fprintf (FP, "    /theta theta1 theta2 add 2 div def\n");
	fprintf (FP, "\n");
	fprintf (FP, "    theta1 theta2 gt {\n");
	fprintf (FP, "	/s theta theta2 sub sin def\n");
	fprintf (FP, "	/theta1 theta 90 add def\n");
	fprintf (FP, "	/theta2 theta 90 sub def\n");
	fprintf (FP, "    } {\n");
	fprintf (FP, "	/s theta2 theta sub sin def\n");
	fprintf (FP, "	/theta1 theta 90 sub def\n");
	fprintf (FP, "	/theta2 theta 90 add def\n");
	fprintf (FP, "    } ifelse\n");
	fprintf (FP, "\n");
	fprintf (FP, "    s recipSQR2 gt {\n");
	fprintf (FP, "	/s SQR2 s sub def\n");
	fprintf (FP, "    } if\n");
	fprintf (FP, "\n");
	fprintf (FP, "    /s s 1 Tension sub mul def\n");
	fprintf (FP, "\n");
	fprintf (FP, "    %% stack: x y len1 len2\n");
	fprintf (FP, "\n");
	fprintf (FP, "    /len2 exch s mul def  %% len2 = len2 * s\n");
	fprintf (FP, "    /len1 exch s mul def  %% len1 = len1 * s\n");
	fprintf (FP, "\n");
	fprintf (FP, "    /y exch def\n");
	fprintf (FP, "    /x exch def\n");
	fprintf (FP, "\n");
	fprintf (FP, "  x len1 theta1 cos mul add %% x + len1 * cos(theta1)\n");
	fprintf (FP, "  y len1 theta1 sin mul sub %% y - len1 * sin(theta1)\n");
	fprintf (FP, "  x len2 theta2 cos mul add %% x + len2 * cos(theta2)\n");
	fprintf (FP, "  y len2 theta2 sin mul sub %% y - len2 * sin(theta2)\n");
	fprintf (FP, "end\n");
	fprintf (FP, "} def\n");
	fprintf (FP, "\n");
	fprintf (FP, "\n");
	fprintf (FP, "/ComputeCP %% spline through points\n");
	fprintf (FP, "{          %% stack:  [ x0 y0 x1 y1 x2 y2 .... ]\n");
	fprintf (FP, "	   %% return: nothing  %% spline path added to current path\n");
	fprintf (FP, "ComputeCPDict begin\n");
	fprintf (FP, "    /SplinePoints exch def\n");
	fprintf (FP, "    /elems SplinePoints length def\n");
	fprintf (FP, "\n");
	fprintf (FP, "    /x1 SplinePoints 0 get def\n");
	fprintf (FP, "    /y1 SplinePoints 1 get def\n");
	fprintf (FP, "    /x2 SplinePoints 2 get def\n");
	fprintf (FP, "    /y2 SplinePoints 3 get def\n");
	fprintf (FP, "    /x3 SplinePoints 4 get def\n");
	fprintf (FP, "    /y3 SplinePoints 5 get def\n");
	fprintf (FP, "\n");
	fprintf (FP, "    /dx x1 x2 sub def\n");
	fprintf (FP, "    /dy y2 y1 sub def\n");
	fprintf (FP, "    /len1 dx dx mul dy dy mul add sqrt def\n");
	fprintf (FP, "    len1 0.0 eq {\n");
	fprintf (FP, "	/theta1 0.0 def\n");
	fprintf (FP, "    } {\n");
	fprintf (FP, "	/theta1 dy dx atan def\n");
	fprintf (FP, "	theta1 0 lt {\n");
	fprintf (FP, "	    /theta1 theta1 360 add def\n");
	fprintf (FP, "	} if\n");
	fprintf (FP, "    } ifelse\n");
	fprintf (FP, "\n");
	fprintf (FP, "    /dx x3 x2 sub def\n");
	fprintf (FP, "    /dy y2 y3 sub def\n");
	fprintf (FP, "    /len2 dx dx mul dy dy mul add sqrt def\n");
	fprintf (FP, "    len2 0.0 eq {\n");
	fprintf (FP, "	/theta2 0.0 def\n");
	fprintf (FP, "    } {\n");
	fprintf (FP, "	/theta2 dy dx atan def\n");
	fprintf (FP, "	theta2 0 lt {\n");
	fprintf (FP, "	    /theta2 theta2 360 add def\n");
	fprintf (FP, "	} if\n");
	fprintf (FP, "    } ifelse\n");
	fprintf (FP, "\n");
	fprintf (FP, "    %% compute and store the left and right control points\n");
	fprintf (FP, "    x2 y2 len1 len2 theta1 theta2 CPoint\n");
	fprintf (FP, "    /yright exch def\n");
	fprintf (FP, "    /xright exch def\n");
	fprintf (FP, "    /yleft  exch def\n");
	fprintf (FP, "    /xleft  exch def\n");
	fprintf (FP, "\n");
	fprintf (FP, "    x1 y1 moveto\n");
	fprintf (FP, "\n");
	fprintf (FP, "    x1 3 xleft mul add 4 div\n");
	fprintf (FP, "    y1 3 yleft mul add 4 div\n");
	fprintf (FP, "    xleft 3 mul x2 add 4 div\n");
	fprintf (FP, "    yleft 3 mul y2 add 4 div\n");
	fprintf (FP, "    x2 y2 curveto\n");
	fprintf (FP, "\n");
	fprintf (FP, "    /xsave xright def\n");
	fprintf (FP, "    /ysave yright def\n");
	fprintf (FP, "    \n");
	fprintf (FP, "    6 2 elems 1 sub {\n");
	fprintf (FP, "	/index exch def\n");
	fprintf (FP, "\n");
	fprintf (FP, "	/x2 x3 def\n");
	fprintf (FP, "	/y2 y3 def\n");
	fprintf (FP, "	/len1 len2 def\n");
	fprintf (FP, "\n");
	fprintf (FP, "	theta2 180 ge {\n");
	fprintf (FP, "	    /theta1 theta2 180 sub def\n");
	fprintf (FP, "	} {\n");
	fprintf (FP, "	    /theta1 theta2 180 add def\n");
	fprintf (FP, "	} ifelse\n");
	fprintf (FP, "\n");
	fprintf (FP, "	/x3 SplinePoints index get def\n");
	fprintf (FP, "	/y3 SplinePoints index 1 add get def\n");
	fprintf (FP, "\n");
	fprintf (FP, "	/dx x3 x2 sub def\n");
	fprintf (FP, "	/dy y2 y3 sub def\n");
	fprintf (FP, "	/len2 dx dx mul dy dy mul add sqrt def\n");
	fprintf (FP, "	len2 0.0 eq {\n");
	fprintf (FP, "	    /theta2 0.0 def\n");
	fprintf (FP, "	} {\n");
	fprintf (FP, "	    /theta2 dy dx atan def\n");
	fprintf (FP, "	    theta2 0 lt {\n");
	fprintf (FP, "		/theta2 theta2 360 add def\n");
	fprintf (FP, "	    } if\n");
	fprintf (FP, "	} ifelse\n");
	fprintf (FP, "\n");
	fprintf (FP, "	x2 y2 len1 len2 theta1 theta2 CPoint\n");
	fprintf (FP, "	/yright exch def\n");
	fprintf (FP, "	/xright exch def\n");
	fprintf (FP, "	/yleft  exch def\n");
	fprintf (FP, "	/xleft  exch def\n");
	fprintf (FP, "\n");
	fprintf (FP, "	xsave ysave xleft yleft x2 y2 curveto\n");
	fprintf (FP, "	/xsave xright def\n");
	fprintf (FP, "	/ysave yright def\n");
	fprintf (FP, "    } for\n");
	fprintf (FP, "\n");
	fprintf (FP, "    xright 3 mul x2 add 4 div\n");
	fprintf (FP, "    yright 3 mul y2 add 4 div\n");
	fprintf (FP, "    xright 3 mul x3 add 4 div\n");
	fprintf (FP, "    yright 3 mul y3 add 4 div\n");
	fprintf (FP, "    x3 y3 curveto\n");
	fprintf (FP, "end\n");
	fprintf (FP, "} def\n");
	fprintf (FP, "\n");
	fprintf (FP, "end\n");
	fprintf (FP, "\n");
	fprintf (FP, "\n");
	fprintf (FP, "/Spline\n");
	fprintf (FP, "{\n");
	fprintf (FP, "    SplineDict begin ComputeCP end\n");
	fprintf (FP, "} def\n");

	/* function to draw rotated ellipes -- thanks Chris! */
	fprintf (FP, "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
	fprintf (FP, "%% General Rotated ellipses.\n");
	fprintf (FP, "%%   x y a0 xrad yrad a1 a2 ellipse\n");
	fprintf (FP, "%%   N.B. xrad and yrad must be != 0 \n");
	fprintf (FP, "%% Chris Beecroft (chrisb@netcom.com)\n");
	fprintf (FP, "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%\n");
	fprintf (FP, "/ellipsedict 9 dict def\n");
	fprintf (FP, "ellipsedict /mtrx matrix put\n");
	fprintf (FP, "/ellipse\n");
	fprintf (FP, "    { ellipsedict begin\n");
	fprintf (FP, "      /endangle exch def\n");
	fprintf (FP, "      /startangle exch def\n");
	fprintf (FP, "      /yrad exch def\n");
	fprintf (FP, "      /xrad exch def\n");
	fprintf (FP, "      /rotation exch def\n");
	fprintf (FP, "      /y exch def\n");
	fprintf (FP, "      /x exch def\n");
	fprintf (FP, "\n");
	fprintf (FP, "      /savematrix mtrx currentmatrix def\n");
	fprintf (FP, "      x y translate\n");
	fprintf (FP, "      rotation rotate\n");
	fprintf (FP, "      xrad yrad scale\n");
	fprintf (FP, "\n");
	fprintf (FP, "      %% convert angle/radius (radius=1) to x,y\n");
	fprintf (FP, "      %% divide in x,y scaling and convert back to polar.\n");
	fprintf (FP, "      %% subtract a bit off endangle to force 360 sin\n");
	fprintf (FP, "      %% to be a small negative number to atan rseults in\n");
	fprintf (FP, "      %% a degree of 360 rather than 0. If startangle is 0\n");
	fprintf (FP, "      %% then a surprise appears and no ellipse is drawn.\n");
	fprintf (FP, "\n");
	fprintf (FP, "      /endangle endangle 0.001 sub sin yrad div endangle cos xrad div atan def\n");
	fprintf (FP, "      /startangle startangle sin yrad div  startangle cos xrad div  atan def\n");
	fprintf (FP, "\n");
	fprintf (FP, "      0 0 1 startangle endangle arc\n");
	fprintf (FP, "      savematrix setmatrix\n");
	fprintf (FP, "      end\n");
	fprintf (FP, "    } def\n");

	/* define function lstr to place and show a l-justifed string */
	fprintf (FP, "%% Define l-justified string: (..) x y lstr\n");
	fprintf (FP, "/lstr { newpath moveto show } def\n");
	fprintf (FP, "\n");

	/* define function rstr to place and show a r-justifed string */
	fprintf (FP, "%% Define r-justified string: (..) x y rstr\n");
	fprintf (FP, "/rstr { newpath moveto\n");
	fprintf (FP, "  dup         %% make copies for stringwidth and show\n");
	fprintf (FP, "  stringwidth %% yields X and Y size\n");
	fprintf (FP, "  pop         %% don't need Y\n");
	fprintf (FP, "  neg 0 rmoveto %% move back\n");
	fprintf (FP, "  show        %% show the string\n");
	fprintf (FP, "} def\n");
	fprintf (FP, "\n");

	/* define function cstr to place and show a centered string */
	fprintf (FP, "%% Define centered string: (..) x y cstr\n");
	fprintf (FP, "/cstr { newpath moveto\n");
	fprintf (FP, "  dup         %% make copies for stringwidth and show\n");
	fprintf (FP, "  stringwidth %% yields X and Y size\n");
	fprintf (FP, "  pop         %% don't need Y\n");
	fprintf (FP, "  2 div neg 0 rmoveto %% move back halfway\n");
	fprintf (FP, "  show        %% show the string\n");
	fprintf (FP, "} def\n");
	fprintf (FP, "\n");

	/* define function strsz to return width and height of a string */
	fprintf (FP,"%% Define string size: (..) strsz w h\n");
	fprintf (FP, "/strsz {\n");
	fprintf (FP, "  gsave\n");
	fprintf (FP, "    newpath 0 0 moveto true charpath pathbbox\n");
	fprintf (FP, "    4 2 roll pop pop\n");
	fprintf (FP, "  grestore\n");
	fprintf (FP, "} def\n");
	fprintf (FP, "\n");

	/* X->PS coord xform */
	xscale = (double)Pw/(double)Xw;
	yscale = (double)Ph/(double)Xh;
	if (yscale < xscale) {
	    /* taller than page .. center horizontally */
	    xpsc.scale = yscale;
	    px0 = Px0 + (Pw - Xw*yscale)/2;
	    py0 = Py0;
	} else {
	    /* wider than page .. position across top */
	    xpsc.scale = xscale;
	    px0 = Px0;
	    py0 = Py0;
	}

	/* set up for possibly-clipped X Windows coord system */
	fprintf (FP, "%% Set up to use %sX Windows coord system.\n",
						    Xh > 0 ? "clipped " : "");
	fprintf (FP,"%% [%d,%d] maps to [%d,%d], scaled by %g and Y-flipped.\n",
					    Xx0, Xy0, px0, py0, xpsc.scale);
	fprintf (FP, "gsave\n");
	fprintf (FP, "0 setlinewidth\n");
	fprintf (FP, "%g %g translate\n", px0 - xpsc.scale*Xx0,
							py0 + xpsc.scale*Xy0);
	fprintf (FP, "%g %g scale\n", xpsc.scale, -xpsc.scale);
	if (Xh > 0) {
	    fprintf (FP, "newpath\n");
	    fprintf (FP,
	    "  %d %d moveto %d 0 rlineto 0 %d rlineto %d 0 rlineto closepath\n",
							Xx0, Xy0, Xw, Xh, -Xw);
	    fprintf (FP, "clip\n");
	}
	fprintf (FP, "\n");

	xpsc.state = XDRAWING;
}

/* return the postscript coord system back to the original.
 * this prepares for additional direct entries or closing down.
 */
void
XPSXEnd()
{
	checkState ("End", XDRAWING);

	/* back to native PS coord system */
	fprintf (FP, "\n");
	fprintf (FP, "%% Restore native PS coord system.\n");
	fprintf (FP, "grestore\n");

	/* set a font to use */
	fprintf (FP, "\n");
	fprintf (FP, "%% Set up a font to use for remaining text.\n");
	fprintf (FP, "/%s findfont %d scalefont setfont\n",deffont,ANNOT_PTSZ);

	xpsc.state = OPEN;
}

/* add a raw PS string to the file.
 */
void
XPSDirect (s)
char *s;
{
	checkState ("Direct", OPEN);

	fprintf (FP, "%s\n", s);
}

/* finish up with any boiler plate, close the file and reset xpsc */
void
XPSClose()
{
	char *title;
	char buf[128];

	checkState ("Close", OPEN);

	/* write the title at the top, if any */
	title = XmTextFieldGetString (title_w);
	if ((int)strlen (title) > 0) {
	    fprintf (FP, "\n");
	    fprintf (FP, "%% Title\n");
	    fprintf (FP,"(%s) 306 %d cstr\n", title, 720 + ANNOT_PTSZ + 10);
	}
	XtFree (title);

	/* add some closing boiler plate */
	fprintf (FP, "\n");
	fprintf (FP, "%% Boiler plate:\n");
	printTime (buf);
	fprintf (FP,"(Created by Camera Version %s on %s) 306 %d cstr\n",
						PATCHLEVEL, buf, AROWY(1));
	fprintf (FP, "(%s) 306 %d cstr\n",
			    "(C) 1997 Elwood Charles Downey", AROWY(0));

	/* file is complete */
	fprintf (FP, "\n%% All finished\nshowpage\n");
	fclose (FP);
	FP = NULL;

	/* if we are really trying to print, start the print command and
	 * remove the temp file.
	 */
	if (xpsc.wantpr) {
	    char buf[1024];
	    char *cmd;

	    cmd = XmTextFieldGetString (prcmd_w);
	    (void) sprintf (buf, "%s %s", cmd, xpsc.prfile);
	    XtFree (cmd);

	    msg ("Print command: \"%s\"", buf);

	    if (system (buf))
		msg ("Print error: %s", buf);
	    else
		msg ("Queued: %s",buf);

#if defined(__STDC__) || defined(VMS)
	    (void) remove (xpsc.prfile);
#else
	    (void) unlink (xpsc.prfile);
#endif
	}

	xpsc_close();
}

/* draw a rotated ellipse.
 * easy for postscript, not so for X.
 * all angles are 64ths degree, 0 is 3oclock, positive ccw (all like in X).
 */
void
XPSDrawEllipse (dsp, win, gc, x, y, a0, w, h, a1, a2)
Display *dsp;
Drawable win;
GC gc;
int x, y;	/* bounding box upper left corner */
int a0;		/* axis rotation */
unsigned w, h;	/* bounding box width, height */
int a1, a2;	/* initial angle, additional extent */
{
#define	MAXEPTS	50	/* n points to draw a full ellipse */
#ifndef PI
#define	PI	3.14159
#endif
	double ca0 = cos(a0/64.0*PI/180.0);
	double sa0 = sin(a0/64.0*PI/180.0);
	double a = (double)w/2.0;	/* semi-major axis */
	double b = (double)h/2.0;	/* semi-minor axis */
	XPoint xpt[MAXEPTS];
	int nxpt, maxpt;
	int i;

	/* find number of points to draw an extent of a2 */
	if (a2 == 0)
	    return;
	if (a2 > 360*64)
	    a2 = 360*64;
	else if (a2 < -360*64)
	    a2 = -360*64;
	maxpt = a2*MAXEPTS/(360*64);
	if (maxpt < 0)
	    maxpt = -maxpt;

	/* compute each point, from a1 through a1+a2 */
	for (nxpt = i = 0; i < maxpt; i++) {
	    XPoint *xptp = &xpt[nxpt];
	    double theta = PI/180.0*(a1 + i*a2/(maxpt-1))/64.0;
	    double ex = a*cos(theta);
	    double ey = -b*sin(theta);
	    double xp =  ex*ca0 + ey*sa0;
	    double yp = -ex*sa0 + ey*ca0;
	    xptp->x = (short)floor(xp + x + a + 0.5);
	    xptp->y = (short)floor(yp + y + b + 0.5);
	    if (nxpt == 0 || (xptp[-1].x != xptp->x || xptp[-1].y != xptp->y))
		nxpt++;
	}
	XDrawLines (dsp, win, gc, xpt, nxpt, CoordModeOrigin);
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	/* ellipse can't handle the thin cases */
	setColor (dsp, gc, 0);
	if ((int)w == 0 || (int)h == 0)
	    doSegment (x, y, x+(int)w, y+(int)h);
	else {
	    fprintf (FP, "newpath %g %g %g %g %g %g %g ellipse stroke\n", x + a,
			y + b, -a0/64.0, a, b, 360-(a1 + a2)/64.0, 360-a1/64.0);
	}
}

/* the following functions always perform their associated X Windows
 * function, then might also draw to the xpsc state if it matches the win.
 */
void
XPSDrawArc (dsp, win, gc, x, y, w, h, a1, a2)
Display *dsp;
Drawable win;
GC gc;
int x, y;
unsigned w, h;
int a1, a2;
{
	XArc xa;

	XDrawArc (dsp, win, gc, x, y, w, h, a1, a2);
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	xa.x = x;
	xa.y = y;
	xa.width = w;
	xa.height = h;
	xa.angle1 = a1;
	xa.angle2 = a2;
	setColor (dsp, gc, 0);
	doArc (&xa, 0);
}

void
XPSDrawArcs (dsp, win, gc, xa, nxa)
Display *dsp;
Drawable win;
GC gc;
XArc xa[];
int nxa;
{
	int i;

	XDrawArcs (dsp, win, gc, xa, nxa);
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	setColor (dsp, gc, 0);
	for (i = 0; i < nxa; i++)
	    doArc (&xa[i], 0);
}

void
XPSDrawLine (dsp, win, gc, x1, y1, x2, y2)
Display *dsp;
Drawable win;
GC gc;
int x1, y1;
int x2, y2;
{
	XDrawLine (dsp, win, gc, x1, y1, x2, y2);
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	setColor (dsp, gc, 0);
	setLineStyle (dsp, gc);
	fprintf (FP, "newpath");
	doSegment (x1, y1, x2, y2);
	fprintf (FP, "stroke\n");
}

void
XPSDrawLines (dsp, win, gc, xp, nxp, mode)
Display *dsp;
Drawable win;
GC gc;
XPoint xp[];
int nxp;
int mode;
{
	XDrawLines (dsp, win, gc, xp, nxp, mode);
	if (xpsc.state != XDRAWING || xpsc.win != win || nxp < 2)
	    return;

	setColor (dsp, gc, 0);
	setLineStyle (dsp, gc);
	doPoly (xp, nxp, mode, 0);
}

void
XPSDrawPoint (dsp, win, gc, x, y)
Display *dsp;
Drawable win;
GC gc;
int x, y;
{
	XArc xa;
	XDrawPoint (dsp, win, gc, x, y);
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	xa.x = x;
	xa.y = y;
	xa.width = 1;
	xa.height = 1;
	xa.angle1 = 0;
	xa.angle2 = 360*64;

	setColor (dsp, gc, 0);
	doArc (&xa, 1);
}

void
XPSDrawPoints (dsp, win, gc, xp, nxp, mode)
Display *dsp;
Drawable win;
GC gc;
XPoint xp[];
int nxp;
int mode;
{
	int i;

	XDrawPoints (dsp, win, gc, xp, nxp, mode);
	if (xpsc.state != XDRAWING || xpsc.win != win || nxp == 0)
	    return;

	setColor (dsp, gc, 0);
	for (i = 0; i < nxp; i++) {
	    XArc xa;

	    xa.x = xp[i].x;
	    xa.y = xp[i].y;
	    xa.width = 1;
	    xa.height = 1;
	    xa.angle1 = 0;
	    xa.angle2 = 360*64;
	    doArc (&xa, 1);
	}
}

void
XPSDrawRectangle (dsp, win, gc, x, y, w, h)
Display *dsp;
Drawable win;
GC gc;
int x, y;
unsigned w, h;
{
	XDrawRectangle (dsp, win, gc, x, y, w, h);
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	setColor (dsp, gc, 0);
	setLineStyle (dsp, gc);
	doRect (x, y, w+1, h+1, 0); /* X'S DrawRect is one bigger than asked */
}

void
XPSDrawRectangles (dsp, win, gc, xra, nxr)
Display *dsp;
Drawable win;
GC gc;
XRectangle xra[];
int nxr;
{
	XRectangle *lastxra;

	XDrawRectangles (dsp, win, gc, xra, nxr);
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	setColor (dsp, gc, 0);
	setLineStyle (dsp, gc);
	for (lastxra = xra+nxr; xra < lastxra; xra++)
	    doRect (xra->x, xra->y, xra->width+1, xra->height+1, 0);
}

void
XPSDrawSegments (dsp, win, gc, xs, nxs)
Display *dsp;
Drawable win;
GC gc;
XSegment xs[];
int nxs;
{
	int i;

	XDrawSegments (dsp, win, gc, xs, nxs);
	if (xpsc.state != XDRAWING || xpsc.win != win || nxs == 0)
	    return;

	setColor (dsp, gc, 0);
	setLineStyle (dsp, gc);
	fprintf (FP, "newpath");
	for (i = 0; i < nxs; i++) {
	    if (!(i & 1))
		fprintf (FP, "\n  ");
	    doSegment (xs[i].x1, xs[i].y1, xs[i].x2, xs[i].y2);
	}
	fprintf (FP, "\nstroke\n");
			
}

/* draw using the font from gc which matches a previously Registered PS font.
 */
void
XPSDrawString (dsp, win, gc, x, y, s, l)
Display *dsp;
Drawable win;
GC gc;
int x, y;
char *s;
int l;
{
	unsigned long gcm;
	XGCValues gcv;
	PSFontInfo *fi;
	Font fid;
	int i;

	XDrawString (dsp, win, gc, x, y, s, l);
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	gcm = GCFont;
	XGetGCValues (dsp, gc, gcm, &gcv);
	fid = gcv.font;

	/* find gc's font in preregistered list */
	fi = NULL;
	for (i = 0; i < xpsc.nfi; i++) {
	    fi = &xpsc.fi[i];
	    if (fi->fid == fid)
		break;
	}
	if (i == xpsc.nfi) {
	    printf ("Unregistered fid. String was '%.*s'\n", l, s);
	    exit(1);
	}

	/* load desired font and take care not to scale the text */
	fprintf (FP, "newpath %d %d moveto gsave\n", x, y);
	setColor (dsp, gc, 0);
	fprintf (FP, "  %g %g scale /%s findfont %g scalefont setfont\n",
		1.0/xpsc.scale, -1.0/xpsc.scale, fi->name, fi->h*xpsc.scale);

	/* show the string -- beware a few special characters */
	fprintf (FP, "  (%s)\n", XPSCleanStr(s,l));
	fprintf (FP, "show grestore\n");
}

void
XPSFillArc (dsp, win, gc, x, y, w, h, a1, a2)
Display *dsp;
Drawable win;
GC gc;
int x, y;
unsigned w, h;
int a1, a2;
{
	XArc xa;

	if (w == h && a2 == 360*64)
	    x_fill_circle (dsp, win, gc, x, y, w);
	else
	    XFillArc (dsp, win, gc, x, y, w, h, a1, a2);
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	xa.x = x;
	xa.y = y;
	xa.width = w;
	xa.height = h;
	xa.angle1 = a1;
	xa.angle2 = a2;
	setColor (dsp, gc, 0);
	doArc (&xa, 1);
}

void
XPSFillArcs (dsp, win, gc, xap, na)
Display *dsp;
Drawable win;
GC gc;
XArc *xap;
int na;
{
	XArc *ap, *lastap;

	/* sorry to break this up.. */
	for (ap = xap, lastap = ap+na; ap < lastap; ap++) {
	    if (ap->width == ap->height && ap->angle2 == 360*64)
		x_fill_circle (dsp, win, gc, ap->x, ap->y, ap->width);
	    else
		XFillArc (dsp, win, gc, ap->x, ap->y, ap->width, ap->height,
							ap->angle1, ap->angle2);
	}

	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	setColor (dsp, gc, 0);
	for (ap = xap, lastap = ap+na; ap < lastap; ap++)
	    doArc (ap, 1);
}

void
XPSFillPolygon (dsp, win, gc, xp, nxp, shape, mode)
Display *dsp;
Drawable win;
GC gc;
XPoint xp[];
int nxp;
int shape;
int mode;
{
	XFillPolygon (dsp, win, gc, xp, nxp, shape, mode);
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	fprintf (FP, "gsave ");
	setColor (dsp, gc, 1);
	doPoly (xp, nxp, mode, 1);
	fprintf (FP, "grestore\n");
}


void
XPSFillRectangle (dsp, win, gc, x, y, w, h)
Display *dsp;
Drawable win;
GC gc;
int x, y;
unsigned w, h;
{
	XFillRectangle (dsp, win, gc, x, y, w, h);
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	setColor (dsp, gc, 0);
	doRect (x, y, w, h, 1);
}

void
XPSFillRectangles (dsp, win, gc, xra, nxr)
Display *dsp;
Drawable win;
GC gc;
XRectangle xra[];
int nxr;
{
	XRectangle *lastxra;

	XFillRectangles (dsp, win, gc, xra, nxr);
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	setColor (dsp, gc, 0);
	for (lastxra = xra+nxr; xra < lastxra; xra++)
	    doRect (xra->x, xra->y, xra->width, xra->height, 1);
}

/* fill a rectangle but do *just* the postscript */
void
PSFillRectangle (win, gray, x, y, w, h)
Drawable win;
double gray;
int x, y;
unsigned w, h;
{
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	fprintf (FP, "gsave %g setgray ", gray);
	doRect (x, y, w, h, 1);
	fprintf (FP, "grestore\n");
}

/* draw the image of the given size with ul at [x0,y0] wrt to win using the X
 * windows coordinate system. win is just used to confirm the currect context.
 * N.B. this also sets overimg which means all subsequent text will be drawn in
 *   black if inv or white if !inv (unless wantcolor is on).
 */
void
XPSImage (win, pix, cm, maxcm, inv, aoip, scale, pixw, pixh)
Drawable win;		/* reference window */
unsigned short pix[];	/* pixels */
char cm[];		/* colormap: index with pixel, get back 0..maxcm */
int maxcm;		/* max colormap value */
int inv;		/* set if window is in inverse video */
AOI *aoip;		/* subimage, else whole if NULL */
double scale;		/* size multiplier */
int pixw, pixh;		/* dimensions of pix[] */
{
	int wanti;
	int x, y;
	int w, h;
	int wrap;
	int n;

	/* confirm */
	if (xpsc.state != XDRAWING || xpsc.win != win)
	    return;

	/* toggle invertness */
	wanti = inv ^ xpsc.wantinvert;

	/* find effective dimensions wrt to pix[] */
	if (aoip) {
	    x = aoip->x;
	    y = aoip->y;
	    w = aoip->w;
	    h = aoip->h;
	} else {
	    x = 0;
	    y = 0;
	    w = pixw;
	    h = pixh;
	}

	/* tried 4 bits per pixel but it was pretty coarse */
	fprintf (FP, "gsave /imgw %d def /imgh %d def\n", w, h);
	fprintf (FP, "  %g %g scale\n", scale*w, scale*h);
	fprintf (FP, "  imgw imgh 8 [imgw 0 0 imgh neg 0 imgh]\n");
	fprintf (FP, "  { currentfile imgw string readhexstring pop } image\n");

	/* postscript draws bottom-to-top so flip tb */
	pix += (y+h-1)*pixw + x;
	wrap = -pixw - w;

	/* postscript draws bottom-to-top so flip tb */
	n = 0;
	for (y = 0; y < h; y++) {
	    for (x = 0; x < w; x++) {
		int fp = *pix++;
		int gp = cm[fp];
		if (wanti)
		    gp = maxcm - gp;
		gp = gp*255/maxcm;
		fprintf (FP, "%x%x", gp>>4, gp&0xf);
		if (((++n)%36) == 0)
		    fprintf (FP, "\n");
	    }
	    pix += wrap;
	}

	fprintf (FP, "\ngrestore\n");

	/* set overimage so subsequent text is written white */
	xpsc.overimg = wanti ? 2 : 1;
}

/* check gc's LineStyle and emit "solid" or "dashed" macro.
 * TODO: check dash details.
 */
static void
setLineStyle (dsp, gc)
Display *dsp;
GC gc;
{
	XGCValues gcv;
	unsigned long gcm;

	gcm = GCLineStyle;
	XGetGCValues (dsp, gc, gcm, &gcv);
	fprintf (FP, "%s ", gcv.line_style == LineSolid ? "solid" : "dashed");
}

/* if we are currently doing color
 *   dig the rgb out of the gc and put in postscript.
 * else if we are currently writing over an image
 *   set color to white
 * else if dogray is set
 *   dig the rgb out of the gc and put in postscript as a gray level.
 * else
 *   do nothing for color (use default black).
 * N.B. can't allow for colormap since we are often used with a pixmap.
 */
static void
setColor (dsp, gc, dogray)
Display *dsp;
GC gc;
int dogray;
{
	XGCValues gcv;
	XColor xc;

	if (xpsc.wantcolor) {
	    XGetGCValues (dsp, gc, (unsigned long) GCForeground, &gcv);
	    xc.pixel = gcv.foreground;
	    XQueryColor (dsp, camcm, &xc);
	    fprintf (FP, "%g %g %g setrgbcolor\n", (double)xc.red/65535.0,
			    (double)xc.green/65535.0, (double)xc.blue/65535.0);
	} else if (xpsc.overimg == 1) {
	    fprintf (FP, "1 setgray\n");
	} else if (xpsc.overimg == 2) {
	    fprintf (FP, "0 setgray\n");
	} else if (dogray) {
	    double r, g, b, gray;

	    XGetGCValues (dsp, gc, (unsigned long) GCForeground, &gcv);
	    xc.pixel = gcv.foreground;
	    XQueryColor (dsp, camcm, &xc);
	    r = (double)xc.red;
	    g = (double)xc.green;
	    b = (double)xc.blue;
	    gray = sqrt ((r*r + g*g + b*b)/3.0)/65535.0; /* SMPTE? */
	    fprintf (FP, "%g setgray\n", gray);
	}
}

/* X's FillArc and DrawArc both fill to the edges of a box with the specified
 * w and h. They don't act goofy like FillRect does.
 */
static void
doArc (xap, fill)
XArc *xap;
int fill;
{
	int x = xap->x;
	int y = xap->y;
	int w = xap->width;
	int h = xap->height;

	/* seems ellipse, at least, can't handle the thin cases */
	if (w == 0 && h == 0)
	    return;
	if (w == 0 || h == 0)
	    doSegment (x, y, x+w, y+h);
	else if (w == h)
	    fprintf (FP, "newpath %g %g %g %g %g arc %s\n",
			    x + w/2.0, y + h/2.0, w/2.0,
			    (double)xap->angle1/64.0,
			    (double)(xap->angle1 + xap->angle2)/64.0,
			    fill ? "fill" : "stroke");
	else
	    fprintf (FP, "newpath %g %g 0 %g %g %g %g ellipse %s\n",
			    x + w/2.0, y + h/2.0, w/2.0, h/2.0,
			    (double)xap->angle1/64.0,
			    (double)(xap->angle1 + xap->angle2)/64.0,
			    fill ? "fill" : "stroke");
}

static void
doPoly (xp, nxp, mode, fill)
XPoint xp[];
int nxp;
int mode;
int fill;
{
	int i;

	if (nxp < 1)
	    return;

	if (nxp < 3 || mode == CoordModePrevious) {
	    char *lt = mode == CoordModeOrigin ? "lineto" : "rlineto";

	    fprintf (FP, "newpath %d %d moveto\n", xp[0].x, xp[0].y);
	    for (i = 1; i < nxp; i++)
		fprintf (FP, "  %d %d %s\n", xp[i].x, xp[i].y, lt);
	    fprintf (FP, "%s\n", fill ? "fill" : "stroke");
	} else {
	    /* Spline requires at least 3 absolute points */
	    fprintf (FP, "newpath\n  [");
	    for (i = 0; i < nxp; i++)
		fprintf (FP, " %d %d ", xp[i].x, xp[i].y);
	    fprintf (FP, "]\nSpline %s\n", fill ? "fill" : "stroke");
	}
}

static void
doRect (x, y, w, h, fill)
int x, y, w, h;
int fill;
{
	fprintf (FP, "newpath %d %d moveto %d %d lineto %d %d lineto\n",
				    x, y,   x+w, y,   x+w, y+h);
	fprintf (FP, "%d %d lineto %d %d lineto", x, y+h, x, y);
	fprintf (FP, " %s\n", fill ? "fill" : "stroke");
}

/* draw a line from [x1,y1] to [x2,y2] */
static void
doSegment (x1, y1, x2, y2)
int x1, y1, x2, y2;
{
	fprintf (FP, " %4d %4d moveto %4d %4d lineto ", x1, y1, x2, y2);
}

static void
checkState (funcname, s)
char *funcname;
XPSState s;
{
	if (xpsc.state != s) {
	    printf ("XPS%s(): state is %d but expected %d\n",
					funcname, (int)xpsc.state, (int)s);
	    exit (1);
	}
}

static void
printTime (char tmstr[])
{
	char *ct;
	int ctl;

#if defined(__STDC__)
	struct tm *tmp;
	time_t t;

	(void) time (&t);
	tmp = gmtime (&t);
	if (tmp) {
	    static char mons[12][4] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	    };
	    static char days[7][4] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	    };
	    char buf[128];

	    (void) sprintf (buf, "%s %s %2d %02d:%02d:%02d %d",
	    	days[tmp->tm_wday], mons[tmp->tm_mon], tmp->tm_mday,
		tmp->tm_hour, tmp->tm_min, tmp->tm_sec, tmp->tm_year+1900);
	    sprintf (tmstr, "%s UTC", buf);
	    return;
	}
#else
	long t;

	time (&t);
#endif

	/* use local time if not standard C or no gmtime() */
	ct = ctime (&t);
	ctl = strlen (ct)-1;	/* chop trailing '\n' */

	sprintf (tmstr,"%.*s Local Time", ctl, ct);
}

/* prepare the named file for writing postscript into.
 * return 0 if ok, else report why with msg(), reset xpsc and return -1.
 */
static int
XPSOpen (fn)
char *fn;
{
	checkState ("Open", ASKING);

	xpsc.fp = fopen (fn, "w");
	if (!xpsc.fp) {
	    msg ("%s: %s", fn, strerror(errno));
	    xpsc_close();
	    return (-1);
	}

	xpsc.state = OPEN;

	return (0);
}

/* create the print dialog */
static void
create_print_w()
{
	Widget w;
	Widget f_w, rc_w;
	Widget rb_w;
	char fname[1024];
	Arg args[20];
	int n;

	/* create the form dialog -- title gets set later */
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	print_w = XmCreateFormDialog (toplevel_w, "Print", args, n);
	set_something (print_w, XmNcolormap, (char *)camcm);
	XtAddCallback (print_w, XmNhelpCallback, help_cb, 0);

	/* make a rowcolumn for stuff */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNspacing, 5); n++;
	rc_w = XmCreateRowColumn (print_w, "VRC", args, n);
	XtManageChild (rc_w);

	/* make a prompt for the title */

	n = 0;
	w = XmCreateLabel (rc_w, "TitleL", args, n);
	set_xmstring (w, XmNlabelString, "Title:");
	XtManageChild (w);

	n = 0;
	title_w = XmCreateTextField (rc_w, "Title", args, n);
	XtManageChild (title_w);

	/* add a separator */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep0", args, n);
	XtManageChild (w);

	/* make the color/gray radio box */
	n = 0;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	rb_w = XmCreateRadioBox (rc_w, "CGRB", args, n);
	XtManageChild (rb_w);

	    n = 0;
	    color_w = XmCreateToggleButton (rb_w, "Color", args, n);
	    XtManageChild (color_w);

	    n = 0;
	    gray_w = XmCreateToggleButton (rb_w, "Grayscale", args, n);
	    XtManageChild (gray_w);

	/* Make the "invert" choice */

	n = 0;
	XtSetArg (args[n], XmNset, True); n++;
	XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
	XtSetArg (args[n], XmNindicatorOn, True); n++;
	invert_w = XmCreateToggleButton (rc_w, "Inverse", args, n);
	set_xmstring (rc_w, XmNlabelString, "Invert colors");
	XtManageChild (invert_w);

	/* add a separator */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep1", args, n);
	XtManageChild (w);

	/* make the fake "to file" toggle button and filename text field */

	n = 0;
	f_w = XmCreateForm (rc_w, "FileF", args, n);
	XtManageChild (f_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    filetb_w = XmCreateToggleButton (f_w, "Save", args, n);
	    set_xmstring (filetb_w, XmNlabelString, "Save to file:");
	    XtManageChild (filetb_w);

	    (void) strcpy (fname, "camera.ps");
	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 50); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNvalue, fname); n++;
	    filename_w = XmCreateTextField (f_w, "Filename", args, n);
	    XtManageChild (filename_w);

	/* make the fake "print" toggle button and print command text field */

	n = 0;
	f_w = XmCreateForm (rc_w, "PrintF", args, n);
	XtManageChild (f_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	    prtb_w = XmCreateToggleButton (f_w, "Print", args, n);
	    set_xmstring (prtb_w, XmNlabelString, "Print command:");
	    XtManageChild (prtb_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 50); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    prcmd_w = XmCreateTextField (f_w, "PrintCmd", args, n);
	    XtManageChild (prcmd_w);

	/* connect them together so they work like a radio box */

	XtAddCallback (filetb_w, XmNvalueChangedCallback, toggle_cb,
							    (XtPointer)prtb_w);
	XtAddCallback (prtb_w, XmNvalueChangedCallback, toggle_cb,
							(XtPointer)filetb_w);

	/* add another separator */

	n = 0;
	w = XmCreateSeparator (rc_w, "Sep2", args, n);
	XtManageChild (w);

	/* add the command buttons across the bottom */

	n = 0;
	XtSetArg (args[n], XmNfractionBase, 10); n++;
	f_w = XmCreateForm (rc_w, "CmdF", args, n);
	XtManageChild (f_w);

	    n = 0;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 1); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 3); n++;
	    w = XmCreatePushButton (f_w, "Ok", args, n);
	    XtAddCallback (w, XmNactivateCallback, ok_cb, 0);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 4); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 6); n++;
	    w = XmCreatePushButton (f_w, "Cancel", args, n);
	    XtAddCallback (w, XmNactivateCallback, cancel_cb, 0);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 7); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 9); n++;
	    w = XmCreatePushButton (f_w, "Help", args, n);
	    XtAddCallback (w, XmNactivateCallback, help_cb, 0);
	    XtManageChild (w);
}

/* called by either of the Save or Print togle buttons to enforce their
 * radio behavior. client is the opposite Widget.
 */
static void
/* ARGSUSED */
toggle_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (XmToggleButtonGetState(w)) {
	    Widget otherw = (Widget)client;
	    XmToggleButtonSetState (otherw, False, False);
	} else
	    XmToggleButtonSetState (w, True, False); /* just like a RB :-) */
}

/* called to reset the pending print query sequence */
static void
no_go()
{
	checkState ("no_go", ASKING);
	xpsc_close();
}

/* called when the Ok button is hit in the print dialog */
/* ARGSUSED */
static void
ok_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int wantc = XmToggleButtonGetState (color_w);
	int wantg = XmToggleButtonGetState (gray_w);
	int wantf = XmToggleButtonGetState (filetb_w);
	int wantp = XmToggleButtonGetState (prtb_w);
	int wanti = XmToggleButtonGetState (invert_w);

	/* color or gray */
	if (!(wantc ^ wantg)) {
	    msg ("Select one of Color or Grayscale printing.");
	    return;	/* leave main plot dialog up */
	}
	xpsc.wantcolor = wantc;

	/* print or save to file */
	if (!(wantf ^ wantp)) {
	    msg ("Select one of Save or Print.");
	    return;	/* leave main plot dialog up */
	}
	xpsc.wantpr = wantp;

	/* inverse */
	xpsc.wantinvert = wanti;

	if (wantp) {
	    /* print */
	    char name[1024];

	    //(void) tmpnam (name);
	    (void) mkstemp(name);

	    /* VMS tmpnam() doesn't always include the required '.' */
	    if (!strchr (name, '.'))
		(void) strcat (name, ".ps");

	    xpsc.prfile = XtNewString (name);
	    if (XPSOpen (xpsc.prfile) == 0)
		call_go();

	} else {
	    /* save to file -- ask whether to clobber if it already exits */
	    char *name = XmTextFieldGetString (filename_w);

	    if (access (name, W_OK) == 0) {
		char buf[1024];
		(void) sprintf (buf, "%s exists: Ok to Overwrite?", name);
		query (toplevel_w, buf, "Yes -- Overwrite", "No -- Cancel",
					    NULL, saveas_confirm, no_go, NULL);
	    } else
		saveas_confirm();

	    XtFree (name);
	}

	XtUnmanageChild (print_w);
}

/* called by Cancel */
/* ARGSUSED */
static void
cancel_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	no_go();
	XtUnmanageChild (print_w);
}

/* called by Help */
/* ARGSUSED */
static void
help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	msg ("Select color or gray; file name or print command");
}

/* called when we have confirmed it's ok to clobber an existing save file */
static void
saveas_confirm()
{
	char *name = XmTextFieldGetString (filename_w);
	if (XPSOpen (name) == 0)
	    call_go();
	XtFree (name);
}


/* finished with xpsc */
static void
xpsc_close()
{
	xpsc.state = CLOSED;

	if (xpsc.go)
	    xpsc.go = NULL;

	if (xpsc.fp) {
	    fclose (xpsc.fp);
	    xpsc.fp = NULL;
	}

	if (xpsc.prfile) {
	    XtFree (xpsc.prfile);
	    xpsc.prfile = NULL;
	}

	xpsc.wantpr = 0;
	xpsc.wantcolor = 0;
	xpsc.wantinvert = 0;
	xpsc.overimg = 0;

	if (xpsc.nfi > 0) {
	    int i;
	    for (i = 0; i < xpsc.nfi; i++)
		XtFree ((void *)xpsc.fi[i].name);
	    XtFree ((void *)xpsc.fi);
	    xpsc.fi = NULL;
	    xpsc.nfi = 0;
	}
}

/* called when it's confirmed to try and print or save */
/* ARGSUSED */
static void
call_go ()
{
	/* call the user's function */
	if (xpsc.go)
	    (*xpsc.go) ();
	else {
	    printf ("call_go() but no (*go)(). state = %d\n", (int)xpsc.state);
	    exit(1);
	}

	/* clean up if the user forgot */
	switch (xpsc.state) {
	case CLOSED:
	    break;
	case ASKING:
	    printf ("call_go() state = %d\n", (int)xpsc.state);
	    exit(1);
	    break;	/* :-) */
	case OPEN:
	    XPSClose ();
	    break;
	case XDRAWING:
	    XPSXEnd();
	    XPSClose();
	    break;
	}
}

/* fill a circle with upper left bounding box at [x,y] with diameter diam.
 * use custom bitmaps because many X servers get it poorly.
 */
static void
x_fill_circle (dsp, win, gc, x, y, diam)
Display *dsp;
Drawable win;
GC gc;
int x, y;
int diam;
{
	static unsigned char star2_bits[] = {
	   0x03, 0x03};
	static unsigned char star3_bits[] = {
	   0x07, 0x07, 0x07};
	static unsigned char star4_bits[] = {
	   0x06, 0x0f, 0x0f, 0x06};
	static unsigned char star5_bits[] = {
	   0x0e, 0x1f, 0x1f, 0x1f, 0x0e};
	static unsigned char star6_bits[] = {
	   0x1e, 0x3f, 0x3f, 0x3f, 0x3f, 0x1e};
	static unsigned char star7_bits[] = {
	   0x1c, 0x3e, 0x7f, 0x7f, 0x7f, 0x3e, 0x1c};
	static unsigned char star8_bits[] = {
	   0x3c, 0x7e, 0xff, 0xff, 0xff, 0xff, 0x7e, 0x3c};
	static unsigned char star9_bits[] = {
	   0x38, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xff, 0x01, 0xff, 0x01, 0xff,
	   0x01, 0xfe, 0x00, 0xfe, 0x00, 0x38, 0x00};
	static unsigned char star10_bits[] = {
	   0x78, 0x00, 0xfe, 0x01, 0xfe, 0x01, 0xff, 0x03, 0xff, 0x03, 0xff,
	   0x03, 0xff, 0x03, 0xfe, 0x01, 0xfe, 0x01, 0x78, 0x00};
	static unsigned char star11_bits[] = {
	   0xf8, 0x00, 0xfe, 0x03, 0xfe, 0x03, 0xff, 0x07, 0xff, 0x07, 0xff,
	   0x07, 0xff, 0x07, 0xff, 0x07, 0xfe, 0x03, 0xfe, 0x03, 0xf8, 0x00};
	static unsigned char star12_bits[] = {
	   0xf0, 0x00, 0xfe, 0x07, 0xfe, 0x07, 0xfe, 0x07, 0xff, 0x0f, 0xff,
	   0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xfe, 0x07, 0xfe, 0x07, 0xfe, 0x07,
	   0xf0, 0x00};
	static unsigned char *star_bits[] = {
	    NULL,
	    NULL,
	    star2_bits,
	    star3_bits,
	    star4_bits,
	    star5_bits,
	    star6_bits,
	    star7_bits,
	    star8_bits,
	    star9_bits,
	    star10_bits,
	    star11_bits,
	    star12_bits,
	};
	static Pixmap pms[XtNumber(star_bits)];

	/* build the pixmaps the first time through */
	if (!pms[2]) {
	    int i;
	    for (i = 0; i < XtNumber(pms); i++)
		if (star_bits[i])
		    pms[i] = XCreateBitmapFromData (dsp, win,
						(char *)(star_bits[i]), i, i);
	}

	/* see whether the special cases apply */
	if (diam < 1)
	    return;
	if (diam < 2) {
	    XDrawPoint (dsp, win, gc, x, y);
	    return;
	}
	if (diam >= XtNumber(pms)) {
	    XFillArc (dsp, win, gc, x, y, diam, diam, 0, 360*64);
	    return;
	}

	/* draw smaller dots with the bitmaps */
	XSetClipMask (dsp, gc, pms[diam]);
	XSetClipOrigin (dsp, gc, x, y);
	XFillRectangle (dsp, win, gc, x, y, diam, diam);
	XSetClipMask (dsp, gc, None); /* TODO: probably should put back old */
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: ps.c,v $ $Date: 2001/05/23 15:34:58 $ $Revision: 1.2 $ $Name:  $"};
