#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../satspec.h"

static void extract(char *str1, char *str2, int start, int n) {
    strncpy(str2, str1 + start, n);
    str2[n] = '\0';
}

static int readdbl(double *val, char *str) {
    char *fel;

    *val = strtod(str, &fel);
    if(fel)
	if(*fel)
	    return -1;

    return 0;
}

static int readflt(float *val, char *str) {
    char *fel;

    *val = strtod(str, &fel);
    if(fel)
	if(*fel)
	    return -1;

    return 0;
}

#if 0
static int readint(int *val, char *str) {
    char *fel;

    *val = strtol(str, &fel, 10);
    if(fel)
	if(*fel)
	    return -1;

    return 0;
}
#endif

static int readuint(unsigned int *val, char *str) {
    char *fel;

    *val = strtoul(str, &fel, 10);
    if(fel)
	if(*fel)
	    return -1;

    return 0;
}

static int readfltexp(float *val, char *str, char *expstr) {
    char *fel;
    double b, x;

    b = strtod(str, &fel);
    if(fel)
	if(*fel)
	    return -1;

    x = strtol(expstr, &fel, 10);
    if(fel)
	if(*fel)
	    return -1;

    /* printf("*** %f %f\n", x, -5.0 + x); */
    *val = b * pow(10.0, -5.0 + x);
    /* printf("val = %f\n", *val); */
    return 0;
}

static int readpiece(unsigned int *val, char *str) {
    int len, ret;
    char *p;

    len = strlen(str);
    if(len < 1 || len > 3)
	return -1;

    for(p = str; *p; p++) {
	if(*p != ' ')
	    break;
    }

    ret = 0;
    while(*p) {
	if(*p >= 'A' && *p <= 'Z') {
	    ret *= 26;
	    ret += *p - 'A' + 1;
	} else if(*p >= 'a' && *p <= 'z') {
	    ret *= 26;
	    ret += *p - 'a' + 1;
	} else if(*p == ' ')
	    break;

	p++;
    }

    while(*p) {
	if(*p != ' ')
	    return -1;

	p++;
    }

    *val = ret;
    return 0;
}

static char *valnames[] = {
    "line 1 checksum",		/* 0x00000001 */
    "line 2 checksum",		/* 0x00000002 */
    "epoch",			/* 0x00000004 */
    "n'",			/* 0x00000008 */
    "n\"",			/* 0x00000010 */
    "bstar",			/* 0x00000020 */
    "ephemeris type",		/* 0x00000040 */
    "element set number",	/* 0x00000080 */
    "inclinaton",		/* 0x00000100 */
    "right ascension",		/* 0x00000200 */
    "eccentricity",		/* 0x00000400 */
    "argument of perigee",	/* 0x00000800 */
    "mean anomaly",		/* 0x00001000 */
    "mean motion",		/* 0x00002000 */
    "revolution number",	/* 0x00004000 */
    "classification character",	/* 0x00008000 */
    "catalog number",		/* 0x00008000 */
    "cospar id"			/* 0x00008000 */
};

char *tleerr(int err) {
    static char errtxt[1024], tmp[80];
    int i;

    if(!err) {
	strcpy(errtxt, "No error");
	return errtxt;
    }

    errtxt[0] = '\0';
    for(i = 0; i < 18; i++)
	if(err & (1 << i)) {
	    sprintf(tmp, "\"%s\" value error\n", valnames[i]);
	    strcat(errtxt, tmp);
	}

    if(errtxt[0])
	return errtxt;
    else
	return 0L;
}


static int tle_checksum(char *str) {
    int i, c, v, sum;

    sum = 0;
    for(i = 0; i < 68; i++) {
	c = ((unsigned char *)str)[i];
	if(c == 0)
	    return -1;

	if(c >= '0' && c <= '9')
	    v = c - '0';
	else if(c == '-')
	    v = 1;
	else 
	    v = 0;

	sum += v;
    }

#ifdef TLE_DEBUG
    printf("Sum = %d\n", sum % 10);
#endif
    if(sum % 10 == (str[68] - '0'))
	return 0;

    return -1;
}

