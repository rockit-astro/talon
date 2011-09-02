/* code to allow reading in an sls file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>

#include <Xm/Xm.h>
#include <Xm/SelectioB.h>
#include <Xm/FileSB.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "strops.h"
#include "catalogs.h"
#include "configfile.h"
#include "telenv.h"
#include "xtools.h"
#include "scan.h"

#include "telsched.h"

static void createSLSFSB(void);
static void opensls_cb (Widget w, XtPointer client, XtPointer call);

static Widget sls_w;   		/* the catalog file selection dialog */

/* present a FSB to let user choose an .sls file to read */
void
manageSLSFileMenu ()
{
	if (!sls_w)
	    createSLSFSB();

	if (XtIsManaged(sls_w))
	    XtUnmanageChild (sls_w);
	else
	    XtManageChild (sls_w);
}

static void
createSLSFSB()
{
	sls_w = XmCreateFileSelectionDialog (toplevel_w, "CatFSB", NULL, 0);
	XtVaSetValues (XtParent(sls_w),
	    XmNtitle, "Scan List Selection",
	    NULL);

	XtAddCallback (sls_w, XmNokCallback, opensls_cb, 0);
	XtAddCallback (sls_w, XmNcancelCallback,
					(XtCallbackProc) XtUnmanageChild, 0);

	/* apply TELHOME */
	set_xmstring (sls_w, XmNdirectory, "./");
	set_xmstring (sls_w, XmNpattern, "*.sls");
}

static void
opensls_cb (Widget w, XtPointer client, XtPointer call)
{
	XmFileSelectionBoxCallbackStruct *s
				= (XmFileSelectionBoxCallbackStruct *)call;
	char filename[1024];
	Scan scan, *sp = &scan;
	Obs *newobs;
	int nnewobs;
	char *base;
	char *str;
	Now nowscan;
	FILE *fp;

	/* get filename */
	XmStringGetLtoR (s->value, XmSTRING_DEFAULT_CHARSET, &str);
	(void) strcpy (filename, str);
	XtFree (str);
	base = basenm(filename);

	fp = fopen (filename, "r");
	if (!fp) {
	    msg ("%s: %s", base, strerror(errno));
	    return;
	}

	watch_cursor(1);

	nowscan = now;
	newobs = 0;
	nnewobs = 0;
	while (readNextSLS (fp, sp, NULL) == 0) {
	    double lststart;
	    Obs *op;

	    newobs = (Obs*)XtRealloc((char *)newobs, (nnewobs+1)*(sizeof(Obs)));
	    op = &newobs[nnewobs++];
	    initObs (op);
	    op->scan = *sp;
	    nowscan.n_mjd = sp->starttm/SPD + 25567.5;
	    now_lst (&nowscan, &lststart);
	    op->lststart = lststart;
	    op->utcstart = lst2utc (lststart);

	    switch (sp->status) {
	    case 'F':
		strcpy (op->yoff, "Scan is marked Failed by telrun.");
		op->off = 1;
		break;
	    case 'D':
		strcpy (op->yoff, "Scan is marked Done by telrun.");
		op->off = 1;
		op->done = 1;
		break;
	    }
	}

	fclose (fp);

	if (nnewobs > 0)
	    addSchedEntries (newobs, nnewobs);

	if (newobs)
	    free ((void *)newobs);
	watch_cursor(0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: readsls.c,v $ $Date: 2001/04/19 21:12:00 $ $Revision: 1.1.1.1 $ $Name:  $"};
