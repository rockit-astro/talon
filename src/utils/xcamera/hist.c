/* code to manage the history of open files on the File pulldown menu.
 * the list is a pre-allocated array of MAXHIST PushButtons, managed when
 * they are needed. The list remains contiguous, used from indeces 0..nhist-1.
 * the label of the PB is just the basename; the full pathname to the file is
 * a malloced string stored in userData.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#ifdef DO_EDITRES
#include <X11/Xmu/Editres.h>
#endif

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/ScrolledW.h>
#include <Xm/Label.h>
#include <Xm/Form.h>
#include <Xm/ToggleB.h>
#include <Xm/CascadeB.h>
#include <Xm/PushB.h>
#include <Xm/MessageB.h>
#include <Xm/Separator.h>

#include "P_.h"
#include "astro.h"
#include "strops.h"
#include "xtools.h"
#include "fits.h"
#include "fieldstar.h"
#include "camera.h"

static void histCB (Widget w, XtPointer client, XtPointer call);

#define	MAXHIST	10		/* max files in history list */
static Widget histw[MAXHIST];	/* file history widgets */
static int nhist;		/* number of histw in use */

/* add the widgets for the history portion of the file pulldown */
void
createHistoryMenu (Widget pdm)
{
	Widget w;
	Arg args[20];
	int i;
	int n;

	/* start with the separator */
	n = 0;
	XtSetArg (args[n], XmNseparatorType, XmDOUBLE_LINE); n++;
	w = XmCreateSeparator (pdm, "HS", args, n);
	XtManageChild (w);

	/* make each of the pbs, unmanaged for now */
	for (i = 0; i < MAXHIST; i++) {
	    n = 0;
	    w = XmCreatePushButton (pdm, "HPB", args, n);
	    XtAddCallback (w, XmNactivateCallback, histCB, 0);
	    histw[i] = w;
	}
}

/* add full to hist list, unless already there */
void
addHistory (char *full)
{
	char *base, *name;
	int i;

	if (full[0] == '\0')
	    return;

	base = basenm (full);

	/* see if already on list */
	for (i = 0; i < nhist; i++) {
	    get_something (histw[i], XmNuserData, (char *)&name);
	    if (strcmp (basenm(name), base) == 0)
		return;
	}

	/* push down, discard last item if full */
	for (i = nhist; i > 0; --i) {
	    if (i == MAXHIST) {
		get_something (histw[i-1], XmNuserData, (char *)&name);
		XtFree (name);
		continue;
	    }
	    get_something (histw[i-1], XmNuserData, (char *)&name);
	    set_something (histw[i], XmNuserData, name);
	    get_xmstring (histw[i-1], XmNlabelString, &name);
	    set_xmstring (histw[i], XmNlabelString, name);
	    XtFree (name);
	    XtManageChild (histw[i]);
	}

	/* store complete pathname */
	if (full[0] == '/')
	    name = XtNewString (full);
	else {
	    char pwd[2048], fullest[2048];
	    (void) getcwd (pwd, sizeof(pwd));
	    (void) sprintf (fullest, "%s/%s", pwd, full);
	    name = XtNewString (fullest);
	}

	/* add new as top entry */
	set_something (histw[0], XmNuserData, name);
	set_xmstring (histw[0], XmNlabelString, base);
	XtManageChild (histw[0]);
	if (nhist < MAXHIST)
	    nhist++;
}

/* delete the given entry from the history list. base is just the basename.
 */
void
delHistory (char *base)
{
	char *name;
	int i;

	/* find on list */
	for (i = 0; i < nhist; i++) {
	    int yes;
	    get_xmstring (histw[i], XmNlabelString, &name);
	    yes = strcmp (name, base) == 0;
	    XtFree (name);
	    if (yes)
		break;
	}
	if (i == nhist)
	    return;	/* oh well */

	/* delete memory saved by item i */
	get_something (histw[i], XmNuserData, (char *)&name);
	XtFree (name);
	
	/* push up to fill the gap left by i */
	while (++i < nhist) {
	    get_something (histw[i], XmNuserData, (char *)&name);
	    set_something (histw[i-1], XmNuserData, name);
	    get_xmstring (histw[i], XmNlabelString, &name);
	    set_xmstring (histw[i-1], XmNlabelString, name);
	    XtFree (name);
	}

	/* decrement total and turn off the bottom one */
	XtUnmanageChild (histw[--nhist]);
}

/* called when a history file is selected */
static void
histCB (Widget w, XtPointer client, XtPointer call)
{
	char *full;

	get_something (w, XmNuserData, (char *)&full);
	openFile (full);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: hist.c,v $ $Date: 2001/04/19 21:11:59 $ $Revision: 1.1.1.1 $ $Name:  $"};
