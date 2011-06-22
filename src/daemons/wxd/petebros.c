/* device-depend support for Peet Bros Ultimeter 2000. */

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

static int openTTY (char *tty);
static int processLine (char *buf, WxStats *wp, double *tp, double *pp);
static int h2b (char *c);

/* read weather station on tty, filling in goodies.
 * exit if can not open tty or tty error, all other errors return -1.
 */
int
PBUpdate (char *tty, WxStats *wp, double *tp, double *pp)
{
	static int pbfd = -1;
	char buf[1024];

	/* open connection if first time */
	if (pbfd < 0)
	    pbfd = openTTY (tty);

	/* discard, sync to next line, then read a whole line */
	(void) tcflush (pbfd, TCIOFLUSH);
	(void) read (pbfd, buf, sizeof(buf));
	if (read (pbfd, buf, sizeof(buf)) < 0) {
	    daemonLog ("%s: %s\n", tty, strerror(errno));
	    exit(1);
	}

	/* process */
	return (processLine (buf, wp, tp, pp));
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
	if (tcgetattr (fd, &tio) < 0) {
	    daemonLog ("tcgetattr: %s: %s\n", tty, strerror(errno));
	    exit(1);
	}
	tio.c_iflag = IGNCR;
	tio.c_cflag = CS8 | CREAD | CLOCAL;
	tio.c_lflag = ICANON;
	cfsetispeed (&tio, B2400);
	if (tcsetattr (fd, TCSANOW, &tio) < 0) {
	    daemonLog ("tcsetattr: %s: %s\n", tty, strerror(errno));
	    exit(1);
	}

	if (rflag) {
	    /* reset, wait, command data logging mode */
	    if (write (fd, ">G\r\n", 4) != 4) {
		daemonLog ("write G: %s: %s\n", tty, strerror(errno));
		exit(1);
	    }
	    sleep (2);	/* need this? */
	    if (write (fd, ">I\r\n", 4) != 4) {
		daemonLog ("write I: %s: %s\n", tty, strerror(errno));
		exit(1);
	    }
	}

	return (fd);
}

/* process one line from the wx station into *wp and *tp and *pp.
 * return -1 if bogus line, else 0.
 */
static int
processLine (char *buf, WxStats *wp, double *tp, double *pp)
{
#define	UNPACK(p)	((h2b(p+0)<<12)+(h2b(p+1)<<8)+(h2b(p+2)<<4)+h2b(p+3))
	double t, p;
	int x;

	/* valid header should be !! -- check then skip */
	if (buf[0] != '!' || buf[1] != '!') {
	    daemonLog ("Bogus line from weather station");
	    return (-1);
	}
	buf += 2;

	/* print the raw line if debugging */
	if (bflag) {
	    int i;

	    for (i = 1; i <= 12; i++)
		printf ("%-2d  ", i);
	    printf ("\n");
	    for (i = 0; i < 50; i++)
		if ((i%4) == 0)
		    printf ("%-2d  ", (i/4)*4);
	    printf ("\n");
	    printf ("%s", buf);
	}

	/* crack the header.
	 * N.B. ignore first two chars of wind dir.
	 */

	x = UNPACK (buf+0)/10;
	if (x >= 0 && x < 200)
	    wp->wspeed = x;

	buf[4] = buf[5] = '0';
	x = 360*UNPACK(buf+4)/256;
	if (x >= 0 && x < 360)
	    wp->wdir = x;

	t = (5./9.)*(UNPACK(buf+8)/10. - 32.);
	p = UNPACK(buf+16)/10.;

	x = UNPACK(buf+24)/10. + .5;
	if (x >= 0 && x <= 100)
	    wp->humidity = x;

	x = (254./100.)*UNPACK(buf+40);	/* .01" to .1mm */
	if (x >= 0 && x < 10000)
	    wp->rain = x;

	return (0);
}

/* hex to binary */
static int
h2b (char *c)
{
	switch (*c) {
	default: /* FALLTHRU */
	case '0': return (0);
	case '1': return (1);
	case '2': return (2);
	case '3': return (3);
	case '4': return (4);
	case '5': return (5);
	case '6': return (6);
	case '7': return (7);
	case '8': return (8);
	case '9': return (9);
	case 'A': return (10);
	case 'B': return (11);
	case 'C': return (12);
	case 'D': return (13);
	case 'E': return (14);
	case 'F': return (15);
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: petebros.c,v $ $Date: 2001/04/19 21:12:11 $ $Revision: 1.1.1.1 $ $Name:  $"};
