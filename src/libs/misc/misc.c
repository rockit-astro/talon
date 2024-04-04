#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "misc.h"


/* insure *hap is in range -PI .. PI and *decp -PI/2 .. PI/2, wrap as needed. */
void
hdRange (double *hap, double *decp)
{
	rdRange (hap, decp);
	haRange (hap);
}

/* insure *rap is in range 0 .. 2*PI and *decp -PI/2 .. PI/2, wrap as needed. */
void
rdRange (double *rap, double *decp)
{
	if (*decp > PI/2) {
	    *decp = PI - *decp;
	    *rap += PI;
	}
	if (*decp < -PI/2) {
	    *decp = -PI - *decp;
	    *rap += PI;
	}

	range (rap, 2*PI);
}

/* insure *hap is in range -PI .. PI */
void
haRange (double *hap)
{
	range (hap, 2*PI);
	if (*hap > PI)
	    *hap -= 2*PI;
}

/* return the modified Julian date now
 * (see astro.h for definition)
 */
double
mjd_now()
{
	struct timeval tv;
	double t;

	(void) gettimeofday (&tv, (struct timezone *)0);

	/* tv is seconds since 00:00:00 1/1/1970 UTC on UNIX systems;
	 * mjd was 25567.5 then.
	 */
	t = (double)tv.tv_sec + tv.tv_usec/1000000.0;
	return (25567.5 + t/SPD);
}

/* given a Now, return UTC, in hours */
double
utc_now (np)
Now *np;
{
	return ((np->n_mjd-mjd_day(np->n_mjd)) * 24.0);
}
