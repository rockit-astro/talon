/* handle the dome controls */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>

#include <Xm/Xm.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <X11/keysym.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "strops.h"
#include "misc.h"
#include "configfile.h"
#include "telstatshm.h"
#include "xtools.h"

#include "xobs.h"
#include "widgets.h"

void
domeOpenCB (Widget w, XtPointer client, XtPointer call)
{
	if (!rusure (toplevel_w, "open the roof"))
	    return;

	fifoMsg (Dome_Id, "open");
	msg ("Opening dome");
}

void
domeCloseCB (Widget w, XtPointer client, XtPointer call)
{
	if (!rusure (toplevel_w, "close the roof"))
	    return;

	msg ("Closing dome");
	fifoMsg (Dome_Id, "close");
}

void
domeAutoCB (Widget w, XtPointer client, XtPointer call)
{
	int set = XmToggleButtonGetState(w);
	char *str;

	if (set)
	    str = "slave dome azimuth to telescope";
	else
	    str = "unlock dome azimuth from telescope";

	if (!rusure (toplevel_w, str)) {
	    XmToggleButtonSetState (w, !set, False);
	    return;
	}

	fifoMsg (Dome_Id, set ? "auto" : "off");

	XmToggleButtonSetState (w, set, False);
}

void
domeGotoCB (Widget w, XtPointer client, XtPointer call)
{
	int shst;
	char buf[1024];
	double newaz;
	char *str;

	str = XmTextFieldGetString (g_w[DAZ_W]);
	newaz = atof (str);
	XtFree (str);

	(void) sprintf (buf, "set the dome azimuth to %g%c", newaz, XK_degree);
	shst = telstatshmp->shutterstate;
	if (shst == SH_OPENING)
	    strcat (buf, "\nwhile the shutter is opening");
	else if (shst == SH_CLOSING)
	    strcat (buf, "\nwhile the shutter is closing");
	else if (telstatshmp->autodome)
	    strcat (buf, "\nand deactivate Auto mode");

	if (!rusure (toplevel_w, buf))
	    return;

	msg ("Starting dome rotation to %g", newaz);
	fifoMsg (Dome_Id, "Az:%g", degrad(newaz));
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: dome.c,v $ $Date: 2007/02/25 23:31:22 $ $Revision: 1.2 $ $Name:  $"};
