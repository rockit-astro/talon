/* This program listens to a pair of fifos for a generic camera command set and
 * manipulates the generic dev/ccdcamera interface. See ccdcamera.h for the
 * commands. We update our state in telstatshm.
 *
 * Commands come in Camera.in as ASCII lines as follows:
 *   Expose X+YxWxH bxXby Dur Shutter Priority Filefile
 *   Source
 *   Comment
 *   Title
 *   Observer
 *
 * N.B. it is up to the clients to wait for a response; if they just send
 *   another command the previous response will be dropped and will not get a
 *   response.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "fits.h"
#include "telstatshm.h"
#include "configfile.h"
#include "cliserv.h"
#include "telenv.h"
#include "strops.h"
#include "running.h"
#include "misc.h"
#include "telfits.h"
#include "ccdcamera.h"
#include "tts.h"

#define	MAXLINE		4096	/* max message from a fifo */


// config setting for time sync
extern int TIME_SYNC_DELAY; // in ccdcamera.c

/* info about a fifo connection */
typedef struct {
    char *name;		/* fifo name; also env name to override default*/
    void (*fp)();	/* function to call to process input from this fifo */
    int fd[2];		/* file descriptors, once opened */
} FifoInfo;

static int verbose;	/* more gets more verbose trace messages */
static TelStatShm *telstatshmp;/* pointer to shared memory segment */

static void usage(char *progname);
static void init_all(void);
static void init_cfg(void);
static void init_shm(void);
static void die(int status);
static void open_fifo(void);
static void on_term(int dummy);
static void mainLoop(void);
static void addID(FImage *fip);
static void addCDELT (FImage *fip);
static void abandon (void);
static void cam_fifo(char *msg);
static int camOK (char *msg);
static int getCoolerTemp (CCDTempInfo *tp);
static void doExpose (char *msg);
static void cam_read(void);
static void flipImg(void);
static void reply (int code, char *fmt, ...);
static void signalNewExposure(char *fname);

static FifoInfo fifo = {"Camera", cam_fifo};
static FifoInfo *fifop = &fifo;

static FImage fimage;		/* info about total result */
static char fname[1024];	/* filename to be written with new image */

/* External command signal of new file */
static char extCmd_name[512];

/* orientation and scale info from config file */
static char camcfg[] = "archive/config/camera.cfg";
static double HPIXSZ, VPIXSZ;   /* degrees/pix */
static int CAMDIG_MAX;		/* max time for full-frame download, secs */
static int DEFTEMP;		/* default temp to set, C */
static int LRFLIP, TBFLIP;	/* 1 to flip */
static int RALEFT, DECUP;	/* raw increase */
static char tele_kw[80];	/* TELESCOP keyword */
static char orig_kw[80];	/* ORIGIN keyword */
static char id_instrume[80];	/* Intrument identification string */

/* what kind of driver */
static char driver[1024];
static int auxcam;

static char *progname;

int
main (int ac, char *av[])
{
	char *str;

	progname = basenm(av[0]);

	/* crack arguments */
	for (av++; --ac > 0 && *(str = *av) == '-'; av++) {
	    char c;
	    while ((c = *++str) != '\0')
		switch (c) {
		case 'v':	/* more verbosity */
		    verbose++;
		    break;
		default:
		    usage(progname);
		    break;
		}
	}

	/* now there are ac remaining args starting at av[0] */
	if (ac > 0)
	    usage(progname);

        /* only ever allow one instance to run */
	if (lock_running(progname) < 0) {
	    daemonLog ("%s: Already running\n", progname);
	    exit(0);
	}

	/* init subsystems */
	init_all();

	/* init shm status */
	telstatshmp->camstate = CAM_IDLE;

	/* go */
	mainLoop();

	/* should never return */
	daemonLog ("Surprise ending\n");
	return (1);
}

