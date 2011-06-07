#ifndef _CCDCAM_H
#define _CCDCAM_H

/* Generic CCD camera interface */

/* ioctl commands */
#define CPREFIX          (('c' << 16) | ('A' << 8))

#define CCD_TRACE	(CPREFIX|1)	/* arg = 1 driver trace on, 0 off */

#define CCD_SET_EXPO	(CPREFIX|10)	/* arg = &CCDExpoParams */
#define CCD_SET_TEMP	(CPREFIX|11)	/* arg = &CCDTempInfo */
#define CCD_SET_SHTR	(CPREFIX|12)	/* arg = 1: force open 0: force close */
#define	CCD_DRIFT_SCAN	(CPREFIX|15)	/* arg = &CCDDriftScan */

#define CCD_GET_TEMP	(CPREFIX|20)	/* arg = &CCDTempInfo */
#define CCD_GET_SIZE	(CPREFIX|21)	/* arg = &CCDExpoParams, sw/sh/bx/by */
#define CCD_GET_ID	(CPREFIX|22)	/* arg = buffer to fill w/id string */
#define	CCD_GET_DUMP	(CPREFIX|255)	/* get reg dump -- camera specific */

/* temperature info.
 * used for both CCD_SET_TEMP and CCD_GET_TEMP
 */

typedef enum {
    CCDTS_ERR,		/* temp controller error -- first so default is err */
    CCDTS_AT,		/* temp = target (+/- 1 degree) */
    CCDTS_UNDER,	/* temp < target */
    CCDTS_OVER,		/* temp > target */
    CCDTS_OFF,		/* turn off temperature controller */
    CCDTS_RDN,		/* temp is ramping down to target */
    CCDTS_RUP,		/* temp is ramping up to ambient */
    CCDTS_STUCK,	/* temp is stuck (can't reach target) */
    CCDTS_MAX,		/* temp cannot go any higher */
    CCDTS_AMB,		/* temp is at ambient, cooler idle */
    CCDTS_SET,		/* only used with CCD_SET_TEMP */
} CCDTempStatus;

typedef struct {
    CCDTempStatus s;	/* status (only used by CCD_GET_TEMP) */
    int t;		/* desired or current temp, degrees C */
} CCDTempInfo;

typedef enum {
    CCDSO_Closed = 0,	/* shutter remains closed throughout exposure */
    CCDSO_Open,		/* shutter is open throughout exposure */
    CCDSO_Dbl,		/* shutter open 3/4: OOCO */
    CCDSO_Multi,	/* shutter open 4/6: OOCOCO */
    CCDSO_N		/* number of options */
} CCDShutterOptions;

/* exposure parameters */
typedef struct {
    int bx, by;         /* x and y binning */
    int sx, sy, sw, sh; /* subimage upper left location and size */
    int duration;       /* ms to expose */
    CCDShutterOptions shutter;        /* how to manage shutter during exp. */
} CCDExpoParams;

/* drift scan parameters.
 * call ioctl with CCD_DRIFT_SCAN and one of these; then call read with a
 *   full-frame array; it starts a continuous drift scan that lasts until
 *   driver is closed.
 * (r) and (w) are from drivers's perspective.
 */
typedef struct {
    int row;		/* (w) rows of array filled so far; wraps to 0 */
    int rowint;		/* (r) interval between row starts, us; set any time */
} CCDDriftScan;

/* CCD driver callbacks */
typedef struct {
    int (*findCCD) (char *path, char *errmsg);
    int (*setExpCCD) (CCDExpoParams *expP, char *errmsg);
    int (*startExpCCD) (char *errmsg);
    int (*performRBIFlushCCD) (int flushMS, int numDrains, char* errmsg);
    int (*setupDriftScan) (CCDDriftScan *dsip, char *errmsg);
    void (*abortExpCCD) (void);
    int (*selectHandleCCD) (char *errmsg);
    int (*setTempCCD) (CCDTempInfo *tp, char *errmsg);
    int (*getTempCCD) (CCDTempInfo *tp, char *errmsg);
    int (*setShutterNow) (int open, char *errmsg);
    int (*getIDCCD) (char buf[], char *errmsg);
    int (*getSizeCCD) (CCDExpoParams *cep, char *errmsg);
    int (*readPix) (char *mem, int nbytes, int block, char *errmsg);
} CCDCallbacks;

#ifndef __KERNEL__
/* these are handy wrappers over the driver calls, if you want to go that
 * route, in ccdcamera.c.
 */
extern int setExpCCD (CCDExpoParams *expP, char *errmsg);
extern int startExpCCD (char *errmsg);
extern int setupDriftScan (CCDDriftScan *dsip, char *errmsg);
extern int performRBIFlushCCD (int flushMS, int numDrains, char* errmsg);
extern int selectHandleCCD (char *errmsg);
extern int readPixelNBCCD (char *mem, int nbytes, char *errmsg);
extern int readPixelCCD (char *mem, int nbytes, char *errmsg);
extern int setTempCCD (CCDTempInfo *tp, char *errmsg);
extern int getTempCCD (CCDTempInfo *tp, char *errmsg);
extern int setShutterNow (int open, char *errmsg);
extern int getIDCCD (char buf[], char *errmsg);
extern int getSizeCCD (CCDExpoParams *cep, char *errmsg);
extern int setPathCCD (char *path, int auxcam, char *errmsg);
extern void abortExpCCD (void);
#endif	/* __KERNEL__ */

#endif	/* _CCDCAM_H */

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: ccdcamera.h,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $
 */
