/* find the 0 of a function by newton's secant method.
 * compile with #define TEST_IT to make a stand-alone program.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "lstsqr.h"

/* use newton's method to find the zero of a function.
 *  f(x):   function to be solved for 0
 *    x0:   initial guess
 *  stop:   stop when fabs(f) <= stop
 * N.B. avoid placing x0 near flat spots, ie, near max/mins/inflections.
 * return -1 if fails to converge, else 0.
 */
int
newton (double (*f)(double x), double x0, double stop, double *zerop)
{
	int maxit = 10-log(stop)/log(2.0); /*grace+stop about halves ech time */
	double dx = .1*x0 + 0.1;
	double y0 = (*f)(x0);
	int i;

	for (i = 0; i < maxit && fabs(y0) > stop; i++) {
	    double x1 = x0 + dx;
	    double y1 = (*f)(x1);
	    dx = y1*(dx/(y0-y1));

#ifdef NEWT_TRACE
	fprintf (stderr, "%10.7f %10.7f\n", x0, y0);
#endif
	    x0 = x1;
	    y0 = y1;
	}

	if (i < maxit) {
	    *zerop = x0;
	    return (0);
	}
	return (-1);
}

#ifdef TEST_IT

#define degrad(x)       ((x)*3.141592653589793/180.)
#define raddeg(x)       ((x)/3.141592653589793*180.)

double X0 = degrad(50);
double Y0 = degrad(10);
double X1 = degrad(-30);
double Y1 = degrad(-60);
double S  = degrad(50);

double
f (double x)
{
	double a, b;

	a = -sin(Y0+x)*sin(Y1+x) + cos(Y0+x)*cos(Y1+x)*cos(S);
	b = cos (X1 - X0);

	return (a - b);
}

int 
main (int ac, char *av[]) 
{
	double x0;

	if (ac != 3) {
	    fprintf (stderr, "%s: x0 dx\n", av[0]);
	    exit(1);
	}

	x0 = newton (f, atof(av[1]), atof(av[2]), 1e-10);

	printf ("f(%g) = %g\n", x0, f(x0));

	return (0);
}
#endif /* TEST_IT */
