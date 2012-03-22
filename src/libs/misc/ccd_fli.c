#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ccdcamera.h"
#include "libfli.h"

/* used for FLI driver */
#define	FLIPP		500000	/* exposure poll period, usecs */
#define	FLIPP_FLASH	100000	/* exposure poll period when shutter flash is in effect, usecs */
#define	FLINF		2	/* number of flushes before an exp */
#define FLI_MIN_EXPOSURE_TIME 8 /* Anything less than this amount equates to a closed shutter, bias exposure */
#define FLI_MIN_FLASHED_EXPOSURE_TIME 1000 /* Minimum length of a shutter flashed exposure that will actually toggle shutter */

static flidev_t fli_dev;	/* one per camera */
static int fli_pixpipe;		/* fd becomes readable when pixels ready */
static int fli_diepipe;		/* kills support child in case parent dies */
static int fli_settemp;		/* target temp, C */
static int fli_tempset;		/* whether cooler is on */
static int fli_row_width, fli_num_rows; /* Determined from subimage and binning settings */

/*
 STO January 2003
 Handle shutter flashing on FLI using direct shutter control
 features of new library
*/
static int flashShutter;	// do we flash shutter on this exposure?
static int nextToggle;		// next time to toggle shutter (counting backward)
static int togglePeriod;	// period of interval between flash states
static int toggleState;		// current state of shutter


typedef struct {
  flidomain_t domain;
  char *dname;
  char *name;
} cam_t; /* Used in fli_findcams */

/************ Support functions for the public Talon camera API ***********/

// Set up for shutter flashing with FLI
static void setupFLIShutterFlash( CCDShutterOptions	type, int expms )
{
	if(	type == CCDSO_Closed || type == CCDSO_Open || expms < FLI_MIN_FLASHED_EXPOSURE_TIME) {
		flashShutter = 0;
		return;
	}
	flashShutter = 1; // yes, let's do it
	switch(type) {
		case CCDSO_Dbl:
		    /* shutter open 3/4: OOCO */
		    nextToggle = expms - expms/2;
		    togglePeriod = expms/4;
		    toggleState = 1;
			break;
			
		case CCDSO_Multi:
			/* shutter open 4/6: OOCOCO */
		    nextToggle = expms - expms/3;
		    togglePeriod = expms/6;
		    toggleState = 1;
			break;
		
		default:
			/* something's wrong! */
			flashShutter = 0;
			break;		
	}		
	
}

// Control shutter during monitoring
static void handleFLIShutterFlash(int msleft)
{
	if(!flashShutter) return; // don't bother
	if(msleft > nextToggle) return; // not time to start
	nextToggle -= togglePeriod;
	toggleState = !toggleState;
	if(!msleft) toggleState = 0; // insure closed when done
	FLIControlShutter(fli_dev, toggleState ? FLI_SHUTTER_OPEN : FLI_SHUTTER_CLOSE);		
}

/* TO CONTROL THE CORRECT HAPPY END OF THE CHILDREN PROCESSES */
void fli_signal_sigchld(int signum) {
  signal(signum,SIG_IGN);
  wait(NULL);
  signal(signum,fli_signal_sigchld);
}

/* create a child helper process that monitors an exposure. set fli_pixpipe
 *   to a file descriptor from which the parent (us) will read EOF when the
 *   current exposure completes. set fli_diepipe to a fd from which the child
 *   will read EOF if we die or intentionally wish to cancel exposure.
 * return 0 if process is underway ok, else -1.
 */
static int
fli_monitor(err)
char *err;
{
    int pixp[2], diep[2];
    int pid;

    if (pipe(pixp) < 0 || pipe(diep)) {
        sprintf (err, "FLI pipe: %s", strerror(errno));
        return (-1);
    }
    //signal (SIGCHLD, SIG_IGN); /* no zombies */
    signal(SIGCHLD,fli_signal_sigchld);
    pid = fork();
    if (pid < 0) {
        sprintf (err, "FLI fork: %s", strerror(errno));
        close (pixp[0]);
        close (pixp[1]);
        close (diep[0]);
        close (diep[1]);
        return(-1);
    }

    if (pid) {
        /* parent */
        fli_pixpipe = pixp[0];
        close (pixp[1]);
        fli_diepipe = diep[1];
        close (diep[0]);
        return (0);
    }

    /* child .. poll for exposure or cancel if EOF on diepipe */
    close (pixp[0]);
    close (diep[1]);
    while (1) {
        struct timeval tv;
        fd_set rs;
        long left;

        if(flashShutter) {
            tv.tv_sec = FLIPP_FLASH/1000000;
            tv.tv_usec = FLIPP_FLASH%1000000;
        } else {
            tv.tv_sec = FLIPP/1000000;
            tv.tv_usec = FLIPP%1000000;
        }

        FD_ZERO(&rs);
        FD_SET(diep[0], &rs);

        switch (select (diep[0]+1, &rs, NULL, NULL, &tv)) {
            case 0: 	/* timed out.. time to poll camera */
                if (FLIGetExposureStatus (fli_dev, &left) != 0)
                    _exit(1);
                // Support for shutter flashing
                handleFLIShutterFlash(left);
                if (!left)
                    _exit(0);
                break;
            case 1:	/* parent died or gave up */
                (void) FLICancelExposure(fli_dev);
                _exit(0);
                break;
            default:	/* local trouble */
                (void) FLICancelExposure(fli_dev);
                _exit(1);
                break;
        }
    }
}

