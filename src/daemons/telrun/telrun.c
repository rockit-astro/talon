/* main() for telrun.
 */

#include <stdio.h>
#include <signal.h>
#include <stdarg.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telstatshm.h"
#include "telenv.h"
#include "cliserv.h"
#include "configfile.h"
#include "running.h"
#include "strops.h"
#include "scan.h"

//#include "config.h"
#include "telrun.h"

#define	DWT	5	/* max secs to wait for daemon to start */

static void usage (void);
static void init_cfg(void);
static void init_shm(void);
static void init_camerad(void);
static void init_telescoped(void);
static void main_loop(void);
static void sigshutdown(int s);
static void checkSun(void);
static int checkWx(void);
static int newProgram (Scan *sp);
static void runPrograms(void);
static void resetPrograms(void);
static void lockus(void);
static void unlockus(void);

TelStatShm *telstatshmp;	/* ubiquitous shared-mem segment */

/* variables set from the config files -- see init_cfg() */
char tcfn[] = "archive/config/telsched.cfg";
char ccfn[] = "archive/config/camera.cfg";
double SETUP_TO;	/* max secs to wait for devices to set up before a run*/
double CAMDIG_MAX;	/* max time for full-frame download, secs */
static double SUNDOWN;	/* rads sun is below horizon we consider dark */
static double STOWALT;	/* stow altitude */
static double STOWAZ;	/* stow az */
static int IGSUN;	/* 1 to ignore whether sun is up */
static int AUTOHOME=0; /* (optional) 1 to force telrun to home if needed.  NOTE: use for CSI implementation only */
static int HOMEWAIT=100; /* (optional) How long to delay to allow homing to complete, seconds  */

/* a collection of programs to manage */
#define	MAXPROG	10	/* modest number at most */
typedef struct {
    PrFunc p;		/* program control function .. NULL if absent */
    int first;		/* enough history to know first time called */
    int bkg;		/* if set, not removed by resetPrograms() */
} Program;
static Program program[MAXPROG];

char scanfile[128] = "archive/telrun/telrun.sls";	/* file to monitor */
static char logdir[] = "user/logs";	/* location of per-sched logs */
static char *progname;	/* us */
static int sunisup = 1;	/* 1 when sun is up, else 0
			 * init to 1 to avoid initial false close.
			 */

// homing check
static int checkHoming(void);
			
int
main(ac, av)
int ac;
char *av[];
{
	char *str;

	/* set initial dir info and set up for auto logging */
	progname = basenm(av[0]);	/* us */

	/* crack arguments */
	for (av++; --ac > 0 && *(str = *av) == '-'; av++) {
	    char c;
	    while ((c = *++str) != '\0')
		switch (c) {
		default:
		    usage();
		    break;
		}
	}

	/* now there are ac remaining args starting at av[0] */
	if (ac != 0)
	    usage();

	/* make lock file -- exit if already there */
	lockus();
	atexit (unlockus);

	/* don't want SIGPIPE on bad fifo writes */
	signal (SIGPIPE, SIG_IGN);

	/* shut down on most normal signals */
	signal (SIGTERM, sigshutdown);
	signal (SIGINT, sigshutdown);
	signal (SIGHUP, sigshutdown);
	signal (SIGPWR, sigshutdown);

	/* init everything */
	init_telescoped();
	init_camerad();
	init_cfg();
	init_fifos();
	init_shm();
	telfixpath (scanfile, scanfile);

	if(AUTOHOME)
	{	
		if(telstatshmp->coverstate!=CV_OPEN && telstatshmp->coverstate!=CV_ABSENT) {
			fifoWrite(Cover_Id,"coverOpen");
			sleep(60);
			if(telstatshmp->coverstate!=CV_OPEN) {
				tlog(NULL,"Failed to open covers");
				all_stop(0);
				return(-1);
			}
		}
			
		if(!checkHoming()) {
#if ENABLE_ROTATING_DOME
			fifoWrite(Dome_Id,"unpark");
#endif		
			tlog(NULL,"Telescope not fully homed -- auto homing (%d seconds)\n",HOMEWAIT);
			// home all the axes if we report not homed
			fifoWrite(Tel_Id,"home");
			fifoWrite(Focus_Id,"home");
			fifoWrite(Filter_Id,"home");
			
			// now wait for this to finish
			sleep(HOMEWAIT);
			fifoWrite(Cover_Id,"coverOpen");
#if ENABLE_ROTATING_DOME
			fifoWrite(Dome_Id,"home");
			sleep(DOMEHOMEWAIT);
#endif
		}	
		// check again before running schedule
		if(!checkHoming()) {
	    	tlog (NULL, "Failed to home within %d seconds. Aborting.\n",HOMEWAIT);
		    all_stop(0);
	    	return(-1);
	    }
	}	

	
	/* go, forever */
	tlog (NULL, "%s start.", progname);
	while (1)
	    main_loop();

	return (1);
}

