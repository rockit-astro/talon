#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#include "ccdcamera.h"

#include "sbigudrv.h"

/* Temperature Conversion Constants
   Defined in the SBIG Universal Driver Documentation */
#define SBIG_T0      25.0
#define SBIG_R0       3.0
#define SBIG_DT_CCD  25.0
#define SBIG_DT_AMB  45.0
#define SBIG_RR_CCD   2.57
#define SBIG_RR_AMB   7.791
#define SBIG_RB_CCD  10.0
#define SBIG_RB_AMB   3.0
#define SBIG_MAX_AD  4096

#define	SBIGPP		500000	    /* exposure poll period, usecs */
static int sbig_pixpipe;		/* fd becomes readable when pixels ready */
static int sbig_diepipe;		/* kills support child in case parent dies */

static CCDExpoParams expsav;	/* copy of exposure params to use */

/************ Support functions for the public Talon camera API ***********/

int sbig_end_exposure()
{
    EndExposureParams eep;

    eep.ccd = CCD_IMAGING;

    int res = SBIGUnivDrvCommand(CC_END_EXPOSURE, &eep, NULL);
    if (res != CE_NO_ERROR) {
        printf("Error ending SBIG camera exposure\n");
        return 0;
    }
    return 1;
}

/* Convert from temperature in degrees C to A/D units for SBIG cameras */
unsigned short sbig_degreesC_to_AD(double degC)
{
    double r;
    unsigned short setpoint;

    if ( degC < -50.0 )
        degC = -50.0;
    else if ( degC > 35.0 )
        degC = 35.0;
    r = SBIG_R0 * exp(log(SBIG_RR_CCD)*(SBIG_T0 - degC)/SBIG_DT_CCD);
    setpoint = (unsigned short)(SBIG_MAX_AD/((SBIG_RB_CCD/r) + 1.0) + 0.5);

    return setpoint;
}

double sbig_AD_to_degreesC(unsigned short ad)
{
    double r, degC;

    if ( ad < 1 )
        ad = 1;
    else if ( ad >= SBIG_MAX_AD - 1 )
        ad = SBIG_MAX_AD - 1;
    r = SBIG_RB_CCD/(((double)SBIG_MAX_AD/ad) - 1.0);
    degC = SBIG_T0 - SBIG_DT_CCD*(log(r/SBIG_R0)/log(SBIG_RR_CCD));

    return degC;
}

/* based on fli_monitor() */
/* KMI TODO: Can we merge common functionality from fli_monitor,
             sbig_monitor, and ccdserver_monitor? */
/* create a child helper process that monitors an exposure. set sbig_pixpipe
 *   to a file descriptor from which the parent (us) will read EOF when the
 *   current exposure completes. set sbig_diepipe to a fd from which the child
 *   will read EOF if we die or intentionally wish to cancel exposure.
 * return 0 if process is underway ok, else -1.
 */
static int
sbig_monitor(err)
char *err;
{
    int pixp[2], diep[2];
    int pid;

    if (pipe(pixp) < 0 || pipe(diep)) {
        sprintf (err, "SBIG pipe: %s", strerror(errno));
        return (-1);
    }

    signal (SIGCHLD, SIG_IGN);    /* no zombies */
    pid = fork();
    if (pid < 0) {
        sprintf (err, "SBIG fork: %s", strerror(errno));
        close (pixp[0]);
        close (pixp[1]);
        close (diep[0]);
        close (diep[1]);
        return(-1);
    }

    if (pid) {
        /* parent */
        sbig_pixpipe = pixp[0];
        close (pixp[1]);
        sbig_diepipe = diep[1];
        close (diep[0]);
        return (0);
    }

    /* child .. poll for exposure or cancel if EOF on diepipe */
    close (pixp[0]);
    close (diep[1]);
    while (1) {
        struct timeval tv;
        fd_set rs;

        int res; /* SBIG command results */

        /* sbigudrv.h */
        QueryCommandStatusParams qcsp;
        QueryCommandStatusResults cmdStatus;

        FD_ZERO(&rs);
        FD_SET(diep[0], &rs);

        /* Set timeout */
        tv.tv_sec = SBIGPP/1000000;
        tv.tv_usec = SBIGPP%1000000;


        switch (select (diep[0]+1, &rs, NULL, NULL, &tv)) {
        case 0:     /* timed out.. time to poll camera */
            qcsp.command = CC_START_EXPOSURE; // Query exposure status
            res = SBIGUnivDrvCommand(CC_QUERY_COMMAND_STATUS,
                                     &qcsp, &cmdStatus);
            if (res != CE_NO_ERROR) {
                printf("Error querying exposure status. Code: %d\n", res);
                _exit(1);
            }


            if ((cmdStatus.status & 0x03) == CS_INTEGRATION_COMPLETE) {
                _exit(0);
            }

            break;
        case 1:    /* parent died or gave up */
            sbig_end_exposure();
            _exit(0);
            break;
        default:    /* local trouble */
            sbig_end_exposure();
            _exit(1);
            break;
        }
    }
}

