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

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: dome.c,v $ $Date: 2007/02/25 23:31:22 $ $Revision: 1.2 $ $Name:  $"};
