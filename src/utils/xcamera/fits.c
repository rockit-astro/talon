/* code to display the FITS header of the current image.
 * and a place to capture additional COMMENTs by the user.
 */

#include <stdio.h>
#include <stdlib.h>

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>
#include <Xm/ToggleB.h>
#include <Xm/PushB.h>
#include <Xm/Text.h>
#include <Xm/Label.h>
#include <Xm/PanedW.h>

#include "xtools.h"
#include "fits.h"
#include "fieldstar.h"
#include "camera.h"

static void createFITS(void);
static void addCommentsCB (Widget w, XtPointer client, XtPointer call);
static void closeCB (Widget w, XtPointer client, XtPointer call);

static Widget fits_w;	/* main FITS test dialog */
static Widget rotf_w;	/* the read-only text widget for existing header lines*/
static Widget rwtf_w;	/* the read-write text widget for new comments */

void
manageFITS ()
{
	if (!fits_w)
	    createFITS();

	if (XtIsManaged(fits_w))
	    raiseShell (fits_w);
	else
	    XtManageChild (fits_w);
}

/* show the FITS header info for the current image
 * and reset the user comments.
 */
void
updateFITS()
{ 
	char *header;
	int i;

	if (!fits_w)
	    createFITS();

	/* room for each FITS line, with nl and a final \0 */
	header = malloc (state.fimage.nvar*(FITS_HCOLS+1) + 1);
	if (!header) {
	    msg ("No memory to display FITS header");
	    return;
	}

	/* copy from fimage.var to header, adding \n after each line */
	for (i = 0; i < state.fimage.nvar; i++) {
	    memcpy(header + i*(FITS_HCOLS+1), state.fimage.var[i], FITS_HCOLS);
	    header[(i+1)*(FITS_HCOLS+1)-1] = '\n';
	}
	header[state.fimage.nvar*(FITS_HCOLS+1)] = '\0';  /* add final \0 */

	XmTextSetString (rotf_w, header);
	free (header);

	XmTextSetString (rwtf_w, "");
}

/* compute new FWHM fields in header and update the title */
void
setFWHMCB (Widget w, XtPointer client, XtPointer call)
{
	FImage *fip = &state.fimage;
	char buf[1024];
	int s;

	if (!fip->image) {
	    msg ("No image");
	    return;
	}

	watch_cursor (1);
	s = setFWHMFITS (fip, buf);
	watch_cursor (0);
	
	if (s < 0) {
	    msg ("FWHM failed: %s", buf);
	    return;
	}

	msg ("");

	updateFITS();
	showHeader();
}

static void
createFITS()
{
	Arg args[20];
	Widget addc_w, close_w;
	Widget pf_w;
	Widget pw_w;
	Widget w;
	int n;

	/* create form */
	
	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False); n++;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	XtSetArg (args[n], XmNcolormap, camcm); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 10); n++;
	fits_w = XmCreateFormDialog (toplevel_w, "FITS", args, n);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "FITS Header"); n++;
	XtSetValues (XtParent(fits_w), args, n);
	XtVaSetValues (fits_w, XmNcolormap, camcm, NULL);

	/* Add and Close button across the bottom */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 20); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 40); n++;
	addc_w = XmCreatePushButton (fits_w, "Add", args, n);
	XtAddCallback (addc_w, XmNactivateCallback, addCommentsCB, NULL);
	wlprintf (addc_w, "Add COMMENTs");
	XtManageChild (addc_w);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomOffset, 10); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 60); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 80); n++;
	close_w = XmCreatePushButton (fits_w, "Close", args, n);
	XtAddCallback (close_w, XmNactivateCallback, closeCB, NULL);
	XtManageChild (close_w);

	/* make a paned window to hold the two scrolled text areas */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, close_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	pw_w = XmCreatePanedWindow (fits_w, "PW", args, n);
	XtManageChild (pw_w);

	    /* make read-only scrolled text for the existing header fields */

	    n = 0;
	    XtSetArg (args[n], XmNautoShowCursorPosition, False); n++;
	    XtSetArg (args[n], XmNeditable, False); n++;
	    XtSetArg (args[n], XmNcursorPositionVisible, False); n++;
	    XtSetArg (args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	    rotf_w = XmCreateScrolledText (pw_w, "RO", args, n);
	    XtManageChild (rotf_w);

	    /* make form to hold a title label and the r/w text field
	     * to use for entering comments.
	     */

	    n = 0;
	    XtSetArg (args[n], XmNpaneMaximum, 100); n++;
	    pf_w = XmCreateForm (pw_w, "PF", args, n);
	    XtManageChild (pf_w);

		/* comments entry title */

		n = 0;
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
		w = XmCreateLabel (pf_w, "CL", args, n);
		XtManageChild (w);
		wlprintf (w, "New COMMENTs:");

		/*  comments entry text field */

		n = 0;
		XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
		XtSetArg (args[n], XmNtopWidget, w); n++;
		XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
		XtSetArg (args[n], XmNautoShowCursorPosition, True); n++;
		XtSetArg (args[n], XmNeditable, True); n++;
		XtSetArg (args[n], XmNcursorPositionVisible, True); n++;
		XtSetArg (args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
		XtSetArg (args[n], XmNwordWrap, True); n++;
		rwtf_w = XmCreateScrolledText (pf_w, "RW", args, n);
		XtManageChild (rwtf_w);
}

/* called when the Add button is activated.
 */
/* ARGSUSED */
static void
addCommentsCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char *comments;
	int l;

	comments = XmTextGetString (rwtf_w);	/* _copy_ of text string */
	l = strlen (comments);
	if (l > 0) {
	    char row[sizeof(FITSRow)+1];	/* we add \0 */
	    char c, *rowp;
	    int w;

	    /* copy the XmText stuff as COMMENT fields in the FITS file.
	     * we break into lines at newline or if longer than 72 chars.
	     * we also skip any empty lines.
	     */
	    w = 0;
	    rowp = comments;
	    do {
		c = row[w++] = *rowp++;
		if (w == 72 || c == '\0' || c == '\n') {
		    if (c == '\n')
			row[w-1] = '\0';
		    else if (w == 72)
			row[w] = '\0';
		    if ((int)strlen(row) > 0)
			setCommentFITS (&state.fimage, "COMMENT", row);
		    w = 0;
		}
	    } while (c);
	}

	XtFree (comments);

	/* show the new resulting header and erase the comments just added */
	updateFITS();
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
	XtUnmanageChild (fits_w);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: fits.c,v $ $Date: 2001/04/19 21:11:59 $ $Revision: 1.1.1.1 $ $Name:  $"};
