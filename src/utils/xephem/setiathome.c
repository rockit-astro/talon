/* support for seti@home.
 *
 * removed when setiathome file formats kept changing too fast for me
 * to keep up with.
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#if defined(__STDC__)
#include <stdlib.h>
#include <string.h>
#endif

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/ToggleB.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "preferences.h"

extern Widget toplevel_w;
extern Colormap xe_cm;
extern XtAppContext xe_app;

extern FILE *fopenh P_((char *name, char *how));
extern Now *mm_get_now P_((void));
extern char *syserrstr P_((void));
extern void fs_sexa P_((char *out, double a, int w, int fracbase));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void obj_set P_((Obj *op, int dbidx));
extern void prompt_map_cb P_((Widget w, XtPointer client, XtPointer call));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *txt));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void sv_id P_((Obj *op));
extern void sv_point P_((Obj *op));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *msg, int app_modal));
extern void zero_mem P_((void *loc, unsigned len));

#define	SETICOLS	44	/* seti_w width */
#define	SETIROWS	18	/* seti_w height */
#define	AUTODT		10	/* auto refresh interval, seconds */

static void sah_create P_((void));
static void sah_refresh_cb P_((Widget w, XtPointer client, XtPointer call));
static void sah_help_cb P_((Widget w, XtPointer client, XtPointer call));
static void sah_popdown_cb P_((Widget w, XtPointer client, XtPointer call));
static void sah_close_cb P_((Widget w, XtPointer client, XtPointer call));
static void sah_auto_cb P_((Widget w, XtPointer client, XtPointer call));
static void sah_point_cb P_((Widget w, XtPointer client, XtPointer call));
static void sah_mark_cb P_((Widget w, XtPointer client, XtPointer call));
static void timer_auto_cb P_((XtPointer client, XtIntervalId *id));

static void timer_on P_((void));
static void timer_off P_((void));
static int doSETI P_((void));
static char *pullDate P_((char in[], char out[]));

static Widget sah_w;		/* main shell */
static Widget dir_w;		/* setiathome directory TF */
static Widget seti_w;		/* main description Text */
static Widget ar_w;		/* auto refresh TB */

static double sah_ra, sah_dec;	/* start_ra (hrs) and start_dec (degs) */
static XtIntervalId auto_tid;	/* auto refresh timer id, 0 while idle */

static char sahcategory[] = "SETI @ Home";	/* Save category */

/* create and bring up the seti@home dialog */
void
sah_manage ()
{
	if (!sah_w)
	    sah_create();

	XtPopup (sah_w, XtGrabNone);
	set_something (sah_w, XmNiconic, (XtArgVal)False);
}

/* called to put up or remove the watch cursor.  */
void
sah_cursor (c)
Cursor c;
{
	Window win;

	if (sah_w && (win = XtWindow(sah_w)) != 0) {
	    Display *dsp = XtDisplay(sah_w);
	    if (c)
		XDefineCursor (dsp, win, c);
	    else
		XUndefineCursor (dsp, win);
	}
}

