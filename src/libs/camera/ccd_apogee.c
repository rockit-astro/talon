#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "ccdcamera.h"
#include "alta_c_wrapper.h"

#define COOLER_OFF -999

CCDExpoParams expsav; /* Saved exposure paramters for call to startExpCCD */
static int target_temp = COOLER_OFF; /* Saved target temperature for cooler. */

static int pixpipe = 0;
static int diepipe = 0;

/* TO CONTROL THE CORRECT HAPPY END OF THE CHILDREN PROCESSES */
void apogee_signal_sigchld(int signum) {
  signal(signum,SIG_IGN);
  wait(NULL);
  signal(signum,apogee_signal_sigchld);
}

/************ Support functions for the public Talon camera API ***********/
static int
camera_monitor(err)
char *err;
{
    int pixp[2], diep[2];
    int pid;

    if (pipe(pixp) < 0 || pipe(diep)) {
        sprintf (err, "Camera monitor pipe: %s", strerror(errno));
        return (-1);
    }

    //signal (SIGCHLD, SIG_IGN); /* no zombies */
    signal(SIGCHLD,apogee_signal_sigchld);
    pid = fork();
    if (pid < 0) {
        sprintf (err, "Camera monitor fork: %s", strerror(errno));
        close (pixp[0]);
        close (pixp[1]);
        close (diep[0]);
        close (diep[1]);
        return(-1);
    }

    if (pid) {
        /* parent */
        pixpipe = pixp[0];
        close (pixp[1]);
        diepipe = diep[1];
        close (diep[0]);
        return (0);
    }

    /* child .. poll for exposure or cancel if EOF on diepipe */
    close (pixp[0]);
    close (diep[1]);
    while (1) {
        struct timeval tv;
        fd_set rs;

        /*
        if(flashShutter) {
            tv.tv_sec = FLIPP_FLASH/1000000;
            tv.tv_usec = FLIPP_FLASH%1000000;
        } else {
            tv.tv_sec = FLIPP/1000000;
            tv.tv_usec = FLIPP%1000000;
        }
        */
        tv.tv_sec = 0;
        tv.tv_usec = 500000; /* Half second polling */

        FD_ZERO(&rs);
        FD_SET(diep[0], &rs);

        switch (select (diep[0]+1, &rs, NULL, NULL, &tv)) {
            case 0: 	/* timed out.. time to poll camera */
                // Support for shutter flashing
                // handleFLIShutterFlash(left);

                if (alta_is_exposure_ready()) {
                    _exit(0);
                }

                break;
            case 1:	/* parent died or gave up */
                alta_abort_exposure();
                _exit(0);
                break;
            default:	/* local trouble */
                alta_abort_exposure();
                _exit(1);
                break;
        }
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
apogee_findCCD (char *path, char *errmsg)
{
    if (! alta_open_camera(errmsg)) {
        return 0;
    }

    return 1;
}

/* check and save the given exposure parameters.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
apogee_setExpCCD (CCDExpoParams *expP, char *errmsg)
{
    int status;

    expsav = *expP; /* Save exposure time and shutter options for later */

    status = alta_set_frame(expP->sx, 
                            expP->sy, 
                            expP->sw/expP->bx, 
                            expP->sh/expP->by,
                            expP->bx, expP->by);

    if (status == 1) {
        return 0;
    }
    sprintf(errmsg, "Invalid subframe parameters");
    return -1;
}

/* start an exposure, as previously defined via setExpCCD().
 * we return immediately but leave things open so ccd_fd or aux_fd can be used.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
apogee_startExpCCD (char *errmsg)
{
    int shutter_open = (expsav.shutter == CCDSO_Open);

    if (! alta_expose(expsav.duration/1000.0, shutter_open)) {
        sprintf(errmsg, "Exposure error");
        return -1;
    }

    if (camera_monitor(errmsg) < 0) {
        return -1;
    }

    return 0;
}

/* set up for a new drift scan.
 * return 0 if ok else -1 with errmsg.
 * N.B. leave camera open of ok.
 */
int 
apogee_setupDriftScan (CCDDriftScan *dsip, char *errmsg)
{
    sprintf (errmsg, "Apogee does not support drift scan");
    return (-1);
}

/* Performs RBI Annihilation process, if supported.
 * RBI Annihilation floods the CCD array with charge for
 * an "exposure" and then discards and drains.
 * This prevents exposure to bright objects from creating
 * Residual Buffer Images in subsequent frames.
 * Return 0 if ok, else set errmsg[] and return -1.
 */
int 
apogee_performRBIFlushCCD (int flushMS, int numDrains, char* errmsg)
{
    sprintf (errmsg, "Apogee does not support RBI Annihilation");
    return (-1);
}

/* abort a current exposure, if any.
 */
void 
apogee_abortExpCCD (void)
{
    alta_abort_exposure();
}

/* after calling startExpCCD() or setupDriftScan(), call this to obtain a
 * handle which is suitable for use with select(2) to wait for pixels to be
 * ready. return file descriptor if ok, else fill errmsg[] and return -1.
 */
int 
apogee_selectHandleCCD (char *errmsg)
{
    if (pixpipe)
        return (pixpipe);
    else {
        sprintf (errmsg, "Camera not exposing");
        return (-1);
    }
}

/* set up the cooler according to tp.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
apogee_setTempCCD (CCDTempInfo *tp, char *errmsg)
{
    if (tp->s == CCDTS_SET) {
        /* Turn cooler on */
        alta_set_cooler(1, tp->t);
        target_temp = tp->t;
    }
    else {
        /* Turn cooler off */
        alta_set_cooler(0, 0);
        target_temp = COOLER_OFF;
    }

    return 0;
}

