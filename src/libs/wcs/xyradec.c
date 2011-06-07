#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "P_.h"
#include "astro.h"
#include "fits.h"
#include "wcs.h"

/* given an x/y location over the given image, return ra and dec, in rads.
 * we require the C* fields in the header.
 * return 0 if all ok, else -1.
 */
int
xy2RADec (fip, x, y, rap, decp)
FImage *fip;
double x, y;
double *rap, *decp;
{
	double xref;	/* x reference coordinate value (deg) */
	double yref;	/* y reference coordinate value (deg) */
	double xrefpix;	/* x reference pixel */
	double yrefpix;	/* y reference pixel */
	double xinc;	/* x coordinate increment (deg) */
	double yinc;	/* y coordinate increment (deg) */
	double rot;	/* rotation (deg)  (from N through E) */
	char *type;	/* projection type code e.g. "-SIN" */
	double xpos;	/* x (RA) coordinate (deg) */
	double ypos;	/* y (dec) coordinate (deg) */
	int i;
	char typestr[128];

	if (getRealFITS (fip, "CRVAL1", &xref) < 0) return (-1);
	if (getRealFITS (fip, "CRVAL2", &yref) < 0) return (-1);
	if (getRealFITS (fip, "CRPIX1", &xrefpix) < 0) return (-1);
	if (getRealFITS (fip, "CRPIX2", &yrefpix) < 0) return (-1);
	if (getRealFITS (fip, "CDELT1", &xinc) < 0) return (-1);
	if (getRealFITS (fip, "CDELT2", &yinc) < 0) return (-1);
	if (getRealFITS (fip, "CROTA2", &rot) < 0) return (-1);
	if (getStringFITS (fip, "CTYPE1", typestr) < 0) return (-1);
	if (strncmp (typestr, "RA--", 4)) return (-1);
	type = typestr + 4;

	/* CRPIXn assume pixels are 1-based */
	i = worldpos(x+1, y+1, xref, yref, xrefpix, yrefpix, xinc, yinc,
					      rot, type, &xpos, &ypos);

	if (i != 0) return (-1);

	*rap = degrad (xpos);
	*decp = degrad (ypos);

	return (0);
}

/* given an ra and dec, in rads, return x/y location over the given image.
 * we require the C* fields in the header.
 * return 0 if all ok, else -1.
 */
int
RADec2xy (fip, ra, dec, xp, yp)
FImage *fip;
double ra, dec;
double *xp, *yp;
{
	double xpos;	/* x (RA) coordinate (deg) */
	double ypos;	/* y (dec) coordinate (deg) */
	double xref;	/* x reference coordinate value (deg) */
	double yref;	/* y reference coordinate value (deg) */
	double xrefpix;	/* x reference pixel */
	double yrefpix;	/* y reference pixel */
	double xinc;	/* x coordinate increment (deg) */
	double yinc;	/* y coordinate increment (deg) */
	double rot;	/* rotation (deg)  (from N through E) */
	char *type;	/* projection type code e.g. "-SIN" */
	int i;
	char typestr[128];

	xpos = raddeg(ra);
	ypos = raddeg(dec);
	if (getRealFITS (fip, "CRVAL1", &xref) < 0) return (-1);
	if (getRealFITS (fip, "CRVAL2", &yref) < 0) return (-1);
	if (getRealFITS (fip, "CRPIX1", &xrefpix) < 0) return (-1);
	if (getRealFITS (fip, "CRPIX2", &yrefpix) < 0) return (-1);
	if (getRealFITS (fip, "CDELT1", &xinc) < 0) return (-1);
	if (getRealFITS (fip, "CDELT2", &yinc) < 0) return (-1);
	if (getRealFITS (fip, "CROTA2", &rot) < 0) return (-1);
	if (getStringFITS (fip, "CTYPE1", typestr) < 0) return (-1);
	if (strncmp (typestr, "RA--", 4)) return (-1);
	type = typestr + 4;

	i = xypix(xpos, ypos, xref, yref, xrefpix, yrefpix, xinc, yinc,
					      rot, type, xp, yp);

	if (i != 0) return (-1);

	/* CRPIXn assume pixels are 1-based */
	*xp -= 1;
	*yp -= 1;

	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: xyradec.c,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $"};
