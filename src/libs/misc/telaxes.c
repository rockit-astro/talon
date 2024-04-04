/* code to handle switching between telescope coordinates and HA/Dec, 
 * including transformation calibration.
 *
 * this derivation assumes the telescope's latitude axis (Dec or
 * Alt, ATCMB) values increase in the positive direction towards the N pole,
 * and the longitude axis (HA or Az) increased ccw as seen from above the
 * scope's N pole looking down (yes, opposite of HA). Whether the motors and
 * encoders really work that way is captured in the {H,D}[E]SIGN config file
 * params. X and Y refer to the distances from the respective home positions.
 *
 * Includes a stand-alone main() for testing with #ifdef TEST_IT which can also
 *   be used to just compute the calibration constants for any set of values.
 *
 * We also include code to handle transforming between idealized orthogonal
 * coords (as needed for all the above) and real-world xy axes coordinates
 * accounting for nonperpendicular axes, zenith and german equatorial flips.
 *
 * #define SOLVE_TRACE to get a trace of the solver activities.
 * #define TEST_IT to include a main() to test with output from xobs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "misc.h"
#include "telstatshm.h"

/* evaluate the error in current YC estimate */
static double g_Y0, g_Y1, g_cosX10, g_cosS;
static double
fYC (double yc)
{
	double r = sin(yc+g_Y0)*sin(yc+g_Y1)
				+ cos(yc+g_Y0)*cos(yc+g_Y1)*g_cosX10 - g_cosS;
	return (r);
}

/* same, but negative so it can be maximized */
static double
fYCn (double yc)
{
	return (-fYC(yc));
}

/* global for access by chisqr */
static int g_nstars;
static TelAxes *g_tap;
static double *g_H;
static double *g_D;
static double *g_X;
static double *g_Y;
static double *g_fitp;

static double
chisqr (double v[5])
{
	TelAxes *tap = g_tap;
	double err = 0.0;
	int i;

	tap->HT = v[0];
	tap->DT = v[1];
	tap->XP = v[2];
	tap->YC = v[3];
	tap->NP = v[4];

	/* don't let Dec solution wander over the pole */
	if (fabs(tap->DT) > degrad(90.0))
	    return (100.0);

	for (i = 0; i < g_nstars; i++) {
	    double x = g_X[i];
	    double y = g_Y[i];
	    double h, d, ca;

	    tel_realxy2ideal (tap, &x, &y);
	    tel_xy2hadec (x, y, tap, &h, &d);
	    solve_sphere (h-g_H[i], PI/2-g_D[i], sin(d), cos(d), &ca, NULL);
	    err += g_fitp[i] = acos(ca);
	}

#ifdef CHISQR_TRACE
	fprintf (stderr, "HDXYN: %6.3f %6.3f %6.3f %6.3f %6.3f -> %10.7f\n",
			    tap->HT, tap->DT, tap->XP, tap->YC, tap->NP, err);
#endif /* CHISQR_TRACE */

	/* solver seems to work better if not driving towards 0 */
	err += 1;

	return (err);
}

/* given an HA/Dec location and the telescope orientation parameters,
 * compute the target raw telescope encoder readings.
 */
void
tel_hadec2xy (
double H, double D,		/* target HA and Dec */
TelAxes *tap,			/* scope coords */
double *X, double *Y)		/* resulting encoder values */
{
	double A, b, c, cc, sc, ca, B;

	A = tap->HT - H;
	b = PI/2 - D;
	c = PI/2 - tap->DT;
	cc = cos(c);
	sc = sin(c);
	solve_sphere (A, b, cc, sc, &ca, &B);

	*X = tap->XP - B;
	range (X, 2*PI);
	*Y = PI/2 - acos(ca) - tap->YC;
	haRange (Y);
}

/* given raw X/Y encoder readings and the telescope orientation parameters,
 * compute the celestial location.
 */
void
tel_xy2hadec (
double X, double Y,		/* encoder values */
TelAxes *tap,			/* scope coords */
double *H, double *D)		/* celestial location */
{
	double A, b, c, cc, sc, ca, B;

	A = tap->XP - X;
	b = PI/2 - (Y + tap->YC);
	c = PI/2 - tap->DT;
	cc = cos(c);
	sc = sin(c);
	solve_sphere (A, b, cc, sc, &ca, &B);

	*H = tap->HT - B;
	haRange (H);
	*D = PI/2 - acos(ca);
}

