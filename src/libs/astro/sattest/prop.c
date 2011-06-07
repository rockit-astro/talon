#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define XKMPER 6378.135
#define DEEP_SPACE (1.0/225.0)
#define T_INC (0.073)
#define MPD (1440.0)

int main(int argc, char **argv) {
    int ret;
    SatElem se;
    SatData sd;
    double jd;
    double ddays;
    char l0[256], l1[256], l2[256], *p0, *p1, *p2, *tmp;
    FILE *fp;
    Vec3 p, dp;

    p0 = l0;
    p1 = l1;
    p2 = l2;
    
    *p0 = *p1 = *p2 = '\0';

    ddays = argc > 1 ? atof(argv[1]) : 0;
    fp = stdin;

    bzero(&sd, sizeof(sd));
    bzero(&se, sizeof(se));

    while(fgets(p2, 256,fp)) {
	if(IS_L1(p1) && IS_L2(p2) && IS_SAME(p1, p2)) {
	    ret = readtle(p1, p2, &se);

	    printf ("IN: %sIN: %s", p1, p2);
	    writetle (&se, p1, p2);
	    printf ("OT: %s\nOT: %s\n", p1, p2);

	    printf ("se_EPOCH  : %30.20f\n", se.se_EPOCH);
	    printf ("se_XNO    : %30.20f\n", se.se_XNO);
	    printf ("se_XINCL  : %30.20f\n", se.se_XINCL);
	    printf ("se_XNODEO : %30.20f\n", se.se_XNODEO);
	    printf ("se_EO     : %30.20f\n", se.se_EO);
	    printf ("se_OMEGAO : %30.20f\n", se.se_OMEGAO);
	    printf ("se_XMO    : %30.20f\n", se.se_XMO);
	    printf ("se_BSTAR  : %30.20f\n", se.se_BSTAR);
	    printf ("se_XNDT20 : %30.20f\n", se.se_XNDT20);
	    printf ("se_orbit  : %30d\n",   se.se_id.orbit);

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

		tmp = strchr(p0, '\n');
		if (tmp)
		    *tmp = '\0';

		tmp = strchr(p0, '\r');
		if (tmp)
		    *tmp = '\0';

#if 0
	se.se_XMO = 5.512537;
	se.se_XNODEO = 2.79538226;
	se.se_OMEGAO = 2.25632477;
	se.se_EO = 0.693824112;
	se.se_XINCL = 1.18927085;
	se.se_XNDD60 = 0; 
	se.se_BSTAR = 9.99999975e-05;
	se.pad1 = 0;
	se.se_XNO = 0.008737795020549919;
	se.se_XNDT20 = 3.6451928606770838e-11;
	se.se_EPOCH = 97013.431886279999;

	se.se_id.catno = 898;
	se.se_id.classif = 21;
	se.se_id.elnum = 256;
	se.se_id.year = 1964;
	se.se_id.launch = 49;
	se.se_id.piece = 5;
	se.se_id.ephtype = 0;
	se.se_id.orbit = 23740;
#endif

		if(sd.elem->se_XNO >= DEEP_SPACE) {
		    sgp4(&sd, &p, &dp, ddays*MPD);
		    printf ("sgp4\n");
		} else {
		    sdp4(&sd, &p, &dp, ddays*MPD);
		    printf ("sdp4\n");
		}

		printf ("EPOCH %15.8f + %g days:  %15.8f %15.8f %15.8f\n",
			se.se_EPOCH, ddays, p.x*XKMPER, p.y*XKMPER, p.z*XKMPER);
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
