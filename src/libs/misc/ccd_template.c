#include <stdio.h>

#include "ccdcamera.h"

/*
 * Template for Talon CCD camera drivers 
 *
 * Steps to implement a new camera interface:
 * 1. Replace "REPLACEME" with a short name for your camera in the functions
 *    below; e.g. "fli", "sbig", "apogee", etc.
 *
 * 2. Implement the public API functions, perhaps using an existing driver
 *    as an example.
 *
 * 3. Add your getCallbacksCCD function to the camera_drivers list in
 *    ccddrivers.h
 */

/************ Support functions for the public Talon camera API ***********/

/* TODO - add support functions here */

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
REPLACEME_findCCD (char *path, char *errmsg)
{

}

/* check and save the given exposure parameters.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
REPLACEME_setExpCCD (CCDExpoParams *expP, char *errmsg)
{
}

/* start an exposure, as previously defined via setExpCCD().
 * we return immediately but leave things open so ccd_fd or aux_fd can be used.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
REPLACEME_startExpCCD (char *errmsg)
{
}

/* set up for a new drift scan.
 * return 0 if ok else -1 with errmsg.
 * N.B. leave camera open of ok.
 */
int 
REPLACEME_setupDriftScan (CCDDriftScan *dsip, char *errmsg)
{
}

/* abort a current exposure, if any.
 */
void 
REPLACEME_abortExpCCD (void)
{
}

/* after calling startExpCCD() or setupDriftScan(), call this to obtain a
 * handle which is suitable for use with select(2) to wait for pixels to be
 * ready. return file descriptor if ok, else fill errmsg[] and return -1.
 */
int 
REPLACEME_selectHandleCCD (char *errmsg)
{
}

/* set up the cooler according to tp.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
REPLACEME_setTempCCD (CCDTempInfo *tp, char *errmsg)
{
}

/* fetch the camera temp, in C.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
REPLACEME_getTempCCD (CCDTempInfo *tp, char *errmsg)
{
}

/* immediate shutter control: open or close.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
REPLACEME_setShutterNow (int open, char *errmsg)
{
}

/* fetch the camera ID string.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
REPLACEME_getIDCCD (char buf[], char *errmsg)
{
}

/* fetch the max camera settings.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
REPLACEME_getSizeCCD (CCDExpoParams *cep, char *errmsg)
{
}

/* read nbytes worth the pixels into mem[].
 * if `block' we wait even if none are ready now, else only wait if some are
 * ready on the first read.
 */
int
REPLACEME_readPix (char *mem, int nbytes, int block, char *errmsg)
{
}

/* Fill in the CCDCallbacks structure with the appropriate function
 * calls for this particular driver. */
void REPLACEME_getCallbacksCCD(CCDCallbacks *callbacks)
{
    callbacks->findCCD = REPLACEME_findCCD;
    callbacks->setExpCCD = REPLACEME_setExpCCD;
    callbacks->startExpCCD = REPLACEME_startExpCCD;
    callbacks->setupDriftScan = REPLACEME_setupDriftScan;
    callbacks->abortExpCCD = REPLACEME_abortExpCCD;
    callbacks->selectHandleCCD = REPLACEME_selectHandleCCD;
    callbacks->setTempCCD = REPLACEME_setTempCCD;
    callbacks->getTempCCD = REPLACEME_getTempCCD;
    callbacks->setShutterNow = REPLACEME_setShutterNow;
    callbacks->getIDCCD = REPLACEME_getIDCCD;
    callbacks->getSizeCCD = REPLACEME_getSizeCCD;
    callbacks->readPix = REPLACEME_readPix;
}

