/*==========================================================================*/
/* Apogee CCD Control API - Low Level Access Functions                      */
/*                                                                          */
/* (c) 1996,1997 Apogee Instruments                                         */
/*                                                                          */
/*                                                                          */
/*                                                                          */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   12/22/96    jmh     Created new API library files               */
/*   1.01    1/25/97    jmh     Removed loop bit funcs, and fixed hbin bug  */
/*   2.00   03-21-2000  gkr     Integrated LINUX conditionals               */
/*                                                                          */
/*==========================================================================*/

#ifdef _APGDLL
#include <windows.h>
#endif


#include <stdio.h>                      /* standard system includes         */
#include <stdlib.h>
#include <time.h>

#ifndef LINUX
#include <dos.h>
#include <conio.h>
#endif

#include "apccd.h"                      /* apogee specific includes         */
#include "apglob.h"
#include "apdata.h"
#include "aplow.h"
#include "aperr.h"
#ifdef _APG_PPI
#include "apppi.h"
#endif
#ifdef _APG_NET
#include "apnet.h"
#endif



/****************************************************************************/
/* WARNING! These low-level functions are intended to be called from the    */
/*          high-level API functions in this library. They should be used   */
/*          with caution.                                                   */
/****************************************************************************/



/*--------------------------------------------------------------------------*/
/* verification checks - used by most of the low-level functions            */
/*                                                                          */
/* WARNING: These macros assume that certain local variables are already    */
/*          defined. These are base, index, and ccd_handle. If these are    */
/*          not correctly defined a compiler error will result.             */
/*                                                                          */
/*--------------------------------------------------------------------------*/


/* check for a valid base address */

#define CHKBASE()   base = camdata[index].base;     /* fetch base address */ \
                    if (base == 0) {                /* see if it's valid  */ \
                        camdata[index].error |= CCD_ERR_BASE;                         \
                        return CCD_ERROR;                                    \
                        }                                                    \
                    else                                                     \
                        camdata[index].error &= ~CCD_ERR_BASE



/* check for a valid CCD channel handle */

#define CHKHCCD()   if (check_parms(ccd_handle) != CCD_OK) {                 \
                        camdata[index].error |= CCD_ERR_HCCD;                         \
                        return CCD_ERROR;                                    \
                        }                                                    \
                    else                                                     \
                        camdata[index].error &= ~CCD_ERR_HCCD



/* check for valid CCD geometry parameters */

#define CHKSIZE()   if (camdata[index].rows > MAX_ROWS) {                    \
                        camdata[index].errorx = APERR_ROWCNT;                  \
                        camdata[index].error |= CCD_ERR_SIZE;                \
                        return CCD_ERROR;                                    \
                        }                                                    \
                    if (camdata[index].cols > MAX_COLS) {                    \
                        camdata[index].errorx = APERR_COLCNT;                  \
                        camdata[index].error |= CCD_ERR_SIZE;                \
                        return CCD_ERROR;                                    \
                        }                                                    \
                    camdata[index].error &= ~CCD_ERR_SIZE

void
initialize_registers(HCCD hccd)
{
	USHORT index = hccd - 1;

	camdata[index].reg[R01] = camdata[index].reg[R12] = INPW(camdata[index].base + CCD_REG12);
}

STATUS DLLPROC
get_camdata(HCCD ccd_handle, PCAMDATA cdPtr)
{
    USHORT index = ccd_handle - 1;

    if ((ccd_handle < 1) || (ccd_handle > MAXCAMS))
        return CCD_ERROR;

	*cdPtr = camdata[index];
	
	return CCD_OK;
}

STATUS DLLPROC
get_slicedata(HCCD ccd_handle, PSLICEDATA sdPtr)
{
    USHORT index = ccd_handle - 1;

    CHKHCCD();

	*sdPtr = slicedata[index];
	
	return CCD_OK;
}

