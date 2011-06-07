#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "ccdcamera.h"
#include "telenv.h"

/* CCD interface for OCAAS-compliant kernel drivers.
 * 
 * KMI - I'm not quite sure how this works or if it is sitll used by anyone.
 * It's easy enough to keep the code in here, but this is currently
 * untested and may not still function correctly
 */

static int ccd_fd = -1;		/* cam file descriptor */
static char *ccd_fn = "dev/ccdcamera";

/************ Support functions for the public Talon camera API ***********/

/* open the CCD camera with O_NONBLOCK and save filedes in ccd_fd.
 * return 0 if ok, else fill errmsg[] and return -1.
 */
static int
openCCD(char *errmsg)
{
	if (ccd_fd >= 0)
	    return (0);

	ccd_fd = telopen (ccd_fn, O_RDWR | O_NONBLOCK);
	if (ccd_fd < 0) {
	    switch (errno) {
	    case ENXIO:
		strcpy (errmsg, "Camera not responding.");
		break;
	    case ENODEV:
		strcpy (errmsg, "Camera not present.");
		break;
	    case EBUSY:
		strcpy (errmsg, "Camera is currently busy.");
		break;
	    default:
		sprintf (errmsg, "%s: %s", ccd_fn, strerror (errno));
		break;
	    }
	    return (-1);
	}
	return (0);
}

static void
closeCCD()
{
	if (ccd_fd >= 0) {
	    (void) close (ccd_fd);
	    ccd_fd = -1;
	}
}



/***************** Begin public Talon CCD interface functions *************/

/* Detect and initialize a camera. Optionally use information passed in 
 * via the *path string. For example, this may be a path to a kernel
 * device node, a path to an auxcam script, an IP address for a networked
 * camera, etc.
 *
 * Return 1 if camera was detected and successfully initialized.
 * Return 0 if camera was not detected.
 * Return -1 and put error string in *errmsg if camera was detected but
 * could not be initialized.
 */
int
ocaas_findCCD (char *path, char *errmsg)
{
    int wasclosed = ccd_fd < 0;

    ccd_fn = path;
    if (openCCD (errmsg) < 0)
        return (-1);
    if (wasclosed)
        closeCCD();

    return 1;
}

/* check and save the given exposure parameters.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
ocaas_setExpCCD (CCDExpoParams *expP, char *errmsg)
{
    int s;
    if (openCCD (errmsg) < 0)
        return (-1);
    s = ioctl (ccd_fd, CCD_SET_EXPO, expP);
    if (s < 0) {
        switch (errno) {
            case ENXIO:
                (void) sprintf (errmsg, "CCD exposure parameters error.");
                break;
            default:
                (void) sprintf (errmsg, "CCD error: %s",strerror(errno));
                break;
        }
        closeCCD();
    }

    return (s);
}

/* start an exposure, as previously defined via setExpCCD().
 * we return immediately but leave things open so ccd_fd or aux_fd can be used.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
ocaas_startExpCCD (char *errmsg)
{
    char dummy[1];

    if (openCCD (errmsg) < 0)
        return (-1);

    /* make sure it's nonblocking, so the subsequent read just
     * starts it
     */
    if (fcntl (ccd_fd, F_SETFL, O_NONBLOCK) < 0) {
        (void) sprintf (errmsg, "startExpCCD() fcntl: %s",
                strerror (errno));
        closeCCD();
        return (-1);
    }

    /* start the exposure by starting a read.
     * we need not supply a buffer since ccd_fd is open for
     * nonblocking i/o and the first call always just starts the i/o.
     * but we do have to supply a positive count so the read calls the
     * driver at all.
     */
    if (read (ccd_fd, dummy, 1) < 0 && errno != EAGAIN) {
        (void) sprintf (errmsg, "startExpCCD(): %s", strerror(errno));
        closeCCD();
        return (-1);
    }

    return 0;
}

/* set up for a new drift scan.
 * return 0 if ok else -1 with errmsg.
 * N.B. leave camera open of ok.
 */
int 
ocaas_setupDriftScan (CCDDriftScan *dsip, char *errmsg)
{
    if (openCCD (errmsg) < 0)
        return (-1);
    if (ioctl (ccd_fd, CCD_DRIFT_SCAN, dsip) < 0) {
        strcpy (errmsg, strerror(errno));
        closeCCD();
        return (-1);
    }
    return (0);
}

/* Performs RBI Annihilation process, if supported.
 * RBI Annihilation floods the CCD array with charge for
 * an "exposure" and then discards and drains.
 * This prevents exposure to bright objects from creating
 * Residual Buffer Images in subsequent frames.
 * Return 0 if ok, else set errmsg[] and return -1.
 */
int 
ocaas_performRBIFlushCCD (int flushMS, int numDrains, char* errmsg)
{
    sprintf (errmsg, "OCAAS drivers do not support RBI Annihilation");
    return (-1);
}

/* abort a current exposure, if any.
 */
