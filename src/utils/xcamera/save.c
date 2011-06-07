/* dialog to allow saving the current image somewhere */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/Label.h>
#include <Xm/TextF.h>

#include "P_.h"
#include "astro.h"
#include "xtools.h"
#include "fits.h"
#include "fieldstar.h"
#include "strops.h"

#include "camera.h"

static void saveCB (Widget w, XtPointer client, XtPointer call);
static void scanCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);

static Widget save_w;	/* main save dialog */
static Widget dir_w;	/* text field containing directory */
static Widget tpl_w;	/* text field containing template */
static Widget fn_w;	/* text field containing file name */
static Widget auto_w;	/* TB for autosave option */

#define	MAXCOLS	128	/* max field length */

void
manageSave ()
{
	if (!save_w)
	    createSave();

	if (XtIsManaged(save_w))
	    raiseShell (save_w);
	else
	    XtManageChild (save_w);
}

/* using the dir and template text fields, create a filename.
 * return -1 and complain if can't use dir, else 0.
 */
int
mkTemplateName (char buf[])
{
	char *d, *dstr = XmTextFieldGetString (dir_w);
	char *t, *tstr = XmTextFieldGetString (tpl_w);
	char try[2*MAXCOLS];
	char *rpp;
	int i;

	/* skip leading dir blanks just to be really nice */
	for (d = dstr; *d == ' '; d++)
	    continue;

	/* check dir for creation rights */
	if (access (d, W_OK|X_OK|R_OK) < 0) {
	    msg ("%s: %s", d, strerror(errno));
	    XtFree (dstr);
	    XtFree (tstr);
	    return (-1);
	}

	/* skip leading template blanks just to be really nice */
	for (t = tstr; *t == ' '; t++)
	    continue;

	/* work on unique name using right-most group of #'s */
	if ((rpp = strrchr (t, '#')) != NULL) {
	    int npre, nwide;
	    char *lpp;
	    int fd;

	    /* find left end of group */
	    for (lpp = rpp; lpp >= t && *lpp == '#'; --lpp)
		continue;
	    lpp++;

	    /* number of chars up to lpp */
	    npre = lpp - t;

	    /* first char past last # */
	    rpp++;

	    /* scan for first file which can not be opened */
	    nwide = rpp - lpp;
	    for (i = 0; i < 999; i++) {
		sprintf (try, "%s/%.*s%0*d%s", d, npre, t, nwide, i, rpp);
		fd = open (try, O_RDWR);
		if (fd < 0)
		    break;
		(void) close (fd);
	    }
	} else {
	    /* no template wild cards so just use it -- too bad if exist */
	    sprintf (try, "%s/%s", d, t);
	}

	/* pass back result */
	strcpy (buf, try);

	/* free */
	XtFree (dstr);
	XtFree (tstr);

	/* ok */
	return (0);
}

/* save fn in state.fname */
void
setFName (char *fn)
{
	strncpy (state.fname, fn, sizeof(state.fname)-1);
}

/* set dir_w and fn_w from state.fname */
void
setSaveName()
{
	char *base = basenm(state.fname);
	char buf[MAXCOLS];

	XmTextFieldSetString (fn_w, base);

	if (base > state.fname)
	    sprintf (buf, "%.*s", base - state.fname - 1, state.fname);
	else
	    buf[0] = '\0';
	XmTextFieldSetString (dir_w, buf);

}

/* fill buf from dir_w and fn_w.
 * if trouble, tell why and return -1, else return 0.
 */
int
getSaveName(char *buf)
{
	char *dp, dcpy[MAXCOLS], *dstr = XmTextFieldGetString (dir_w);
	char *fp, fcpy[MAXCOLS], *fstr = XmTextFieldGetString (fn_w);

	/* copy and free then don't have to worry about it */
	strncpy (dcpy, dstr, sizeof(dcpy));
	strncpy (fcpy, fstr, sizeof(fcpy));
	XtFree (dstr);
	XtFree (fstr);

	/* skip leading dir blanks */
	for (dp = dcpy; *dp == ' '; dp++)
	    continue;
	if (*dp == '\0') {
	    msg ("No directory");
	    return (-1);
	}
	if (dp > dcpy)
	    XmTextFieldSetString (dir_w, dp);	/* fix for user */

	/* skip leading filename blanks, and scan if empty */
	for (fp = fcpy; *fp == ' '; fp++)
	    continue;
	if (*fp == '\0') {
	    if (mkTemplateName (fcpy) < 0)
		return (-1);	/* already explained */
	    if (strlen(fcpy) == 0) {
		msg ("No dir or file name");
		return (-1);
	    }
	    fp = basenm (fcpy);
	    XmTextFieldSetString (fn_w, fp);	/* show user */
	}
	if (fp > fcpy)
	    XmTextFieldSetString (fn_w, fp);	/* fix for user */

	/* append .fts if no extension on filename */
	if (!strchr (fp, '.')) {
	    strcat (fp, ".fts");
	    XmTextFieldSetString (fn_w, fp);	/* fix for user */
	}

	/* ok */
	sprintf (buf, "%s/%s", dp, fp);
	return (0);
}

/* return 0 if auto save is on, else -1 */
int
saveAuto()
{
	return (XmToggleButtonGetState(auto_w) ? 0 : -1);
}

/* save state.fimage in state.fname.
 * TODO: ask before hammering an existing file.
 */
