/* read the given .sch files and report any syntax problems.
 * keep going but exit with 0 only if all are ok.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "strops.h"
#include "telenv.h"
#include "catalogs.h"
#include "configfile.h"
#include "scan.h"

#define CHKSCH
#include "telsched.h"

static void usage (char *progname);

static char *filename;
static char catdir[1024];
static char filtercfg[1024];

FilterInfo *filterp;
int nfilt;


int
main (int ac, char *av[])
{
	char *progname = av[0];
	int vflag = 0;
	char buf[1024];
	char *str;
	int status;
	int def;

	/* crack arguments */
	for (av++; --ac > 0 && *(str = *av) == '-'; av++) {
	    char c;
	    while ((c = *++str) != '\0')
		switch (c) {
		case 'v':
		    vflag++;
		    break;
		default:
		    usage(progname);
		    break;
		}
	}

	/* now there are ac remaining args starting at av[0] */
	if (ac == 0)
	    usage (progname);

	/* full paths */
	telfixpath (catdir, "archive/catalogs");
	telfixpath (filtercfg, "archive/config/filter.cfg");

	/* read list of available filters */
	nfilt = readFilterCfg (vflag, filtercfg, &filterp, &def, buf);
	if (nfilt < 0) {
	    fprintf (stderr, "%s: %s\n", basenm(filtercfg), buf);
	    exit (1);
	}

	/* read all the files. set status = 1 if any have trouble */
	status = 0;
	while (ac-- > 0) {
	    Obs *op = NULL;
	    filename = *av++;

	    if (readObsFile (filename, &op) < 0)
		status = 1;
	}

	/* exit with net status */
	return (status);
}

static void
usage (char *progname)
{
	fprintf(stderr,"%s: [-v] *.sch ...\n", progname);
	exit (1);
}

/* alternate versions for us */

void
msg (char *fmt, ...)
{
	va_list ap;

	/* N.B. mksch and perl scripts depend on this format */
	fprintf (stderr, "%s: ", basenm(filename));

	va_start (ap, fmt);
	vfprintf (stderr, fmt, ap);
	va_end (ap);

	fprintf (stderr, "\n");
}

/* using op->scan.obj.o_name, fill in op->scan.obj */
int
searchCatEntry (Obs *op)
{
	char m[1024];

	if (op->scan.obj.o_name[0] == '\0') {
	    msg ("No source object");
	    return (-1);
	}

	if (searchDirectory (catdir,op->scan.obj.o_name,&op->scan.obj,m) < 0) {
	    msg ("%s: %s", op->scan.obj.o_name, m);
	    return (-1);
	}

	return (0);
}

/* stubs that mean little or nothing to obs.c */
int COMPRESSH=1;
double LSTDELTADEF=0;
double CAMDIG_MAX=0;
char imdir[] = "dummy";
int DEFBIN=1, DEFIMW=100, DEFIMH=100;
double lst2utc(double l){return (0.0);}
void computeCir(Obs *op){}
int dateistoday (char *s) {return (0);}
void get_obsarray (Obs **opp, int *ip) {*ip = 0;};

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: chksch.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
