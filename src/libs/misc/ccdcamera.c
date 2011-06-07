/* follows are generic camera control wrapper functions.
 * they work with the drivers for varios cameras as defined in ccd_*.c:
 *   an auxcam user program.
 *   a remote (or local) camera controlled by a TCP/IP daemon
 *   a camera from Finger Lakes Instruments (FLI)
 *   a camera from SBIG
 *   an Apogee Alta camera
 *   an OCAAS-compliant camera driver
 *
 * Only one may be used at a time. The choice is determined in setPathCCD(),
 * which calls initCCD(), which in turn calls the xxx_findCCD() function
 * in each driver until a camera is detected or all fail.
 *
 * use this to log activities to stderr
#define	CMDTRACE
*/

#include <stdio.h>
#include <string.h>

#include "ccdcamera.h"
#include "ccddrivers.h"

/* A collection of function pointers to the appropriate camera driver,
 * as detected by initCCD(). If NULL, then there is no detected camera. */
CCDCallbacks *callbacks = NULL;

int TIME_SYNC_DELAY; /* Used by auxcam for time synchronized delay */

int initCCD (char *path, int auxcam, char *msg);


/* Search for a camera that can be used by Talon.
 *
 * This is basically a wrapper around initCCD function which makes
 * sure we only attempt camera initialization once. If setPathCCD
 * is called multiple times, just return the results of the
 * first call.
 *
 * Note that with the advent of USB cameras, this function is now somewhat
 * confused. With such devices you generally do not need to give a path
 * to a valid device; the USB subsystem or camera driver can figure it 
 * out automatically.
 *
 * path: String pointing to the device file used to communicate with
 *       the camera, or the script used by auxcam, or a host:port
 *       address for a networked CCDServer. For USB cameras,
 *       this can usually be any string.
 *
 * auxcam: If 1, treat path as an auxcam script
 *
 * Returns 0 if camera is detected; otherwise -1
 * and fill *msg with the error string
 */
int setPathCCD (char *path, int auxcam, char *msg)
{
	static char saved_msg[1024];
	static int saved_retval = 0;
	static int first_time = 1;

	if (first_time) {
		saved_retval = initCCD(path, auxcam, saved_msg);
		first_time = 0;
	}

    /* If we are called multiple times, just return the same results
     * as the first call */
	strncpy(msg, saved_msg, 1023);
	return saved_retval;
}


/* set a path to a driver or auxcam program.
 * N.B. path[] memory must be persistent.
 * return 0 if tests ok, else -1.
 * called at most once by setPathCCD
 */
int
initCCD (char *path, int auxcam, char *msg)
{
    char fixpath[1024]; /* Temporary space for modifying the path, if needed */
    int i;

	/* default value if an empty path is passed */
	if (path && !path[0])
	    strcpy (path, "dev/ccdcamera");

    /* If configured to look for an auxcam, prepend a special tag to the
     * path so the auxcam_findCCD function knows to act */
    if (auxcam) {
        sprintf(fixpath, "auxcam:%s", path);
    }
    else {
        strcpy(fixpath, path);
    }

    /* Search for each type of camera. If 1 is returned, we have a camera
     * and can return happily. If -1 is returned, there was an error opening
     * a detected camera and we return -1 and an error message. Otherwise,
     * the camera was simply not detected and we continue to the next one.
     */

    CCDCallbacks cb;

    int num_drivers = sizeof(camera_drivers)/sizeof(camera_drivers[0]);
    printf("%d possible camera drivers...\n", num_drivers);

    for (i = 0; i < num_drivers; i++) {
        printf("Trying driver %d of %d...\n", i+1, num_drivers);

        /* Get callback functions for a particular camera driver*/
        (camera_drivers[i])(&cb);

        /* Try to detect physical camera supported by that driver */
        if (cb.findCCD(path, msg) == 1) {
            callbacks = (CCDCallbacks *)malloc(sizeof(cb));
            memcpy(callbacks, &cb, sizeof(cb));
            printf("Camera detected!\n");
            return 0;
        }
    }

    printf("No cameras detected\n");

    return -1;
}



/***** Stub functions that call the hooks for the appropriate cam driver ****/

