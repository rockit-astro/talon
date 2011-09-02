/* code to create and manage the dialog that allows deleting scans by
 * schedule file and/or cataloge name.
 */

#include <stdio.h>
#include <stdlib.h>

#include <Xm/List.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "xtools.h"
#include "configfile.h"
#include "scan.h"

#include "telsched.h"

static void createDeleteSched(void);
static void del_cb (Widget w, XtPointer client, XtPointer call);
static void delall_cb (Widget w, XtPointer client, XtPointer call);
static void close_cb (Widget w, XtPointer client, XtPointer call);
static void fill_list(void);
static int itemcmp (const void *p1, const void *p2);

static Widget delfd_w;	/* the overall form dialog */
static Widget l_w;	/* list of schedule files */

static char **litems;	/* malloced list of malloced unique schedule filenames*/
static int nlitems;	/* number of entries in litems[] */

void
manageDeleteSchedMenu()
{
	if (!delfd_w)
	    createDeleteSched();

	if (XtIsManaged(delfd_w))
	    XtUnmanageChild (delfd_w);
	else {
	    fill_list();
	    XtManageChild (delfd_w);
	}
}

void
updateDeleteSchedMenu()
{
	if (!delfd_w || !XtIsManaged(delfd_w))
	    return;
	fill_list();
}

static void 
createDeleteSched()
{
	Widget f_w;
	Widget w;
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 10); n++;
	delfd_w = XmCreateFormDialog (toplevel_w, "DSFD", args, n);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Loaded File Deletion"); n++;
	XtSetValues (XtParent(delfd_w), args, n);

	/* make the bottom control buttons in a form.
	 * N.B. build from bottom up so scrolled list grows to total height
	 */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNfractionBase, 16); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	f_w = XmCreateForm (delfd_w, "CtrlF", args, n);
	XtManageChild (f_w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 1); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 5); n++;
	    w = XmCreatePushButton (f_w, "Delete", args, n);
	    XtAddCallback (w, XmNactivateCallback, del_cb, NULL);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 6); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 10); n++;
	    w = XmCreatePushButton (f_w, "Delete", args, n);
	    wlprintf (w, "Delete all");
	    XtAddCallback (w, XmNactivateCallback, delall_cb, NULL);
	    XtManageChild (w);

	    n = 0;
	    XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	    XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNleftPosition, 11); n++;
	    XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	    XtSetArg (args[n], XmNrightPosition, 15); n++;
	    w = XmCreatePushButton (f_w, "Close", args, n);
	    XtAddCallback (w, XmNactivateCallback, close_cb, NULL);
	    XtManageChild (w);

	/* the scrolled list */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, f_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNheight, 150); n++;
	l_w = XmCreateScrolledList (delfd_w, "DSSL", args, n);
	XtAddCallback (l_w, XmNdefaultActionCallback, del_cb, NULL);
	XtManageChild (l_w);
}

/* called whenever the Close button is pressed.
 */
/* ARGSUSED */
static void
close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (delfd_w);
}

/* called whenever the Delete button is pressed or an item is double-clicked
 * in the list.
 * N.B. we are called from two different classes of widgets -- don't use call.
 */
/* ARGSUSED */
static void
del_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int *pos = NULL;
	int newmem;
	int npos;
	int i;


	newmem = XmListGetSelectedPos (l_w, &pos, &npos);
	if (!newmem) {
	    msg ("No selection");
	    return;
	}

	/* should only ever be one */
	for (i = 0; i < npos; i++) {
	    /* remember the pos entries are 1-based */
	    if (pos[i] < 1 || pos[i] > nlitems) {
		fprintf (stderr, "DelSched list item out of range\n");
		return;
	    }
	    deleteSchedEntries (litems[pos[i]-1]);
	    /*
	    printf ("delete %d: %s\n", pos[i], litems[pos[i]-1]);
	    */
	}

	if (pos)
	    XtFree ((char *)pos);

	fill_list();
}

/* delete all items */
/* ARGSUSED */
static void
delall_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	deleteAllSchedEntries ();
	fill_list();
}

/* (re)fill litems[] and l_w with a list of sorted unique schedfn entrires from
 * the current list of Obs.
 */
static void
fill_list()
{
	Obs *obs;
	int nobs;
	int i;

	/* clear out old stuff first */
	if (litems) {
	    for (i = 0; i < nlitems; i++)
		XtFree (litems[i]);
	    XtFree ((char *)litems);
	    litems = NULL;
	    nlitems = 0;
	}

	get_obsarray (&obs, &nobs);

	/* scan each obs->schedfn and add to litems if not already there */
	for (i = 0; i < nobs; i++) {
	    Obs *op = obs+i;
	    int j;

	    for (j = 0; j < nlitems; j++)
		if (strcmp (op->scan.schedfn, litems[j]) == 0)
		    break;
	    if (j < nlitems)
		continue;

	    litems = (char **) XtRealloc
				((char *)litems, (nlitems+1)*sizeof(char *));
	    litems[nlitems] = XtMalloc (sizeof (op->scan.schedfn));
	    strcpy (litems[nlitems], op->scan.schedfn);
	    nlitems++;
	}

	/* sort */
	if (nlitems > 0)
	    qsort ((char *)litems, nlitems, sizeof(char *), itemcmp);

	/* fill l_w */
	XtUnmanageChild (l_w);
	XmListDeleteAllItems (l_w);
	for (i = 0; i < nlitems; i++) {
	    XmString str;
	    str = XmStringCreateLtoR (litems[i], XmSTRING_DEFAULT_CHARSET);
	    XmListAddItem (l_w, str, 0);	/* keep appending to list */
	    XmStringFree (str);
	}
	XtManageChild (l_w);
}

static int
itemcmp (p1, p2)
const void *p1;
const void *p2;
{
	return (strcmp (*(char **)p1, *(char **)p2));
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: delsched.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
