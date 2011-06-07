/* handle the fifo details */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telstatshm.h"
#include "strops.h"
#include "cliserv.h"
#include "scan.h"

#include "telrun.h"

/* this is used to describe the several fifos used to communicate with
 * the telescoped.
 */
typedef struct
{
	FifoId check; /* cross-check with symbolic code name */
	char *name; /* fifo name */
	int (*cb)(); /* function to call to process input from this fifo */
	int fd[2]; /* channel to daemon */
	int pending; /* set in Write, reset in Read */
} FifoInfo;

static int tel_rd_cb(FifoInfo *fip);
static int filter_rd_cb(FifoInfo *fip);
static int focus_rd_cb(FifoInfo *fip);
static int dome_rd_cb(FifoInfo *fip);
static int cover_rd_cb(FifoInfo *fip);
static int lights_rd_cb(FifoInfo *fip);
static int camera_rd_cb(FifoInfo *fip);
static int generic_rd_cb(FifoInfo *fip);
static int fifoRead(FifoInfo *fip, char buf[], int l);
static void on_alrm(int dummy);

/* polling timeout -- need not be real fast since fifos trigger too.
 * only thing we actually just poll is change in telrun.sls file.
 */
#define	POLL_TO	500	/* ms */

/* fifo info
 * N.B. order must match FifoIds
 */
static FifoInfo fifos[] =
{
{ Tel_Id, "Tel", tel_rd_cb },
{ Filter_Id, "Filter", filter_rd_cb },
{ Focus_Id, "Focus", focus_rd_cb },
{ Dome_Id, "Dome", dome_rd_cb },
{ Cover_Id, "Cover", cover_rd_cb },
{ Lights_Id, "Lights", lights_rd_cb },
{ Cam_Id, "Camera", camera_rd_cb } };

#define NFIFOS        (sizeof(fifos)/sizeof(fifos[0]))

/* info for select() */
static struct timeval timev;
static fd_set fdset;
static int maxfdp1;

/* make all the stream connections.
 * exit if trouble.
 */
void init_fifos()
{
	FifoInfo *fip;
	int i;

	/* open the connections */
	for (fip = fifos; fip < &fifos[NFIFOS]; fip++)
	{
		char buf[1024];

		if (cli_conn(fip->name, fip->fd, buf) < 0)
		{
			fprintf(stderr, "%s\n", buf);
			exit(1);
		}
	}

	/* find largest fd we'll read from and setup fdset */
	maxfdp1 = 0;
	FD_ZERO (&fdset);
	for (i = 0; i < NFIFOS; i++)
	{
		if (fifos[i].fd[0] > maxfdp1)
			maxfdp1 = fifos[i].fd[0];
		FD_SET (fifos[i].fd[0], &fdset);
	}
	maxfdp1++; /* plus one */

	/* setup nominal timeout */
	timev.tv_sec = POLL_TO / 1000;
	timev.tv_usec = 1000 * (POLL_TO % 1000);

	/* connect dummy SIGALRM handler for setTrigger() */
	signal(SIGALRM, on_alrm);
}

/* return 1 if any fifo is pending, else 0.
 * N.B. do *not* include the Camera since its work can overlap.
 */
int chk_pending()
{
	FifoInfo *fip;

	for (fip = fifos; fip < &fifos[NFIFOS]; fip++)
		if (fip->pending && fip->check != Cam_Id)
			return (1);

	return (0);
}

/* check for activity from any fifos.
 * return 0 if ok else -1
 * N.B. this includes a sleep delay
 */
int chk_fifos()
{
	struct timeval tv;
	FifoInfo *fip;
	fd_set fds;
	int s;
	int nbad;

	/* wait for fd data or timeout */
	fds = fdset; /* fresh copy each time */
	tv = timev; /* ditto */
	s = select(maxfdp1, &fds, NULL, NULL, &tv);

	/* check for os trouble -- signals are ok */
	if (s < 0 && errno != EINTR)
	{
		tlog(NULL, "select() error: %s", strerror(errno));
		return (-1);
	}

	/* dispatch the handlers for all pending file descriptors */
	nbad = 0;
	for (fip = fifos; s > 0 && fip < &fifos[NFIFOS]; fip++)
		if (FD_ISSET (fip->fd[0], &fds))
		{
			if ((*fip->cb)(fip) < 0)
				nbad++;
			s--;
		}

	/* return */
	return (nbad > 0 ? -1 : 0);
}

/* set an alarm to break the select() at the given time, if it is in the
 * future. this can be used by any program which does not want to be
 * subject to the polling latency.
 * N.B. a previous trigger is lost.
 */
void setTrigger(time_t t)
{
	time_t tnow = time(NULL);

	if (t > tnow)
		alarm(t - tnow);
}

