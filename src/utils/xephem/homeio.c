/* functions to support paths relative to HOME, fifos, and other misc io. */

#include <stdio.h>
/* stdio.h only declares fdopen() for POSIX, not STDC */
#if !defined(_POSIX_SOURCE)
extern FILE *fdopen();
#endif
#include <ctype.h>
#include <math.h>

#if defined(__STDC__)
#include <stdlib.h>
#include <string.h>
#else
extern char *getenv();
extern void *malloc();
extern void *realloc();
#endif

#ifdef _POSIX_SOURCE
#include <unistd.h>
#else
extern int close();
#endif

#include "P_.h"
#include "astro.h"
#include "circum.h"

/* look around for O_WRONLY. Older VMS had it in xlib.h; some VMS has it in
 * fcntl.h (but not all have this file); some now have it in file.h. geeze.
 */
#if defined(VMS) && !defined(O_WRONLY)
#include <file.h>
#endif
#if !defined(O_WRONLY)
#include <fcntl.h>
#endif

extern char *getXRes P_((char *name, char *def));
extern void db_write_line P_((Obj *op, char line[]));
extern void xe_msg P_((char *msg, int app_modal));

static int open_wfifores P_((char *fn, FILE **fpp));

/* if path starts with `~' replace with $HOME.
 * we also remove any leading or trailing blanks.
 * caller should save returned string before calling us again.
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

/* get the anchor for all of xephem's support files.
 * use TELHOME env first, else ShareDir X resource, else current dir.
 */
char *
getShareDir()
{
	static char *basedir;

	if (!basedir) {
	    char *th = getenv ("TELHOME");
	    if (th) {
		basedir = malloc (strlen(th) + 10);
		if (basedir)
		    (void) sprintf (basedir, "%s/xephem", th);
	    }
	    if (!basedir)
		basedir = getXRes ("ShareDir", ".");
	}

	return (basedir);

}

/* return a string for whatever is in errno right now.
 * I never would have imagined it would be so crazy to turn errno into a string!
 */
char *
syserrstr ()
{
#if defined(__STDC__)
/* some older gcc don't have strerror */
#include <errno.h>
return (strerror (errno));
#else
#if defined(VMS)
#include <errno.h>
#include <perror.h>
#else
#if !defined(__FreeBSD__) && !defined(__EMX__)
/* this is aready in stdio.h on FreeBSD */
/* this is already in stdlib.h in EMX   M. Goldberg 27 January 1997 for OS/2 */
extern char *sys_errlist[];
#endif
extern int errno;
#endif
return (sys_errlist[errno]);
#endif
}

/* send the given object to the loc_fifo if alt>0.
 * open/close fifo each time.
 * if op == NULL then just test the fifo.
 * return 0 if all ok, else xe_msg() and return -1.
 * N.B. we assume SIGPIPE is being ignored.
 */
int
skyloc_fifo (op)
Obj *op;
{
	char line[1024];
	char fn[1024];
	FILE *fp;

	/* check vis */
	if (op && op->s_alt < 0) {
	    sprintf (line, "%s is below the horizon", op->o_name);
	    xe_msg (line, 1);
	    return (-1);
	}

	/* open fifo */
	(void) sprintf (fn, "%s/fifos/xephem_loc_fifo",  getShareDir());
	if (open_wfifores (fn, &fp) < 0)
	    return(-1);
	if (!op) {
	    /* just testing the fifo */
	    (void) fclose (fp);
	    return (0);
	}

	/* send the formatted location string */
	db_write_line (op, line);
	if (fprintf (fp, "%s\n", line) < 0 || fflush (fp) != 0) {
	    (void) sprintf (line, "%s: %s\n", fn, syserrstr());
	    xe_msg (line, 1);
	    (void) fclose (fp);
	    return (-1);
	}

	(void) fclose (fp);
	return (0);
}

/* open the named fifo for write-only.
 * complain and return -1 if trouble, else return 0 and fill in *fpp;
 */
static int
open_wfifores (fn, fpp)
char *fn;
FILE **fpp;
{
#ifdef O_NONBLOCK               /* _POSIX_SOURCE */
#define NODELAY O_NONBLOCK
#else
#define NODELAY O_NDELAY
#endif
	char msg[1024];
	FILE *fp;
	int fd;

	/* open for write-only, non-blocking in case their's no reader.
	 * this prevents clogged writes too.
	 */
	fd = openh (fn, O_WRONLY|NODELAY, 0);
	if (fd < 0) {
	    (void) sprintf (msg,"%s: %s", fn, syserrstr());
	    xe_msg (msg, 1);
	    return (-1);
	}

	/* make into a FILE pointer for use with fprintf */
	fp = fdopen (fd, "w");
	if (!fp) {
	    (void) sprintf (msg, "Can not fdopen %s", fn);
	    xe_msg (msg, 0);
	    (void) close (fd);
	    return (-1);
	}

	*fpp = fp;

	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: homeio.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
