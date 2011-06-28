#ifndef __MATH_H
#define __MATH_H

#if defined(_AVR)
float fabs(float x);
float frexp(float x, int *eptr);
float tanh(float x);
float sin(float x);
float atan(float x);
float atan2(float y, float x);
float asin(float x);
float exp10(float x);
float log10i(float x);
float log10(float x);
float fmod(float y, float z);
float sqrt(float x);
float cos(float x);
float ldexp(float d, int n);
float modf(float y, float *i);
float floor(float y);
float ceil(float y);
float fround(float d);
float tan(float x);
float acos(float x);
float exp(float x);
float log(float x);
float pow(float x,float y);
float sinh(float x);
float cosh(float x);
#else

double exp10(double x);				/* 10 ** x */
double exp(double x);				/* e ** x */
double log(double x);				/* ln x */
double log10(double x);				/* log 10 of x */
double pow(double x, double y);		/* x ** y */
double fabs(double);
double fmod(double, double);
double sqrt(double x);

/* Note that these functions now use RADIAN arguments, as
 * per ANSI C rules
 */
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double degree_to_radian(double);
double radian_to_degree(double);

#if 0
double modf(double v, double *ip);	/* break v into fractional and integral */
#endif

#endif
#endif
