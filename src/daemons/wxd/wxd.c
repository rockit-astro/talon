/* OCAAS common daemon for weather stations.
 * we support Davis, RainWise and Pete Bros.
 * also Digitemp temp sensors.
 * update telstatshmp and log weather stats when they change.
 * if pressure or temp data look crazy, use defaults.
 * also support getting data from another node by tailing its log file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telstatshm.h"
#include "telenv.h"
#include "running.h"
#include "strops.h"
#include "misc.h"
#include "tts.h"
#include "configfile.h"

#include "dd.h"

#define	LOOPDELAY	2	/* overall pause between reading attempts */
#define	MINLOGDT	1	/* log at least this often, hours */
#define	NOAUXT		-99.99

extern void digitemp(char *tty, WxStats *wp);
extern void createPage (char *tfn, char *ofn, WxStats *wp, double t, double p);

int rflag;
int bflag;

static char *wxtty;
static char *auxtty;
static char *tailfn;
static int dflag;
static int nflag;
static int fflag;
static int lflag;
static int oflag;
static int Rflag;
static int sflag;
static int vflag;
static char *logfile;

static TelStatShm *telstatshmp;

static void usage (char *progname);
static void fake (char *buf, WxStats *wp, double *tp, double *pp);
static void guard (double *tp, double *pp);
static void tailFile (WxStats *wp, double *tp, double *pp);
static void dispense (WxStats *wp, double t, double p);
static void initShm(void);
static void initCfg(void);
static FILE *openLogFile(void);
static void bldWdirstr (WxStats *wp);

static WxStats lastwxs;
static int lastwxs_inited;
static WxStats logwxs;
static double logt;
static double logp;
static double alertMjd;
static double logmjd;

static char wcfn_def[] = "archive/config/wx.cfg";
static char *wcfn = wcfn_def;
static int HAVEWX;
static int MINT, MAXT;
static int MAXH;
static int MAXWS;
static int DELWS;
static int DELWD;
static int DELH;
static int DELR;
static int DELT;
static int DELP;
static int ALRTTM;
static int HAVEAUX;

static char tcfn[] = "archive/config/telsched.cfg";
static double TEMPERATURE;
static double PRESSURE;
static char *fake_alerts;
static char *fake_readings;
static char *utemplate, *uoutput;	/* template and output file names */
static char *geturl;			/* get data from this url */

static char telserverHost[64];
static int telserverPort;

static int telserverUpdate(WxStats *wp, double *t, double *p);
static void connectTelserver(char * host, int port);
static int getTelserverInfo(char *outbuf);
static int myrecv(int fd, char *buf, int len);
static int readsockline(int sockfd, char *buf, int len);