static void
usage (progname)
char *progname;
{
	fprintf (stderr, "%s: [options]\n", progname);
	fprintf (stderr, " -v: more makes more verbose.\n");
	exit (1);
}

/* initialize various subsystems */
static void
init_all()
{
	char buf[1024];

	/* get default parameters */
	init_cfg();

	/* init the telstatshm segment */
	init_shm();

	/* set camera cooling once, also insures we can connect */
	if (camOK(buf) < 0) {
	    daemonLog ("%s", buf);
	    die(0);
	}

	/* connect the signal handlers */
	signal (SIGTERM, on_term);

	/* open the fifo */
	open_fifo();
}

static void
init_shm()
{
	int shmid;
	long addr;

	shmid = shmget (TELSTATSHMKEY, sizeof(TelStatShm), 0666|IPC_CREAT);
	if (shmid < 0) {
	    perror ("shmget TELSTATSHMKEY");
	    exit (1);
	}

	addr = (long) shmat (shmid, (void *)0, 0);
	if (addr == -1) {
	    perror ("shmat TELSTATSHMKEY");
	    exit (1);
	}

	telstatshmp = (TelStatShm *) addr;
}

static void
init_cfg()
{
#define	NCCFG	(sizeof(ccfg)/sizeof(ccfg[0]))
	static CfgEntry ccfg[] = {
	    {"TELE",	CFG_STR,	tele_kw, sizeof(tele_kw)},
	    {"ORIG",	CFG_STR,	orig_kw, sizeof(tele_kw)},
	    {"VPIXSZ",	CFG_DBL,	&VPIXSZ},
	    {"HPIXSZ",	CFG_DBL,	&HPIXSZ},
	    {"LRFLIP",	CFG_INT,	&LRFLIP},
	    {"TBFLIP",	CFG_INT,	&TBFLIP},
	    {"RALEFT",	CFG_INT,	&RALEFT},
	    {"DECUP",	CFG_INT,	&DECUP},
	    {"DEFTEMP",	CFG_INT,	&DEFTEMP},
	    {"CAMDIG_MAX", CFG_INT,	&CAMDIG_MAX},
	    {"ID_INSTRUME",	CFG_STR,	id_instrume, sizeof(id_instrume)},
	};
	int n;

	/* gather config file info */
	n = readCfgFile (1, camcfg, ccfg, NCCFG);
	if (n != NCCFG) {
	    cfgFileError (camcfg, n, NULL, ccfg, NCCFG);
	    exit(1);
	}

	// read optional item(s)
    if(read1CfgEntry(1, camcfg, "DRIVER", CFG_STR, &driver, sizeof(driver)) < 0) {
        strcpy(driver, "");
    }
	if(read1CfgEntry(1, camcfg, "AUXCAM", CFG_INT, &auxcam, sizeof(auxcam)) < 0) {
		auxcam = 0;
	}
	if(read1CfgEntry(1, camcfg, "SIGNALCMD", CFG_STR, extCmd_name, sizeof(extCmd_name)) < 0) {
		strcpy(extCmd_name,"");
	}
	if(read1CfgEntry(1, camcfg, "TIME_SYNC_DELAY", CFG_INT, &TIME_SYNC_DELAY, sizeof(TIME_SYNC_DELAY)) < 0) {
		TIME_SYNC_DELAY = 0;
	}

	/* we want degrees */
	VPIXSZ /= 3600.0;
	HPIXSZ /= 3600.0;
}

/* create and attach to the command fifo.
 * exit if trouble.
 */
static void
open_fifo()
{
	char buf[1024];

	if (serv_conn (fifop->name, fifop->fd, buf) < 0) {
	    daemonLog ("%s", buf);
	    exit(1);
	}
}

/* stop the camera then exit */
static void
die(int status)
{
	if (verbose)
	    daemonLog ("die()");

	reply (-1, "Final error");
	dis_conn (fifop->name, fifop->fd);
	unlock_running (progname, 0);
	if (telstatshmp)
	    (void) shmdt ((void *)telstatshmp);
	exit (status);
}