/* shut down all activity */
void stop_all_devices()
{
	if (IMOT->have)
		fifoWrite(Filter_Id, "Stop");
	if (OMOT->have)
		fifoWrite(Focus_Id, "Stop");
	if (telstatshmp->domestate != DS_ABSENT || telstatshmp->shutterstate
			!= SH_ABSENT)
		fifoWrite(Dome_Id, "Stop");
	
//	fifoWrite(Cover_Id, "coverClose");
	
	if (telstatshmp->lights > 0)
		fifoWrite(Lights_Id, "0");
	fifoWrite(Tel_Id, "Stop");
	fifoWrite(Cam_Id, "Stop"); /* if IDLE can be a race with a new cmd */
}

/* write a new message to given fifo */
void fifoWrite(int f, char *fmt, ...)
{
	FifoInfo *fip = &fifos[f];
	char buf[512];
	va_list ap;

	/* cross-check */
	if (fip->check != f)
	{
		tlog(NULL, "Bug! Bogus fifo cross-check: %d vs %d", fip->check, f);
		die();
	}

	va_start (ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end (ap);

	if (cli_write(fip->fd, buf, buf) < 0)
	{
		tlog(NULL, "%s: %s", fip->name, buf);
		die();
	}

	fip->pending = 1;
}

/* called whenever we get input from the Tel fifo */
/* ARGSUSED */
static int tel_rd_cb(fip)
	FifoInfo *fip;
{
	return (generic_rd_cb(fip));
}

/* called whenever we get input from the FILTER fifos */
/* ARGSUSED */
static int filter_rd_cb(fip)
	FifoInfo *fip;
{
	char tf = IMOT->have ? telstatshmp->filter : '?'; /*mybe just reset ok*/
	char sf = cscan->filter;
	char buf[4096];
	int s;

	s = fifoRead(fip, buf, sizeof(buf));

	/* not using generic() so we can include the filter name */
	if (s >= 0)
	{
		tlog(cscan, "Filter: %c %s", tf, buf);
		return (0);
	}
	else
	{
		tlog(cscan, "Filter: error: %c %s", sf, buf);
		return (-1);
	}
}

/* called whenever we get input from the Focus fifos */
/* ARGSUSED */
static int focus_rd_cb(fip)
	FifoInfo *fip;
{
	return (generic_rd_cb(fip));
}

/* called whenever we get input from the DOME fifos */
/* ARGSUSED */
static int dome_rd_cb(fip)
	FifoInfo *fip;
{
	return (generic_rd_cb(fip));
}

/* called whenever we get input from the COVER fifos */
/* ARGSUSED */
static int cover_rd_cb(fip)
	FifoInfo *fip;
{
	return (generic_rd_cb(fip));
}

/* called whenever we get input from the Lights fifos */
/* ARGSUSED */
static int lights_rd_cb(fip)
	FifoInfo *fip;
{
	return (generic_rd_cb(fip));
}

/* generic fifo callback, used when all we do is report status and check errs */
/* ARGSUSED */
static int generic_rd_cb(fip)
	FifoInfo *fip;
{
	char buf[4096];
	int s;

	s = fifoRead(fip, buf, sizeof(buf));

	if (s >= 0)
	{
		tlog(cscan, "%s: %s", fip->name, buf);
		return (0);
	}
	else
	{
		tlog(cscan, "%s: error: %s", fip->name, buf);
		return (-1);
	}
}

/* camera daemon is reporting in */
/* ARGSUSED */
static int camera_rd_cb(fip)
	FifoInfo *fip;
{
	char buf[4096];
	int s;

	s = fifoRead(fip, buf, sizeof(buf));

	/* can not use generic since this fifo can be in use for background
	 * processing when we don't know the current scan for sure.
	 */
	if (s >= 0)
	{
		tlog(NULL, "Camera: %s", buf);
		return (0);
	}
	else
	{
		tlog(NULL, "Camera: error: %s", buf);
		return (-1);
	}
}

/* read message from the given fifo, return the leading code, put remainder
 * of message in buf (without the leading number).
 */
static int fifoRead(fip, buf, l)
	FifoInfo *fip;char *buf;int l;
{
	int code;

	if (cli_read(fip->fd, &code, buf, l) < 0)
	{
		tlog(NULL, "%s: %s", fip->name, buf);
		die();
	}

	if (code <= 0)
		fip->pending = 0;

	return (code);
}

/* just absort a SIGALRM */
static void on_alrm(int dummy)
{
}

/* For RCS Only -- Do Not Edit */
static char
		*rcsid[2] =
				{
						(char *) rcsid,
						"@(#) $RCSfile: fifoio.c,v $ $Date: 2001/04/19 21:12:10 $ $Revision: 1.1.1.1 $ $Name:  $" };
