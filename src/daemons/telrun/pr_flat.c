/* program to take a flat */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
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
#include "strops.h"
#include "scan.h"

#include "telrun.h"

static char dcfn[] = "archive/config/dome.cfg";
static char icfn[] = "archive/config/filter.cfg";

static time_t tmpTime;		/* used to save times from one step to next */
static int NFLAT;		/* config entry -- total to take */
static double FLATTAZ;		/* telescope azimuth for dome flat */
static double FLATTALT;		/* telescope altitude for dome flat */
static double FLATDAZ;		/* dome azimuth for dome flat */
static double DOMETOL;		/* max dome position tolerance, rads */
static int nflat;		/* n taken so far */
static int flatpid;		/* pid of process building real result */
static int filter;		/* index into filinfo we are using */

/* filled with filter/focus/temp info */
static FilterInfo *filinfo;
static int nfilinfo;

static void readCfg(void);
static void startFlat(time_t n);
static int buildReal(void);
static int tmpName(int n, char buf[]);
static void scanError(void);
static int findFilter (char f);

typedef void *StepFuncP;
typedef StepFuncP (*StepFunc)(time_t now);

static StepFuncP ps_wait4Start(time_t n);
static StepFuncP ps_wait4Setup(time_t n);
static StepFuncP ps_takeRaw(time_t n);
static StepFuncP ps_waitReal(time_t n);

/* program to create a new flat reference.
 * called periodically by main_loop().
 * first is only set when just starting this scan.
 * when finished, we also may move state along to pr_regscan.
 * return 0 if making progress, -1 on error or finished.
 * N.B. we are responsible for all our own logging and cleanup on errors.
 * N.B. due to circumstances beyond our control we may never get called again.
 */
int
pr_flat (int first)
{
	static StepFuncP step;

	/* start over if first call */
	if (first) {
	    readCfg();			/* read config entry */
	    nflat = 0;			/* so far */
	    step = ps_wait4Start;	/* init sequencer */
	}

	/* run this step, next is its return, done when NULL */
	step = step ? (*(void *(*)())step)(time(NULL)) : NULL;
	return (step ? 0 : -1);
}

/* start things moving to set up for the flat described by cscan */
void
pr_flatSetup()
{
	DomeState ds = telstatshmp->domestate;
	DShState ss = telstatshmp->shutterstate;
	char buf[64];

	/* insure we have everything */
	readCfg();

	/* start filter */
	filter = findFilter (cscan->filter);
	if (IMOT->have) {
	    buf[0] = cscan->filter;
	    buf[1] = '\0';
	    fifoWrite (Filter_Id, buf);
	}

	/* start telescope */
	sprintf (buf, "Alt:%g Az:%g", FLATTALT, FLATTAZ);
	fifoWrite (Tel_Id, buf);

	/* turn on the lights */
	if (telstatshmp->lights >= 0
			&& telstatshmp->lights != filinfo[filter].flatlights)
	    fifoWrite (Lights_Id, "%d", filinfo[filter].flatlights);

	/* insure focus is stopped */
	if (OMOT->have && OMOT->cvel)
	    fifoWrite (Focus_Id, "Stop");

	/* insure roof is closed */
	if (ss != SH_ABSENT && ss != SH_CLOSING && ss != SH_CLOSED)
	    fifoWrite (Dome_Id, "Close");

	/* insure dome is at correct az */
    if(FLATDAZ != 0.0) {
    	if ((ss == SH_ABSENT || ss == SH_CLOSED) && ds == DS_STOPPED
	    		    && delra(telstatshmp->dometaz-FLATDAZ) > DOMETOL)
	        fifoWrite (Dome_Id, "Az:%g", FLATDAZ);
    }
}

/* wait for start time */
static StepFuncP
ps_wait4Start(time_t n)
{
	/* check for too late */
	if (n > cscan->starttm + cscan->startdt) {
	    n -= cscan->starttm + cscan->startdt;
	    tlog (cscan, "Flat too late by %d secs", (int) n);
	    scanError();
	    return (NULL);
	}

	/* go! */

	/* N.B. we depend on bias to insure we don't start too early. */

	tlog (cscan, "Checking flat setup");
	pr_flatSetup();

	/* now wait for setup and time to start */
	tmpTime = n + SETUP_TO;
	return (ps_wait4Setup);
}

