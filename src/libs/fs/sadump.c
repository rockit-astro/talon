/* same program to read a patch from the SA1.0 CDROM.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "fieldstar.h"

static void usage (void);

static char cdpath_def[] = "/mnt/cdrom";
static char *cdpath = cdpath_def;
static char *pname;

int
main (int ac, char *av[])
{
	char msg[1024];
	double ra, dec, f;
	char *arg;
	FieldStar *tsp, *sp;
	int nogsc = 0;
	int n;

	pname = av[0];

	while ((--ac > 0) && ((*++av)[0] == '-')) {
	    char *s;
	    for (s = av[0]+1; *s != '\0'; s++)
		switch (*s) {
		case 'c':
		    if (ac < 2)
			usage();
		    cdpath = *++av;
		    ac--;
		    break;
		case 'g':
		    nogsc = 1;
		    break;
		default:
		    usage();
		}
	}

	/* ac remaiing args starting at av[0] */
	if (ac != 3)
	    usage();

	/* check for CDROM */
	if (USNOSetup (cdpath, !nogsc, msg) < 0) {
	    fprintf (stderr, "%s: %s\n", cdpath, msg);
	    exit(1);
	}

	/* grab region */
	if (scansex (arg = *av++, &ra) < 0) {
	    fprintf (stderr, "Bad RA format: %s\n", arg);
	    exit (1);
	}
	ra = hrrad(ra);
	if (scansex (arg = *av++, &dec) < 0) {
	    fprintf (stderr, "Bad Dec format: %s\n", arg);
	    exit (1);
	}
	dec = degrad(dec);
	if (scansex (arg = *av++, &f) < 0) {
	    fprintf (stderr, "Bad Field format: %s\n", arg);
	    exit (1);
	}
	f = degrad(f);

	/* go get 'em */
	n = USNOFetch (ra, dec, f, 20.0, &sp, msg);
	if (n < 0) {
	    fprintf (stderr, "Fetch error: %s\n", msg);
	    exit(1);
	}

	/* print 'em */
	for (tsp = sp; tsp < &sp[n]; tsp++) {
	    printf ("%-13s,f", tsp->name);
	    fs_sexa (msg, radhr(tsp->ra), 2, 36000);
	    printf (",%s", msg);
	    fs_sexa (msg, raddeg(tsp->dec), 3, 3600);
	    printf (",%s", msg);
	    printf (",%5.2f\n", tsp->mag);
	}

	return (0);
}

static void
usage()
{
	fprintf (stderr, "%s: [options] RA Dec Field\n", pname);
	fprintf (stderr, "  -c path: alternate cdrom path. default is %s\n",
								cdpath_def);
	fprintf (stderr, "  -g     : discard GSC stars\n");
	fprintf (stderr, "       RA: field center, H:M:S\n");
	fprintf (stderr, "      Dec: field center, D:M:S\n");
	fprintf (stderr, "    Field: field diameter, D:M:S\n");

	exit (1);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: sadump.c,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $"};