float C_to_F(int C)
{
        return 9/5.0*C+32;
}

void sbig_print_temp_status()
{
        QueryTemperatureStatusResults qtsr;


        int error = SBIGUnivDrvCommand(CC_QUERY_TEMPERATURE_STATUS,
                                        NULL, &qtsr);

        if (error != CE_NO_ERROR) {
		printf("SBIG: Error getting temperature status");
		return;
	}

        float ccdDegreesC = sbig_AD_to_degreesC(qtsr.ccdThermistor);
        float ambDegreesC = sbig_AD_to_degreesC(qtsr.ambientThermistor);

        float ccdDegreesF = C_to_F(ccdDegreesC);
        float ambDegreesF = C_to_F(ambDegreesC);

        float setpointDegreesC = sbig_AD_to_degreesC(qtsr.ccdSetpoint);
        float setpointDegreesF = C_to_F(setpointDegreesC);

        printf("Cooler status:\n");
        printf("  Enabled: %d\n", qtsr.enabled);
        printf("  Setpoint: %d (%.2f C, %.2f F)\n", qtsr.ccdSetpoint,
                        setpointDegreesC, setpointDegreesF);
        printf("  Power: %d\n", qtsr.power);
        printf("  Current CCD reading: %d (%.2f C, %.2f F)\n",
                        qtsr.ccdThermistor, ccdDegreesC, ccdDegreesF);
        printf("  Current ambient reading: %d (%.2f C, %.2f F)\n",
                        qtsr.ambientThermistor, ambDegreesC, ambDegreesF);
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
sbig_findCCD (char *path, char *errmsg)
{
    static int sbig_use = 0;  // Is the SBIG camera currently open?
    int res; // Results from SBIGUnivDrvCommand commands

    /* Let's make sure nothing else is trying to control the driver first */
    SBIGUnivDrvCommand(CC_CLOSE_DEVICE, NULL, NULL);
    SBIGUnivDrvCommand(CC_CLOSE_DRIVER, NULL, NULL);

    if (sbig_use) {
        /* being reopened */
        if (sbig_pixpipe) {
            close (sbig_pixpipe);
            sbig_pixpipe = 0;
            close (sbig_diepipe);
            sbig_diepipe = 0;
        }
        sbig_use = 0;
    }

    /* Now open it for ourselves */
    res = SBIGUnivDrvCommand(CC_OPEN_DRIVER, NULL, NULL);
    if (res != CE_NO_ERROR) {
        // printf("Error initializing SBIG camera driver. Code %d\n", res);
        return 0;
    }

    OpenDeviceParams odp;
    odp.deviceType = DEV_USB;
    res = SBIGUnivDrvCommand(CC_OPEN_DEVICE, &odp, NULL);

    if (res != CE_NO_ERROR) {
        // printf("Error opening SBIG camera device. Code %d\n", res);
        return 0;
    }

    EstablishLinkParams elp;
    elp.sbigUseOnly = 0;  /* Reserved by SBIG, must be 0 */
    EstablishLinkResults elr;
    res = SBIGUnivDrvCommand(CC_ESTABLISH_LINK, &elp, &elr);

    if (res != CE_NO_ERROR) {
        sprintf(errmsg,"Error establishing link to CCD camera. Code: %d", 
                        res);
        return -1;
    }

    sbig_use = 1;

    return 1; /* Found camera and established link */
}

/* check and save the given exposure parameters.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int
sbig_setExpCCD (CCDExpoParams *expP, char *errmsg)
{
    /* Don't need to do anything here... just reference expsav
     * when we actually start the exposure for SBIG cameras. */

    expsav = *expP;

    return 0;
}

/* start an exposure, as previously defined via setExpCCD().
 * we return immediately but leave things open so ccd_fd or aux_fd can be used.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int
sbig_startExpCCD (char *errmsg)
{
    StartExposureParams sep;
    sep.ccd = CCD_IMAGING;
    printf("Exposing for %d units\n", expsav.duration);
    sep.exposureTime = expsav.duration/10; // convert ms to 100ths of sec
    sep.abgState = 0;  /* Anti-blooming gate */
    if (expsav.shutter == CCDSO_Closed) {
        sep.openShutter = 2;  /* Close for exposure and readout */
    }
    else {
        sep.openShutter = 1; /* Open for exposure, close for readout */
    }

    int res = SBIGUnivDrvCommand(CC_START_EXPOSURE, &sep, NULL);
    if (res != CE_NO_ERROR) {
        sprintf(errmsg, "Can not start SBIG expose");
        return -1;
    }

    if (sbig_monitor(errmsg) < 0)
        return (-1);    

    return 0;
}