/* create the s@h shell */
static void
sah_create()
{
	Widget sp_w, cl_w;
	Widget sahf_w;
	Widget w;
	Arg args[20];
	int n;

	/* create shell and form */
	n = 0;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNtitle, "xephem SETI@home"); n++;
	XtSetArg (args[n], XmNiconName, "SETI"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	sah_w = XtCreatePopupShell ("SETI", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (sah_w, XmNcolormap, (XtArgVal)xe_cm);
        XtAddCallback (sah_w, XmNpopdownCallback, sah_popdown_cb, 0);
	sr_reg (sah_w, "XEphem*SETI.x", sahcategory, 0);
	sr_reg (sah_w, "XEphem*SETI.y", sahcategory, 0);

	n = 0;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 5); n++;
	sahf_w = XmCreateForm (sah_w, "SETIF", args, n);
	XtAddCallback (sahf_w, XmNhelpCallback, sah_help_cb, 0);
	XtManageChild (sahf_w);

	/* label and directory TF */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	w = XmCreateLabel (sahf_w, "SAHDL", args, n);
	XtManageChild (w);
	set_xmstring (w, XmNlabelString, "SETI@home client directory:");

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	dir_w = XmCreateTextField (sahf_w, "directory", args, n);
	wtip (dir_w, "Path to setiathome working directory.");
	XtAddCallback (dir_w, XmNactivateCallback, sah_refresh_cb, NULL);
	sr_reg (dir_w, NULL, sahcategory, 1);
	XtManageChild (dir_w);

	/* Refresh PB */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, dir_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 10); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 40); n++;
	w = XmCreatePushButton (sahf_w, "Refresh", args, n);
	wtip (w, "Update information once from setiathome working directory.");
	XtAddCallback (w, XmNactivateCallback, sah_refresh_cb, NULL);
	XtManageChild (w);

	/* Auto refresh TB */
	n = 0;
	XtSetArg (args[n], XmNindicatorType, XmN_OF_MANY); n++;
	XtSetArg (args[n], XmNvisibleWhenOff, True); n++;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, dir_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 60); n++;
	ar_w = XmCreateToggleButton (sahf_w, "AutoRefresh", args, n);
	wtip (ar_w, "Automatically update information every few seconds.");
	XtAddCallback (ar_w, XmNvalueChangedCallback, sah_auto_cb, 0);
	set_xmstring (ar_w, XmNlabelString, "Auto refresh");
	XtManageChild (ar_w);

	/* Sky Point PB */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, ar_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 10); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 40); n++;
	sp_w = XmCreatePushButton (sahf_w, "SP", args, n);
	wtip (sp_w,
	    "Center and mark work unit on Sky View, and assign to ObjZ.");
	XtAddCallback (sp_w, XmNactivateCallback, sah_point_cb, 0);
	set_xmstring (sp_w, XmNlabelString, "Sky Point");
	XtManageChild (sp_w);

	/* Sky Mark PB */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, ar_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 60); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 90); n++;
	w = XmCreatePushButton (sahf_w, "SM", args, n);
	wtip (w,
	    "Mark work unit on Sky View if within field, and assign to ObjZ.");
	XtAddCallback (w, XmNactivateCallback, sah_mark_cb, 0);
	set_xmstring (w, XmNlabelString, "Sky Mark");
	XtManageChild (w);

	/* close */
	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 10); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 40); n++;
	cl_w = XmCreatePushButton (sahf_w, "Close", args, n);
	XtAddCallback (cl_w, XmNactivateCallback, sah_close_cb, 0);
	wtip (cl_w, "Close this dialog.");
	XtManageChild (cl_w);

	/* help */
	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 60); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 90); n++;
	w = XmCreatePushButton (sahf_w, "Help", args, n);
	wtip (w, "Obtain more information about this dialog.");
	XtAddCallback (w, XmNactivateCallback, sah_help_cb, 0);
	XtManageChild (w);

	/* R/O TF stretched in center */
	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sp_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, cl_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNeditable, False); n++;
	XtSetArg (args[n], XmNcursorPositionVisible, False); n++;
	XtSetArg (args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	seti_w = XmCreateText (sahf_w, "Table", args, n);
	wtip (seti_w, "Info about current setiathome processing.");
	XtManageChild (seti_w);
	sr_reg (seti_w, "XEphem*SETI*Table.rows", sahcategory, 0);
	sr_reg (seti_w, "XEphem*SETI*Table.columns", sahcategory, 0);
}

/* callback from popping down the main shell */
/* ARGSUSED */
static void
sah_popdown_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	timer_off();
}

/* callback from the Close PB */
/* ARGSUSED */
static void
sah_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtPopdown (sah_w);
}

/* callback for Help
 */
/* ARGSUSED */
static void
sah_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg[] = {
"Display status from info in a SETI@home directory."
};

	hlp_dialog ("SETIathome", msg, sizeof(msg)/sizeof(msg[0]));
}

/* callback from the Refresh button /and/ the directory text field.
 */
/* ARGSUSED */
static void
sah_refresh_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	doSETI();
}

/* callback from the Sky Point button.
 */
/* ARGSUSED */
static void
sah_point_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Now *np = mm_get_now();
	Obj o, *op = &o;

	zero_mem (op, sizeof(Obj));
	strcpy (op->o_name, "SETI@home");
	op->o_type = FIXED;
	set_fmag (op, 0);
	op->f_RA = (float)hrrad(sah_ra);
	op->f_dec = (float)degrad(sah_dec);
	op->f_epoch = J2000;
	obj_cir (np, op);
	sv_point (op);
	obj_set (op, OBJZ);
}

/* callback from the Sky Mark button.
 */
/* ARGSUSED */
static void
sah_mark_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Now *np = mm_get_now();
	Obj o, *op = &o;

	zero_mem (op, sizeof(Obj));
	strcpy (op->o_name, "SETI@home");
	op->o_type = FIXED;
	set_fmag (op, 0);
	op->f_RA = (float)hrrad(sah_ra);
	op->f_dec = (float)degrad(sah_dec);
	op->f_epoch = J2000;
	obj_cir (np, op);
	sv_id (op);
	obj_set (op, OBJZ);
}

/* commence auto refresh */
static void
timer_on()
{
	long interval;

	if (auto_tid)
	    XtRemoveTimeOut (auto_tid);

	/* looks nice to sync on whole AUTODT interval */
	interval = AUTODT - (time(NULL)%AUTODT);
	auto_tid = XtAppAddTimeOut (xe_app, interval*1000L, timer_auto_cb, 0);
}

