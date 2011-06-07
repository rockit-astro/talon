/* stubs to support xephem references */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "preferences.h"

extern double deltat (double Mjd);
extern double atod P_((char *buf));

void
pm_set (int p)
{
}

void
xe_msg (char *msg, int app_modal)
{
	fprintf (stderr, "%s\n", msg);
}

FILE *
fopenh (name, how)
char *name;
char *how;
{
	return (fopen (name, how));
}

int
existsh (name)
char *name;
{
	FILE *fp = fopen (name, "r");

	if (fp) {
	    fclose (fp);
	    return (0);
	}
	return (-1);
}

char *
expand_home (path)
char *path;
{
	return (strcpy (malloc (strlen(path)+1), path));
}


char *
syserrstr ()
{
	return (strerror(errno));
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: stubs.c,v $ $Date: 2001/04/19 21:12:06 $ $Revision: 1.1.1.1 $ $Name:  $"};
