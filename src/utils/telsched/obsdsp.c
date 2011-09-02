/* code to manipulate ObsDsp's. */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/TextF.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "xtools.h"
#include "configfile.h"
#include "misc.h"
#include "scan.h"

#include "telsched.h"

static void init_colors(void);
static void display_riset (Widget w, int flags, double tm);
static void display_trans (Widget w, int flags, double tm);

/* width of each column.
 * N.B. these must be in same order as the OD_ enums in telsched.h.
 */
static int dspCW[OD_N] = {
    17, 7, 6, 6, 8, 6, 6, 4, 1, 6, 6, 6,
/* size of UTCSTART column shows seconds if sorting is that fine */
#if SECSPERSLOT < 60
    8
#else
    5
#endif
};

static int got_colors;	/* set once we have read the color resources */
static Pixel elig_p;	/* list foreground color when it's ok to run this one */
static Pixel notelig_p;	/* list foreground color when it's not ok to run this */
static Pixel done_p;	/* list foreground color when item has been performed */

/* create the widgets for a ObsDsp, all managed.
 * name is the name of the container.
 */
void
initObsDsp (odp, parent, name)
ObsDsp *odp;
Widget parent;
char *name;
{
	Arg args[20];
	Pixel fg, bg;
	int i;
	int n;

	memset ((char *)odp, 0, sizeof(ObsDsp));

	n = 0;
	XtSetArg (args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg (args[n], XmNisAligned, False); n++;
	XtSetArg (args[n], XmNmarginHeight, 0); n++;
	XtSetArg (args[n], XmNmarginWidth, 0); n++;
	XtSetArg (args[n], XmNspacing, 0); n++;
	odp->container = XmCreateRowColumn (parent, name, args, n);
	XtManageChild (odp->container);

	/* used for the Off tb */
	n = 0;
	XtSetArg (args[n], XmNindicatorOn, False); n++;
	XtSetArg (args[n], XmNshadowThickness, 2); n++;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	odp->off = XmCreateToggleButton (odp->container, "ObsDspOff", args, n);
	XtManageChild (odp->off);

	/* used for the Edit pb */
	n = 0;
	odp->edit = XmCreatePushButton (odp->container, "ObsDspEdit", args, n);
	XtManageChild (odp->edit);

	XtVaGetValues (odp->off, XmNbackground, &bg, NULL);
	XtVaGetValues (odp->off, XmNforeground, &fg, NULL);

	for (i = 0; i < OD_N; i++)
	    odp->f[i] = XtVaCreateManagedWidget ("DspTF",
					xmTextFieldWidgetClass, odp->container,
		XmNcolumns, dspCW[i],
		XmNshadowThickness, 1,
		XmNtopShadowColor, bg,
		XmNtopShadowPixmap, XmUNSPECIFIED_PIXMAP,
		XmNbottomShadowColor, fg,
		XmNbottomShadowPixmap, XmUNSPECIFIED_PIXMAP,
		XmNeditable, False,
		XmNcursorPositionVisible, False,
		XmNmarginHeight, 0,
		XmNmarginWidth, 1,
		XmNblinkRate, 0,
		NULL);
}
	
/* fill in the generally non-changing stuff of op into odp.
 */
void
fillFixedObsDsp (odp, op)
ObsDsp *odp;
Obs *op;
{
	char buf[128];

	wtprintf (odp->f[OD_NAME], "%.*s", dspCW[OD_NAME], op->scan.obj.o_name);
	wtprintf (odp->f[OD_DUR], "%*g", dspCW[OD_DUR], op->scan.dur);
	wtprintf (odp->f[OD_FILT], "%c", op->scan.filter);

#if SECSPERSLOT < 60
	if (op->utcstart == NOTIME)
	    strcpy (buf, "  Any   ");
	else
	    fs_sexa (buf, op->utcstart, 2, 3600);
#else
	if (op->utcstart == NOTIME)
	    strcpy (buf, "  Any");
	else
	    fs_sexa (buf, op->utcstart, 2, 60);
#endif
	wtprintf (odp->f[OD_UTCSTART], buf);
}

/* fill in the potentially time-dependent stuff of op into odp.
 * N.B. we assume op is already computed for the current moment.
 * N.B. blank all fields for camera calibration scans.
 */
void
fillTimeDObsDsp (odp, op, np)
ObsDsp *odp;
Obs *op;
Now *np;
{
	static char blank[] = " ";
	Now startnow;
	Obj startobj;
	char buf[128];
	double tmp;
	Obj *objp = &op->scan.obj;

	if (op->scan.ccdcalib.data == CD_NONE) {
	    wtprintf (odp->f[OD_RA], blank);
	    wtprintf (odp->f[OD_DEC], blank);
	    wtprintf (odp->f[OD_EPOCH], blank);
	    wtprintf (odp->f[OD_HA], blank);
	    wtprintf (odp->f[OD_ALT], blank);
	    wtprintf (odp->f[OD_AZ], blank);
	    wtprintf (odp->f[OD_RISETM], blank);
	    wtprintf (odp->f[OD_TRANSTM], blank);
	    wtprintf (odp->f[OD_SETTM], blank);
	    return;
	}

	/* if start time has been established, build a Now for then and use it
	 * to get the local circumstances at that moment.
	 * otherwise, don't display ha/alt/az.
	 */
	if (op->utcstart != NOTIME) {
	    startnow = *np;
	    startobj = *objp;

	    startnow.n_mjd = mjd_day(startnow.n_mjd) + op->utcstart/24.0;
	    obj_cir (&startnow, &startobj);

	    np = &startnow;
	    objp = &startobj;
	}

	/* show fixed obs using EOD they came with, else use EOD */
	if (objp->o_type == FIXED) {
	    fs_sexa (buf, radhr(objp->f_RA), 2, 600);
	    wtprintf (odp->f[OD_RA], buf);
	    fs_sexa (buf, raddeg(objp->f_dec), 3, 60);
	    wtprintf (odp->f[OD_DEC], buf);
	    mjd_year (objp->f_epoch, &tmp);
	    wtprintf (odp->f[OD_EPOCH], "%6.1f", tmp);
	} else {
	    fs_sexa (buf, radhr(objp->s_ra), 2, 600);
	    wtprintf (odp->f[OD_RA], buf);
	    fs_sexa (buf, raddeg(objp->s_dec), 3, 60);
	    wtprintf (odp->f[OD_DEC], buf);
	    /* s_* fields are always done to EOD */
	    wtprintf (odp->f[OD_EPOCH], "EOD");
	}

	if (op->utcstart != NOTIME) {
	    double lst;

	    now_lst (np, &lst);
	    tmp = hrrad(lst) - objp->s_ra;
	    haRange (&tmp);
	    fs_sexa (buf, radhr(tmp), 3, 600);
	    wtprintf (odp->f[OD_HA], buf);

	    fs_sexa (buf, raddeg(objp->s_alt), 3, 60);
	    wtprintf (odp->f[OD_ALT], buf);

	    fs_sexa (buf, raddeg(objp->s_az), 3, 60);
	    wtprintf (odp->f[OD_AZ], buf);
	} else {
	    wtprintf (odp->f[OD_HA], "");
	    wtprintf (odp->f[OD_ALT], "");
	    wtprintf (odp->f[OD_AZ], "");
	}

	display_riset (odp->f[OD_RISETM], op->rs.rs_flags & ~RS_NOSET,
							    op->rs.rs_risetm);
	display_trans (odp->f[OD_TRANSTM], op->rs.rs_flags, op->rs.rs_trantm);
	display_riset (odp->f[OD_SETTM], op->rs.rs_flags & ~RS_NORISE,
							    op->rs.rs_settm);
}

/* set odp's foreground color according to the state of op.
 */
void
setObsDspColor (odp, op)
ObsDsp *odp;
Obs *op;
{
	Pixel p;
	int i;

	if (!got_colors) {
	    init_colors();
	    got_colors = 1;
	}

	if (op->done)
	    p = done_p;
	else if (!op->elig || op->off)
	    p = notelig_p;
	else
	    p = elig_p;

	for (i = 0; i < OD_N; i++) {
	    Widget w = odp->f[i];
	    Pixel oldp;

	    get_something (w, XmNforeground, (char *)&oldp);
	    if (oldp != p) {
		String text;
		XtVaSetValues (w,
		    XmNforeground, p,
		    NULL);
		/* color wouldn't stick unless we rewrote the text ?! */
		get_something (w, XmNvalue, (char *)&text);
		set_something (w, XmNvalue, text);
		XtFree (text);
	    }
	}
}

/* make some of the widgets in the odp appear empty.
 */
void
setObsDspEmpty (odp)
ObsDsp *odp;
{
	int i;

	for (i = 0; i < OD_N; i++)
	    wtprintf (odp->f[i], "");
}

/* get the color resources we'll need.
 * exit if we can't.
 */
static void
init_colors()
{
	typedef struct {
	    char *resname;
	    Pixel *pixp;
	} ColorRes;
	static ColorRes colorres[] = {
	    {"eligibleColor", &elig_p},
	    {"notEligibleColor", &notelig_p},
	    {"doneColor", &done_p}
	};
	Display *dsp = XtDisplay(toplevel_w);
	Colormap def_cm = DefaultColormap (dsp, 0);
	XColor defxc, dbxc;
	ColorRes *crp;
	char *cval;

	for (crp = colorres; crp < &colorres[XtNumber(colorres)]; crp++) {
	    cval = getXRes (toplevel_w, crp->resname, NULL);
	    if (!cval) {
		printf ("Can not find resource `%.80s'\n", crp->resname);
		exit (1);
	    }
	    if (!XAllocNamedColor (dsp, def_cm, cval, &defxc, &dbxc)) {
		printf ("Can not XAlloc color `%.80s'\n", cval);
		exit (1);
	    }
	    *crp->pixp = defxc.pixel;
	}
}

static void
display_riset (w, flags, tm)
Widget w;
int flags;
double tm;
{
	if (flags & (RS_ERROR))
	    wtprintf (w, "Error ");
	else if (flags & RS_CIRCUMPOLAR)
	    wtprintf (w, "CirPol");
	else if (flags & RS_NEVERUP)
	    wtprintf (w, "NvrUp ");
	else if (flags & RS_NOSET)
	    wtprintf (w, "NoSet ");
	else if (flags & RS_NORISE)
	    wtprintf (w, "NoRise");
	else {
	    char str[32];

	    fs_sexa (str, mjd_hr(tm), 2, 60);	/* 5 chars: "hh:mm" */
	    (void) strcat (str, " ");		/* always add 1 */
	    wtprintf (w, str);
	}
}

static void
display_trans (w, flags, tm)
Widget w;
int flags;
double tm;
{
	if (flags & (RS_ERROR))
	    wtprintf (w, "Error ");
	else if (flags & RS_NEVERUP)
	    wtprintf (w, "NvrUp ");
	else if (flags & RS_NOTRANS)
	    wtprintf (w, "NoTrns");
	else {
	    char str[32];

	    fs_sexa (str, mjd_hr(tm), 2, 60);	/* 5 chars: "hh:mm" */
	    (void) strcat (str, " ");		/* always add 1 */
	    wtprintf (w, str);
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: obsdsp.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
