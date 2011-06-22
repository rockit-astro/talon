/* device-depend support for RainWise WS-2000 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <termios.h>

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

#define	RBUFL	256		/* max receive line length */

static int openTTY (char *tty);
static int readRW (int fd, unsigned char *rcv_buf, int *barcalp);
static int processRW (unsigned char *rcv_buf, int barcal, WxStats *wp,
    double *tp, double *pp);
static int get_acknowledge(int fd);
static int get_serial_char (int fd);
static int send_buf (int fd, unsigned char *buf, int n);
static int get_line (int fd, char buf[]);
static int simple_cmd (int fd, char cmd);
static int rcv_chksum (unsigned char *buf);

/* read weather station on tty, filling in goodies.
 * exit if can not open tty or tty error, all other errors return -1.
 */
int
RWUpdate (char *tty, WxStats *wp, double *tp, double *pp)
{
	static int fd = -1;
	unsigned char rcv_buf[RBUFL];
	int barcal;

	/* open connection if first time */
	if (fd < 0)
	    fd = openTTY (tty);

	/* read and process */
	if (readRW (fd, rcv_buf, &barcal) == 0)
	    return (processRW (rcv_buf, barcal, wp, tp, pp));
	return (-1);
}

/* open tty and return fd to connection, else exit */
static int
openTTY (char *tty)
{
	struct termios tio;
	int fd;

	fd = telopen (tty, O_RDWR|O_NONBLOCK);
	if (fd < 0) {
	    daemonLog ("%s: %s\n", tty, strerror(errno));
	    exit(1);
	}
	if (fcntl (fd, F_SETFL, 0 /* no NONBLOCK now */ ) < 0) {
	    daemonLog ("fctnl: %s: %s\n", tty, strerror(errno));
	    exit(1);
	}

	/* set line characteristics */
	memset (&tio, 0, sizeof(tio));
	tio.c_iflag = 0;
	tio.c_cflag = CS8 | CREAD | CLOCAL;
	tio.c_oflag = 0;
	tio.c_lflag = 0;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	cfsetispeed (&tio, B9600);
	cfsetospeed (&tio, B9600);
	if (tcsetattr (fd, TCSANOW, &tio) < 0) {
	    daemonLog ("tcsetattr: %s: %s\n", tty, strerror(errno));
	    exit(1);
	}

	if (rflag) {
	    if (simple_cmd (fd, 'S') < 0)
		return (-1);
	    if (simple_cmd (fd, 'T') < 0)
		return (-1);
	    if (simple_cmd (fd, 'Z') < 0)
		return (-1);
	}

	return (fd);
}

/* send ":<cmd>" look for ">OK" back.
 * return 0 if indeed, else -1.
 */
static int
simple_cmd (int fd, char cmd)
{
	char buf[RBUFL];

	buf[0] = ':';
	buf[1] = cmd;
	if (send_buf (fd, buf, 2) < 0)
	    return (-1);
	if (get_acknowledge (fd) < 0)
	    return (-1);
	if (get_line (fd, buf) < 0 || strncmp (buf, "OK", 2))
	    return (-1);
	return (0);
}

/* issue a D command and read back the response in rcv_buf.
 * also fill in barcalp with barometer calibration value.
 * return 0 if ok, else -1
 */
static int
readRW (int fd, unsigned char *rcv_buf, int *barcalp)
{
	unsigned char xmt_buf[32];
	int n;

	/* send command to get one line of data */
	n = 0;
	xmt_buf[n++] = ':';			/* attention */
	xmt_buf[n++] = 'D';			/* send data line */
	tcflush (fd, TCIOFLUSH);		/* fresh stuff */
	if (send_buf(fd, xmt_buf, n) < 0)	/* send */
	    return (-1);

	/* expect > */
	if (get_acknowledge(fd) < 0)
	    return (-1);

	/* read thru next lf */
	if (get_line (fd, rcv_buf) < 0)
	    return (-1);

	/* validate */
	if (rcv_chksum(rcv_buf) < 0)
	    return (-1);

	/* ok to process */
	return (0);
}

/* process info from rcv_buf[].
 * return 0 if ok, else -1
 */
static int
processRW (unsigned char *rcv_buf, int barcal, WxStats *wp, double *tp,
double *pp)
{
	double temperature;
	double wind_speed;
	double hi_wind_speed;
	double wind_direction;
	double barometer;
	double humidity;
	double rain;
	int s;

	s = sscanf (rcv_buf, "D,%*[^,],%*[^,],%lf,%lf,%lf,%lf,%lf,%lf,%lf",
		    &temperature, &humidity, &barometer, &wind_direction,
		    &wind_speed, &hi_wind_speed, &rain);
    	if (s != 7) {
	    daemonLog ("No data: %s", rcv_buf);
	    return (-1);
	}

	wp->wspeed = wind_speed*1.609 + .5;	/* MPH -> KPH */
	wp->wdir = wind_direction;		/* degs E of N */
	wp->humidity = humidity;		/* % */
	wp->rain = rain*254;			/* in -> .1mm */
	*tp = 5*(temperature-32)/9;		/* F -> C */
	*pp = barometer*33.86;			/* in -> mBar */

	return (0);
}

/* compute the checksum for rcv_buf and return 0 if ok, else -1
 */
static int
rcv_chksum (unsigned char *rcv_buf)
{
	int sum;
	int c;
	int i;

	for (i = 0, sum = 0; i < RBUFL-1 && (c = rcv_buf[i]) && c != '!'; i++)
	    sum += c;
	return ((sum%256) == atoi(&rcv_buf[++i]) ? 0 : -1);
}

/* send first n chars of buf to fd.
 * return 0 if ok else -1
 */
static int
send_buf (int fd, unsigned char *buf, int n)
{
	int w;

	if ((w = write (fd, buf, n)) != n) {
	    if (w < 0)
		daemonLog ("write error: %s\n", strerror(errno));
	    else 
		daemonLog ("short write: %d vs. %d\n", w, n);
	    exit(1);
	}

	return (0);
}

/* read one char from fd, return 0 if it is '>' else -1
 */
static int
get_acknowledge(int fd)
{
	int i;
	if ((i = get_serial_char(fd)) != '>')
	    return (-1);
	return (0);
}                     

/* read chars from fd into buf[] until see '\n', add '\0'.
 * return 0 if ok, else -1
 */
static int
get_line (int fd, char rcv_buf[])
{
	int c, n;

	n = 0;
	do {
	    c = get_serial_char (fd);
	    if (c < 0)
		return (-1);
	    rcv_buf[n++] = c;
	} while (c != '\n');
	rcv_buf[n] = '\0';
	return (0);
}

/* Get a character from fd.
 * return it or -1 if trouble
 */
static int
get_serial_char (int fd)
{
	unsigned  char c;
	struct timeval tv;
	fd_set rfd;

	tv.tv_sec = 4;
	tv.tv_usec = 0;
	FD_ZERO (&rfd);
	FD_SET (fd, &rfd);

	if (select (fd+1, &rfd, NULL, NULL, &tv) <= 0) {
	    daemonLog ("timeout\n");
	    return (-1);
	}
	if (read (fd, &c, 1) != 1) {
	    daemonLog ("get_serial_char error: %s\n", strerror(errno));
	    exit(1);
	}

	if (bflag) {
	    printf ("%c", c);
	    fflush (stdout);
	}

	return (c);
}      

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: rainwise.c,v $ $Date: 2001/04/19 21:12:11 $ $Revision: 1.1.1.1 $ $Name:  $"};
