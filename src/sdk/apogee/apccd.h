/*==========================================================================*/
/* Apogee CCD Control API                                                   */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   12/22/96    jmh     Created new API library files               */
/*   1.10    6/23/97    ecd     changes for linux                           */
/*                                                                          */
/*==========================================================================*/

#ifndef _apccd
#define _apccd


/*==========================================================================*/
/* system limits                                                            */
/*==========================================================================*/

/* N.B. if change MAXCAMS, add more apg?[] arrays in main.c 		    */
#define MAXCAMS         4		/* max number of CCD's in system    */

#define MIN_TEMP        -40             /* minimum cooler temperature       */
#define MAX_TEMP         40             /* maximum cooler temperature       */

#define MIN_AIC         1               /* minimum AIC value                */
#define MAX_AIC         4096            /* maximum AIC value                */

#define MIN_BIC         MIN_AIC         /* minimum BIC value                */
#define MAX_BIC         MAX_AIC         /* maximum BIC value                */

#define MAX_ROWS        4096            /* maximum unbinned rows on CCD     */
#define MAX_COLS        4096            /* maximum unbinned columns on CCD  */

#define MAX_HBIN        8               /* maximum horizontal binning factor*/
#define MAX_VBIN        8               /* maximum vertical binning factor  */

/*==========================================================================*/
/* General type declaractions and definitions                               */
/*==========================================================================*/

/* convenience types */

typedef unsigned char   UCHAR;
typedef char            CHAR;
typedef unsigned short  USHORT;
typedef short           SHORT;
typedef unsigned long   ULONG;
typedef long            LONG;
typedef void            VOID;

/* custom types */

typedef unsigned short *PIMAGE;             /* pointer to image data        */
typedef unsigned short *PLINE;              /* pointer to line of image data*/
typedef short          *PSHORT;             /* pointer to a short value     */
typedef long           *PLONG;              /* pointer to a long value      */
typedef unsigned short *PUSHORT;
typedef unsigned long  *PULONG;             /* pointer to unsigned long     */
typedef char           *PCHAR;              /* pointer to char (string)     */
typedef unsigned char  *PUCHAR;             /* pointer to an unsigned char  */
typedef short           HCCD;               /* CCD handle                   */
typedef short           STATUS;             /* function return status value */
typedef unsigned short  XMHNDL;             /* extended memory block handle */

/*--------------------------------------------------------------------------*/
/* General purpose values                                                   */
/*--------------------------------------------------------------------------*/

#ifndef TRUE
#define TRUE            1
#endif

#ifndef FALSE
#define FALSE           0
#endif

#ifndef YES
#define YES             1
#endif

#ifndef NO
#define NO              0
#endif

#ifndef ON
#define ON              1
#endif

#ifndef OFF
#define OFF             0
#endif

#define IGNORE          -1              /* this MUST be defined for the API */


/*==========================================================================*/
/* function return and parameter values                                     */
/*==========================================================================*/

/*--------------------------------------------------------------------------*/
/* function return values                                                   */
/*--------------------------------------------------------------------------*/

#define CCD_ERROR       0xFFFF  /* non-specific error return value (-1)     */
#define CCD_OK          0x0000  /* no errors - function completed normally  */

/*--------------------------------------------------------------------------*/
/* global API error status variable - used with open_camera                 */
/*--------------------------------------------------------------------------*/

extern STATUS APCCD_OpenErr;

#define CCD_OPEN_NOERR      0   /* no error detected                        */
#define CCD_OPEN_HCCD       1   /* could not obtain a control channel handle*/
#define CCD_OPEN_CFGNAME    2   /* no config file specified                 */
#define CCD_OPEN_CFGDATA    3   /* config contains invalid or missing data  */
#define CCD_OPEN_LOOPTST    4   /* loopback test failed, no controller found*/
#define CCD_OPEN_ALLOC      5   /* memory alloc failed - system error       */
#define CCD_OPEN_LOAD       6   /* could not load CCD control parameters    */


/*--------------------------------------------------------------------------*/
/* Error bits - bit values stored in camdata[n].error by API functions      */
/*                                                                          */
/* camdata[n].error should be checked whenever an error is returned by one  */
/* of the API functions (function returns CCD_ERROR) to determine the exact */
/* cause of the failure. Bits which have been set by an error will not be   */
/* cleared until the function is called again and no error results or the   */
/* programmer specifically clears camdata[n].error by writing zero to it.   */
/*--------------------------------------------------------------------------*/