/* die gracefully on sigterm */
static void
on_term(int dummy)
{
	die(0);
}

static void
mainLoop()
{
	struct timeval tv, *tvp = NULL;
	char buf[1024];
	fd_set rfdset;
	int maxfdp1;
	int cam_fd = -1;
	int s;

	/* infinite loop processing commands from the fifo and driver */
	while (1) {

	    /* always listen to fifop */
	    FD_ZERO (&rfdset);
	    FD_SET (fifop->fd[0], &rfdset);
	    maxfdp1 = fifop->fd[0] + 1;

	    /* set timeout value, if any, and add cam_fd if active */
	    switch (telstatshmp->camstate) {
	    case CAM_IDLE:
		/* just idling -- wait forever for fifo */
		cam_fd = -1;
		tvp = NULL;
		break;

	    case CAM_EXPO:
		/* wait for exposure */
		cam_fd = selectHandleCCD(buf);
		if (cam_fd < 0) {
		    reply (-2, "EXPO can not get camera fd: %s", buf);
		    continue;
		}
		tv.tv_sec = fimage.dur/1000 + CAMDIG_MAX;
		tv.tv_usec = 0;
		tvp = &tv;
		break;

	    case CAM_READ:
		/* wait for download */
		cam_fd = selectHandleCCD(buf);
		if (cam_fd < 0) {
		    reply (-3, "READ can not get camera fd: %s", buf);
		    continue;
		}
		tv.tv_sec = CAMDIG_MAX;
		tv.tv_usec = 0;
		tvp = &tv;
	    }

	    if (cam_fd >= 0) {
		FD_SET (cam_fd, &rfdset);
		if (cam_fd + 1 > maxfdp1)
		    maxfdp1 = cam_fd + 1;
	    }

	    if (verbose)
		daemonLog ("sleeping %d sec %s", tvp ? tvp->tv_sec : 100000,
					    cam_fd >= 0 ? "for cam_fd" : "");

	    /* call select, waiting for commands or timeout */
	    s = select (maxfdp1, &rfdset, NULL, NULL, tvp);
	    if (s < 0) {
		if (errno == EINTR)
		    continue;
		perror ("select");
		die(1);	/* never returns */
	    }
	    if (s == 0) {
		/* timed out -- means trouble if we are active */
		switch (telstatshmp->camstate) {
		case CAM_IDLE:
		    break;
		case CAM_EXPO:
		    reply (-4, "Exposure timed out");
		    break;
		case CAM_READ:
		    reply (-5, "Download timed out");
		    break;
		}
		continue;
	    }

	    /* respond to the driver if its fd is ready */
	    if (cam_fd >= 0 && FD_ISSET (cam_fd, &rfdset)) {
		cam_read();
		continue;
	    }

	    /* read and process a message arriving on the fifo */
	    if (FD_ISSET (fifop->fd[0], &rfdset)) {
		char msg[MAXLINE];
		int n = serv_read (fifop->fd, msg, sizeof(msg)-1);

		if (n < 0) {
		    daemonLog ("%s: %s", fifop->name, msg);
		    die(1);
		}
		if (verbose)
		    daemonLog ("%s -> %s", fifop->name, msg);
		(*fifop->fp) (msg);
	    }
	}
}

/* abandon any current exposure */
static void
abandon()
{
	abortExpCCD();
	telstatshmp->camstate = CAM_IDLE;
	resetFImage (&fimage);
}

/* called when we receive a message from the Camera fifo.
 */
