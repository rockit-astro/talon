/* misc handy functions.
 * every system has such, no?
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"

/* zero from loc for len bytes */
void
zero_mem (loc, len)
void *loc;
unsigned len;
{
	(void) memset (loc, 0, len);
}

/* return a string for whatever is in errno right now.
 * I never would have imagined it would be so hard to turn errno into a string!
 */
char *
syserrstr ()
{
#if defined(__STDC__)
/* gcc doesn't have strerror */
#include <errno.h>
#include <string.h>
	return (strerror (errno));
#else
#if defined(VMS)
#include <errno.h>
#include <perror.h>
#else
	extern char *sys_errlist[];
	extern int errno;
#endif
	return (sys_errlist[errno]);
#endif
}

/* insure 0 <= *v < r.
 */
void
range (v, r)
double *v, r;
{
	*v -= r*floor(*v/r);
}

double
mjd_2000()
{
	return (2451544.5 - MJD0);
}

void
pm_set (p)
int p;
{
}

/* given a string of the form xx:mm[:ss] or xx:mm.dd, convert it to a double at
 * *dp. actually, ':' may also be ';', '/' or ',' too. all components may be 
 * floats.
 * return -1 if trouble, else 0
 */
int
scansex (str, dp)
char *str;
double *dp;
{
	double x, m = 0.0, s = 0.0;
	int isneg;
	int nf;

	while (isspace(*str))
	    str++;
	if (*str == '-') {
	    isneg = 1;
	    str++;
	} else
	    isneg = 0;

	nf = sscanf (str, "%lf%*[:;/,]%lf%*[:;/,]%lf", &x, &m, &s);
	if (nf < 1)
	    return (-1);
	*dp = x + m/60.0 + s/3600.0;
	if (isneg)
	    *dp = - *dp;
	return (0);
}

/* sprint the variable a in sexagesimal format into out[].
 * w is the number of spaces for the whole part.
 * fracbase is the number of pieces a whole is to broken into; valid options:
 *	360000:	<w>:mm:ss.ss
 *	36000:	<w>:mm:ss.s
 *	3600:	<w>:mm:ss
 *	600:	<w>:mm.m
 *	60:	<w>:mm
 */
void
fs_sexa (out, a, w, fracbase)
char *out;
double a;
int w;
int fracbase;
{
	unsigned long n;
	int d;
	int f;
	int m;
	int s;
	int isneg;

	/* save whether it's negative but do all the rest with a positive */
	isneg = (a < 0);
	if (isneg)
	    a = -a;

	/* convert to an integral number of whole portions */
	n = (unsigned long)(a * fracbase + 0.5);
	d = n/fracbase;
	f = n%fracbase;

	/* form the whole part; "negative 0" is a special case */
	if (isneg && d == 0)
	    (void) sprintf (out, "%*s-0", w-2, "");
	else
	    (void) sprintf (out, "%*d", w, isneg ? -d : d);
	out += w;

	/* do the rest */
	switch (fracbase) {
	case 60:	/* dd:mm */
	    m = f/(fracbase/60);
	    (void) sprintf (out, ":%02d", m);
	    break;
	case 600:	/* dd:mm.m */
	    (void) sprintf (out, ":%02d.%1d", f/10, f%10);
	    break;
	case 3600:	/* dd:mm:ss */
	    m = f/(fracbase/60);
	    s = f%(fracbase/60);
	    (void) sprintf (out, ":%02d:%02d", m, s);
	    break;
	case 36000:	/* dd:mm:ss.s*/
	    m = f/(fracbase/60);
	    s = f%(fracbase/60);
	    (void) sprintf (out, ":%02d:%02d.%1d", m, s/10, s%10);
	    break;
	case 360000:	/* dd:mm:ss.ss */
	    m = f/(fracbase/60);
	    s = f%(fracbase/60);
	    (void) sprintf (out, ":%02d:%02d.%02d", m, s/100, s%100);
	    break;
	default:
	    printf ("fs_sexa: unknown fracbase: %d\n", fracbase);
	    exit(1);
	}
}

/* if path starts with `~' replace with $HOME.
 * we also remove any leading or trailing blanks.
 * N.B. string returned will always be a copy of path but caller should
 *   never free it.
 */
char *
expand_home (path)
char *path;
{
	static char *mpath;
	static char *home;
	static int homel;
	char *p;
	int l;

	/* get home, if we don't already know it */
	if (!home) {
	    home = getenv ("HOME");
	    if (home)
		homel = strlen (home);
	}

	/* start mpath so we can always just use realloc */
	if (!mpath)
	    mpath = malloc (1);

	/* skip leading blanks */
	l = strlen (path);
	while (*path == ' ') {
	    path++;
	    l--;
	}

	/* move '\0' back past any trailing blanks */
	for (p = &path[l-1]; p >= path && *p == ' '; --l)
	    *p-- = '\0';

	/* prepend home if starts with ~ */
	if (path[0] == '~' && home)
	    sprintf (mpath = realloc (mpath, homel+l), "%s%s", home, path+1);
	else
	    strcpy (mpath = realloc(mpath, l+1), path);

	return (mpath);
}

/* like fopen() but substitutes HOME if name starts with '~'
 */
FILE *
fopenh (name, how)
char *name;
char *how;
{
	return (fopen (expand_home(name), how));
}

/* like open(2) but substitutes HOME if name starts with '~'.
 */
int
openh (name, flags, perm)
char *name;
int flags;
int perm;
{
	return (open (expand_home(name), flags, perm));
}

/* return 0 if the given file exists, else -1.
 * substitute HOME if name starts with '~'.
 */
int
existsh (name)
char *name;
{
	int fd = openh (name, O_RDONLY, 0);

	if (fd < 0)
	    return (-1);
	(void) close (fd);
	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: support.c,v $ $Date: 2001/04/19 21:12:06 $ $Revision: 1.1.1.1 $ $Name:  $"};
