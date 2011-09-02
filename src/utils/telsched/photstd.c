/* code to manage making standard Landolt photometric calibration scans.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include <Xm/Xm.h>
#include <Xm/TextF.h>
#include <Xm/PushB.h>
#include <Xm/Label.h>
#include <Xm/Form.h>
#include <Xm/Separator.h>
#include <Xm/RowColumn.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "photstd.h"
#include "configfile.h"
#include "xtools.h"
#include "telenv.h"
#include "scan.h"

#include "telsched.h"

static void create_stdp_w(void);
static void init_stdp_w(void);
static void close_cb (Widget w, XtPointer client, XtPointer call);
static void ok_cb (Widget w, XtPointer client, XtPointer call);
static int get_setup (double *startp, int *nfp, int *prip, double durp[],
    char path[], char errmsg[]);
static int alt_sf (const void *i1, const void *i2);
static void add_stdfield (PStdStar *sp, Obs *op, int idx, double lststart,
    int pri, double dur, char path[], int filter);

static Widget stdp_w;		/* main setup dialog */
static Widget startt_w;		/* starting time text field */
static Widget nflds_w;		/* number of fields prompt text field */
static Widget bdur_w;		/* b duration prompt text field */
static Widget vdur_w;		/* v duration prompt text field */
static Widget rdur_w;		/* r duration prompt text field */
static Widget idur_w;		/* i duration prompt text field */
static Widget pri_w;		/* sort priority prompt text field */
static Widget dir_w;		/* image directory prompt text field */

static char fnprefix[] = "ldt";
static char caldir[] = "archive/photcal";

/* called to set up for inserting standard set of photometric scans. */
void
stdp_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!stdp_w) {
	    create_stdp_w();
	    init_stdp_w();
	}

	if (XtIsManaged(stdp_w))
	    XtUnmanageChild (stdp_w);
	else
	    XtManageChild (stdp_w);
}

static void
create_stdp_w()
{
	typedef struct {
	    char *prompt;
	    char *tfname;
	    Widget *tfp;
	} Prompt;
	static Prompt prompts[] = {
	    /* N.B. put the widest one on top */
	    {"Image Directory:","ImDir",	&dir_w},
	    {"Sort PRIORITY:",	"priority",	&pri_w},
	    {"Start UTC:",	"UTCStart",	&startt_w},
	    {"N Fields:",	"nFields",	&nflds_w},
	    {"B Dur, secs:",	"BDur",		&bdur_w},
	    {"V Dur, secs:",	"VDur",		&vdur_w},
	    {"R Dur, secs:",	"RDur",		&rdur_w},
	    {"I Dur, secs:",	"IDur",		&idur_w},
	};
	Widget sep_w;
	Widget flw = 0;
	Widget w;
	Arg args[20];
	int n;
	int i;

	n = 0;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 10); n++;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	stdp_w = XmCreateFormDialog (toplevel_w, "PhotStd", args, n);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Setup Standard Photometric Scans"); n++;
	XtSetValues (XtParent(stdp_w), args, n);

	/* make the label/prompt pairs */

	for (i = 0; i < XtNumber(prompts); i++) {
	    Prompt *pp = &prompts[i];

	    n = 0;
	    if (i == 0) {
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    } else {
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
		XtSetArg (args[n], XmNtopWidget, *pp[-1].tfp); n++;
	    }
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNalignment, XmALIGNMENT_END); n++;
	    w = XmCreateLabel (stdp_w, "L", args, n);
	    set_xmstring (w, XmNlabelString, pp->prompt);
	    XtManageChild (w);

	    n = 0;
	    if (i == 0) {
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
		flw = w;
	    } else {
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
		XtSetArg (args[n], XmNtopWidget, *pp[-1].tfp); n++;
	    }
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	    XtSetArg (args[n], XmNleftWidget, flw); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	    w = XmCreateTextField (stdp_w, pp->tfname, args, n);
	    XtManageChild (w);
	    *pp->tfp = w;
	}

	/* sep */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, *prompts[XtNumber(prompts)-1].tfp);n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (stdp_w, "Sep", args, n);
	XtManageChild (sep_w);

	/* controls at the bottom */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 20); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 40); n++;
	w = XmCreatePushButton (stdp_w, "Apply", args, n);
	XtManageChild (w);
	XtAddCallback (w, XmNactivateCallback, ok_cb, NULL);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, sep_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 60); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 80); n++;
	w = XmCreatePushButton (stdp_w, "Close", args, n);
	XtManageChild (w);
	XtAddCallback (w, XmNactivateCallback, close_cb, NULL);
}

