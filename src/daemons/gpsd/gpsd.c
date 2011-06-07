/* program to continuously read the NMEA GPRMC data sentence and extract
 * time, lat and long. Must be root in order to actually set the time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <limits.h>
#include <unistd.h>
#include <netdb.h>
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
#include "misc.h"
#include "strops.h"
#include "running.h"
#include "configfile.h"
#include "telstatshm.h"
#include "telenv.h"


#define	NSETSKIPS	60			/* secs between clock updates */
#define	MAXLTERR	degrad(30./3600.)	/* max lat jitter */
#define	MAXLGERR	degrad(30./3600.)	/* max longitude jitter*/

static char gcfn[] = "archive/config/gpsd.cfg";
static char tcfn[] = "archive/config/telsched.cfg";
static char ltname[] = "LATITUDE";
static char lgname[] = "LONGITUDE";

static char *gpstty;

static int aflag;
static int cflag;
static int fflag;
static int hflag;
static int iflag;
static int oflag;
static int sflag;
static int vflag;
static int Vflag;
static int xflag;
static int zflag;
static int before;
static int rflag;

static char telserverHost[64];
static int telserverPort;

static char *extCmd;
static double offsetSecs = 0.0;
static int delaySecs = 0;

static int timesetonce;

static TelStatShm *telstatshmp;

static void usage (char *progname);
static int openGps();
static int processLine (char *line);
static int freshLine (int fd, char *buf, int len);
static void ttySync(int fd);
static void initShm(void);
static void havegps(void);
static void setTty (void);
static void setTime (double Mjd);
static int llIsOff(double lt, double lg);
static void setCfg(double lt, double lg);
static void delay(long dsecs);
static void delayshort(long msecs);

static void connectTelserver(char * host, int port);
static int getTelserverInfo(char *outbuf);
static int myrecv(int fd, char *buf, int len);
static int readsockline(int sockfd, char *buf, int len);