void 
ocaas_abortExpCCD (void)
{
    /* just closing the driver will force an exposure abort if needed */
    closeCCD();
}

/* after calling startExpCCD() or setupDriftScan(), call this to obtain a
 * handle which is suitable for use with select(2) to wait for pixels to be
 * ready. return file descriptor if ok, else fill errmsg[] and return -1.
 */
int 
ocaas_selectHandleCCD (char *errmsg)
{
    if (openCCD (errmsg) < 0)
        return(-1);
    return (ccd_fd);
}

/* set up the cooler according to tp.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
ocaas_setTempCCD (CCDTempInfo *tp, char *errmsg)
{
    int wasclosed = ccd_fd < 0;
    int s;

    if (openCCD (errmsg) < 0)
        return(-1);
    s = ioctl (ccd_fd, CCD_SET_TEMP, tp);
    if (s < 0)
        strcpy (errmsg, "Cooler command error");
    if (wasclosed)
        closeCCD();
    return (s);
}

/* fetch the camera temp, in C.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
ocaas_getTempCCD (CCDTempInfo *tp, char *errmsg)
{
    int wasclosed = ccd_fd < 0;
    int s;

    if (openCCD (errmsg) < 0)
        return (-1);
    s = ioctl (ccd_fd, CCD_GET_TEMP, tp);
    if (s < 0)
        strcpy (errmsg, "Error fetching camera temperature");
    if (wasclosed)
        closeCCD();
    return (s);
}

/* immediate shutter control: open or close.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
ocaas_setShutterNow (int open, char *errmsg)
{
    int wasclosed = ccd_fd < 0;
    int s;

    if (openCCD (errmsg) < 0)
        return (-1);
    s = ioctl (ccd_fd, CCD_SET_SHTR, open);
    if (s < 0)
        strcpy (errmsg, "Shutter error");
    if (wasclosed)
        closeCCD();
    return (s);
}

/* fetch the camera ID string.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
ocaas_getIDCCD (char buf[], char *errmsg)
{
    int wasclosed = ccd_fd < 0;
    int s;

    if (openCCD (errmsg) < 0)
        return (-1);
    s = ioctl (ccd_fd, CCD_GET_ID, buf);
    if (s < 0)
        strcpy (errmsg, "Can not get camera ID string");
    if (wasclosed)
        closeCCD();
    return (s);
}

/* fetch the max camera settings.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
ocaas_getSizeCCD (CCDExpoParams *cep, char *errmsg)
{
    int wasclosed = ccd_fd < 0;
    int s;

    if (openCCD (errmsg) < 0)
        return (-1);
    s = ioctl (ccd_fd, CCD_GET_SIZE, cep);
    if (s < 0)
        strcpy (errmsg, "Can not get camera size info");
    if (wasclosed)
        closeCCD();
    return (s);
}

/* read nbytes worth the pixels into mem[].
 * if `block' we wait even if none are ready now, else only wait if some are
 * ready on the first read.
 */
int
ocaas_readPix (char *mem, int nbytes, int block, char *errmsg)
{
    int nread;
    int s;

	int fflags = block ? 0 : O_NONBLOCK;

    if (ccd_fd < 0) {
        (void) strcpy (errmsg, "camera not open");
        return (-1);
    }
    if (fcntl (ccd_fd, F_SETFL, fflags) < 0) {
        (void) sprintf (errmsg, "fcntl(%d): %s",fflags,strerror(errno));
        closeCCD();
        return (-1);
    }

    /* must read all pixels in one shot from driver */
    s = 0;
    nread = read (ccd_fd, mem, nbytes);
    if (nread != nbytes) {
        if (nread < 0)
            (void) sprintf (errmsg, "%s", strerror (errno));
        else
            (void) sprintf (errmsg, "%d but expected %d",nread,nbytes);
        s = -1;
    }

    closeCCD();
    return (s);
}

/* Fill in the CCDCallbacks structure with the appropriate function
 * calls for this particular driver. */
void ocaas_getCallbacksCCD(CCDCallbacks *callbacks)
{
    callbacks->findCCD = ocaas_findCCD;
    callbacks->setExpCCD = ocaas_setExpCCD;
    callbacks->startExpCCD = ocaas_startExpCCD;
    callbacks->setupDriftScan = ocaas_setupDriftScan;
    callbacks->performRBIFlushCCD = ocaas_performRBIFlushCCD;
    callbacks->abortExpCCD = ocaas_abortExpCCD;
    callbacks->selectHandleCCD = ocaas_selectHandleCCD;
    callbacks->setTempCCD = ocaas_setTempCCD;
    callbacks->getTempCCD = ocaas_getTempCCD;
    callbacks->setShutterNow = ocaas_setShutterNow;
    callbacks->getIDCCD = ocaas_getIDCCD;
    callbacks->getSizeCCD = ocaas_getSizeCCD;
    callbacks->readPix = ocaas_readPix;
}