/* set up whatever is needed first time the dialog appears */
static void
init_stdp_w()
{
	double duskmjd, dawnmjd;
	char buf[1024];

	/* init time to about midnight */
	dawnduskToday (&dawnmjd, &duskmjd);
	fs_sexa (buf, floor(mjd_hr((duskmjd+dawnmjd)/2)), 2, 60);
	wtprintf (startt_w, "%s", buf[0] == ' ' ? buf+1 : buf);

	/* init directory */
	telfixpath (buf, caldir);
	wtprintf (dir_w, "%s", buf);
	XtVaSetValues (dir_w, XmNcolumns, strlen(buf), NULL);

	/* init priority */
	wtprintf (pri_w, "0");

	/* init n fields */
	wtprintf (nflds_w, "6");

	/* init the durations */
	wtprintf (bdur_w, "%g", PHOTBDUR);
	wtprintf (vdur_w, "%g", PHOTVDUR);
	wtprintf (rdur_w, "%g", PHOTRDUR);
	wtprintf (idur_w, "%g", PHOTIDUR);
}

static void
close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (stdp_w);
}

static void
ok_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Obs *oldobs;
	int noldobs;
	Obs *new;
	char errmsg[1024];
	FILE *fp;
	Now startnow;
	double utcstart, lststart;
	int nfields, pri;
	int nf;
	PStdStar *sp;
	char pn[1024];
	char ref[1024];
	double dur[4];
	int nsp;
	int first;
	int base;
	int i;

	/* fetch the options */
	if (get_setup (&utcstart, &nfields, &pri, dur, pn, errmsg) < 0) {
	    msg ("%s", errmsg);
	    return;
	}

	/* find lststart from now and utcstart */
	startnow = now;
	startnow.n_mjd = mjd_day(now.n_mjd) + utcstart/24.0;
	now_lst(&startnow, &lststart);

	/* open and read the calibration standards */
	sprintf (ref, "%s/photcal.ref", pn);
	fp = telfopen (ref, "r");
	if (!fp) {
	    msg ("%s: %s", ref, strerror(errno));
	    return;
	}
	nsp = photStdRead (fp, &sp);
	fclose (fp);
	if (nsp < 0) {
	    msg ("%s: can not read", ref);
	    return;
	}
	if (nsp == 0) {
	    msg ("%s: 0 fields", ref);
	    photFree (sp, nsp);
	    return;
	}

	/* compute all altitudes */
	for (i = 0; i < nsp; i++)
	    obj_cir (&startnow, &sp[i].o);

	/* sort by increasing altitude */
	qsort ((void *)sp, nsp, sizeof(PStdStar), alt_sf);

	/* find lowest field rising just above MINALT to insure all stay up */
	for (first = 0; first < nsp; first++)
	    if (sp[first].o.s_alt > MINALT && sp[first].o.s_az < PI)
		break;
	nf = nsp - first;	/* total number of fields we can use */
	if (nf < nfields) {
	    msg ("Only found %d fields above %g degrees", nf, raddeg(MINALT));
	    photFree (sp, nsp);
	    return;
	}

	/* make a new list of 4*nfields zeroed Obs to build up.
	 * 4* because we do each field in each color BVRI.
	 */
	new = (Obs *) calloc (4*nfields, sizeof(Obs));
	if (!new) {
	    msg ("No memory for %d new fields", 4*nfields);
	    photFree (sp, nsp);
	    return;
	}

	/* start next index based on landolt scans already created. */
	get_obsarray (&oldobs, &noldobs);
	for (base = i = 0; i < noldobs; i++)
	    if (!strncmp(oldobs[i].scan.imagefn, fnprefix, sizeof(fnprefix)-1))
		base++;

	msg ("Adding %d Landolt scans...", 4*nfields);
	watch_cursor(1);

	/* add 4 scans for each of nfields evenly selected from among the
	 * ones in the range first .. (nsp-1)
	 */
	for (i = 0; i < nfields; i++) {
	    static char fl[4] = {'B', 'V', 'R', 'I'};
	    PStdStar *tsp = nfields > 1 ? &sp[first + i*(nf-1)/(nfields-1)]
					: &sp[first];
	    int fi = 4*i;
	    int id = base + fi;
	    int j;

	    for (j = 0; j < 4; j++) {
		add_stdfield(tsp,&new[fi+j],id+j,lststart,pri,dur[j],pn,fl[j]);
		lststart += TOTSECS(dur[j])/3600.;
	    }
	}

	/* update the main list */
	addSchedEntries (new, 4*nfields);
	watch_cursor(0);

	/* finished */
	photFree (sp, nsp);
	free ((char *)new);
}