static int checkHoming(void)
{
	/* See if telescope has been homed */
	int homed = 1;
	if(homed && telstatshmp->minfo[TEL_HM].have) homed = telstatshmp->minfo[TEL_HM].ishomed;
	if(homed && telstatshmp->minfo[TEL_DM].have) homed = telstatshmp->minfo[TEL_DM].ishomed;
	if(homed && telstatshmp->minfo[TEL_RM].have) homed = telstatshmp->minfo[TEL_RM].ishomed;
	if(homed && telstatshmp->minfo[TEL_OM].have) homed = telstatshmp->minfo[TEL_OM].ishomed;
	if(homed && telstatshmp->minfo[TEL_IM].have) homed = telstatshmp->minfo[TEL_IM].ishomed;
	return(homed);
}

/* exit when things might be very bad.
 * N.B. only intended for use after everything is up and running.
 */
void
die()
{
	all_stop(1);
	exit(2);
}

/* add p to the list of programs to be executed.
 * if bkg, mark as such and do not kill.
 * they take themselves out of the list by returning -1.
 * return 0 if ok, -1 if no more room.
 */
int
addProgram (PrFunc p, int bkg)
{
	int i;

	/* insert in any unused slot */
	for (i = 0; i < MAXPROG; i++) {
	    Program *pp = &program[i];
	    if (!pp->p) {
		pp->p = p;
		pp->first = 1;
		pp->bkg = bkg;
		return (0);
	    }
	}

	tlog (NULL, "Out of program slots!!");
	return (-1);
}

/* stop the 'scope and all current programs.
 * keep shared memory up to date.
 * also, if mark, mark current scan as failed.
 */
void
all_stop(int mark)
{
	/* stop all hardware */
	tlog (cscan, "All stop");
	stop_all_devices();

	/* fail out any running scan, if desired */
	if (mark && cscan->running) {
	    tlog (cscan, "Failing scan that creates %s.", cscan->imagefn);
	    markScan (scanfile, cscan, 'F');
	}
	/* always cancel shm publishing */
	cscan->running = 0;
	cscan->starttm = 0;

	resetPrograms();
}

/* reset all non-bkg programs */
static void
resetPrograms()
{
	int i;

	for (i = 0; i < MAXPROG; i++) {
	    Program *pp = &program[i];
	    if (pp->p && !pp->bkg)
		pp->p = NULL;
	}
}

/* run all programs  -- remove from list if returns -1 */
static void
runPrograms()
{
	int i;

	for (i = 0; i < MAXPROG; i++) {
	    Program *pp = &program[i];
	    if (pp->p) {
		if ((*pp->p)(pp->first) < 0)
		    pp->p = NULL;
		else
		    pp->first = 0;
	    }
	}
}

/* write a log message to stdout with a time stamp and source name.
 * if sp then also append to sp->schedfn.log in user/logs.
 * N.B. if result doesn't end with \n we add it.
 */
void
tlog (Scan *sp, char *fmt, ...)
{
	char buf[1024];
	va_list ap;
	FILE *fp;
	int l;

	/* set up fp as per-schedule log file if appropriate */
	if (sp && sp->starttm && sp->schedfn[0]) {
	    sprintf (buf, "%s/%s", logdir, sp->schedfn);
	    l = strlen (buf);
	    if (l >= 4 && strcasecmp (&buf[l-4], ".sch") == 0)
		strcpy (&buf[l-4], ".log");	/* change .sch to .log */
	    else
		strcat (buf, ".log");		/* or just append it */
	    fp = telfopen (buf, "a+");		/* tough luck if fails */
	} else
	    fp = NULL;

	/* start with time stamp and source name if set */
	l = 0;
	l = sprintf (buf+l, "%s: ", timestamp(time(NULL)));
	if (sp && sp->starttm && sp->obj.o_name[0])
	    l += sprintf (buf+l, "%s: ", sp->obj.o_name);

	/* format the message */
	va_start (ap, fmt);
	l += vsprintf (buf+l, fmt, ap);
	va_end (ap);

	/* add \n if not already */
	if (l > 0 && buf[l-1] != '\n') {
	    buf[l++] = '\n';
	    buf[l] = '\0';
	}

	/* log to stdout */
	fputs (buf, stdout);
	fflush (stdout);

	/* and to fp if possible */
	if (fp) {
	    fputs (buf, fp);
	    fclose (fp);
	}
}