/* ARGSUSED */
static void
cam_fifo (char *msg)
{
	char buf[1024];

	if (strncasecmp (msg, "Expose", 6) == 0) {
	    if (telstatshmp->camstate != CAM_IDLE) abandon();		/* just paranoid */
	    doExpose (msg);	/* handles all its own reply's */
	} else if (strncasecmp (msg, "Stop", 4) == 0) {
	    abandon();
	    reply (0, "Stop complete");
	} else if (strncasecmp (msg, "Reset", 5) == 0) {
	    init_cfg();
		// Do this again, because we might have changed cameras
		if (camOK(buf) < 0) {
			reply(-99,"Error during camera reset: %s",buf);
	    	daemonLog ("%s", buf);
		    //die(0);
		}
	    else reply (0, "Reset complete");
	} else {
	    /* anything else is an abort/reconfig */
	    reply (-7, "Unknown command: %s", msg);
	    init_cfg();
	}
}

/* fetch and save cooler temp */
static int
getCoolerTemp (CCDTempInfo *tp)
{
	char errmsg[1024];

	if (getTempCCD (tp, errmsg) < 0) {
	    daemonLog ("%s", errmsg);
	    return (-1);
	}

	telstatshmp->camtemp = tp->t;
	telstatshmp->coolerstatus = tp->s;

	return (0);
}

/* return 0 if camera appears to be connected and cooling ok, else -1 */
static int
camOK(char msg[])
{
	CCDTempInfo tinfo;

	if (setPathCCD (driver, auxcam, msg) < 0)
	    return (-1);
	if (getCoolerTemp(&tinfo) < 0) {
	    sprintf (msg, "Can not get cooler temp");
	    return (-1);
	}

	if (tinfo.s != CCDTS_AT) {
	    tinfo.s = CCDTS_SET;
	    tinfo.t = DEFTEMP;
	    if (setTempCCD (&tinfo, msg) < 0)
		return (-1);
	    telstatshmp->camtarg = DEFTEMP;
	}

	return (0);
}

/* msg is the first line of an Expose message.
 * eat the whole message and start the real camera.
 * if all ok, return expecting driver to tell us when exposure is done.
 * reply with a negative code if trouble.
 */
