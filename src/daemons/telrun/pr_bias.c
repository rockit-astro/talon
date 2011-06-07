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
static int NBIAS;		/* config entry -- total to take */
static int nbias;		/* n taken so far */
static int biaspid;		/* pid of process building real result */

static void readCfg(void);
static void startBias(time_t n);
static int buildReal(void);
static int tmpName(int n, char buf[]);
static void scanError(void);

typedef void *StepFuncP;
typedef StepFuncP (*StepFunc)(time_t now);

static StepFuncP ps_wait4Start(time_t n);
static StepFuncP ps_takeRaw(time_t n);
static StepFuncP ps_waitReal(time_t n);

/* program to create a new bias reference.
 * called periodically by main_loop().
 * first is only set when just starting this scan.
 * when finished, we also may move state along to pr_therm or pr_regscan.
 * return 0 if making progress, -1 on error or finished.
 * N.B. we are responsible for all our own logging and cleanup on errors.
 * N.B. due to circumstances beyond our control we may never get called again.
 */
int
pr_bias (int first)
{
	static StepFuncP step;
	Scan *sp = cscan;

	/* start over if first call */
	if (first) {
	    readCfg();			/* read config entry */
	    nbias = 0;			/* so far */
	    step = ps_wait4Start;	/* init sequencer */

	    /* start setting up other equipment if possible */
	    if (sp->ccdcalib.newc == CT_FLAT) {
		tlog (sp, "Starting flat pre-setup");
		pr_flatSetup();
	    } else if (sp->ccdcalib.data != CD_NONE) {
		tlog (sp, "Starting scan pre-setup");
		pr_regSetup();
	    }
	}

	/* run this step, next is its return, done when NULL */
	step = step ? (*(void *(*)())step)(time(NULL)) : NULL;
	return (step ? 0 : -1);
}

/* wait for start time */
static StepFuncP
ps_wait4Start(time_t n)
{
	Scan *sp = cscan;
	int takedata = sp->ccdcalib.data != CD_NONE;
	int pretime = takedata ? 2*SETUP_TO : 0;

	/* check for nominally near starttm */
	if (n > sp->starttm + sp->startdt) {
	    n -= sp->starttm + sp->startdt;
	    tlog (sp, "Bias too late by %d secs", (int) n);
	    scanError();
	    return (NULL);
	}

	/* if taking data, get started well early, else wait for starttm */
	if (n < sp->starttm - pretime)
	    return (ps_wait4Start);		/* waiting */

	if (n > sp->starttm) // if no time to setup
	{
		n -= sp->starttm;
	    tlog (sp, "Starting bias too late by %d secs", (int) n);
	    scanError();
		return(NULL);	  // abort
	}		
	
	/* go! */

	/* only publish if there is no real data to be taken later */
	if (!takedata) {
	    int late = n - cscan->starttm;

	    if (late)
		tlog (sp, "Starting calibrations .. late by %d sec%s", late,
							late == 1 ? "" : "s");
	    else
		tlog (sp, "Starting calibrations on time");

	    sp->starttm = n;		/* real start time */
	    sp->running = 1;		/* we are under way */
	}

	nbias = 0;
	startBias(n);
	return (ps_takeRaw);
}

/* wait until tmpTime for bias to finish.
 * start another if more else start to build real.
 */
static StepFuncP
ps_takeRaw(time_t n)
{
	if (n < tmpTime || telstatshmp->camstate != CAM_IDLE)
	    return (ps_takeRaw);
	if (++nbias < NBIAS) {
	    startBias(n);
	    return (ps_takeRaw);
	}

	/* all intermediate biases are complete. start to build real one */
	if (buildReal() < 0) {
	    scanError();
	    return (NULL);
	}
	return (ps_waitReal);
}