static void
usage ()
{
	fprintf (stderr,"%s:\n", progname);
	exit(1);
}

/* read config file. exit if any trouble */
static void
init_cfg()
{
#define	NTCFG	(sizeof(tcfg)/sizeof(tcfg[0]))
#define	NTCFG2	(sizeof(tcfg2)/sizeof(tcfg2[0]))
#define	NCCFG	(sizeof(ccfg)/sizeof(ccfg[0]))
	static CfgEntry tcfg[] = {
	    {"SUNDOWN",	CFG_DBL, &SUNDOWN},
	    {"SETUP_TO",CFG_DBL, &SETUP_TO},
	    {"STOWALT",	CFG_DBL, &STOWALT},
	    {"STOWAZ",	CFG_DBL, &STOWAZ},
	    {"IGSUN",	CFG_INT, &IGSUN},
	};
	static CfgEntry tcfg2[] = {
	    {"AUTOHOME", CFG_INT, &AUTOHOME},
	    {"HOMEWAIT", CFG_INT, &HOMEWAIT},
#if ENABLE_ROTATING_DOME
	    {"DOMEHOMEWAIT", CFG_INT, &DOMEHOMEWAIT},
#endif
	};
	static  CfgEntry ccfg[] = {
	    {"CAMDIG_MAX", CFG_DBL, &CAMDIG_MAX},
	};
	int n;

	/* read in everything */
	n = readCfgFile (1, tcfn, tcfg, NTCFG);
	if (n != NTCFG) {
	    cfgFileError (tcfn, n, NULL, tcfg, NTCFG);
	    exit(1);
	}
	
	// Read optional "AUTOHOME" value; if exists and non-zero it will force telrun to home on start if needed
	// Also read HOMEWAIT
	AUTOHOME = 0;
	readCfgFile(1, tcfn, tcfg2, NTCFG2);
	
	n = readCfgFile (1, ccfn, ccfg, NCCFG);
	if (n != NCCFG) {
	    cfgFileError (ccfn, n, NULL, ccfg, NCCFG);
	    exit(1);
	}
}

/* make sure camerad is running. if not, start it now, else die.
 * N.B. this is *not* where we build the permanent fifo connection.
 */
static void
init_camerad()
{
	char buf[1024];
	int fd[2];
	int i;

	/* make sure it is running */
	if (testlock_running("camerad") < 0) {
	    if (system ("rund camerad") != 0) {
		daemonLog ("Can not rund camerad");
		exit (1);
	    }
	}

	/* connect up, allowing a little delay if new */
	for (i = 0; i < DWT; i++) {
	    if (cli_conn ("Camera", fd, buf) == 0)
		break;
	    sleep (1);
	}
	if (i == DWT) {
	    daemonLog ("Can not connect to camerad: %s", buf);
	    exit (1);
	}

	/* just close filedes -- we open again officially later */
	close (fd[0]);
	close (fd[1]);
}

/* make sure telescoped is running. if not, start it now, else die.
 * N.B. this is *not* where we build the permanent fifo connection.
 */
static void
init_telescoped()
{
	char buf[1024];
	int fd[2];
	int i;

	/* make sure it is running */
	if (testlock_running("telescoped") < 0) {
	    if (system ("rund telescoped") != 0) {
		daemonLog ("Can not rund telescoped");
		exit (1);
	    }
	}

	/* connect up, allowing a little delay if new */
	for (i = 0; i < DWT; i++) {
	    if (cli_conn ("Tel", fd, buf) == 0)
		break;
	    sleep (1);
	}
	if (i == DWT) {
	    daemonLog ("Can not connect to telescoped: %s", buf);
	    exit (1);
	}

	/* just close filedes -- we open again officially later */
	close (fd[0]);
	close (fd[1]);
}

static void
init_shm()
{
	if (open_telshm(&telstatshmp) < 0) {
	    tlog (NULL, "Shm: %s", strerror(errno));
	    unlockus();
	    exit (1);
	}
}