/* wait for all required systems to come ready then start camera */
static StepFuncP
ps_wait4Setup(time_t n)
{
	DomeState ds = telstatshmp->domestate;
	int camok = telstatshmp->camstate == CAM_IDLE;
	int telok = telstatshmp->telstate == TS_STOPPED;
	int filok = FILTER_READY;
	int domok = (ds == DS_ABSENT) || (ds == DS_STOPPED
			    && delra(telstatshmp->domeaz-FLATDAZ) <= DOMETOL);
	int allok;
	Scan *sp = cscan;

    // ignore dome rotation if flat az is not defined
    if(FLATDAZ == 0.0) domok = 1;
    allok = camok && telok && filok && domok;

    // but we do want to be sure the door is closed
    domok &= (telstatshmp->shutterstate == SH_CLOSED);

	/* trouble if not finished within SETUP */
	if (!allok && n > tmpTime) {
	    tlog (sp, "Flat setup timed out for%s%s%s%s%s",
						    camok ? "" : " camera",
						    telok ? "" : " telescope",
						    filok ? "" : " filter",
						    domok ? "" : " dome");
	    all_stop(1);
	    return (NULL);
	}

	/* don't wait past allowable dt in any case */
	if (n > sp->starttm + sp->startdt) {
	    n -= sp->starttm + sp->startdt;
	    tlog (sp, "Flat setup too late by %d secs", (int) n);
	    all_stop(1);
	    return (NULL);
	}

	/* wait until everything ready */
	if (!allok)
	    return (ps_wait4Setup);

	/* go! */

	/* N.B. we never publish (set starttm/running) because either bias
	 * did already (if not taking data) or regscan will (if taking data).
	 */

	nflat = 0;
	startFlat(n);
	return (ps_takeRaw);
}

/* wait until tmpTime for flat to finish.
 * start another if more else start to build real.
 */
static StepFuncP
ps_takeRaw(time_t n)
{
	if (n < tmpTime || telstatshmp->camstate != CAM_IDLE)
	    return (ps_takeRaw);
	if (++nflat < NFLAT) {
	    startFlat(n);
	    return (ps_takeRaw);
	}

	/* all intermediate flats are complete. start to build real one */
	if (telstatshmp->lights > 0)
	    fifoWrite (Lights_Id, "0");
	if (buildReal() < 0) {
	    scanError();
	    return (NULL);
	}
	return (ps_waitReal);
}

/* poll for flatpid to complete */
static StepFuncP
ps_waitReal(time_t n)
{
	Scan *sp = cscan;
	int options = WNOHANG;
	int pid, status;
	char buf[1024];
	FILE *ls;

	pid = waitpid (flatpid, &status, options);
	if (pid == 0)
	    return (ps_waitReal);	/* continue waiting */
	if (pid < 0) {
	    tlog (sp, "flat waitpid(%d): %s", flatpid, strerror(errno));
	    scanError();
	    return (NULL);		/* error */
	}
	if (pid != flatpid) {
	    tlog (sp, "flat waitpid(%d) returns unknown pid: %d",
	    							flatpid, pid);
	    scanError();
	    return (NULL);		/* error */
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
	    tlog (sp, "flat abnormal termination: %d", status);
	    scanError();
	    return (NULL);		/* error */
	}

	/* new flat is complete */

	/* show filename */
	ls = popen ("cd $TELHOME/archive/calib; ls -1t cf*", "r");
	if (ls) {
	    if (fgets (buf, sizeof(buf), ls))
		tlog (sp, "New %c flat file: %s", sp->filter, buf);
	    pclose (ls);
	}

	/* start next phase if appropriate else mark D */
	if (sp->ccdcalib.data != CD_NONE) {
	    if (addProgram (pr_regscan, 0) < 0) {
		tlog (sp, "flat could not go on to regular scan");
		scanError();
		return (NULL);		/* error */
	    }
	} else {
	    tlog (sp, "Flat is complete");
	    markScan (scanfile, sp, 'D');
	    sp->running = 0;
	    sp->starttm = 0;
	}

	return (NULL);			/* done working on flats at least */
}


/* helper funcs */

