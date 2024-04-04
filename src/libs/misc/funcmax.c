/* functions to help find maxima */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "lstsqr.h"

/* find x which maximizes y=f(x) to within dy.
 * set *xmaxp and return 0 if ok.
 * return 0 if ok, -1 if actually concave, -2 if flat, -3 if too rough.
 */
int
funcmax (double (*f)(double x), double x0, double dy, double *xmaxp)
{
	double xmax, ymax;
	double x[3];
	double y[3];
	int n;

	/* start at x0 and a little either way */
	x[0] = .95*x0 - .05;
	y[0] = (*f)(x[0]);
	x[1] = x0;
	y[1] = (*f)(x[1]);
	x[2] = 1.05*x0 + .05;
	y[2] = (*f)(x[2]);

#ifdef MAX_TRACE
	fprintf (stderr, "x0= %10.6f y0= %10.6f\n", x[0], y[0]);
	fprintf (stderr, "x1= %10.6f y1= %10.6f\n", x[1], y[1]);
	fprintf (stderr, "x2= %10.6f y2= %10.6f\n", x[2], y[2]);
#endif
	n = parabmax (x, y, &xmax);
	if (n < 0)
	    return (n);
	ymax = (*f)(xmax);

	/* keep seeking max until changes < dy or detect trouble */
	while (1) {
	    int minyi, i;
	    double newymax;

	    /* replace entry with smallest y with new pair */
	    minyi = 0;
	    for (i = 1; i < 3; i++)
		if (y[i] < y[minyi])
		    minyi = i;
	    x[minyi] = xmax;
	    y[minyi] = ymax;

	    /* find peak of parabola defined by x[] and y[] */
	    if (parabmax (x, y, &xmax) < 0)
		return (-3);
	    newymax = (*f)(xmax);
	    if (fabs(ymax - newymax) < dy)
		break;
	    ymax = newymax;

#ifdef MAX_TRACE
	fprintf (stderr, "x0= %10.6f y0= %10.6f\n", x[0], y[0]);
	fprintf (stderr, "x1= %10.6f y1= %10.6f\n", x[1], y[1]);
	fprintf (stderr, "x2= %10.6f y2= %10.6f\n", x[2], y[2]);
	fprintf (stderr, "xm= %10.6f ym= %10.6f\n", xmax, ymax);
#endif
	}

	*xmaxp = xmax;
	return (0);
}

/* given xi and yi=f(xi), i=0..2, return x at the max of the
 *   parabola fit through the three points.
 * return 0 if ok, -1 if actually concave, -2 if flat.
 */
int
parabmax (double x[3], double y[3], double *maxp)
{
	double y10 = y[1] - y[0];
	double y12 = y[1] - y[2];
	double x0 = x[0];
	double x1 = x[1];
	double x2 = x[2];
	double x10 = x1 - x0;
	double x12 = x1 - x2;
	double x00 = x0*x0;
	double x11 = x1*x1;
	double x22 = x2*x2;
	double x1122 = x11 - x22;
	double x1100 = x11 - x00;
	
	double n, d;

	/* check for concave */
	n = x10*y12 - x12*y10;
	d = x10*x1122 - x12*x1100;
	if (d == 0 || n/d > 0)
	    return (-1);

	/* check for straight line */
	n = y10*x1122 - y12*x1100;
	d = y10*x12 - y12*x10;
	if (d == 0)
	    return (-2);

	/* ok, it's convex for sure */
	*maxp = 0.5*n/d;
	return (0);

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
	double xmax;
	int s;

	if (ac != 3) {
	    fprintf (stderr, "%s: x0 dy\n", av[0]);
	    exit(1);
	}

	s = funcmax (f, atof(av[1]), atof(av[2]), &xmax);
	if (s < 0) {
	    printf ("No solution\n");
	    return (1);
	}

	printf ("f(%g) = %g\n", xmax, f(xmax));

	return (0);
}
#endif /* TEST_IT */
