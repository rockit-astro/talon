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
		$Id: delwcsfits.c,v 1.2 2002/10/01 19:48:24 steve Exp $
		$History:$
*/

#if USE_DISTANCE_METHOD
/* delete all the C*, AMDX* and AMDY* fields.
 * return 0 if at least one such field is found, else -1.
 */
#else
/* delete all the C* fields.
 * return 0 if at least one such field is found, else -1.
 */
#endif
int
delWCSFITS (FImage *fip, int verbose)
{
#if USE_DISTANCE_METHOD
	static char *flds[] = {
	    "CTYPE1", "CRVAL1", "CDELT1", "CRPIX1", "CROTA1",
	    "CTYPE2", "CRVAL2", "CDELT2", "CRPIX2", "CROTA2",
	    "AMDX0", "AMDX1", "AMDX2", "AMDX3", "AMDX4",
	    "AMDX5", "AMDX6", "AMDX7", "AMDX8", "AMDX9",
	    "AMDX10", "AMDX11", "AMDX12", "AMDX13",
	    "AMDY0", "AMDY1", "AMDY2", "AMDY3", "AMDY4",
	    "AMDY5", "AMDY6", "AMDY7", "AMDY8", "AMDY9",
	    "AMDY10", "AMDY11", "AMDY12", "AMDY13"
	};
#else
	static char *flds[] = {
	    "CTYPE1", "CRVAL1", "CDELT1", "CRPIX1", "CROTA1",
	    "CTYPE2", "CRVAL2", "CDELT2", "CRPIX2", "CROTA2"
	};
#endif	
	int i;
	int n;

	n = 0;

	for (i = 0; i < sizeof(flds)/sizeof(flds[0]); i++) {
	    int s = delFImageVar (fip, flds[i]);
	    if (s == 0)
		n++;
	    if (verbose)
		printf ("%s: %s\n", flds[i], s == 0 ? "Deleted" : "Not found");
	}

	return (n > 0 ? 0 : -1);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: delwcsfits.c,v $ $Date: 2002/10/01 19:48:24 $ $Revision: 1.2 $ $Name:  $"};
