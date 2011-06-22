/*
 * ccd_andor.c
 *
 *  Created on: Apr 14, 2011
 *      Author: luis
 */

#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ccdcamera.h"
#include "atmcdLXd.h"	// andor include file
#define DEBUG

#ifdef DEBUG
#define andor_debug(...) {fprintf(stderr, "[%s]:", __FUNCTION__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n");}
#else
#define DEBUG
#endif

#define COOLER_OFF -999

typedef struct andor_camera_typedef
{
	at_32 handler;
	int index;
	int opened;
	int width;
	int height;
	int target_temp;
} andor_camera_t;

andor_camera_t andor_camera;

/************ Support functions for the public Talon camera API ***********/
// set shutter mode to auto for open, 2 for hold closed
typedef enum shutter_mode_t
{
	SHUTTER_AUTO = 0, SHUTTER_OPEN = 1, SHUTTER_CLOSED = 2
} shutter_mode;

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
int andor_findCCD(char *path, char *errmsg)
{
	andor_debug("andor_findCCD\n");
	fflush(stdout);

	int ret;
	if (andor_camera.opened)
	{
		ShutDown();
		andor_camera.opened = 0;
	}

	at_32 ncameras = 0;
	ret = GetAvailableCameras(&ncameras);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) no available ANDOR cameras\n", ret);
		return -1;
	}
	if (ncameras == 1)
	{
		andor_debug("Info (%d) %ld ANDOR camera available\n", ret, ncameras);
	}
	else
	{
		andor_debug("Info (%d) %ld ANDOR cameras available\n", ret, ncameras);
		if (ncameras == 0)
		{
			return -1;
		}
	}

	int camera_index = -1;
	sscanf(path, "/dev/andor%d", &camera_index);
	andor_debug("camera_index = %d", camera_index);

	if (camera_index < 0 || camera_index >= ncameras)
	{
		sprintf(errmsg, "Error (%d) ANDOR camera %d not available\n", ret,
				camera_index);
		andor_debug("Error (%d) ANDOR camera  %d not available\n", ret, camera_index);
		return -1;
	}

	at_32 tmphandler;
	ret = GetCameraHandle(camera_index, &tmphandler);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) opening ANDOR camera\n", ret);
		andor_debug("Error (%d) opening ANDOR camera\n", ret);
		return -1;
	}
	andor_debug("Info camera handler[%d] = %ld", camera_index, tmphandler);

	ret = SetCurrentCamera(tmphandler);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) setting ANDOR camera %d\n", ret, camera_index);
		andor_debug("Error (%d) setting ANDOR camera %d\n", ret, camera_index);
		return -1;
	}
	sleep(1);

	ret = Initialize("/usr/local/etc/andor");
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) opening ANDOR camera\n", ret);
		andor_debug("Error (%d) opening ANDOR camera\n", ret);
		return -1;
	}
	sleep(1);

	int tmpwidth, tmpheight;
	ret = GetDetector(&tmpwidth, &tmpheight);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) getting CCD size\n", ret);
		andor_debug("Error (%d) getting CCD size\n", ret);
		return -1;
	}


	andor_camera.handler = tmphandler;
	andor_camera.index = camera_index;
	andor_camera.opened = 1;
	andor_camera.width = tmpwidth;
	andor_camera.height = tmpheight;
	andor_camera.target_temp = COOLER_OFF;
	andor_debug("Sucess (%d) opening ANDOR camera %d\n", ret, camera_index);

	return 1;
}

