/* build the gui for xobs */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

#include <Xm/Xm.h>
#include <X11/keysym.h>
#include <X11/Shell.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/CascadeB.h>
#include <Xm/Form.h>
#include <Xm/Separator.h>
#include <Xm/MainW.h>
#include <Xm/RowColumn.h>
#include <Xm/ScrollBar.h>
#include <Xm/ToggleB.h>
#include <Xm/BulletinB.h>
#include <Xm/SelectioB.h>
#include <Xm/MessageB.h>
#include <Xm/TextF.h>
#include <Xm/Text.h>
#include <Xm/DrawingA.h>
#include <Xm/Frame.h>

#include <X11/xpm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "strops.h"
#include "misc.h"
/* #include "db.h" */
#include "telenv.h"
#include "telstatshm.h"
#include "cliserv.h"
#include "xtools.h"

#include "xobs.h"
#include "widgets.h"

#define	MAXMSG	50000		/* max message text length */

Pixel ltcolors[LTN];
Pixel editableColor;
Pixel uneditableColor;

String fallbacks[] = {
    "XObs*background: CornSilk3",
    "XObs*foreground: #11f",
    "XObs*Query*background: #d33",
    "XObs*Query*foreground: white",

    "XObs*highlightThickness: 0",
    "XObs*XmFrame.shadowThickness: 4",
    "XObs*XmFrame.marginWidth: 4",
    "XObs*XmFrame.marginHeight: 4",
    "XObs*XmText.shadowThickness: 2",
    "XObs*XmTextField.shadowThickness: 2",
    "XObs*XmText.background: white",

    "XObs.OkColor: green",
    "XObs.IdleColor: gray",
    "XObs.ActiveColor: yellow",
    "XObs.WarnColor: red",
    "XObs.EditableColor: #ddd",
    "XObs.UneditableColor: white",

    NULL
};

/* master widget set */
Widget g_w[N_W];

static char prT[] = "prompt";
static char pbT[] = "button";
static char frT[] = "frame";

#define	LOGODASZ	100	/* size of logo drawing area, pixels */
static GC guigc;
static Pixmap logopm;
static XpmAttributes logoxpma;
static Pixel tshadow_p, bshadow_p;

static Widget msg_w;
static int msg_txtl;

#define	LIGHTW		10	/* width of light indicator */
#define	LIGHTH		10	/* height of light indicator */

static Widget mkLogo (Widget main_w);
static Widget mkCurrent(Widget main_w);
static Widget mkCamera(Widget main_w);
static Widget mkDome(Widget main_w);
static Widget mkControl(Widget main_w);
static Widget mkStatus(Widget main_w);
static Widget mkScope(Widget main_w);
static Widget mkMsg(Widget main_w);
static Widget mkInfo(Widget main_w);
static Widget mkCover(Widget main_w);

static Widget mkPrompt (Widget p_w, int tc, Widget *l_wp, Widget *tf_wp,
    Widget *lb_wp);
static void mkRow (Widget p_w, Widget w_p[], int n, int w);
static void mkFrame (Widget parent_w, char *name, char *title,
    Widget *frame_wp, Widget *form_wp);
static void lightExpCB (Widget wid, XtPointer client, XtPointer call);
static void drawLt (Widget wid);
static void wltprintf (char *tag, Widget w, char *fmt, ...);
static void mkGC(void);

static char logfn[] = "archive/logs/xobsmsgs.log";

/* build the entire GUI off toplevel_w */
void
mkGUI(char *version)
{
	char buf[1024];
	Widget t_w = toplevel_w;
	Widget main_w;
	Widget cur_w;
	Widget skymap_w;
	Widget logo_w;
	Widget cam_w;
	Widget ctrl_w;
	Widget status_w;
	Widget dome_w;
	Widget scope_w;
	Widget msgf_w;
	Widget info_w;
	Widget hf_w;
	Widget cover_w;

	(void) sprintf (buf, "XObservatory -- Version %s", version);
	XtVaSetValues (toplevel_w, XmNtitle, buf, NULL);

	ltcolors[LTOK] = getColor (t_w, getXRes (t_w, "OkColor", "White"));
	ltcolors[LTIDLE] = getColor (t_w, getXRes (t_w, "IdleColor","White"));
	ltcolors[LTACTIVE]= getColor(t_w,getXRes(t_w, "ActiveColor","White"));
	ltcolors[LTWARN] = getColor (t_w, getXRes (t_w, "WarnColor","White"));
	editableColor= getColor (t_w, getXRes (t_w, "EditableColor","White"));
	uneditableColor=getColor(t_w,getXRes(t_w,"UneditableOkColor","White"));

    printf("KMI about to create widget\n");

	main_w = XtVaCreateManagedWidget ("MainF", xmFormWidgetClass,toplevel_w,
	    XmNverticalSpacing, 10,
	    XmNhorizontalSpacing, 10,
	    XmNresizePolicy, XmRESIZE_NONE,
	    NULL);
    printf("KMI widget created\n");

	skymap_w = mkSky (main_w);
	XtVaSetValues (skymap_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    NULL);

	logo_w = mkLogo(main_w);
	XtVaSetValues (logo_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    NULL);

	cur_w = mkCurrent(main_w);
	XtVaSetValues (cur_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_WIDGET,
	    XmNleftWidget, skymap_w,
	    XmNrightAttachment, XmATTACH_WIDGET,
	    XmNrightWidget, logo_w,
	    NULL);

	hf_w = XtVaCreateManagedWidget ("HF", xmFormWidgetClass, main_w,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, skymap_w,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    NULL);

	    cam_w = mkCamera(hf_w);
	    XtVaSetValues (cam_w,
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		NULL);

	    ctrl_w = mkControl(hf_w);
	    XtVaSetValues (ctrl_w,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_WIDGET,
		XmNleftWidget, cam_w,
		XmNleftOffset, 10,
		NULL);

	    scope_w = mkScope(hf_w);
	    XtVaSetValues (scope_w,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		NULL);

	    dome_w = mkDome (hf_w);
	    XtVaSetValues (dome_w,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, scope_w,
		XmNrightOffset, 10,
		NULL);

	    cover_w = mkCover(hf_w);
	    XtVaSetValues(cover_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_WIDGET,
	    XmNrightWidget, dome_w,
	    XmNrightOffset, 10,
	    NULL);

	    status_w = mkStatus(hf_w);
	    XtVaSetValues (status_w,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_WIDGET,
		XmNleftWidget, ctrl_w,
		XmNleftOffset, 10,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, cover_w,
		XmNrightOffset, 10,
		NULL);

	info_w = mkInfo(main_w);
	XtVaSetValues (info_w,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, hf_w,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    NULL);

	msgf_w = mkMsg(main_w);
	XtVaSetValues (msgf_w,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, info_w,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    NULL);
}

