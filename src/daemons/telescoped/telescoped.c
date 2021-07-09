/* This process listens to several FIFO pairs for generic telescope
 * and focus commands and manipulates CSIMCs accordingly.
 * The fifo names all end with ".in" and ".out" which are with respect to us,
 * the server. Several config files establish several options and parameters.
 *
 * All commands and responses are ASCII strings. All commands will get at
 * least one response. Responses are in the form of a number, a space, and
 * a short description. Numbers <0 indicate fatal errors; 0 means the command
 * is complete; numbers >0 are intermediate progress messages. When a response
 * number <= 0 is returned, there will be no more. It is up to the clients to
 * wait for a completion response; if they just send another command the
 * previous command and its responses will be dropped forever.
 *
 * FIFO pairs:
 *   Tel	telescope axes, field rotator
 *   Focus	desired focus motion, microns as per focus.cfg
 *
 * v0.1	10/28/93 First draft: Elwood C. Downey
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "strops.h"
#include "telstatshm.h"
#include "running.h"
#include "csimc.h"
#include "misc.h"
#include "telenv.h"

#include "teled.h"

TelStatShm *telstatshmp;	/* shared telescope info */
int virtual_mode = 0;			/* non-zero for virtual mode enabled */

char tscfn[] = "archive/config/telsched.cfg";
char tdcfn[] = "archive/config/telescoped.cfg";
char hcfn[] = "archive/config/home.cfg";
char ocfn[] = "archive/config/focus.cfg";
char ccfn[] = "archive/config/cover.cfg";

static void usage (void);
static void init_all(void);
static void allreset(void);
static void init_shm(void);
static void init_tz(void);
static void on_sig(int fake);
static void main_loop(void);

static char logdir[] = "archive/logs";
static char *progname;

// Global values read from config
double STOWALT, STOWAZ;

int
main (ac, av)
int ac;
char *av[];
{
    char *str;

    progname = basenm(av[0]);

    /* crack arguments */
    for (av++; --ac > 0 && *(str = *av) == '-'; av++) {
        char c;
        while ((c = *++str) != '\0')
            switch (c) {
            case 'h':	/* no hardware: legacy syntax */
            	//ICE
            	virtual_mode = 0;
            	printf("CSI Hardware Mode\n");
            	//ICE
            	break;
            case 'v':	/* same thing, but mnemonic to new name */
            	//ICE
            	virtual_mode = 1;
                printf("Virtual Mode\n");
                //ICE
                break;
            default:
                usage();
                break;
            }
    }

    /* now there are ac remaining args starting at av[0] */
    if (ac > 0)
        usage();

    /* only ever one */
    if (lock_running(progname) < 0) {
        tdlog ("%s: Already running", progname);
        exit(0);
    }

    /* init all subsystems once */
    init_all();

    /* go */
    main_loop();

    /* should never get here */
    return (1);
}

/* write a log message to stdout with a time stamp.
 * N.B. if fmt doesn't end with \n we add it.
 */
void
tdlog (char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    int l;

    /* start with time stamp */
    l = sprintf (buf, "%s INFO ", timestamp(time(NULL)));

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
}

/* stop the telescope then exit */
void
die()
{
    tdlog ("die()!");
    allstop();
    close_fifos();
    unlock_running (progname, 0);
    exit (0);
}

/* tell everybody to stop */
void
allstop()
{
    tel_msg ("Stop");
    focus_msg ("Stop");
}