int
main (int ac, char *av[])
{
	char *prog = basenm(av[0]);
	double t, p;
	int osecs = 0;
	WxStats wxs;
	char *str;
	char *cp;

	/* crack the args */
	for (av++; --ac > 0 && *(str = *av) == '-'; av++) {
	    char c;
	    while ((c = *++str) != '\0')
		switch (c) {
		case '1':
		    if (ac < 2)
			usage(prog);
		    osecs = atoi(*++av);
		    ac--;
		    oflag++;
		    break;
		case 'a':
		    if (ac < 2)
			usage(prog);
		    fake_alerts = *++av;
		    ac--;
		    break;
		case 'b':
		    bflag++;
		    break;
		case 'c':
		    if (ac < 2)
			usage(prog);
		    wcfn = *++av;
		    ac--;
		    break;
		case 'd': /* match d or D */
		case 'D':
		    if (nflag || geturl) {
			fprintf (stderr, "Specify only one data source.\n");
			usage(prog);
		    }
		    if (c == 'd') {
		        setDavisModel(DAVIS_WEATHER_MONITOR_II);
		    }
		    else {
		        setDavisModel(DAVIS_VANTAGE_PRO);
		    }
		    dflag++;
		    break;
		case 'f':
		    if (ac < 2)
			usage(prog);
		    fflag++;
		    fake_readings = *++av;
		    ac--;
		    break;
		case 'g':
		    if (dflag || nflag) {
			fprintf (stderr, "Specify only one data source.\n");
			usage(prog);
		    }
		    if (ac < 2)
			usage(prog);
		    geturl = *++av;
		    ac--;
		    break;
		case 'l':
		    if (ac < 2)
			usage(prog);
		    logfile = *++av;
		    ac--;
		    lflag++;
		    break;
		case 'n':
		    if (dflag || geturl) {
			fprintf (stderr, "Specify only one data source.\n");
			usage(prog);
		    }
		    nflag++;
		    break;
		case 'r':
		    rflag++;
		    break;
		case 's':
		    sflag++;
		    break;
		case 't':
		    if (ac < 2)
			usage(prog);
		    tailfn = *++av;
		    ac--;
		    break;
		case 'u':
		    if (ac < 3)
			usage(prog);
		    uoutput = *++av;
		    utemplate = *++av;
		    ac -= 2;
		    break;
		case 'w':
		    if (ac < 2)
			usage(prog);
		    wxtty = *++av;
		    ac--;
		    break;
		case 'v':
		    vflag++;
		    break;
		case 'x':
		    if (ac < 2)
			usage(prog);
		    auxtty = *++av;
		    ac--;
		    break;
		case 'R':
			ac--;
			av++;
			cp = strchr(*av,':');
			if(cp) {
				Rflag++;
				*cp++ = 0;
				strcpy(telserverHost,*av);
				telserverPort = atoi(cp);
				if(telserverPort <=0) {
					usage(prog);
				}
			} else {
				usage(prog);
			}
			break;				
		default:
		    usage (prog);
		    break;
		}
	}
	if (ac > 0)
	    usage (prog);

	/* only one, please */
	if (lock_running (prog) < 0) {
	    daemonLog ("%s: already running\n", prog);
	    exit (0);
	}

	/* initialize */
	if (sflag)
	    initShm();
	if (oflag)
	    (void) alarm (osecs);
	initCfg();
	memset ((void *)&wxs, 0, sizeof(wxs));
	t = TEMPERATURE;
	p = PRESSURE;

	if (!HAVEWX && !HAVEAUX && !fflag && !tailfn && !Rflag) {
	    daemonLog ("No weather station, fake simulation, tailing file, Aux temps, or remote telserver configured\n");
	    exit (0);
	}
	
	if(Rflag) {
		connectTelserver(telserverHost, telserverPort);		// will exit on failure
	}

	/* forever */
	if (fflag) {
	    while (1) {
		fake (fake_readings, &wxs, &t, &p);
		bldWdirstr (&wxs);
		digitemp(NULL, &wxs);
		dispense (&wxs, t, p);
		sleep (1);
	    }
	} else {
	    int (*uf)();
	    int s;

	    /* establish hardware interface, if any */
	    if (!Rflag && HAVEWX) {
		if (dflag)
		    uf = DUpdate;
		else if (nflag)
		    uf = RWUpdate;
		else if (geturl) {
		    uf = URLUpdate;
		    wxtty = geturl;	/* N.B. overload */
		} else
		    uf = PBUpdate;
	    } else
		uf = NULL;
		
	    while (1) {
		s = -1;
		if(Rflag) {
			if(telserverUpdate(&wxs, &t, &p) < 0) break;
			bldWdirstr(&wxs);
			s = 0;
		}
		else {						
			if (uf) {
			    s = (*uf) (wxtty, &wxs, &t, &p);
			    guard (&t, &p);
		    	bldWdirstr (&wxs);
			}
			if (tailfn) {
			    tailFile (&wxs, &t, &p);
		    	bldWdirstr (&wxs);
			    s = 0;
			}
			if (HAVEAUX)
			    digitemp(auxtty, &wxs);
		}			
		if (s == 0)
		    dispense (&wxs, t, p);
		sleep (LOOPDELAY);
	    }
	    daemonLog ("EOF from %s", prog);
	}

	/* we should never die */
	return (1);
}