int
main (int ac, char *av[])
{
	char *prog = basenm(av[0]);
	char buf[1024];
	int gpsfd;
	int osecs = 0;
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
		    aflag++;
		    break;
		case 'c':
		    cflag++;
		    break;
		case 'f':
		    fflag++;
		    break;
		case 'h':
			hflag++;
			break;		
		case 'i':
		    iflag++;
		    break;
		case 's':
		    sflag++;
		    break;
		case 't':
		    if (ac < 2)
			usage(prog);
		    gpstty = *++av;
		    ac--;
		    break;
		case 'v':
		    vflag++;
		    break;
		case 'N':
		    Vflag++;
		    break;
		case 'w':
			if(ac < 2) usage(prog);
			delaySecs = atol(*++av);
			if(delaySecs < 0 || delaySecs > 1000) {
				daemonLog("Invalid delay %ld (0-1000 allowed)");		
				exit(1);
			}
			ac--;
			break;				
		case 'x':
			xflag++;
			if(ac < 2) usage(prog);
			extCmd = *++av;
			ac--;
			break;
		case 'z':			
			zflag++;
			if(ac < 2) usage(prog);
			offsetSecs = atod(*++av);
			ac--;
			break;
		case 'r':
			ac--;
			av++;
			cp = strchr(*av,':');
			if(cp) {
				rflag++;
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

	if ((aflag || fflag) && geteuid() != 0) {
	    daemonLog ("Must be root to use -a or -f");
	    exit (0);
	}
	
	if(zflag && !aflag && !fflag) {
		daemonLog ("Must use -a or -f to use -z");
	}
	
	if(offsetSecs > 1000.0 || offsetSecs < -1000.0) {
		daemonLog("-z Offsets must be within 1000 seconds");
	}		

	havegps();
	
	if (sflag)
	    initShm();
	if (!gpstty)
	    setTty();
	if (oflag)
	    (void) alarm (osecs);

	if (lock_running(prog) < 0) {
	    printf ("%s: Already running\n", prog);
	    exit(0);
	}

	/* open device and go forever */
	gpsfd = openGps();
    ttySync(gpsfd); // do initial flush, sync
	while (freshLine (gpsfd, buf, sizeof(buf)) == 0) {
	    if(0 == processLine (buf)) {	
            ttySync(gpsfd); // flush after successful read to resynch
    	    delay(delaySecs);
        }
        else {
            if(Vflag) { // diagnostic: show non-parsed lines
                int eb = 0;
                while(buf[eb] >=32 && eb < sizeof(buf)-1) eb++;
                buf[eb] = 0;
                daemonLog("*** read: %s\n",buf);
            }
        }
   	    if(!delaySecs && rflag) { // impose a delay on telserver regardless
   	    	delayshort(900);
   	    }
	}		

	/* we should never die */
	daemonLog ("EOF from %s\n", prog);
	return (1);
}

/* Wait for a time */
static void delay(long dsecs)
{
/*
	long secs;
	struct timeval tv;
	
	if(!dsecs) return;

	gettimeofday(&tv,NULL);
	secs = tv.tv_sec;
	while(tv.tv_sec - secs < dsecs) {
		gettimeofday(&tv,NULL);
	}
	
*/
	struct timeval tv;

	errno = 0;			
	tv.tv_sec = dsecs;
	tv.tv_usec = 1000;
	select(0,NULL,NULL,NULL,&tv);	
	errno = 0;	
}

/* Wait for a time < 1 sec*/
static void delayshort(long msecs)
{
/*
	long usecs;
	long usecset;
	struct timeval tv;
		
	if(!msecs) return;
	
	usecset = msecs*1000;
		

	gettimeofday(&tv,NULL);
	usecs = tv.tv_sec * 1000000 + tv.tv_usec;
	while((tv.tv_sec * 1000000 + tv.tv_usec) - usecs < usecset) {
		gettimeofday(&tv,NULL);
	}
*/
	struct timeval tv;
	
	errno = 0;
	tv.tv_sec = 0;
	tv.tv_usec = 1000*msecs;
	select(0,NULL,NULL,NULL,&tv);	
	errno = 0;
}


static void
usage (char *progname)
{
	FILE *fp = stderr;

	fprintf(fp, "%s: [options]\n", progname);
	fprintf(fp, " -1 n : wait up to n secs for one good loop then exit\n");
	fprintf(fp, " -a   : smoothly adjust OS time (must be root)\n");
	fprintf(fp, " -c   : update location one time in %s\n", basenm(tcfn));
	fprintf(fp, " -f   : rudely force OS time first time (must be root)\n");
	fprintf(fp, " -h   : rudely adjust OS time EACH time (not recommended) (root only)\n");
	fprintf(fp, " -i   : report _something_ even if invalid\n");
	fprintf(fp, " -r host:port connect to telserver at host:port for remotely shared GPS data (overrides tty and HAVEGPS)\n");
	fprintf(fp, " -s   : update lat/long in telstatshm once\n");
	fprintf(fp, " -t t : alternate tty. default is from %s\n",basenm(gcfn));
	fprintf(fp, " -v   : verbose\n");
    fprintf(fp, " -N   : Output non-parsed lines (diagnostic)\n");
    fprintf(fp, " -w s  : delay 's' seconds between updates (default is 0)\n");
	fprintf(fp, " -x cmd   : external script source for GPS info (overrides tty and HAVEGPS)\n");
	fprintf(fp, " -z n.nnn : offset time received by n.nnn seconds\n");

	exit(1);
}

/* open gps connection or exit */
static int
openGps()
{
	struct termios tio;
	int fd;
	
	/* If we are running a telserver, open and return 0 (to ignore) */
	if(rflag) {
		connectTelserver(telserverHost, telserverPort);		// will exit on failure
		return (0);
	}		
	
	/* If we are running an external script, simply return */
	if(xflag) {
		return(0);		
	}
	
	/* open connection */
	fd = telopen (gpstty, O_RDONLY|O_NONBLOCK);
	if (fd < 0) {
	    daemonLog ("open %s: %s\n", gpstty, strerror(errno));
	    exit(1);
	}
	if (fcntl (fd, F_SETFL, 0 /* no NONBLOCK now */ ) < 0) {
	    daemonLog ("fcntl %s: %s\n", gpstty, strerror(errno));
	    exit(1);
	}

	/* set line characteristics */
	if (tcgetattr (fd, &tio) < 0) {
	    daemonLog ("tcgetattr %s: %s\n", gpstty, strerror(errno));
	    exit(1);
	}
	tio.c_iflag = IGNCR;
	tio.c_cflag = CS8 | CREAD | CLOCAL;
	tio.c_lflag = ICANON;
	cfsetispeed (&tio, B4800);
	if (tcsetattr (fd, TCSANOW, &tio) < 0) {
	    daemonLog ("tcsetattr %s: %s\n", gpstty, strerror(errno));
	    exit(1);
	}

	/* ok */
	return (fd);
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

/* read a fresh line from fd.
 * return 0 if ok, else -1.
 */
static int
freshLine (int fd, char *buf, int len)
{
	if(rflag) {
		/* Telserver */
		if(getTelserverInfo(buf) > 0) { // assumes len is large enough
			return 0;
		}
		daemonLog("Exit due to telserver read error\n");
		return -1;
	}	
	if(xflag) {
		/* External script */
		int pid, fd;
		int p[2];
		int i;

		if (pipe(p) < 0) {
		    strcpy (buf, strerror(errno));
		    return (-1);
		}

		pid = fork();
		if (pid < 0) {
		    /* failed */
		    close (p[0]);
	    	close (p[1]);
		    strcpy (buf, strerror(errno));
		    return (-1);
		}

		if (pid == 0) {
		    /* child */
	    	close (p[0]);
		    close (0);
		    dup2 (p[1], 1);
	    	close (p[1]);
		    for (i = 2; i < 100; i++)
			close (i);
	    	execvp (extCmd, NULL);
		    exit (errno);
		}

		/* parent continues.. */
		close (p[1]);
		fd = p[0];
	    /* read response, gather exit status */
	    while ((i = read (fd, buf, 1)) > 0)
		buf += i;
	    *buf = '\0';
	    close (fd);
	    wait (&i);
	    if (WIFEXITED(i))
		i = -WEXITSTATUS(i);
	    else
		i = -(32 + WTERMSIG(i));
	    if (i != 0 && strlen(buf) == 0)
		sprintf (buf, "Error from %s: %s", extCmd, strerror(-i));

		if(i != 0) return (-1);
		return (0);
		
	} else {
		/* TTY */
		/* discard, sync to next line, then read a whole line */

/* STO REMOVED THIS VERSION, REPLACED BELOW 
		(void) tcflush (fd, TCIOFLUSH);
		(void) read (fd, buf, len);
		if (read (fd, buf, len) < 0) {
		    daemonLog ("tty: %s\n", strerror(errno));
		    return (-1);
		}
*/

        // read until we get a $ that marks start of a line, then read to line end
        int i;
        while(1) {
            buf[0] = 0;
            while(buf[0] != '$') {
                if(read(fd, buf, 1) < 0) {
	    	        daemonLog ("tty: %s\n", strerror(errno));
    		        return (-1);
	    	    }
            }
            for(i=1; i<len; i++) {
                if(read(fd,&buf[i],1) < 0) {
	        	    daemonLog ("tty: %s\n", strerror(errno));
		            return (-1);
    		    }
                if(buf[i] <= 32) { // end of line, presumably
                    return(0);
                }
            }
        }
	}
	
	return (0);
}

static void
ttySync(int fd) {
    if(!xflag) {
        (void) tcflush(fd,TCIOFLUSH);
    }
}

static int
processLine (char *buf)
{
#define	NFLDS	10
	static int chkedLL;
	char *tok;
	double lt, lg;
	double Mjd;
	int h, m, s;
	int M, D, Y;
	int ltdeg, lgdeg;
	double ltmin, lgmin;
	char ltNS = '\0', lgEW = '\0';
	int i;

	for (i = 0; i < NFLDS; i++) {
	    tok = strtok (i == 0 ? buf : NULL, ",");
	    if (!tok)
		return -1;

	    switch (i) {
	    case 0:	/* ID */
		if (strcmp (tok, "$GPRMC"))
		    return -1;
		break;

	    case 1: /* UTC */
		if (sscanf (tok, "%2d%2d%2d", &h, &m, &s) != 3 && !iflag)
		    return -1;
		break;

	    case 2: /* valid */
		if (*tok != 'A') {
		    if (vflag)
			printf ("No lock");
		    if (!iflag) {
			printf ("\n");
			return -1;
		    } else
			printf (": ");
		}
		break;

	    case 3: /* lat */
		if (sscanf (tok, "%2d%lf", &ltdeg, &ltmin) != 2 && !iflag)
		    return -1;
		break;

	    case 4: /* N/S */
		ltNS = *tok;
		if (ltNS != 'N' && ltNS != 'S' && !iflag)
		    return -1;
		break;

	    case 5: /* longitude */
		if (sscanf (tok, "%3d%lf", &lgdeg, &lgmin) != 2 && !iflag)
		    return -1;
		break;

	    case 6: /* E/W */
		lgEW = *tok;
		if (lgEW != 'E' && lgEW != 'W' && !iflag)
		    return -1;
		break;

	    case 7: /* knots */
		break;

	    case 8: /* track */
		break;

	    case 9: /* date -- DDMMYY */
		if (sscanf (tok, "%2d%2d%2d", &D, &M, &Y) != 3 && !iflag)
		    return -1;
		if (Y < 90)
		    Y += 2000;
		else
		    Y += 1900;
		break;
	    }
	}

	/* built lt +N, lg +E */
	lt = degrad (ltdeg + ltmin/60.0);
	if (ltNS == 'S')
	    lt = -lt;
	lg = degrad (lgdeg + lgmin/60.0);
	if (lgEW == 'W')
	    lg = -lg;
	cal_mjd (M, D+(((s/60.+m)/60.+h)/24.), Y, &Mjd);
	if (vflag || !before) {
	    char ltstr[32], lgstr[32];

	    fs_sexa (ltstr, raddeg(lt), 3, 3600);
	    fs_sexa (lgstr, -raddeg(lg), 4, 3600);
	    daemonLog("%13.5f=(%02d/%02d/%02d %02d:%02d:%02d) %10.7f=%s %10.7f=%s\n",
			    Mjd+MJD0, M, D, Y, h, m, s, lt, ltstr, -lg, lgstr);
	    before = 1;
	}

	/* set and/or report time, depending on flags */
	setTime (Mjd);

	/* just do lat/long at most one time to avoid constant jitter */
	if (!chkedLL) {
	    if (llIsOff (lt, -lg)) {
		if (cflag)
		    setCfg (lt, -lg);
		if (sflag) {
		    telstatshmp->now.n_lat = lt;
		    telstatshmp->now.n_lng = lg;
		}
	    }
	    chkedLL = 1;
	}

	if (oflag)
	    exit(0);

    return 0;
}

/* if aflag gently adjust the system clock to Mjd.
 * if fflag and !timesetonce rudely hammer the system clock to Mjd.
 * if vflag report error (even if we don't fix)
 */
static void
setTime (double Mjd)
{
	static int nskips;
	double computer_clock, secs_slow;
	struct timeval tv;
	
	/* Adjust the seconds by the given offset (if any) */
	double secAdj = offsetSecs / SPD;
	Mjd += secAdj;
	
	/* Adjust time each read if we've set a specific wait time */
	if(!delaySecs) { // otherwise...
		/* don't just hammer on it constantly.
		 * N.B. make sure this allows a time set the first call.
		 */
		if ((nskips++) % NSETSKIPS)
		    return;
	}
	
	/* get current error */
	computer_clock = mjd_now();
	secs_slow = (Mjd - computer_clock)*SPD;
	if (fabs(secs_slow) >= 1)
	    daemonLog ("%s by %g secs\n", secs_slow>0?"slow":"fast", secs_slow);

	if (hflag || (fflag && !timesetonce)) {
	    /* set the time abruptly to Mjd */
	    double secs_1_1_1970 = (Mjd - 25567.5)*SPD;
	    tv.tv_sec = (long)floor(secs_1_1_1970);
	    tv.tv_usec = (long)floor((secs_1_1_1970 - tv.tv_sec)*1000000);
	    timesetonce = 1;	/* tried to anyway */
	    if (settimeofday (&tv, NULL) < 0) {
		daemonLog ("settimeofday: %s\n", strerror(errno));
		return;
	    }
	    if (vflag)
		printf (".. corrected abruptly\n");
	} else if (aflag) {
	    /* adjust the time gently */
	    tv.tv_sec = (long)floor(secs_slow);
	    tv.tv_usec = (long)floor((secs_slow - tv.tv_sec)*1000000);			

	    if (adjtime (&tv, NULL) < 0) {
		daemonLog ("adjtime: %s\n", strerror(errno));
		return;
	    }
	    if (vflag)
		printf (".. correcting gradually %g\n",(double) tv.tv_sec+(tv.tv_usec/1000000.0));
	}
}

/* return 1 if lt or lg are way off from their values in telsched.cfg, else 0.
 * N.B. lt is +N, lg is +W
 */
static int
llIsOff(double lt, double lg)
{
	double cfglt, cfglg;

	/* read values currently in file */
	if (read1CfgEntry (1, tcfn, ltname, CFG_DBL, &cfglt, 0) < 0) {
	    daemonLog ("%s: no %s\n", tcfn, ltname);
	    exit (1);
	}
	if (read1CfgEntry (1, tcfn, lgname, CFG_DBL, &cfglg, 0) < 0) {
	    daemonLog ("%s: no %s\n", tcfn, lgname);
	    exit (1);
	}

	return (fabs(lt-cfglt) > MAXLTERR || fabs(lg-cfglg) > MAXLGERR);
}

/* update the LONGITUDE and LATITUDE entries in tcfn[].
 * N.B. lt is +N, lg is +W
 */
static void
setCfg(double lt, double lg)
{
	char buf[64];

	/* write new values */
	(void) sprintf (buf, "%.6f", lt);
	if (writeCfgFile (tcfn, ltname, buf, NULL) < 0) {
	    daemonLog ("%s: %s", tcfn, strerror(errno));
	    return;
	}
	(void) sprintf (buf, "%.6f", lg);
	if (writeCfgFile (tcfn, lgname, buf, NULL) < 0) {
	    daemonLog ("%s: %s", tcfn, strerror(errno));
	    return;
	}

	if (vflag)
	    printf ("Updated %s\n", tcfn);
}

/* check HAVEGPS and exit if not set */
static void
havegps()
{
	int havegps;
	
	if(xflag || rflag) {
		return;		// use of external script or telserver overrides the config file
	}		

	if (read1CfgEntry (1, gcfn, "HAVEGPS", CFG_INT, &havegps, 0) < 0) {
	    daemonLog ("%s: no HAVEGPS\n", gcfn);
	    exit (1);
	}

	if (!havegps) {
	    daemonLog ("No GPS configured -- exiting\n");
	    exit (0);
	}
}

/* get tty from config file */
static void
setTty (void)
{
	static char cfgtty[32];

	if (read1CfgEntry (1, gcfn, "GPSTTY",CFG_STR,cfgtty,sizeof(cfgtty))<0){
	    daemonLog ("%s: no GPSTTY\n", gcfn);
	    exit (1);
	}

	gpstty = cfgtty;
}

/* ------------------------------------------

  Get 'gps' updates from telserver -- allows for shared
  use of a single gps across different systems

------------------------------------------------ */

static int sockfd;

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
 * Get GPS related information from telserver over sockfd
 * and translate into useable form into buf
 */
static int getTelserverInfo(char *outbuf)
{
	char buf[1024];
	double jd;
	time_t t;
	char TIME[16];
	char DATE[16];
	char NS[4];
	char EW[4];
	double LAT,LON;
	char asct[64];
	
	errno = 0;
		
	// send the telserver the request
	strcpy(buf,"GetInfo JD Latitude Longitude\r\n");
	
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
		return -1;
	}
	
	if(0 >= readsockline(sockfd,buf,sizeof(buf))) {
		daemonLog("Error reading from socket (1)\n");
		return -1;
	}
		
	jd = atof(buf);
	t = ((jd - MJD0) - 25567.5)*24.*3600.;
	strcpy(asct,timestamp(t));
	strcpy(TIME,&asct[8]);
	asct[8] = 0;
	strcpy(DATE,&asct[6]);
	asct[6] = 0;
	strcat(DATE,&asct[4]);
	asct[4] =0;
	strcat(DATE,&asct[2]);
	
	if(0 >= readsockline(sockfd,buf,sizeof(buf))) {
		daemonLog("Error reading from socket (2)\n");
		return -1;
	}
		
	LAT = atof(buf);
	if(LAT >=0) {
		strcpy(NS,"N");
	} else {
		strcpy(NS,"S");		
		LAT = -LAT;
	}
		
	if(0 >= readsockline(sockfd,buf,sizeof(buf))) {
		daemonLog("Error reading from socket (3)\n");
		return -1;
	}
		
	LON = atof(buf);
	if(LON >=0) {
		strcpy(EW,"E");
	} else {
		strcpy(EW,"W");
		LON = -LON;		
	}
	LON = 57.2957 * LON;

	if(0 >= readsockline(sockfd,buf,sizeof(buf))) {
		daemonLog("Error reading from socket (4)\n");
		return -1;
	}
			
	if(strcmp(buf,"*<<<")) {
		daemonLog("Invalid block end returned from telserver (%s)\n",buf);
		return -1;
	}
	
	sprintf(outbuf,"$GPRMC,%s,A,%.4f,%s,%.4f,%s,0,0,%s\n",TIME,LAT,NS,LON,EW,DATE);	
	return(strlen(outbuf));
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
	
	errno = 0;

	err = select(fd+1, &rfds, NULL, NULL, &tv);
	if(err <= 0) {
		daemonLog("Timeout on recv for socket %d",fd);
		return 0;
	}
	
	errno = 0;
	rb = 0;
	
	while(!errno || errno == EINTR || errno == EAGAIN) {
		errno = 0;
		rb = read(fd,buf,len);
		if(rb==len) break;
	}
	
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
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: gpsd.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.9 $ $Name:  $"};
