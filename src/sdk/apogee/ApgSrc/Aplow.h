/*==========================================================================*/
/* Apogee CCD Control API - Low Level Functions                             */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   12/22/96    jmh     Created new API library files               */
/*                                                                          */
/*==========================================================================*/

#ifndef _aplow
#define _aplow

#include "apccd.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==========================================================================*/
/* control register bits and masks                                          */
/*==========================================================================*/

/*--------------------------------------------------------------------------*/
/* CAMREG : camera register bit set patterns                                */
/*--------------------------------------------------------------------------*/

#define CCD_BIT_OPT2	0x0002
#define CCD_BIT_OPT1	0x0004
#define CCD_BIT_GAIN	0x0008

#define CCD_MSK_OPT2	0xFFFD
#define CCD_MSK_OPT1	0xFFFB
#define CCD_MSK_GAIN	0xFFF7


/*--------------------------------------------------------------------------*/
/* REG01: control register bit set patterns                                 */
/*--------------------------------------------------------------------------*/

#define CCD_BIT_COOLER  0x8000          /* 1 = enable cooler operation      */
#define CCD_BIT_CABLE   0x4000          /* 1 = long cable, 0 = short        */
#define CCD_BIT_SLICE   0x2000          /* 1 = slice delay, 0 = normal      */
#define CCD_BIT_FLUSH   0x1000          /* start flushing             (TOG) */
#define CCD_BIT_NEXT    0x0800          /* start next line            (TOG) */
#define CCD_BIT_TIMER   0x0400          /* 1 = timer load enable            */
#define CCD_BIT_RDONE   0x0200          /* done reading curr line     (TOG) */
#define CCD_BIT_DOWN    0x0100          /* 1 = cooler shutdown              */
#define CCD_BIT_SHUTTER 0x0080          /* 1 = shutter enable               */
#define CCD_BIT_NOFLUSH 0x0040          /* stops flushing             (TOG) */
#define CCD_BIT_TRIGGER 0x0020          /* 1 = ext. trigger enable          */
#define CCD_BIT_CACHE   0x0010          /* 1 = enable FIFO caching          */
#define CCD_BIT_RESET   0x0008          /* system reset               (TOG) */
#define CCD_BIT_OVERRD  0x0004          /* 1 = shutter override             */
#define CCD_BIT_TMSTRT  0x0002          /* start timer with offset    (TOG) */
#define CCD_BIT_REVERSE 0x0001          /* 1 = reverse shifts, 0 = normal   */

/*--------------------------------------------------------------------------*/
/* REG01: control register bit mask patterns                                */
/*--------------------------------------------------------------------------*/

#define CCD_MSK_COOLER  0x7FFF
#define CCD_MSK_CABLE   0xBFFF
#define CCD_MSK_SLICE   0xDFFF
#define CCD_MSK_FLUSH   0xEFFF
#define CCD_MSK_NEXT    0xF7FF
#define CCD_MSK_TIMER   0xFBFF
#define CCD_MSK_RDONE   0xFDFF
#define CCD_MSK_DOWN    0xFEFF
#define CCD_MSK_SHUTTER 0xFF7F
#define CCD_MSK_NOFLUSH 0xFFBF
#define CCD_MSK_TRIGGER 0xFFDF
#define CCD_MSK_CACHE   0xFFEF
#define CCD_MSK_RESET   0xFFF7
#define CCD_MSK_OVERRD  0xFFFB
#define CCD_MSK_TMSTRT  0xFFFD
#define CCD_MSK_REVERSE 0xFFFE

/*--------------------------------------------------------------------------*/
/* temperature status bit masks                                             */
/*--------------------------------------------------------------------------*/

// isolate cooler enable bit, if 0, ignore rest
#define CCD_TMPOFF		0x20               /* 000x xxxx & 0010 0000 = 0     */
// isolate shutdown bits, ignore rest
#define CCD_TMPDONE  	0x18               /* 0011 1xxx & 0001 1000 = 0x18  */
// isolate cooler state bits, ignore shutdown complete
#define CCD_TNOSDC      0x7

#define CCD_TMPAMB      0x10               /* 0011 0xxx & 0001 1000 = 0x10  */

#define CCD_TMPOK       0x4                /* 0010 x100 & 0000 0111 = 0x4  */
#define CCD_TMPSTUCK    0x2                /* 0010 x010 & 0000 0111 = 0x2  */
#define CCD_TMPMAX      0x1                /* 0010 x001 & 0000 0111 = 0x1  */
#define CCD_TMPRAMP     0x0                /* 0010 x000 & 0000 0111 = 0x0  */