/* poll for biaspid to complete */
static StepFuncP
ps_waitReal(time_t n)
{
	Scan *sp = cscan;
	int options = WNOHANG;
	int pid, status;
	char buf[1024];
	FILE *ls;

	pid = waitpid (biaspid, &status, options);
	if (pid == 0)
	    return (ps_waitReal);	/* continue waiting */
	if (pid < 0) {
	    tlog (sp, "bias waitpid(%d): %s", biaspid, strerror(errno));
	    scanError();
	    return (NULL);		/* error */
	}
	if (pid != biaspid) {
	    tlog (sp, "bias waitpid(%d) returns unknown pid: %d",
	    							biaspid, pid);
	    scanError();
	    return (NULL);		/* error */
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
	    tlog (sp, "bias abnormal termination: %d", status);
	    scanError();
	    return (NULL);		/* error */
	}

	/* new bias is complete */

	/* show filename */
	ls = popen ("cd $TELHOME/archive/calib; ls -1t cb*", "r");
	if (ls) {
	    if (fgets (buf, sizeof(buf), ls))
		tlog (sp, "New bias file: %s", buf);
	    pclose (ls);
	}

	/* start next phase if appropriate else mark D */
	if (sp->ccdcalib.newc > CT_BIAS) {
	    if (addProgram (pr_thermal, 0) < 0) {
		tlog (sp, "bias could not go on to thermal");
		scanError();
		return (NULL);		/* error */
	    }
	} else if (sp->ccdcalib.data != CD_NONE) {
	    if (addProgram (pr_regscan, 0) < 0) {
		tlog (sp, "bias could not go on to regular scan");
		scanError();
		return (NULL);		/* error */
	    }
	} else {
	    tlog (sp, "Bias is complete");
	    markScan (scanfile, sp, 'D');
	    sp->running = 0;
	    sp->starttm = 0;
	}

	return (NULL);			/* done working on bias at least */
}


/* helper funcs */

/* read camera.cfg for NBIAS or die! */
static void
readCfg()
{
	static char name[] = "NBIAS";
	int s;

	s = read1CfgEntry (0, ccfn, name, CFG_INT, &NBIAS, 0);
	if (s < 0) {
	    tlog (cscan, "%s: no %s", ccfn, name);
	    die();
	}
}

/* start camera for intermediate bias frame number nbias and set tmpTime */
static void
startBias(time_t n)
{
	Scan *sp = cscan;
	char fullpath[32];

	tlog (sp, "Starting bias %d of %d", nbias+1, NBIAS);

	(void) tmpName (nbias, fullpath);
	fifoWrite (Cam_Id,
		    "Expose %d+%dx%dx%d %dx%d %g %d %d %s\n%s\n%s\n%s\n%s\n",
			sp->sx, sp->sy, sp->sw, sp->sh, sp->binx, sp->biny,
			    0.0, CCDSO_Closed, sp->priority, fullpath,
			sp->obj.o_name,
			sp->comment,
			sp->title,
			sp->observer);

	tmpTime = n + (int)floor(CAMDIG_MAX + 0.5);
}

/* build command line and fork process to assemble a real bias file.
 * return 0 if ok, else -1
 */
static int
buildReal()
{
	char cmd[2048];
#if JSF_VERSION
    sprintf(cmd,"calimagePV -B %d",nbias);
#else
	int l = 0;
	int i;

	/* build command line */
	l += sprintf (cmd+l, "calimage -B");
	for (i = 0; i < nbias; i++) {
	    l += sprintf (cmd+l, " ");
	    l += tmpName (i, cmd+l);
	}
	tlog (cscan, "%s", cmd);	/* rm's are not very interesting */

	l += sprintf (cmd+l, "; rm");
	for (i = 0; i < nbias; i++) {
	    l += sprintf (cmd+l, " ");
	    l += tmpName (i, cmd+l);
	}
#endif

	/* fork/exec sh to do this */
	switch (biaspid = fork()) {
	case -1:
	    tlog (cscan, "bias fork(): %s", strerror(errno));
	    return (-1);
	case 0:
	    execl ("/bin/sh", "sh", "-c", cmd, NULL);
	    tlog (cscan, "bias execl(sh): %s", strerror(errno));
	    exit(1);
	    return (-1);	/* superflous */
	default:
	    return (0);
	}
}

/* fill buf[] with name of intermediate bias file n, return string length */
static int
tmpName(int n, char buf[])
{
	return(sprintf (buf, "/tmp/Bias%03d", n));
}

/* mark the current scan as failed and completed */
static void
scanError()
{
	markScan (scanfile, cscan, 'F');
	cscan->running = 0;
	cscan->starttm = 0;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: pr_bias.c,v $ $Date: 2002/12/13 15:56:39 $ $Revision: 1.6 $ $Name:  $"};
