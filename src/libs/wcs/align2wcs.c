#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "P_.h"
#include "astro.h"
#include "fits.h"
#include "wcs.h"


/* find the shift dx and dy of fip2 so it most closely matches fip1.
 * we require the C* WCS fields for this.
 * return 0 if ok, else < 0 with an excuse in msg[] if can't do it.
 * N.B. both images ust be same size, at same scale, and we ignore rotation.
 */
int
align2WCS (fip1, fip2, dxp, dyp, msg)
FImage *fip1, *fip2;
int *dxp, *dyp;
char msg[];
{
	double ra1, dec1;	/* image center of fip1 */
	double x2, y2;		/* where that is on fip2 */

	if (checkWCSFITS (fip1, 0) < 0) {
	    sprintf (msg, "no WCS fields in reference image");
	    return (-1);
	}
	if (checkWCSFITS (fip2, 0) < 0) {
	    sprintf (msg, "no WCS fields in comparison image");
	    return (-2);
	}
	if (xy2RADec (fip1, fip1->sw/2.0, fip1->sh/2.0, &ra1, &dec1) < 0) {
	    sprintf (msg, "xy2RADec() failed");
	    return (-3);
	}
	if (RADec2xy (fip2, ra1, dec1, &x2, &y2) < 0) {
	    sprintf (msg, "RADec2xy() failed");
	    return (-4);
	}

	*dxp = fip1->sw/2 - x2;
	*dyp = fip1->sh/2 - y2;

	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: align2wcs.c,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $"};
