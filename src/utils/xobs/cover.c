/* handle the cover controls */

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

void coverOpenCB(Widget w, XtPointer client, XtPointer call)
{
    if (!rusure(toplevel_w, "open the mirror cover"))
        return;

    fifoMsg(Cover_Id, "coverOpen");
    msg("Opening mirror cover");
}

void coverCloseCB(Widget w, XtPointer client, XtPointer call)
{
    if (!rusure(toplevel_w, "close the mirror cover"))
        return;

    msg("Closing mirror cover");
    fifoMsg(Cover_Id, "coverClose");
}