/* find position angle of a source at the given HA and Dec.
 * N.B. the sign of PA will be like that of HA, ie, +west
 */
void
tel_hadec2PA (
double H, double D,		/* source HA and Dec */
TelAxes *tap,			/* scope coords */
double lt,			/* site latitude */
double *PA)			/* position angle */
{
	double A, b, c, cc, sc, B;

	A = H;
	b = PI/2 - lt;
	c = PI/2 - D;
	cc = cos(c);
	sc = sin(c);
	solve_sphere (A, b, cc, sc, NULL, &B);

	if (B > PI)
	    B -= 2*PI;
	*PA = B;
}

/* given actual encoder angles from home, correct to
 * an idealized othogonal coord system as per the given TexAxes.
 */
void
tel_realxy2ideal (TelAxes *tap, double *Xp, double *Yp)
{
	double X = *Xp, Y = *Yp;
	double t, cost, sint, costp;

	/* undo german eq flip if currently activated */
	if (tap->GERMEQ && tap->GERMEQ_FLIP) {
	    X += PI;
	    Y = PI - 2*tap->YC - Y;
	    /* N.B. we assume subsequent code does not require 2*PI fix */
	}

	/* undo non-perp */
	t = PI/2 - (tap->YC + Y);
	cost = cos(t);
	sint = sin(t);
	costp = cost*cos(tap->NP);
	Y = asin(costp) - tap->YC;
	if (fabs(sint) < 1e-6)
	    X -= -cost*sin(tap->NP) < 0.0 ? -PI/2 : PI/2;
	else
	    X -= atan2 (-cost*sin(tap->NP), sint);

	/* unflip pole if enabled */
	if (tap->ZENFLIP) {
	    X -= PI;
	    Y = PI - 2*tap->YC - Y;
	}

	/* just catch the extremes */
	while (X > 2*PI)
	    X -= 2*PI;
	while (X < -2*PI)
	    X += 2*PI;
	while (Y > 2*PI)
	    Y -= 2*PI;
	while (Y < -2*PI)
	    Y += 2*PI;

	*Xp = X;
	*Yp = Y;
}

/* given idealized othogonal encoder angles, correct to
 * form actual encoder angles from home as per the given TexAxes.
 */
void
tel_ideal2realxy (TelAxes *tap, double *Xp, double *Yp)
{
	double X = *Xp, Y = *Yp;
	double t, cost, sint, costp;

	/* flip over pole if desired */
	if (tap->ZENFLIP) {
	    X += PI;
	    Y = PI - 2*tap->YC - Y;
	}

	/* affect non-perp */
	t = PI/2 - (tap->YC + Y);
	cost = cos(t);
	sint = sin(t);
	if (fabs(sint) < 1e-6)
	    X += -cost*sin(tap->NP) < 0.0 ? -PI/2 : PI/2;
	else
	    X += atan2 (-cost*sin(tap->NP), sint);
	costp = cost/cos(tap->NP);
	if (costp >  1.0) costp =  1.0;
	if (costp < -1.0) costp = -1.0;
	Y = asin(costp) - tap->YC;

	/* affect german eq flip if enabled */
	if (tap->GERMEQ) {
	    /* we define western sky as "flipped" */
	    int flip = 0;	/* don't trust ^= on GERMEQ_FLIP bit field */
	    while (X < tap->hneglim || X >= tap->hneglim+PI) {
		if (X < tap->hneglim)
		    X += PI;
		else
		    X -= PI;
		Y = PI - 2*tap->YC - Y;
		flip ^= 1;
	    }
	    tap->GERMEQ_FLIP = flip;
	}

	/* catch the extremes */
	while (X > 2*PI)
	    X -= 2*PI;
	while (X < -2*PI)
	    X += 2*PI;
	while (Y > 2*PI)
	    Y -= 2*PI;
	while (Y < -2*PI)
	    Y += 2*PI;

	*Xp = X;
	*Yp = Y;
}