static void
usage (char *progname)
{
	FILE *fp = stderr;

	fprintf(fp, "%s [options]\n", progname);
	fprintf(fp, "$Revision: 1.10 $\n");
	fprintf(fp, "Purpose: supply weather info to Talon system\n");
	fprintf(fp, "Options:\n");
        fprintf(fp, " -1 n   : wait up to n secs for one good loop then exit\n");
	fprintf(fp, " -a {TCHWR0}: set fake alert code(s) for MaxT, MinT, Hum, Wind, Rain, None\n");
	fprintf(fp, " -b     : print raw lines back from wx station\n");
	fprintf(fp, " -c f   : use alternate wx.cfg; default is %s\n",wcfn_def);
	fprintf(fp, " -d     : operate a Davis Weather Monitor II weather station, else Pete Bros\n");
	fprintf(fp, " -D     : operate a Davis Vantage Pro/Pro2 weather station\n");
	fprintf(fp, " -f s   : create fake settings. s = 'ws wd t h p rain'\n");
	fprintf(fp, " -g u   : get data from url u\n");
	fprintf(fp, " -l f   : append stats to file f (- for stdout)\n");
	fprintf(fp, " -n     : operate a RainWise WS-2000 weather station, else Pete Bros\n");
	fprintf(fp, " -r     : send an initial reset and setup command\n");
	fprintf(fp, " -R <host:port> : connect to telserver at host:port for remotely shared WX data\n");
	fprintf(fp, " -s     : update telstatshm\n");
	fprintf(fp, " -t f   : do not use real hw, tail last line of log file f for new values\n");
	fprintf(fp, " -u o t : store data in file o from template file t.\n");
	fprintf(fp, " -w t   : alternate wx tty. default is in %s\n", basenm(wcfn_def));
	fprintf(fp, " -x t   : alternate aux tty. default is in %s\n", basenm(wcfn));
	fprintf(fp, " -v     : verbose to stdout\n");
	fprintf(fp, "\n");
	fprintf(fp, "Log format, space-separated fields:\n");
	fprintf(fp, "  JD\n");
	fprintf(fp, "  Wind speed, kph\n");
	fprintf(fp, "  Wind direction, degrees E of N\n");
	fprintf(fp, "  Temp, C\n");
	fprintf(fp, "  Humidity, %%\n");
	fprintf(fp, "  Station pressure, mb\n");
	fprintf(fp, "  Rain, mm\n");
	fprintf(fp, "  alert code(s), same as with -a\n");
	fprintf(fp, "  Aux0, C, or %g\n", NOAUXT);
	fprintf(fp, "  Aux1, C, or %g\n", NOAUXT);
	fprintf(fp, "  Aux2, C, or %g\n", NOAUXT);

	exit(1);
}

static void
initShm()
{
	int shmid;
	long addr;

	shmid = shmget (TELSTATSHMKEY, sizeof(TelStatShm), 0666|IPC_CREAT);
	if (shmid < 0) {
	    perror ("shmget");
	    exit (1);
	}

	addr = (long) shmat (shmid, (void *)0, 0);
	if (addr == -1) {
	    perror ("shmat");
	    exit (1);
	}

	telstatshmp = (TelStatShm *) addr;
}

/* create fake settings from buf
 */
static void
fake (char *buf, WxStats *wp, double *tp, double *pp)
{
	double rainmm;

	if (6 != sscanf (buf, "%d %d %lf %d %lf %lf", &wp->wspeed, &wp->wdir,
					    tp, &wp->humidity, pp, &rainmm)) {
	    daemonLog ("Bad fake format: Wspd Wdir temp hum pre rain");
	    exit (1);
	}
	wp->rain = rainmm * 10;
}

/* filter out preposterous values
 */
static void
guard (double *tp, double *pp)
{
	if (*tp < -40 || *tp > 80)
	    *tp = TEMPERATURE;
	if (*pp < 600 || *pp > 1200)
	    *pp = PRESSURE;
}

/* open tailfn, read last last, set stats.
 * N.B. we assume tailfn is set and in log format.
 */