/* check and save the given exposure parameters.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int
setExpCCD (CCDExpoParams *expP, char *errmsg)
{
    if (callbacks != NULL) {
        return callbacks->setExpCCD(expP, errmsg);
    }

    sprintf(errmsg, "No active camera");
    return -1;
}

/* start an exposure, as previously defined via setExpCCD().
 * we return immediately but leave things open so ccd_fd or aux_fd can be used.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int
startExpCCD (char *errmsg)
{
    if (callbacks != NULL) {
        return callbacks->startExpCCD(errmsg);
    }

    sprintf(errmsg, "No active camera");
    return -1;
}

/* set up for a new drift scan.
 * return 0 if ok else -1 with errmsg.
 * N.B. leave camera open of ok.
 */
int
setupDriftScan (CCDDriftScan *dsip, char *errmsg)
{
    if (callbacks != NULL) {
        return callbacks->setupDriftScan(dsip, errmsg);
    }

    sprintf(errmsg, "No active camera");
    return -1;
}

/* abort a current exposure, if any.
 */
void
abortExpCCD()
{
    if (callbacks != NULL) {
        return callbacks->abortExpCCD();
    }
}

/* Performs RBI Annihilation process, if supported.
 * RBI Annihilation floods the CCD array with charge for
 * an "exposure" and then discards and drains.
 * This prevents exposure to bright objects from creating
 * Residual Buffer Images in subsequent frames.
 * Return 0 if ok, else set errmsg[] and return -1.
 */
int
performRBIFlushCCD (int flushMS, int numDrains, char *errmsg)
{
    if (callbacks != NULL) {
        return callbacks->performRBIFlushCCD(flushMS, numDrains, errmsg);
    }

    sprintf(errmsg, "No active camera");
    return -1;
}

/* after calling startExpCCD() or setupDriftScan(), call this to obtain a
 * handle which is suitable for use with select(2) to wait for pixels to be
 * ready. return file descriptor if ok, else fill errmsg[] and return -1.
 */
int
selectHandleCCD (char *errmsg)
{
    if (callbacks != NULL) {
        return callbacks->selectHandleCCD(errmsg);
    }

    sprintf(errmsg, "No active camera");
    return -1;
}

/* set up the cooler according to tp.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int
setTempCCD (CCDTempInfo *tp, char *errmsg)
{
    if (callbacks != NULL) {
        return callbacks->setTempCCD(tp, errmsg);
    }

    sprintf(errmsg, "No active camera");
    return -1;
}

/* fetch the camera temp, in C.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int
getTempCCD (CCDTempInfo *tp, char *errmsg)
{
    if (callbacks != NULL) {
        return callbacks->getTempCCD(tp, errmsg);
    }

    sprintf(errmsg, "No active camera");
    return -1;
}

/* immediate shutter control: open or close.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int
setShutterNow (int open, char *errmsg)
{
    if (callbacks != NULL) {
        return callbacks->setShutterNow(open, errmsg);
    }

    sprintf(errmsg, "No active camera");
    return -1;
}

/* fetch the camera ID string.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int
getIDCCD (char buf[], char *errmsg)
{
    if (callbacks != NULL) {
        return callbacks->getIDCCD(buf, errmsg);
    }

    sprintf(errmsg, "No active camera");
    return -1;
}

/* fetch the max camera settings.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int
getSizeCCD (CCDExpoParams *cep, char *errmsg)
{	
    if (callbacks != NULL) {
        return callbacks->getSizeCCD(cep, errmsg);
    }

    sprintf(errmsg, "No active camera");
    return -1;
}

/* read nbytes worth the pixels into mem[].
 * if `block' we wait even if none are ready now, else only wait if some are
 * ready on the first read.
 */
static int
readPix (char *mem, int nbytes, int block, char *errmsg)
{
    if (callbacks != NULL) {
        return callbacks->readPix(mem, nbytes, block, errmsg);
    }

    sprintf(errmsg, "No active camera");
    return -1;
}

/********** Simplified public wrapper functions around readPix ************/

/* after calling startExpCCD() call this to read the pixels and close camera.
 * if the pixels are not yet ready, return will be -1 and errno EAGAIN.
 * return 0 if ok, else set errmsg[] and return -1.
 * N.B. must supply enough mem for entire read.
 */
int
readPixelNBCCD (char *mem, int nbytes, char *errmsg)
{
	return (readPix (mem, nbytes, 0, errmsg));
}

/* after calling startExpCCD() call this to read the pixels and close camera.
 * if the pixels are not yet ready, we block until they are.
 * return 0 if ok, else set errmsg[] and return -1.
 * N.B. must supply enough mem for entire read.
 */
int
readPixelCCD (char *mem, int nbytes, char *errmsg)
{
	return (readPix (mem, nbytes, 1, errmsg));
}