/* set up for a new drift scan.
 * return 0 if ok else -1 with errmsg.
 * N.B. leave camera open of ok.
 */
int 
sbig_setupDriftScan (CCDDriftScan *dsip, char *errmsg)
{
    sprintf (errmsg, "SBIG does not support drift scan");
    return -1;
}

/* Performs RBI Annihilation process, if supported.
 * RBI Annihilation floods the CCD array with charge for
 * an "exposure" and then discards and drains.
 * This prevents exposure to bright objects from creating
 * Residual Buffer Images in subsequent frames.
 * Return 0 if ok, else set errmsg[] and return -1.
 */
int 
sbig_performRBIFlushCCD (int flushMS, int numDrains, char* errmsg)
{
    sprintf (errmsg, "SBIG does not support RBI Annihilation");
    return (-1);
}

/* abort a current exposure, if any.
 */
void
sbig_abortExpCCD()
{
    if (sbig_pixpipe > 0) {
        close (sbig_pixpipe);
        sbig_pixpipe = 0;
        close (sbig_diepipe);
        sbig_diepipe = 0;
    } else {
        sbig_end_exposure();
    }
}

/* after calling startExpCCD() or setupDriftScan(), call this to obtain a
 * handle which is suitable for use with select(2) to wait for pixels to be
 * ready. return file descriptor if ok, else fill errmsg[] and return -1.
 */
int
sbig_selectHandleCCD (char *errmsg)
{
    if (sbig_pixpipe) {
        return sbig_pixpipe;
    }
    else {
        sprintf(errmsg, "SBIG not exposing");
        return (-1);
    }
}

/* set up the cooler according to tp.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
sbig_setTempCCD (CCDTempInfo *tp, char *errmsg)
{
    int res;
    SetTemperatureRegulationParams strp;

    if (tp->s == CCDTS_SET) {
        strp.regulation = 1;
        strp.ccdSetpoint = sbig_degreesC_to_AD(tp->t);
        res = SBIGUnivDrvCommand(CC_SET_TEMPERATURE_REGULATION, 
                &strp, NULL);
        if (res != CE_NO_ERROR) {
            sprintf(errmsg, "Cannot set SBIG temp to %d", tp->t);
            return -1;
        }
    }
    else {
        strp.regulation = 0;
        strp.ccdSetpoint = 65535;
        res = SBIGUnivDrvCommand(CC_SET_TEMPERATURE_REGULATION, 
                &strp, NULL);
        if (res != CE_NO_ERROR) {
            sprintf(errmsg, "Error turning off SBIG temp regulator");
            return -1;
        }
    }
    return 0;
}

/* fetch the camera temp, in C.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
sbig_getTempCCD (CCDTempInfo *tp, char *errmsg)
{
    sbig_print_temp_status();

    QueryTemperatureStatusResults qtsr;
    int res = SBIGUnivDrvCommand(CC_QUERY_TEMPERATURE_STATUS, NULL, &qtsr);

    if (res != CE_NO_ERROR) {
        sprintf(errmsg, "Error reading SBIG CCD temperature");
        return -1;
    }

    tp->t = sbig_AD_to_degreesC(qtsr.ccdThermistor);

    if (!qtsr.enabled) {
        tp->s = CCDTS_OFF;
    }
    else {
        if (sbig_AD_to_degreesC(qtsr.ccdThermistor) < 
                sbig_AD_to_degreesC(qtsr.ccdSetpoint)) {
            tp->s = CCDTS_UNDER;
        } else if (sbig_AD_to_degreesC(qtsr.ccdThermistor) > 
                sbig_AD_to_degreesC(qtsr.ccdSetpoint)) {
            tp->s = CCDTS_RDN;
        } else {
            tp->s = CCDTS_AT;
        }
    }

    return 0;
}

/* immediate shutter control: open or close.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
sbig_setShutterNow (int open, char *errmsg)
{
    QueryCommandStatusParams qcsp;
    QueryCommandStatusResults cmdStatus;

    qcsp.command = CC_MISCELLANEOUS_CONTROL; // Query exposure status
    int res = SBIGUnivDrvCommand(CC_QUERY_COMMAND_STATUS,
            &qcsp, &cmdStatus);
    if (res != CE_NO_ERROR) {
        sprintf(errmsg, "Error querying shutter status. Code: %d\n", res);
        return -1;
    }

    MiscellaneousControlParams mcp;
    mcp.fanEnable = (cmdStatus.status & 0x100) >> 8;
    mcp.ledState = (cmdStatus.status & 0x1800) >> 11;

    if (open) {
        mcp.shutterCommand = 1;
    } else {
        mcp.shutterCommand = 2;
    }

    res = SBIGUnivDrvCommand(CC_MISCELLANEOUS_CONTROL, &mcp, NULL);

    if (res != CE_NO_ERROR) {
        sprintf(errmsg, "Error opening/closing shutter. Code: %d\n", res);
        return -1;
    }

    return 0;
}

/* fetch the camera ID string.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
sbig_getIDCCD (char buf[], char *errmsg)
{
    GetCCDInfoParams gcip;
    gcip.request = CCD_INFO_IMAGING;
    GetCCDInfoResults0 ccdInfo;
    int res = SBIGUnivDrvCommand(CC_GET_CCD_INFO, &gcip, &ccdInfo);
    if (res != CE_NO_ERROR) {
        strcpy(errmsg, "Can not get SBIG model name");
        return -1;
    }

    strcpy(buf, ccdInfo.name);
    return 0;
}

/* fetch the max camera settings.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
sbig_getSizeCCD (CCDExpoParams *cep, char *errmsg)
{
    int res;
    GetCCDInfoParams gcip;
    gcip.request = CCD_INFO_IMAGING;
    GetCCDInfoResults0 ccdInfo;
    res = SBIGUnivDrvCommand(CC_GET_CCD_INFO, &gcip, &ccdInfo);

    if (res != CE_NO_ERROR) {
        sprintf(errmsg, "Can not get SBIG camera size");
        return -1;
    }
    cep->sw = ccdInfo.readoutInfo[0].width;
    cep->sh = ccdInfo.readoutInfo[0].height;
    cep->bx = 2;    /* Actually depends on camera model, but relation */
    cep->by = 2;    /* is complex... 1x1 and 2x2 is all that's universal */
    return 0;
}