/*--------------------------------------------------------------------------*/
/* AP controller register definitions and bit masks                         */
/*--------------------------------------------------------------------------*/

/* zero-norm index values */
#define R01     0
#define R02     1
#define R03     2
#define R04     3
#define R05     4
#define R06     5
#define R07     6
#define R08     7
#define R09     8
#define R10     9
#define R11     10
#define R12     11

/* add register offset to base value to get I/O address */

#define CCD_REG01       0x000               /* write only registers         */
#define CCD_REG02       0x002
#define CCD_REG03       0x004
#define CCD_REG04       0x006
#define CCD_REG05       0x008
#define CCD_REG06       0x00a
#define CCD_REG07       0x00c
#define CCD_REG08       0x00e

#define CCD_REG09       0x000               /* read only registers          */
#define CCD_REG10       0x002
#define CCD_REG11       0x006
#define CCD_REG12       0x008

/* REG03 */

#define CCD_R_VBIN      0x3f00              /* VBIN bits in REG03           */
#define CCD_R_TMRHI     0x000f              /* high four bits of timer      */

#define CCD_SHFT_VBIN   8

/* REG04 */

#define CCD_R_AIC       0x0fff              /* AIC register in REG04        */
#define CCD_BIT_GCE     0x8000              /* REG04: Gconfig enable        */
#define CCD_BIT_GV1     0x4000              /* REG04: Gv1 bit               */
#define CCD_BIT_GV2     0x2000              /* REG04: Gv2 bit               */
#define CCD_BIT_AMP     0x1000              /* REG04: amp enable bit        */
#define CCD_MSK_AMP     0xEFFF              /* REG04: amp enable mask       */

#define CCD_SHFT_GCE    15
#define CCD_SHFT_GV1    14
#define CCD_SHFT_GV2    13
#define CCD_SHFT_AMP    12

/* REG05 */

#define CCD_R_RELAY     0xff00              /* RELAY bits in REG05          */
#define CCD_R_DAC       0x00ff              /* DAC bits in REG05            */

#define CCD_SHFT_RELAY  8

/* REG06 */

#define CCD_R_PIXEL     0x0fff              /* pixel count in REG06         */
#define CCD_R_HBIN      0x7000              /* hbin bits in REG06           */
#define CCD_BIT_VSYNC   0x8000              /* REG06: DAC vsync             */

#define CCD_SHFT_HBIN   12
#define CCD_SHFT_VSYNC  15

/* REG07 */

#define CCD_R_MODE      0xf000              /* mode bits in REG07           */
#define CCD_R_LINE      0x0fff              /* line register in REG07       */

#define CCD_SHFT_MODE   12

/* REG08 */

#define CCD_R_TEST      0xf000              /* test bits in REG08           */
#define CCD_R_BIC       0x0fff              /* BIC register in REG08        */

#define CCD_SHFT_TEST   12

/* REG10 */

#define CCD_R_TEMP      0x00ff              /* temp data in REG10           */

/* REG11 */

#define CCD_BIT_TST     0x8000              /* REG11: test bit              */
#define CCD_BIT_ACK     0x4000              /* REG11: command ack bit       */
#define CCD_BIT_FD      0x0800              /* REG11: frame done bit        */
#define CCD_BIT_TRG     0x0400              /* REG11: trigger bit           */
#define CCD_BIT_AT      0x0080              /* REG11: at temp bit           */
#define CCD_BIT_SC      0x0040              /* REG11: shutdown complete     */
#define CCD_BIT_TMAX    0x0020              /* REG11: temp max bit          */
#define CCD_BIT_TMIN    0x0010              /* REG11: temp min bit          */
#define CCD_BIT_DATA    0x0008              /* REG11: cam data bit          */
#define CCD_BIT_COK     0x0004              /* REG11: cache OK bit          */
#define CCD_BIT_LN      0x0002              /* REG11: line done bit         */
#define CCD_BIT_EXP     0x0001              /* REG11: exposin (?) bit       */

#define CCD_SHFT_TST    15
#define CCD_SHFT_ACK    14
#define CCD_SHFT_FD     11
#define CCD_SHFT_TRG    10
#define CCD_SHFT_AT      7
#define CCD_SHFT_SC      6
#define CCD_SHFT_TMAX    5
#define CCD_SHFT_TMIN    4
#define CCD_SHFT_DATA    3
#define CCD_SHFT_COK     2
#define CCD_SHFT_LN      1
#define CCD_SHFT_EXP     0

/* REG12  (use defs for command register (REG01) plus these */

#define CCD_BIT_FL      0x2000              /* start flushL bit             */
#define CCD_BIT_PP      0x0010              /* ping-pong bit (huh?)         */
#define CCD_BIT_TMR     0x0001              /* start timer bit              */


