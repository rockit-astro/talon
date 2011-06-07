#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "P_.h"
#include "astro.h"
#include "fits.h"
#include "wcs.h"

/* reset the WCS fields in fip1 according to the bounding box within fip0.
 *    set CRVAL1 to RA of box center;
 *    set CRVAL2 to Dec of box center;
 *    set CRPIX1 to half-width of box
 *    set CRPIX2 to half-height of box
 * N.B. we assume the other fields have already been copied from fip0 to fip1.
 */
void
resetWCS (fip0, fip1, x, y, w, h)
FImage *fip0;
FImage *fip1;
int x, y, w, h;
{
	double ra, dec;		/* center of bounding box */

	(void) xy2RADec (fip0, x+w/2.0, y+h/2.0, &ra, &dec);
	setRealFITS (fip1, "CRVAL1", raddeg(ra), 10, "RA at CRPIX1, degrees");
	setRealFITS (fip1, "CRVAL2", raddeg(dec),10, "Dec at CRPIX2, degrees");
	setRealFITS (fip1, "CRPIX1", w/2.0, 10, "RA reference pixel index");
	setRealFITS (fip1, "CRPIX2", h/2.0, 10, "Dec reference pixel index");
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: resetwcs.c,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $"};
