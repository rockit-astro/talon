/*==========================================================================*/
/* Apogee Camera Control API                                                */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   12/22/96    jmh     Created new API library files               */
/*   2.00   31.07.97    gkr     General cleanup, modifications for fifo     */
/*   x.20   26.08.97    gkr     Modifications for 95/NT                     */
/*   x.21   05.11.97    gkr     Modified to expose low level routines       */
/*                      gkr     Added [system] gain INI entry               */
/*   x.30   12.02.98    gkr     Added ASM 16/32 for line buffer transfers   */
/*                      gkr     Added VFLUSH optimizations                  */
/*                      gkr     Added HFLUSH optimizations                  */
/*                      gkr     Modified CAMDATA structure for above        */
/*                      gkr     Modified CAMDATA structure for LabView      */
/*   x.31   24.03.98    gkr     Added errorx code to high level routines    */
/*          25.03.98    gkr     Modified system limits 4095, 63, etc.       */
/*          27.04.98    gkr     Modified acquire_slice first line test      */
/*          27.04.98    gkr     Added INI [system]frame_timeout = 20        */
/*          27.04.98    gkr     Added INI [system]line_timeout = 1          */
/*   x.33   28.08.98    gkr     Modified INI skipr and skipc tests          */
/*          28.08.98    gkr     Modified load_ccd skipr and skipc test      */
/*   x.40   05.10.98    gkr     Modified CAM_DATA to include old_int        */
/*          05.10.98    gkr     Modifications to support old interfaces     */
/*   x.41   12.04.99    gkr     Use tscale in FD timeout calculations       */
/*   x.42   01.10.99    gkr     Added priority routines and code            */
/*   x.43   28.10.99    gkr     Added PPI reg_offset code                   */
/*   x.43   03-21-2000  gkr     Added LINUX conditionals                    */
/*                                                                          */
/*==========================================================================*/

#ifndef _apccd
#define _apccd