void
writeImage()
{
	char *fn;
	int fd;

	if (!state.fimage.image) {
	    msg ("No image");
	    return;
	}
	if (state.lrflip || state.tbflip) {
	    msg ("Can not save flipped images -- FITS edits unclear");
	    XmToggleButtonSetState (auto_w, False, False);
	    return;
	}

	fn = state.fname;

	/* we don't support .fth files (yet :-) */
	if (strcasecmp (fn+strlen(fn)-4, ".fth") == 0) {
	    msg ("Saving in compressed format is not (yet) supported.");
	    return;
	}

	/* write the file */
	fd = open (fn, O_RDWR|O_CREAT|O_TRUNC, 0666);
	if (fd < 0)
	    msg ("%s: %s", fn, strerror (errno));
	else {
	    char errmsg[256];

	    fchmod (fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
	    if (writeFITS (fd, &state.fimage, errmsg, 1) < 0)
		msg ("%s: %s", fn, errmsg);
	    else
		msg ("Wrote %s", basenm(fn));
	    close (fd);
	}

	addHistory(state.fname);
}

void
createSave()
{
	Widget f_w;
	Widget rc_w;
	Widget l_w;
	Widget w;
	Arg args[20];
	int n;

	/* create form */
	
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_ANY); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	save_w = XmCreateFormDialog (toplevel_w, "Save", args,n);
	
	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Save"); n++;
	XtSetValues (XtParent(save_w), args, n);
	XtVaSetValues (save_w, XmNcolormap, camcm, NULL);

	/* make a master rc */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	rc_w = XmCreateRowColumn (save_w, "RC", args, n);
	XtManageChild (rc_w);

	/* make the directory prompt and text field */

	n = 0;
	l_w = XmCreateLabel (rc_w, "DL", args, n);
	wlprintf (l_w, "Directory:");
	XtManageChild (l_w);

	n = 0;
	XtSetArg (args[n], XmNmaxLength, MAXCOLS); n++;
	XtSetArg (args[n], XmNcolumns, 30); n++;
	dir_w = XmCreateTextField (rc_w, "Dir", args, n);
	XtAddCallback (dir_w, XmNactivateCallback, saveCB, NULL);
	XtManageChild (dir_w);

	/* make the file prompt and text field */

	n = 0;
	l_w = XmCreateLabel (rc_w, "FL", args, n);
	wlprintf (l_w, "File name:");
	XtManageChild (l_w);

	n = 0;
	XtSetArg (args[n], XmNmaxLength, MAXCOLS); n++;
	XtSetArg (args[n], XmNcolumns, 30); n++;
	fn_w = XmCreateTextField (rc_w, "File", args, n);
	XtAddCallback (fn_w, XmNactivateCallback, saveCB, NULL);
	XtManageChild (fn_w);

	/* make the template prompt and text field */

	n = 0;
	l_w = XmCreateLabel (rc_w, "TL", args, n);
	wlprintf (l_w, "Template:");
	XtManageChild (l_w);

	n = 0;
	XtSetArg (args[n], XmNmaxLength, MAXCOLS); n++;
	XtSetArg (args[n], XmNcolumns, 30); n++;
	tpl_w = XmCreateTextField (rc_w, "Template", args, n);
	XtAddCallback (tpl_w, XmNactivateCallback, saveCB, NULL);
	XtManageChild (tpl_w);

	/* center the autosave TB */

	n = 0;
	f_w = XmCreateForm (rc_w, "ASF", args, n);
	XtManageChild (f_w);

	    n = 0;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 30); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 70); n++;
	    auto_w = XmCreateToggleButton (f_w, "AS", args, n);
	    wlprintf (auto_w, "Auto save");
	    XtManageChild (auto_w);

	/* evenly space the control buttons */

	n = 0;
	f_w = XmCreateForm (rc_w, "ASF", args, n);
	XtManageChild (f_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 10); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 30); n++;
	    XtSetArg (args[n], XmNshowAsDefault, True); n++;
	    w = XmCreatePushButton (f_w, "Save", args, n);
	    XtAddCallback (w, XmNactivateCallback, saveCB, NULL);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNtopOffset, 5); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomOffset, 5); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 40); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 60); n++;
	    w = XmCreatePushButton (f_w, "Scan", args, n);
	    XtAddCallback (w, XmNactivateCallback, scanCB, NULL);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNtopOffset, 5); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomOffset, 5); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 70); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 90); n++;
	    w = XmCreatePushButton (f_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, closeCB, NULL);
	    XtManageChild (w);
}

/* called when the save button is activated
 * or if RETURN is hit from any of the text fields.
 * N.B. do not use call -- this is used by more than one class.
 */
/* ARGSUSED */
static void
saveCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (state.fimage.image) {
	    if (getSaveName(state.fname) == 0) {
		writeImage();
		showHeader();
	    }
	} else
	    msg ("No image");
}

/* called when the scan button is activated.
 */
/* ARGSUSED */
static void
scanCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char buf[2*MAXCOLS];

	if (mkTemplateName (buf) == 0) {
	    XmTextFieldSetString (fn_w, basenm (buf));
	    msg ("");
	}
}

/* called when the close button is activated.
 */
/* ARGSUSED */
static void
closeCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (save_w);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: save.c,v $ $Date: 2001/04/19 21:12:00 $ $Revision: 1.1.1.1 $ $Name:  $"};