/* read camera.cfg and dome.cfg or die! */
static void
readCfg()
{
#define NCCFG   (sizeof(ccfg)/sizeof(ccfg[0]))
#define NDCFG   (sizeof(dcfg)/sizeof(dcfg[0]))

	static CfgEntry ccfg[] = {
	    {"NFLAT",   CFG_INT, &NFLAT},
	};

	static CfgEntry dcfg[] = {
	    {"DOMETOL",	 CFG_DBL, &DOMETOL},
	    {"FLATTAZ",	 CFG_DBL, &FLATTAZ},
	    {"FLATTALT", CFG_DBL, &FLATTALT},
	    {"FLATDAZ",	 CFG_DBL, &FLATDAZ},
	};

	char buf[1024];
	int n;

	n = readCfgFile (0, ccfn, ccfg, NCCFG);
	if (n != NCCFG) {
	    tlog (cscan, "%s: no %s", ccfn, ccfg[0].name);
	    die();
	}

	n = readCfgFile (0, dcfn, dcfg, NDCFG);
	if (n != NDCFG) {
	    tlog (cscan, "%s: no %s, %s, %s or %s", dcfn, dcfg[0].name,
				    dcfg[1].name, dcfg[2].name, dcfg[3].name);
	    die();
	}

        nfilinfo = readFilterCfg (0, icfn, &filinfo, NULL, buf);
	if (nfilinfo <= 0) {
	    if (nfilinfo < 0)
		tlog (cscan, "%s: %s", basenm(icfn), buf);
	    else
		tlog (cscan, "%s: no entries", basenm(icfn));
	    die();
	}
}

/* start camera for intermediate flat frame number nflat and set tmpTime */
static void
startFlat(time_t n)
{
#if JSF_VERSION
    // JSF Specific:
    // Define a "secret" shutter code that means
    // we're taking a flat so the PVCAM script can name it appropriately
    #define FLAT_SHUTTER	-1
#else
    #define FLAT_SHUTTER    CCDSO_Open
#endif

	double dur = filinfo[filter].flatdur;
	Scan *sp = cscan;
	char fullpath[32];

	tlog (cscan, "Starting %gs %c flat %d of %d", dur, sp->filter, nflat+1,
									NFLAT);

	(void) tmpName (nflat, fullpath);
	fifoWrite (Cam_Id,
		    "Expose %d+%dx%dx%d %dx%d %g %d %d %s\n%s\n%s\n%s\n%s\n",
			sp->sx, sp->sy, sp->sw, sp->sh, sp->binx, sp->biny,
			    dur, FLAT_SHUTTER, sp->priority, fullpath,
			sp->obj.o_name,
			sp->comment,
			sp->title,
			sp->observer);

	tmpTime = n + (int)floor(filinfo[filter].flatdur + CAMDIG_MAX + 0.5);
}

/* build command line and fork process to assemble a real flat file.
 * return 0 if ok, else -1
 */
static int
buildReal()
{
	char cmd[2048];

#if JSF_VERSION
    sprintf(cmd,"calimagePV -F %d",nflat);
#else
	int l = 0;
	int i;

	/* build command line */
	l += sprintf (cmd+l, "calimage -F -l %c", cscan->filter);
	for (i = 0; i < nflat; i++) {
	    l += sprintf (cmd+l, " ");
	    l += tmpName (i, cmd+l);
	}
	tlog (cscan, "%s", cmd);	/* rm's are not very interesting */

	l += sprintf (cmd+l, "; rm");
	for (i = 0; i < nflat; i++) {
	    l += sprintf (cmd+l, " ");
	    l += tmpName (i, cmd+l);
	}
#endif

	/* fork/exec sh to do this */
	switch (flatpid = fork()) {
	case -1:
	    tlog (cscan, "flat fork(): %s", strerror(errno));
	    return (-1);
	case 0:
	    execl ("/bin/sh", "sh", "-c", cmd, NULL);
	    tlog (cscan, "flat execl(sh): %s", strerror(errno));
	    exit(1);
	    return (-1);	/* superflous */
	default:
	    return (0);
	}
}

/* fill buf[] with name of intermediate flat file n, return string length */
static int
tmpName(int n, char buf[])
{
	return(sprintf (buf, "/tmp/Flat%03d", n));
}

/* mark the current scan as failed and completed */
static void
scanError()
{
	markScan (scanfile, cscan, 'F');
	cscan->running = 0;
	cscan->starttm = 0;

	if (telstatshmp->lights > 0)
	    fifoWrite (Lights_Id, "0");
}

/* find filter f in filinfo and return index */
static int
findFilter (char f)
{
	char capf = islower(f) ? toupper(f) : f;
	int i;

	for (i = 0; i < nfilinfo; i++) {
	    char capn = filinfo[i].name[0];
	    if (islower(capn))
		capn = toupper(capn);
	    if (capn == capf)
		return (i);
	}

	/* ?? */
	tlog (cscan, "No config entry for filter %c", f);
	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: pr_flat.c,v $ $Date: 2002/12/13 15:56:39 $ $Revision: 1.5 $ $Name:  $"};