int readtle(char *line1, char *line2, SatElem *elem) {
    int fel;
    unsigned int yr, launch, piece, catno, elnum, ephtype, orbit;
    double tmp;
    char epoch_yr[4];
    char epoch_frac[16];
    char dt2[12];
    char ddt6[12];
    char ddt6exp[4];
    char bstar[12];
    char bstarexp[4];
    char et[4];
    char eln[4];
    char rev[8];
    char inclin[12];
    char raan[12];
    char ecc[12];
    char aop[12];
    char ma[12];
    char mm[12];
    char catno_s1[8];
    char catno_s2[8];
    char classif[4];
    char cospar_yr[4];
    char cospar_launch[4];
    char cospar_piece[4];

    if(tle_checksum(line1))
	return 0x00000001;

    if(tle_checksum(line2))
	return 0x00000002;

    fel = 0;

    extract(line1, catno_s1, 2, 5);

    extract(line1, classif, 7, 1);

    if(classif[0] >= 'a' && classif[0] <= 'z')
	classif[0] -= 32;

    extract(line1, cospar_yr, 9, 2);
    extract(line1, cospar_launch, 11, 3);
    extract(line1, cospar_piece, 14, 3);

    extract(line1, epoch_yr, 18, 2);

    extract(line1, epoch_frac, 20, 12);

    extract(line1, dt2, 33, 10);

    if(dt2[1] != '.' || (dt2[0] != ' ' &&
			 dt2[0] != '-' &&
			 dt2[0] != '+' &&
			 dt2[0] != '0'))
	fel |= 0x00000008;

    extract(line1, ddt6, 44, 6);

    extract(line1, ddt6exp, 50, 2);

    extract(line1, bstar, 53, 6);

    extract(line1, bstarexp, 59, 2);
    extract(line1, et, 62, 1);

    if(et[0] == ' ')
	et[0] = '0';

    extract(line1, eln, 65, 3);
    extract(line2, catno_s2, 2, 5);
    extract(line2, inclin, 8, 8);
    extract(line2, raan, 17, 8);

    extract(line2, ecc + 2, 26, 7);
    ecc[0] = '0';
    ecc[1] = '.';

    extract(line2, aop, 34, 8);
    extract(line2, ma, 43, 8);
    extract(line2, mm, 52, 11);
    extract(line2, rev, 63, 5);

#ifdef TLE_DEBUG
    printf("catno_s1   = \"%s\"\n", catno_s1);
    printf("catno_s2   = \"%s\"\n", catno_s2);
    printf("classif    = \"%s\"\n", classif);
    printf("epoch_yr   = \"%s\"\n", epoch_yr);
    printf("epoch_frac = \"%s\"\n", epoch_frac);
    printf("dt2        = \"%s\"\n", dt2);
    printf("ddt6       = \"%s\"\n", ddt6);
    printf("ddt6exp    = \"%s\"\n", ddt6exp);
    printf("bstar      = \"%s\"\n", bstar);
    printf("bstarexp   = \"%s\"\n", bstarexp);
    printf("et         = \"%s\"\n", et);
    printf("eln        = \"%s\"\n", eln);
    printf("inclin     = \"%s\"\n", inclin);
    printf("raan       = \"%s\"\n", raan);
    printf("ecc        = \"%s\"\n", ecc);
    printf("aop        = \"%s\"\n", aop);
    printf("ma         = \"%s\"\n", ma);
    printf("mm         = \"%s\"\n", mm);
    printf("rev        = \"%s\"\n", rev);
#endif

    if((classif[0] < 'A' || classif[0] > 'Z') && classif[0] != ' ')
	fel |= 0x00008000;

    if(strcmp(catno_s1, catno_s2))
	fel |= 0x00010000;

    if(readuint(&catno, catno_s1))			fel |= 0x00010000;

    if(cospar_yr[0] == ' ' && cospar_yr[1] == ' ')
	yr = 0;
    else {
	if(readuint(&yr, cospar_yr))			fel |= 0x00020000;

	if(yr >= 57)
	    yr += 1900;
	else
	    yr += 2000;
    }

    if(cospar_launch[0] == ' ' &&
       cospar_launch[1] == ' ' &&
       cospar_launch[2] == ' ')
	launch = 0;
    else {
	if(readuint(&launch, cospar_launch))		fel |= 0x00020000;
    }

    if(cospar_piece[0] == ' ' &&
       cospar_piece[1] == ' ' &&
       cospar_piece[2] == ' ') {
	piece = 0;
    } else {
	if(readpiece(&piece, cospar_piece))		fel |= 0x00020000;
    }

    if(readdbl(&tmp, epoch_yr))				fel |= 0x00000004;
    if(readdbl(&(elem->se_EPOCH), epoch_frac))		fel |= 0x00000004;

    if(tmp < 57.0)
	tmp += 100.0;

    elem->se_EPOCH += tmp * 1000.0;

    if(readdbl(&(elem->se_XNDT20), dt2))		fel |= 0x00000008;
    if(readfltexp(&(elem->se_XNDD60), ddt6, ddt6exp))	fel |= 0x00000010;
    if(readfltexp(&(elem->se_BSTAR), bstar, bstarexp))	fel |= 0x00000020;
    if(readuint(&ephtype, et))				fel |= 0x00000040;
    if(readuint(&elnum, eln))				fel |= 0x00000080;
    if(readflt(&(elem->se_XINCL), inclin))		fel |= 0x00000100;
    if(readflt(&(elem->se_XNODEO), raan))		fel |= 0x00000200;
    if(readflt(&(elem->se_EO), ecc))			fel |= 0x00000400;
    if(readflt(&(elem->se_OMEGAO), aop))		fel |= 0x00000800;
    if(readflt(&(elem->se_XMO), ma))			fel |= 0x00001000;
    if(readdbl(&(elem->se_XNO), mm))			fel |= 0x00002000;
    if(readuint(&orbit, rev))				fel |= 0x00004000;

    elem->se_id.ephtype = ephtype;
    elem->se_id.orbit = orbit;
    elem->se_id.elnum = elnum;
    elem->se_id.catno = catno;
    elem->se_id.year = yr;
    elem->se_id.launch = launch;
    elem->se_id.piece = piece;

    if(classif[0] == ' ')
	elem->se_id.classif = 0;
    else
	elem->se_id.classif = classif[0] - 'A' + 1;;

#define XMNPDA 1440.
#define TWOPI 6.2831853

#define XXAPA(X) ((X) * TWOPI / XMNPDA)
#define XXRAD(X) ((X) / 180.0 * M_PI)

    elem->se_XMO = XXRAD(elem->se_XMO);
    elem->se_XNODEO = XXRAD(elem->se_XNODEO);
    elem->se_OMEGAO = XXRAD(elem->se_OMEGAO);
    elem->se_EO = (elem->se_EO);
    elem->se_XINCL = XXRAD(elem->se_XINCL);
    elem->se_XNO = elem->se_XNO * TWOPI / XMNPDA;
    elem->se_XNDT20 = elem->se_XNDT20 * TWOPI / XMNPDA / XMNPDA;
    elem->se_XNDD60 = elem->se_XNDD60 * TWOPI / XMNPDA / XMNPDA / XMNPDA;
    elem->se_BSTAR = (elem->se_BSTAR);
    elem->se_EPOCH = (elem->se_EPOCH);
    return fel;
}