/* assemble the options from the main dialog.
 * return 0 if ok else fill errmsg[] and return -1.
 */
static int
get_setup (double *startp, int *nfp, int *prip, double durp[], char path[],
char errmsg[])
{
	char *str;

	/* fetch the starting time */
	get_something (startt_w, XmNvalue, (char *)&str);
	if (scansex (str, startp) < 0) {
	    sprintf (errmsg, "Bad start time format: %s", str);
	    XtFree (str);
	    return (-1);
	}
	XtFree (str);

	/* fetch the number of fields */
	get_something (nflds_w, XmNvalue, (char *)&str);
	*nfp = atoi (str);
	XtFree (str);
	if (*nfp <= 0) {
	    sprintf (errmsg, "Please enter at least 1 field");
	    return (-1);
	}

	/* fetch the desired priority */
	get_something (pri_w, XmNvalue, (char *)&str);
	*prip = atoi (str);
	XtFree (str);

	/* fetch directory */
	str = XmTextFieldGetString (dir_w);
	strcpy (path, str);
	XtFree (str);
	if (path[0] != '/') {
	    sprintf (errmsg, "Directory must begin with /: %s", path);
	    return (-1);
	}

	/* fetch the durations */
	get_something (bdur_w, XmNvalue, (char *)&str);
	durp[0] = atof (str);
	XtFree (str);
	get_something (vdur_w, XmNvalue, (char *)&str);
	durp[1] = atof (str);
	XtFree (str);
	get_something (rdur_w, XmNvalue, (char *)&str);
	durp[2] = atof (str);
	XtFree (str);
	get_something (idur_w, XmNvalue, (char *)&str);
	durp[3] = atof (str);
	XtFree (str);

	return (0);
}

/* i1 and i2 are pointers to PStdStar.
 * sort by increasing altitude, in qsort fashion.
 */
static int
alt_sf (const void *i1, const void *i2)
{
	PStdStar *f1 = (PStdStar *)i1;
	PStdStar *f2 = (PStdStar *)i2;
	int r;

	if (f1->o.s_alt < f2->o.s_alt)
	    r = -1;
	else if (f1->o.s_alt > f2->o.s_alt)
	    r = 1;
	else
	    r = 0;

	return (r);
}

/* fill in an Obs from a PStdStar for the given color reference */
static void
add_stdfield (
PStdStar *sp,
Obs *op,
int idx,
double lststart,
int priority,
double duration,
char path[],
int filter)
{
	ACPYZ (op->scan.schedfn, "Landolt.sch");
	sprintf (op->scan.imagefn, "%.3s@@@%02x", fnprefix, idx);
	ACPYZ (op->scan.imagedn, path);
	ACPYZ (op->scan.comment, "This entry was generated by telsched");
	ACPYZ (op->scan.title, "Standard Landolt Photometric Survey Image");
	ACPYZ (op->scan.observer, "Operator");

	op->scan.obj = sp->o;
	memset ((char *)&op->rs, 0, sizeof(op->rs));

	(void) ccdStr2Calib ("CATALOG", &op->scan.ccdcalib);
	op->scan.compress = 0;
	op->scan.sx = 0;
	op->scan.sy = 0;
	op->scan.sw = DEFIMW;
	op->scan.sh = DEFIMH;
	op->scan.binx = DEFBIN;
	op->scan.biny = DEFBIN;
	op->scan.filter = filter;
	op->scan.shutter = CCDSO_Open;

	switch (filter) {
	case 'B': op->scan.dur = duration; break;
	case 'V': op->scan.dur = duration; break;
	case 'R': op->scan.dur = duration; break;
	case 'I': op->scan.dur = duration; break;
	default: printf ("add_stdfield(): Bogus filter: %c\n", filter); exit(1);
	}

	op->lststart = lststart;
	op->utcstart = lst2utc(lststart);
	op->scan.startdt = (int)floor(LSTDELTADEF*60+.5);
	op->scan.priority = priority;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: photstd.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
