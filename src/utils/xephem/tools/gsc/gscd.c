/* a daemon process to listen to TCP port 7577 for GSC requests and respond.
 * we harmlessly exit if it appears we are already running on this system.
 * use -i to run via inetd, else we run as our own daemon.
 */

/* #define NOFORKS to not use separate forked processes */
#undef NOFORKS

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "gscd.h"

typedef unsigned char UC;
typedef unsigned long UL;

extern int GSCSetup (char *cdp, char *chp, char msg[]);
extern int GSCFetch (double ra0, double dec0, double fov, double fmag,
    ObjF **opp, int nopp, char msg[]);

static int already_running();
static void usage (char *p);
static void daemonize(void);
static void setupGSC(void);
static void go(void);
static void handleClient (int fd);
static int clreply (int fd, int status, char *msg);
static int sendbytes (int fd, UC buf[], int n);
static int recvbytes (int fd, UC buf[], int n);
static int tout (int fd, int w);
static int unpackbuf (UC buf[SPKTSZ], double *rap, double *decp, double *fovp,
    double *magp, char msg[]);
static void packbuf (Obj *op, UC buf[RPKTSZ]);
static void wmsg (char *fmt, ...);

#define	VER	1	/* max client version we can handle */
#define	MAXCL	5	/* max clients we'll serve simultaneously */
#define	NPKTS	50	/* how many stars we push out the network at once */
#define	TOUT	60	/* secs we'll wait for a client */

/* defaults and option flags */
static char *cdpath = "/cdrom";		/* cdrom mount point */
static char *chpath = "../../auxil/gsccache";	/* path to base of cache */
static double maxfov = degrad(5);	/* max field-of-view we'll retrn, rads*/
static dflag, aflag, iflag, mflag, vflag;	/* misc flags */

int
main (int ac, char *av[])
{
	char *pname = av[0];

	/* crack our args */
	while ((--ac > 0) && ((*++av)[0] == '-')) {
	    char *s;
	    for (s = av[0]+1; *s != '\0'; s++)
	    switch (*s) {
	    case 'a': /* don't use the disk cache */
		aflag++;
		break;
	    case 'c': /* alternate cache path */
		if (ac-- < 1)
		    usage (pname);
		chpath = *++av;
		break;
	    case 'd': /* don't daemonize */
		dflag++;
		break;
	    case 'f': /* alternate max fov */
		if (ac-- < 1)
		    usage (pname);
		maxfov = degrad(atof(*++av));
		break;
	    case 'h': /* help and exit */
		usage (pname);
		break;
	    case 'i': /* run via inetd */
		iflag++;
		break;
	    case 'm': /* don't cdrom */
		mflag++;
		break;
	    case 'r': /* alternate cdrom path */
		if (ac-- < 1)
		    usage (pname);
		cdpath = *++av;
		break;
	    case 'v': /* verbose */
		vflag++;
		break;
	    default:
		usage (pname);
	    }
	}

	/* any more args is a usage problem */
	if (ac > 0)
	    usage (pname);

	/* guard against accidental multiple instances, unless of course if
	 * being run via inetd.
	 */
	if (!iflag && already_running()) {
	    fprintf (stderr, "%s: Already running\n", pname);
	    return (0);
	}

	/* we exit unless told not to daemonize or being run from inetd */
	if (!dflag && !iflag)
	    daemonize();

	/* set up the disk GSC access */
	setupGSC();

	/* set up for the sys logger */
	openlog (pname, 0, LOG_LOCAL7);

	/* handle one client if running via inetd else loop ourselves forever */
	if (iflag)
	    handleClient (0);
	else
	    go();

	return (0);
}