static void
tailFile (WxStats *wp, double *tp, double *pp)
{
	static int tailfd;
	char buf[1024];
	double jd, rain;
	long n;

	/* open first time only */
	if (tailfd <= 0) {
	    tailfd = open (tailfn, O_RDONLY);
	    if (tailfd < 0) {
		daemonLog ("%s: %s\n", tailfn, strerror(errno));
		tailfn = 0;
		return;
	    }
	}

	/* seek to end, then back to first \n to read last line */
	n = lseek (tailfd, 0L, SEEK_END);
	if (n == (off_t)-1) {
	    daemonLog ("%s lseek: %s\n", tailfn, strerror(errno));
	    tailfn = 0;
	    return;
	}
	do {
	    if (lseek (tailfd, -2L, SEEK_CUR) < 0) {
		daemonLog ("%s lseek: %s\n", tailfn, strerror(errno));
		tailfn = 0;
		return;
	    }
	    if (read (tailfd, buf, 1) < 0) {
		daemonLog ("%s read: %s\n", tailfn, strerror(errno));
		tailfn = 0;
		return;
	    }
	} while (buf[0] != '\n' && --n > 1);
	read (tailfd, buf, sizeof(buf));

	fake_alerts = (char *) malloc(5*sizeof(char));
	/* crack */
	sscanf (buf, "%lf %d %d %lf %d %lf %lf %5c", &jd, &wp->wspeed, &wp->wdir,
						tp, &wp->humidity, pp, &rain, fake_alerts);
	wp->rain = rain*10 + .5;
	wp->updtime = (jd-MJD0-25567.50)*SPD + .5;
#if 0
	fprintf (stderr, "%s %d %d %g %d %g %d\n", ctime(&wp->updtime),
			wp->wspeed, wp->wdir, *tp, wp->humidity, *pp, wp->rain);
#endif
}

