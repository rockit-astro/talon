/* code handle setting the C* WCS fields */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/ToggleB.h>
#include <Xm/TextF.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>

#include "P_.h"
#include "astro.h"
#include "xtools.h"
#include "fits.h"
#include "wcs.h"
#include "fieldstar.h"
#include "camera.h"

/* called when asked to set the WCS fields */
/* ARGSUSED */
void
setWCSCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	FImage *fip = &state.fimage;
	char str[1024];

	if (!fip->image) {
	    msg ("No image");
	    return;
	}
	if (state.lrflip || state.tbflip) {
	    msg ("Can not solve for WCS when image is flipped");
	    return;
	}

	watch_cursor (1);

	msg ("Searching for WCS solution...");
	stopchk_up(toplevel_w, "WCS Search", "Press Stop to abandon search");
	if (setWCSFITS (fip, !usno_ok, huntrad, stopchk, 0, str) < 0)
	    msg ("Solution failed: %s", str);
	else {
	    if (str[0])
		msg (str);
	    resetGSC();
	    updateFITS();
	    gscSetDialog();
	    refreshScene (0, 0, fip->sw, fip->sh);
	}

	stopchk_down();
	watch_cursor (0);
}

/* like xy2RADec() but accounts for flipping and precesses to user's epoch */
int
xy2rd (fip, x, y, rap, decp)
FImage *fip;
double x, y;
double *rap, *decp;
{
	double eq;

	if (state.lrflip)
	    x = fip->sw - x - 1;
	if (state.tbflip)
	    y = fip->sh - y - 1;
	if (xy2RADec (fip, x, y, rap, decp) < 0)
	    return (-1);

    printf("xy2RADec(%f, %f) returned (%f, %f)\n", x, y, *rap, *decp);

	if (!getRealFITS(fip,"EQUINOX",&eq) || !getRealFITS(fip,"EPOCH",&eq)) {
        printf("doing epoch correction\n");
	    double e = pEyear();
        printf("e=%f, eq=%f\n", e, eq);
	    if (e != eq) {
		double mjdeq;
		year_mjd (eq, &mjdeq);
		precess (mjdeq, pEmjd(), rap, decp);
	    }
	}

    printf("xy2rd(%f, %f) returning (%f, %f)\n", x, y, *rap, *decp);

	return (0);
}

/* like RADec2xy() but accounts for flipping and incoming is always 2000 */
int
rd2kxy (fip, ra, dec, xp, yp)
FImage *fip;
double ra, dec;
double *xp, *yp;
{
	double eq;

	if (!getRealFITS(fip,"EQUINOX",&eq) || !getRealFITS(fip,"EPOCH",&eq)) {
	    if (eq != 2000) {
		double mjdeq;
		year_mjd (eq, &mjdeq);
		precess (J2000, mjdeq, &ra, &dec);
	    }
	}
	if (RADec2xy (fip, ra, dec, xp, yp) < 0)
	    return (-1);
	if (state.lrflip)
	    *xp = fip->sw - *xp - 1;
	if (state.tbflip)
	    *yp = fip->sh - *yp - 1;
	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: wcs.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $"};