/* set the given Pixel resource to newp.
 * return 1 if it was different, 0 if it was the same already.
 */
int
setColor (Widget w, char *resource, Pixel newp)
{
	Pixel oldp;

	XtVaGetValues (w, resource, &oldp, NULL);
	if (oldp != newp)
	    XtVaSetValues (w, resource, newp, NULL);
	return (oldp != newp);
}

void
setLt (Widget w, LtState s)
{
	if (setColor (w, XmNforeground, ltcolors[s]))
	    drawLt (w);
}

/* make a da for use as status indicator.
 * when exposed, it draws an indicator using its foreground color
 */
Widget
mkLight (Widget p_w)
{
	Display *dsp = XtDisplay(p_w);
	Pixel bg_p, fg_p, sel_p;
	Widget da_w;
	Colormap cm;

	da_w = XtVaCreateManagedWidget ("LDA", xmDrawingAreaWidgetClass, p_w,
	    XmNwidth, LIGHTW+8,
	    XmNheight, LIGHTH+8,
	    NULL);

	/* get shadow colors for the current background */
	XtVaGetValues (da_w,
	    XmNcolormap, &cm,
	    XmNbackground, &bg_p,
	    NULL);
	XmGetColors (DefaultScreenOfDisplay(dsp), cm, bg_p, &fg_p, &tshadow_p,
							&bshadow_p, &sel_p);

	/* default color to Idle for now */
	XtVaSetValues (da_w, XmNforeground, ltcolors[LTIDLE], NULL);

	XtAddCallback (da_w, XmNexposeCallback, lightExpCB, NULL);

	return (da_w);
}

/* write a message with time stamp and save to log */
void
msg (char *fmt, ...)
{
	char utbuf[32];
	char mbuf[1024];
	double ut;
	va_list ap;
	int ul, ml;

	time_t t=time(NULL);
	struct tm *tmp = gmtime (&t);
	sprintf (utbuf, "%04d-%02d-%02dT%02d:%02d:%02d", tmp->tm_year+1900,
		 tmp->tm_mon+1, tmp->tm_mday, tmp->tm_hour,
		 tmp->tm_min, tmp->tm_sec);
	/* start mbuf with current UT */
	/*	ut = utc_now (&telstatshmp->now);
		fs_sexa (utbuf, ut, 2, 3600);*/
	ul = sprintf (mbuf, "%s ", utbuf);

	/* append the message */
	va_start (ap, fmt);
	ml = ul + vsprintf (mbuf+ul, fmt, ap);
	va_end (ap);

	/* trim trailing whitespace and ignore if that's all there is */
	while (ml > ul && (mbuf[ml-1] == ' ' || mbuf[ml-1] == '\n'))
	    mbuf[--ml] = '\0';
	if (ml == ul)
	    return;

	/* add \n */
	mbuf[ml++] = '\n';
	mbuf[ml] = '\0';

	/* write and log it */
	rmsg (mbuf);
}

/* write a raw line to the message area and save to log */
void
rmsg (char *line)
{
	FILE *logfp;
	int ll;

	/* trim if over the max length */
	if (msg_txtl > MAXMSG) {
	    String new, old = XmTextGetString (msg_w);

	    for (new = &old[msg_txtl - MAXMSG]; *new++ != '\n'; )
		continue;
	    new = XtNewString (new);
	    XmTextSetString (msg_w, new);
	    msg_txtl = strlen (new);
	    XtFree (new);
	    XtFree (old);
	}

	/* add to screen -- but not too long */
	ll = strlen (line);
	XmTextReplace (msg_w, msg_txtl, msg_txtl, line);
	msg_txtl += ll;

	/* scroll so start of last line is visible */
	XmTextSetInsertionPosition (msg_w, msg_txtl-ll);

	/* append to log */
	logfp = telfopen (logfn, "a");
	if (logfp) {
	    fprintf (logfp, "%s", line);
	    fclose (logfp);
	}
}