/* Locate and indentify available FLI cameras 
   (modified from takepic.c in flilib distribution)
   Returns number of cameras detected
*/
static int fli_findcams(flidomain_t domain, cam_t **cam)
{
    int num_fli_cams = 0;
    char **tmplist;

    FLIList(domain | FLIDEVICE_CAMERA, &tmplist);

    if (tmplist != NULL && tmplist[0] != NULL)
    {
        int i, cams = 0;

        for (i = 0; tmplist[i] != NULL; i++)
            cams++;

        if ((*cam = realloc(*cam, (num_fli_cams + cams) * sizeof(cam_t))) == NULL)
        {
            return 0;
        }

        for (i = 0; tmplist[i] != NULL; i++)
        {
            int j;
            cam_t *tmpcam = *cam + i;

            for (j = 0; tmplist[i][j] != '\0'; j++) {
                if (tmplist[i][j] == ';')
                {
                    tmplist[i][j] = '\0';
                    break;
                }
            }

            tmpcam->domain = domain;
            switch (domain)
            {
                case FLIDOMAIN_PARALLEL_PORT:
                    tmpcam->dname = "parallel port";
                    break;

                case FLIDOMAIN_USB:
                    tmpcam->dname = "USB";
                    break;

                case FLIDOMAIN_SERIAL:
                    tmpcam->dname = "serial";
                    break;

                case FLIDOMAIN_INET:
                    tmpcam->dname = "inet";
                    break;

                default:
                    tmpcam->dname = "Unknown domain";
                    break;
            }
            tmpcam->name = strdup(tmplist[i]);
        }

        num_fli_cams += cams;
    }

    FLIFreeList(tmplist);

    return num_fli_cams;
}