/* read nbytes worth the pixels into mem[].
 * if `block' we wait even if none are ready now, else only wait if some are
 * ready on the first read.
 */
int
sbig_readPix (char *mem, int nbytes, int block, char *errmsg)
{
    StartReadoutParams srp;
    srp.ccd = CCD_IMAGING;
    srp.left = expsav.sx/expsav.bx;
    srp.top = expsav.sy/expsav.by;
    srp.width = expsav.sw/expsav.bx;
    srp.height = expsav.sh/expsav.by;

    if (expsav.bx > 1 || expsav.by > 1) {
        srp.readoutMode = 1; // 2x2 binning
    } else {
        srp.readoutMode = 0; // No binning
    }

    if (!sbig_end_exposure()) {
        sprintf(errmsg, "Error ending previous exposure.");
        return -1;
    }

    int res = SBIGUnivDrvCommand(CC_START_READOUT, &srp, NULL);
    if (res != CE_NO_ERROR) {
        sprintf(errmsg, "Error starting SBIG readout. Code %d\n", res);
        return -1;
    }

    int bytecount = 0;
    int nextbytecount;
    int row = 0;
    for (row = 0; row < srp.height; row++) {
        ReadoutLineParams rlp;
        rlp.ccd = CCD_IMAGING;
        rlp.readoutMode = srp.readoutMode;
        rlp.pixelStart = srp.left;
        rlp.pixelLength = srp.width;

        printf("  row %d, byte %d\n", row, bytecount);
        nextbytecount = bytecount + 2*srp.width;

        if (nextbytecount > nbytes) {
            // We don't want to return too many bytes here
            break;
        }

        res = SBIGUnivDrvCommand(CC_READOUT_LINE, &rlp, mem+bytecount);

        if (res != CE_NO_ERROR) {
            sprintf(errmsg, "Error reading row %d. Code %d", row, res);
            return -1;
        }

        bytecount = nextbytecount;
    }

    EndReadoutParams erp;
    erp.ccd = CCD_IMAGING;
    res = SBIGUnivDrvCommand(CC_END_READOUT, &erp, NULL);
    if (res != CE_NO_ERROR) {
        sprintf(errmsg, "Error ending SBIG readout");
        return -1;
    }

    return 0;
}

/* Fill in the CCDCallbacks structure with the appropriate function
 * calls for this particular driver. */
void sbig_getCallbacksCCD(CCDCallbacks *callbacks)
{
    callbacks->findCCD = sbig_findCCD;
    callbacks->setExpCCD = sbig_setExpCCD;
    callbacks->startExpCCD = sbig_startExpCCD;
    callbacks->setupDriftScan = sbig_setupDriftScan;
    callbacks->performRBIFlushCCD = sbig_performRBIFlushCCD;
    callbacks->abortExpCCD = sbig_abortExpCCD;
    callbacks->selectHandleCCD = sbig_selectHandleCCD;
    callbacks->setTempCCD = sbig_setTempCCD;
    callbacks->getTempCCD = sbig_getTempCCD;
    callbacks->setShutterNow = sbig_setShutterNow;
    callbacks->getIDCCD = sbig_getIDCCD;
    callbacks->getSizeCCD = sbig_getSizeCCD;
    callbacks->readPix = sbig_readPix;
}

