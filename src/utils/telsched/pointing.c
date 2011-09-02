/* code to add a complete pointing mesh over the whole sky to the scan list.
 * mesh is constrained by mount limits.
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
#include "xtools.h"
#include "telenv.h"
#include "configfile.h"
#include "scan.h"

#include "telsched.h"

static void create_pting_w (void);
static void init_pting_w(void);
static void close_cb (Widget w, XtPointer client, XtPointer call);
static void ok_cb (Widget w, XtPointer client, XtPointer call);
static int get_setup (double *startp, double *durp, double *meshsz,
    int *prip, char path[], char errmsg[]);
static void add_pting_scans(double startt, double dur, double meshsz,
    int pri, char path[]);
static void add_field (Obs *op, double t, double d, int pri, int idx,
    char path[]);

static char fnprefix[] = "ptg";	/* prefix to use for created image names */
static char ptdir[] = "archive/pointmesh";

static Widget pting_w;		/* main dialog */
static Widget startt_w;		/* start time, utc */
static Widget meshsz_w;		/* mesh size, degrees */
static Widget pri_w;		/* scheduling priority */
static Widget dir_w;		/* directory */
static Widget dur_w;		/* duration */

/* callback to allow inserting pointing mesh scans */
void
pting_cb (Widget w, XtPointer client, XtPointer call)
{
	if (!pting_w) {
	    create_pting_w();
	    init_pting_w();
	}

	if (XtIsManaged(pting_w))
	    XtUnmanageChild (pting_w);
	else
	    XtManageChild (pting_w);
}

static void
create_pting_w()
{
	typedef struct {
	    char *prompt;
	    char *tfname;
	    Widget *tfp;
	} Prompt;
	static Prompt prompts[] = {
	    /* N.B. put the widest one on top */
	    {"Image Directory:","ImDir",	&dir_w},
	    {"Sort PRIORITY:",	"Priority",	&pri_w},
	    {"UTC Start:",	"UTCStart",	&startt_w},
	    {"Size, degrees:",	"MeshSz",	&meshsz_w},
	    {"Duration, secs:",	"Dur",		&dur_w},
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
	pting_w = XmCreateFormDialog (toplevel_w, "PointMesh", args, n);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Setup Pointing Mesh"); n++;
	XtSetValues (XtParent(pting_w), args, n);

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
	    w = XmCreateLabel (pting_w, "L", args, n);
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
	    w = XmCreateTextField (pting_w, pp->tfname, args, n);
	    XtManageChild (w);
	    *pp->tfp = w;
	}

	/* sep */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, *prompts[XtNumber(prompts)-1].tfp);n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	sep_w = XmCreateSeparator (pting_w, "Sep", args, n);
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
	w = XmCreatePushButton (pting_w, "Apply", args, n);
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
	w = XmCreatePushButton (pting_w, "Close", args, n);
	XtManageChild (w);
	XtAddCallback (w, XmNactivateCallback, close_cb, NULL);
}

/* set up whatever is needed first time the dialog appears */
static void
init_pting_w()
{
	double duskmjd, dawnmjd;
	char buf[128];

	/* init mesh size */
	wtprintf (meshsz_w, "%.2f", raddeg(PTGRAD));

	/* init duration */
	wtprintf (dur_w, "%g", MESHEXPTIME);

	/* init time at dusk */
	dawnduskToday (&dawnmjd, &duskmjd);
	fs_sexa (buf, mjd_hr (duskmjd), 2, 60);
	wtprintf (startt_w, "%s", buf[0] == ' ' ? buf+1 : buf);

	/* init priority */
	wtprintf (pri_w, "0");

	/* init directory */
	telfixpath (buf, ptdir);
	wtprintf (dir_w, "%s", buf);
	XtVaSetValues (dir_w, XmNcolumns, strlen(buf), NULL);
}

static void
close_cb (Widget w, XtPointer client, XtPointer call)
{
	XtUnmanageChild (pting_w);
}

static void
ok_cb (Widget w, XtPointer client, XtPointer call)
{
	char errmsg[1024];
	double utcstart, dur, meshsz;
	char path[1024];
	int pri;

	/* fetch the options */
	if (get_setup (&utcstart, &dur, &meshsz, &pri, path, errmsg) < 0) {
	    msg ("%s", errmsg);
	    return;
	}

	/* do it */
	add_pting_scans (utcstart, dur, meshsz, pri, path);
}

/* assemble the options from the dialog.
 * return 0 if ok else fill errmsg[] and return -1.
 */
static int
get_setup(double *startp, double *durp, double *meshsz, int *prip,
char path[], char errmsg[])
{
	char *str;

	/* fetch the starting time, utc */
	get_something (startt_w, XmNvalue, (char *)&str);
	if (scansex (str, startp) < 0) {
	    sprintf (errmsg, "Bad start time format: %s", str);
	    XtFree (str);
	    return (-1);
	}
	XtFree (str);

	/* fetch the duration */
	get_something (dur_w, XmNvalue, (char *)&str);
	*durp = atof (str);
	XtFree (str);
	if (*durp <= 0) {
	    sprintf (errmsg, "Please enter a duration > 0");
	    return (-1);
	}

	/* fetch the mesh size, in rads */
	get_something (meshsz_w, XmNvalue, (char *)&str);
	*meshsz = atof (str);
	XtFree (str);
	if (*meshsz <= 0) {
	    sprintf (errmsg, "Please enter a mesh size > 0");
	    return (-1);
	}
	*meshsz = degrad(*meshsz);

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

	return (0);
}