/*
  RBI Annihilation code for FLI
  STO 3-17-2009
  This implements according to sample code provided by Jim Moronski of FLI
  
  The process is an entirely self-contained set of exposures using the RBI
  flood settings.  Not supported if the API does not define RBI settings.

*/
static int fli_rbiFlush(int flushMS, int numDrains, char* errmsg)
{ 

#ifdef FLI_FRAME_TYPE_RBI_FLUSH
   
    long ul_x, ul_y, lr_x, lr_y;
    fliframe_t frametype;
    long expms = flushMS;

    printf("RBI setup\n");

    if (FLIGetVisibleArea (fli_dev, &ul_x, &ul_y, &lr_x, &lr_y) != 0) {
        sprintf (errmsg, "Can not get FLI visible area");
        return (-1);
    }

    if (FLISetImageArea (fli_dev, ul_x, ul_y, lr_x, lr_y) != 0) {
        sprintf (errmsg, "Can not set FLI image area to %ld %ld %ld %ld",
                ul_x, ul_y, lr_x, lr_y);
        return (-1);
    }

    if (FLISetFrameType (fli_dev, FLI_FRAME_TYPE_RBI_FLUSH) != 0) {
        sprintf (errmsg, "Can not set FLI frame type to FLI_FRAME_TYPE_RBI_FLUSH");
        return (-1);
    }

    if(expms < FLI_MIN_EXPOSURE_TIME) expms = FLI_MIN_EXPOSURE_TIME;

    if (FLISetExposureTime (fli_dev, expms) != 0) {
        sprintf(errmsg, "Can not set FLI exposure time to %dms",flushMS);
        return (-1);
    }
    if (FLISetHBin (fli_dev, 1) != 0) {
        sprintf(errmsg, "Can not set FLI Hbinning to %d", 1);
        return (-1);
    }
    if (FLISetVBin (fli_dev, 1) != 0) {
        sprintf(errmsg, "Can not set FLI Vbinning to %d", 1);
        return (-1);
    }
    if (FLISetNFlushes (fli_dev, FLINF) != 0) {
        sprintf(errmsg, "Can not set FLI n flushes");
        return (-1);
    }

    printf("RBI exposing for %dms\n", flushMS);
    
    if (FLIExposeFrame (fli_dev) != 0) {
        sprintf (errmsg, "Error exposing frame");
        return (-1);
    }
    if (fli_monitor(errmsg) < 0) {
        sprintf(errmsg, "Error setting up fli_monitor");
        return (-1);
    }
    // wait for exposure to end, ignore reading
    if(readPixelCCD(NULL, 0, errmsg) < 0) {
        sprintf(errmsg, "Error waiting for exposure");
        return (-1);
    }
    // Now take a set of bias frames to flush
    int cnt = numDrains;
    if (FLISetExposureTime (fli_dev, FLI_MIN_EXPOSURE_TIME) != 0) {
        sprintf(errmsg, "Can not set FLI exposure time to %d for bias flush", FLI_MIN_EXPOSURE_TIME);
        return (-1);
    }

    printf("RBI flushing with %d bias frames\n",cnt);
 
    while(cnt--)
    {
        long remaining_exposure = 0;
        FLIExposeFrame(fli_dev);
        do {
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 200000;
            select (0, NULL, NULL, NULL, &tv);
            FLIGetExposureStatus(fli_dev, &remaining_exposure);
            
        } while(remaining_exposure != 0);
    }

    printf("RBI complete\n");
    return 0;

#else
    sprintf(errmsg, "RBI Support not available in this version of FLI library");
    return -1;
#endif
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
fli_findCCD (char *path, char *errmsg)
{
    static int fli_use = 0; /* Is the FLI device currently open? */
	cam_t *cam = NULL; /* Array of camera objects filled by fli_findcams */

    /* Make sure that somebody else hasn't already opened the camera... */
    if (fli_use) {
        /* being reopened */
        if (fli_pixpipe) {
            close (fli_pixpipe);
            fli_pixpipe = 0;
            close (fli_diepipe);
            fli_diepipe = 0;
        }
        FLIClose (fli_dev);
        fli_use = 0;
    }

	int ncameras=0;
	int i;
	ncameras=fli_findcams(FLIDOMAIN_USB, &cam);
	for(i=0;i<ncameras;i++) {
		printf("[ccd_fli.c] i=%d, name=%s, dname=%s\n",i,cam[i].name,cam[i].dname);
		if(strcmp(path,cam[i].name)==0) {
			/* This assumes we want to open the first available camera (cam[0]) */
			if (FLIOpen(&fli_dev, cam[i].name, FLIDEVICE_CAMERA | cam[i].domain) != 0) {
				sprintf(errmsg, "Error opening FLI camera\n");
				return -1;
			}
			fli_use = 1;
			return 1; /* Connected successfully */
		}
	}

    return 0; /* No cameras found */
}

/* check and save the given exposure parameters.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int
fli_setExpCCD (CCDExpoParams *expP, char *errmsg)
{
    long ul_x, ul_y, lr_x, lr_y;
    fliframe_t frametype;
    long expms;

    if (FLIGetVisibleArea (fli_dev, &ul_x, &ul_y, &lr_x, &lr_y) != 0) {
        sprintf (errmsg, "Can not get FLI visible area");
        return (-1);
    }

    ul_x += expP->sx;
    ul_y += expP->sy;
    lr_x = ul_x + expP->sw/expP->bx;
    lr_y = ul_y + expP->sh/expP->by;

    /* Compute row width and number of rows from binning and subimage data */
    fli_row_width = lr_x-ul_x;
    fli_num_rows = lr_y-ul_y;

    if (FLISetImageArea (fli_dev, ul_x, ul_y, lr_x, lr_y) != 0) {
        sprintf (errmsg, "Can not set FLI image area to %ld %ld %ld %ld",
                ul_x, ul_y, lr_x, lr_y);
        return (-1);
    }

    /* set shutter as desired, or insure closed if <= MIN_TIME */
    if(expP->shutter == CCDSO_Closed) {
        frametype = FLI_FRAME_TYPE_DARK;
    } else {
        frametype = FLI_FRAME_TYPE_NORMAL;
    }
    if (expP->duration <= FLI_MIN_EXPOSURE_TIME) {
        expms = FLI_MIN_EXPOSURE_TIME;
        frametype = FLI_FRAME_TYPE_DARK;
    } else
        expms = expP->duration;;

    if (FLISetFrameType (fli_dev, frametype) != 0) {
        sprintf (errmsg, "Can not set FLI frame type to %d",
                (int)frametype);
        return (-1);
    }
    if (FLISetExposureTime (fli_dev, expms) != 0) {
        sprintf(errmsg, "Can not set FLI exposure time to %ldms",expms);
        return (-1);
    }
    // Support shutter flashing (new library)
    setupFLIShutterFlash(expP->shutter, expms);

    /* Setting the bit depth is not supported in the current version
       of libfli, but we should be in 16-bit mode by default */

    /*if (FLISetBitDepth (fli_dev, FLI_MODE_16BIT) != 0) {
      sprintf (errmsg, "Can not set FLI bit depth to 16");
      return (-1);
      }*/

    if (FLISetHBin (fli_dev, expP->bx) != 0) {
        sprintf(errmsg, "Can not set FLI Hbinning to %d", expP->bx);
        return (-1);
    }
    if (FLISetVBin (fli_dev, expP->by) != 0) {
        sprintf(errmsg, "Can not set FLI Vbinning to %d", expP->by);
        return (-1);
    }
    if (FLISetNFlushes (fli_dev, FLINF) != 0) {
        sprintf(errmsg, "Can not set FLI n flushes");
        return (-1);
    }

    return (0);

}

