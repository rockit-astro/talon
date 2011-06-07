#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#include "../vector.h"
#include "../sattypes.h"
#include "../satlib.h"

#include "proto.h"

#define IS_L1(S) ((S)[0]=='1'&&(S)[1]==' ')
#define IS_L2(S) ((S)[0]=='2'&&(S)[1]==' ')
#define IS_SAME(A,B) ((A)[2]==(B)[2]&&(A)[3]==(B)[3]&&(A)[4]==(B)[4]&&(A)[5]==(B)[5]&&(A)[6]==(B)[6])

#define XKMPER 6378.16
#define DEEP_SPACE (1.0/225.0)
#define T_INC (0.073)
#define MINUTES_PER_DAY (1440.0)

int main(int argc, char **argv) {
    int ret;
    SatElem set, se;
    SatData sdt, sd;
    double jdt, jd;
    char l0[256], l1[256], l2[256], *p0, *p1, *p2, *tmp;
    FILE *fp;
    Vec3 pt, dpt, p, dp;

    p0 = l0;
    p1 = l1;
    p2 = l2;
    
    *p0 = *p1 = *p2 = '\0';

    if(argc != 3) {
	fprintf(stderr, "Arguments: <test> <tles>\n");
	fprintf(stderr, "  The <test> file should contain a single\n"
		"  two-line element set. The <tles> file should contain\n"
		"  the TLE sets with which to compare the <test> "
		"propagation.\n");

	exit(2);
    }

    fp = fopen(argv[1], "r");
    if(!fp) {
	perror(argv[1]);
	exit(2);
    }

    bzero(&sdt, sizeof(sdt));
    bzero(&set, sizeof(set));

    bzero(&sd, sizeof(sd));
    bzero(&se, sizeof(se));

    fgets(p0, 256, fp);
    fgets(p1, 256, fp);
    fgets(p2, 256, fp);

    ret = readtle(p0, p1, &set);
    if (ret != 0)
	ret = readtle(p1, p2, &set);

    if (ret != 0) {
	fprintf(stderr, "Error 0x%08x encountered in %s (see readtle.c)\n",
		ret, argv[1]);
	exit(2);
    }

    fclose(fp);

    jdt = epoch_jd(set.se_EPOCH);

    sdt.elem = &set;

    fp = fopen(argv[2], "r");
    if(!fp) {
	perror(argv[2]);
	exit(2);
    }

    printf("   tdiff (days)   Xdiff           Ydiff           Zdiff\n");

    while(fgets(p2, 256,fp)) {
	if(IS_L1(p1) && IS_L2(p2) && IS_SAME(p1, p2)) {
	    ret = readtle(p1, p2, &se);

	    if(ret == 0) {
		if(sd.prop.sgp4) {
		    free(sd.prop.sgp4);
		    sd.prop.sgp4 = 0L;
		}

		if(sd.deep) {
		    free(sd.deep);
		    sd.deep = 0L;
		}

		bzero(&sd, sizeof(sd));

		sd.elem = &se;

		jd = epoch_jd(sd.elem->se_EPOCH);

		/*
		 * Propagate test element set to reference set epoch.
		 */
		if(sdt.elem->se_XNO >= DEEP_SPACE)
		    sgp4(&sdt, &pt, &dpt, (jd - jdt) * 1440.0);
		else
		    sdp4(&sdt, &pt, &dpt, (jd - jdt) * 1440.0);

#ifdef TRACE
		printf ("se_EPOCH  : %30.20f\n", se.se_EPOCH);
		printf ("se_XNO    : %30.20f\n", se.se_XNO);
		printf ("se_XINCL  : %30.20f\n", se.se_XINCL);
		printf ("se_XNODEO : %30.20f\n", se.se_XNODEO);
		printf ("se_EO     : %30.20f\n", se.se_EO);
		printf ("se_OMEGAO : %30.20f\n", se.se_OMEGAO);
		printf ("se_XMO    : %30.20f\n", se.se_XMO);
		printf ("se_BSTAR  : %30.20f\n", se.se_BSTAR);
		printf ("se_XNDT20 : %30.20f\n", se.se_XNDT20);
		printf ("se_orbit  : %30d\n",    se.se_id.orbit);
#endif /* TRACE */

		if(sd.elem->se_XNO >= DEEP_SPACE)
		    sgp4(&sd, &p, &dp, 0.);
		else
		    sdp4(&sd, &p, &dp, 0.);

#ifdef SHOW_DIFF
		printf("%15.8f %15.8f %15.8f %15.8f\n",
		       jd - jdt,
		       (pt.x - p.x) * XKMPER,
		       (pt.y - p.y) * XKMPER,
		       (pt.z - p.z) * XKMPER);
#else	/* ! SHOW REF */
		printf("%15.8f %15.8f %15.8f %15.8f\n",
		       jd - jdt, pt.x*XKMPER, pt.y*XKMPER, pt.z*XKMPER);
#endif
	    }
	}

	tmp = p0;
	p0 = p1;
	p1 = p2;
	p2 = tmp;
    }

    fclose(fp);
    return 0;
}
