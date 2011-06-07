/* code to send a request for GSC stars to an xephem xephemdbd or an ESO server.
 * ESO code based on sample from Miguel Albrecht <malbrech@eso.org>
 */

#include <stdio.h>
#include <math.h>

#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#else
extern void *realloc(), *malloc();
#endif

#if defined(_POSIX_SOURCE)
#include <unistd.h>
#else
extern int close();
#endif

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "net.h"
#include "patchlevel.h"

extern char *syserrstr P_((void));
extern void fs_sexa P_((char *out, double a, int w, int fracbase));
extern void stopd_up P_((void));
extern void stopd_down P_((void));
extern void xe_msg P_((char *msg, int app_modal));
extern void zero_mem P_((void *loc, unsigned len));

static int moreObjF P_((ObjF **opp, int nop, int nnew));


/* fetch GSC stars around the given field of view from the named xephem host.
 * if find some add them to the list at *opp and return new total, else
 * return -1 with a diagnostic message in msg[].
 */
int
gscnetfetch (url, np, ra, dec, fov, mag, opp, nop, msg)
char *url;	/* /<host>/xephemdbd.pl or whatever */
Now *np;	/* current circumstances */
double ra, dec;	/* RA/Dec of center of view, rads, J2000 */
double fov;	/* diamater of field of view, rads */
double mag;	/* limiting magnitude desired */
ObjF **opp;	/* *opp is malloced with stars found */
int nop;	/* number already at *opp */
char msg[];	/* return diagnostic message here, if returning -1 */
{
static char ofmt[] = "http://%s%s?VERSION=3&FMT=EDB&CENTRIC=GEO&PRECES=ASTRO&GS=on&DATE=Now&TIME=&RA=%s&DEC=%s&FOV=%g&MAG=%g&LAT=0+0+0&LONG=0+0+0&ELEV=0 HTTP/1.0\r\nUser-Agent: xephem/%s\r\n\r\n";
	char *h0p, *h1p, host[1024];
	char rbuf[32], *rbp, dbuf[32], *dbp;
	char buf[2048];
	int sockfd;
	int n;

	/* pull out the host from url */
	h0p = url[0] == '/' ? url+1 : url;
	h1p = strchr (h0p, '/');
	if (!h1p) {
	    (void) strcpy (msg, "No host in xephemdbd url");
	    return (-1);
	}
	(void) sprintf (host, "%.*s", h1p-h0p, h0p);

	/* format the GET -- skip leading blanks in rbuf and dbuf */
	fs_sexa (rbuf, radhr(ra), 2, 36000);
	for (rbp = rbuf; *rbp == ' '; rbp++) continue;
	fs_sexa (dbuf, raddeg(dec), 3, 3600);
	for (dbp = dbuf; *dbp == ' '; dbp++) continue;
	(void) sprintf (buf, ofmt, host, h1p, rbp, dbp, raddeg(fov), mag, PATCHLEVEL);

	/* let user abort */
	stopd_up();

	/* send the GET method to host and connection to read response */
	sockfd = httpGET (host, buf, msg);
	if (sockfd < 0) {
	    stopd_down();
	    return (-1);
	}

	/* now read lines of stars from the socket and collect good lines in
	 * opp array
	 */
	while ((n = recvlineb (sockfd, buf, sizeof(buf))) > 0) {
	    Obj o, *op;

	    if (db_crack_line (buf, &o, NULL) < 0 || o.o_type != FIXED)
		continue;

	    if (moreObjF (opp, nop, 1) < 0) {
		(void) strcpy (msg, "No memory");
		(void) close (sockfd);
		stopd_down();
		return (-1);
	    }

	    op = (Obj *)(&(*opp)[nop++]);
	    memcpy ((void *)op, (void *)&o, sizeof(ObjF));
	}

	(void) close (sockfd);
	stopd_down();

	if (n < 0) {
	    (void) strcat (msg, syserrstr());
	    return (-1);
	}
	return (nop);
}

/* fetch GSC stars around the given field of view from the named ESO host.
 * if find some add them to the list at *opp and return new total, else
 * return -1 with a diagnostic message in msg[].
 */
int
gscesofetch (np, ra, dec, fov, mag, opp, nop, msg)
Now *np;	/* current circumstances */
double ra, dec;	/* RA/Dec of center of view, rads, J2000 */
double fov;	/* diamater of field of view, rads */
double mag;	/* limiting magnitude desired */
ObjF **opp;	/* *opp is malloced with stars found */
int nop;	/* number already at *opp */
char msg[];	/* return diagnostic message here, if returning -1 */
{
	static char eso_host[] = "archive.eso.org";
static char ifmt[] = "%s %lf %lf %lf %lf %lf %lf %*s %lf %*s %*s %d";
static char ofmt[] = "/skycat/servers/gsc-server?%.6f%c%.6f&r=0,%.1f&m=0,%g&s=R&f=1&* HTTP/1.0\r\nUser-Agent: xephem/%s\r\n\r\n";
	static char eofcode[] = "[EOD]";
	char buf[1024];
	int sockfd;
	int n;

	/* format the GET */
	(void) sprintf (buf, ofmt, radhr(ra), dec<0?'-':'+', fabs(raddeg(dec)),
					    raddeg(fov/2)*60, mag, PATCHLEVEL);

	/* let user abort */
	stopd_up();

	/* send the GET method to host and connection to read response */
	sockfd = httpGET (eso_host, buf, msg);
	if (sockfd < 0) {
	    stopd_down();
	    return (-1);
	}

	/* now read lines of stars from the socket and collect in opp array */
	while ((n = recvlineb (sockfd, buf, sizeof(buf))) > 0) {
	    double rh, rm, rs;
	    double dd, dm, ds;
	    char name[32];
	    double m;
	    Obj *op;
	    int c;

	    if (strncmp (buf, eofcode, sizeof(eofcode)-1) == 0)
		break;

	    if (sscanf (buf,ifmt,name,&rh,&rm,&rs,&dd,&dm,&ds,&m,&c) != 9)
		continue;
	    if (moreObjF (opp, nop, 1) < 0) {
		(void) strcpy (msg, "No memory");
		(void) close (sockfd);
		stopd_down();
		return (-1);
	    }
	    op = (Obj *)(&(*opp)[nop++]);
	    zero_mem ((void *)op, sizeof(ObjF));

	    (void) sprintf (op->o_name, "GSC %.4s-%.4s", name+1, name+6);
	    op->o_type = FIXED;
	    op->f_class = c == 0 ? 'S' : 'T';
	    op->f_RA = (float) hrrad (rs/3600.0 + rm/60.0 + rh);
	    dd = fabs(dd);
	    op->f_dec = (float) degrad (ds/3600.0 + dm/60.0 + dd);
	    if (buf[23] == '-')
		op->f_dec *= -1.0;
	    op->f_epoch = (float)J2000;
	    set_fmag (op, m);
	}

	(void) close (sockfd);
	stopd_down();

	if (n < 0) {
	    (void) strcat (msg, syserrstr());
	    return (-1);
	}
	return (nop);
}

/* grow *opp, which already contains nop, to hold nnew more.
 * if ok update *opp and return 0, else -1.
 */
static int
moreObjF (opp, nop, nnew)
ObjF **opp;
int nop;
int nnew;
{
	char *newmem;

	/* extend *opp for nnew more ObjF's */
	if (*opp)
	    newmem = realloc ((void *)*opp, (nop+nnew)*sizeof(ObjF));
	else
	    newmem = malloc (nnew * sizeof(ObjF));
	if (!newmem)
	    return (-1);
	*opp = (ObjF *)newmem;
	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: gscnet.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
