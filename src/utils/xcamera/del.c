/* dialog to allow deleting the current image from disk */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <string.h>

#include <Xm/Xm.h>
#include <Xm/MessageB.h>

#include "xtools.h"
#include "fits.h"
#include "fieldstar.h"
#include "strops.h"
#include "camera.h"

static void createDel(void);
static int updateDel(void);
static void delCB (Widget w, XtPointer client, XtPointer call);

static Widget del_w;	/* main save dialog */

void
manageDel ()
{
	if (!del_w)
	    createDel();

	if (XtIsManaged(del_w))
	    raiseShell (del_w);
	else {
	    if (updateDel() == 0)
		XtManageChild (del_w);
	}
}

static void
createDel()
{
	Arg args[20];
	int n;

	/* create message box and hook up the ok button */

	n = 0;
	XtSetArg (args[n], XmNtitle, "File Delete"); n++;
	XtSetArg (args[n], XmNdialogType, XmDIALOG_APPLICATION_MODAL); n++;
	del_w = XmCreateQuestionDialog (toplevel_w, "Del", args,n);
	XtAddCallback (del_w, XmNokCallback, delCB, NULL);
	XtUnmanageChild (XmMessageBoxGetChild (del_w, XmDIALOG_HELP_BUTTON));
}

/* put state.fname into messageString of del_w dialog.
 * return 0 if ok else -1.
 */
static int
updateDel()
{
	if (!del_w)
	    createDel();

	if (state.fname[0]) {
	    char buf[2048];

	    (void) sprintf (buf, "Delete %s?", basenm(state.fname));
	    set_xmstring (del_w, XmNmessageString, buf);
	    return (0);
	} else {
	    msg ("No file open.");
	    return (-1);
	}
}

/* called when the Ok button is activated.
 */
/* ARGSUSED */
static void
delCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char *base = basenm (state.fname);

	if (remove (state.fname) < 0)
	    msg ("%s: %s", base, strerror (errno));
	else {
	    delHistory (base);
	    msg ("%s deleted", base);
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: del.c,v $ $Date: 2001/04/19 21:11:59 $ $Revision: 1.1.1.1 $ $Name:  $"};