/****************************************************************************/
/* WARNING! These low-level functions are intended to be called from the    */
/*          high-level API functions in this library. They should be used   */
/*          with caution.                                                   */
/****************************************************************************/

/*--------------------------------------------------------------------------*/
/* parameter verification functions                                         */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC check_parms              (HCCD ccd_handle);

/*--------------------------------------------------------------------------*/
/* parameter load functions                                                 */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC load_aic_count           (HCCD ccd_handle, USHORT aic_count);
STATUS DLLPROC load_bic_count           (HCCD ccd_handle, USHORT bic_count);
STATUS DLLPROC load_camera_register     (HCCD ccd_handle, USHORT regdata);
STATUS DLLPROC load_desired_temp        (HCCD ccd_handle, DOUBLE desired_temp);
STATUS DLLPROC load_horizontal_binning  (HCCD ccd_handle, USHORT hbin);
STATUS DLLPROC load_line_count          (HCCD ccd_handle, USHORT line_count);
STATUS DLLPROC load_mode_bits           (HCCD ccd_handle, USHORT mode);
STATUS DLLPROC load_pixel_count         (HCCD ccd_handle, USHORT pixel_count);
STATUS DLLPROC load_test_bits           (HCCD ccd_handle, USHORT test);
STATUS DLLPROC load_timer_and_vbinning  (HCCD ccd_handle, ULONG  timer, USHORT vbin);
STATUS DLLPROC load_vertical_binning    (HCCD ccd_handle, USHORT vbin);

/*--------------------------------------------------------------------------*/
/* polling functions                                                        */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC poll_command_acknowledge (HCCD ccd_handle, USHORT timeout);
STATUS DLLPROC poll_frame_done          (HCCD ccd_handle, USHORT timeout);
STATUS DLLPROC poll_got_trigger         (HCCD ccd_handle, USHORT timeout);
STATUS DLLPROC poll_line_done           (HCCD ccd_handle, USHORT timeout);
STATUS DLLPROC poll_cache_read_ok       (HCCD ccd_handle, USHORT timeout);
STATUS DLLPROC poll_temp_status         (HCCD ccd_handle, PSHORT tmpcode);
STATUS DLLPROC poll_timer_status        (HCCD ccd_handle, USHORT timeout);

/*--------------------------------------------------------------------------*/
/* data input functions                                                     */
/*--------------------------------------------------------------------------*/

USHORT DLLPROC read_data                (HCCD ccd_handle);
USHORT DLLPROC read_temp                (HCCD ccd_handle);

/*--------------------------------------------------------------------------*/
/* system control functions                                                 */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC reset_system             (HCCD ccd_handle);
STATUS DLLPROC start_flushing           (HCCD ccd_handle);
STATUS DLLPROC start_next_line          (HCCD ccd_handle);
STATUS DLLPROC start_timer_offset       (HCCD ccd_handle);
STATUS DLLPROC start_timer              (HCCD ccd_handle);
STATUS DLLPROC stop_flushing    		(HCCD ccd_handle);
STATUS DLLPROC assert_done_reading      (HCCD ccd_handle);

/*--------------------------------------------------------------------------*/
/* bit set/clear functions                                                  */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC set_cable_length         (HCCD ccd_handle, USHORT bitval);
STATUS DLLPROC set_fifo_caching         (HCCD ccd_handle, USHORT bitval);
STATUS DLLPROC set_shutter_enable       (HCCD ccd_handle, USHORT bitval);
STATUS DLLPROC set_temp_shutdown        (HCCD ccd_handle, USHORT bitval);
STATUS DLLPROC set_cooler_enable        (HCCD ccd_handle, USHORT bitval);
STATUS DLLPROC set_trigger              (HCCD ccd_handle, USHORT bitval);
STATUS DLLPROC set_slice_delay          (HCCD ccd_handle, USHORT bitval);
STATUS DLLPROC set_reverse_enable       (HCCD ccd_handle, USHORT bitval);
STATUS DLLPROC set_slice_amp            (HCCD ccd_handle, USHORT bitval);

/*--------------------------------------------------------------------------*/
/* base I/O address verification and search                                 */
/*--------------------------------------------------------------------------*/

USHORT DLLPROC loopback					(USHORT base);

/*--------------------------------------------------------------------------*/
/* configuration file access                                                */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC config_load 	   			(HCCD ccd_handle, PCHAR  cfgname);
STATUS DLLPROC config_save				(HCCD ccd_handle, PCHAR  cfgname);

#ifdef __cplusplus
}
#endif

#endif
