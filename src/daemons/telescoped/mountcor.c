/* code to account for a mount model.
 */

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "csimc.h"
#include "strops.h"
#include "telenv.h"
#include "telstatshm.h"

#include "teled.h"

#define COMMENT '#' /* ignore lines beginning with this */

static char meshfn[] = "archive/config/telescoped.mesh"; /* name of mesh file */

typedef struct
{
    double ha, dec;   /* sky loc of error node */
    double dha, ddec; /* error (target - wcs), dha is polar angle */
} MeshPoint;

static MeshPoint *mpoints; /* malloced list of mesh points, from file */
static int nmpoints;

static double ptgrad; /* pointing interpolation radius, rads */

static void interp(double ha, double dec, double *ehap, double *edecp);
static void readMeshFile(void);
static MeshPoint *newMeshPoint(void);
static int cmpMP(const void *p1, const void *p2);
static void sortMPoints(void);

/* do whatever when we want to reinitialize for mount corrections.
 * this amounts to (re)reading the pointing mesh list and sorting it by dec.
 */
void init_mount_cor()
{
    static char ptgradnm[] = "PTGRAD";

    if (read1CfgEntry(1, tscfn, ptgradnm, CFG_DBL, &ptgrad, 0) < 0)
    {
        tdlog("%s: %s not found\n", basenm(tscfn), ptgradnm);
        die();
    }

    readMeshFile();
    if (mpoints)
        sortMPoints();
}

/* given an ha and dec, find the amounts by which the ideal should be
 * added to account for the mount correction.
 * everything is in rads. the ha error is already the polar angle.
 */
void tel_mount_cor(ha, dec, dhap, ddecp) double ha, dec;
double *dhap;
double *ddecp;
{
    if (!mpoints)
    {
        *dhap = 0.0;
        *ddecp = 0.0;
    }
    else
        interp(ha, dec, dhap, ddecp);
}

/* given a target location and the mesh points, interpolate to find the error.
 * use a weighted average based on distance if find two or more mesh points
 *   within ptgrad. the weight is the inverse of the distance away from the
 *   target, scaled 1 to 0 out to ptgrad. If don't find at least two then just
 *   use the closest directly.
 * N.B. we assume mpoints is sorted by increasing tdec.
 */
static void interp(double ha, double dec, double *ehap, double *edecp)
{
    double cdec = cos(dec), sdec = sin(dec);
    double cptgrad = cos(ptgrad);
    double swh, swd, sw;
    int nfound;
    int l, u, m;
    int i;

    /* binary search for closest tdec */
    l = 0;
    u = nmpoints - 1;
    do
    {
        m = (l + u) / 2;
        if (dec < mpoints[m].dec)
            u = m - 1;
        else
            l = m + 1;
    } while (l <= u);

    /* look either side of m within ptgrad */
    for (u = m; u < nmpoints && fabs(dec - mpoints[u].dec) <= ptgrad; u++)
        continue;
    for (l = m; l >= 0 && fabs(dec - mpoints[l].dec) <= ptgrad; --l)
        continue;

    swh = swd = sw = 0.0;
    nfound = 0;
    for (i = l + 1; i < u; i++)
    {
        MeshPoint *rp = &mpoints[i];
        double cosr, w; /* cos dist, weight */
        double edec;    /* dec error */
        double eha;     /* ha error */

        /* distance to this mesh point -- reject immediately if > ptgrad */
        cosr = sdec * sin(rp->dec) + cdec * cos(rp->dec) * cos(ha - rp->ha);
        if (cosr < cptgrad)
            continue;

        /* weight varies linearly from 1 if right on a mesh point to 0
         * at ptgrad.
         */
        w = (ptgrad - acos(cosr)) / ptgrad;

        eha = rp->dha;
        edec = rp->ddec;

        swh += w * eha;
        swd += w * edec;
        sw += w;

        nfound++;
    }

    /* if found at least two, use average.
     * else find closest and use it.
     */
    if (nfound >= 2)
    {
        *ehap = swh / sw;
        *edecp = swd / sw;
    }
    else
    {
        MeshPoint *closestrp = NULL;
        double closestcosr = -1;

        for (i = 0; i < nmpoints; i++)
        {
            MeshPoint *rp = &mpoints[i];
            double cosr;

            cosr = sdec * sin(rp->dec) + cdec * cos(rp->dec) * cos(ha - rp->ha);
            if (cosr > closestcosr)
            {
                closestrp = rp;
                closestcosr = cosr;
            }
        }
        if (closestrp)
        {
            *ehap = closestrp->dha;
            *edecp = closestrp->ddec;
        }
        else
        {
            *ehap = 0.0;
            *edecp = 0.0;
        }
    }
}

/* add room for one more in mpoints[] and return pointer to the new one.
 * return NULL if no more room.
 */
static MeshPoint *newMeshPoint()
{
    char *new;

    new = mpoints ? realloc((void *)mpoints, (nmpoints + 1) * sizeof(MeshPoint)) : malloc(sizeof(MeshPoint));
    if (!new)
        return (NULL);

    mpoints = (MeshPoint *)new;
    return (&mpoints[nmpoints++]);
}

/* read the mesh file into mpoints.
 * the mesh file is expected to have errors in arc mins, dHA a polar angle.
 * if trouble, return with mpoints == NULL.
 */
static void readMeshFile()
{
    char line[1024];
    FILE *fp;
    double ha, dec, dha, ddec;
    int lineno = 0;

    /* reset mpoints array */
    if (mpoints)
    {
        free((void *)mpoints);
        nmpoints = 0;
        mpoints = NULL;
    }

    /* open mesh file */
    fp = telfopen(meshfn, "r");
    if (!fp)
    {
        tdlog("%s: %s", meshfn, strerror(errno));
        return;
    }

    /* read each line, building mpoints[] */
    while (fgets(line, sizeof(line), fp))
    {
        MeshPoint *mp;

        lineno++;
        if (line[0] == COMMENT)
            continue;
        if (sscanf(line, "%lf %lf %lf %lf", &ha, &dec, &dha, &ddec) != 4)
        {
            tdlog("%s: skipping bad entry, line %d", meshfn, lineno);
            continue;
        }
        mp = newMeshPoint();
        if (!mp)
        {
            tdlog("No memory for mesh log -- corrections will be 0");
            if (mpoints)
            {
                free((void *)mpoints);
                mpoints = NULL;
            }
            nmpoints = 0;
            break;
        }
        mp->ha = hrrad(ha);
        mp->dec = degrad(dec);
        mp->dha = degrad(dha / 60.0);
        mp->ddec = degrad(ddec / 60.0);
    }

    fclose(fp);

    tdlog("%s: read %d mesh points", meshfn, nmpoints);
}

/* qsort-style function to compare 2 MeshPoints by increasing tdec */
static int cmpMP(const void *p1, const void *p2)
{
    MeshPoint *m1 = (MeshPoint *)p1;
    MeshPoint *m2 = (MeshPoint *)p2;
    double ddec = m1->dec - m2->dec;

    if (ddec < 0)
        return (-1);
    if (ddec > 0)
        return (1);
    return (0);
}

/* sort mpoints array by uncreasing dec */
static void sortMPoints()
{
    qsort((void *)mpoints, nmpoints, sizeof(MeshPoint), cmpMP);
}