#define CCD_ERR_BASE    0x0001  /* invalid base I/O address passed to func  */
#define CCD_ERR_REG     0x0002  /* register access operation error          */
#define CCD_ERR_SIZE    0x0004  /* invalid CCD geometry                     */
#define CCD_ERR_HBIN    0x0008  /* invalid horizontal binning factor        */
#define CCD_ERR_VBIN    0x0010  /* invalid vertical binning factor          */
#define CCD_ERR_AIC     0x0020  /* invalid AIC value                        */
#define CCD_ERR_BIC     0x0040  /* invalid BIC value                        */
#define CCD_ERR_OFF     0x0080  /* invalid line offset value                */
#define CCD_ERR_SETUP   0x0100  /* CCD controller sub-system not initialized*/
#define CCD_ERR_TEMP    0x0200  /* CCD cooler failure                       */
#define CCD_ERR_READ    0x0400  /* failure reading image data               */
#define CCD_ERR_BUFF    0x0800  /* invalid buffer pointer specfied          */
#define CCD_ERR_NOFILE  0x1000  /* file not found or not valid              */
#define CCD_ERR_CFG     0x2000  /* config. data invalid                     */
#define CCD_ERR_HCCD    0x4000  /* invalid CCD handle passed to function    */
#define CCD_ERR_PARM    0x8000  /* invalid parameter passed to function     */

/*--------------------------------------------------------------------------*/
/* command and status codes                                                 */
/*--------------------------------------------------------------------------*/

/* O = status output from controller, I = cmd input to controller, B = both */

#define CCD_TMP_OK      0       /* (O) temp = target (+/- 1 degree)         */
#define CCD_TMP_UNDER   1       /* (O) temp < target                        */
#define CCD_TMP_OVER    2       /* (O) temp > target                        */
#define CCD_TMP_ERR     3       /* (O) temp controller error                */
#define CCD_TMP_SET     4       /* (I) set CCD temperature                  */
#define CCD_TMP_AMB     5       /* (I) start ramp-up to ambient temperature */
#define CCD_TMP_OFF     6       /* (B) turn off temperature controller      */
#define CCD_TMP_RDN     7       /* (O) temp is ramping down to target       */
#define CCD_TMP_RUP     8       /* (O) temp is ramping up to ambient        */
#define CCD_TMP_STUCK   9       /* (O) temp is stuck (can't reach target)   */
#define CCD_TMP_MAX     10      /* (O) temp cannot go any higher            */
#define CCD_TMP_DONE    11      /* (O) temp is at ambient, cooler idle      */

#define CCD_TRIGNORM    0       /* trigger in "normal" mode                 */
#define CCD_TRIGEXT     1       /* trigger in "external" mode               */

#define CCD_SHTTR_NORM  0       /* shutter in normal mode of operation      */
#define CCD_SHTTR_BIAS  1       /* shutter in bias mode of operation        */
#define CCD_SHTTR_DARK  1       /* shutter in dark frame mode of operation  */
#define CCD_SHTTR_OPEN  2       /* force shutter open                       */
#define CCD_SHTTR_CLOSE 3       /* release an open shutter                  */

#define CCD_CMD_RSTSYS  0       /* reset system (controller card)           */
#define CCD_CMD_FLSTRT  1       /* start flushing operation                 */
#define CCD_CMD_FLSTOP  2       /* stop flushing operation                  */
#define CCD_CMD_LNSTRT  3       /* start line read                          */
#define CCD_CMD_TMSTRT  4       /* start exposure timer                     */

#define NOBLOCK         0       /* command ack polling control              */
#define BLOCK           1

#define NOWAIT          0       /* used to disable polling timeouts         */

#define CCD_INTERNAL    0       /* get parameter from control data block    */
#define CCD_PARAMETER   1       /* get parameter from function parm list    */

#define CCD_NOTRDY      -1      /* used by check_line and check_image       */

#define FLUSH           0       /* sets up line count for full flush        */
#define EXPOSE          1       /* sets up line count for BIR skip          */


/*==========================================================================*/
/* High-Level API Functions                                                 */
/*==========================================================================*/

/*--------------------------------------------------------------------------*/
/* CCD controller access and set-up                                         */
/*--------------------------------------------------------------------------*/

HCCD
open_camera        (SHORT  mode,            /* mode bits                    */
                    SHORT  test,            /* test bits                    */
                    PCHAR  cfgname,         /* config file, NULL for default*/
                    SHORT  testmode);       /* T = test for controller card */