/* do something with the data */
static void
dispense (WxStats *wp, double t, double p)
{
	double Mjd = mjd_now();

	/* first time save current as last to avoid spurious alerts */
	if (!lastwxs_inited) {
	    lastwxs = *wp;
	    lastwxs_inited = 1;
	}

	/* determine alert status */
	wp->alert = 0;
	if (fake_alerts || tailfn) {
	    if (strchr (fake_alerts, 'T')) wp->alert |= WXA_MAXT;
	    if (strchr (fake_alerts, 'C')) wp->alert |= WXA_MINT;
	    if (strchr (fake_alerts, 'H')) wp->alert |= WXA_MAXH;
	    if (strchr (fake_alerts, 'W')) wp->alert |= WXA_MAXW;
	    if (strchr (fake_alerts, 'R')) wp->alert |= WXA_RAIN;
	} else {
	    if (wp->auxtmask) {
		if ((wp->auxtmask&0x1) && wp->auxt[0]>MAXT) wp->alert|=WXA_MAXT;
		if ((wp->auxtmask&0x1) && wp->auxt[0]<MINT) wp->alert|=WXA_MINT;
		if ((wp->auxtmask&0x2) && wp->auxt[1]>MAXT) wp->alert|=WXA_MAXT;
		if ((wp->auxtmask&0x2) && wp->auxt[1]<MINT) wp->alert|=WXA_MINT;
		if ((wp->auxtmask&0x4) && wp->auxt[2]>MAXT) wp->alert|=WXA_MAXT;
		if ((wp->auxtmask&0x4) && wp->auxt[2]<MINT) wp->alert|=WXA_MINT;
	    }
	    if (t > MAXT) wp->alert |= WXA_MAXT;
	    if (t < MINT) wp->alert |= WXA_MINT;
	    if (wp->humidity > MAXH) wp->alert |= WXA_MAXH;
	    if (wp->wspeed > MAXWS) wp->alert |= WXA_MAXW;
	    if (wp->rain > lastwxs.rain) wp->alert |= WXA_RAIN;
	}
	if (wp->alert) {
	    alertMjd = Mjd;
	    if (!lastwxs.alert) {
		toTTS ("Weather alert!");
		if (wp->alert & WXA_MAXT)
		    toTTS ("Temperature exceeds maximum of %d degrees C.",MAXT);
		if (wp->alert & WXA_MINT)
		    toTTS("Temperature is below minimum of %d degrees C.",MINT);
		if (wp->alert & WXA_MAXH)
		    toTTS ("Humidity exceeds maximum of %d percent.", MAXH);
		if (wp->alert & WXA_MAXW)
		    toTTS ("Wind speed exceeds maximum of %d K-P-H.", MAXWS);
		if (wp->alert & WXA_RAIN)
		    toTTS ("Precipitation has been detected.");
	    }
	} else {
	    if (Mjd <= alertMjd + ALRTTM/1440.)
		wp->alert = lastwxs.alert;   /* retain until gone for ALRTTM */
	    if (!wp->alert && lastwxs.alert)
		toTTS ("Weather alert has been cancelled.");
	}

	/* print if vflag */
	if (vflag) {
	    /* much like shm/telshow */
	    printf ("%s MJD = %13.5f; Wind = %3d km/h @ %-3d; T = %5.1fC; H = %3d%%; P = %6.1f mB; Rain= %5.1f mm\n",
			wp->alert ? "ALERT" : "     ", Mjd+MJD0,
			wp->wspeed, wp->wdir, t, wp->humidity, p, wp->rain/10.);
	    
	    if (wp->auxtmask) {
		char buf1[128];
		int i = sprintf (buf1, "AuxTemp :");

		if (wp->auxtmask & 0x1)
		    i += sprintf (buf1+i, " Aux0 = %7.2fC   ", wp->auxt[0]);
		if (wp->auxtmask & 0x2)
		    i += sprintf (buf1+i, " Aux1 = %7.2fC   ", wp->auxt[1]);
		if (wp->auxtmask & 0x4)
		    i += sprintf (buf1+i, " Aux2 = %7.2fC   ", wp->auxt[2]);
		printf ("%s\n", buf1);
	    }
	}

	/* log if enabled and (stale or conditions differ sufficiently) */
	if (lflag &&
		(Mjd > logmjd + (MINLOGDT/24.0) ||
		abs(wp->wspeed - logwxs.wspeed) >= DELWS ||
		abs(wp->wdir - logwxs.wdir) >= DELWD ||
		wp->rain - logwxs.rain >= DELR*10 ||
		wp->rain < logwxs.rain || /* got reset */
		abs(wp->humidity - logwxs.humidity) >= DELH ||
		fabs(t - logt) >= DELT ||
		fabs(p - logp) >= DELP ||
		((wp->auxtmask&0x1) && fabs(wp->auxt[0]-logwxs.auxt[0])>=DELT)||
		((wp->auxtmask&0x2) && fabs(wp->auxt[1]-logwxs.auxt[1])>=DELT)||
		((wp->auxtmask&0x4) && fabs(wp->auxt[2]-logwxs.auxt[2])>=DELT)||
		wp->alert != logwxs.alert)) {

	    FILE *logfp = openLogFile();

	    if (logfp) {
		/* N.B. must be same format as cracked in tailFile() */
		fprintf (logfp, "MJD = %13.5f; Wind = %3d km/h @ %-3d; T = %5.1fC; H = %3d%%; P = %6.1f mB; Rain = %5.1f mm; Alarm =",
			Mjd+MJD0, wp->wspeed, wp->wdir, t,
			wp->humidity, p, wp->rain/10.);

		fprintf (logfp, "%c", (wp->alert & WXA_MAXT) ? 'T' : '-');
		fprintf (logfp, "%c", (wp->alert & WXA_MINT) ? 'C' : '-');
		fprintf (logfp, "%c", (wp->alert & WXA_MAXH) ? 'H' : '-');
		fprintf (logfp, "%c", (wp->alert & WXA_MAXW) ? 'W' : '-');
		fprintf (logfp, "%c", (wp->alert & WXA_RAIN) ? 'R' : '-');

		fprintf (logfp, " %6.2f %6.2f %6.2f",
				(wp->auxtmask&0x1) ? wp->auxt[0] : NOAUXT,
				(wp->auxtmask&0x2) ? wp->auxt[1] : NOAUXT,
				(wp->auxtmask&0x4) ? wp->auxt[2] : NOAUXT);

		fprintf (logfp, "\n");
		fflush (logfp);
		if (logfp != stdout)
		    (void) fclose (logfp);
		logwxs = *wp;
		logt = t;
		logp = p;
		logmjd = Mjd;
	    }
	}

	if (sflag) {
	    /* advertise fresh stuff in shared mem */
	    wp->updtime = time(NULL);
	    telstatshmp->now.n_temp = t;
	    telstatshmp->now.n_pressure = p;
	    telstatshmp->wxs = *wp;
	}

	if (oflag)
	    exit(0);

	if (utemplate)
	    createPage (utemplate, uoutput, wp, t, p);

	/* save for next comparison */
	lastwxs = *wp;
}

