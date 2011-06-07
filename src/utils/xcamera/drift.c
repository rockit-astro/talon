/* drift scan stuff */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <Xm/Xm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "strops.h"
#include "xtools.h"
#include "fits.h"
#include "fitscorr.h"
#include "configfile.h"
#include "fieldstar.h"
#include "telenv.h"
#include "ccdcamera.h"
#include "telstatshm.h"
#include "telfits.h"

#include "camera.h"

#define	DS_MS	200			/* drift scan update interval, ms */
static XtIntervalId dsTID;		/* used for monitoring scan progress */
static int dscan_shmid;			/* shared mem id for dscan */
static CCDDriftScan *dscanp;		/* shared mem addr for dscan */
static int nbytes;			/* bytes in state.fimage.image */
static int pix_shmid;			/* shared mem id for pixel array */
static char *pixarray;			/* shared mem pixel array */
static int dsapid;			/* drift scan aux pid */
static CCDExpoParams ce;		/* chip size */
static int aux_died;			/* set if aux child dies unexpectedly */
static State laststate;			/* try to avoid excess drawing */
static int lastrow;			/* used to watch for change and wrap */


static void driftScanTO (XtPointer client, XtIntervalId *idp);
static char *initShm(int len, int *shmidp);
static int startDSAuxProc (void);
static void auxProcess (int ccdfd);
static void onChild (int signo);
static void newFITS(int usdur);

/* return 0 if even supporting drift scan, else -1 */
int
haveDS(void)
{
	char buf[1024];
	int ret;

	ret = setupDriftScan (NULL, buf);
	abortExpCCD();	/* just to insure driver is closed */
	return (ret);
}

/* start continuous drift scanning at the given rate, us per row. */
void
startCDS (int us)
{
	char errmsg[1024];

	/* compute full-chip array size */
	if (getSizeCCD (&ce, errmsg) < 0) {
	    msg ("Can't get camera size: %s", errmsg);
	    return;
	}
	nbytes = ce.sw * ce.sh * sizeof(CamPixel);

	/* build a new FITS image */
	newFITS(us);

	/* put fimage.image in shared memory */
	pix_shmid = 0;
	pixarray = initShm (nbytes, &pix_shmid);
	if (!pixarray)
	    return;

	/* build a CCDDriftScan struct in shared memory */
	dscan_shmid = 0;
	dscanp = (CCDDriftScan *) initShm (sizeof(CCDDriftScan), &dscan_shmid);
	if (!dscanp) {
	    dsStop();
	    return;
	}
	dscanp->row = 0;
	dscanp->rowint = us;

	/* open shutter */
	if (setShutterNow (1, errmsg) < 0) {
	    msg ("Drift scan shutter: %s\n", errmsg);
	    dsStop();
	    return;
	}

	/* start helper process that produces the scan in shared mem */
	aux_died = 0;
	signal (SIGCHLD, onChild);
	if (startDSAuxProc() < 0) {
	    dsStop();
	    return;
	}

	/* start timer to watch */
	lastrow = 0;
	memset (&laststate, 0, sizeof(State));
	dsTID = XtAppAddTimeOut (app, DS_MS, driftScanTO, 0);
}

/* install a new drift scan interval */
void
dsNewInterval (int us)
{
	FImage *fip = &state.fimage;

	dscanp->rowint = us;

	/* update FITS and header */
	fip->dur = 1e-3 * us * ce.sh;		/* want ms */
	setSimpleFITSHeader (fip);
	showHeader();
}

/* called to stop drift scanning.
 * should be safe to call any time want to clean up.
 */
void
dsStop(void)
{
	if (dsapid > 0) {
	    (void) kill (dsapid, SIGINT);
	    (void) waitpid (dsapid, NULL, 0);
	    dsapid = 0;
	}
	if (dscan_shmid) {
	    struct shmid_ds s;
	    (void) shmctl (dscan_shmid, IPC_RMID, &s);
	    dscan_shmid = 0;
	}
	if (pix_shmid) {
	    struct shmid_ds s;
	    (void) shmctl (pix_shmid, IPC_RMID, &s);
	    pix_shmid = 0;
	}
	if (dsTID) {
	    XtRemoveTimeOut (dsTID);
	    dsTID = 0;
	}
	abortExpCCD();
}

