/* code to allow scheduling whole catalogs.
 */

#include <stdio.h>
#include <stdlib.h>
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

#define	MAXLINE		4096	/* longest possible line */
#define	COMMENT		'#'	/* lines starting with this are ignored */

static void createCatFSB(void);

static Widget catd_w;   	/* the catalog file selection dialog */
static char catdir[1024];	/* cat directory; filled from CatFSB dir */

/* search all catalogs for op->scan.obj.o_name.
 * if found, fill in op->scan.obj and return 0,
 * else print error and return -1
 */
int
searchCatEntry (op)
Obs *op;	/* using op->scan.obj.o_name, fill in op->scan.obj */
{
	char m[1024];

	if (op->scan.obj.o_name[0] == '\0') {
	    msg ("No source");
	    return (-1);
	}

	/* create the catalog dialog so we can always pick up the default
	 * resource value, even if user hasn't brought it up yet.
	 */
	if (!catd_w)
	    createCatFSB();

	if (searchDirectory (catdir,op->scan.obj.o_name,&op->scan.obj,m) < 0) {
	    msg ("%s: %s", op->scan.obj.o_name, m);
	    return (-1);
	}

	return (0);
}

/* read the given filename of "catalog" records and create an array of
 * Obs records. Return the address of the malloced array to *opp.
 * set the camera and other fields to a reasonable default too.
 * return the number of entries in it or -1 if trouble.
 * N.B. if return > 0 caller should free() *opp when he's finished with it.
 */
int
readCatFile (catfn, opp)
char *catfn;
Obs **opp;
{
	char *base = basenm(catfn);
	char m[1024];
	Obs *obsp;
	Obj *objp;
	int i, n;

	/* read the Objs */
	n = readCatalog (catfn, &objp, m);
	if (n < 0) {
	    msg (m);
	    return (-1);
	}
	if (n == 0) {
	    msg ("No objects found in %s", base);
	    return (0);
	}

	/* malloc room for n clean Obs */
	obsp = (Obs *) calloc (n, sizeof(Obs));
	if (!obsp) {
	    msg ("Can not malloc room for %s new records", n);
	    free ((char *)objp);
	    return (-1);
	}

	/* copy each Obj into Obs.scan.obj and set other Obs fields too */
	for (i = 0; i < n; i++) {
	    Obs *osp = &obsp[i];
	    Obj *ojp = &objp[i];
	    Scan *sp = &osp->scan;

	    sp->obj = *ojp;

	    ACPYZ (sp->schedfn, base); /* taggable for delete */
	    ACPYZ (sp->observer, "Operator");
	    ACPYZ (sp->title, "Catalog");
	    sprintf (sp->comment, "Read from catalog %s", base);
	    ACPYZ (sp->imagedn, imdir);

	    sp->dur = 30;
	    sp->filter = FDEFLT;
	    sp->binx = sp->biny = DEFBIN;
	    sp->sx = sp->sy = 0;
	    sp->sw = DEFIMW;
	    sp->sh = DEFIMH;
	    sp->compress = COMPRESSH;
	    (void) ccdStr2Calib ("CATALOG", &sp->ccdcalib);
	    (void) ccdStr2SO ("OPEN", &sp->shutter);
	    sp->startdt = LSTDELTADEF*60;	/* want in seconds */

	    osp->lststart = NOTIME;
	}

	free ((char *)objp);

	*opp = obsp;
	return (n);
}

void
manageCatFileMenu()
{
	if (!catd_w)
	    createCatFSB();

	if (XtIsManaged(catd_w))
	    XtUnmanageChild (catd_w);
	else
	    XtManageChild (catd_w);
}

static void
createCatFSB()
{
	catd_w = XmCreateFileSelectionDialog (toplevel_w, "CatFSB", NULL, 0);
	XtVaSetValues (XtParent(catd_w),
	    XmNtitle, "Catalog File Selection",
	    NULL);

	XtAddCallback (catd_w, XmNokCallback, opencat_cb, 0);
	XtAddCallback (catd_w, XmNokCallback,
					(XtCallbackProc) XtUnmanageChild, 0);
	XtAddCallback (catd_w, XmNcancelCallback,
					(XtCallbackProc) XtUnmanageChild, 0);

	/* apply TELHOME */
	telfixpath (catdir, "archive/catalogs");
	set_xmstring (catd_w, XmNdirectory, catdir);
	set_xmstring (catd_w, XmNpattern, "*.edb");
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: cat.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
