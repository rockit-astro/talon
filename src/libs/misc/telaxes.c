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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "lstsqr.h"
#include "misc.h"
#include "telstatshm.h"

/* only way to get output from test program is with the solver trace */
#ifdef TEST_IT
#define SOLVE_TRACE
#endif

/* evaluate the error in current YC estimate */
static double g_Y0, g_Y1, g_cosX10, g_cosS;
static double fYC(double yc)
{
    double r = sin(yc + g_Y0) * sin(yc + g_Y1) + cos(yc + g_Y0) * cos(yc + g_Y1) * g_cosX10 - g_cosS;
    return (r);
}

/* same, but negative so it can be maximized */
static double fYCn(double yc)
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

static double chisqr(double v[5])
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

    for (i = 0; i < g_nstars; i++)
    {
        double x = g_X[i];
        double y = g_Y[i];
        double h, d, ca;

        tel_realxy2ideal(tap, &x, &y);
        tel_xy2hadec(x, y, tap, &h, &d);
        solve_sphere(h - g_H[i], PI / 2 - g_D[i], sin(d), cos(d), &ca, NULL);
        err += g_fitp[i] = acos(ca);
    }

#ifdef CHISQR_TRACE
    fprintf(stderr, "HDXYN: %6.3f %6.3f %6.3f %6.3f %6.3f -> %10.7f\n", tap->HT, tap->DT, tap->XP, tap->YC, tap->NP,
            err);
#endif /* CHISQR_TRACE */

    /* solver seems to work better if not driving towards 0 */
    err += 1;

    return (err);
}

/* set up the necessary temp arrays and call the multivariat solver.
 */
static int call_lstsqr(double ftol)
{
    TelAxes *tap = g_tap;
    double p0[5], p1[5];

    p0[0] = tap->HT;
    p0[1] = tap->DT;
    p0[2] = tap->XP;
    p0[3] = tap->YC;
    p0[4] = tap->NP;

    p1[0] = 1.05 * tap->HT + 0.05;
    p1[1] = 1.05 * tap->DT + 0.05;
    p1[2] = 1.05 * tap->XP + 0.05;
    p1[3] = 1.05 * tap->YC + 0.05;
    p1[4] = tap->NP + 0.05;

    if (lstsqr(chisqr, p0, p1, 5, ftol) < 0)
        return (-1);

    tap->HT = p0[0];
    tap->DT = p0[1];
    tap->XP = p0[2];
    tap->YC = p0[3];
    tap->NP = p0[4];

    return (0);
}

#ifdef SOLVE_TRACE
/* cross-check */
static void prChk(TelAxes *tap, double H, double D, double X, double Y)
{
    double x, y, h, d;
    double ca;

    x = X;
    y = Y;
    tel_realxy2ideal(tap, &x, &y);
    tel_xy2hadec(x, y, tap, &h, &d);
    solve_sphere(H - h, PI / 2 - D, sin(d), cos(d), &ca, NULL);
    printf(" %7.1f", 3600 * raddeg(acos(ca)));

    tel_hadec2xy(H, D, tap, &x, &y);
    tel_ideal2realxy(tap, &x, &y);
    solve_sphere(X - x, PI / 2 - (Y + tap->YC), sin(y + tap->YC), cos(y + tap->YC), &ca, NULL);
    printf(" %7.1f", 3600 * raddeg(acos(ca)));

    printf("\n");
}
#endif /* SOLVE_TRACE */

/* given at least 2 HA/Dec/Encoder collections, find HT/DT/YC/XP/NP.
 * with just 2 stars, NP is set to 0.
 * return 0 with angular residuals in *fitp, else return -1.
 */