/* check and save the given exposure parameters.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int andor_setExpCCD(CCDExpoParams *expP, char *errmsg)
{
	andor_debug("andor_setExpCCD\n");
	int ret;
	int mode;

	ret = SetReadMode(4);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) setting read mode\n", ret);
		return -1;
	}


	int Vspeeds;
	int Hspeeds;
	int numChannels;
	int noGains;

	//Settin Vertical Shift Speeds
	GetNumberVSSpeeds(&Vspeeds);
//	SetVSSpeed(1);


//	//Seting Horizontal Shift Speeds
	GetNumberADChannels(&numChannels);
//	SetADChannel(1);

	GetNumberHorizontalSpeeds(&Hspeeds);
//	GetNumberHSSpeeds(1, 1, &Hspeeds);
//	SetHSSpeed(1, 0);


	GetNumberPreAmpGains(&noGains);
//	SetPreAmpGain(2);



	printf("Vspeeds = %d Hspeeds = %d numChannels = %d noGains = %d\n", Vspeeds, Hspeeds, numChannels, noGains);


//	SetVSSpeed(1);
//	SetADChannel(1);
	ret = SetHSSpeed(0, 2);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) setting SetHSSpeed\n", ret);
		return -1;
	}

	ret = SetPreAmpGain(1);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) setting SetPreAmpGain\n", ret);
		return -1;
	}






	ret = SetAcquisitionMode(1);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) setting acquisition mode\n", ret);
		return -1;
	}

	float time = ((float) expP->duration) / 1000.0;
	ret = SetExposureTime(time);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) setting exposure time\n", ret);
		return -1;
	}

	if (expP->shutter == CCDSO_Closed)
		mode = SHUTTER_CLOSED;
	else
		mode = SHUTTER_AUTO;
	ret = SetShutter(1, mode, 50, 50);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) setting shutter mode\n", ret);
		return -1;
	}

	andor_debug("expP->bx = %d", expP->bx);
	andor_debug("expP->by = %d", expP->by);
	andor_debug("expP->sx = %d", expP->sx);
	andor_debug("expP->sy = %d", expP->sy);
	andor_debug("expP->sw = %d", expP->sw);
	andor_debug("expP->sh = %d", expP->sh);

	int hstart = expP->sx + 1;
	int vstart = expP->sy + 1;
	int hend = expP->sx + expP->sw;
	int vend = expP->sy + expP->sh;

	andor_debug("hstart = %d", hstart);
	andor_debug("vstart = %d", vstart);
	andor_debug("hend   = %d", hend);
	andor_debug("vend   = %d", vend);

	ret = SetImage(expP->bx, expP->by, hstart, hend, vstart, vend);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) setting image parameters\n", ret);
		return -1;
	}

	return 0;
}

/* start an exposure, as previously defined via setExpCCD().
 * we return immediately but leave things open so ccd_fd or aux_fd can be used.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int andor_startExpCCD(char *errmsg)
{
	andor_debug("andor_startExpCCD\n");
	int ret;

	ret = StartAcquisition();
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) starting exposure\n", ret);
		return -1;
	}

	return 0;
}

/* set up for a new drift scan.
 * return 0 if ok else -1 with errmsg.
 * N.B. leave camera open of ok.
 */
int andor_setupDriftScan(CCDDriftScan *dsip, char *errmsg)
{
	andor_debug("andor_setupDriftScan\n");

	return -1;
}

/* Performs RBI Annihilation process, if supported.
 * RBI Annihilation floods the CCD array with charge for
 * an "exposure" and then discards and drains.
 * This prevents exposure to bright objects from creating
 * Residual Buffer Images in subsequent frames.
 * Return 0 if ok, else set errmsg[] and return -1.
 */
int andor_performRBIFlushCCD(int flushMS, int numDrains, char* errmsg)
{
	andor_debug("andor_performRBIFlushCCD\n");

	return 0;
}

/* abort a current exposure, if any.
 */
void andor_abortExpCCD()
{
	andor_debug("andor_abortExpCCD\n");

	AbortAcquisition();

	return;
}

/* after calling startExpCCD() or setupDriftScan(), call this to obtain a
 * handle which is suitable for use with select(2) to wait for pixels to be
 * ready. return file descriptor if ok, else fill errmsg[] and return -1.
 */
int andor_selectHandleCCD(char *errmsg)
{
	andor_debug("andor_selectHandleCCD %d\n", andor_camera.handler);
	return 1;
}

