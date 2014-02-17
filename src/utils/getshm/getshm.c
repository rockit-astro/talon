/* 
    Main program to read the Talon shared memory and print all the requested 
    information as formatted strings, suitable for FITS headers 
*/

#include "getshm.h"

TelStatShm *initShm()
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
    TelStatShm *telstatshmp;

    telstatshmp = initShm();

    exit(EXIT_SUCCESS);
}