/* set whether the user controls are sensitive */
void
guiSensitive (int whether)
{
	static Widget *batch_wp[] = {
	    &g_w[TSERV_W],
	    &g_w[TSTOW_W],
	    &g_w[TGOTO_W],
	    &g_w[TTRACK_W],
	    &g_w[CFHOME_W],
	    &g_w[CFLIM_W],
	    &g_w[CAUTOF_W],
	    &g_w[CPADDLE_W],
	    &g_w[CCNFOFF_W],
	    &g_w[CCNFOFF_W],
	};
	int i;

	for (i = 0; i < XtNumber(batch_wp); i++)
	    XtSetSensitive (*batch_wp[i], whether && xobs_alone);

	/* these are only ever sensitive if we are alone */
	if (!xobs_alone) {
	    XtSetSensitive (g_w[CSTOP_W], 0);
	    XtSetSensitive (g_w[CSOUND_W], 0);
	}

	/* dome sensitivity is left up to updateStatus() */
}

/* callback for the logo drawing area expose */
static void
logoExpCB (Widget w, XtPointer client, XtPointer call)
{
	XmDrawingAreaCallbackStruct *s = (XmDrawingAreaCallbackStruct *)call;
	XExposeEvent *evp = &s->event->xexpose;
	int logox = (LOGODASZ - logoxpma.width)/2;
	int logoy = (LOGODASZ - logoxpma.height)/2;

	if (!guigc)
	    mkGC();

	XCopyArea (evp->display, logopm, evp->window, guigc, evp->x - logox,
		evp->y - logoy, evp->width, evp->height, evp->x, evp->y); 
}

static Widget
mkLogo (Widget main_w)
{
	Display *dsp = XtDisplay(main_w);
	Window root = RootWindow(dsp, 0);
	char fn[1024];
	Widget da_w;
	Widget fr_w;
	int xpms;

	strcpy (fn, "archive/config/logo.xpm");
	telfixpath (fn, fn);

	fr_w = XtVaCreateManagedWidget ("LFr", xmFrameWidgetClass, main_w,
	    NULL);

	logoxpma.valuemask = 0;
	xpms = XpmReadFileToPixmap(dsp, root, fn, &logopm, NULL, &logoxpma);

	da_w = XtVaCreateManagedWidget ("LDA", xmDrawingAreaWidgetClass, fr_w,
	    XmNwidth, LOGODASZ,
	    XmNheight, LOGODASZ,
	    NULL);
	if (xpms == XpmSuccess)
	    XtAddCallback(da_w, XmNexposeCallback, logoExpCB, NULL);
	
	return (fr_w);
}

static Widget
mkCurrent(Widget main_w)
{
	typedef struct {
	    char *lbl;
	} Ctrl;
	static Ctrl ctrls[] = {
	    {""},
	    {"RA(J2000)"},
	    {"Dec(J2000)"},
	    {"HA"},
	    {"Altitude"},
	    {"Azimuth"},
	};

	Widget fr_w, f_w;
	Widget lf_w;
	Widget l_w[XtNumber(ctrls)];
	int i;

	mkFrame (main_w, "Current", "Positions", &fr_w, &f_w);

	lf_w = XtVaCreateManagedWidget ("CLF", xmFormWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    NULL);
	for (i = 0; i < XtNumber(l_w); i++) {
	    l_w[i] = XtVaCreateManagedWidget ("CL", xmLabelWidgetClass,lf_w,
		NULL);
	    wltprintf (prT, l_w[i], "%s", ctrls[i].lbl);
	}
	mkRow (lf_w, l_w, XtNumber(l_w), 20);

	lf_w = XtVaCreateManagedWidget ("CCF", xmFormWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, lf_w,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    NULL);
	l_w[0] = XtVaCreateManagedWidget ("CL", xmLabelWidgetClass, lf_w,
	    XmNalignment, XmALIGNMENT_CENTER,
	    NULL);
	wltprintf (prT, l_w[0], "Current");
	for (i = 1; i < XtNumber(l_w); i++) {
	    l_w[i] = XtVaCreateManagedWidget ("CPT",xmTextFieldWidgetClass,lf_w,
		XmNbackground, uneditableColor,
		XmNcolumns, 10,
		XmNcursorPositionVisible, False,
		XmNeditable, False,
		XmNmarginHeight, 1,
		XmNmarginWidth, 1,
		NULL);
	}
	mkRow (lf_w, l_w, XtNumber(l_w), 20);
	g_w[PCRA_W] = l_w[1];
	g_w[PCDEC_W] = l_w[2];
	g_w[PCHA_W] = l_w[3];
	g_w[PCALT_W] = l_w[4];
	g_w[PCAZ_W] = l_w[5];

	lf_w = XtVaCreateManagedWidget ("CCF", xmFormWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, lf_w,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    NULL);
	l_w[0] = XtVaCreateManagedWidget ("CL", xmLabelWidgetClass, lf_w,
	    XmNalignment, XmALIGNMENT_CENTER,
	    NULL);
	wltprintf (prT, l_w[0], "Target");
	for (i = 1; i < XtNumber(l_w); i++) {
	    l_w[i] = XtVaCreateManagedWidget ("CPT",xmTextFieldWidgetClass,lf_w,
		XmNbackground, uneditableColor,
		XmNcolumns, 10,
		XmNcursorPositionVisible, False,
		XmNeditable, False,
		XmNmarginHeight, 1,
		XmNmarginWidth, 1,
		NULL);
	}
	mkRow (lf_w, l_w, XtNumber(l_w), 20);
	g_w[PTRA_W] = l_w[1];
	g_w[PTDEC_W] = l_w[2];
	g_w[PTHA_W] = l_w[3];
	g_w[PTALT_W] = l_w[4];
	g_w[PTAZ_W] = l_w[5];

	lf_w = XtVaCreateManagedWidget ("CCF", xmFormWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, lf_w,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    NULL);
	l_w[0] = XtVaCreateManagedWidget ("CL", xmLabelWidgetClass, lf_w,
	    XmNalignment, XmALIGNMENT_CENTER,
	    NULL);
	wltprintf (prT, l_w[0], "Difference");
	for (i = 1; i < XtNumber(l_w); i++) {
	    l_w[i] = XtVaCreateManagedWidget ("CPT",xmTextFieldWidgetClass,lf_w,
		XmNbackground, uneditableColor,
		XmNcolumns, 10,
		XmNcursorPositionVisible, False,
		XmNeditable, False,
		XmNmarginHeight, 1,
		XmNmarginWidth, 1,
		NULL);
	}
	mkRow (lf_w, l_w, XtNumber(l_w), 20);
	g_w[PDRA_W] = l_w[1];
	g_w[PDDEC_W] = l_w[2];
	g_w[PDHA_W] = l_w[3];
	g_w[PDALT_W] = l_w[4];
	g_w[PDAZ_W] = l_w[5];

	return (fr_w);
}

