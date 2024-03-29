/* 
    Main program to read the Talon shared memory and print all the requested 
    information as formatted strings, suitable for FITS headers 
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>

#include "P_.h"
#include "astro.h"
#include "telstatshm.h"


static char* telStateNames[] = {
    "TS_ABSENT",
    "TS_STOPPED",
    "TS_HUNTING",
    "TS_TRACKING",
    "TS_SLEWING",
    "TS_HOMING",
    "TS_LIMITING"
};

TelStatShm *init_shm()
{
	int shmid;
	long addr;

	shmid = shmget (TELSTATSHMKEY, sizeof(TelStatShm), 0);
	if (shmid < 0) {
	    perror ("shmget TELSTATSHMKEY");
	    exit (EXIT_FAILURE);
	}

	addr = (long) shmat (shmid, (void *)0, 0);
	if (addr == -1) {
	    perror ("shmat TELSTATSHMKEY");
	    exit (EXIT_FAILURE);
	}

	return (TelStatShm *) addr;
}

int main (int argc, char **argv)
{
    int i;
    TelStatShm *telstatshmp;
    char buf[128];
    double lst;
    int axes[] = { TEL_HM, TEL_DM, TEL_OM };

    telstatshmp = init_shm();
    now_lst(&telstatshmp->now, &lst);

    printf("{\n");
    printf("\t\"mjd\": %f,\n", telstatshmp->now.n_mjd);
    printf("\t\"lst\": %f,\n", lst);
    printf("\t\"tel_state\": \"%s\",\n", telStateNames[telstatshmp->telstate]);
    printf("\t\"axes\": [\n");
    for (i = 0; i < 3; i++)
    {
         MotorInfo m = telstatshmp->minfo[axes[i]];
         printf("\t\t{\"ishomed\": %d, \"cpos\": %f, \"cvel\": %f, \"step\": %d, \"df\": %f, \"poslim\": %f, \"neglim\": %f }%s\n",
	    m.ishomed, m.cpos, m.cvel, m.step, m.df, m.poslim, m.neglim, (i < 2 ? "," : ""));
    }
    printf("\t],\n");
    printf("\t\"ra\": \"%f\",\n", telstatshmp->CJ2kRA);
    printf("\t\"dec\": \"%f\"\n", telstatshmp->CJ2kDec);
    printf("}\n");

    exit(EXIT_SUCCESS);
}