/* code for main loop */
static void
main_loop()
{
	/* handle pending fifo messages or timeout */
	if (chk_fifos() < 0)
	    all_stop(1);

	/* check on sun */
	checkSun();
	/* don't run if weather alert is in progress */
	if (checkWx() < 0)
	    return;
	/* check for new sls file and abort current if any */
	if (newSLS(scanfile) == 0) {
	    tlog (cscan, "New %s detected", basenm(scanfile));
	    all_stop(0);	/* new file -- can't mark */
	}
	/* check for more work if nothing queued and stopped */
	if (!cscan->running && !cscan->starttm && !chk_pending()) {
	    Scan s;
	    if (findNew (scanfile, &s) == 0) {
		if (newProgram (&s) == 0) {
		    char buf[64];
		    *cscan = s;
		    strcpy (buf, timestamp(s.starttm)); /* save from tlog */
		    tlog (cscan, "Scheduled at %s", buf);
		}
	    }
	}
	/* run all programs, if ok */
	if (IGSUN || !sunisup)
	    runPrograms();
}

/* shut down due to receipt of signal */
static void
sigshutdown(int sig)
{
	tlog (cscan, "Telrun exiting due to receipt of signal %d", sig);

	all_stop(1);

	/* close shutter too if getting SIGHUP or SIGPWR */
	if ((sig == SIGHUP || sig == SIGPWR)
				&& telstatshmp->shutterstate != SH_ABSENT) {
	    tlog (cscan, "Closing dome due to receipt of signal %d", sig);
	    fifoWrite (Dome_Id, "Close");
	}

	unlockus();

	/* consider this normal, mostly for the sake of rund */
	exit (0);
}

/* called periodically to check whether sun is up.
 * set sunisup to 1 if it is, else to 0.
 * if just changed to 1 and !IGSUN also close roof.
 */
static void
checkSun()
{
	static double last_mjd = 0;
	Now *np = &telstatshmp->now;
	int sunwasup;
	Obj o;

	/* reuse last result if not worth recomputing yet */
	if (mjd < last_mjd + 60/SPD)
	    return;
	last_mjd = mjd;

	/* get sun altitude */
	o.o_type = PLANET;
	o.pl.pl_code = SUN;
	(void) obj_cir (np, &o);

	/* set sunisup, and close if just coming up now */
	sunwasup = sunisup;
	sunisup = o.s_alt > -SUNDOWN;
	if (!IGSUN && sunisup && !sunwasup) {
	    tlog (cscan, "Shutting down at dawn");
	    all_stop(1);
	    fifoWrite (Dome_Id, "Close");
	    fifoWrite (Tel_Id, "Alt:%g Az:%g", STOWALT, STOWAZ);
	}
}

/* return -1 if weather alert is in progress, else 0 */
static int
checkWx()
{
	static int last_wxalert;
	int wxvalid = time(NULL) < telstatshmp->wxs.updtime + 30;
	int wxalert = wxvalid && telstatshmp->wxs.alert;

	if (wxalert && !last_wxalert)
	    tlog (NULL, "Weather alert asserted");
	if (!wxalert && last_wxalert)
	    tlog (NULL, "Weather alert rescinded");
	last_wxalert = wxalert;

	return (wxalert ? -1 : 0);
}

/* check all the possible programs and pick one to run the given scan.
 * if find one, add to program[] and return 0, else return -1
 */
static int
newProgram (Scan *sp)
{
	PrFunc p;

	/* select a program to handle this scan */
    switch(sp->ccdcalib.newc) {
        case CT_BIAS:
        	p = pr_new_bias;
        	break;
        case CT_THERMAL:
        	p = pr_new_thermal;
        	break;
        case CT_FLAT:
            p = pr_new_flat;
            break;
        case CT_NONE:
            if(sp->ccdcalib.data != CD_NONE) {
                p = pr_regscan;
            } else {
    	        tlog (sp, "CCDCALIB is a no-op");
        	    return (-1);
            }
            break;
        default:
            p = pr_regscan;
            break;
    }
/*
	if (sp->ccdcalib.newc != CT_NONE)
	    p = pr_bias;
	else if (sp->ccdcalib.data != CD_NONE)
	    p = pr_regscan;
	else {
	    tlog (sp, "CCDCALIB is a no-op");
	    return (-1);
	}
*/
	/* add to program list */
	if (addProgram (p, 0) < 0)
	    return (-1);
	return (0);
}

static void
lockus()
{
	if (lock_running(progname) < 0) {
	    tlog (NULL, "%s: Already running", progname);
	    exit(2);
	}
}

static void
unlockus()
{
	unlock_running (progname, 0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: telrun.c,v $ $Date: 2006/08/18 15:45:47 $ $Revision: 1.5 $ $Name:  $"};