/* read the config files for variables we use here */
void
init_cfg()
{
#define NTSCFG  (sizeof(tscfg)/sizeof(tscfg[0]))
    static double LONGITUDE, LATITUDE, TEMPERATURE, PRESSURE, ELEVATION;
    static CfgEntry tscfg[] = {
        {"STOWALT",		CFG_DBL, &STOWALT},
        {"STOWAZ",		CFG_DBL, &STOWAZ},
        {"LONGITUDE",	CFG_DBL, &LONGITUDE},
        {"LATITUDE",	CFG_DBL, &LATITUDE},
        {"TEMPERATURE",	CFG_DBL, &TEMPERATURE},
        {"PRESSURE",	CFG_DBL, &PRESSURE},
        {"ELEVATION",	CFG_DBL, &ELEVATION},
    };

    Now *np = &telstatshmp->now;
    int n;

    n = readCfgFile (1, tscfn, tscfg, NTSCFG);
    if (n != NTSCFG) {
        cfgFileError (tscfn, n, (CfgPrFp)tdlog, tscfg, NTSCFG);
// Don't die...	    die();
    }

    /* basic defaults if no GPS or weather station */
    lng = -LONGITUDE;		/* we want rads +E */
    lat = LATITUDE;			/* we want rads +N */
    temp = TEMPERATURE;		/* we want degrees C */
    pressure = PRESSURE;		/* we want mB */
    elev = ELEVATION/ERAD;		/* we want earth radii*/

#undef NTSCFG
}

static void
main_loop()
{
  while (1) {
    chk_fifos();
  }
}

/* tell everybody to reset */
static void
allreset()
{
    tel_msg ("Reset");
    focus_msg ("Reset");
    cover_msg("Reset");
    init_cfg();	/* even us */
}

/* print a usage message and exit */
static void
usage ()
{
    fprintf (stderr, "%s: [options]\n", progname);
    fprintf (stderr, " -v: (or -h) run in virtual mode w/o actual hardware attached.\n");
    exit (1);
}

/* initialize all the various subsystems.
 * N.B. call this only once.
 */
static void
init_all()
{
    /* connect to the telstatshm segment */
    init_shm();

    /* divine timezone */
    init_tz();

    /* always want local apparent place */
    telstatshmp->now.n_epoch = EOD;

    /* no guiding */
    telstatshmp->jogging_ison = 0;

    /* init csimcd */
    csiInit();

    /* connect the signal handlers */
    signal (SIGINT, on_sig);
    signal (SIGTERM, on_sig);
    signal (SIGHUP, on_sig);

    /* don't get signal if write to fifo fails */
    signal (SIGPIPE, SIG_IGN);

    /* create the fifos to announce we are fully ready */
    init_fifos();

    /* initialize config files and hardware subsystem  */
    allreset();
}

/* create the telstatshmp shared memory segment */
static void
init_shm()
{
    int len = sizeof(TelStatShm);
    int shmid;
    long addr;
    int new;

    /* open/create */
    tdlog("shm len=%d", len);
    new = 0;
    shmid = shmget (TELSTATSHMKEY, len, 0664);
    if (shmid < 0) {
        if (errno == ENOENT)
            shmid = shmget (TELSTATSHMKEY, len, 0664|IPC_CREAT);
        if (shmid < 0) {
            tdlog ("shmget: %s", strerror(errno));
            exit (1);
        }
        new = 1;
    }

    /* connect */
    addr = (long) shmat (shmid, (void *)0, 0);
    if (addr == -1) {
        tdlog ("shmat: %s", strerror(errno));
        exit (1);
    }

    /* always zero when we start */
    memset ((void *)addr, 0, len);

    /* handy */
    telstatshmp = (TelStatShm *) addr;

    /* store the PID of this process */
    telstatshmp->telescoped_pid = getpid();
}

static void
init_tz()
{
    Now *np = &telstatshmp->now;
    time_t t = time(NULL);
    struct tm *gtmp, *ltmp;
    double gmkt, lmkt;


    gtmp = gmtime (&t);
    gtmp->tm_isdst = 0;	/* _should_ always be 0 already */
    gmkt = (double) mktime (gtmp);

    ltmp = localtime (&t);
    ltmp->tm_isdst = 0;	/* let mktime() figure out zone */
    lmkt = (double) mktime (ltmp);

    tz = (gmkt - lmkt) / 3600.0;
}

static void
on_sig(int signo)
{
    tdlog ("Received signal %d", signo);
    die();
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: telescoped.c,v $ $Date: 2002/10/23 21:44:13 $ $Revision: 1.2 $ $Name:  $"};