static void
doExpose (char *msg)
{
	FImage *fip = &fimage;
	int nbytes;
	int bx, by;
	int sx, sy, sw, sh;
	double dur;
	int pri;
	int shutter;
	char buf[1024];
	char src[128];
	char cmt[128];
	char ttl[128];
	char obs[128];
	CCDExpoParams ep;
	int s;

	/* first line is all the exposure info */
	s = sscanf (msg, "Expose %d+%dx%dx%d %dx%d %lf %d %d %s",
		&sx, &sy, &sw, &sh, &bx, &by, &dur, &shutter, &pri, fname);
	if (s != 10) {
	    reply (-8, "Expose format error: %s", msg);
	    return;
	}
	if (sx<0 || sy<0 || bx<1 || by<1 || sw/bx<1 || sh/by<1 || dur<0.0) {
	    reply (-9, "Bad Expose args: %d+%dx%dx%d %dx%d %g",
					    sx, sy, sw, sh, bx, by, dur);
	    return;
	}

	/* then source, comment, title, observer */
	s = serv_read (fifop->fd, src, sizeof(src));
	if (s < 0) {
	    reply (-10, "Error reading Source: %s", src);
	    return;
	}
	s = serv_read (fifop->fd, cmt, sizeof(cmt));
	if (s < 0) {
	    reply (-11, "Error reading Comment: %s", cmt);
	    return;
	}
	s = serv_read (fifop->fd, ttl, sizeof(ttl));
	if (s < 0) {
	    reply (-12, "Error reading Title: %s", ttl);
	    return;
	}
	s = serv_read (fifop->fd, obs, sizeof(obs));
	if (s < 0) {
	    reply (-13, "Error reading Observer: %s", obs);
	    return;
	}

	if (verbose)
	    daemonLog ("Expose %d+%dx%dx%d %dx%d %g S%d P%d %s\n    %s\n    %s\n    %s\n    %s\n",
			sx, sy, sw, sh, bx, by, dur, shutter, pri, fname,
			src, cmt, ttl, obs);

	/* fill in fimage */
	initFImage (fip);
	fip->bitpix = 16;
	fip->sw = sw/bx;
	fip->sh = sh/by;
	fip->sx = sx;
	fip->sy = sy;
	fip->bx = bx;
	fip->by = by;
	fip->dur = dur*1000; /* N.B.: dur is in secs; FImage.dur is in ms */

	/* get memory for the pixels */
	nbytes = fip->sw * fip->sh * fip->bitpix/8;
	fip->image = malloc (nbytes);
	if (!fip->image) {
	    reply (-14, "Can't malloc(%d) for pixels", nbytes);
	    return;
	}

	/* set up the FITS header.
	 * Simple does SIMPLE BITPIX NAXIS NAXIS1 NAXIS2 BZERO BSCALE OFFSET1
	 * OFFSET2 XFACTOR YFACTOR EXPTIME
	 */
	setSimpleFITSHeader (fip);
	if (orig_kw[0] != '\0')
	    setStringFITS (fip, "ORIGIN", orig_kw, NULL);
	if (tele_kw[0] != '\0')
	    setStringFITS (fip, "TELESCOP", tele_kw, NULL);
	setCommentFITS (fip, "COMMENT", ttl);
	setCommentFITS (fip, "COMMENT", cmt);
	setStringFITS  (fip, "OBSERVER", obs, "Investigator(s)");
	setStringFITS  (fip, "OBJECT", src, "Object name");
	setIntFITS  (fip, "PRIORITY", pri, "Scheduling priority");
	addID (fip);
	addCDELT (fip);

	/* ok, try telling all this to the camera */

	ep.bx = bx;
	ep.by = by;
	ep.sx = sx;
	ep.sy = sy;
	ep.sw = sw;
	ep.sh = sh;
	ep.duration = dur*1000;
	ep.shutter = shutter;

    // STO... set this stat BEFORE exposure so we don't get caught up
    // in pre-expose timing delays (i.e. FLI flush)
	telstatshmp->camstate = CAM_EXPO;

	if (setExpCCD (&ep, buf) < 0 || startExpCCD (buf) < 0) {
    	telstatshmp->camstate = CAM_IDLE;
	    reply (-15, "Setup error: %s", buf);
	    return;
	}

	/* yes! */
	telstatshmp->camstate = CAM_EXPO;
	toTTS ("Beginning %g second exposure.", dur);
	return;
}

/* add the name of the camera manufaturer as the INSTRUME string */
static void
addID (FImage *fip)
{
	char buf[256], err[1024];
	if (getIDCCD (buf, err) == 0)
	{
		if (strlen(id_instrume) != 0)
		{
			strcpy(buf, id_instrume);
		}

		setStringFITS (fip, "INSTRUME", buf, NULL);
	}

}

/* figure out the right signs for CDELT{1,2} */
static void
addCDELT (FImage *fip)
{
	int sign;

	sign = LRFLIP == RALEFT ? 1 : -1;
	printf("LRFLIP=%d, RALEFT=%d\n", LRFLIP, RALEFT);
	printf("CDELT1 sign: %d\n", sign);
	setRealFITS (fip, "CDELT1", HPIXSZ*fip->bx * sign, 10,
						"RA step right, degrees/pixel");

	sign = TBFLIP == DECUP  ? 1 : -1;
	printf("TBFLIP=%d, DECUP=%d\n", TBFLIP, DECUP);
	printf("CDELT2 sign: %d\n", sign);
	setRealFITS (fip, "CDELT2", VPIXSZ*fip->by * sign, 10,
						"Dec step down, degrees/pixel");
}

/* select() says pixels are ready from the camera.
 */
