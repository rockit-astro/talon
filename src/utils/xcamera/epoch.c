/* code to handle setting and using the desired display epoch */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/SelectioB.h>
#include <Xm/Form.h>
#include <Xm/ToggleB.h>
#include <Xm/PushB.h>

#include "P_.h"
#include "astro.h"
#include "xtools.h"
#include "fits.h"
#include "fieldstar.h"
#include "camera.h"

static double epoch_mjd;		/* desired display epoch, as an mjd */
static double epoch_year;		/* desired display epoch, as a year */

static Widget epoch_w;			/* main dialog */

static void set_string(void);
static void okCB (Widget w, XtPointer client, XtPointer call);

void
manageEpoch ()
{
	if (!epoch_w)
	    createEpoch();

	if (XtIsManaged(epoch_w))
	    raiseShell (epoch_w);
	else {
	    set_string();
	    XtManageChild (epoch_w);
	    msg ("");
	}
}

void
createEpoch()
{
	XmString xs;
	String s;
	Arg args[20];
	int n;

	xs = XmStringCreateSimple ("Enter precession equinox:");

	n = 0;
	XtSetArg (args[n], XmNselectionLabelString, xs); n++;
	epoch_w = XmCreatePromptDialog (toplevel_w, "Equinox", args, n);
	XmStringFree (xs);
	XtAddCallback (epoch_w, XmNokCallback, okCB, NULL);
	XtUnmanageChild (XmSelectionBoxGetChild(epoch_w, XmDIALOG_HELP_BUTTON));

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	  */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Equinox Setup"); n++;
	XtSetValues (XtParent(epoch_w), args, n);

	/* get default value */
	get_xmstring (epoch_w, XmNtextString, &s);
	epoch_year = atof (s);
	XtFree (s);

	/* convert the decimal year to mjd */
	year_mjd (epoch_year, &epoch_mjd);

}

/* return the current precession epoch as a year */
double
pEyear()
{
	return (epoch_year);
}

/* return the current precession epoch as an mjd */
double
pEmjd()
{
	return (epoch_mjd);
}

/* precess the given coordinates, which are epoch 2000, to the current
 * epoch, and return the new values IN PLACE.
 * both args are in radians.
 */
void
p2000toE (double *rap, double *decp)
{
	if (epoch_mjd != J2000)
	    precess (J2000, epoch_mjd, rap, decp);
}

/* set the prompt string to match epoch_mjd */
static void
set_string()
{
	char ystr[64];

	(void) sprintf (ystr, "%g", epoch_year);
	set_xmstring (epoch_w, XmNtextString, ystr);
}

static void
okCB (Widget w, XtPointer client, XtPointer call)
{
	double year;
	String s;

	get_xmstring (w, XmNtextString, &s);
	year = atof (s);
	XtFree (s);


	/* sanity check */
	if (year < 1800 || year > 2100)
	    msg ("Epoch sanity checked failed for %g", year);
	else {
	    year_mjd (year, &epoch_mjd);
	    epoch_year = year;
	    msg ("Epoch set to %g", year);
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: epoch.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $"};