int tel_solve_axes(double H[],   /* HA of the known stars */
                   double D[],   /* Dec of the known stars */
                   double X[],   /* X-axes of the known stars */
                   double Y[],   /* Y-axes of the known stars */
                   int nstars,   /* number of entries in the arrays */
                   double ftol,  /* fractional tolerance goal */
                   TelAxes *tap, /* axis params to be devined */
                   double fitp[] /* angular residual for each star, rads */
)
{
    double sinD0, cosD0;
    double sinD1, cosD1;
    double sinH01, cosH01;
    double sinX10, cosX10;
    double sinYC0, cosYC0;
    double sinYC1, cosYC1;
    double cosS, sinA, cosA, sinB, cosB;
    double HT, sinDT, cosDT, XP, YC, NP;
    double x, y;
    double A, B;
    int s;

    /* need at least 2 */
    if (nstars < 2)
        return (-1);

    /* store some frequently reused trig values */
    sinD0 = sin(D[0]);
    cosD0 = cos(D[0]);
    sinD1 = sin(D[1]);
    cosD1 = cos(D[1]);
    sinH01 = sin(H[0] - H[1]);
    cosH01 = cos(H[0] - H[1]);
    sinX10 = sin(X[1] - X[0]);
    cosX10 = cos(X[1] - X[0]);

    /* estimate YC which gives encoder sep same as sky */
    cosS = sinD0 * sinD1 + cosD0 * cosD1 * cosH01;
    g_Y0 = Y[0];
    g_Y1 = Y[1];
    g_cosX10 = cosX10;
    g_cosS = cosS;

#ifdef JUST_MAKE_fYC_GRAPH
    for (YC = -PI; YC < PI; YC += .1)
        printf("%g %g\n", YC, fYC(YC));
    exit(1);
#endif

    if (newton(fYC, tap->YC, ftol, &YC) < 0)
    {
        /* may not quite include 0 due to measurement errors */
        if (funcmax(fYCn, tap->YC, ftol, &YC) < 0)
            return (-1);
    }

    /* guard for wraps */
    while (YC < -2 * PI)
        YC += 2 * PI;
    while (YC > 2 * PI)
        YC -= 2 * PI;

    /* handy */
    sinYC0 = sin(YC + Y[0]);
    cosYC0 = cos(YC + Y[0]);
    sinYC1 = sin(YC + Y[1]);
    cosYC1 = cos(YC + Y[1]);

    /* find estimate of DT */
    sinA = cosD1 * sinH01;
    cosA = (sinD1 - sinD0 * cosS) / cosD0;
    A = atan2(sinA, cosA);
    sinB = cosYC1 * sinX10;
    cosB = (sinYC1 - sinYC0 * cosS) / cosYC0;
    B = atan2(sinB, cosB);
    sinDT = sinYC0 * sinD0 + cosYC0 * cosD0 * cos(B - A);
    cosDT = sqrt(1.0 - sinDT * sinDT); /* + ok since -PI/2..DT..PI/2 */

    /* find estimage of HT */
    HT = H[0] + atan2(cosYC0 * sin(B - A), (sinYC0 - sinDT * sinD0) / cosD0);
    haRange(&HT);

    /* find estimage of XP */
    XP = X[1] + atan2(cosD1 * sin(HT - H[1]), (sinD1 - sinDT * sinYC1) / cosDT);
    range(&XP, 2 * PI);

    /* resulting first approx to model */
    tap->HT = HT;
    tap->DT = asin(sinDT);
    tap->XP = XP;
    tap->YC = YC;
    tap->NP = 0.0;

#ifdef SOLVE_TRACE
    fprintf(stderr, "Est1: HT/DT/XP/YC/NP: %10.7f %10.7f %10.7f %10.7f %10.7f\n", tap->HT, tap->DT, tap->XP, tap->YC,
            tap->NP);
#endif /* SOLVE_TRACE */

    /* if just 2 stars, stop and claim no residuals */
    if (nstars < 3)
    {
        fitp[0] = fitp[1] = 0.0;
        return (0);
    }

    /* to seed non-perp, use the model so far (which is based on the
     * first 2 points) and attribute to NP all remaining error in the 3rd
     * point.
     */
    tel_hadec2xy(H[2], D[2], tap, &x, &y);
    NP = (sin(y + YC) - sin(Y[2] + YC)) / (cos(y + YC) * sin(x));
    tap->NP = NP;

    /* provide global access for the model */
    g_nstars = nstars;
    g_H = H;
    g_D = D;
    g_X = X;
    g_Y = Y;
    g_fitp = fitp;
    g_tap = tap;

    /* refine with least-squares minimization of all residuals */
    s = call_lstsqr(ftol);

#ifdef SOLVE_TRACE
    fprintf(stderr, "Est2: HT/DT/XP/YC/NP: %10.7f %10.7f %10.7f %10.7f %10.7f\n", tap->HT, tap->DT, tap->XP, tap->YC,
            tap->NP);

    {
        int i;
        for (i = 0; i < nstars; i++)
        {
            printf("  %2d: %5.0f\" ", i, 3600.0 * raddeg(fitp[i]));
            prChk(tap, H[i], D[i], X[i], Y[i]);
        }
    }
#endif /* SOLVE_TRACE */

    /* guard for wraps */
    hdRange(&tap->HT, &tap->DT);
    while (tap->XP > 2 * PI)
        tap->XP -= 2 * PI;
    while (tap->XP < -2 * PI)
        tap->XP += 2 * PI;
    while (tap->YC > 2 * PI)
        tap->YC -= 2 * PI;
    while (tap->YC < -2 * PI)
        tap->YC += 2 * PI;

    return (s);
}

