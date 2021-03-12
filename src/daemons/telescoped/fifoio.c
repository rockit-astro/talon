/* manage the fifo traffic */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/param.h>
#include <sys/shm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "misc.h"
#include "csimc.h"
#include "telstatshm.h"
#include "running.h"
#include "cliserv.h"

#include "teled.h"

#define	MAXLINE		1024	/* max message from a fifo */

/* info about a fifo connection */
typedef struct {
    FifoId id;		/* cross-check with symbolic code name */
    char *name;		/* fifo name */
    void (*fp)();	/* function to call to process input from this fifo */
    int fd[2];		/* fifo descriptors once opened */
} FifoInfo;

/* array of info about each fifo pair we deal with.
 * N.B. must be in same order as the FifoName enum, above
 */
static FifoInfo fifo[] = {
    {Tel_Id,	"Tel",        tel_msg},
    {Focus_Id,	"Focus",      focus_msg},
    {Dome_Id, 	"Dome",       dome_msg},
    {Cover_Id,	"Cover",      cover_msg}
};
#define	N_F	(sizeof(fifo)/sizeof(fifo[0]))

static void open_fifos (void);
static void open_1fifo (FifoInfo *fip);
static void close_1fifo (FifoInfo *fip);
static void reopen_1fifo (FifoInfo *fip);
static void set_shmtime (void);

/* write a code and new message to given fifo.
 * also log with tdlog() if code is < 0.
 */
void
fifoWrite (FifoId f, int code, char *fmt, ...)
{
    FifoInfo *fip = &fifo[f];
    char buf[MAXLINE];
    char errmsg[1024];
    va_list ap;
    int n;

    /* cross-check */
    if (fip->id != f) {
        tdlog ("Bug! Bogus fifo cross-check: %d vs %d", fip->id, f);
        die();
    }

    /* format message into buf */
    va_start (ap, fmt);
    vsprintf (buf, fmt, ap);
    va_end (ap);

    /* send it */
    n = serv_write (fip->fd, code, buf, errmsg);
    if (n < 0)
        tdlog ("%s: %s", fip->name, errmsg);

    /* log too if looks like an error message */
    if (code < 0)
        tdlog ("%s: %s", fip->name, buf);
}

/* close all fifos */
void
close_fifos()
{
    FifoInfo *fip;

    for (fip = fifo; fip < &fifo[N_F]; fip++)
        close_1fifo (fip);
}

/* create all the public points of contact */
void
init_fifos()
{
    open_fifos();
}

/* check for and dispatch all incoming messages.
 * then call all handlers for followup regardless.
 * keep telstatshmp->now_mjd as current as possible.
 */
void
chk_fifos()
{
    FifoInfo *fip;
    struct timeval tv;
    fd_set rfdset;
    int maxfdp1;
    int i, s;

    /* initialize the select read mask of fds for which we care */
    FD_ZERO (&rfdset);
    maxfdp1 = 0;
    for (i = 0; i < N_F; i++) {
        FD_SET (fifo[i].fd[0], &rfdset);
        if (fifo[i].fd[0] > maxfdp1)
            maxfdp1 = fifo[i].fd[0];
    }
    maxfdp1++;

    /* set up the max polling delay */
    tv.tv_sec = 0;
    tv.tv_usec = 1000000/HZ;	/* every other tick or so */

    /* call select, waiting for commands or timeout */
    while ((s=select(maxfdp1,&rfdset,NULL,NULL,&tv))<0 && errno==EINTR)
        continue;
    if (s < 0) {
        tdlog ("select(): %s", strerror(errno));
        return;	/* main will repeat -- we don't wanna die */
    }

    /* dispatch any fifo messages */
    for (fip = fifo; s > 0 && fip < &fifo[N_F]; fip++) {
        if (FD_ISSET (fip->fd[0], &rfdset)) {
            char msg[MAXLINE];
            int n;

            /* retreive new message */
            n = serv_read (fip->fd, msg, sizeof(msg)-1);
            if (n < 0) {
                tdlog ("%s: read: %s", fip->name, msg);
                reopen_1fifo(fip);		/* exits if fails */
                break;			/* need new select() */
            }

            /* keep time current */
            set_shmtime();

            /* dispatch */
            (*fip->fp) (msg);

            /* handled this one */
            s--;
        }
    }

    /* then call each handler in polling mode (ie, w/o message) */
    for (fip = fifo; fip < &fifo[N_F]; fip++) {
        set_shmtime();			/* keep time current */
        (*fip->fp) (NULL);			/* general update poll */
    }
}

/* create and attach all the fifos */
static void
open_fifos()
{
    FifoInfo *fip;

    for (fip = fifo; fip < &fifo[N_F]; fip++)
        open_1fifo (fip);
}

/* open one fifo channel.
 * die() if trouble.
 */
static void
open_1fifo (FifoInfo *fip)
{
    char msg[1024];

    if (serv_conn (fip->name, fip->fd, msg) < 0) {
        tdlog ("%s: %s", fip->name, msg);
        die();
    }
}

/* close fifos for this channel */
static void
close_1fifo (FifoInfo *fip)
{
    dis_conn (fip->name, fip->fd);
}

/* close and reopen this channel as a recovery strategy */
static void
reopen_1fifo (FifoInfo *fip)
{
    tdlog ("%s: closing", fip->name);
    close_1fifo (fip);
    tdlog ("%s: reopening", fip->name);
    open_1fifo (fip);
}

/* set current time in telstatshmp */
static void
set_shmtime()
{
    telstatshmp->now.n_mjd = mjd_now();
    struct timeval tv;
    gettimeofday(&tv, NULL);
    telstatshmp->heartbeat=tv.tv_sec*1000000+tv.tv_usec;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: fifoio.c,v $ $Date: 2001/04/19 21:12:09 $ $Revision: 1.1.1.1 $ $Name:  $"};