/*--------------------------------------------------------------------------*/
/* load_aic_count                                                           */
/*                                                                          */
/*                                                                          */
/* Type       : Parameter load function                                     */
/*                                                                          */
/* Description: Loads the AIC count into register 4                         */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  aic_count       AIC column count value              */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
load_aic_count (HCCD ccd_handle, USHORT aic_count)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();
    CHKSIZE();

	activate_camera (ccd_handle);
	
    /* check AIC range */
    if ((aic_count < MIN_AIC) || (aic_count > MAX_AIC)) {
    	camdata[index].errorx = APERR_AIC;
        camdata[index].error |= CCD_ERR_AIC;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_AIC;        /* reset camdata[index].error variable       */

    /* output new value */
    tmpval  = camdata[index].reg[R04];  /* get current register contents    */
    tmpval &= ~CCD_R_AIC;               /* clear current AIC value          */
    tmpval |= aic_count;                /* OR in the new value              */
    OUTPW(base + CCD_REG04,tmpval);     /* and write it back to the port    */
    camdata[index].reg[R04] = tmpval;   /* save our changes in the mirror   */

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* load_bic_count                                                           */
/*                                                                          */
/*                                                                          */
/* Type       : Parameter load function                                     */
/*                                                                          */
/* Description: Loads the BIC count into register 8                         */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  bic_count       BIC column count value              */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
load_bic_count (HCCD ccd_handle, USHORT bic_count)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();
    CHKSIZE();

	activate_camera (ccd_handle);
	
    /* check BIC range */
    if ((bic_count < MIN_BIC) || (bic_count > MAX_BIC)) {
    	camdata[index].errorx = APERR_BIC;
        camdata[index].error |= CCD_ERR_BIC;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_BIC;        /* reset camdata[index].error variable       */

    /* output new value */
    tmpval = camdata[index].reg[R08];   /* get current register contents    */
    tmpval &= ~CCD_R_BIC;               /* clear current BIC value          */
    tmpval |= bic_count;                /* OR in the new value              */
    OUTPW(base + CCD_REG08,tmpval);     /* and write it back to the port    */
    camdata[index].reg[R08] = tmpval;   /* save our changes in the mirror   */

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* load_camera_register                                                     */
/*                                                                          */
/*                                                                          */
/* Type       : Parameter load function                                     */
/*                                                                          */
/* Description: Outputs a stream of bits to the camera register.            */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  regdata         Register bits to output             */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
load_camera_register (HCCD ccd_handle, USHORT regdata)
{
    USHORT tmpval1, tmpval2, base, i;
    USHORT index = ccd_handle - 1;
    USHORT bitstr;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    reset_system(ccd_handle);           /* stop other activities            */

    tmpval1 = camdata[index].reg[R04];  /* get current register contents    */
    tmpval1 &= CCD_R_AIC;               /* preserve the AIC counter value   */

    tmpval2 = 0;
    tmpval2 |= 0x9000;                  /* enable output control bits       */
    OUTPW(base+CCD_REG04,tmpval2|tmpval1);

    bitstr = (regdata&0x00fe)|0x0080;   /* bit 7 always on, bit 0 always off*/
	camdata[index].camreg = bitstr;     /* preserve camera register bits    */

    for (i = 0; i < 8; i++) {

        if (bitstr & 0x0080)            /* set output bit based on input bit*/
            tmpval2 |= 0xd000;          /* enable data bit                  */
        else
            tmpval2 &= 0x9000;          /* disable data bit                 */
        OUTPW(base+CCD_REG04,tmpval2 | tmpval1);

        /* toggle the clock bit to send data */
        tmpval2 |= 0x2000;              /* turn on the clock bit            */
        OUTPW(base+CCD_REG04,tmpval2 | tmpval1);
        tmpval2 &= 0xd000;              /* turn off clock bit               */
        OUTPW(base+CCD_REG04,tmpval2 | tmpval1);

        bitstr = bitstr << 1;           /* shift next bit into test position*/
        }

    OUTPW(base + CCD_REG04,tmpval1);    /* we're done, disable serial out   */
    camdata[index].reg[R04] = tmpval1;

    start_flushing(ccd_handle);         /* resume flushing action           */

    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* load_desired_temp                                                        */
/*                                                                          */
/*                                                                          */
/* Type       : Parameter load function                                     */
/*                                                                          */
/* Description: Writes the target cooler temp to the DAC register           */
/*              using the following equation:                               */
/*                                                                          */
/*              target = cal + (temp * scale)                               */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              DOUBLE  desired_temp    Target temp in degrees C            */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
load_desired_temp (HCCD ccd_handle, DOUBLE desired_temp)
{
    USHORT tmpval, base, targ;
    USHORT index = ccd_handle - 1;
    DOUBLE cal, scale;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* check target temp for valid value */
    if ((desired_temp < camdata[index].temp_min) || (desired_temp > camdata[index].temp_max)) {
    	camdata[index].errorx = APERR_TEMP;
        camdata[index].error |= CCD_ERR_PARM;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_PARM;       /* reset camdata[index].error variable       */

    /* compute target temperature */
    cal  = camdata[index].temp_cal;
    scale= camdata[index].temp_scale;
    targ = (USHORT) (cal + (desired_temp * scale));

    /* output new value */
    tmpval = camdata[index].reg[R05];   /* get current register contents    */
    tmpval &= ~CCD_R_DAC;               /* clear current temp value         */
    tmpval |= (targ & CCD_R_DAC);       /* OR in the new value              */
    OUTPW(base + CCD_REG05,tmpval);     /* and write it back to the port    */
    camdata[index].reg[R05] = tmpval;   /* save our changes in the mirror   */

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* load_horizontal_binning                                                  */
/*                                                                          */
/*                                                                          */
/* Type       : Parameter load function                                     */
/*                                                                          */
/* Description: Loads horizontal binning value into register 6              */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  hbin            Horizontal binning factor:          */
/*                                                                          */
/*                                      Reg  Binning                        */
/*                                      Val  Factor                         */
/*                                      ---  -------                        */
/*                                       00 =   1                           */
/*                                        :     :                           */
/*                                       07 =   8                           */
/*                                                                          */
/*                                      The hbin value should already be    */
/*                                      in correct form (zero-based) when   */
/*                                      passed to this function.            */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
load_horizontal_binning (HCCD ccd_handle, USHORT hbin)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();
    CHKSIZE();

	activate_camera (ccd_handle);
	
    /* check hbin factor for valid value */
    if ((hbin == 0) || (hbin > MAX_HBIN)) {
        camdata[index].errorx = APERR_HBIN;
        camdata[index].error |= CCD_ERR_HBIN;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_HBIN;       /* reset camdata[index].error variable       */

    hbin--;                             /* adjust value for controller      */

    /* output new value */
    tmpval = camdata[index].reg[R06];   /* get current register contents    */
    tmpval &= ~CCD_R_HBIN;              /* clear current hbin value         */
    tmpval |= ((hbin << 12) & 0x7000);  /* OR in the new value              */
    OUTPW(base + CCD_REG06,tmpval);     /* and write it back to the port    */
    camdata[index].reg[R06] = tmpval;   /* save our changes in the mirror   */

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* load_line_count                                                          */
/*                                                                          */
/*                                                                          */
/* Type       : Parameter load function                                     */
/*                                                                          */
/* Description: Loads the flush sequencer line counter register. Refer to   */
/*              the documentation for additional details on the use of this */
/*              function.                                                   */
/*                                                                          */
/* Inputs:      HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  line_count      Line count value                    */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
load_line_count (HCCD ccd_handle, USHORT line_count)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();
    CHKSIZE();

	activate_camera (ccd_handle);
	
    /* check line count value */
    if ((line_count == 0) || (line_count >= MAX_ROWS)) {
        camdata[index].errorx = APERR_ROWCNT;
        camdata[index].error |= CCD_ERR_OFF;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_OFF;        /* reset camdata[index].error variable       */

    /* output new value */
    tmpval = camdata[index].reg[R07];   /* get current register contents    */
    tmpval &= ~CCD_R_LINE;              /* clear current line count value   */
    tmpval |= (line_count & CCD_R_LINE);/* OR in the new value              */
    OUTPW(base + CCD_REG07,tmpval);     /* and write it back to the port    */
    camdata[index].reg[R07] = tmpval;   /* save our changes in the mirror   */

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* load_mode_bits                                                           */
/*                                                                          */
/*                                                                          */
/* Type       : Parameter load function                                     */
/*                                                                          */
/* Description: Sets the mode control bits. Used for factory-defined        */
/*              special functions and diagnostics. Does not check for a     */
/*              valid mode value.                                           */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  mode            Mode nibble (lowest 4 bits of parm) */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
load_mode_bits (HCCD ccd_handle, USHORT mode)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* output new value */
    tmpval = camdata[index].reg[R07];   /* get current register contents    */
    tmpval &= ~CCD_R_MODE;              /* clear current mode value         */
    tmpval |= ((mode << 12) & 0xf000);  /* OR in the new value              */
    OUTPW(base + CCD_REG07,tmpval);     /* and write it back to the port    */
    camdata[index].reg[R07] = tmpval;   /* save our changes in the mirror   */

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* load_pixel_count                                                         */
/*                                                                          */
/*                                                                          */
/* Type       : Parameter load function                                     */
/*                                                                          */
/* Description: Loads the binned ROI column count for use during data       */
/*              read-out.                                                   */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channe          */
/*              USHORT  pixel_count     Binned ROI pixel count value        */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
load_pixel_count (HCCD ccd_handle, USHORT pixel_count)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();
    CHKSIZE();

	activate_camera (ccd_handle);
	
    /* check for valid pixel count */
    if (pixel_count > MAX_COLS / camdata[index].hbin) {
        camdata[index].errorx = APERR_COLCNT;
        camdata[index].error |= CCD_ERR_SIZE;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_SIZE;

    /* output new value */
    tmpval = camdata[index].reg[R06];   /* get current register contents    */
    tmpval &= ~CCD_R_PIXEL;             /* clear current pixel count value  */
    tmpval |= (pixel_count & 0x0fff);   /* OR in the new value              */
    OUTPW(base + CCD_REG06,tmpval);     /* and write it back to the port    */
    camdata[index].reg[R06] = tmpval;   /* save our changes in the mirror   */

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* load_test_bits                                                           */
/*                                                                          */
/*                                                                          */
/* Type       : Parameter load function                                     */
/*                                                                          */
/* Description: Sets the test control bits. Used for factory-defined        */
/*              special functions and diagnostics. Does not check for a     */
/*              valid test value.                                           */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  test            Test nibble (lowest 4 bits of parm) */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
load_test_bits (HCCD ccd_handle, USHORT test)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* output new value */
    tmpval = camdata[index].reg[R08];   /* get current register contents    */
    tmpval &= ~CCD_R_TEST;              /* clear current test value         */
    tmpval |= ((test << 12) & 0xf000);  /* OR in the new value              */
    OUTPW(base + CCD_REG08,tmpval);     /* and write it back to the port    */
    camdata[index].reg[R08] = tmpval;   /* save our changes in the mirror   */

    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* load_timer_and_vbinning                                                  */
/*                                                                          */
/*                                                                          */
/* Type       : Parameter load function                                     */
/*                                                                          */
/* Description: Loads the vertical binning and exposure timer values. There */
/*              is no limit check on exposure timing.                       */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              ULONG   timer           Exposure time value, in clock units */
/*              USHORT  vbin            Vertical binning factor             */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
load_timer_and_vbinning (HCCD ccd_handle, ULONG  timer, USHORT vbin)
{
    USHORT tmpval1, tmpval2, tmpval3, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();
    CHKSIZE();

	activate_camera (ccd_handle);
	
    /* check vbin value */
    if ((vbin == 0) || (vbin > MAX_VBIN)) {
        camdata[index].errorx = APERR_VBIN;
        camdata[index].error |= CCD_ERR_VBIN;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_VBIN;

    tmpval1  = camdata[index].reg[R03];         /* get previous reg. value  */
    tmpval1 &= ~CCD_R_VBIN;                     /* clear old vbin bits      */
    tmpval1 |= (vbin << CCD_SHFT_VBIN);         /* put in new vbin bits     */
    OUTPW(base + CCD_REG03,tmpval1);            /* write back to camera     */
    camdata[index].reg[R03] = tmpval1;          /* save new reg. value      */

    tmpval3 = INPW(base + CCD_REG12);           /* get current CCD status   */

    OUTPW(base+CCD_REG01, tmpval3 | CCD_BIT_TIMER);

    tmpval1 = (USHORT) timer & 0x0000ffff;      /* low 16-bits of timer     */

    tmpval2  = camdata[index].reg[R03];         /* get prev. reg. value     */
    tmpval2 &= 0xff00;                          /* preserve vbin register   */
    tmpval2 |= (USHORT) (timer >> 16);          /* set high 4 bits          */

    OUTPW(base + CCD_REG02,tmpval1);            /* write low 16-bits        */
    OUTPW(base + CCD_REG03,tmpval2);            /* write high 4 bits        */

    OUTPW(base+CCD_REG01, tmpval3 & ~CCD_BIT_TIMER);

    camdata[index].reg[R02] = tmpval1;          /* save new reg. value      */
    camdata[index].reg[R03] = tmpval2;          /* save new reg. value      */
    camdata[index].reg[R01] = camdata[index].reg[R12] = tmpval3;

    return CCD_OK;
}

/*--------------------------------------------------------------------------*/
/* poll_command_acknowledge                                                 */
/*                                                                          */
/*                                                                          */
/* Type       : Polling function                                            */
/*                                                                          */
/* Description: Polls the command acknowledge bit. Command acknowledge is   */
/*              set to TRUE when the controller completes an operation      */
/*              initiated by one of the command bits.                       */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              SHORT   timeout         Response timeout limit, in seconds  */
/*                                      A value of NOWAIT results in an     */
/*                                      immediate response with no polling. */
/*                                                                          */
/* Returns    : Tri-State return value:                                     */
/*                                                                          */
/*              TRUE        Bit TRUE or Operation Success                   */
/*              FALSE       Bit FALSE or Operation Falied                   */
/*              CCD_ERROR   Function encountered an error                   */
/*                          Check camdata[index].error variable for cause   */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
poll_command_acknowledge (HCCD ccd_handle,USHORT timeout)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;
    time_t tstrt, tend;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* immediate test and return if timeout = NOWAIT */
    if (timeout == NOWAIT) {
        tmpval = INPW(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_ACK)               /* test the cmd ack bit     */
            return TRUE;
        else
            return FALSE;
        }

    /* do a timed poll of the target bit */
    time(&tstrt);                               /* get current time, in secs*/
    time(&tend);

    while ((tend-tstrt) <= (time_t) timeout) {  /* while delta < timeout    */
        tmpval = INPW(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_ACK)               /* check for true condition */
            return TRUE;
        time(&tend);                            /* update interval end time */
        }

    return FALSE;                               /* exit here if we timed out*/
}



/*--------------------------------------------------------------------------*/
/* poll_frame_done                                                          */
/*                                                                          */
/*                                                                          */
/* Type       : Polling function                                            */
/*                                                                          */
/* Description: Polls the frame done bit. Frame done is set to TRUE when    */
/*              line count sequencer completes, such when it completes a    */
/*              flushing operation or a BIR count.                          */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              SHORT   timeout         Response timeout limit, in seconds  */
/*                                      A value of NOWAIT results in an     */
/*                                      immediate response with no polling. */
/*                                                                          */
/* Returns    : Tri-State return value:                                     */
/*                                                                          */
/*              TRUE        Bit TRUE or Operation Success                   */
/*              FALSE       Bit FALSE or Operation Falied                   */
/*              CCD_ERROR   Function encountered an error                   */
/*                          Check camdata[index].error variable for cause   */
/*                                                                          */
/*--------------------------------------------------------------------------*/


STATUS DLLPROC
poll_frame_done (HCCD ccd_handle, USHORT timeout)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;
    time_t tstrt, tend;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* immediate test and return if timeout = NOWAIT */
    if (timeout == NOWAIT) {
        tmpval = INPW(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_FD)                /* test the frame done bit  */
            return TRUE;
        else
            return FALSE;
        }

    /* do a timed poll of the target bit */

    time(&tstrt);                               /* get current time, in secs*/
    time(&tend);
    while ((tend-tstrt) <= (time_t) timeout) {  /* while delta < timeout    */
        tmpval = INPW(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_FD)                /* test the frame done bit  */
            return TRUE;
        time(&tend);                            /* update interval end time */
        }

    return FALSE;                               /* exit here if we timed out*/
}



/*--------------------------------------------------------------------------*/
/* poll_got_trigger                                                         */
/*                                                                          */
/*                                                                          */
/* Type       : Polling function                                            */
/*                                                                          */
/* Description: Polls the trigger bit in register 11.                       */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              SHORT   timeout         Response timeout limit, in seconds  */
/*                                      A value of NOWAIT results in an     */
/*                                      immediate response with no polling. */
/*                                                                          */
/* Returns    : Tri-State return value:                                     */
/*                                                                          */
/*              TRUE        Bit TRUE or Operation Success                   */
/*              FALSE       Bit FALSE or Operation Falied                   */
/*              CCD_ERROR   Function encountered an error                   */
/*                          Check camdata[index].error variable for cause   */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
poll_got_trigger (HCCD ccd_handle, USHORT timeout)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;
    time_t tstrt, tend;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* immediate test and return if timeout = NOWAIT */
    if (timeout == NOWAIT) {
        tmpval = INPW(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_TRG)               /* test the trigger bit     */
            return TRUE;
        else
            return FALSE;
        }

    /* do a timed poll of the target bit */
    time(&tstrt);                               /* get current time, in secs*/
    time(&tend);

    while ((tend-tstrt) <= (time_t) timeout) {  /* while delta < timeout    */
        tmpval = INPW(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_TRG)               /* check for true condition */
            return TRUE;
        time(&tend);                            /* update interval end time */
        }

    return FALSE;                               /* exit here if we timed out*/
}



/*--------------------------------------------------------------------------*/
/* poll_line_done                                                           */
/*                                                                          */
/*                                                                          */
/* Type       : Polling function                                            */
/*                                                                          */
/* Description: Polls the line done bit in register 11. Line done is TRUE   */
/*              when a valid line of CCD data, the length of which is the   */
/*              pixel count (binned value of ROI columns), is present at    */
/*              data output register.                                       */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              SHORT   timeout         Response timeout limit, in seconds  */
/*                                      A value of NOWAIT results in an     */
/*                                      immediate response with no polling. */
/*                                                                          */
/* Returns    : Tri-State return value:                                     */
/*                                                                          */
/*              TRUE        Bit TRUE or Operation Success                   */
/*              FALSE       Bit FALSE or Operation Falied                   */
/*              CCD_ERROR   Function encountered an error                   */
/*                          Check camdata[index].error variable for cause   */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
poll_line_done (HCCD ccd_handle, USHORT timeout)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;
    time_t tstrt, tend;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* immediate test and return if timeout = NOWAIT */
    if (timeout == NOWAIT) {
        tmpval = INPW(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_LN)                /* test the cmd ack bit     */
            return TRUE;
        else
            return FALSE;
        }

    /* do a timed poll of the target bit */
    time(&tstrt);                               /* get current time, in secs*/
    time(&tend);

    while ((tend-tstrt) <= (time_t) timeout) {  /* while delta < timeout    */
        tmpval = INPW(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_LN)                /* check for true condition */
            return TRUE;
        time(&tend);                            /* update interval end time */
        }

    return FALSE;                               /* exit here if we timed out*/
}



/*--------------------------------------------------------------------------*/
/* poll_cache_read_ok                                                       */
/*                                                                          */
/*                                                                          */
/* Type       : Polling function                                            */
/*                                                                          */
/* Description: Polls the cache status bit in register 11. Used during      */
/*              cached read operations to determine cache status.           */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              SHORT   timeout         Response timeout limit, in seconds  */
/*                                      A value of NOWAIT results in an     */
/*                                      immediate response with no polling. */
/*                                                                          */
/* Returns    : Tri-State return value:                                     */
/*                                                                          */
/*              TRUE        Bit TRUE or Operation Success                   */
/*              FALSE       Bit FALSE or Operation Falied                   */
/*              CCD_ERROR   Function encountered an error                   */
/*                          Check camdata[index].error variable for cause   */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
poll_cache_read_ok (HCCD ccd_handle, USHORT timeout)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;
    time_t tstrt, tend;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* immediate test and return if timeout = NOWAIT */
    if (timeout == NOWAIT) {
        tmpval = INPW(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_COK)               /* test the cmd ack bit     */
            return TRUE;
        else
            return FALSE;
        }

    /* do a timed poll of the target bit */
    time(&tstrt);                               /* get current time, in secs*/
    time(&tend);

    while ((tend-tstrt) <= (time_t) timeout) {  /* while delta < timeout    */
        tmpval = INPW(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_COK)               /* check for true condition */
            return TRUE;
        time(&tend);                            /* update interval end time */
        }

    return FALSE;                               /* exit here if we timed out*/
}



/*--------------------------------------------------------------------------*/
/* poll_temp_status                                                         */
/*                                                                          */
/*                                                                          */
/* Type       : Polling function                                            */
/*                                                                          */
/* Description: Collects various status bits related to the CCD cooler      */
/*              and generates a composite status word. Does not have a      */
/*              timeout parameter. Returns immediately after reading the    */
/*              appropriate bits.                                           */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              PSHORT  tmpcode         Pointer to a variable in which to   */
/*                                      return the temp status code.        */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
poll_temp_status (HCCD ccd_handle, PSHORT tmpcode)
{
    USHORT tmpval1, tmpval2, tmpval3, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    tmpval1 = tmpval2 = tmpval3 = 0;

    tmpval1 = INPW(base + CCD_REG12);
    camdata[index].reg[R01] = camdata[index].reg[R12] = tmpval1;
    
    tmpval2 = (tmpval1 >> 10) & 0x0020; /* got cooler enable bit    */
    tmpval3 |= tmpval2;

    tmpval2 = (tmpval1 >> 4) & 0x0010;  /* got shutdown bit         */
    tmpval3 |= tmpval2;

    tmpval1 = INPW(base + CCD_REG11);
    camdata[index].reg[R11] = tmpval1;

    tmpval2 = (tmpval1 >> 3) & 0x0008;  /* got shutdown comp bit    */
    tmpval3 |= tmpval2;

    tmpval2 = (tmpval1 >> 5) & 0x0004;  /* got at temp bit          */
    tmpval3 |= tmpval2;

    tmpval2 = (tmpval1 >> 4) & 0x0002;  /* got temp max bit         */
    tmpval3 |= tmpval2;

    tmpval2 = (tmpval1 >> 4) & 0x0001;  /* got temp min bit         */
    tmpval3 |= tmpval2;

    *tmpcode = tmpval3;     /* and we're done */

    return TRUE;
}



/*--------------------------------------------------------------------------*/
/* poll_timer_status                                                        */
/*                                                                          */
/*                                                                          */
/* Type       : Polling function                                            */
/*                                                                          */
/* Description: Polls the exposure bit, which is FALSE when the timer       */
/*              counts down to zero.                                        */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              SHORT   timeout         Response timeout limit, in seconds  */
/*                                      A value of NOWAIT results in an     */
/*                                      immediate response with no polling. */
/*                                                                          */
/* Returns    : Tri-State return value:                                     */
/*                                                                          */
/*              TRUE        Operation Success, exposure is complete         */
/*              FALSE       Operation Failed, exposure timeout              */
/*              CCD_ERROR   Function encountered an error                   */
/*                          Check camdata[index].error variable for cause   */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
poll_timer_status (HCCD ccd_handle, USHORT timeout)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;
    time_t tstrt, tend;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* immediate test and return if timeout = NOWAIT */
    if (timeout == NOWAIT) {
        tmpval = INPW(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (!(tmpval & CCD_BIT_EXP))            /* test the exposing bit    */
            return TRUE;
        else
            return FALSE;
        }

    /* do a timed poll of the target bit */
    time(&tstrt);                               /* get current time, in secs*/
    time(&tend);

    while ((tend-tstrt) <= (time_t) timeout) {  /* while delta < timeout    */
        tmpval = INPW(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (!(tmpval & CCD_BIT_EXP))            /* test the exposing bit    */
            return TRUE;
        time(&tend);                            /* update interval end time */
        }

    return FALSE;                               /* exit here if we timed out*/
}



/*--------------------------------------------------------------------------*/
/* read_data                                                                */
/*                                                                          */
/* Type       : Data input function                                         */
/*                                                                          */
/* Description: Reads a 16-bit word of data from the CCD data ouput         */
/*              register. Does not check for valid CCD channel handle or    */
/*              valid base address.                                         */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*                                                                          */
/* Returns    : Unsigned data word from CCD.                                */
/*                                                                          */
/*--------------------------------------------------------------------------*/

USHORT DLLPROC
read_data (HCCD ccd_handle)
{
    USHORT tmpval;
    USHORT base;
    USHORT index = ccd_handle - 1;

	activate_camera (ccd_handle);
	
    base = camdata[index].base;

    tmpval = INPW(base + CCD_REG09);
    camdata[index].reg[R09] = (USHORT) (tmpval + 32768);

    return tmpval;
}


/*--------------------------------------------------------------------------*/
/* read_temp                                                                */
/*                                                                          */
/* Type       : Data input function                                         */
/*                                                                          */
/* Description: Reads an 8-bit value from the CCD temperature sensor A/D.   */
/*              Does not check for valid CCD channel handle or valid base   */
/*              address.                                                    */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*                                                                          */
/* Returns    : Temperature value from CCD A/D. The value is promoted to an */
/*              unsigned short on return, which may be recast to a signed   */
/*              value by the caller.                                        */
/*                                                                          */
/*--------------------------------------------------------------------------*/

USHORT DLLPROC
read_temp (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

	activate_camera (ccd_handle);
	
    base = camdata[index].base;
    tmpval = INPW(base + CCD_REG10);
    camdata[index].reg[R10] = tmpval & 0x00ff;

    return (tmpval & 0x00ff);
}



/*--------------------------------------------------------------------------*/
/* reset_system                                                             */
/*                                                                          */
/*                                                                          */
/* Type       : System control function                                     */
/*                                                                          */
/* Description: Toggles the reset bit                                       */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
reset_system (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status   */
    tmpval  = INPW(base + CCD_REG12);
	/* preserve cooler enable bit */
	tmpval &= CCD_BIT_COOLER;
    OUTPW(base+CCD_REG01, tmpval);
    camdata[index].reg[R01] = camdata[index].reg[R12] = tmpval;

    /* toggle control bit */
    OUTPW(base+CCD_REG01, tmpval | CCD_BIT_RESET);
    OUTPW(base+CCD_REG01, tmpval & ~CCD_BIT_RESET);
    OUTPW(base+CCD_REG01, tmpval | CCD_BIT_RESET);
    OUTPW(base+CCD_REG01, tmpval & ~CCD_BIT_RESET);

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* start_flushing                                                           */
/*                                                                          */
/*                                                                          */
/* Type       : System control function                                     */
/*                                                                          */
/* Description: Toggles the flush start bit                                 */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
start_flushing (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status   */
    tmpval = INPW(base + CCD_REG12);
    camdata[index].reg[R01] = camdata[index].reg[R12] = tmpval;

    /* toggle control bit */
    OUTPW(base+CCD_REG01, tmpval | CCD_BIT_FLUSH);
    OUTPW(base+CCD_REG01, tmpval & ~CCD_BIT_FLUSH);

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* start_next_line                                                          */
/*                                                                          */
/*                                                                          */
/* Type       : System control function                                     */
/*                                                                          */
/* Description: Toggles the next line                                       */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
start_next_line (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status   */
    tmpval = INPW(base + CCD_REG12);
    camdata[index].reg[R01] = camdata[index].reg[R12] = tmpval;

    /* toggle control bit */
    OUTPW(base+CCD_REG01, tmpval | CCD_BIT_NEXT);
    OUTPW(base+CCD_REG01, tmpval & ~CCD_BIT_NEXT);

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* start_timer_offset                                                       */
/*                                                                          */
/*                                                                          */
/* Type       : System control function                                     */
/*                                                                          */
/* Description: Toggles the timer start bit                                 */
/*              This controls the "timer start with offset" bit.            */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
start_timer_offset (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status   */
    tmpval = INPW(base + CCD_REG12);
    camdata[index].reg[R01] = camdata[index].reg[R12] = tmpval;

    /* toggle control bit */
    OUTPW(base+CCD_REG01, tmpval | CCD_BIT_TMSTRT);
    OUTPW(base+CCD_REG01, tmpval & ~CCD_BIT_TMSTRT);

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* stop_flushing                                                            */
/*                                                                          */
/*                                                                          */
/* Type       : System control function                                     */
/*                                                                          */
/* Description: Toggles the flush stop bit                                  */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC stop_flushing (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status   */
    tmpval = INPW(base + CCD_REG12);
    camdata[index].reg[R01] = camdata[index].reg[R12] = tmpval;

    /* toggle control bit */
    OUTPW(base+CCD_REG01, tmpval | CCD_BIT_NOFLUSH);
    OUTPW(base+CCD_REG01, tmpval & ~CCD_BIT_NOFLUSH);

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* assert_done_reading                                                      */
/*                                                                          */
/*                                                                          */
/* Type       : System control function                                     */
/*                                                                          */
/* Description: Toggles the done reading bit                                */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
assert_done_reading (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status   */
    tmpval = INPW(base + CCD_REG12);
    camdata[index].reg[R01] = camdata[index].reg[R12] = tmpval;

    /* toggle control bit */
    OUTPW(base+CCD_REG01, tmpval | CCD_BIT_RDONE);
    OUTPW(base+CCD_REG01, tmpval & ~CCD_BIT_RDONE);

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* set_cable_length                                                         */
/*                                                                          */
/*                                                                          */
/* Type       : Bit set/clear function                                      */
/*                                                                          */
/* Description: Sets cable mode bit.                                        */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  bitval          Desired bit on/off (1/0) value      */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_cable_length (HCCD ccd_handle, USHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status */
    tmpval = camdata[index].reg[R01];

    /* modify bit per control parameter */
    tmpval &= CCD_MSK_CABLE;
    if (bitval)
        tmpval |= CCD_BIT_CABLE;

    /* write change back to control register */
    OUTPW(base+CCD_REG01,tmpval);
    camdata[index].reg[R01] = tmpval;

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* set_slice_amp                                                          */
/*                                                                          */
/*                                                                          */
/* Type       : Bit set/clear function                                      */
/*                                                                          */
/* Description: Sets slice amp bit.                                       */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  bitval          Desired bit on/off (1/0) value      */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_slice_amp (HCCD ccd_handle, USHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status */
    tmpval = camdata[index].reg[R04];

    /* modify bit per control parameter */
    tmpval &= CCD_MSK_AMP;
    if (bitval)
        tmpval |= CCD_BIT_AMP;

    /* write change back to control register */
    OUTPW(base + CCD_REG04,tmpval);
    camdata[index].reg[R04] = tmpval;

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* set_slice_delay                                                          */
/*                                                                          */
/*                                                                          */
/* Type       : Bit set/clear function                                      */
/*                                                                          */
/* Description: Sets slice delay bit.                                       */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  bitval          Desired bit on/off (1/0) value      */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_slice_delay (HCCD ccd_handle, USHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status */
    tmpval = camdata[index].reg[R01];

    /* modify bit per control parameter */
    tmpval &= CCD_MSK_SLICE;
    if (bitval)
        tmpval |= CCD_BIT_SLICE;

    /* write change back to control register */
    OUTPW(base+CCD_REG01,tmpval);
    camdata[index].reg[R01] = tmpval;

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* set_reverse_enable                                                       */
/*                                                                          */
/*                                                                          */
/* Type       : Bit set/clear function                                      */
/*                                                                          */
/* Description: Sets reverse enable bit.                                    */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  bitval          Desired bit on/off (1/0) value      */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_reverse_enable (HCCD ccd_handle, USHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status */
    tmpval = camdata[index].reg[R01];

    /* modify bit per control parameter */
    tmpval &= CCD_MSK_REVERSE;
    if (bitval)
        tmpval |= CCD_BIT_REVERSE;

    /* write change back to control register */
    OUTPW(base+CCD_REG01,tmpval);
    camdata[index].reg[R01] = tmpval;

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* set_fifo_caching                                                         */
/*                                                                          */
/*                                                                          */
/* Type       : Bit set/clear function                                      */
/*                                                                          */
/* Description: Sets FIFO caching mode                                      */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  bitval          Desired bit on/off (1/0) value      */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_fifo_caching (HCCD ccd_handle, USHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status */
    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_CACHE;

    /* modify bit per control parameter */
    if (bitval)
        tmpval |= CCD_BIT_CACHE;

    /* write change back to control register */
    OUTPW(base+CCD_REG01,tmpval);
    camdata[index].reg[R01] = tmpval;

    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* set_shutter_enable                                                       */
/*                                                                          */
/*                                                                          */
/* Type       : Bit set/clear function                                      */
/*                                                                          */
/* Description: Sets or clears the shutter mode (enable/disable) bit        */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  bitval          Desired bit on/off (1/0) value      */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_shutter_enable (HCCD ccd_handle, USHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status */
    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_SHUTTER;

    /* modify bit per control parameter */
    if (bitval)
        tmpval |= CCD_BIT_SHUTTER;

    /* write change back to control register */
    OUTPW(base+CCD_REG01,tmpval);
    camdata[index].reg[R01] = tmpval;

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* set_shutter                                                              */
/*                                                                          */
/*                                                                          */
/* Type       : Bit set/clear function                                      */
/*                                                                          */
/* Description: Set or clears the shutter open override bit                 */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  bitval          Desired bit on/off (1/0) value      */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_shutter (HCCD ccd_handle, USHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status */
    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_OVERRD;

    /* modify bit per control parameter */
    if (bitval)
        tmpval |= CCD_BIT_OVERRD;

    /* write change back to control register */
    OUTPW(base+CCD_REG01,tmpval);
    camdata[index].reg[R01] = tmpval;

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* set_temp_val                                                             */
/*                                                                          */
/*                                                                          */
/* Type       : Bit set/clear function                                      */
/*                                                                          */
/* Description: Sets the cooler enable bit.                                 */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_temp_val (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status */
    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_COOLER;

    /* set bit ON */
    tmpval |= CCD_BIT_COOLER;

    /* write change back to control register */
    OUTPW(base+CCD_REG01,tmpval);
    camdata[index].reg[R01] = tmpval;

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* set_temp_shutdown                                                        */
/*                                                                          */
/*                                                                          */
/* Type       : Bit set/clear function                                      */
/*                                                                          */
/* Description: Sets or clears the cooler shutdown bit                      */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  bitval          Desired bit on/off (1/0) value      */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_temp_shutdown (HCCD ccd_handle, USHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

    /* get current CCD status */
    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_DOWN;

    /* modify bit per control parameter */
    if (bitval)
        tmpval |= CCD_BIT_DOWN;

    /* write change back to control register */
    OUTPW(base+CCD_REG01,tmpval);
    camdata[index].reg[R01] = tmpval;

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* set_cooler_enable                                                        */
/*                                                                          */
/*                                                                          */
/* Type       : Bit set/clear function                                      */
/*                                                                          */
/* Description: Sets or clears the cooler enable bit                        */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  bitval          Desired bit on/off (1/0) value      */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_cooler_enable (HCCD ccd_handle, USHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status */
    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_COOLER;

    /* modify bit per control parameter */
    if (bitval)
        tmpval |= CCD_BIT_COOLER;

    /* write change back to control register */
    OUTPW(base+CCD_REG01,tmpval);
    camdata[index].reg[R01] = tmpval;

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* set_trigger                                                              */
/*                                                                          */
/*                                                                          */
/* Type       : Bit set/clear function                                      */
/*                                                                          */
/* Description: Sets or clears the trigger enable bit                       */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  bitval          Desired bit on/off (1/0) value      */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_trigger (HCCD ccd_handle, USHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
    /* get current CCD status */
    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_TRIGGER;
	camdata[index].trigger_mode = CCD_TRIGNORM;

    /* modify bit per control parameter */
    if (bitval) {
        tmpval |= CCD_BIT_TRIGGER;
		camdata[index].trigger_mode = CCD_TRIGEXT;
		}

    /* write change back to control register */
    OUTPW(base+CCD_REG01,tmpval);
    camdata[index].reg[R01] = tmpval;

    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* set_gain                                                                 */
/*                                                                          */
/*                                                                          */
/* Type       : Bit set/clear function                                      */
/*                                                                          */
/* Description: Sets or clears the high gain bit                            */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*              USHORT  bitval          Desired bit on/off (1/0) value      */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_gain (HCCD ccd_handle, USHORT bitval)
{
    USHORT tmpval;
    USHORT index = ccd_handle - 1;

    /* check camera handle */
    CHKHCCD();

	activate_camera (ccd_handle);
	
	/* check whether dual gain system */
	if (!camdata[index].gain) {
        camdata[index].error |= CCD_ERR_CFG;
		return CCD_ERROR;
		}

    /* mask gain from camera register mirror */
	tmpval = camdata[index].camreg;
    tmpval &= CCD_MSK_GAIN;

    /* modify bit per control parameter */
    if (bitval)
        tmpval |= CCD_BIT_GAIN;

    /* update camera registers */
	return (load_camera_register(ccd_handle, tmpval));
}


/*--------------------------------------------------------------------------*/
/* output_port                                                              */
/*                                                                          */
/* generate outputs on the auxilliary control lines.                        */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
output_port (HCCD ccd_handle, USHORT bits)
{
    USHORT base;
    USHORT index = ccd_handle - 1;
	USHORT mask, tmpval;

    /* check handle and get base address */
    CHKHCCD();
    CHKBASE();

	activate_camera (ccd_handle);
	
	/* mask and shift input bits to start at bit 8 */
	mask = (USHORT) ((1L << camdata[index].port_bits) - 1);
	bits = (bits & mask) << 8;

    /* replace available bits in register value */
    mask = ~(mask << 8);
    tmpval = (camdata[index].reg[R05] & mask) | bits;

	/* write changes to control register and save mirror */
    OUTPW(base + CCD_REG05, tmpval);
    camdata[index].reg[R05] = tmpval;

    return CCD_OK;
}
