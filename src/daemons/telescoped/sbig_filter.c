
/*
    SBIG Filter wheel support functions for filter.c

    Note: This file is brought into filter.c via #include
    Not meant to be added directly to project or compiled directly.
    See the use of flags controlling use of this code in the Makefile
*/

#include "sbigudrv.h"
static int sbig_is_connected = 0;


/* KMI 2/2/06 - SBIG filter functions; */

static void sbig_error_str(short code, char *errorStr)
{
    GetErrorStringParams gesp;
    gesp.errorNo = code;

    GetErrorStringResults result;

    SBIGUnivDrvCommand(CC_GET_ERROR_STRING, &gesp, &result);

    strcpy(errorStr, result.errorString);
}

static int sbig_error(int code, char *msg)
{
    char errorMsg[1024];

    if (code != CE_NO_ERROR) {
        fifoWrite(Filter_Id, 0, "SBIG filter error: %s\n", msg);
        sbig_error_str(code, errorMsg);
        fifoWrite(Filter_Id, 0, "Error description: %s\n", errorMsg);
        return 1;
    }

    return 0;
}

/*
    "ExtFilt" functions.  These are have similar interfaces between different devices.

        Each returns true or false (non-zero or zero)
        On error, these functions call error functions that use fifoWrite directly to
        output status and errors to the display and logs.
        Regardless of output, Talon will output a final error message upon error of
        any of these functions (i.e a FALSE return value)
*/

/* Connect to SBIG driver and camera; return 0 if error, 1 if success */
int sbig_reset()
{
    int error;

    error = SBIGUnivDrvCommand(CC_OPEN_DRIVER, NULL, NULL);
    if (sbig_error(error, "Error opening SBIG driver")) return 0;
    fifoWrite(Filter_Id, 0, "SBIG Driver opened...\n");

    OpenDeviceParams odp;
    odp.deviceType = DEV_USB1;
    error = SBIGUnivDrvCommand(CC_OPEN_DEVICE, &odp, NULL);
    if (sbig_error(error, "Error opening SBIG device")) return 0;
    fifoWrite(Filter_Id, 0, "SBIG Device opened\n");

    EstablishLinkParams elp;
    EstablishLinkResults elr;
    error = SBIGUnivDrvCommand(CC_ESTABLISH_LINK, &elp, &elr);
    if (sbig_error(error, "Error establishing link to SBIG cam")) return 0;
    fifoWrite(Filter_Id, 0, "Link established to SBIG camera\n");

    /* We appear to be connected; return success */
    sbig_is_connected = 1;
    return 1;
}

/* Close an SBIG connection; return 0 if error, 1 if success */
int sbig_shutdown()
{
    int error;

    error = SBIGUnivDrvCommand(CC_CLOSE_DEVICE, NULL, NULL);
    if (sbig_error(error, "Error closing SBIG device")) return 0;
    fifoWrite(Filter_Id, 0, "SBIG Device closed...\n");

    error = SBIGUnivDrvCommand(CC_CLOSE_DRIVER, NULL, NULL);
    if (sbig_error(error, "Error closing SBIG driver")) return 0;
    fifoWrite(Filter_Id, 0, "SBIG Driver closed...\n");

    return 1;
}

/* Re-align filter wheel */
int sbig_home()
{
    CFWParams cfwparams;

    cfwparams.cfwModel = CFWSEL_AUTO;
    cfwparams.cfwCommand = CFWC_INIT;

    CFWResults results;

    int error = SBIGUnivDrvCommand(CC_CFW, &cfwparams, &results);

    if (sbig_error(error, "Error homing filter wheel")) return 0;

    cfwparams.cfwCommand = CFWC_QUERY;
    results.cfwStatus = CFWS_UNKNOWN;

    int triesLeft = 5;
    while (results.cfwStatus != CFWS_IDLE && triesLeft > 0) {
        triesLeft--;
        int error = SBIGUnivDrvCommand(CC_CFW, &cfwparams, &results);
        if (sbig_error(error, "Error homing filter wheel")) return 0;
        usleep(500000);
    }
    if (triesLeft == 0) {
        printf("Timed out waiting for filter wheel to home\n");
        return 0;
    }

    printf("SBIG filter wheel homed\n");

    return 1;
}

/* Select a filter by number (0 based, for Talon... SBIG adjustment for 1-based is in function) */
int sbig_select(int position)
{
    CFWParams cfwparams;

    cfwparams.cfwModel = CFWSEL_AUTO;
    //cfwparams.cfwCommand = CFWC_OPEN_DEVICE;
    cfwparams.cfwCommand = CFWC_GOTO;
    // NOTE: Typo in sbigudrv.h calls cfwParam1 'cwfParam1'
    cfwparams.cwfParam1 = position + 1;  // sbig is 1-based

    CFWResults results;

    int error = SBIGUnivDrvCommand(CC_CFW, &cfwparams, &results);

    if (error == CE_BAD_PARAMETER) {
        printf("Error: invalid filter position %d for "
               "SBIG filter wheel\n", position);
        return 0;
    }

    if (sbig_error(error, "Error controlling filter wheel")) return 0;

    printf("Filter %d selected\n", position);

    return 1;
}


/* Connect this implementation via our function pointers */

void set_for_sbig()
{
    extFilt_reset_func = sbig_reset;
    extFilt_shutdown_func = sbig_shutdown;
    extFilt_home_func = sbig_home;
    extFilt_select_func = sbig_select;
}


/* End SBIG filter functions */