/* start an exposure, as previously defined via setExpCCD().
 * we return immediately but leave things open so ccd_fd or aux_fd can be used.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int
fli_startExpCCD (char *errmsg)
{
    if (FLIExposeFrame (fli_dev) != 0) {
        sprintf (errmsg, "Can not start FLI expose");
        return (-1);
    }
    if (fli_monitor(errmsg) < 0)
        return (-1);    
    return 0;
}

/* set up for a new drift scan.
 * return 0 if ok else -1 with errmsg.
 * N.B. leave camera open of ok.
 */
int 
fli_setupDriftScan (CCDDriftScan *dsip, char *errmsg)
{
    sprintf (errmsg, "FLI does not support drift scan");
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
fli_performRBIFlushCCD (int flushMS, int numDrains, char* errmsg)
{
    // implemented in private code, above
    return fli_rbiFlush(flushMS, numDrains, errmsg);
}

/* abort a current exposure, if any.
 */
void
fli_abortExpCCD()
{
    if (fli_pixpipe > 0) {
        close (fli_pixpipe);
        fli_pixpipe = 0;
        close (fli_diepipe);
        fli_diepipe = 0;
    } else
        (void) FLICancelExposure(fli_dev);
}

/* after calling startExpCCD() or setupDriftScan(), call this to obtain a
 * handle which is suitable for use with select(2) to wait for pixels to be
 * ready. return file descriptor if ok, else fill errmsg[] and return -1.
 */
int
fli_selectHandleCCD (char *errmsg)
{
    if (fli_pixpipe)
        return (fli_pixpipe);
    else {
        sprintf (errmsg, "FLI not exposing");
        return (-1);
    }
}


/* set up the cooler according to tp.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
fli_setTempCCD (CCDTempInfo *tp, char *errmsg)
{
    if (tp->s == CCDTS_SET) {
        if (FLISetTemperature (fli_dev, (double)tp->t) != 0) {
            sprintf (errmsg, "Can not set FLI temp to %d", tp->t);
            return (-1);
        }
        fli_settemp = tp->t;
        fli_tempset = 1;
    } else
        fli_tempset = 0;
    /* KMI TODO - can't turn FLI CCD cooler off???? */
    return (0);
}

/* fetch the camera temp, in C.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
fli_getTempCCD (CCDTempInfo *tp, char *errmsg)
{
    double t;

    if (FLIGetTemperature (fli_dev, &t) != 0) {
        sprintf (errmsg, "Can not get FLI temp");
        return (-1);
    }
    tp->t = (int)floor(t+.5);

    if (fli_tempset) {
        tp->s = tp->t < fli_settemp ? CCDTS_UNDER :
            tp->t > fli_settemp ? CCDTS_RDN :
            CCDTS_AT;
    } else {
        tp->s = CCDTS_OFF;
    }
    return (0);

}

/* immediate shutter control: open or close.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
fli_setShutterNow (int open, char *errmsg)
{
    int rt = FLIControlShutter(fli_dev, open ? FLI_SHUTTER_OPEN : FLI_SHUTTER_CLOSE);
    if(rt) {
        sprintf (errmsg, "Shutter error");
        return (-1);
    }
    return 0;
}

/* fetch the camera ID string.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
fli_getIDCCD (char buf[], char *errmsg)
{
    long rev;
    char model[100];

    if (FLIGetFWRevision (fli_dev, &rev) != 0) {
        strcpy (errmsg, "Can not get FLI rev");
        return(-1);
    }
    if (FLIGetModel (fli_dev, model, sizeof(model)) != 0) {
        strcpy (errmsg, "Can not get FLI model");
        return(-1);
    }
    sprintf (buf, "FLI %s Rev %d.%02d", model, (int) (rev/256), (int) rev&0xff);
    return (0);
}

/* fetch the max camera settings.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
fli_getSizeCCD (CCDExpoParams *cep, char *errmsg)
{
    long ul_x, ul_y, lr_x, lr_y;

    if (FLIGetVisibleArea (fli_dev, &ul_x, &ul_y, &lr_x, &lr_y) != 0) {
        sprintf (errmsg, "Can not get FLI size");
        return (-1);
    }
    cep->sw = lr_x - ul_x;
    cep->sh = lr_y - ul_y;
    cep->bx = 10;			/* ?? */
    cep->by = 10;			/* ?? */
    return (0);
}


