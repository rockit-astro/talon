/* create a dialog which allows selecting an object from any .edb file.
 */

#include <stdio.h>
#include <stdlib.h>

#include <Xm/Form.h>
#include <Xm/FileSB.h>
#include <Xm/List.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telenv.h"
#include "xtools.h"
#include "telenv.h"
#include "catbrowse.h"

static Widget toplevel_w;	/* the dialog box parent */
static Widget main_w;		/* the overall dialog box */
static Widget fsb_w;		/* the file selection box */
static Widget list_w;		/* the scrolled list */
static CatBrowseCB callback;	/* user's function to call when make selection*/
static char filename[512];	/* user's selected filename */
static char edbentry[256];	/* static storage returned via callback */

static void create_main_w(void);

/* put up a dialog to allow the user to browse for .edb files and select
 * an entry. when they finally do, call the passed callback.
 * N.B. the string passed back via cbp is static -- the caller should copy it
 */
void
catalogBrowse (Widget top_w, CatBrowseCB cbp)
{
	toplevel_w = top_w;
	callback = cbp;

	if (!main_w)
	    create_main_w();

	if (XtIsManaged(main_w))
	    XtUnmanageChild(main_w);
	else
	    XtManageChild (main_w);
}

/* called to read the given .edb file and add names of valid entries to list_w
 */
static void
list_edb (FILE *fp)
{
	XmString *names = NULL;
	int nnames = 0;
	char buf[1024];
	Obj o;

	while (fgets (buf, sizeof(buf), fp) != NULL) {
	    buf[strlen(buf)-1] = '\0';
	    if (db_crack_line (buf, &o, NULL) == 0) {
		XmString name = XmStringCreateSimple (o.o_name);
		names = (XmString *) XtRealloc ((void *)names,
						(nnames+1)*sizeof(XmString));
		names[nnames++] = name;
	    }
	}

	/* _much_ faster to load as an array than individually */
	XmListDeleteAllItems (list_w);
	XmListAddItems (list_w, names, nnames, 0);

	while (--nnames >= 0)
	    XmStringFree (names[nnames]);
	XtFree ((void *)names);
}

/* called when user selects a db entry
 */
static void
list_select_cb (Widget w, XtPointer client, XtPointer data)
{
	XmListCallbackStruct *lp = (XmListCallbackStruct *)data;
	String selection;
	char buf[1024];
	FILE *fp;
	Obj o;

	/* insure single-selection mode */
	if (lp->reason != XmCR_DEFAULT_ACTION)
	    return;

	/* open (last) selected filename */
	fp = telfopen (filename, "r");
	if (!fp)
	    return;

	/* get selection */
	XmStringGetLtoR (lp->item, XmSTRING_DEFAULT_CHARSET, &selection);

	/* find in file */
	edbentry[0] = '\0';
	while (fgets (buf, sizeof(buf), fp) != NULL) {
	    buf[strlen(buf)-1] = '\0';
	    if (db_crack_line (buf, &o, NULL) == 0) {
		if (strcmp (o.o_name, selection) == 0) {
		    (void) strcpy (edbentry, buf);
		    break;
		}
	    }
	}

	/* finished with selection and file */
	XtFree ((void *)selection);
	fclose (fp);

	/* invoke user's callback with entry */
	if (edbentry[0] != '\0')
	    (*callback) (edbentry);
}

/* called when user selects a file */
static void
fsb_ok_cb (Widget w, XtPointer client, XtPointer data)
{
	XmFileSelectionBoxCallbackStruct *s =
				    (XmFileSelectionBoxCallbackStruct *)data;
	char *sp;
	FILE *fp;

	if (s->reason != XmCR_OK)
	    return;

	XmStringGetLtoR (s->value, XmSTRING_DEFAULT_CHARSET, &sp);
	(void) strcpy (filename, sp);
	XtFree (sp);

	fp = telfopen (filename, "r");
	if (fp)
	    list_edb (fp);
	fclose (fp);
}

/* called when user cancels out */
static void
fsb_cancel_cb (Widget w, XtPointer client, XtPointer data)
{
	XtUnmanageChild (main_w);
}

/* create the main form dialog */
static void
create_main_w()
{
	Widget w;
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNtitle, "Catalog Browser"); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_GROW); n++;
	main_w = XmCreateFormDialog (toplevel_w, "CatBrowse", args, n);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	fsb_w = XmCreateFileSelectionBox (main_w, "FSB", args, n);
	XtAddCallback (fsb_w, XmNokCallback, fsb_ok_cb, NULL);
	XtAddCallback (fsb_w, XmNcancelCallback, fsb_cancel_cb, NULL);
	set_xmstring (fsb_w, XmNcancelLabelString, "Close"); n++;
	XtManageChild (fsb_w);

	/* don't need Help */
	w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_HELP_BUTTON);
	XtUnmanageChild (w);

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, fsb_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNvisibleItemCount, 10); n++;
	XtSetArg (args[n], XmNselectionPolicy, XmSINGLE_SELECT); n++;
	list_w = XmCreateScrolledList (main_w, "SL", args, n);
	XtAddCallback (list_w, XmNdefaultActionCallback, list_select_cb,NULL);
	XtManageChild (list_w);
}

#ifdef TESTIT

#include <Xm/PushB.h>

static void
newdb_cb (char *edb_entry)
{
	printf ("Selection: %s\n", edb_entry);
}

static void
browse_cb (Widget w, XtPointer client, XtPointer data)
{
	catalogBrowse (w, newdb_cb);
}

int
main (int ac, char *av[])
{

	XtAppContext app;
	Widget top_w, pb_w;

	top_w = XtAppInitialize (&app,"CatBrowse",NULL,0,&ac,av,NULL,NULL,0);

	pb_w = XmCreatePushButton (top_w, "Browse", NULL, 0);
	XtAddCallback (pb_w, XmNactivateCallback, browse_cb, NULL);
	XtManageChild (pb_w);

	XtRealizeWidget (top_w);
	XtAppMainLoop (app);

	return (0);
}
#endif /* TESTIT */

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: catbrowse.c,v $ $Date: 2001/04/19 21:12:13 $ $Revision: 1.1.1.1 $ $Name:  $"};