/* fetch the camera temp, in C.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
apogee_getTempCCD (CCDTempInfo *tp, char *errmsg)
{
    tp->t = (int)alta_get_cooler_temp();

    if (target_temp != COOLER_OFF) {
        tp->s = tp->t < target_temp ? CCDTS_UNDER :
            tp->t > target_temp ? CCDTS_RDN :
            CCDTS_AT;
    } else {
        tp->s = CCDTS_OFF;
    }

    return 0;
}

/* immediate shutter control: open or close.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
apogee_setShutterNow (int open, char *errmsg)
{
    alta_open_shutter(open);
    return 0;
}

/* fetch the camera ID string.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
apogee_getIDCCD (char buf[], char *errmsg)
{
    char sensor_name[1024];
    alta_get_name(sensor_name);

    sprintf(buf, "Apogee %s", sensor_name);

    return 0;
}

/* fetch the max camera settings.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
apogee_getSizeCCD (CCDExpoParams *cep, char *errmsg)
{
    unsigned short width, height, x_binning, y_binning;

    alta_frame_size(&width, &height, &x_binning, &y_binning);

    cep->sw = width;
    cep->sh = height;
    cep->bx = x_binning;
    cep->by = y_binning;

    return 0;
}

/* read nbytes worth the pixels into mem[].
 * if `block' we wait even if none are ready now, else only wait if some are
 * ready on the first read.
 */
int
apogee_readPix (char *mem, int nbytes, int block, char *errmsg)
{
    if (!block && alta_is_exposure_ready()) {
        sprintf(errmsg, "Camera is exposing");
        return -1;
    }

    alta_read_pixels((unsigned short *)mem, nbytes);

    return 0;
}

/* Fill in the CCDCallbacks structure with the appropriate function
 * calls for this particular driver. */
void apogee_getCallbacksCCD(CCDCallbacks *callbacks)
{
    callbacks->findCCD = apogee_findCCD;
    callbacks->setExpCCD = apogee_setExpCCD;
    callbacks->startExpCCD = apogee_startExpCCD;
    callbacks->setupDriftScan = apogee_setupDriftScan;
    callbacks->performRBIFlushCCD = apogee_performRBIFlushCCD;
    callbacks->abortExpCCD = apogee_abortExpCCD;
    callbacks->selectHandleCCD = apogee_selectHandleCCD;
    callbacks->setTempCCD = apogee_setTempCCD;
    callbacks->getTempCCD = apogee_getTempCCD;
    callbacks->setShutterNow = apogee_setShutterNow;
    callbacks->getIDCCD = apogee_getIDCCD;
    callbacks->getSizeCCD = apogee_getSizeCCD;
    callbacks->readPix = apogee_readPix;
}