/* read config files */
static void
initCfg()
{
#define	NWCFG	(sizeof(wcfg)/sizeof(wcfg[0]))
#define	NTCFG	(sizeof(tcfg)/sizeof(tcfg[0]))
	static char cfgwxtty[32];
	static char cfgauxtty[32];
	static CfgEntry wcfg[] = {
	    {"HAVEWX",	CFG_INT, &HAVEWX},
	    {"MINT",	CFG_INT, &MINT},
	    {"MAXT",	CFG_INT, &MAXT},
	    {"MAXH",	CFG_INT, &MAXH},
	    {"MAXWS",	CFG_INT, &MAXWS},
	    {"DELWS",	CFG_INT, &DELWS},
	    {"DELWD",	CFG_INT, &DELWD},
	    {"DELH",	CFG_INT, &DELH},
	    {"DELR",	CFG_INT, &DELR},
	    {"DELT",	CFG_INT, &DELT},
	    {"DELP",	CFG_INT, &DELP},
	    {"ALRTTM",	CFG_INT, &ALRTTM},
	    {"WXTTY",	CFG_STR, cfgwxtty, sizeof(cfgwxtty)},

	    {"HAVEAUX",	CFG_INT, &HAVEAUX},
	    {"AUXTTY",	CFG_STR, cfgauxtty, sizeof(cfgauxtty)},
	};
	static CfgEntry tcfg[] = {
	    {"TEMPERATURE", CFG_DBL, &TEMPERATURE},
	    {"PRESSURE",    CFG_DBL, &PRESSURE},
	};
	int n;

	if (vflag)
	    printf ("Reading %s\n", wcfn);
	n = readCfgFile (1, wcfn, wcfg, NWCFG);
	if (n != NWCFG) {
	    cfgFileError (wcfn, n, NULL, wcfg, NWCFG);
	    exit (1);
	}
	n = readCfgFile (1, tcfn, tcfg, NTCFG);
	if (n != NTCFG) {
	    cfgFileError (tcfn, n, NULL, tcfg, NTCFG);
	    exit (1);
	}

	/* use config values if not set on command line */
	if (!wxtty)
	    wxtty = cfgwxtty;
	if (!auxtty)
	    auxtty = cfgauxtty;

}

/* open the log file for append.
 * support - as stdout.
 */
static FILE *
openLogFile()
{
	FILE *logfp;

	if (!logfile || strcmp (logfile, "-") == 0)
	    logfp = stdout;
	else
	    logfp = fopen (logfile, "a");
	if (!logfp)
	    daemonLog ("%s: %s\n", logfile, strerror(errno));
	return (logfp);
}

static void
bldWdirstr (WxStats *wp)
{
	if (wp->wdir >= 0 && wp->wdir < 360) {
	    (void) strncpy (wp->wdirstr, cardDirName (degrad(wp->wdir)),
							sizeof(wp->wdirstr)-1);
	} else
	    (void) sprintf (wp->wdirstr, "%d", wp->wdir);
}

/* ------------------------------------------

  Get 'wx' updates from telserver -- allows for shared
  use of a single weather station across different systems

------------------------------------------------ */

