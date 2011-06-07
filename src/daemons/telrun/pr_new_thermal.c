/* program to take a bias */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telenv.h"
#include "telstatshm.h"
#include "configfile.h"
#include "scan.h"

#include "telrun.h"

static time_t tmpTime;		/* used to save times from one step to next */

static void scanError(void);

typedef void *StepFuncP;
typedef StepFuncP (*StepFunc)(time_t now);
static StepFuncP ps_wait4Start(time_t n);
static StepFuncP ps_wait4Exp(time_t n);

/* program to create a new bias reference.
 * called periodically by main_loop().
 * first is only set when just starting this scan.
 * when finished, we also may move state along to pr_therm or pr_regscan.
 * return 0 if making progress, -1 on error or finished.
 * N.B. we are responsible for all our own logging and cleanup on errors.
 * N.B. due to circumstances beyond our control we may never get called again.
 */
int
pr_new_thermal (int first)
{
	static StepFuncP step;

	/* start over if first call */
	if (first) {
	    step = ps_wait4Start;	/* init sequencer */
	    /* start setting up other equipment if possible */	
	}

	/* run this step, next is its return, done when NULL */
	step = step ? (*(void *(*)())step)(time(NULL)) : NULL;
	return (step ? 0 : -1);
}

static StepFuncP
ps_wait4Start(time_t n)
{
	Scan *sp = cscan;
	char fullpath[1024];
	
	/* check for nominally near starttm */
	if (n > sp->starttm + sp->startdt) {
	    n -= sp->starttm + sp->startdt;
	    tlog (sp, "Dark too late by %d secs", (int) n);
	    scanError();
	    return (NULL);
	}

	/* if taking data, get started well early, else wait for starttm */
	if (n < sp->starttm)
	    return (ps_wait4Start);		/* waiting */

	
	/* go! */
	sp->starttm = n;		/* real start time */
	sp->running = 1;		/* we are under way */
	sprintf (fullpath, "%s/%s", sp->imagedn, sp->imagefn);
	fifoWrite (Cam_Id,
		    "Expose %d+%dx%dx%d %dx%d %g %d %d %s\n%s\n%s\n%s\n%s\n",
			sp->sx, sp->sy, sp->sw, sp->sh, sp->binx, sp->biny,
			    sp->dur, CCDSO_Closed, sp->priority, fullpath,
			sp->obj.o_name,
			sp->comment,
			sp->title,
			sp->observer);

	/* save estimate of finish time */
	tmpTime = n + (int)floor(sp->dur + 0.5);

	/* ok */
	return (ps_wait4Exp);
}
static StepFuncP
ps_wait4Exp (time_t n)
{
	Scan *sp = cscan;
	if (n <= tmpTime || telstatshmp->camstate == CAM_EXPO || telstatshmp->camstate == CAM_READ)
	    return (ps_wait4Exp);	/* waiting... */
	markScan (scanfile, sp, 'D');
	sp->running = 0;
	sp->starttm = 0;
	/* all up to post processing now */
	return (NULL);
}

static void
scanError()
{
	markScan (scanfile, cscan, 'F');
	cscan->running = 0;
	cscan->starttm = 0;
}