static Widget
mkCamera(Widget main_w)
{
	Widget fr_w, f_w;
	Widget da_w;
	Widget tbl_w;
	Widget l_w, tf_w;
	Widget fof_w, fmb_w;
	Widget lgt_w;
	Widget w;

	mkFrame (main_w, "Camera", "Camera", &fr_w, &f_w);

	tbl_w = XtVaCreateManagedWidget ("CRC", xmRowColumnWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNnumColumns, 1,
	    XmNpacking, XmPACK_TIGHT,
	    NULL);

	w = mkPrompt (tbl_w, 8, &l_w, &tf_w, &da_w);
	wtip (tf_w, "Current focus position, microns from home, +in");
	wltprintf (prT, l_w, "Focus, %cm", XK_mu);
	XtVaSetValues (l_w, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
	XtVaSetValues (tf_w,
	    XmNbackground, uneditableColor,
	    XmNcursorPositionVisible, False,
	    XmNeditable, False,
	    XmNmaxLength, 8,
	    XmNmarginHeight, 1,
	    XmNmarginWidth, 1,
	    NULL);
	g_w[CFO_W] = tf_w;
	g_w[CFOLT_W] = da_w;

	w = mkPrompt (tbl_w, 8, &l_w, &tf_w, &da_w);
	wtip (tf_w, "Field rotator position angle");
	XtVaSetValues (l_w, XmNalignment, XmALIGNMENT_BEGINNING, NULL);
	wltprintf (prT, l_w, "Rotator");
	XtVaSetValues (tf_w,
	    XmNbackground, uneditableColor,
	    XmNcursorPositionVisible, False,
	    XmNeditable, False,
	    XmNmarginHeight, 1,
	    XmNmarginWidth, 1,
	    NULL);
	g_w[CR_W] = tf_w;
	g_w[CRL_W] = l_w;
	g_w[CRLT_W] = da_w;

	return (fr_w);
}

static Widget
mkDome(Widget main_w)
{
	Widget fr_w, f_w;
	Widget lf_w;
	Widget tbl_w;
	Widget l_w, tf_w;
	Widget w;

	mkFrame (main_w, "Roof", "Roof", &fr_w, &f_w);

	tbl_w = XtVaCreateManagedWidget ("DRC", xmRowColumnWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNisAligned, False,
	    XmNadjustMargin, False,
	    XmNpacking, XmPACK_TIGHT,
	    XmNspacing, 2,
	    NULL);

	lf_w = XtVaCreateManagedWidget ("DOF", xmFormWidgetClass, tbl_w, NULL);
	g_w[DOLT_W] = mkLight(lf_w);
	mkRow (lf_w, &g_w[DOLT_W], 1, 1);
	w = XtVaCreateManagedWidget ("O", xmPushButtonWidgetClass, tbl_w, NULL);
	wltprintf (pbT, w, "Open");
	XtAddCallback (w, XmNactivateCallback, domeOpenCB, NULL);
	g_w[DOPEN_W] = w;
	wtip (w, "Open the dome shutter (or roof)");

	w = XtVaCreateManagedWidget ("C", xmPushButtonWidgetClass, tbl_w, NULL);
	wltprintf (pbT, w, "Close");
	XtAddCallback (w, XmNactivateCallback, domeCloseCB, NULL);
	g_w[DCLOSE_W] = w;
	wtip (w, "Close the dome shutter (or roof)");
	lf_w = XtVaCreateManagedWidget ("DCF", xmFormWidgetClass, tbl_w, NULL);
	g_w[DCLT_W] = mkLight(lf_w);
	mkRow (lf_w, &g_w[DCLT_W], 1, 1);

	return (fr_w);
}

static Widget
mkCover(Widget main_w)
{
	Widget fr_w, f_w;
	Widget lf_w;
	Widget tbl_w;
	Widget l_w, tf_w;
	Widget w;

	mkFrame (main_w, "Cover", "Cover", &fr_w, &f_w);

	tbl_w = XtVaCreateManagedWidget ("CVRC", xmRowColumnWidgetClass, f_w,
		    XmNtopAttachment, XmATTACH_FORM,
		    XmNbottomAttachment, XmATTACH_FORM,
		    XmNleftAttachment, XmATTACH_FORM,
		    XmNrightAttachment, XmATTACH_FORM,
		    XmNisAligned, False,
		    XmNadjustMargin, False,
		    XmNpacking, XmPACK_TIGHT,
		    XmNspacing, 2,
		    NULL);

	lf_w = XtVaCreateManagedWidget ("CVOF", xmFormWidgetClass, tbl_w, NULL);
	g_w[COLT_W] = mkLight(lf_w);
	mkRow (lf_w, &g_w[COLT_W], 1, 1);

	w = XtVaCreateManagedWidget ("CO", xmPushButtonWidgetClass, tbl_w, NULL);
	wltprintf (pbT, w, "Open");
	XtAddCallback (w, XmNactivateCallback, coverOpenCB, NULL);
	g_w[COPEN_W] = w;
	wtip (w, "Open the mirror cover");

	w = XtVaCreateManagedWidget ("CC", xmPushButtonWidgetClass, tbl_w, NULL);
	wltprintf (pbT, w, "Close");
	XtAddCallback (w, XmNactivateCallback, coverCloseCB, NULL);
	g_w[CCLOSE_W] = w;
	wtip (w, "Close the mirror cover");

	lf_w = XtVaCreateManagedWidget ("CVCF", xmFormWidgetClass, tbl_w, NULL);
	g_w[CVLT_W] = mkLight(lf_w);
	mkRow (lf_w, &g_w[CVLT_W], 1, 1);

	return (fr_w);
}

static Widget
mkControl(Widget main_w)
{
	typedef struct {
	    char *lbl;		/* button label */
	    int tb;		/* 1 for TB, 0 for PB */
	    void (*cb)();	/* callback, nor NULL */
	    Widget *wp;		/* widget, or NULL */
	    char *tip;		/* tip */
	} Ctrl;
	static Ctrl ctrls[] = {
	    {"Stop",        0, g_stop,   &g_w[CSTOP_W],
	    	"Stop all equipment"},
	    {"Quit",        0, g_exit,   &g_w[CEXIT_W],
	    	"Quit this program"},
	    {"Find Homes",  0, g_home,   &g_w[CFHOME_W],
	    	"Start telescope seeking all home switches, if any"},
	    {"Find Limits", 0, g_limit,  &g_w[CFLIM_W],
	    	"Start telescope seeking all limit switches, if any"},
	    {"No Confirm",  1, g_confirm,   &g_w[CCNFOFF_W],
	    	"Toggle whether to confirm actions first"},
	    {"Paddle",      0, g_paddle, &g_w[CPADDLE_W],
	    	"Toggle a tool to directly command telescope motions"}, 
	    {"Sounds",      1, soundCB,  &g_w[CSOUND_W],
	    	"Toggle whether to make sounds while equipment is setting up"},
	};
	Widget fr_w, f_w;
	Widget tbl_w;
	Widget w;
	int i;

	mkFrame (main_w, "Control", "Control", &fr_w, &f_w);

	tbl_w = XtVaCreateManagedWidget ("CRC", xmRowColumnWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNnumColumns, (XtNumber(ctrls)+1)/2,
	    XmNadjustLast, False,
	    XmNisAligned, False,
	    XmNorientation, XmHORIZONTAL,
	    XmNpacking, XmPACK_COLUMN,
	    XmNspacing, 5,
	    NULL);

	    for (i = 0; i < XtNumber(ctrls); i++) {
		Ctrl *cp = &ctrls[i];

		if (cp->tb) {
		    w = XtVaCreateManagedWidget ("T", xmToggleButtonWidgetClass,
		    	tbl_w,
			XmNalignment, XmALIGNMENT_BEGINNING,
			XmNindicatorOn, True,
			XmNindicatorType, XmN_OF_MANY,
			XmNvisibleWhenOff, True,
			XmNmarginHeight, 0,
			NULL);
		    if (cp->cb)
			XtAddCallback (w, XmNvalueChangedCallback, cp->cb,NULL);

		} else {
		    w = XtVaCreateManagedWidget ("P", xmPushButtonWidgetClass,
			tbl_w,
			XmNentryAlignment, XmALIGNMENT_CENTER,
			XmNmarginHeight, 0,
			NULL);
		    if (strcmp(cp->lbl, "Stop") == 0)
			XtVaSetValues (w,
			    XmNforeground, getColor (toplevel_w, "white"),
			    XmNbackground, getColor (toplevel_w, "red"),
			    NULL);
		    if (cp->cb)
			XtAddCallback (w, XmNactivateCallback, cp->cb, NULL);
		}

		wltprintf (pbT, w, "%s", cp->lbl);
		if (cp->wp)
		    *(cp->wp) = w;
		if (cp->tip)
		    wtip (w, cp->tip);
	    }

	return (fr_w);
}

static Widget
mkStatus(Widget main_w)
{
	typedef struct {
	    char *lbl;
	    Widget *wp;
	} Ctrl;
	static Ctrl ctrls[] = {
	    {"Tracking", &g_w[STLT_W]},
	    {"Slewing", &g_w[SSLT_W]},
	    {"Homing", &g_w[SHLT_W]},
	    {"Limiting", &g_w[SLLT_W]},
	    {"Confirm", &g_w[SCLT_W]},
	};
	Widget fr_w, f_w;
	Widget tbl_w;
	int i;

	mkFrame (main_w, "Status", "Status", &fr_w, &f_w);

	tbl_w = XtVaCreateManagedWidget ("CRC", xmRowColumnWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNpacking, XmPACK_TIGHT,
	    XmNspacing, 4,
	    NULL);

	for (i = 0; i < XtNumber(ctrls); i++) {
	    Widget sf_w, sl_w;
	    Widget lt_w;

	    sf_w = XtVaCreateManagedWidget ("SF", xmFormWidgetClass, tbl_w,
		NULL);

	    lt_w = mkLight (sf_w);
	    XtVaSetValues (lt_w,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		NULL);

	    setLt (lt_w, LTIDLE);

	    if (ctrls[i].wp)
		*ctrls[i].wp = lt_w;

	    sl_w = XtVaCreateManagedWidget ("P", xmLabelWidgetClass, sf_w,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, lt_w,
		XmNalignment, XmALIGNMENT_BEGINNING,
		NULL);
	    wltprintf (prT, sl_w, "%s", ctrls[i].lbl);
	}

	return (fr_w);
}

static Widget
mkScope(Widget main_w)
{
	typedef struct {
	    char *lbl;
	    GUIWidgets gw;
	    char *tip;
	} Ctrl;
	static Ctrl ctrls[] = {
	    {"RA",  TRA_W,    "Target RA, H:M:S"},
	    {"Dec", TDEC_W,   "Target Declination, D:M:S"},
	    {"Ep",  TEPOCH_W, "Target Epoch, years"},
	    {"HA",  THA_W,    "Target Hour angle, H:M:S"},
	    {"Alt", TALT_W,   "Target altitude above horizon, D:M:S"},
	    {"Az",  TAZ_W,    "Target azimuth, E of N, D:M:S"},
	};
	Widget fr_w, f_w;
	Widget rc_w;
	Widget rrc_w, rf_w;
	Widget l_w[3];
	Widget tf_w, lb_w;
	int i;

	mkFrame (main_w, "Scope", "Telescope", &fr_w, &f_w);

	rrc_w = XtVaCreateManagedWidget ("SRC", xmRowColumnWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    XmNpacking, XmPACK_TIGHT,
	    XmNspacing, 6,
	    NULL);
	    
	rf_w = XtVaCreateManagedWidget ("SF", xmFormWidgetClass, rrc_w,
	    NULL);

	l_w[0] = XtVaCreateManagedWidget ("SSt", xmPushButtonWidgetClass, rf_w,
	    NULL);
	wltprintf (pbT, l_w[0], "Service");
	XtAddCallback (l_w[0], XmNactivateCallback, s_service, NULL);
	g_w[TSERV_W] = l_w[0];
	wtip (l_w[0], "Slew to service position");

	l_w[1] = XtVaCreateManagedWidget ("SSv", xmPushButtonWidgetClass, rf_w,
	    NULL);
	wltprintf (pbT, l_w[1], "Stow");
	XtAddCallback (l_w[1], XmNactivateCallback, s_stow, NULL);
	g_w[TSTOW_W] = l_w[1];
	wtip (l_w[1], "Slew to stow position");

	l_w[2] = XtVaCreateManagedWidget ("SGT", xmPushButtonWidgetClass, rf_w,
	    NULL);
	wltprintf (pbT, l_w[2], "Slew");
	XtAddCallback (l_w[2], XmNactivateCallback, s_goto, NULL);
	g_w[TGOTO_W] = l_w[2];
	wtip (l_w[2], "Slew to coordinates and stop");

	mkRow (rf_w, l_w, 3, 8);
	    
	rf_w = XtVaCreateManagedWidget ("SF", xmFormWidgetClass, rrc_w,
	    NULL);

	l_w[0] = XtVaCreateManagedWidget ("SSt", xmPushButtonWidgetClass, rf_w,
	    NULL);
	wltprintf (pbT, l_w[0], "Here");
	XtAddCallback (l_w[0], XmNactivateCallback, s_here, NULL);
	g_w[THERE_W] = l_w[0];
	wtip (l_w[0], "Load all fields with current scope position");

	l_w[2] = XtVaCreateManagedWidget ("SGT", xmPushButtonWidgetClass, rf_w,
	    NULL);
	wltprintf (pbT, l_w[2], "Track");
	XtAddCallback (l_w[2], XmNactivateCallback, s_track, NULL);
	g_w[TTRACK_W] = l_w[2];
	wtip (l_w[2], "Slew to coordinates and track");

	mkRow (rf_w, l_w, 3, 8);

	rc_w = XtVaCreateManagedWidget ("CRC", xmRowColumnWidgetClass, rrc_w,
	    XmNmarginWidth, 0,
	    XmNnumColumns, 2,
	    XmNpacking, XmPACK_COLUMN,
	    NULL);

	for (i = 0; i < XtNumber(ctrls); i++) {
	    mkPrompt (rc_w, 9, &lb_w, &tf_w, NULL);
	    wltprintf (prT, lb_w, "%s", ctrls[i].lbl);
	    XtVaSetValues (tf_w,
		XmNbackground, editableColor,
		XmNpendingDelete, False,
		XmNmaxLength, 9,
		NULL);
	    if (ctrls[i].tip)
		wtip (tf_w, ctrls[i].tip);
	    if (ctrls[i].gw)
		g_w[ctrls[i].gw] = tf_w;
	    XtAddCallback (tf_w, XmNvalueChangedCallback, s_edit,
						    (XtPointer)ctrls[i].gw);
	}

	return (fr_w);
}

static void
msg_erase (Widget w, XtPointer client, XtPointer call)
{
	XmTextReplace (msg_w, 0, msg_txtl, "");
	msg_txtl = 0;
	XFlush (XtDisplay(toplevel_w));
}

static void
msg_latest (Widget w, XtPointer client, XtPointer call)
{
	XmTextSetInsertionPosition (msg_w, msg_txtl);
}

static Widget
mkMsg(Widget main_w)
{
	Widget fr_w, f_w;
	Widget e_w, l_w;
	Widget sw_w;
	Widget sb_w;

	mkFrame (main_w, "Msg", "Messages", &fr_w, &f_w);

	e_w = XtVaCreateManagedWidget ("E", xmPushButtonWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    NULL);
	wltprintf (pbT, e_w, "Erase");
	XtAddCallback (e_w, XmNactivateCallback, msg_erase, NULL);
	wtip (e_w, "Erase all messages");

	l_w = XtVaCreateManagedWidget ("L", xmPushButtonWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, e_w,
	    XmNtopOffset, 5,
	    XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET,
	    XmNleftWidget, e_w,
	    XmNrightAttachment, XmATTACH_FORM,
	    NULL);
	wltprintf (pbT, l_w, "Last");
	XtAddCallback (l_w, XmNactivateCallback, msg_latest, NULL);
	wtip (l_w, "Scroll to bottom to see most recent message");

	msg_w = XmCreateScrolledText (f_w, "messageST", NULL, 0);
	sw_w = XtParent(msg_w);
	XtVaSetValues (sw_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_WIDGET,
	    XmNrightWidget, e_w,
	    XmNrightOffset, 5,
	    NULL);
	XtVaSetValues (msg_w,
	    XmNeditMode, XmMULTI_LINE_EDIT,
	    XmNeditable, False,
	    XmNcursorPositionVisible, False,
	    XmNrows, 10,
	    XmNmarginHeight, 1,
	    XmNmarginWidth, 1,
	    NULL);
	wtip (msg_w, "Scrolled list of Status messages");

	/* turn off hor, turn on vert scroll bar */
	XtVaGetValues (sw_w, XmNhorizontalScrollBar, &sb_w, NULL);
	if (sb_w)
	    XtUnmanageChild (sb_w);
	XtVaGetValues (sw_w, XmNverticalScrollBar, &sb_w, NULL);
	if (sb_w)
	    XtManageChild (sb_w);

	XtManageChild (msg_w);

	return (fr_w);
}

static Widget
mkInfo(Widget main_w)
{
	typedef struct {
	    char *lbl;
	    Widget *wp;
	    char *tip;
	} Ctrl;
	static Ctrl ctrls[] = {
	    {"Local", &g_w[ILT_W], "Local Civil Time"},
	    {"UT", &g_w[IUT_W], "Universal Time"},
	    {"UT Date", &g_w[IUTD_W], "Universal Date"},
	    {"LST", &g_w[ILST_W], "Local Sidereal Time"},
	    {"JD", &g_w[IJD_W], "Julian Date"},
	    {"Moon", &g_w[IMOON_W], "Moon phase, direction and altitude"},
	    {"Sun", &g_w[ISUN_W], "Sun direction and altitude"},
	    {"Dusk", &g_w[IDUSK_W], "UT end of twilight"},
	    {"Dawn", &g_w[IDAWN_W], "UT start of twilight"},
	};
	char site[1024];
	Widget fr_w, f_w;
	Widget lf_w;
	Widget sep_w;
	Widget l_w[XtNumber(ctrls)];
	int i;

	sprintf (site, "Site Information at %s", BANNER);
	mkFrame (main_w, "Info", site, &fr_w, &f_w);

	lf_w = XtVaCreateManagedWidget ("ILF", xmFormWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    NULL);
	for (i = 0; i < XtNumber(l_w); i++) {
	    l_w[i] = XtVaCreateManagedWidget ("IL", xmLabelWidgetClass, lf_w,
		NULL);
	    wltprintf (prT, l_w[i], "%s", ctrls[i].lbl);
	}
	mkRow (lf_w, l_w, XtNumber(l_w), 25);

	lf_w = XtVaCreateManagedWidget ("ILF", xmFormWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_WIDGET,
	    XmNtopWidget, lf_w,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_FORM,
	    NULL);
	for (i = 0; i < XtNumber(l_w); i++) {
	    l_w[i] = XtVaCreateManagedWidget ("IL", xmTextFieldWidgetClass,lf_w,
		XmNbackground, uneditableColor,
		XmNcolumns, 8,
		XmNcursorPositionVisible, False,
		XmNeditable, False,
		XmNmarginHeight, 1,
		XmNmarginWidth, 1,
		NULL);
	    if (ctrls[i].wp)
		*ctrls[i].wp = l_w[i];
	    if (ctrls[i].tip)
		wtip (l_w[i], ctrls[i].tip);
	}
	mkRow (lf_w, l_w, XtNumber(l_w), 25);

	return (fr_w);
}

/* make a frame in the given parent with the given name and title, put a form
 * in it and return both the frame and form.
 */
static void
mkFrame (Widget parent_w, char *name, char *title, Widget *frame_wp,
Widget *form_wp)
{
	Widget f_w, fr_w, title_w;

	fr_w = XtVaCreateManagedWidget (name, xmFrameWidgetClass, parent_w,
	    NULL);

	title_w = XtVaCreateManagedWidget ("FL", xmLabelWidgetClass, fr_w,
	    XmNchildType, XmFRAME_TITLE_CHILD,
	    XmNchildHorizontalAlignment, XmALIGNMENT_CENTER,
	    NULL);
	wltprintf (frT, title_w, "%s", title);

	f_w = XtVaCreateManagedWidget ("FF", xmFormWidgetClass, fr_w,
	    XmNchildType, XmFRAME_WORKAREA_CHILD,
	    NULL);

	*frame_wp = fr_w;
	*form_wp = f_w;
}

/* put n widgets in a form horizontally with gaps of 1 and each w wide */
static void
mkRow (Widget f_w, Widget w_p[], int n, int w)
{
	int i;

	XtVaSetValues (f_w, XmNfractionBase, n*(w+1)-1, NULL);

	for (i = 0; i < n; i++)
	    XtVaSetValues (w_p[i],
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_POSITION,
		XmNleftPosition, i*(w+1),
		XmNrightAttachment, XmATTACH_POSITION,
		XmNrightPosition, w+i*(w+1),
		NULL);
}

/* make a horizontal label/text-field pair, giving the tf tc columns.
 * add a light box on the right if *lb_wp
 */
static Widget
mkPrompt (Widget p_w, int tc, Widget *l_wp, Widget *tf_wp, Widget *lb_wp)
{
	Widget f_w, l_w, tf_w;

	f_w = XtVaCreateManagedWidget ("PF", xmFormWidgetClass, p_w, NULL);

	tf_w = XtVaCreateManagedWidget ("PTF", xmTextFieldWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNcolumns, tc,
	    XmNmarginHeight, 1,
	    XmNmarginWidth, 1,
	    NULL);

	if (lb_wp) {
	    Widget lb_w = mkLight (f_w);

	    XtVaSetValues (lb_w,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		NULL);

	    XtVaSetValues (tf_w,
		XmNrightAttachment, XmATTACH_WIDGET,
		XmNrightWidget, lb_w,
		XmNrightOffset, 10,
		NULL);

	    *lb_wp = lb_w;
	} else {
	    XtVaSetValues (tf_w,
		XmNrightAttachment, XmATTACH_FORM,
		NULL);
	}

	l_w = XtVaCreateManagedWidget ("PL", xmLabelWidgetClass, f_w,
	    XmNtopAttachment, XmATTACH_FORM,
	    XmNbottomAttachment, XmATTACH_FORM,
	    XmNleftAttachment, XmATTACH_FORM,
	    XmNrightAttachment, XmATTACH_WIDGET,
	    XmNrightWidget, tf_w,
	    NULL);

	*l_wp = l_w;
	*tf_wp = tf_w;
	return (f_w);
}

static void
lightExpCB (Widget wid, XtPointer client, XtPointer call)
{
	drawLt (wid);
}

static void
drawLt (Widget wid)
{
	Display *dsp = XtDisplay(wid);
	Window win = XtWindow(wid);
	Dimension w, h;
	int x, y;
	Pixel p;

	if (!win)
	    return;

	XtVaGetValues (wid,
	    XmNforeground, &p,
	    XmNwidth, &w,
	    XmNheight, &h,
	    NULL);

	x = (w - LIGHTW)/2;
	y = (h - LIGHTH)/2;

	if (!guigc)
	    mkGC();

	XSetForeground (dsp, guigc, p);
	XFillRectangle (dsp, win, guigc, x, y, LIGHTW, LIGHTH);

	XSetForeground (dsp, guigc, bshadow_p);
	XDrawLine (dsp, win, guigc, x, y+LIGHTH, x+LIGHTW, y+LIGHTH);
	XDrawLine (dsp, win, guigc, x+LIGHTW, y+LIGHTH, x+LIGHTW, y);
	XDrawLine (dsp, win, guigc, x-3, y+LIGHTH+3, x+LIGHTW+3, y+LIGHTH+3);
	XDrawLine (dsp, win, guigc, x+LIGHTW+3, y+LIGHTH+3, x+LIGHTW+3, y-3);

	XSetForeground (dsp, guigc, tshadow_p);
	XDrawLine (dsp, win, guigc, x+LIGHTW, y, x, y);
	XDrawLine (dsp, win, guigc, x, y, x, y+LIGHTH);
	XDrawLine (dsp, win, guigc, x+LIGHTW+3, y-3, x-3, y-3);
	XDrawLine (dsp, win, guigc, x-3, y-3, x-3, y+LIGHTH+3);
}

/* print something to the labelString XmString resource of the given widget
 * using the given tag (aka charset).
 * take care not to do the i/o if it's the same string again.
 */
static void
wltprintf (char *tag, Widget w, char *fmt, ...)
{
	va_list ap;
	char newbuf[512], *txtp;

	va_start (ap, fmt);
	vsprintf (newbuf, fmt, ap);
	va_end (ap);

	get_xmstring (w, XmNlabelString, &txtp);
	if (strcmp (txtp, newbuf)) {
	    XmString str = XmStringCreate (newbuf, tag);
	    set_something (w, XmNlabelString, (char *)str);
	    XmStringFree (str);
	}
	XtFree (txtp);
}

static void
mkGC()
{
	guigc = XCreateGC(XtDisplay(toplevel_w), XtWindow(toplevel_w),0L,NULL);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: gui.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.3 $ $Name:  $"};