static void
cam_read()
{
	FImage *fip = &fimage;
	char msg[1024];
	CCDTempInfo tinfo;
	int nbytes;
//	time_t endt;
	int fd;
	int s;

	/* get the time and other stats when the exposure ended */
//	endt = time (NULL);
	timeStampFITS (fip, /*endt*/0, "Time at end of exposure");
	getCoolerTemp (&tinfo);
	addShmFITS (fip, telstatshmp);

	if (verbose)
	    daemonLog ("creating %s", fname);

	/* check for unexpected state */
	if (telstatshmp->camstate != CAM_EXPO) {
	    reply (-16, "Pixels ready but state not EXPO: %d",
							telstatshmp->camstate);
	    return;
	}

	/* inform fifo listener the shutter is closed */
	telstatshmp->camstate = CAM_READ;	/* set before reply */
	reply (1, "Exposure complete");
	toTTS ("The exposure is finished. Now downloading pixels.");

	/* read the pixels -- don't set IDLE until finished writing to disk */
	nbytes = fip->sw * fip->sh * fip->bitpix/8;
	s = readPixelCCD (fip->image, nbytes, msg);
	if (s < 0) {
	    reply (-17, "Readout error: %s", msg);
	    return;
	}

	/* do any desired flipping */
	flipImg();

	/* write the completed FITS file from fimage */
	fd = open (fname, O_RDWR|O_CREAT|O_TRUNC, 0666);
	if (fd < 0) {
	    reply (-18, "File create: %s: %s", fname, strerror (errno));
	    return;
	} else {
	    fchmod (fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
	    s = writeFITS (fd, fip, msg, 0);
	    (void) close (fd);
	    if (s < 0) {
		reply (-19, "File i/o: %s: %s", fname, msg);
		return;
	    }
	}

	/* Now give external signal to registered command */
	if(strlen(extCmd_name) > 0) {
		(void) signalNewExposure(fname);
	}

	/* ok! */
	resetFImage (fip);
	telstatshmp->camstate = CAM_IDLE;	/* set before sending message */
	reply (0, "File %s created", basenm(fname));
	toTTS ("Image is now complete.");
}

/*
 * Signal an external command and notify that a file has been saved
 * This external command will receive the file path as a single parameter
 *
 * See call in camRead
 * See extCmd_name set in initCfg
 */
static void
signalNewExposure(char *fname)
{
	int s;
	char cmd[1024];

	if(strlen(extCmd_name) == 0) return; 	// no external command registered

	sprintf (cmd, "nice %s %s &> /dev/null &",extCmd_name,fname);
	if ((s = system(cmd)) != 0) {
	    daemonLog ("signalNewExposure fork failed: %s", strerror(errno));
	}

}


/* implement any required/desired image row and/or column flipping.
 * N.B. for germal eq in flip mode, we add an extra 180 rot via 2 flips
 */
static void
flipImg()
{
	int geflip = telstatshmp->tax.GERMEQ && telstatshmp->tax.GERMEQ_FLIP;
	int flip;

	flip = LRFLIP;
	if (geflip)
	    flip = !flip;
	if (flip)
	    flipImgCols ((CamPixel *)fimage.image, fimage.sw, fimage.sh);

	flip = TBFLIP;
	if (geflip)
	    flip = !flip;
	if (flip)
	    flipImgRows ((CamPixel *)fimage.image, fimage.sw, fimage.sh);
}

/* write a code and message to the client fifo.
 * also abandon() if code is < 0.
 * it's ok if no one is listening.
 */
static void
reply (int code, char *fmt, ...)
{
	char buf[1024];
	char err[1024];
	va_list ap;
	int n;

	if (code < 0)
	    abandon();

	va_start (ap, fmt);
	vsprintf (buf, fmt, ap);
	va_end (ap);

	n = serv_write (fifop->fd, code, buf, err);
	if (n < 0 && verbose)
	    daemonLog ("%s: %s", fifop->name, err);
	else if (verbose || code < 0)
	    daemonLog ("%s <- %s", fifop->name, buf);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: camerad.c,v $ $Date: 2006/08/27 20:21:30 $ $Revision: 1.11 $ $Name:  $"};
