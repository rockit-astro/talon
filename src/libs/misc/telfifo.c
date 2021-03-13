/* KMI - these definitions were pulled from xobs.h and xobs/fifos.c, with
 *       modifications to make into a more generally usable library
 *
 * First version - 8/4/2005
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/shm.h>

#include "P_.h"
#include "astro.h"
#include "cliserv.h"
#include "telstatshm.h"

#include "telfifo.h"

static TelStatShm *telstatshmp; /* to shared memory segment */


FifoInfo fifos[] = { {"Tel",     Tel_Id},
                     {"Focus",   Focus_Id},
                     {"Dome",    Dome_Id},
                     {"Cover", Cover_Id}	};

FifoInfo getFIFO(int id)
{
    return fifos[id];
}

// For programs with semi-complicated shutdown routines (e.g. need to unlock
// status as a running program), a callback function can be set which will
// be called if we encounter a critical error while using FIFOs. The callback
// should cause the program to exit; otherwise, we'll do it for them

// Placeholder error function that just exits
void fifo_die()
{
    exit(1);
}

static void (*error_callback)() = fifo_die;

// This is what we call locally on an error
void fifo_error()
{
    error_callback();
    fifo_die();
}

void setFifoErrorCallback(void (*func)())
{
    error_callback = func;
}



/* write a message to given fifo -- generally die if real trouble but we
 * do return -1 if fifo is not available now.
 */
int
fifoMsg (FifoId fid, char *fmt, ...)
{
    FifoInfo *fip = &fifos[fid];
    char buf[512];
    va_list ap;

    /* cross-check */
    if (fip->fid != fid) {
        printf("Bug! fifoMsg fifo cross-check failed:%d %d\n",fip->fid,fid);
        fifo_error();
    }

    /* fifos can be closed while telrun is on; passive */
    if (!fip->fdopen) {
        printf("Service not available.");
        return (-1);
    }

    /* format into buf */
    va_start (ap, fmt);
    vsprintf (buf, fmt, ap);
    va_end (ap);

    /* clear out any stale message */
    printf(" ");

    /* send command */
    if (cli_write (fip->fd, buf, buf) < 0) {
        printf ("%s: %s\n", fip->name, buf);
        fifo_error();
    }

    /* ok */
    return (0);
}



/* read a message from the given fifo.
 * if ok, return the leading code value and put the remainder in buf[],
 * else print and die.
 */
int
fifoRead (FifoId fid, char buf[], int buflen)
{
    FifoInfo *fip = &fifos[fid];
    int v;

    /* cross-check */
    if (fip->fid != fid) {
        printf("Bug! fifoRd fifo cross-check failed: %d %d\n",fip->fid,fid);
        fifo_error();
    }

    if (cli_read (fip->fd, &v, buf, buflen) < 0) {
        printf ("%s: %s\n", fip->name, buf);
        fifo_error();
    }
    return (v);
}

/* shut down all activity */
void
stopAllDevices()
{
    int shmid;
    long addr;

    if(!telstatshmp)
    {
        // Get shared memory information
        shmid = shmget (TELSTATSHMKEY, sizeof(TelStatShm), 0666|IPC_CREAT);
        if (shmid < 0) {
            //strcpy(error, "shmget TELSTATSHMKEY");
            return ;
        }
    
        addr = (long) shmat (shmid, (void *)0, 0);
        if (addr == -1) {
            //strcpy(error, "shmat TELSTATSHMKEY");
            return ;
        }
    
        telstatshmp = (TelStatShm *) addr;
    }
    
    fifoMsg (Tel_Id, "Stop");
    if (telstatshmp->shutterstate != SH_ABSENT)
        fifoMsg (Dome_Id, "Stop");
    if (OMOT->have)
        fifoMsg (Focus_Id, "Stop");
}

/* make connections to daemons.
 * N.B. do nothing gracefully if connections are already ok.
 */
void
openFIFOs()
{
    FifoInfo *fip;

    for (fip = fifos; fip < &fifos[numFifos]; fip++) {
        char buf[1024];

        if (!fip->fdopen) {
            if (cli_conn (fip->name, fip->fd, buf) == 0)
            {
                fip->fdopen = 1;
                fprintf (stderr, "%s opened\n",fip->name);
            }
            else
            {
                fprintf (stderr, "%s: %s\n", fip->name, buf);
                fifo_error();
            }
        }
    }
}

/* close connections to daemons.
 * N.B. do nothing gracefully if connections are already closed down.
 */
void
closeFIFOs()
{
    FifoInfo *fip;

    for (fip = fifos; fip < &fifos[numFifos]; fip++) {
        if (fip->fdopen) {
            (void) close (fip->fd[0]);
            (void) close (fip->fd[1]);
            fip->fdopen = 0;
        }
    }
}