/* drift scan monitor */
/* ARGSUSED */
static void
driftScanTO (XtPointer client, XtIntervalId *idp)
{
	if (aux_died) {
	    msg ("Drift scan helper process died");
	    camCancel();
	} else {
	    /* grab latest pixels */
	    memcpy (state.fimage.image, pixarray, nbytes);

	    /* update screen but try to do as little work as possible */
	    if (memcmp (&state, &laststate, sizeof(State))) {
		presentNewImage();
		memcpy (&laststate, &state, sizeof(State));
	    } else
		newXImage();
	    XmUpdateDisplay(toplevel_w);

	    /* show new row count and possibly save when fill */
	    if (dscanp->row != lastrow) {
		msg ("Row %d", dscanp->row);
		if (dscanp->row < lastrow) {
		    /* done.. save if enabled */
		    if (!saveAuto() && !mkTemplateName (state.fname)) {
			setSaveName();
			writeImage();
		    }
		}
		lastrow = dscanp->row;
	    }

	    /* repeat */
	    dsTID = XtAppAddTimeOut (app, DS_MS, driftScanTO, 0);
	}
}

/* create a private shared mem segment.
 * return address in memory and new shmid, else 0.
 */
static char *
initShm(int len, int *shmidp)
{
	char *newaddr;
	int shmid;

	shmid = shmget (IPC_PRIVATE, len, 0644);
	if (shmid < 0) {
	    if (shmid < 0) {
		msg ("shmget: %s", strerror(errno));
		return (0);
	    }
	}

	newaddr = shmat (shmid, 0, 0);
	if ((int)newaddr == -1) {
	    struct shmid_ds s;
	    msg ("shmat: %s", strerror(errno));
	    (void) shmctl (shmid, IPC_RMID, &s);
	    return (0);
	}

	memset (newaddr, 0, len);
	*shmidp = shmid;
	return (newaddr);
}

/* fork the process that does the actual drift scan. since it reads into
 * shared memory we can control it from here.
 */
static int
startDSAuxProc (void)
{
	char errmsg[1024];
	int ccdfd;

	/* install scan params and get driver open */
	if (setupDriftScan (dscanp, errmsg) < 0) {
	    msg ("Drift scan: %s", errmsg);
	    return (-1);
	}

	/* get fd to use */
	ccdfd = selectHandleCCD (errmsg);
	if (ccdfd < 0) {
	    msg ("Driver: %s", errmsg);
	    return (-1);
	}

	/* fork a new process */
	dsapid = fork();
	if (dsapid < 0) {
	    msg ("fork: %s", strerror(errno));
	    return (-1);
	}

	/* parent just goes on */
	if (dsapid)
	    return (0);

	/* new child process -- never returns */
	auxProcess (ccdfd);

	/* just for lint */
	return (0);
}
	
/* helper process that just does the scanning read into shared mem */
static void
auxProcess (int ccdfd)
{
	char errmsg[1024];
	int i;

	/* close X connection */
	for (i = 3; i < 100; i++)
	    if (i != ccdfd)
		close (i);

	/* open shutter */
	if (setShutterNow (1, errmsg) < 0) {
	    printf ("Shutter: %s\n", strerror(errno));
	    exit(1);
	}

	/* child just issues read which blocks and causes drift scan to run */
	i = read (ccdfd, pixarray, nbytes);
	if (i < 0)
	    printf ("Aux: %s\n", strerror(errno));

	/* close and exit */
	(void) setShutterNow (0, errmsg);
	exit (i < 0 ? 1 : 0);
}

/* helper process died.
 * N.B. just flag it, don't do things asyncronously here.
 */
static void
onChild (int signo)
{
	aux_died = 1;
}

/* set up state.fimage for a new run.
 */
static void
newFITS(int usdur)
{
	FImage *fip = &state.fimage;
	char cam_id[256];

	resetFImage (fip);

	fip->image = malloc (nbytes);
	if (!fip->image) {
	    printf ("Can not get %d bytes for FITS image\n", nbytes);
	    exit(1);
	}

	fip->bitpix = 16;
	fip->sw = ce.sw;
	fip->sh = ce.sh;
	fip->sx = 0;
	fip->sy = 0;
	fip->bx = 1;
	fip->by = 1;
	fip->dur = 1e-3 * usdur * ce.sh;	/* want ms */

	setSimpleFITSHeader (fip);

	if (getIDCCD (cam_id, cam_id) == 0 && cam_id[0])
	    setStringFITS (fip, "INSTRUME", cam_id, NULL);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: drift.c,v $ $Date: 2001/04/19 21:11:59 $ $Revision: 1.1.1.1 $ $Name:  $"};
