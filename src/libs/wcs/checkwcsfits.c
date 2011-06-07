/*
 * Updated version: Merges DJA version "Distance" WCS/Astrometry work
 * with standard Talon libraries.
 *
 * Control Compilation via wcs.h header #define USE_DISTANCE_METHOD
 * Set to 1 (to enable) or 0 (to disable, and use former triangle method only)
 *
 * Last update DJA 020917
 * Merged STO 020930
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "P_.h"
#include "astro.h"
#include "fits.h"
#include "wcs.h"

/*
	CVS History:
		$Id: checkwcsfits.c,v 1.2 2002/10/01 19:48:24 steve Exp $
		$Log: checkwcsfits.c,v $
		Revision 1.2  2002/10/01 19:48:24  steve
		Added David Asher's WCS/Astrometry changes as a compile option
		
*/

#if USE_DISTANCE_METHOD
 /* check the WCS, AMDX*, AMDY* fields and print any that are found if verbose
 * (AMD* fields only printed if nonzero).
 * return 0 if all are found, -1 if any WCS field missing, -2 if any AMD*
 * missing, -3 if at least one WCS & one AMD* missing.
 */
#else
/* check the WCS fields and print any that are found if verbose.
 * return 0 if all are found, else -1.
 */
#endif
int
checkWCSFITS (FImage *fip, int verbose)
{
	char str[128];
	double v;
	int n;
#if USE_DISTANCE_METHOD
	int i,m;
#endif	

	n = 0;

	if (getStringFITS (fip, "CTYPE1", str) == 0) {
	    if (verbose) printf ("CTYPE1 = %s\n", str);
	    n++;
	}
	if (getRealFITS (fip, "CRVAL1", &v) == 0) {
	    if (verbose) printf ("CRVAL1 = %g\n", v);
	    n++;
	}
	if (getRealFITS (fip, "CDELT1", &v) == 0) {
	    if (verbose) printf ("CDELT1 = %g\n", v);
	    n++;
	}
	if (getRealFITS (fip, "CRPIX1", &v) == 0) {
	    if (verbose) printf ("CRPIX1 = %g\n", v);
	    n++;
	}
	if (getRealFITS (fip, "CROTA1", &v) == 0) {
	    if (verbose) printf ("CROTA1 = %g\n", v);
	    n++;
	}

	if (getStringFITS (fip, "CTYPE2", str) == 0) {
	    if (verbose) printf ("CTYPE2 = %s\n", str);
	    n++;
	}
	if (getRealFITS (fip, "CRVAL2", &v) == 0) {
	    if (verbose) printf ("CRVAL2 = %g\n", v);
	    n++;
	}
	if (getRealFITS (fip, "CDELT2", &v) == 0) {
	    if (verbose) printf ("CDELT2 = %g\n", v);
	    n++;
	}
	if (getRealFITS (fip, "CRPIX2", &v) == 0) {
	    if (verbose) printf ("CRPIX2 = %g\n", v);
	    n++;
	}
	if (getRealFITS (fip, "CROTA2", &v) == 0) {
	    if (verbose) printf ("CROTA2 = %g\n", v);
	    n++;
	}

#if USE_DISTANCE_METHOD
	m = 0;

	for (i = 0; i < 14; i++) {
	  sprintf (str, "AMDX%i", i);
	  if (getRealFITS (fip, str, &v) == 0) {
	    if (verbose && v != 0) printf ("%s = %-20G", str, v);
	    m++;
	  }
	  sprintf (str, "AMDY%i", i);
	  if (getRealFITS (fip, str, &v) == 0) {
	    if (verbose && v != 0) printf ("%s = %G\n", str, v);
	    m++;
	  }
	}

	return ( (m == 28 ? 0 : -2) + (n == 10 ? 0 : -1) );
#else	
	return (n == 10 ? 0 : -1);
#endif
}