static int sockfd = -1;

static int telserverUpdate(WxStats *wp, double *tp, double *pp)
{
	char buf[1024];
	double jd, rain;
	
	if(getTelserverInfo(buf) < 0) { // assumes len is large enough
		daemonLog("Exit due to telserver read error\n");
		return -1;
	}

	/* crack */
	sscanf (buf, "%lf %d %d %lf %d %lf %lf", &jd, &wp->wspeed, &wp->wdir,
						tp, &wp->humidity, pp, &rain);
	wp->rain = rain*10 + .5;
	wp->updtime = (jd-MJD0-25567.50)*SPD + .5;
	return 0;
}

/*
 * Connect to the telserver. Use sockfd to hold resulting connected socket
 */
static void connectTelserver(char * host, int port)
{
	struct sockaddr_in cli_socket;
	struct hostent *hp;
	int len;

	/* get host name running server */
	if (!(hp = gethostbyname(host))) {
		daemonLog("Error resolving host: %s\n",strerror(errno));		
	    exit(1);
	}

	/* create a socket */
	if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
		daemonLog("Error creating socket: %s\n",strerror(errno));		
	    exit(1);
	}

	/* connect to the server */
	len = sizeof(cli_socket);
	memset (&cli_socket, 0, len);
	cli_socket.sin_family = AF_INET;
	cli_socket.sin_addr.s_addr =
			    ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
	cli_socket.sin_port = htons(port);
	if (connect (sockfd, (struct sockaddr *)&cli_socket, len) < 0) {
		daemonLog("Error connecting to telserver: %s\n",strerror(errno));
	    exit(1);
	}
	
	/* ready */
	if (vflag) {
	    daemonLog ("Connected to telserver at %s:%d\n", host,port);
	    daemonLog ("sockfd = %d\n",sockfd);
	}
}

/*
 * Get WX related information from telserver over sockfd
 * and translate into useable form into buf
 */