/* given an HA/Dec location and the telescope orientation parameters,
 * compute the target raw telescope encoder readings.
 */
void tel_hadec2xy(double H, double D,   /* target HA and Dec */
                  TelAxes *tap,         /* scope coords */
                  double *X, double *Y) /* resulting encoder values */
{
    double A, b, c, cc, sc, ca, B;

    A = tap->HT - H;
    b = PI / 2 - D;
    c = PI / 2 - tap->DT;
    cc = cos(c);
    sc = sin(c);
    solve_sphere(A, b, cc, sc, &ca, &B);

    *X = tap->XP - B;
    range(X, 2 * PI);
    *Y = PI / 2 - acos(ca) - tap->YC;
    haRange(Y);
}

/* given raw X/Y encoder readings and the telescope orientation parameters,
 * compute the celestial location.
 */
void tel_xy2hadec(double X, double Y,   /* encoder values */
                  TelAxes *tap,         /* scope coords */
                  double *H, double *D) /* celestial location */
{
    double A, b, c, cc, sc, ca, B;

    A = tap->XP - X;
    b = PI / 2 - (Y + tap->YC);
    c = PI / 2 - tap->DT;
    cc = cos(c);
    sc = sin(c);
    solve_sphere(A, b, cc, sc, &ca, &B);

    *H = tap->HT - B;
    haRange(H);
    *D = PI / 2 - acos(ca);
}

/* find position angle of a source at the given HA and Dec.
 * N.B. the sign of PA will be like that of HA, ie, +west
 */
void tel_hadec2PA(double H, double D, /* source HA and Dec */
                  TelAxes *tap,       /* scope coords */
                  double lt,          /* site latitude */
                  double *PA)         /* position angle */
{
    double A, b, c, cc, sc, B;

    A = H;
    b = PI / 2 - lt;
    c = PI / 2 - D;
    cc = cos(c);
    sc = sin(c);
    solve_sphere(A, b, cc, sc, NULL, &B);

    if (B > PI)
        B -= 2 * PI;
    *PA = B;
}

/* given actual encoder angles from home, correct to
 * an idealized othogonal coord system as per the given TexAxes.
 */
