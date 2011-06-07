/* connect xephem to the the telescope daemon.
 * the basic tasks are connecting the right fifos together and giving the user
 * feedback based on daemon responses.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Form.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telstatshm.h"
#include "configfile.h"
#include "xtools.h"
#include "strops.h"
#include "telenv.h"
#include "xobs.h"

extern Widget toplevel_w;

static void xephemCB (XtPointer client, int *fd, XtInputId *id);
static void inFifoCB (XtPointer client, XtIntervalId *id);

static char locfifo[1024];
static int locfd = -1;
static char infifo[1024];
static int infd = -1;

#define	XE_INPER	333	/* interval between checking infifo, ms */

/* one-time set-up to monitor xephem */
void
initXEphem()
{
	char *path;

	path = getXRes (toplevel_w,"LocFIFO","xephem/fifos/xephem_loc_fifo");
	telfixpath (locfifo, path);

	path = getXRes (toplevel_w,"InFIFO","xephem/fifos/xephem_in_fifo");
	telfixpath (infifo, path);

	/* start infifo-push polling */
	XtAppAddTimeOut (app, XE_INPER, inFifoCB, 0);
}

/* input ready from xephem loc fifo.
 * might be for tracking or for axis calibration.
 */
static void
xephemCB (XtPointer client, int *fd, XtInputId *id)
{
	char nm[32], ty[32], ra[32], dec[32];
	char buf[1024];
	int r;

	/* read object */
	r = read (*fd, buf, sizeof(buf));
	if (r <= 0) {
	    msg ("Disconnecting from %s", basenm(locfifo));
	    (void) close (*fd);
	    locfd = -1;
	    XtRemoveInput (*id);
	    return;
	}
	if (buf[r-1] != '\0')
	    buf[r] = '\0';

	/* let axis have a look and have first chance to say no */
	if (axes_xephemSet (buf) < 0)
	    return;

	/* start tracking it */
	fifoMsg (Tel_Id, "%s", buf);

	/* comfort the user */
	(void) strcpy (nm, "?");
	(void) strcpy (ty, "?");
	(void) strcpy (ra, "?");
	(void) strcpy (dec, "?");
	(void) sscanf (buf, "%[^,],%[^,],%[^,],%[^,]", nm, ty, ra, dec);
	if (strcmp (nm, "TelAnon") == 0)
	    (void) sprintf (buf, "%s %s", ra, dec);
	else
	    (void) strcpy (buf, nm);
	msg ("Hunting for %s ...", buf);
}

/* periodic xephem support.
 * 1) if not already going ok, connect to locfifo.
 * 2) if known, send scope coords to infifo
 */
static void
inFifoCB (XtPointer client, XtIntervalId *id)
{
	/* try to (re)connect to loc fifo */
	if (locfd < 0) {
	    locfd = open (locfifo, O_RDWR|O_NONBLOCK);
	    if (locfd >= 0)
		XtAppAddInput(app, locfd, (XtPointer)XtInputReadMask,
								xephemCB, 0);
	}

	/* now send current coords to infifo, if known */
	switch (telstatshmp->telstate) {
	case TS_HUNTING:	/* FALLTHRU */
	case TS_SLEWING:	/* FALLTHRU */
	case TS_TRACKING:	/* FALLTHRU */
	case TS_STOPPED:	/* FALLTHRU */
	case TS_LIMITING:

	    if (infd < 0)
		infd = open (infifo, O_WRONLY|O_NONBLOCK);
	    if (infd >= 0) {
		char buf[1024];
		int n = sprintf (buf, "RA:%.6f Dec:%.6f Epoch:2000\n",
				telstatshmp->CJ2kRA, telstatshmp->CJ2kDec);
		if (write (infd, buf, n) != n) {
		    /* try again next time */
		    close (infd);
		    infd = -1;
		}
	    }

	    break;

	default:
	    break;
	}

	/* repeat */
	XtAppAddTimeOut (app, XE_INPER, inFifoCB, 0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: xephem.c,v $ $Date: 2001/04/19 21:12:06 $ $Revision: 1.1.1.1 $ $Name:  $"};