#ifdef __cplusplus
extern "C" {
#endif

// Conditional compilation defines

// define WINUTIL to activate Win32 specific code sections

#ifdef WINUTIL
#define IDLE_CLASS			0
#define NORMAL_CLASS		1
#define HIGH_CLASS			2
#define REALTIME_CLASS		3

#define IDLE_THREAD			0
#define LOWEST_THREAD		1
#define BELOW_NORMAL_THREAD	2
#define NORMAL_THREAD		3
#define ABOVE_NORMAL_THREAD	4
#define HIGH_THREAD			5
#define REALTIME_THREAD		6
#endif

// define FIFO to perform automatic fifo compensation during readout
#define FIFO

// Define low level I/O and a few common macros for Linux
#ifdef LINUX
#include <asm/io.h>
#define _inp(a)		inb((USHORT)(a))
#define _outp(a,b)	outb((USHORT)(b), (USHORT)(a))
#define max(a,b)    ((a)>(b)?(a):(b))
#define _MAX_PATH	256
#endif

// define _APG_PPI to activate parallel port interface code
// define _APG_NET to activate network interface code
#ifdef _APG_PPI
#define INPW(a)		ppi_inpw((USHORT)(a))
#define OUTPW(a,b)	ppi_outpw((USHORT)(a),(USHORT)(b))
#else
#ifdef _APG_NET
#define INPW(a)		net_inpw((USHORT)(a))
#define OUTPW(a,b)	net_outpw((USHORT)(a),(USHORT)(b))
#else
#ifdef LINUX
#define INPW(a)		inw((USHORT)(a))
#define OUTPW(a,b)	outw((USHORT)(b), (USHORT)(a))
#else
#define INPW(a)		_inpw((USHORT)(a))
#define OUTPW(a,b)	_outpw((USHORT)(a),(USHORT)(b))
#endif
#endif
#endif

/*==========================================================================*/
/* system limits                                                            */
/*==========================================================================*/

#define MODE_DEFAULT	0				/* default system mode value */
#define TEST_DEFAULT	0				/* default system test value */

#define MIN_AIC         1               /* minimum AIC value                */
#define MAX_AIC         4095            /* maximum AIC value                */

#define MIN_BIC         MIN_AIC         /* minimum BIC value                */
#define MAX_BIC         MAX_AIC         /* maximum BIC value                */

#define MAX_ROWS        4095            /* maximum unbinned rows on CCD     */
#define MAX_COLS        4095            /* maximum unbinned columns on CCD  */

#define MAX_HBIN        8               /* maximum horizontal binning factor*/
#define MAX_VBIN        63              /* maximum vertical binning factor  */

#define MAXCAMS         16              /* max number of CCD's in system    */

/*==========================================================================*/
/* General type declaractions and definitions                               */
/*==========================================================================*/

#ifdef DLLPROC
#undef DLLPROC
#endif

#if defined(WINVER) && !defined(_APGDLL)
#define _APGDLL
#endif

#ifdef LINUX
#define DLLPROC
#else
#ifdef _APGDLL
#ifdef WIN32
#define DLLPROC __stdcall
#else
#define DLLPROC far pascal _export
#endif
#else
#ifdef WIN32
#define DLLPROC __stdcall
#else
#define DLLPROC extern far pascal
#endif
#endif
#endif

/* convenience types */

typedef unsigned char   UCHAR;
typedef char            CHAR;
typedef unsigned short  USHORT;
typedef short           SHORT;
typedef unsigned long   ULONG;
#if !defined(WINVER) && !defined(_APGDLL)
typedef long            LONG;
typedef int				BOOL;
#endif
typedef double          DOUBLE;
#if defined(WINVER) || defined(_APGDLL)
#define VOID            void
#else
typedef void            VOID;
#endif

/* custom types */

#ifdef LINUX
#define FAR
#define HUGE
#else
#ifdef WIN32
#define HUGE
#else
#define HUGE			_huge
#define FAR             _far
#endif
#endif
typedef unsigned short  HUGE *PDATA;        /* pointer to image data        */
typedef unsigned short  HUGE *PLINE;        /* pointer to line of image data*/
typedef short           FAR *PSHORT;        /* pointer to a short           */
typedef unsigned short  FAR *PUSHORT;		/* pointer to a unsigned short  */
typedef unsigned long   FAR *PULONG;        /* pointer to unsigned long     */
typedef char            FAR *PCHAR;         /* pointer to char (string)     */
typedef unsigned char   FAR *PUCHAR;        /* pointer to an unsigned char  */
typedef double          FAR *PDOUBLE;		/* pointer to a double          */
typedef short           HCCD;               /* CCD handle                   */
typedef int             STATUS;             /* function return status type  */

/*==========================================================================*/
/* Global structure definitions                                             */
/*==========================================================================*/

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  Geometry paramter and binning                                           */
/*                                                                          */
/*  rows        Unbinned rows on device                                     */
/*  cols        Unbinned columns on device                                  */
/*  imgrows     Unbinned rows in ROI                                        */
/*  imgcols     Unbinned pixels in ROI                                      */
/*  aic         Binned after-image column count                             */
/*  bic         Binned before-image column count                            */
/*  hbin        Horizontal binning factor                                   */
/*  vbin        Vertical binning factor                                     */
/*  bir         Before-image line (row) count                               */
/*  air         After-image line (row) count                                */
/*                                                                          */
/*--------------------------------------------------------------------------*/

typedef struct {
    USHORT      base;           /* base address of controller card          */
    HCCD        handle;         /* assigned channel handle value            */
    USHORT      mode;           /* mode bits                                */
    USHORT      test;           /* test bits                                */

    USHORT      rows;           /* chip geometry - total rows (lines)       */
    USHORT      cols;           /* chip geometry - total columns (pix/row)  */
    USHORT      imgrows;        /* unbinned rows in ROI                     */
    USHORT      imgcols;        /* unbinned columns in ROI                  */

    USHORT      bir;            /* before-image line count                  */
    USHORT      bic;            /* before-image column count                */
    USHORT      skipr;          /* rows to discard before data transfer     */
    USHORT      skipc;			/* number of pixels in fifo pipeline        */

    USHORT      hbin;           /* horizontal image binning factor          */
    USHORT      vbin;           /* vertical image binning factor            */
    USHORT      rowcnt;         /* binned ROI rows                          */
    USHORT      colcnt;         /* binned ROI columns                       */

    USHORT      hflush;         /* horizontal flush binning factor          */
    USHORT      vflush;         /* vertical flush binning factor            */
    USHORT      air;            /* after-image line count                   */
    USHORT      aic;            /* after-image column count                 */

    USHORT      shutter_en;     /* shutter mode, enable or disable          */
    USHORT      trigger_mode;   /* trigger mode, external or normal         */
    USHORT      caching;        /* FIFO caching available                   */
    USHORT		gain;			/* High gain available 1/0                  */

    ULONG       timer;          /* timer count value                        */
    USHORT      cable;          /* cable length mode, long or short         */
    USHORT		data_bits;		/* ADC bit depth                            */

	/* Doubles must be on 64 bit boundary for LabView access                */
    DOUBLE      tscale;         /* timer calibration scaling factor         */
    DOUBLE      target_temp;    /* desired final temp of dewer              */
    DOUBLE		temp_backoff;	/* suggested temp adjust at max/min         */
    DOUBLE      temp_cal;       /* temp calibration factor                  */
    DOUBLE      temp_scale;     /* temp scaling factor                      */
	DOUBLE      temp_min;		/* min desired temp                         */
	DOUBLE      temp_max;		/* max desired temp                         */
	
    USHORT		port_bits;		/* TTL port bit depth                       */
    USHORT      oflush;         /* vertical flush binning offset            */
    USHORT		frame_timeout;	/* frame done timeout                       */
    USHORT		line_timeout;	/* line done timeout                        */

	USHORT		slice;			/* slice acquisitions available 1/0         */
    USHORT		slice_time;     /* minimum slice shutter time               */
    USHORT      camreg;         /* Gdata for serial output to cam head      */
    SHORT       gotcfg;         /* T if cfg prev loaded, F otherwise        */
    
	CHAR        cfgname[128];	/* configuration file name                  */

    USHORT      reg[12];        /* controller register mirrors              */

	USHORT		old_cam;		/* old camera interface --> mode=test=0     */
	USHORT		reg_offset;		/* PPI register offset                      */

    USHORT      error;          /* contains error bits for particular chan  */
    USHORT      errorx;			/* extended error information               */
    } CAMDATA;

typedef CAMDATA         HUGE *PCAMDATA;     /* pointer to camera structure  */

typedef struct {
    USHORT      bir;            /* before-slice line count                  */
    USHORT      bic;            /* before-slice column count                */
    USHORT      rowcnt;         /* binned slice rows                        */
    USHORT      colcnt;         /* binned slice columns                     */
    USHORT      imgrows;        /* unbinned rows in slice                   */
    USHORT      imgcols;        /* unbinned columns in slice                */
    USHORT      hbin;           /* horizontal slice binning factor          */
    USHORT      vbin;           /* vertical slice binning factor            */
    USHORT      valid;          /* config_slice called at least once        */
} SLICEDATA;

typedef SLICEDATA         HUGE *PSLICEDATA; /* pointer to slice structure   */

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

#ifdef WINVER
#undef IGNORE
#endif
#define IGNORE          ((USHORT)-1)              /* this MUST be defined for the API */

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
#define CCD_OPEN_CFGDATA    3   /* config missing or missing required data  */
#define CCD_OPEN_CFGSEND    4   /* error sending config data to controller  */
#define CCD_OPEN_LOOPTST    5   /* loopback test failed, no controller found*/
#define CCD_OPEN_ALLOC      6   /* memory alloc failed - system error       */
#define CCD_OPEN_MODE       7   /* could not load CCD mode bits             */
#define CCD_OPEN_TEST       8   /* could not load CCD test bits             */
#define CCD_OPEN_LOAD       9   /* could not load CCD control parameters    */
#define CCD_OPEN_NTIO	   10	/* NT I/O driver not present                */
#define CCD_OPEN_CABLE	   11	/* could not set cable length				*/
#define CCD_OPEN_CAMREG	   12	/* could not set camera register			*/

/*--------------------------------------------------------------------------*/
/* Error bits - bit values stored in camdata[n].error by API functions      */
/*                                                                          */
/* camdata[n].error should be checked whenever an error is returned by one  */
/* of the API functions (function returns CCD_ERROR) to determine the exact */
/* cause of the failure.                                                    */
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
#define CCD_ERR_BUFF    0x0800  /* invalid buffer pointer specified         */
#define CCD_ERR_NOFILE  0x1000  /* file not found or not valid              */
#define CCD_ERR_CFG     0x2000  /* configuration data invalid               */
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
#define CCD_TMP_RUP     8       /* (O) temp is ramping up to ambient/target */
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
#define SLICE           2       /* EXPOSE without timer reset               */

/*==========================================================================*/
/* High-Level API Functions                                                 */
/*==========================================================================*/

/*--------------------------------------------------------------------------*/
/* System routines and error reporting                                      */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
get_error          (HCCD   ccd_handle,      /* handle to open CCD device    */
					PUSHORT error);			/* pointer to error value       */

USHORT DLLPROC
get_version		   (void);					/* return library version       */

/*--------------------------------------------------------------------------*/
/* CCD controller access and set-up                                         */
/*--------------------------------------------------------------------------*/

HCCD DLLPROC
open_camera		   (USHORT mode,            /* mode bits                    */
					USHORT test,            /* test bits                    */
		    		PCHAR  cfgname);        /* configuration file name      */

STATUS DLLPROC
get_openerror	   (void);					/* return open_camera error     */

STATUS DLLPROC
close_camera       (HCCD   ccd_handle);     /* handle to open CCD device    */

STATUS DLLPROC
config_camera      (HCCD   ccd_handle,      /* handle to open CCD device    */
		    		USHORT bic_count,       /* before image column count    */
		    		USHORT bir_count,       /* before image row count       */
		    		USHORT col_count,       /* number of columns to read    */
		    		USHORT row_count,       /* number of rows to read       */
		    		USHORT hbin,            /* horizontal binning factor    */
		    		USHORT vbin,            /* vertical binning factor      */
		    		ULONG  timer,           /* exposure time                */
		    		USHORT cable_length,    /* cable length adjustment      */
		    		USHORT use_config);     /* use config or last values    */


STATUS DLLPROC
get_camdata        (HCCD ccd_handle,        /* handle to open CCD device    */
					PCAMDATA cd_ptr);       /* pointer to user structure    */

/*--------------------------------------------------------------------------*/
/* image acquisition and read-out                                           */
/*--------------------------------------------------------------------------*/

USHORT DLLPROC
set_mask 		   (USHORT new_mask);		/* 16 bit IRQ mask              */

STATUS DLLPROC
set_gain		   (HCCD   ccd_handle,
					USHORT bitval);			/* high gain 0/1                */

STATUS DLLPROC
set_shutter 	   (HCCD ccd_handle,
					USHORT bitval);			/* shutter open 0/1             */

STATUS DLLPROC
start_exposure     (HCCD   ccd_handle,      /* handle to open CCD device    */
		    		USHORT shutter_en,      /* shutter operation mode       */
		    		USHORT trigger_mode,    /* trigger enable/disable       */
		    		USHORT block,           /* set blocking mode ON/OFF     */
		    		USHORT flushwait);		/* wait for flush if TRUE       */

STATUS DLLPROC
check_exposure     (HCCD   ccd_handle);     /* handle to open CCD device    */

STATUS DLLPROC
check_trigger      (HCCD   ccd_handle);     /* handle to open CCD device    */

STATUS DLLPROC
check_line         (HCCD   ccd_handle);     /* handle to open CCD device    */


STATUS DLLPROC
check_image        (HCCD   ccd_handle);     /* handle to open CCD device    */


STATUS DLLPROC
acquire_line       (HCCD   ccd_handle,      /* handle to open CCD device    */
		    		USHORT caching,         /* use FIFO caching (T/F)       */
		    		PLINE  line_buffer,     /* pointer to line buffer       */
		    		USHORT  firsttime);     /* TRUE if first time called    */

STATUS DLLPROC
flush_air          (HCCD   ccd_handle,      /* handle to open CCD device    */
					USHORT airwait);		/* wait for AIR if TRUE         */

STATUS DLLPROC
acquire_image      (HCCD   ccd_handle,      /* handle to open CCD device    */
		    		USHORT caching,         /* use FIFO caching (T/F)       */
		    		PDATA buffer,           /* pointer to image buffer      */
					USHORT airwait);

STATUS DLLPROC
flush_image        (HCCD   ccd_handle,      /* handle to open CCD device    */
					USHORT airwait);		/* wait for AIR if TRUE         */

STATUS DLLPROC
reset_flush        (HCCD   ccd_handle);     /* handle to open CCD device    */

/*--------------------------------------------------------------------------*/
/* slice acquisition and read-out                                           */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
config_slice       (HCCD   ccd_handle,      /* handle to open CCD device    */
		    		USHORT bic_count,       /* before slice column count    */
		    		USHORT bir_count,       /* before slice row count       */
		    		USHORT col_count,       /* number of columns to read    */
		    		USHORT row_count,       /* number of rows to read       */
		    		USHORT hbin,            /* horizontal binning factor    */
		    		USHORT vbin);           /* vertical binning factor      */

STATUS DLLPROC
get_slicedata      (HCCD ccd_handle,        /* handle to open CCD device    */
					PSLICEDATA sd_ptr);     /* pointer to user structure    */

STATUS DLLPROC
acquire_slice      (HCCD   ccd_handle,      /* handle to open CCD device    */
		    		USHORT caching,         /* use FIFO caching (T/F)       */
					PDATA buffer,           /* pointer to image buffer      */
					USHORT reverse_enable,	/* restore imaging pixels       */
					USHORT shutter_gate,	/* control shutter              */
					USHORT overshoot,       /* overshoot row count          */
					USHORT fast);			/* enables faster timing        */

/*--------------------------------------------------------------------------*/
/* control functions                                                        */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_temp           (HCCD   ccd_handle,      /* handle to open CCD device    */
		    		DOUBLE desired_temp,    /* desired temp, in degrees C   */
		    		USHORT function);       /* temp control function        */


STATUS DLLPROC
get_temp           (HCCD   ccd_handle,      /* handle to open CCD device    */
		    		PSHORT temp_status,     /* temp status code value       */
		    		PDOUBLE temp_read);     /* temp value from register     */


STATUS DLLPROC
output_port        (HCCD   ccd_handle,      /* handle to open CCD device    */
		    		USHORT bits);           /* bit pattern to write to port */


STATUS DLLPROC
get_timer          (HCCD   ccd_handle,      /* handle to open CCD device    */
		    		PSHORT trigger,         /* trigger status return        */
		    		PSHORT timer);          /* timer status return          */


STATUS DLLPROC
ccd_command        (HCCD   ccd_handle,      /* handle to open CCD device    */
		    		USHORT cmdcode,         /* command function code        */
		    		USHORT block,           /* command ack polling control  */
		    		USHORT timeout);        /* command resp. timout limit   */

#ifdef WINUTIL
STATUS DLLPROC
get_acquire_priority_class(DWORD FAR *dwClass);

STATUS DLLPROC
set_acquire_priority_class(DWORD dwClass);

STATUS DLLPROC
get_acquire_thread_priority(DWORD FAR *dwClass);

STATUS DLLPROC
set_acquire_thread_priority(DWORD dwClass);
#endif

#ifdef __cplusplus
}
#endif

#endif
