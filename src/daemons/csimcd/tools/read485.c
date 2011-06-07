/* listen to RS485 on stdin and report all chars.
 * handy to add to LAN and monitor traffic.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include "csimc.h"

#define	SPEED	B38400		/* desired speed */

#define	BPR	8		/* bytes per row of dump */

void dump (unsigned char *p, int np);

int
main (int ac, char *av[])
{
	struct termios tio;
	struct timeval tv0;
	int arg;

	/* set raw character access */
	memset (&tio, 0, sizeof(tio));
	tio.c_cflag = CS8|CREAD|CLOCAL;
	tio.c_iflag = IGNPAR|IGNBRK;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	cfsetospeed (&tio, SPEED);
	cfsetispeed (&tio, SPEED);
	if (tcsetattr (0, TCSANOW, &tio) < 0) {
	    perror ("");
	    exit (1);
	}

	/* immediate output */
	setbuf (stdout, NULL);

	/* RTS off to receive
	arg = TIOCM_RTS; ioctl (0, TIOCMSET, &arg);
	*/
	arg = 0; ioctl (0, TIOCMSET, &arg);

	/* read and echo */
	gettimeofday (&tv0, NULL);
	while (1) {
	    char buf[100];
	    struct timeval tv1;
	    int n = read (0, buf, sizeof(buf));
	    char *sync = memchr (buf, PSYNC, n);
	    int ms, ns;

	    gettimeofday (&tv1, NULL);
	    ms = (tv1.tv_sec-tv0.tv_sec)*1000 + (tv1.tv_usec-tv0.tv_usec)/1000;
	    printf ("%4d ", ms);
	    if (sync && (ns = sync - buf) > 0) {
		dump (buf, ns);
		printf ("     ");
		dump (buf+ns, n-ns);
	    } else
		dump (buf, n);
	    tv0 = tv1;
	}

	return (0);
}

/* dump np bytes starting at p to log */
void
dump (unsigned char *p, int np)
{
	int ntot = 0;
	int i, n;

	do {
	    char buf[10*BPR], *bp = buf;
	    n = np > BPR ? BPR : np;
	    bp += sprintf (bp, "%3d..%3d: ", ntot, ntot+n-1);
	    for (i = 0; i < BPR; i++)
		if (i < n)
		    bp += sprintf (bp, "%02x ", p[i]);
		else
		    bp += sprintf (bp, "   ");
	    bp += sprintf (bp, "   ");
	    for (i = 0; i < BPR; i++) {
		*bp++ = (i < n && isprint(p[i])) ? p[i] : ' ';
		*bp++ = ' ';
	    }
	    *bp = 0;
	    printf ("%s%s\n", ntot ? "     " : "", buf);
	    p += n;
	    ntot += n;
	} while (np -= n);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: read485.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $"};
