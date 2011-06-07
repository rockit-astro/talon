/* tool to read an .ini file and use it to insmod apogee.o */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#include "telenv.h"
#include "crackini.h"

static void usage (void);
static void buildapg0 (char *fname, char *apg0);
static void rmmod (void);
static void insmod (char *driver, char *apg0, int impact, int trace);
static char *getp (IniList *lp, char *fname, char *section, char *name);

static char defdriver[] = "dev/apogee.o";
static char *pname;
static int tflag;
static int vflag;

int
main (int ac, char *av[])
{
	int root = !geteuid();
	char driver[1024];
	char fname[1024];
	char apg0[1024];
	int impact;

	/* save our name */
	pname = av[0];

	/* prepare default driver name */
	telfixpath (driver, defdriver);

	/* fetch args */
	while ((--ac > 0) && ((*++av)[0] == '-')) {
	    char *s;
	    for (s = av[0]+1; *s != '\0'; s++)
		switch (*s) {
		case 'd':	/* alternate driver */
		    if (ac < 2)
			usage();
		    strcpy (driver, *++av);
		    ac--;
		    break;

		case 't':	/* driver trace on */
		    tflag++;
		    break;

		case 'v':	/* verbose */
		    vflag++;
		    break;

		default:
		    usage();
		}
	}

	/* ac remaining args starting at av[0] */
	if (ac != 2)
	    usage();
	strcpy (fname, av[0]);
	impact = atoi(av[1]);

	/* go -- each exit if trouble */
	buildapg0 (fname, apg0);
	if (vflag)
	    fprintf (stderr, "apg0=%s\n", apg0);
	if (root) {
	    rmmod();
	    insmod (driver, apg0, impact, tflag);
	} else if (vflag)
	    fprintf (stderr, "==> Not root so not actually doing anything\n");

	return (0);
}

static void
usage()
{
	fprintf (stderr, "%s: [options] inifile impact\n", pname);
	fprintf (stderr, "Purpose: install apogee driver using an original Apogee .ini file.\n");
	fprintf (stderr, "Options:\n");
	fprintf (stderr, "  -d file: specify an alternate driver\n");
	fprintf (stderr, "           default is $TELHOME/%s\n", defdriver);
	fprintf (stderr, "  -t:      turn on driver trace\n");
	fprintf (stderr, "  -v:      verbose output\n");

	exit (1);
}

/* run rmmod() if loaded */
static void
rmmod()
{
	if (!system ("cat /proc/modules | grep -s apogee")) {
	    if (vflag)
		fprintf (stderr, "Removing apogee module\n");
	    if (system ("/sbin/rmmod apogee"))
		exit(1);
	}
}

/* fetch the params */
static void
buildapg0 (char *fname, char *apg0)
{
	static char syssec[] = "system";
	static char geosec[] = "geometry";
	static char tmpsec[] = "temp";
	static char regsec[] = "camreg";
	char telpath[1024];
	IniList *lp;
	int nr, nc, bir, bic;
	char *vp;

	/* read -- try fname straight, then with fixpath */
	if (vflag)
	    fprintf (stderr, "reading %s\n", fname);
	lp = iniRead (fname);
	if (!lp) {
	    if (errno == ENOENT && fname[0] != '/') {
		sprintf (telpath, "archive/config/%s", fname);
		fname = telpath;
		telfixpath (fname, fname);
		lp = iniRead (fname);
	    }
	    if (!lp) {
		fprintf (stderr, "%s: %s\n", fname, strerror(errno));
		exit(1);
	    }
	}

	/* init apg0 */
	apg0[0] = '\0';

	/* get and append each */
	vp = getp (lp, fname, syssec, "base");
	apg0 += sprintf (apg0, "0x%s", vp);	/* already hex */

	vp = getp (lp, fname, geosec, "rows");
	apg0 += sprintf (apg0, ",%d", nr = atoi(vp));

	vp = getp (lp, fname, geosec, "columns");
	apg0 += sprintf (apg0, ",%d", nc = atoi(vp));

	vp = getp (lp, fname, geosec, "bir");
	apg0 += sprintf (apg0, ",%d", bir = atoi(vp));

	vp = getp (lp, fname, geosec, "imgrows");
	apg0 += sprintf (apg0, ",%d", nr - bir - atoi(vp));

	vp = getp (lp, fname, geosec, "bic");
	apg0 += sprintf (apg0, ",%d", bic = atoi(vp));

	vp = getp (lp, fname, geosec, "imgcols");
	apg0 += sprintf (apg0, ",%d", nc - bic - atoi(vp));

	vp = getp (lp, fname, tmpsec, "cal");
	apg0 += sprintf (apg0, ",%d", atoi(vp));

	vp = getp (lp, fname, tmpsec, "scale");
	apg0 += sprintf (apg0, ",%.0f", 100*atof(vp));

	vp = getp (lp, fname, syssec, "tscale");
	apg0 += sprintf (apg0, ",%.0f", 100*atof(vp));

	vp = getp (lp, fname, syssec, "caching");
	apg0 += sprintf (apg0, ",%d", strcasecmp(vp,"off") ? 1 : 0);

	vp = getp (lp, fname, syssec, "cable");
	apg0 += sprintf (apg0, ",%d", strcasecmp(vp,"short") ? 1 : 0);

	vp = getp (lp, fname, syssec, "mode");
	apg0 += sprintf (apg0, ",%d", atoi(vp));

	vp = getp (lp, fname, syssec, "test");
	apg0 += sprintf (apg0, ",%d", atoi(vp));

	vp = getp (lp, fname, syssec, "data_bits");
	apg0 += sprintf (apg0, ",%d", atoi(vp) == 16 ? 1 : 0);

	vp = getp (lp, fname, regsec, "gain");
	apg0 += sprintf (apg0, ",%d", atoi(vp));

	vp = getp (lp, fname, regsec, "opt1");
	apg0 += sprintf (apg0, ",%d", atoi(vp));

	vp = getp (lp, fname, regsec, "opt2");
	apg0 += sprintf (apg0, ",%d", atoi(vp));

	/* finished */
	iniFree (lp);
}

/* run insmod() */
static void
insmod (char *driver, char *apg0, int impact, int trace)
{
	char cmd[1024];
	int insstatus;

	sprintf (cmd, "/sbin/insmod -f %s apg0=%s ap_impact=%d ap_trace=%d",
						driver, apg0, impact, trace);
	if (vflag)
	    fprintf (stderr, "%s\n", cmd);
	insstatus = system (cmd);
	system ("grep apogee /var/log/messages | tail -3");
	if (insstatus != 0)
	    exit (1);
}

static char *
getp (IniList *lp, char *fname, char *section, char *name)
{
	char *vp;

	if (!(vp = iniFind(lp, section, name))) {
	    fprintf (stderr, "%s: No %s.%s\n", fname, section, name);
	    exit (1);
	}

	return (vp);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: insapogee.c,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $"};