static int getTelserverInfo(char *outbuf)
{
	char buf[1024];
	double TEMP,PRESS,RAIN;
	char parse[128];
	double TIME;
	int WSPD,WDIR,HUM;
	char WDIRSTR[8];
	int ALERT;
	double AUX0,AUX1,AUX2;
	char *tp;
	// STO 2003-01-18: Modified to allow occasional missed read,
	// by simply reporting the data from last time
	static char lastLine[1024];
	static int skips;
	int maxSkips = 15;
	
	// send the telserver the request
	strcpy(buf,"GetInfo AirTemp AirPressure WeatherInfo\r\n");
	
	if(strlen(buf) > send(sockfd,buf,strlen(buf),MSG_NOSIGNAL)) {
		daemonLog("Error sending command through socket: %s\n",strerror(errno));
		return -1;
	}
		
	if(0 >= readsockline(sockfd,buf,sizeof(buf))) {
		daemonLog("Error reading from socket (0)\n");
		return -1;
	}
		
	if(strcmp(buf,"*>>>")) {
		daemonLog("Invalid block start returned from telserver (%s)\n",buf);
		//return -1;
		goto skip;
	}
	
	if(0 >= readsockline(sockfd,buf,sizeof(buf))) {
		daemonLog("Error reading from socket (1)\n");
		return -1;
	}

	TEMP = atof(buf);
	if(0 >= readsockline(sockfd,buf,sizeof(buf))) {
		daemonLog("Error reading from socket (2)\n");
		return -1;
	}
	PRESS = atof(buf);
	if(0 >= readsockline(sockfd,buf,sizeof(buf))) {
		daemonLog("Error reading from socket (3)\n");
		return -1;
	}
	
	// another check for skip...
	if(!TEMP && !PRESS) goto skip;
	
	tp = strtok(buf," ");
	if(!tp) goto skip;
	strcpy(parse,tp ? tp : "0");
	WSPD = atoi(parse);
	tp = strtok(NULL," ");
	if(!tp) goto skip;
	strcpy(parse,tp ? tp : "0");
	WDIR = atoi(parse);
	tp = strtok(NULL," ");
	if(!tp) goto skip;
	strcpy(WDIRSTR,tp ? tp : "--");
	tp = strtok(NULL," ");
	if(!tp) goto skip;
	strcpy(parse,tp ? tp : "0");
	HUM = atoi(parse);
	tp = strtok(NULL," ");
	if(!tp) goto skip;
	strcpy(parse,tp ? tp : "0");
	RAIN = atof(parse);
	tp = strtok(NULL," ");
	if(!tp) goto skip;
	strcpy(parse,tp ? tp : "0");
	ALERT = atoi(parse);
	tp = strtok(NULL," ");
	if(!tp) goto skip;
	strcpy(parse,tp ? tp : "0");
	AUX0 = atof(parse);
	tp = strtok(NULL," ");
	if(!tp) goto skip;
	strcpy(parse,tp ? tp : "0");
	AUX1 = atof(parse);
	tp = strtok(NULL," ");
	if(!tp) goto skip;
	strcpy(parse,tp ? tp : "0");
	AUX2 = atof(parse);
	tp = strtok(NULL," ");
	if(!tp) goto skip;
	strcpy(parse,tp ? tp : "0");
	TIME = atof(parse);
	
	// mjd was 25567.5 on 00:00:00 1/1/1970 UTC (UNIX' epoch)
	// jd was 2440587.5
	TIME = 2440587.5 + TIME / 86400; // jd + timestamp / secsPerDay
	RAIN /= 10; // decimal shift
	
	sprintf(outbuf,"%7.5f %3.3d %3.3d %2.1f %2.2d %3.1f %4.1f %c%c%c%c%c %2.2f %2.2f %2.2f\n",
			TIME,WSPD,WDIR,TEMP,HUM,PRESS,RAIN,
			(ALERT & 1 ? 'T' : '-'),
			(ALERT & 2 ? 'C' : '-'),
			(ALERT & 4 ? 'H' : '-'),
			(ALERT & 8 ? 'W' : '-'),
			(ALERT & 16 ? 'R' : '-'),
			AUX0,AUX1,AUX2);

    // read closing info
	if(0 >= readsockline(sockfd,buf,sizeof(buf))) {
		daemonLog("Error reading from socket (0)\n");
		return -1;
	}
		
	if(strcmp(buf,"*<<<")) {
		daemonLog("Invalid block end returned from telserver (%s)\n",buf);
		//return -1;
		goto skip;
	}
	
	// record in case we skip later
	skips = 0;		
	strcpy(lastLine,outbuf);
	
	return(strlen(outbuf));

skip:	
// STO: Skipped...	
	if(++skips > maxSkips) {
		daemonLog("Telserver data skipped more than %d consecutive times\n",maxSkips);
		return -1;
	}
	// use the last line of data again for an allowed skip
	strcpy(outbuf,lastLine);
	return strlen(outbuf);					
}

// receive, w/timeout.  return true/false
static int myrecv(int fd, char *buf, int len)
{
	int rb;
	int err;

	fd_set rfds;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	tv.tv_sec = 10;
	tv.tv_usec = 0;

	err = select(fd+1, &rfds, NULL, NULL, &tv);
	if(err <= 0) {
		daemonLog("Timeout on recv for socket %d",fd);
		return 0;
	}

	rb = read(fd,buf,len);		// use read instead of recv... seems to be the CLOSE_WAIT fix!
	
	return (rb==len);
}

// read up to the next newline (\r\n)
// return number of characters returned or -1 for error
static int readsockline(int sockfd, char *buf, int len)
{
	int cnt = 0;
	char ch;

	while(cnt < len) {
		if(!myrecv(sockfd,&ch,1)) {
			return -1;
		}
		if(ch >= ' ') {
			buf[cnt++] = ch;
		}
		else if(ch == '\n') {
			buf[cnt++] = 0;
			return cnt;
		}
	}	
	buf[len-1] = 0;
	return len;
}		


/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: wxd.c,v $ $Date: 2003/01/19 00:15:08 $ $Revision: 1.10 $ $Name:  $"};
