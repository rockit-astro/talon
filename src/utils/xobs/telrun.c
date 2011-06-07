/* code to handle managing the telrun batch process */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <Xm/Xm.h>	/* for xtel.h */

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telstatshm.h"
#include "telenv.h"
#include "configfile.h"
#include "running.h"
#include "xobs.h"

#define	POLLSECS	5		/* secs to poll for telrun to start */
#define	LOGPOLL		1000		/* telrun.log polling period, ms */

static char telrun[] = "telrun";
static char logfn[] = "archive/logs/telrun.log";

static XtIntervalId pollId;
static long lastpos;

static void logCB (XtPointer client, XtIntervalId *id);

/* execute telrun.
 * return -1 if trouble, else 0.
 */
int
startTelrun ()
{
	char cmd[128];
	int i;

	sprintf (cmd, "rund %s", telrun);

	if (system (cmd) == 0) {
	    for (i = 0; i < POLLSECS; i++) {
		sleep (1);
		if (testlock_running (telrun) == 0) {
		    msg ("New %s successfully started", telrun);
		    return (0);
		}
	    }
	}

	msg ("%s failed to start. Check rund.%s.log or %s.log.",
						    telrun, telrun, telrun);
	return (-1);
}

/* stop telrun */
void
stopTelrun()
{
	unlock_running (telrun, 1);
}

/* return 0 if telrun is running, else -1 */
int
chkTelrun()
{
	return (testlock_running(telrun));
}

/* toggle whether to monitor telrun.log. */
void
monitorTelrun(int whether)
{
	if (pollId) {
	    XtRemoveTimeOut (pollId);
	    pollId = (XtIntervalId)0;
	}

	if (whether) {
	    lastpos = 0L;
	    pollId = XtAppAddTimeOut (app, LOGPOLL, logCB, 0);
	}
}

/* periodicly check telrun.log and add to mesg area.
 * initially skip up to current end.
 */
static void
logCB (XtPointer client, XtIntervalId *id)
{
	long posnow;
	FILE *logfp;

	/* open fresh each time */
	logfp = telfopen (logfn, "r");
	if (!logfp) {
	    msg ("Can not monitor %s: %s", logfn, strerror(errno));
	    return;
	}

	/* seek to end and get current length */
	if (fseek (logfp, 0L, SEEK_END) < 0) {
	    msg ("Can not seek on %s: %s", logfn, strerror(errno));
	    fclose (logfp);
	    return;
	}
	posnow = ftell (logfp);

	/* if first time, skip to end.
	 * else if grown, go back and catch up.
	 */
	if (lastpos == 0L) {
	    lastpos = posnow;
	} else if (posnow > lastpos) {
	    char buf[1024];

	    fseek (logfp, lastpos, SEEK_SET);
	    while (fgets (buf, sizeof(buf), logfp))
		rmsg (buf);
	    clearerr (logfp);
	    lastpos = ftell (logfp);
	}

	/* finished with file */
	fclose (logfp);

	/* repeat */
	pollId = XtAppAddTimeOut (app, LOGPOLL, logCB, 0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: telrun.c,v $ $Date: 2001/04/19 21:12:06 $ $Revision: 1.1.1.1 $ $Name:  $"};
