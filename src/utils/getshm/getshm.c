/*
    Main program to read the Talon shared memory and print all the requested
    information as formatted strings, suitable for FITS headers
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <time.h>

#include "P_.h"
#include "astro.h"
#include "telstatshm.h"

TelStatShm *init_shm(void);

TelStatShm *init_shm()
{
    int shmid;
    long addr;

    shmid = shmget(TELSTATSHMKEY, sizeof(TelStatShm), 0);
    if (shmid < 0)
    {
        perror("shmget TELSTATSHMKEY");
        exit(EXIT_FAILURE);
    }

    addr = (long)shmat(shmid, (void *)0, 0);
    if (addr == -1)
    {
        perror("shmat TELSTATSHMKEY");
        exit(EXIT_FAILURE);
    }

    return (TelStatShm *)addr;
}

int main(int argc, char **argv)
{
    char buf[128];
    double lst, fupos;
    long maxtime = 90;
    TelStatShm *telstatshmp;

    if (argc == 2)
    {
        maxtime = atol(argv[1]);
    }
    if ((argc > 2) || (maxtime == 0L))
    {
        printf("Syntax: %s [max_time_for_meteo]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    telstatshmp = init_shm();

    printf("MJD-OBS = %16.8lf ", telstatshmp->now.n_mjd + MJD0 - 2400000.5);
    printf("/ Modified Julian Day of Talon variables\n");
    now_lst(&telstatshmp->now, &lst);
    fs_sexa(buf, lst, 2, 3600);
    printf("LST     = %s ", buf);
    printf("/ Local sidereal time\n");
    fs_sexa(buf, raddeg(telstatshmp->now.n_lat), 3, 3600);
    printf("LATITUDE= %s ", buf);
    printf("/ Telescope latitude (degrees +N)\n");
    fs_sexa(buf, raddeg(telstatshmp->now.n_lng), 4, 3600);
    printf("LONGITUD= %s ", buf);
    printf("/ Telescope longitude (degrees +E)\n");
    fs_sexa(buf, raddeg(telstatshmp->Calt), 3, 3600);
    printf("ELEVATIO= %s ", buf);
    printf("/ Elevation at MJD-OBS (degrees)\n");
    fs_sexa(buf, raddeg(telstatshmp->Caz), 3, 3600);
    printf("AZIMUTH = %s ", buf);
    printf("/ Azimuth at MJD-OBS (degrees E of N)\n");
    fs_sexa(buf, radhr(telstatshmp->CAHA), 3, 360000);
    printf("HA      = %s ", buf);
    printf("/ Hour Angle at MJD-OBS\n");
    fs_sexa(buf, radhr(telstatshmp->CARA), 3, 360000);
    printf("RAEOD   = %s ", buf);
    printf("/ Apparent RA at MJD-OBS\n");
    fs_sexa(buf, raddeg(telstatshmp->CADec), 3, 36000);
    printf("DECEOD  = %s ", buf);
    printf("/ Apparent Dec at MJD-OBS\n");
    fs_sexa(buf, radhr(telstatshmp->CJ2kRA), 3, 360000);
    printf("RA      = %s ", buf);
    printf("/ J2000 RA at MJD-OBS\n");
    fs_sexa(buf, raddeg(telstatshmp->CJ2kDec), 3, 36000);
    printf("DEC     = %s ", buf);
    printf("/ J2000 Dec at MJD-OBS\n");
    fs_sexa(buf, radhr(telstatshmp->DJ2kRA), 3, 36000);
    printf("OBJRA   = %s ", buf);
    printf("/ Target RA in J2000\n");
    fs_sexa(buf, raddeg(telstatshmp->DJ2kDec), 3, 36000);
    printf("OBJDEC  = %s ", buf);
    printf("/ Target Dec in J2000\n");
    printf("EQUINOX = 2000.0 ");
    printf("/ Equinox for RA and Dec (in years)\n");
    printf("RAWHENC = %lf ", telstatshmp->minfo[TEL_HM].cpos);
    printf("/ HA encoder at MJD-OBS (radians)\n");
    printf("RAWDENC = %lf ", telstatshmp->minfo[TEL_DM].cpos);
    printf("/ Dec encoder at MJD-OBS (radians)\n");
    if (telstatshmp->minfo[TEL_OM].have)
    {
        MotorInfo *mip = &telstatshmp->minfo[TEL_OM];
        printf("RAWOSTP = %lf ", mip->cpos);
        printf("/ Focus encoder at MJD-OBS (radians)\n");
        fupos = mip->step / ((2 * PI) * mip->focscale) * mip->cpos;
        printf("FOCUSPOS = %lf ", fupos);
        printf("/ Focus position from home (microns)\n");
    }

    exit(EXIT_SUCCESS);
}
