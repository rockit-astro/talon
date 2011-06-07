/* handle powerfail/ok messages
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "strops.h"
#include "telstatshm.h"
#include "running.h"
#include "rot.h"
#include "misc.h"
#include "telenv.h"
#include "csimc.h"
#include "cliserv.h"
#include "tts.h"

#include "teled.h"

// replaced -- new stow command in tel.c
//double STOWALT, STOWAZ;

static int pwrfail;			/* 1 while in progress */

/* called when we receive a message from the Power fifo.
 */
/* ARGSUSED */
void
power_msg (msg)
char *msg;
{
    if (!msg)
        return;

    if (strncasecmp (msg, "powerfail", 9) == 0) {
        //char buf[128];

        tdlog ("Power failure.. closing roof and stowing scope.");
        allstop();
        dome_msg ("Close");
        /* Replaced -- new STOW command
        sprintf (buf, "Alt:%g Az:%g", STOWALT, STOWAZ);
        tel_msg (buf);
        */
        tel_msg("Stow");
        pwrfail = 1;
        fifoWrite (Power_Id, 0, "Power fail response underway");
        toTTS ("A power failure has occured.");
    } else if (strncasecmp (msg, "powerok", 7) == 0) {
        tdlog ("Power restored.. resuming normal operation.");
        allstop();
        pwrfail = 0;
        fifoWrite (Power_Id, 0, "Power resumes.");
        toTTS ("Power has been restored..");
    } else {
        fifoWrite (Power_Id, -1, "Bogus Power message: %s", msg);
    }
}

/* return 0 if powerfail is in progress, else -1 for normal operation */
int
chkPowerfail(void)
{
    return (pwrfail ? 0 : -1);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: powerfail.c,v $ $Date: 2002/10/23 21:44:13 $ $Revision: 1.2 $ $Name:  $"};