STATUS
close_camera       (HCCD   ccd_handle);     /* handle to open CCD device    */


STATUS
config_camera      (HCCD   ccd_handle,      /* handle to open CCD device    */
                    SHORT  rows,            /* unbinned rows in CCD         */
                    SHORT  columns,         /* unbinned columns in CCD      */
                    SHORT  bir_count,       /* pre-image row count          */
                    SHORT  bic_count,       /* before image column count    */
                    SHORT  image_cols,      /* number of columns in ROI     */
                    SHORT  image_rows,      /* number of rows in ROI        */
                    SHORT  hbin,            /* horizontal binning factor    */
                    SHORT  vbin,            /* vertical binning factor      */
                    LONG   exp_time,        /* exposure time                */
                    SHORT  cable_length,    /* cable length adjustment      */
                    SHORT  use_config);     /* use config or last values    */


STATUS
check_parms        (HCCD ccd_handle);       /* handle to verify             */


/*--------------------------------------------------------------------------*/
/* image acquisition and read-out                                           */
/*--------------------------------------------------------------------------*/

STATUS
start_exposure     (HCCD   ccd_handle,      /* handle to open CCD device    */
                    SHORT  shutter_en,      /* shutter operation mode       */
                    SHORT  trigger_mode,    /* trigger enable/disable       */
                    SHORT  block,           /* set blocking mode ON/OFF     */
                    SHORT  flushwait);      /* wait for frame flush if TRUE */

STATUS
check_line         (HCCD   ccd_handle);     /* handle to open CCD device    */


STATUS
check_image        (HCCD   ccd_handle);     /* handle to open CCD device    */


STATUS
acquire_line       (HCCD   ccd_handle,      /* handle to open CCD device    */
                    SHORT  caching,         /* use FIFO caching (T/F)       */
                    PLINE  line_buffer,     /* pointer to line buffer       */
                    SHORT  firsttime);      /* TRUE if first time called    */


STATUS
acquire_image      (HCCD   ccd_handle,      /* handle to open CCD device    */
                    SHORT  caching,         /* use FIFO caching (T/F)       */
                    PIMAGE buffer,          /* pointer to image buffer      */
                    SHORT  airwait);        /* wait for AIR if TRUE         */

STATUS
flush_air          (HCCD   ccd_handle,      /* handle to open CCD device    */
                    SHORT  airwait);        /* wait for AIR if TRUE         */

STATUS
reset_flush        (HCCD   ccd_handle);     /* handle to open CCD device    */

/*--------------------------------------------------------------------------*/
/* control functions                                                        */
/*--------------------------------------------------------------------------*/

STATUS
set_temp           (HCCD   ccd_handle,      /* handle to open CCD device    */
                    LONG   desired_temp,    /* desired temp, in degrees C   */
                    SHORT  function);       /* temp control function        */


STATUS
get_temp           (HCCD   ccd_handle,      /* handle to open CCD device    */
		    PSHORT  temp_status,    /* temp status code value       */
                    PLONG  temp_read);      /* temp value from register     */


STATUS
output_port        (HCCD   ccd_handle,      /* handle to open CCD device    */
                    SHORT  bits);           /* bit pattern to write to port */


STATUS
shutter            (HCCD   ccd_handle,      /* handle to open CCD device    */
                    SHORT  state);          /* shutter state control        */


STATUS
get_timer          (HCCD   ccd_handle,      /* handle to open CCD device    */
                    PSHORT trigger,         /* trigger status return        */
                    PSHORT timer);          /* timer status return          */


STATUS
ccd_command        (HCCD   ccd_handle,      /* handle to open CCD device    */
                    SHORT  cmdcode,         /* command function code        */
                    SHORT  block,           /* command ack polling control  */
                    SHORT  timeout);        /* command resp. timout limit   */


/*--------------------------------------------------------------------------*/
/* base I/O address verification and search                                 */
/*--------------------------------------------------------------------------*/

USHORT
loopback           (USHORT base);           /* target address of controller */

/*--------------------------------------------------------------------------*/
/* configuration file access                                                */
/*--------------------------------------------------------------------------*/

STATUS
config_load (       HCCD   ccd_handle,      /* handle to open CCD device    */
                    PCHAR  cfgname);        /* file name or NULL for default*/


STATUS
config_save (       HCCD   ccd_handle,      /* handle to open CCD device    */
                    PCHAR  cfgname);        /* file name or NULL for default*/

#endif



/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: apccd.h,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $
 */