/* add the batch of scans for the given mesh size */
static void
add_pting_scans(
double utcstart,	/* start time, utc */
double dur,		/* duration, seconds */
double meshsz,		/* mesh size, rads */
int pri,		/* priority */
char path[])		/* where to put images */
{

	Obs *oldobs;
	int noldobs;
	double duskmjd, dawnmjd, startmjd;
	char startstr[64];
	Obs *meshobs;
	Now nowscan;
	double ha, dec;
	double maxha, maxdec;
	int maxscans, nscans;
	int gowest;
	int i, base;

	/* find start and dusk/dawn times */
	dawnduskToday (&dawnmjd, &duskmjd);
	startmjd = mjd_day(now.n_mjd) + utcstart/24.0;
	if (!IGSUN && startmjd < duskmjd - 1./24./3600.) { /* 1 second slack! */
	    msg ("Please start Mesh after dusk");
	    return;
	}

	/* make array of plenty of new Obs. */
	maxscans = (int)(5*PI/(meshsz*meshsz));
	meshobs = (Obs *) calloc (maxscans, sizeof(Obs));
	if (!meshobs) {
	    msg ("No memory for mesh");
	    return;
	}

	/* report effort is starting */
	fs_sexa (startstr, mjd_hr(startmjd), 2, 60);
	msg ("Building mesh points beginning at %s ...", startstr);
	watch_cursor(1);

	/* start next index based on pointing scans already created. */
	get_obsarray (&oldobs, &noldobs);
	for (base = i = 0; i < noldobs; i++)
	    if (!strncmp(oldobs[i].scan.imagefn, fnprefix, sizeof(fnprefix)-1))
		base++;

	/* for each ha/dec .. assign to next time slot if it is ok */
	nowscan = now;
	nscans = 0;
	gowest = 1;
	maxdec = MAXDEC > degrad(89.9) ? degrad(89.9) : MAXDEC; /* avoid 90 */
	maxha = MAXHA;
	for (dec = degrad(-90) + meshsz; dec <= maxdec; dec += meshsz) {
	    /* zig back and forth to eliminate big slew back */
	    double dha = meshsz*gowest/cos(dec);

	    for (ha = -maxha*gowest; ha*gowest <= maxha; ha += dha) {
		Obs *obsp = &meshobs[nscans];
		Obj *objp = &obsp->scan.obj;
		double lststart;
		double ra;

		/* init obs each time -- will reuse if nscans not incremented */
		initObs (obsp);

		/* see if ha/dec is high enough to view now */
		nowscan.n_mjd = startmjd;
		now_lst(&nowscan, &lststart);
		objp->o_type = FIXED;
		objp->f_epoch = now.n_mjd; /* ~EOD */
		ra = hrrad(lststart) - ha;
		range (&ra, 2*PI);
		objp->f_RA = ra;
		objp->f_dec = dec;
		obj_cir (&nowscan, objp);
		if (objp->s_alt < MINALT)
		    continue;	/* can't see that ha/dec from here */
		if (objp->s_alt > MAXALT)
		    continue;	/* within the zenith hole */

		/* add new scan */
		add_field (obsp, lststart, dur, pri, base+nscans, path);

		/* increment start time */
		startmjd += SECS2SLOTS(dur)*SECSPERSLOT/SPD;

		/* beware of dawn if care */
		if (!IGSUN && startmjd > dawnmjd) {
		    msg ("Mesh does not complete before dawn");
		    free ((void *)meshobs);
		    watch_cursor(0);
		    return;
		}

		/* inc total number of scans */
		nscans++;
		if (nscans > maxscans) {
		    fprintf (stderr, "Bug! nscans > maxscans: %d > %d\n",
							    nscans, maxscans);
		    exit(1);
		}
	    }

	    /* go the other way next swath */
	    gowest *= -1;
	}

	/* update the main list */
	addSchedEntries (meshobs, nscans);
	watch_cursor(0);

	/* cleanup */
	free ((void *)meshobs);
}

/* fill in an Obs to observe at lst t */
static void
add_field (Obs *op, double t, double d, int pri, int idx, char path[])
{
	Scan *sp = &op->scan;

	ACPYZ (sp->schedfn, "PointMesh.sch");
	sprintf (sp->imagefn, "%.3s@@@%02x", fnprefix, idx);
	sprintf (sp->obj.o_name, "Point Mesh %d", idx);
	ACPYZ (sp->comment, "This entry was generated by telsched");
	ACPYZ (sp->title, "Pointing Mesh Image");
	ACPYZ (sp->observer, "Operator");
	ACPYZ (sp->imagedn, path);

	sprintf (sp->obj.o_name, "PMesh %3d", idx);

	ccdStr2Calib ("CATALOG", &sp->ccdcalib);
	sp->compress = MESHCOMP;
	sp->sx = 0;
	sp->sy = 0;
	sp->sw = DEFIMW;
	sp->sh = DEFIMH;
	sp->binx = DEFBIN;
	sp->biny = DEFBIN;
	sp->filter = MESHFILTER;
	sp->shutter = CCDSO_Open;
	sp->dur = d;

	op->lststart = t;
	op->utcstart = lst2utc(t);
	sp->startdt = 60;	/* seconds */
	sp->priority = pri;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: pointing.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