/* cease auto refresh */
static void
timer_off()
{
	if (auto_tid) {
	    XtRemoveTimeOut (auto_tid);
	    auto_tid = (XtIntervalId)0;
	}
	XmToggleButtonSetState(ar_w, False, False);
}

/* callback from the auto refresh TB.
 */
/* ARGSUSED */
static void
sah_auto_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	/* turn on timer if want to and things are working, else turn off */
	if (XmToggleButtonGetState(w) && doSETI() == 0)
	    timer_on();
	else
	    timer_off();
}

/* called automatically during auto refresh */
static void
timer_auto_cb (client, id)
XtPointer client;
XtIntervalId *id;
{
	if (doSETI() < 0)
	    timer_off();
	else
	    timer_on();
}


/* open SETI@home files in dir_w and fill seti_w with info.
 * return 0 if ok, else issue xe_msg and return -1
 */
static int
doSETI ()
{
#if defined (__STDC__)
	time_t tm;
#else
	long tm;
#endif
	static char lastseti[SETIROWS][SETICOLS];
	char seti[SETIROWS][SETICOLS];
	char rastr[32], decstr[32];
	String dir;
	char buf[1024];
	char dirbuf[1024];
	char filebuf[1024];
	char msgbuf[1024];
	double cpu=0, prog=0;
	int major_version=0;
	int nresults=0;
	FILE *fp;
	int i, j, k;

	/* init lastseti[][] with all blanks and Text with blank lines */
	if (lastseti[0][0] == '\0') {
	    for (i = 0; i < SETIROWS; i++) {
		for (j = 0; j < SETICOLS; j++)
		    lastseti[i][j] = ' ';
		k = i*(SETICOLS+1);
		sprintf (msgbuf, "%*s", SETICOLS, "");
		XmTextReplace (seti_w, k, k+SETICOLS, msgbuf);
		if (i < SETIROWS-1)
		    XmTextReplace (seti_w, k+SETICOLS, k+SETICOLS+1, "\n");
	    }
	}

	/* clear seti[][] */
	zero_mem ((void *)seti, sizeof(seti));

	/* get dir name */
	dir = XmTextFieldGetString (dir_w);
	(void) strncpy (dirbuf, dir, sizeof(buf));
	XtFree (dir);

	/* check version file */
	(void) sprintf (filebuf, "%s/%s", dirbuf, "version.sah");
	fp = fopenh (filebuf, "r");
	if (!fp) {
	    (void) sprintf (msgbuf, "%s:\n%s", filebuf, syserrstr());
	    xe_msg (msgbuf, 1);
	    return (-1);
	}
	while (fgets (buf, sizeof(buf), fp)) {
	    if (!strncmp (buf, "major_version=", 14)) {
		major_version = atoi (buf+14);
		break;
	    }
	}
	fclose (fp);
	if (major_version < 2) {
	    xe_msg ("major_version in version.sah must be at least 2", 1);
	    return (-1);
	}

	/* open state file */
	(void) sprintf (filebuf, "%s/%s", dirbuf, "state.sah");
	fp = fopenh (filebuf, "r");
	if (!fp) {
	    (void) sprintf (msgbuf, "%s:\n%s", filebuf, syserrstr());
	    xe_msg (msgbuf, 1);
	    return (-1);
	}

	/* scan for interesting stuff */
	while (fgets (buf, sizeof(buf), fp)) {
	    if (!strncmp (buf, "cpu=", 4)) {
		cpu = atof (buf+4);
	    } else
	    if (!strncmp (buf, "prog=", 5)) {
		prog = atof (buf+5);
	    } else
	    if (!strncmp (buf, "cr=", 3)) {
		(void) sprintf (seti[14],"   Doppler: %g Hz/sec", atof(buf+3));
	    }
	}
	fclose (fp);

	/* add to seti buffer */
	fs_sexa (msgbuf, cpu/3600., 2, 3600);	/* secs to H:M:S */
	(void) sprintf (seti[15], "  CPU time: %s", msgbuf);
	(void) sprintf (seti[16], "  Progress: %5.2f%%", 100*prog);
	fs_sexa (msgbuf, cpu*(1/prog-1)/3600., 2, 3600); /* secs to H:M:S */
	(void) sprintf (seti[17], " Remaining: %s", msgbuf);

	/* open work_unit file */
	(void) sprintf (filebuf, "%s/%s", dirbuf, "work_unit.sah");
	fp = fopenh (filebuf, "r");
	if (!fp) {
	    (void) sprintf (msgbuf, "%s:\n%s", filebuf, syserrstr());
	    xe_msg (msgbuf, 1);
	    return (-1);
	}

	/* scan for interesting stuff */
	while (fgets (buf, sizeof(buf), fp)) {
	    buf[strlen(buf)-1] = '\0';
	    if (!strncmp (buf, "start_ra=", 9)) {
		sah_ra = atof (buf+9);
	    } else
	    if (!strncmp (buf, "start_dec=", 10)) {
		sah_dec = atof (buf+10);
	    } else
	    if (!strncmp (buf, "time_recorded=", 14)) {
		(void) sprintf (seti[10],"  Recorded: %s GMT",
							pullDate(buf,msgbuf));
	    } else
	    if (!strncmp (buf, "subband_base=", 13)) {
		(void) sprintf (seti[13], " Frequency: %.11f GHz",
							    atof(buf+13)*1e-9);
	    } else
	    if (!strncmp (buf, "receiver=", 9)) {
		(void) sprintf (seti[12], "  Receiver: %s", buf+9);
	    } else
	    if (!strncmp (buf, "name=", 5)) {
		(void) sprintf (seti[9], " Work unit: %.*s", SETICOLS-13,buf+5);
	    }
	}
	fclose (fp);

	/* open user_info file */
	(void) sprintf (filebuf, "%s/%s", dirbuf, "user_info.sah");
	fp = fopenh (filebuf, "r");
	if (!fp) {
	    (void) sprintf (msgbuf, "%s:\n%s", filebuf, syserrstr());
	    xe_msg (msgbuf, 1);
	    return (-1);
	}

	/* scan for interesting stuff */
	while (fgets (buf, sizeof(buf), fp)) {
	    buf[strlen(buf)-1] = '\0';
	    if (!strncmp (buf, "name=", 5)) {
		(void) sprintf (seti[2], " User name: %.*s", SETICOLS-13,buf+5);
	    } else
	    if (!strncmp (buf, "email_addr=", 11)) {
		(void) sprintf (seti[3], "User email: %.*s",SETICOLS-13,buf+11);
	    } else
	    if (!strncmp (buf, "register_time=", 14)) {
		(void) sprintf(seti[4], "Registered: %s GMT",
							pullDate(buf,msgbuf));
	    } else
	    if (!strncmp (buf, "last_result_time=", 17)) {
		(void) sprintf(seti[7], " Last Sent: %s GMT",
							pullDate(buf,msgbuf));
	    } else
	    if (!strncmp (buf, "nresults=", 9)) {
		nresults = atoi (buf+9);
	    } else
	    if (!strncmp (buf, "total_cpu=", 10)) {
		int d, h, m;
		double t = atof (buf+10);
		d = (int)floor(t/86400.);
		t -= d*86400.;
		h = (int)floor(t/3600.);
		t -= h*3600.;
		m = (int)floor(t/60.);
		t -= m*60.;
		(void) sprintf (seti[5], " Total CPU: %d days + %2d:%02d:%02d",
							    d, h, m, (int)t);
	    }
	}
	fclose (fp);

	/* add composite lines to seti buffer */
	fs_sexa (rastr, sah_ra, 2, 3600);
	fs_sexa (decstr, sah_dec, 3, 3600);
	(void) sprintf (seti[11],"  Position: RA=%s Dec=%s", rastr, decstr);
	(void) sprintf (seti[6], "Work units: %d sent", nresults);

	time (&tm);
	if (pref_get(PREF_ZONE) == PREF_UTCTZ) {
	    Now *np = mm_get_now();
	    tm += (long)(tz*3600);
	    strcpy (buf, "GMT");
	} else {
	    Now *np = mm_get_now();
	    strcpy (buf, tznm);
	}
	strcpy (msgbuf, ctime(&tm));
	msgbuf[strlen(msgbuf)-1] = '\0';	/* no trailing \n */
	(void) sprintf (seti[0], " Refreshed: %s %s", msgbuf, buf);

	/* install seti buffer in Text widget -- keep flashing to a minimum */
	msgbuf[1] = '\0';
	for (i = 0; i < SETIROWS; i++) {
	    for (j = 0; j < SETICOLS; j++) {
		if ((msgbuf[0] = seti[i][j]) != lastseti[i][j]) {
		    if (msgbuf[0] == '\0')
			msgbuf[0] = ' ';
		    k = i*(SETICOLS+1)+j;
		    XmTextReplace (seti_w, k, k+1, msgbuf);
		    lastseti[i][j] = msgbuf[0];
		}
	    }
	}
	XmTextShowPosition (seti_w, 0);


	return (0);
}

/* extract the date in () from in[] into out[] */
static char *
pullDate (in, out)
char in[], out[];
{
	if (sscanf (in, "%*[^(](%[^)]", out) != 1)
	    strcpy (out, "?");
	return (out);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: setiathome.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
