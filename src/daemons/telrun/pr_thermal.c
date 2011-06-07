/* program to take a thermal */

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
static int NTHERM;		/* config entry -- total to take */
static double THERMDUR;		/* config entry -- seconds per thermal */
static int ntherm;		/* n taken so far */
static int thermpid;		/* pid of process building real result */

static void readCfg(void);
static void startTherm(time_t n);
static int buildReal(void);
static int tmpName(int n, char buf[]);
static void scanError(void);

typedef void *StepFuncP;
typedef StepFuncP (*StepFunc)(time_t now);

static StepFuncP ps_wait4Start(time_t n);
static StepFuncP ps_takeRaw(time_t n);
static StepFuncP ps_waitReal(time_t n);

/* program to create a new thermal reference.
 * called periodically by main_loop().
 * first is only set when just starting this scan.
 * when finished, we also may move state along to pr_flat or pr_regscan.
 * return 0 if making progress, -1 on error or finished.
 * N.B. we are responsible for all our own logging and cleanup on errors.
 * N.B. due to circumstances beyond our control we may never get called again.
 */
int
pr_thermal (int first)
{
	static StepFuncP step;

	/* start over if first call */
	if (first) {
	    readCfg();			/* read config entry */
	    ntherm = 0;			/* so far */
	    step = ps_wait4Start;	/* init sequencer */
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

	/* check for too late */
	if (n > sp->starttm + sp->startdt) {
	    n -= sp->starttm + sp->startdt;
	    tlog (sp, "Thermal too late by %d secs", (int) n);
	    scanError();
	    return (NULL);
	}
	
	/* go! */

	/* N.B. we depend on bias to insure we don't start too early.
	 * N.B. we never publish (set starttm/running) because either bias
	 * did already (if not taking data) or regscan will (if taking data).
	 */

	ntherm = 0;
	startTherm(n);
	return (ps_takeRaw);
}

/* wait until tmpTime for thermal to finish.
 * start another if more else start to build real.
 */
static StepFuncP
ps_takeRaw(time_t n)
{
	if (n < tmpTime || telstatshmp->camstate != CAM_IDLE)
	    return (ps_takeRaw);
	if (++ntherm < NTHERM) {
	    startTherm(n);
	    return (ps_takeRaw);
	}

	/* all intermediate thermals are complete. start to build real one */
	if (buildReal() < 0) {
	    scanError();
	    return (NULL);
	}
	return (ps_waitReal);
}

/* poll for thermpid to complete */
static StepFuncP
ps_waitReal(time_t n)
{
	Scan *sp = cscan;
	int options = WNOHANG;
	int pid, status;
	char buf[1024];
	FILE *ls;

	pid = waitpid (thermpid, &status, options);
	if (pid == 0)
	    return (ps_waitReal);	/* continue waiting */
	if (pid < 0) {
	    tlog (sp, "thermal waitpid(%d): %s", thermpid, strerror(errno));
	    scanError();
	    return (NULL);		/* error */
	}
	if (pid != thermpid) {
	    tlog (sp, "thermal waitpid(%d) returns unknown pid: %d",
	    							thermpid, pid);
	    scanError();
	    return (NULL);		/* error */
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
	    tlog (sp, "thermal abnormal termination: %d", status);
	    scanError();
	    return (NULL);		/* error */
	}

	/* new therm is complete */

	/* show filename */
	ls = popen ("cd $TELHOME/archive/calib; ls -1t ct*", "r");
	if (ls) {
	    if (fgets (buf, sizeof(buf), ls))
		tlog (sp, "New thermal file: %s", buf);
	    pclose (ls);
	}

	/* start next phase if appropriate else mark D */
	if (sp->ccdcalib.newc == CT_FLAT) {
	    if (addProgram (pr_flat, 0) < 0) {
		tlog (sp, "thermal could not go on to flat");
		scanError();
		return (NULL);		/* error */
	    }
	} else if (sp->ccdcalib.data != CD_NONE) {
	    if (addProgram (pr_regscan, 0) < 0) {
		tlog (sp, "thermal could not go on to regular scan");
		scanError();
		return (NULL);		/* error */
	    }
	} else {
	    tlog (sp, "Thermal is complete");
	    markScan (scanfile, sp, 'D');
	    sp->running = 0;
	    sp->starttm = 0;
	}

	return (NULL);			/* done working on thermal anyway */
}


/* helper funcs */

/* read camera.cfg for NTHERM and THERMDUR or die! */
static void
readCfg()
{
#define NCCFG   (sizeof(ccfg)/sizeof(ccfg[0]))

	static CfgEntry ccfg[] = {
	    {"NTHERM",   CFG_INT, &NTHERM},
	    {"THERMDUR", CFG_DBL, &THERMDUR},
	};

	int n;

	n = readCfgFile (0, ccfn, ccfg, NCCFG);
	if (n != NCCFG) {
	    tlog (cscan, "%s: no %s and/or %s", ccfn,ccfg[0].name,ccfg[1].name);
	    die();
	}
}

/* start camera for intermediate thermal frame number ntherm and set tmpTime */
static void
startTherm(time_t n)
{
	Scan *sp = cscan;
	char fullpath[32];

	tlog (cscan, "Starting %gs thermal %d of %d",
						    THERMDUR, ntherm+1, NTHERM);

	(void) tmpName (ntherm, fullpath);
	fifoWrite (Cam_Id,
		    "Expose %d+%dx%dx%d %dx%d %g %d %d %s\n%s\n%s\n%s\n%s\n",
			sp->sx, sp->sy, sp->sw, sp->sh, sp->binx, sp->biny,
			    THERMDUR, CCDSO_Closed, sp->priority, fullpath,
			sp->obj.o_name,
			sp->comment,
			sp->title,
			sp->observer);

	tmpTime = n + (int)floor(THERMDUR + CAMDIG_MAX + 0.5);
}

/* build command line and fork process to assemble a real thermal file.
 * return 0 if ok, else -1
 */
static int
buildReal()
{
	char cmd[2048];
#if JSF_VERSION
    sprintf(cmd,"calimagePV -T %d",ntherm);
#else
	int l = 0;
	int i;

	/* build command line */
	l += sprintf (cmd+l, "calimage -T");
	for (i = 0; i < ntherm; i++) {
	    l += sprintf (cmd+l, " ");
	    l += tmpName (i, cmd+l);
	}
	tlog (cscan, "%s", cmd);	/* rm's are not very interesting */

	l += sprintf (cmd+l, "; rm");
	for (i = 0; i < ntherm; i++) {
	    l += sprintf (cmd+l, " ");
	    l += tmpName (i, cmd+l);
	}
#endif

	/* fork/exec sh to do this */
	switch (thermpid = fork()) {
	case -1:
	    tlog (cscan, "thermal fork(): %s", strerror(errno));
	    return (-1);
	case 0:
	    execl ("/bin/sh", "sh", "-c", cmd, NULL);
	    tlog (cscan, "thermal execl(sh): %s", strerror(errno));
	    exit(1);
	    return (-1);	/* superflous */
	default:
	    return (0);
	}
}

/* fill buf[] with name of intermediate thermal file n, return string length */
static int
tmpName(int n, char buf[])
{
	return(sprintf (buf, "/tmp/Therm%03d", n));
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
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: pr_thermal.c,v $ $Date: 2002/12/04 08:48:01 $ $Revision: 1.3 $ $Name:  $"};