/* set up the cooler according to tp.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int andor_setTempCCD(CCDTempInfo *tp, char *errmsg)
{
	andor_debug("andor_setTempCCD\n");
	int ret;

	if (tp->s == CCDTS_OFF)
	{
		ret = CoolerOFF();
		if (ret != DRV_SUCCESS)
		{
			tp->s = CCDTS_ERR;
			sprintf(errmsg, "Error (%d) setting cooler OFF\n", ret);
			return -1;
		}
		return 0;
	}

	ret = CoolerON();
	if (ret != DRV_SUCCESS)
	{
		tp->s = CCDTS_ERR;
		sprintf(errmsg, "Error (%d) setting cooler ON\n", ret);
		return -1;
	}

	ret = SetTemperature(tp->t);
	if (ret == DRV_TEMPERATURE_NOT_SUPPORTED)
	{
		tp->s = CCDTS_ERR;
		sprintf(errmsg, "Info (%d) CCD temperature not supported\n", ret);
		return -1;
	}
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) setting temperature\n", ret);
		return -1;
	}

	andor_camera.target_temp = tp->t;

	return 0;
}

/* fetch the camera temp, in C.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int andor_getTempCCD(CCDTempInfo *tp, char *errmsg)
{
	andor_debug("andor_getTempCCD\n");
	int ret;

	ret = GetTemperature(&tp->t);
	if (ret == DRV_TEMPERATURE_NOT_SUPPORTED)
	{
		tp->s = CCDTS_ERR;
		sprintf(errmsg, "Info (%d) CCD temperature not supported\n", ret);
		return -1;
	}
	else if (ret == DRV_TEMPERATURE_OUT_RANGE)
	{
		tp->s = CCDTS_ERR;
		sprintf(errmsg, "Info (%d) CCD temperature out of range\n", ret);
		return -1;
	}

	int coolerOn;
	ret = IsCoolerOn(&coolerOn);
	if (ret != DRV_SUCCESS)
	{
		tp->s = CCDTS_ERR;
		sprintf(errmsg, "Error (%d) getting cooler status\n", ret);
		return -1;
	}

	if (coolerOn)
	{
		if (tp->t < andor_camera.target_temp)
			tp->s = CCDTS_UNDER;
		else if (tp->t > andor_camera.target_temp)
			tp->s = CCDTS_RDN;
		else
			tp->s = CCDTS_AT;
	}
	else
	{
		tp->s = CCDTS_OFF;
	}

	return 0;
}

/* immediate shutter control: open or close.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int andor_setShutterNow(int open, char *errmsg)
{
	andor_debug("andor_setShutterNow\n");
	int ret;
	int mode;

	if (open)
		mode = SHUTTER_OPEN;
	else
		mode = SHUTTER_CLOSED;
	ret = SetShutter(1, mode, 50, 50);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) setting shutter mode\n", ret);
		return -1;
	}

	return 0;
}

/* fetch the camera ID string.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int andor_getIDCCD(char buf[], char *errmsg)
{
	andor_debug("andor_getIDCCD\n");
	int ret;
	char model[128];
	int sn;

	ret = GetHeadModel(model);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) getting camera ID (Model)\n", ret);
		return -1;
	}

	ret = GetCameraSerialNumber(&sn);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) getting camera ID (Serial Number)\n", ret);
		return -1;
	}

	sprintf(buf, "%s-%d", model, sn);
	return 0;
}

/* fetch the max camera settings.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int andor_getSizeCCD(CCDExpoParams *cep, char *errmsg)
{
	andor_debug("andor_getSizeCCD\n");

	int ret;

	ret = GetDetector(&cep->sw, &cep->sh);
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) getting CCD size\n", ret);
		return -1;
	}

	return 0;
}

/* read nbytes worth the pixels into mem[].
 * if `block' we wait even if none are ready now, else only wait if some are
 * ready on the first read.
 */
int andor_readPix(char *mem, int nbytes, int block, char *errmsg)
{
	andor_debug("andor_readPix (%d bytes)\n", nbytes);
	int ret;

	if (block)
	{
		ret = WaitForAcquisition();
		if (ret == DRV_NO_NEW_DATA)
		{
			sprintf(errmsg, "Error (%d) no data available\n", ret);
			return -1;
		}
		if (ret != DRV_SUCCESS)
		{
			sprintf(errmsg, "Error (%d) reading data\n", ret);
			return -1;
		}

		andor_debug("WaitForAcquisition finished\n");
	}

	ret = GetAcquiredData16((unsigned short*) mem,
			andor_camera.width * andor_camera.height);
	if (ret == DRV_ACQUIRING)
	{
		sprintf(errmsg, "Info (%d) camera is exposing\n", ret);
		return -1;
	}
	if (ret != DRV_SUCCESS)
	{
		sprintf(errmsg, "Error (%d) reading data\n", ret);
		return -1;
	}

	return 0;
}

/* Fill in the CCDCallbacks structure with the appropriate function
 * calls for this particular driver. */
void andor_getCallbacksCCD(CCDCallbacks *callbacks)
{
	andor_debug("andor_getCallbacksCCD\n");

	callbacks->findCCD = andor_findCCD;
	callbacks->setExpCCD = andor_setExpCCD;
	callbacks->startExpCCD = andor_startExpCCD;
	callbacks->setupDriftScan = andor_setupDriftScan;
	callbacks->performRBIFlushCCD = andor_performRBIFlushCCD;
	callbacks->abortExpCCD = andor_abortExpCCD;
	callbacks->selectHandleCCD = andor_selectHandleCCD;
	callbacks->setTempCCD = andor_setTempCCD;
	callbacks->getTempCCD = andor_getTempCCD;
	callbacks->setShutterNow = andor_setShutterNow;
	callbacks->getIDCCD = andor_getIDCCD;
	callbacks->getSizeCCD = andor_getSizeCCD;
	callbacks->readPix = andor_readPix;
}
