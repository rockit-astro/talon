#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "rot.h"

/* rotate p[] by a about x */
void
rotx (double p[3], double a)
{
	double ca = cos(a);
	double sa = sin(a);
	double x = p[0];
	double y = p[1];
	double z = p[2];

	p[0] = x;
	p[1] = y*ca - z*sa;
	p[2] = y*sa + z*ca;
}

/* rotate p[] by a about y */
void
roty (double p[3], double a)
{
	double ca = cos(a);
	double sa = sin(a);
	double x = p[0];
	double y = p[1];
	double z = p[2];

	p[0] = x*ca + z*sa;
	p[1] = y;
	p[2] = -x*sa + z*ca;
}

/* rotate p[] by a about z */
void
rotz (double p[3], double a)
{
	double ca = cos(a);
	double sa = sin(a);
	double x = p[0];
	double y = p[1];
	double z = p[2];

	p[0] = x*ca - y*sa;
	p[1] = x*sa + y*ca;
	p[2] = z;
}
