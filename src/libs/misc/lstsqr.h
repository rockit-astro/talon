/* lstsqr.c */
extern int lstsqr(double (*chisqr)(double p[]), double params0[], double params1[], int np, double ftol);

/* newton.c */
extern int newton(double (*f)(double x), double x0, double err, double *zerop);

/* funcmax.c */
extern int funcmax(double (*f)(double x), double x0, double dy, double *xmaxp);
extern int parabmax(double x[3], double y[3], double *maxp);
