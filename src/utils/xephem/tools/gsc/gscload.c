/* load and/or list the GSC cache from CDROM around the given location.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"

extern int GSCSetup (char *cdp, char *chp, char msg[]);
extern int GSCFetch (double ra0, double dec0, double fov, double fmag,
    ObjF **opp, int nopp, char msg[]);
extern int scansex (char *str, double *dp);
extern void fs_sexa (char *out, double a, int w, int fracbase);

#define	MAXEXPT	10000	/* max number of stars we will print */

static char cdpath_default[] = "/cdrom";
static char *cdpath = cdpath_default;
static char chpath_default[] = "../gsccache";
static char *chpath = chpath_default;

static int lflag;
static int aflag;
static int mflag;

static void usage (char *progname);
static void listStars (ObjF *sp, int n);

int
main (int ac, char *av[])
{
	char *progname = av[0];
	char msg[1024];
	double ra, dec, fov, fmag;
	ObjF *gs;
	char *str;
	int nexpt;
	int n;

	/* crack arguments */
	for (av++; --ac > 0 && *(str = *av) == '-'; av++) {
	    char c;
	    /* in the loop ac includes the current arg */
	    while ((c = *++str))
		switch (c) {
		case 'a':	/* don't use cache files */
		    aflag++;
		    break;
		case 'c':	/* alternate cache base dir */
		    if (ac < 2) {
			fprintf (stderr, "No dir with -c\n");
			usage (progname);
		    }
		    chpath = *++av;
		    ac--;
		    break;
		case 'l':	/* list too */
		    lflag++;
		    break;
		case 'm':	/* don't use CD-ROM */
		    mflag++;
		    break;
		case 'r':	/* alternate cdrom mount point */
		    if (ac < 2) {
			fprintf (stderr, "No dir with -r\n");
			usage (progname);
		    }
		    cdpath = *++av;
		    ac--;
		    break;
		default:
		    usage(progname);
		    break;
		}
	}
         
	/* now there are ac remaining args starting at av[0] */
	if (ac != 4) {
	    fprintf (stderr, "RA Dec FOV Mag are required\n");
	    usage (progname);
	}

	if (scansex (av[0], &ra) < 0) {
	    fprintf (stderr, "Bad RA format\n");
	    usage (progname);
	}
	ra = hrrad(ra);
	if (scansex (av[1], &dec) < 0) {
	    fprintf (stderr, "Bad Dec format\n");
	    usage (progname);
	}
	dec = degrad(dec);
	fov = degrad(atof (av[2]));
	fmag = atof (av[3]);

	/* estimate number of stars we can expect, based on 10**(M/7)/deg**2 */
	nexpt = (int)(pow(10.0, fmag/5) * raddeg(fov)*raddeg(fov));
	if (lflag && nexpt > MAXEXPT) {
	    fprintf (stderr, "Potentially too many stars to print.\n");
	    exit(1);
	}

	if (GSCSetup (mflag?NULL:cdpath, aflag?NULL:chpath, msg) < 0) {
	    fprintf (stderr, "%s\n", msg);
	    exit(1);
	}

	gs = NULL;
	n = GSCFetch (ra, dec, fov, fmag, lflag ? &gs : NULL, 0, msg);
	if (n < 0)
	    fprintf (stderr, "%s\n", msg);
	else if (lflag)
	    listStars (gs, n);
	else
	    printf ("%d\n", n);
	
	return (0);
}

static void
usage (char *p)
{
    FILE *fp = stderr;

    fprintf(fp,"%s: [options] RA Dec FOV Mag\n", p);
    fprintf(fp,"Options:\n");
    fprintf(fp,"  -r dir:  alternate CDROM mount point; default is %s\n",
								cdpath_default);
    fprintf(fp,"  -c dir:  alternate cache base directory; default is %s\n",
								chpath_default);
    fprintf(fp,"  -l:      list the stars on stdout: name,type,ra,dec,mag\n");
    fprintf(fp,"  -a:      don't attempt to use the cache files\n");
    fprintf(fp,"  -m:      don't attempt to use the CD-ROM\n");
    fprintf(fp,"\n");
    fprintf(fp,"Required:\n");
    fprintf(fp,"  RA:      Center RA: HH:MM:SS\n");
    fprintf(fp,"  Dec:     Center Dec: DD:MM:SS\n");
    fprintf(fp,"  FOV:     Field-of-View, degrees\n");
    fprintf(fp,"  Mag:     faintest mag desired\n");
    fprintf(fp,"\n");
    fprintf(fp,"To load entire northern CDROM (~86MB):\n");
    fprintf(fp,"  gscload 0 90 194 20\n");
    fprintf(fp,"To load entire southern CDROM (~73MB):\n");
    fprintf(fp,"  gscload 0 -90 164 20\n");


    exit (1);
}

static void
listStars (ObjF *sp, int n)
{
	char rastr[32], decstr[32];

	while (--n >= 0) {
	    Obj *op = (Obj *)sp;
	    fs_sexa (rastr, radhr(op->f_RA), 2, 360000);
	    fs_sexa (decstr, raddeg(op->f_dec), 4, 36000);
	    printf ("%s,f|%c,%s,%s,%g\n", op->o_name, op->f_class,
					    rastr, decstr, op->f_mag/MAGSCALE);
	    sp++;
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: gscload.c,v $ $Date: 2001/04/19 21:12:06 $ $Revision: 1.1.1.1 $ $Name:  $"};
