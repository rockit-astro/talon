/* handle the dome controls */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include <X11/keysym.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/Xm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "misc.h"
#include "strops.h"
#include "telstatshm.h"
#include "xtools.h"

#include "widgets.h"
#include "xobs.h"

void domeOpenCB(Widget w, XtPointer client, XtPointer call)
{
    if (!rusure(toplevel_w, "open the roof"))
        return;

    fifoMsg(Dome_Id, "open");
    msg("Opening dome");
}

void domeCloseCB(Widget w, XtPointer client, XtPointer call)
{
    if (!rusure(toplevel_w, "close the roof"))
        return;

    msg("Closing dome");
    fifoMsg(Dome_Id, "close");
}