static int writeexp(double val, char *str) {
    double tmp, ltmp;

    if(val == 0.0) {
	strcpy(str, " 00000-0");
	return 0;
    }

    tmp = fabs(val);
    ltmp = floor(log10(tmp * 1E20));

    /*
    printf("ltmp = %lf\n", ltmp);
    printf("val -> %+05.0lf\n", val * pow(10.0, 19.0 - ltmp));
    */

    ltmp -= 19.0;

    if(ltmp >= 10.0 || ltmp <= -10.0)
	return -1;

    sprintf(str, "%+06.0f%+01.0f", val * pow(10.0, 5.0 - ltmp), ltmp);

    if(str[0] == '+')
	str[0] = ' ';

    /* printf("val = \"%s\"\n", str); */
    return 0;
}

int writetle(SatElem *el, char *line1, char *line2) {
    int tmp, pi, pn;
    char piece[4], tmpstr[80];
    double dtmp;

    sprintf(line1, "1 ");
    sprintf(line2, "2 ");

    if(el->se_id.catno < 1 || el->se_id.catno > 99999) {
	return -1;
    }

    tmp = el->se_id.classif;
    if(tmp == 0)
	tmp = ' ';
    else
	tmp += 'A' - 1;

    if((tmp < 'A' || tmp > 'Z') && tmp != ' ') {
	return -1;
    }

    sprintf(line1 + 2, "%05d%c ", el->se_id.catno, tmp);

    if(el->se_id.year) {
	if(el->se_id.year < 1957 || el->se_id.year > 2056)
	    return -1;

	sprintf(line1 + 9, "%02d",
		el->se_id.year > 2000 ?
		el->se_id.year - 2000 :
		el->se_id.year - 1900);
    } else
	sprintf(line1 + 9, "  ");

    if(el->se_id.launch) {
	if(el->se_id.launch > 999)
	    return -1;

	sprintf(line1 + 11, "%03d", el->se_id.launch);
    } else
	sprintf(line1 + 11, "   ");

    if(el->se_id.piece) {
	if(el->se_id.piece > 18278)
	    return -1;
	else if(el->se_id.piece < 1) {
	    sprintf(line1 + 14, "    ");
	} else {
	    tmp = el->se_id.piece;
	    tmp -= 1;

	    pi = 0;

	    if(tmp >= 702) {
		pn = (tmp - 702) / 676;
		piece[pi++] = pn + 'A';
		tmp = (tmp - 26) % 676 + 26;
	    }

	    if(tmp >= 26) {
		pn = (tmp - 26) / 26;
		piece[pi++] = pn + 'A';
		tmp = tmp % 26;
	    }

	    piece[pi++] = tmp + 'A';

	    sprintf(line1 + 14, "%-3.3s", piece);
	}
    } else
	sprintf(line1 + 14, "   ");

    if(el->se_EPOCH >= 100000.0)
	dtmp = el->se_EPOCH - 100000.0;
    else
	dtmp = el->se_EPOCH;

    sprintf(line1 + 17, " %014.8f ", dtmp);

    dtmp = el->se_XNDT20 / TWOPI * XMNPDA * XMNPDA;

    if(fabs(dtmp) >= 1.0)
	return -1;

    sprintf(tmpstr, "%+10.8f", dtmp);

    printf("tmpstr = \"%s\"\n", tmpstr);

    if(tmpstr[0] == '-')
	line1[33] = '-';
    else
	line1[33] = ' ';

    sprintf(line1 + 34, "%9.9s", tmpstr + 2);

    dtmp = el->se_XNDD60 / TWOPI * XMNPDA * XMNPDA * XMNPDA;

    printf("dtmp = %f\n", dtmp);
    writeexp(dtmp, tmpstr);

    sprintf(line1 + 43, " %-8.8s", tmpstr);

    writeexp(el->se_BSTAR, tmpstr);

    sprintf(line1 + 52, " %-8.8s", tmpstr);

    sprintf(line1 + 61, " %1.1d", el->se_id.ephtype);

    printf("elnum = %d\n", el->se_id.elnum);
    sprintf(line1 + 63, "  %3.3d", el->se_id.elnum);

    /*
     * Line 2
     */
    sprintf(line2 + 2, "%05d ", el->se_id.catno);

    sprintf(line2 + 8, "%8.4f", el->se_XINCL / M_PI * 180.0);

    sprintf(line2 + 16, " %8.4f", el->se_XNODEO / M_PI * 180.0);

    if(el->se_EO >= 1.0)
	return -1;

    sprintf(line2 + 25, " %07.0f", el->se_EO * 1E7);

    sprintf(line2 + 33, " %8.4f", el->se_OMEGAO / M_PI * 180.0);

    sprintf(line2 + 42, " %8.4f", el->se_XMO / M_PI * 180.0);

    sprintf(line2 + 51, " %11.8f", el->se_XNO / TWOPI * XMNPDA);

    sprintf(line2 + 63, "%5d", el->se_id.orbit);

    return 0;
}