void tel_realxy2ideal(TelAxes *tap, double *Xp, double *Yp)
{
    double X = *Xp, Y = *Yp;
    double t, cost, sint, costp;

    /* undo german eq flip if currently activated */
    if (tap->GERMEQ && tap->GERMEQ_FLIP)
    {
        X += PI;
        Y = PI - 2 * tap->YC - Y;
        /* N.B. we assume subsequent code does not require 2*PI fix */
    }

    /* undo non-perp */
    t = PI / 2 - (tap->YC + Y);
    cost = cos(t);
    sint = sin(t);
    costp = cost * cos(tap->NP);
    Y = asin(costp) - tap->YC;
    if (fabs(sint) < 1e-6)
        X -= -cost * sin(tap->NP) < 0.0 ? -PI / 2 : PI / 2;
    else
        X -= atan2(-cost * sin(tap->NP), sint);

    /* unflip pole if enabled */
    if (tap->ZENFLIP)
    {
        X -= PI;
        Y = PI - 2 * tap->YC - Y;
    }

    /* just catch the extremes */
    while (X > 2 * PI)
        X -= 2 * PI;
    while (X < -2 * PI)
        X += 2 * PI;
    while (Y > 2 * PI)
        Y -= 2 * PI;
    while (Y < -2 * PI)
        Y += 2 * PI;

    *Xp = X;
    *Yp = Y;
}

/* given idealized othogonal encoder angles, correct to
 * form actual encoder angles from home as per the given TexAxes.
 */
void tel_ideal2realxy(TelAxes *tap, double *Xp, double *Yp)
{
    double X = *Xp, Y = *Yp;
    double t, cost, sint, costp;

    /* flip over pole if desired */
    if (tap->ZENFLIP)
    {
        X += PI;
        Y = PI - 2 * tap->YC - Y;
    }

    /* affect non-perp */
    t = PI / 2 - (tap->YC + Y);
    cost = cos(t);
    sint = sin(t);
    if (fabs(sint) < 1e-6)
        X += -cost * sin(tap->NP) < 0.0 ? -PI / 2 : PI / 2;
    else
        X += atan2(-cost * sin(tap->NP), sint);
    costp = cost / cos(tap->NP);
    if (costp > 1.0)
        costp = 1.0;
    if (costp < -1.0)
        costp = -1.0;
    Y = asin(costp) - tap->YC;

    /* affect german eq flip if enabled */
    if (tap->GERMEQ)
    {
        /* we define western sky as "flipped" */
        int flip = 0; /* don't trust ^= on GERMEQ_FLIP bit field */
        while (X < tap->hneglim || X >= tap->hneglim + PI)
        {
            if (X < tap->hneglim)
                X += PI;
            else
                X -= PI;
            Y = PI - 2 * tap->YC - Y;
            flip ^= 1;
        }
        tap->GERMEQ_FLIP = flip;
    }

    /* catch the extremes */
    while (X > 2 * PI)
        X -= 2 * PI;
    while (X < -2 * PI)
        X += 2 * PI;
    while (Y > 2 * PI)
        Y -= 2 * PI;
    while (Y < -2 * PI)
        Y += 2 * PI;

    *Xp = X;
    *Yp = Y;
}

#ifdef TEST_IT
/* read lines of the form
 *     HA Dec XEnv YEnc (all angles, all rads)
 * from stdin (ignoring all others).
 * N.B. define SOLVE_TRACE to print the results :-)
 */

#define MAXSTARS 50

int main(int ac, char *av[])
{
    double X[MAXSTARS], Y[MAXSTARS], H[MAXSTARS], D[MAXSTARS], fit[MAXSTARS];
    char buf[1024];
    TelAxes tax, *tap = &tax;
    int ns;

    ns = 0;
    while (fgets(buf, sizeof(buf), stdin))
    {
        if (sscanf(buf, "%lf %lf %lf %lf", &H[ns], &D[ns], &X[ns], &Y[ns]) != 4)
            continue;
        if (++ns == MAXSTARS)
            break;
    }

    tap->HT = 0;
    tap->DT = .8;
    tap->XP = 3;
    tap->YC = 1.3;

    if (tel_solve_axes(H, D, X, Y, ns, 1e-5, tap, fit) < 0)
    {
        fprintf(stderr, "Alignment failed\n");
        exit(1);
    }

    return (0);
}
#endif /* TEST_IT */