/* return 1 if it appears we are already running locally, else return 0 */
static int
already_running()
{
	struct sockaddr_in serv_addr;
	int sockfd;

	/* create a socket to a server on this host */
	(void) memset ((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serv_addr.sin_port = htons(GSCPORT);
	if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
	    return (0);

	/* connect */
	if (connect (sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0){
	    close (sockfd);
	    return (0);
	}

	close (sockfd);
	return (1);
}

static void
usage (char *p)
{
    FILE *fp = stderr;

    fprintf (fp, "usage: %s [options]\n", p);
    fprintf (fp, "version %d\n", VER);
    fprintf (fp, "  -r <dir>:  alternate CDROM mount point (default is %s)\n",
									cdpath);
    fprintf (fp, "  -c <dir>:  alternate cache directory (default is %s)\n",
    									chpath);
    fprintf (fp, "  -f <degs>: max FOV to return (default is %g)\n",
							    raddeg(maxfov));
    fprintf (fp, "  -d:        do not daemonize (default is to fork and this parent exits)\n");
    fprintf (fp, "  -h:        help usage summary -- this\n");
    fprintf (fp, "  -i:        run via inetd and send messages to syslog\n");
    fprintf (fp, "  -a:        don't make or use the disk cache\n");
    fprintf (fp, "  -m:        don't use the cdrom\n");
    fprintf (fp, "  -v:        verbose\n");
    fprintf (fp, "  -vv:       debug/trace\n");
    exit (1);
}

/* fork a child, then exit */
static void
daemonize()
{
#ifndef NOFORKS
	switch (fork()) {
	case 0: /* child */
	    return;
	case -1:/* error */
	    perror ("fork");
	    exit (1);
	default:/* parent */
	    exit (0);
	}
#endif /* NOFORKS */
}

static void
setupGSC()
{
	char msg[1024];

	if (aflag && mflag) {
	    wmsg ("Enable at least one of CDROM and Cache");
	    exit (1);
	}

	if (GSCSetup (mflag?NULL:cdpath, aflag?NULL:chpath, msg) < 0) {
	    wmsg ("%s", msg);
	    exit (1);
	}
}

static void
go()
{
	int nchildren = 0;
	struct sockaddr_in serv_addr;
	int sockfd;

	/* make a socket endpoint */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    perror ("socket");
	    exit (1);
	}

	/* bind it to port GSCPORT for any Internet connection */
	memset ((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons (GSCPORT);
	if (bind (sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0) {
	    perror ("bind");
	    exit(1);
	}

	/* willing to accept connections with a backlog of 5 pending */
	listen (sockfd, 5);

	/* wait for clients forever.
	 * give each their own process as they arrive.
	 * don't allow more than MAXCL children to be running at once.
	 */
	for (;;) {
	    int newsockfd, clilen, childpid;
	    struct sockaddr_in cli_addr;

	    /* wait for a customer */
	    clilen = sizeof(cli_addr);
	    newsockfd = accept (sockfd, (struct sockaddr *)&cli_addr, &clilen);
	    if (newsockfd < 0) {
		perror ("accept");
		exit(1);
	    }

#ifndef NOFORKS
	    /* bury any exited children and decrement the count */
	    while (waitpid (-1, NULL, WNOHANG) > 0)
		nchildren--;

	    /* if not too busy, make a fresh child and serve the client */
	    if (nchildren <= MAXCL) {
		/* create a new process to handle the new client's traffic */
		if ((childpid = fork()) < 0) {
		    perror ("fork");
		    exit (1);
		}
		if (childpid == 0) {
		    close (sockfd); /* don't need the server socket here */
#endif /* NOFORKS */
		    handleClient (newsockfd);
#ifndef NOFORKS
		    exit (0);
		}
	    } else
		(void) clreply (newsockfd, 1, "Too busy now");

	    /* don't need the client socket here */
#endif /* NOFORKS */
	    close (newsockfd);
	}
}

/* handle the client using socket fd.
 */
static void
handleClient (int fd)
{
	struct sockaddr_in cli;
	char msg[1024];
	UC buf[SPKTSZ];
	double ra, dec, fov, mag;
	ObjF *op = NULL;
	int send, sent;
	char *whom;
	int n;

	/* see who it is */
	n = sizeof (cli);
	if (getpeername (fd, (struct sockaddr *)&cli, &n) < 0)
	    whom = "client???";
	else
	    whom = inet_ntoa (cli.sin_addr);

	/* log the connect */
	if (vflag)
	    wmsg ("%s: Connect", whom);

	/* read the request */
	if (recvbytes (fd, buf, SPKTSZ) < 0) {
	    wmsg ("%s: Bogus request packet -- dropping", whom);
	    return;
	}

	/* crack it */
	if (unpackbuf (buf, &ra, &dec, &fov, &mag, msg) < 0) {
	    (void) clreply (fd, 1, msg);
	    wmsg ("%s: Disconnected: bad packet: %s", whom, msg);
	    return;
	}
	if (vflag > 1)
	    wmsg ("%s: Request: RA=%g Dec=%g FOV=%g Mag=%g", whom,
							    ra, dec, fov, mag);

	/* clamp the FOV at maxfov */
	if (fov > maxfov) {
	    if (vflag > 1)
		wmsg ("%s: FOV clamped from %g to %g", whom, fov, raddeg(fov));
	    fov = maxfov;
	}

	/* collect the stars */
	n = GSCFetch (ra, dec, fov, mag, &op, 0, msg);
	if (n < 0) {
	    (void) clreply (fd, 1, msg);
	    wmsg ("%s: Disconnected: Trouble gathering stars at RA=%g Dec=%g FOV=%g Mag=%g: %s",
	      					whom, ra, dec, fov, mag, msg);
	    return;
	}

	if (vflag)
	    wmsg ("%s: Sending %d stars at RA=%g Dec=%g FOV=%g Mag=%g", whom,
							n, ra, dec, fov, mag);

	/* tell client how many are coming */
	(void) sprintf (msg, "%d", n);
	if (clreply (fd, 0, msg) < 0) {
	    wmsg ("%s: Dropping: Can't reply", whom);
	    if (op)
		free ((void *)op);
	    return;
	}

	/* send stars in bunches of NPKTS */
	if (vflag > 1)
	    wmsg ("%s: Sending %d stars...", whom, n);
	for (sent = 0; sent < n; sent += send) {
	    UC pkts[NPKTS][RPKTSZ];
	    int i;

	    send = sent + NPKTS > n ? n - sent : NPKTS;
	    for (i = 0; i < send; i++)
		packbuf ((Obj *)&op[sent+i], pkts[i]);
	    if (sendbytes (fd, (UC *)pkts, send*RPKTSZ) < 0) {
		wmsg ("%s: Dropping: Error after %d of %d stars", whom, sent,n);
		if (op)
		    free ((void *)op);
		return;
	    }
	}

	/* ok, that's all */
	if (vflag) {
	    wmsg ("%s: Client disconnect", whom);
	}

	if (op)
	    free ((void *)op);

	return;
}

/* send the client message to the given socket.
 * return 0 if ok else -1.
 */
static int
clreply (int fd, int status, char *msg)
{
	UC buf[80];

	buf[0] = status;

	if (msg) {
	    memcpy ((char *)buf+1, msg, sizeof(buf)-1);
	    if (vflag>1)
		wmsg ("Reply message: %d %s", status, msg);
	}

	return (sendbytes (fd, buf, sizeof(buf)));
}

/* send n bytes from buf to socket fd.
 * return 0 if ok else -1
 */
static int
sendbytes (int fd, UC buf[], int n)
{
	int ns, tot;

	for (tot = 0; tot < n; tot += ns) {
	    if (tout (fd, 1) < 0)
		return (-1);
	    ns = send (fd, buf+tot, n-tot, 0);
	    if (ns <= 0)
		return (-1);
	}
	return (0);
}

/* receive exactly n bytes from socket fd into buf.
 * return 0 if ok else -1
 */
static int
recvbytes (int fd, UC buf[], int n)
{
	int nr, tot;

	for (tot = 0; tot < n; tot += nr) {
	    if (tout (fd, 0) < 0)
		return (-1);
	    nr = recv (fd, buf+tot, n-tot, 0);
	    if (nr <= 0)
		return (-1);
	}
	return (0);
}

/* wait up to TOUT secs for the ability to read/write using fd.
 * w is 1 for writing, 0 for reading.
 * return 0 if ok to proceed, else -1 if trouble or timeout.
 */
static int
tout (int fd, int w)
{
	fd_set fdset, *rset, *wset;
	struct timeval tv;

	FD_ZERO (&fdset);
	FD_SET (fd, &fdset);
	if (w) {
	    rset = NULL;
	    wset = &fdset;
	} else {
	    rset = &fdset;
	    wset = NULL;
	}

	tv.tv_sec = TOUT;
	tv.tv_usec = 0;

	return (select (fd+1, rset, wset, NULL, &tv) <= 0 ? -1 : 0);
}

/* break apart a request buffer.
 * return 0 if looks ok, else fill msg with a reason and return -1.
 */
static int
unpackbuf (UC buf[SPKTSZ], double *rap, double *decp, double *fovp,
double *magp, char msg[])
{
	UL t;

	/* version number */
	if (buf[0] != VER) {
	    (void) sprintf (msg, "Version %d not supported", buf[0]);
	    return (-1);
	}

	/* 0 <= ra < 2*PI */
	t = ((UL)buf[1] << 16) | ((UL)buf[2] << 8) | (UL)buf[3];
	*rap = t*(2*PI)/(1L<<24);
	if (*rap < 0 || *rap >= 2*PI) {
	    (void) sprintf (msg, "Bogus RA: %g", *rap);
	    return (-1);
	}

	/* -PI/2 <= dec <= PI/2 */
	t = ((UL)buf[4] << 16) | ((UL)buf[5] << 8) | (UL)buf[6];
	*decp = t*PI/((1L<<24)-1) - PI/2;
	if (*decp < -PI/2 || *decp > PI/2) {
	    (void) sprintf (msg, "Bogus Dec: %g", *decp);
	    return (-1);
	}

	/* 0 <= fov <= MAXFOV */
	t = ((UL)buf[7] << 8) | (UL)buf[8];
	*fovp = t*MAXFOV/((1L<<16)-1);
	if (*fovp < 0 || *fovp > MAXFOV) {
	    (void) sprintf (msg, "Bogus FOV: %g", *fovp);
	    return (-1);
	}

	/* BMAG <= mag <= FMAG */
	t = ((UL)buf[9] << 8) | (UL)buf[10];
	*magp = t*(FMAG-BMAG)/((1L<<16)-1) + BMAG;
	if (*magp < BMAG || *magp > FMAG) {
	    (void) sprintf (msg, "Bogus Mag: %g", *magp);
	    return (-1);
	}

	return (0);
}

/* pack a star into a RPKTSZ buffer */
static void
packbuf (Obj *op, UC buf[RPKTSZ])
{
	UL t0, t1;

	sscanf (op->o_name, "GSC %ld-%ld", &t0, &t1);
	buf[0] = (t0 >> 8) & 0xff;
	buf[1] = (t0)      & 0xff;
	buf[2] = (t1 >> 8) & 0xff;
	buf[3] = (t1)      & 0xff;

	buf[4] = op->f_class == 'S' ? 0 : 1;

	t0 = floor(op->f_RA/(2*PI)*(1L<<24) + 0.5);
	buf[5] = (t0 >> 16) & 0xff;
	buf[6] = (t0 >> 8)  & 0xff;
	buf[7] = (t0)       & 0xff;

	t0 = floor((op->f_dec + PI/2)/PI*((1L<<24)-1) + 0.5);
	buf[8] = (t0 >> 16) & 0xff;
	buf[9] = (t0 >> 8)  & 0xff;
	buf[10]= (t0)       & 0xff;

	t0 = floor((op->f_mag/MAGSCALE - BMAG)/(FMAG-BMAG)*((1L<<16)-1) + 0.5);
	buf[11] = (t0 >> 8) & 0xff;
	buf[12] = (t0)      & 0xff;
}

/* write the message to syslog or fprintf, depending on iflag.
 * N.B: we add a date stamp and trailing \n if going to stderr.
 */
static void
wmsg (char *fmt, ...)
{
	va_list a;

	va_start (a, fmt);

	if (iflag) {
	    char buf[4096];
	    (void) vsprintf (buf, fmt, a);
	    syslog (LOG_INFO, "%s", buf);
	} else {
	    struct tm *tmp;
	    time_t t;
	    char *z;

	    (void) time (&t);
	    tmp = gmtime (&t);
	    if (tmp)
		z = "UTC";
	    else {
		tmp = localtime (&t);
		z = "LT";
	    }
	    fprintf (stderr, "%s %02d/%02d/%d %02d:%02d:%02d: ", z,
		tmp->tm_mon+1, tmp->tm_mday, tmp->tm_year+1900, tmp->tm_hour,
						    tmp->tm_min, tmp->tm_sec);
	    vfprintf (stderr, fmt, a);
	    fprintf (stderr, "\n");
	}

	va_end (a);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: gscd.c,v $ $Date: 2001/04/19 21:12:06 $ $Revision: 1.1.1.1 $ $Name:  $"};
