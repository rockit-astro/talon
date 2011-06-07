#include <sys/time.h>
#include <sys/timeb.h>
#include <math.h>

#include "../sattypes.h"
#include "../satlib.h"

#include "proto.h"

/*
 * Returns UT days since J2000
 */
double current_jd() {
    /* struct timeb tb; */
    struct timeval tv;
    struct timezone tz;
    double jd;

    gettimeofday(&tv, &tz);
    jd = -10957.5 + (tv.tv_sec + tv.tv_usec / 1000000.0) / 86400.0;

    return jd;
}

/*
 * Returns hour angle of greenwich, in radians.
 * Argument is days since J2000.
 */
double ut1_to_gha(double T) {
    register double ha;

    T /= 36525.0;

    ha = 7.64017175260476135E-06L +
	(2.73790935079537123E-03L +
	 (2.95028772783735164E-11L +
	  1.96466144446979475E-15L * T) * T) * T;

    ha *= 36525.0;
    ha *= M_PI * 2;
    ha = fmod(ha, M_PI * 2);
    if(ha < 0.0)
	ha += M_PI * 2;

    return ha;
}

/*
 * Compute local sidereal time, given gha and longitude from greenwich.
 */
double ut1_to_lst(double T, double gha, double lon) {
    double ret, ut;

    ut = (T - floor(T) + 0.5) * M_PI * 2.0;

    ret = fmod(ut + gha + lon, M_PI * 2.0);

    if(ret < 0.0)
	ret += M_PI * 2.0;

    return ret;
}

void smallsleep(double t) {
    struct timeval tv;

    tv.tv_sec = t;
    tv.tv_usec = (t - tv.tv_sec) * 1000000;

    select(0, 0L, 0L, 0L, &tv);
}

static int daynums[100] = {
    -18262, -17897, -17532, -17166, -16801, -16436, -16071, -15705, -15340,
    -14975, -14610, -14244, -13879, -13514, -13149, -12783, -12418, -12053,
    -11688, -11322, -10957, -10592, -10227, -9861,  -9496,  -9131,  -8766,
    -8400,  -8035,  -7670,  -7305,  -6939,  -6574,  -6209,  -5844,  -5478,
    -5113,  -4748,  -4383,  -4017,  -3652,  -3287,  -2922,  -2556,  -2191,
    -1826,  -1461,  -1095,  -730,   -365,   0,      366,    731,    1096,
    1461,   1827,   2192,   2557,   2922,   3288,   3653,   4018,   4383,
    4749,   5114,   5479,   5844,   6210,   6575,   6940,   7305,   7671,
    8036,   8401,   8766,   9132,   9497,   9862,   10227,  10593,  10958,
    11323,  11688,  12054,  12419,  12784,  13149,  13515,  13880,  14245,
    14610,  14976,  15341,  15706,  16071,  16437,  16802,  17167,  17532,
    17898
};

double epoch_jd(double epoch) {
    int year, dn;
    double frac;

    year = (epoch + 2E-7) / 1000;	/* XXX? */

    frac = epoch - year * 1000;

    year %= 100;

    if(year < 50)
	year += 100;

    dn = daynums[year - 50] - 1;
    year += 1900;

    /*
      printf("year %d, daynum = %d, frac = %14.10f\n", year, dn, frac);
      printf("jd = %d\n", dn + 2451545);
    */
    return dn + frac - 0.5;
}

int yearvals[] = {
  -631152000, -599616000, -568080000, -536457600, -504921600,
  -473385600, -441849600, -410227200, -378691200, -347155200,
  -315619200, -283996800, -252460800, -220924800, -189388800,
  -157766400, -126230400, -94694400,  -63158400,  -31536000,
  0,          31536000,   63072000,   94694400,   126230400,
  157766400,  189302400,  220924800,  252460800,  283996800,
  315532800,  347155200,  378691200,  410227200,  441763200,
  473385600,  504921600,  536457600,  567993600,  599616000,
  631152000,  662688000,  694224000,  725846400,  757382400,
  788918400,  820454400,  852076800,  883612800,  915148800,
  946684800,  978307200,  1009843200, 1041379200, 1072915200,
  1104537600, 1136073600, 1167609600, 1199145600, 1230768000,
  1262304000, 1293840000, 1325376000, 1356998400, 1388534400,
  1420070400, 1451606400, 1483228800, 1514764800, 1546300800,
  1577836800, 1609459200, 1640995200, 1672531200, 1704067200,
  1735689600, 1767225600, 1798761600, 1830297600, 1861920000,
  1893456000, 1924992000, 1956528000, 1988150400, 2019686400,
  2051222400, 2082758400, 2114380800, 2145916800
};

void jd_timeval(double jd, struct timeval *tv) {
    int i, dnum, sec;

    dnum = jd;

    for(i = 99; i >= 0; --i)
	if(daynums[i] <= dnum)
	    break;

    if(i < 0 || i > 88) {
	tv->tv_sec = -1;
	tv->tv_usec = -1;
	return;
    }

    jd -= daynums[i];
    dnum = jd;
    jd *= 86400.0;
    jd += 43200.0;

    sec = jd;
    jd -= sec;

    tv->tv_sec = yearvals[i] + sec;
    tv->tv_usec = jd * 1.0E6;

    return;
}

#ifdef TID_ALONE
void main() {
    int i;
    struct timeval tv;

    jd_timeval(-970.0000, &tv);
    printf("%s", ctime(&(tv.tv_sec)));
    jd_timeval(current_jd(), &tv);
    printf("%s", ctime(&(tv.tv_sec)));
    jd_timeval(epoch_jd(94104.07041152), &tv);
    printf("%s", ctime(&(tv.tv_sec)));

/*
    printf("%14.9f\n", epoch_jd(97104.07041152));
    printf("%14.9f\n", epoch_jd(96000.0));
    printf("%14.9f\n", epoch_jd(0.0));
    printf("current %14.9f\n", current_jd());
    */
}
#endif