/* read nbytes worth the pixels into mem[].
 * if `block' we wait even if none are ready now, else only wait if some are
 * ready on the first read.
 */
int
fli_readPix (char *mem, int nbytes, int block, char *errmsg)
{
    struct timeval tv, *tvp;
    size_t bytesgrabbed = 0;	
    fd_set rs;
    long ary_l, ary_t, ary_r, ary_b;
    long vis_l, vis_t, vis_r, vis_b;
    int rt;
    int flush_rows, row_size, row; // KMI

    if (!fli_pixpipe) {
        strcpy (errmsg, "FLI not exposing");
        /* TODO cleanup */
        return (-1);
    }

    /* block or just check fli_pipe */
    FD_ZERO(&rs);
    FD_SET(fli_pixpipe, &rs);
    if (block)
        tvp = NULL;
    else {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        tvp = &tv;
    }
    switch (select (fli_pixpipe+1, &rs, NULL, NULL, tvp)) {
        case 1:	/* ready */
            break;
        case 0:	/* timed out -- must be non-blocking */
            sprintf (errmsg, "FLI is Exposing");
            return (-1);
        case -1:
        default:
            sprintf (errmsg, "FLI %s", strerror(errno));
            return (-1);
    }

    close (fli_pixpipe);
    fli_pixpipe = 0;
    close (fli_diepipe);
    fli_diepipe = 0;

    // STO/KMI: 2002-01-09 replacement for FLIReadFrame for new library
    rt = FLIGetArrayArea(fli_dev, &ary_l, &ary_t, &ary_r, &ary_b);
    if(!rt) rt = FLIGetVisibleArea(fli_dev, &vis_l, &vis_t, &vis_r, &vis_b);
    // Flush rows above image
    if(!rt) {
        flush_rows = vis_t - ary_t;
        rt = FLIFlushRow(fli_dev, flush_rows, 1);
    }
    // grab rows from visible area
    // KMI: When setting up the exposure, we determined the proper row 
    //      width and number of rows from subimage and binning info
    if(!rt) {			
        row_size = fli_row_width * 2; // 16 bit data

        for(row = 0; row < fli_num_rows; row++) {
            rt = FLIGrabRow(fli_dev, mem + row * row_size, fli_row_width);
            if(rt) break;
            bytesgrabbed += row_size;
        }
    }
    // flush rows below images
    if(!rt) {
        flush_rows = ary_b - vis_b;
        rt = FLIFlushRow(fli_dev, flush_rows, 1);
    }
    if(rt) {
        sprintf(errmsg, "FLI error reading pixels");
        return -1; // FLI error
    }

    if (nbytes != bytesgrabbed) {
        sprintf (errmsg, "FLI %d bytes short", nbytes-bytesgrabbed);
        return (-1);
    }

    return (0);
}

/* Fill in the CCDCallbacks structure with the appropriate function
 * calls for this particular driver. */
void fli_getCallbacksCCD(CCDCallbacks *callbacks)
{
    callbacks->findCCD = fli_findCCD;
    callbacks->setExpCCD = fli_setExpCCD;
    callbacks->startExpCCD = fli_startExpCCD;
    callbacks->setupDriftScan = fli_setupDriftScan;
    callbacks->performRBIFlushCCD = fli_performRBIFlushCCD;
    callbacks->abortExpCCD = fli_abortExpCCD;
    callbacks->selectHandleCCD = fli_selectHandleCCD;
    callbacks->setTempCCD = fli_setTempCCD;
    callbacks->getTempCCD = fli_getTempCCD;
    callbacks->setShutterNow = fli_setShutterNow;
    callbacks->getIDCCD = fli_getIDCCD;
    callbacks->getSizeCCD = fli_getSizeCCD;
    callbacks->readPix = fli_readPix;
}

