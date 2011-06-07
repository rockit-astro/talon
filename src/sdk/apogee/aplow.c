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
/*   1.02    6/23/97    ecd     changes for linux                           */
/*   1.03    5/26/00    ecd     tweak reset_system                          */
/*                                                                          */
/*==========================================================================*/


#include "apccd.h"                      /* apogee specific includes         */
#include "apglob.h"
#include "apdata.h"
#include "aplow.h"

#include "aplinux.h"



#define R01     0                       /* zero-norm index values           */
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
                        camdata[index].error |= CCD_ERR_SIZE;                         \
                        return CCD_ERROR;                                    \
                        }                                                    \
                    if (camdata[index].cols > MAX_COLS) {                    \
                        camdata[index].error |= CCD_ERR_SIZE;                         \
                        return CCD_ERROR;                                    \
                        }                                                    \
                    camdata[index].error &= ~CCD_ERR_SIZE




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

STATUS load_aic_count (HCCD ccd_handle, SHORT aic_count)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();
    CHKSIZE();


    /* check AIC range */

    if ((aic_count < MIN_AIC) || (aic_count > MAX_AIC)) {
        camdata[index].error |= CCD_ERR_AIC;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_AIC;        /* reset camdata[index].error variable       */


    /* output new value */

    tmpval  = camdata[index].reg[R04];  /* get current register contents    */
    tmpval &= ~CCD_R_AIC;               /* clear current AIC value          */
    tmpval |= aic_count;                /* OR in the new value              */
    outpw(base + CCD_REG04,tmpval);     /* and write it back to the port    */
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

STATUS load_bic_count (HCCD ccd_handle, SHORT bic_count)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();
    CHKSIZE();


    /* check BIC range */

    if ((bic_count < MIN_BIC) || (bic_count > MAX_BIC)) {
        camdata[index].error |= CCD_ERR_BIC;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_BIC;        /* reset camdata[index].error variable       */


    /* output new value */

    tmpval = camdata[index].reg[R08];   /* get current register contents    */
    tmpval &= ~CCD_R_BIC;               /* clear current BIC value          */
    tmpval |= bic_count;                /* OR in the new value              */
    outpw(base + CCD_REG08,tmpval);     /* and write it back to the port    */
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

STATUS load_camera_register (HCCD ccd_handle, SHORT regdata)
{
    USHORT tmpval1, tmpval2, base, i;
    USHORT index = ccd_handle - 1;
    USHORT bitstr;

    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();

    reset_system(ccd_handle);           /* stop other activities            */

    tmpval1 = camdata[index].reg[R04];  /* get current register contents    */
    tmpval1 &= CCD_R_AIC;               /* preserve the AIC counter value   */

    tmpval2 = 0;
    tmpval2 |= 0x9000;                  /* enable output control bits       */
    outpw(base+CCD_REG04,tmpval2|tmpval1);

    /* bit 7 always on, bit 0 always off*/
    bitstr = ((USHORT) regdata & 0x00ff)| 0x0080;

    for (i = 0; i < 8; i++) {

        if (bitstr & 0x0080)            /* set output bit based on input bit*/
            tmpval2 |= 0xd000;          /* enable data bit                  */
        else
            tmpval2 &= 0x9000;          /* disable data bit                 */
        outpw(base+CCD_REG04,tmpval2 | tmpval1);

        /* toggle the clock bit to send data */
        tmpval2 |= 0x2000;              /* turn on the clock bit            */
        outpw(base+CCD_REG04,tmpval2 | tmpval1);
        tmpval2 &= 0xd000;              /* turn off clock bit               */
        outpw(base+CCD_REG04,tmpval2 | tmpval1);

        bitstr = bitstr << 1;           /* shift next bit into test position*/
        }

    outpw(base + CCD_REG04,tmpval1);    /* we're done, disable serial out   */
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
/*              SHORT   desired_temp    Target temp in degrees C            */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS load_desired_temp (HCCD ccd_handle, LONG desired_temp)
{
    USHORT tmpval, base, targ;
    USHORT index = ccd_handle - 1;
    LONG cal, scale;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* check target temp for valid value */

    if ((desired_temp < MIN_TEMP) || (desired_temp > MAX_TEMP)) {
        camdata[index].error |= CCD_ERR_PARM;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_PARM;       /* reset camdata[index].error variable       */


    /* compute target temperature */

    cal  = camdata[index].temp_cal;
    scale= camdata[index].temp_scale;
    targ = (USHORT) (cal + (desired_temp * scale) / FP_SCALE);


    /* output new value */

    tmpval = camdata[index].reg[R05];   /* get current register contents    */
    tmpval &= ~CCD_R_DAC;               /* clear current temp value         */
    tmpval |= (targ & CCD_R_DAC);       /* OR in the new value              */
    outpw(base + CCD_REG05,tmpval);     /* and write it back to the port    */
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

STATUS load_horizontal_binning (HCCD ccd_handle, SHORT hbin)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();
    CHKSIZE();

    /* check hbin factor for valid value */

    if ((hbin == 0) || (hbin > MAX_HBIN)) {
        camdata[index].error |= CCD_ERR_HBIN;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_HBIN;       /* reset camdata[index].error variable       */

    hbin--;                             /* adjust value for controller      */

    /* output new value */

    tmpval = camdata[index].reg[R06];   /* get current register contents    */
    tmpval &= ~CCD_R_HBIN;              /* clear current hbin value         */
    tmpval |= ((hbin << 12) & 0x7000);  /* OR in the new value              */
    outpw(base + CCD_REG06,tmpval);     /* and write it back to the port    */
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

STATUS load_line_count (HCCD ccd_handle, SHORT line_count)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();
    CHKSIZE();


    /* check line count value */

    if (line_count > MAX_ROWS) {
        camdata[index].error |= CCD_ERR_OFF;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_OFF;        /* reset camdata[index].error variable       */


    /* output new value */

    tmpval = camdata[index].reg[R07];   /* get current register contents    */
    tmpval &= ~CCD_R_LINE;              /* clear current line count value   */
    tmpval |= (line_count & 0x0fff);    /* OR in the new value              */
#if 0
printk ("LC = 0x%x\n", tmpval);
#endif
    outpw(base + CCD_REG07,tmpval);     /* and write it back to the port    */
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

STATUS load_mode_bits (HCCD ccd_handle, SHORT mode)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* output new value */

    tmpval = camdata[index].reg[R07];   /* get current register contents    */
    tmpval &= ~CCD_R_MODE;              /* clear current mode value         */
    tmpval |= ((mode << 12) & 0xf000);  /* OR in the new value              */
    outpw(base + CCD_REG07,tmpval);     /* and write it back to the port    */
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

STATUS load_pixel_count (HCCD ccd_handle, SHORT pixel_count)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();
    CHKSIZE();


    /* check for valid pixel count */

    if ((SHORT) pixel_count > MAX_COLS/(camdata[index].hbin + 1)) {
        camdata[index].error |= CCD_ERR_SIZE;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_SIZE;


    /* output new value */

    tmpval = camdata[index].reg[R06];   /* get current register contents    */
    tmpval &= ~CCD_R_PIXEL;             /* clear current pixel count value  */
    tmpval |= (pixel_count & 0x0fff);   /* OR in the new value              */
    outpw(base + CCD_REG06,tmpval);     /* and write it back to the port    */
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

STATUS load_test_bits (HCCD ccd_handle, SHORT test)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* output new value */

    tmpval = camdata[index].reg[R08];   /* get current register contents    */
    tmpval &= ~CCD_R_TEST;              /* clear current test value         */
    tmpval |= ((test << 12) & 0xf000);  /* OR in the new value              */
    outpw(base + CCD_REG08,tmpval);     /* and write it back to the port    */
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

STATUS load_timer_and_vbinning (HCCD ccd_handle, LONG  timer, SHORT vbin)
{
    USHORT tmpval1, tmpval2, tmpval3, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();
    CHKSIZE();


    /* check vbin value */

    if ((vbin == 0) || (vbin > MAX_VBIN)) {
        camdata[index].error |= CCD_ERR_VBIN;
        return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_VBIN;

    tmpval1  = camdata[index].reg[R03];         /* get previous reg. value  */
    tmpval1 &= ~CCD_R_VBIN;                     /* clear old vbin bits      */
    tmpval1 |= (vbin << 8);                     /* put in new vbin bits     */
    outpw(base + CCD_REG03,tmpval1);            /* write back to camera     */
    camdata[index].reg[R03] = tmpval1;          /* save new reg. value      */

    tmpval3 = inpw(base + CCD_REG12);           /* get current CCD status   */

    outpw(base+CCD_REG01, tmpval3 | CCD_BIT_TIMER);

    tmpval1 = (USHORT) timer & 0x0000ffff;      /* low 16-bits of timer     */

    tmpval2  = camdata[index].reg[R03];         /* get prev. reg. value     */
    tmpval2 &= 0xff00;                          /* preserve vbin register   */
    tmpval2 |= (USHORT) (timer >> 16);          /* set high 4 bits          */

    outpw(base + CCD_REG02,tmpval1);            /* write low 16-bits        */
    outpw(base + CCD_REG03,tmpval2);            /* write high 4 bits        */

#if 0
    printk ("xx2: 0x%04x  xx4: 0x%04x\n", tmpval1, tmpval2);
#endif

    outpw(base+CCD_REG01, tmpval3 & ~CCD_BIT_TIMER);

    camdata[index].reg[R02] = tmpval1;          /* save new reg. value      */
    camdata[index].reg[R03] = tmpval2;          /* save new reg. value      */
    camdata[index].reg[R12] = tmpval3;
    camdata[index].reg[R01] = camdata[index].reg[R12];


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

STATUS poll_command_acknowledge (HCCD ccd_handle, SHORT timeout)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;
    time_t tstrt, tend;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* immediate test and return if timeout = NOWAIT */

    if (timeout == NOWAIT) {
        tmpval = inpw(base + CCD_REG11);        /* get register contents    */
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
        tmpval = inpw(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_ACK)               /* check for true condition */
            return TRUE;
	schedule();				/* hog, albeit politely     */
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

STATUS poll_frame_done (HCCD ccd_handle, SHORT timeout)
{
#define	POLLJIF	5				/* jiffies for each loop */
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;
    int to;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();

    /* allow NOWAIT to be anything (even tho we know it's 0) */

    if (timeout == NOWAIT)
	timeout = 0;

    /* poll for frame done, up to timeout seconds */

    for (to = 0; to <= (int)timeout*HZ; to += POLLJIF) {
        tmpval = inpw(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_FD)		/* test the frame done bit  */
	    return TRUE;
	ap_pause (POLLJIF, index);		/* wait */
    }

    printk ("apogee[%d]: frame_done timeout\n", index);
    return FALSE;                               /* exit here if we timed out*/
#undef	POLLJIF
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

STATUS poll_got_trigger (HCCD ccd_handle, SHORT timeout)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;
    time_t tstrt, tend;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* immediate test and return if timeout = NOWAIT */

    if (timeout == NOWAIT) {
        tmpval = inpw(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_TRG)               /* test the cmd ack bit     */
            return TRUE;
        else
            return FALSE;
        }


    /* do a timed poll of the target bit */

    time(&tstrt);                               /* get current time, in secs*/
    time(&tend);

    while ((tend-tstrt) <= (time_t) timeout) {  /* while delta < timeout    */
        tmpval = inpw(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_TRG)               /* check for true condition */
            return TRUE;
	schedule();				/* hog, albeit politely     */
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

STATUS poll_line_done (HCCD ccd_handle, SHORT timeout)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;
    time_t tstrt, tend;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* immediate test and return if timeout = NOWAIT */

    if (timeout == NOWAIT) {
        tmpval = inpw(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_LN)                /* test the cmd ack bit     */
            return TRUE;
        else {
	    printk ("apogee[%d]: line_done timeout\n", index);
            return FALSE;
	    }
        }


    /* do a timed poll of the target bit */

    time(&tstrt);                               /* get current time, in secs*/
    time(&tend);

    while ((tend-tstrt) <= (time_t) timeout) {  /* while delta < timeout    */
        tmpval = inpw(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_LN)                /* check for true condition */
            return TRUE;
	schedule();				/* hog, albeit politely     */
        time(&tend);                            /* update interval end time */
        }


    printk ("apogee[%d]: line_done timeout\n", index);
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

STATUS poll_cache_read_ok (HCCD ccd_handle, SHORT timeout)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;
    time_t tstrt, tend;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* immediate test and return if timeout = NOWAIT */

    if (timeout == NOWAIT) {
        tmpval = inpw(base + CCD_REG11);        /* get register contents    */
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
        tmpval = inpw(base + CCD_REG11);        /* get register contents    */
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

STATUS poll_temp_status (HCCD ccd_handle, PSHORT tmpcode)
{
    USHORT tmpval1, tmpval2, tmpval3, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();

    tmpval1 = tmpval2 = tmpval3 = 0;

    tmpval1 = inpw(base + CCD_REG12);
    camdata[index].reg[R12] = tmpval1;

    tmpval2 = (tmpval1 >> 10) & 0x0020; /* got cooler bit           */
    tmpval3 |= tmpval2;

    tmpval2 = (tmpval1 >> 4) & 0x0010;  /* got shutdown bit         */
    tmpval3 |= tmpval2;

    tmpval1 = inpw(base + CCD_REG11);
    camdata[index].reg[R11] = tmpval1;

#if 0
    printk ("xx6: 0x%04x  xx8: 0x%04x\n", tmpval1, camdata[index].reg[R12]);
#endif

    tmpval2 = (tmpval1 >> 3) & 0x0008;  /* got shutdown comp bit    */
    tmpval3 |= tmpval2;

    tmpval2 = (tmpval1 >> 5) & 0x0004;  /* got at temp bit          */
    tmpval3 |= tmpval2;

    tmpval2 = (tmpval1 >> 4) & 0x0002;  /* got temp max bit         */
    tmpval3 |= tmpval2;

    tmpval2 = (tmpval1 >> 4) & 0x0001;  /* got temp min bit         */
    tmpval3 |= tmpval2;

    *tmpcode = tmpval3;     /* and we're done */


    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* poll_timer_status                                                        */
/*                                                                          */
/*                                                                          */
/* Type       : Polling function                                            */
/*                                                                          */
/* Description: Polls the exposure done bit, which is TRUE when the timer   */
/*              counts down to zero.                                        */
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

STATUS poll_timer_status (HCCD ccd_handle, SHORT timeout)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;
    time_t tstrt, tend;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* immediate test and return if timeout = NOWAIT */

    if (timeout == NOWAIT) {
        tmpval = inpw(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_EXP)               /* test the cmd ack bit     */
            return TRUE;
        else
            return FALSE;
        }


    /* do a timed poll of the target bit */

    time(&tstrt);                               /* get current time, in secs*/
    time(&tend);

    while ((tend-tstrt) <= (time_t) timeout) {  /* while delta < timeout    */
        tmpval = inpw(base + CCD_REG11);        /* get register contents    */
        camdata[index].reg[R11] = tmpval;       /* save register in mirror  */
        if (tmpval & CCD_BIT_EXP)               /* check for true condition */
            return TRUE;
	schedule();				/* hog, albeit politely     */
        time(&tend);                            /* update interval end time */
        }


    return FALSE;                               /* exit here if we timed out*/
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
/* Returns    : Temperature value from CCD A/D.                             */
/*                                                                          */
/*--------------------------------------------------------------------------*/

LONG read_temp (HCCD ccd_handle)
{
    USHORT base;
    USHORT index = ccd_handle - 1;
    LONG raw, cal, scale;

    base = camdata[index].base;

    raw = inpw(base + CCD_REG10) & 0x00ff;
    camdata[index].reg[R10] = raw;

    cal  = camdata[index].temp_cal;
    scale= camdata[index].temp_scale;

#if 0
    printk ("raw = %d cooked = %d\n", raw, (raw - cal) * FP_SCALE / scale);
#endif

    return ((raw - cal) * FP_SCALE / scale);
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

STATUS reset_system (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status   */
    tmpval  = inpw(base + CCD_REG12);

    /* preserve cooler enable bit */
    tmpval &= CCD_BIT_COOLER;
    outpw(base+CCD_REG01, tmpval);
    camdata[index].reg[R01] = camdata[index].reg[R12] = tmpval;

    /* toggle control bit -- twice!! */

    outpw(base+CCD_REG01, tmpval | CCD_BIT_RESET);
    outpw(base+CCD_REG01, tmpval & ~CCD_BIT_RESET);
    outpw(base+CCD_REG01, tmpval | CCD_BIT_RESET);
    outpw(base+CCD_REG01, tmpval & ~CCD_BIT_RESET);

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

STATUS start_flushing (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status   */

    tmpval = inpw(base + CCD_REG12);
    camdata[index].reg[R12] = tmpval;
    camdata[index].reg[R01] = camdata[index].reg[R12];


    /* toggle control bit */

    outpw(base+CCD_REG01, tmpval | CCD_BIT_FLUSH);
    outpw(base+CCD_REG01, tmpval & ~CCD_BIT_FLUSH);


    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* start_flushingL                                                          */
/*                                                                          */
/*                                                                          */
/* Type       : System control function                                     */
/*                                                                          */
/* Description: Toggles the flushL start bit                                */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS start_flushingL (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status   */

    tmpval = inpw(base + CCD_REG12);
    camdata[index].reg[R12] = tmpval;
    camdata[index].reg[R01] = camdata[index].reg[R12];


    /* toggle control bit */

    outpw(base+CCD_REG01, tmpval | CCD_BIT_FLUSHL);
    outpw(base+CCD_REG01, tmpval & ~CCD_BIT_FLUSHL);


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

STATUS start_next_line (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status   */

    tmpval = inpw(base + CCD_REG12);
    camdata[index].reg[R12] = tmpval;
    camdata[index].reg[R01] = camdata[index].reg[R12];


    /* toggle control bit */

    outpw(base+CCD_REG01, tmpval | CCD_BIT_NEXT);
    outpw(base+CCD_REG01, tmpval & ~CCD_BIT_NEXT);


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

STATUS start_timer_offset (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status   */

    tmpval = inpw(base + CCD_REG12);
    camdata[index].reg[R12] = tmpval;
    camdata[index].reg[R01] = camdata[index].reg[R12];


    /* toggle control bit */

    outpw(base+CCD_REG01, tmpval | CCD_BIT_TMSTRT);
    outpw(base+CCD_REG01, tmpval & ~CCD_BIT_TMSTRT);


    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* start_timer                                                              */
/*                                                                          */
/*                                                                          */
/* Type       : System control function                                     */
/*                                                                          */
/* Description: Toggles the timer start bit                                 */
/*              This controls the "timer start without offset" bit.         */
/*                                                                          */
/* Inputs     : HCCD    ccd_handle      Handle to valid CCD channel         */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS start_timer (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status   */

    tmpval = inpw(base + CCD_REG12);
    camdata[index].reg[R12] = tmpval;
    camdata[index].reg[R01] = camdata[index].reg[R12];


    /* toggle control bit */

    outpw(base+CCD_REG01, tmpval | CCD_BIT_TMGO);
    outpw(base+CCD_REG01, tmpval & ~CCD_BIT_TMGO);


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

STATUS stop_flushing (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();

    /* get current CCD status   */

    tmpval = inpw(base + CCD_REG12);
    camdata[index].reg[R12] = tmpval;
    camdata[index].reg[R01] = camdata[index].reg[R12];


    /* toggle control bit */

    outpw(base+CCD_REG01, tmpval | CCD_BIT_NOFLUSH);
    outpw(base+CCD_REG01, tmpval & ~CCD_BIT_NOFLUSH);


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

STATUS assert_done_reading (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status   */

    tmpval = inpw(base + CCD_REG12);
    camdata[index].reg[R12] = tmpval;
    camdata[index].reg[R01] = camdata[index].reg[R12];


    /* toggle control bit */

    outpw(base+CCD_REG01, tmpval | CCD_BIT_RDONE);
    outpw(base+CCD_REG01, tmpval & ~CCD_BIT_RDONE);


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

STATUS set_cable_length (HCCD ccd_handle, SHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status */

    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_CABLE;


    /* modify bit per control parameter */

    if (bitval)
        tmpval |= CCD_BIT_CABLE;


    /* write change back to control register */

    outpw(base+CCD_REG01,tmpval);
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

STATUS set_fifo_caching (HCCD ccd_handle, SHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status */

    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_CACHE;


    /* modify bit per control parameter */

    if (bitval)
        tmpval |= CCD_BIT_CACHE;


    /* write change back to control register */

    outpw(base+CCD_REG01,tmpval);
    camdata[index].reg[R01] = tmpval;


    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* set_output_port                                                          */
/*                                                                          */
/*                                                                          */
/* Type       : Bit set/clear function                                      */
/*                                                                          */
/* Description: Sets a specific bit on the aux. output port to the value    */
/*              passed in the parameter state.                              */
/*                                                                          */
/* Inputs:      HCCD ccd_handle         Handle to valid CCD channel         */
/*              USHORT bit              Target bit to modify, 0 to 7        */
/*              USHORT state            Target output state (1 or 0)        */
/*                                                                          */
/* Returns    : CCD_OK or CCD_ERROR if failure                              */
/*              Check camdata[index].error variable for failure cause       */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS set_output_port (HCCD ccd_handle, SHORT bit, SHORT state)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;
    USHORT outbit;

    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status */

    tmpval = camdata[index].reg[R05];

    /* modify bit per control parameter */

    outbit = 0;
    outbit = (1 << (bit + 8)) & CCD_R_RELAY;

    if (state == ON)
        tmpval |= outbit;               /* OR in bit to enable output       */
    else
        tmpval &= ~outbit;              /* turn off, leave DAC bits alone   */

    /* write change back to control register */

    outpw(base+CCD_REG05,tmpval);
    camdata[index].reg[R05] = tmpval;

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* set_shutter                                                              */
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

STATUS set_shutter (HCCD ccd_handle, SHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status */

    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_SHUTTER;


    /* modify bit per control parameter */

    if (bitval)
        tmpval |= CCD_BIT_SHUTTER;


    /* write change back to control register */

    outpw(base+CCD_REG01,tmpval);
    camdata[index].reg[R01] = tmpval;


    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* set_shutter_open                                                         */
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

STATUS set_shutter_open (HCCD ccd_handle, SHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status */

    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_OVERRD;


    /* modify bit per control parameter */

    if (bitval)
        tmpval |= CCD_BIT_OVERRD;


    /* write change back to control register */

    outpw(base+CCD_REG01,tmpval);
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

STATUS set_temp_val (HCCD ccd_handle)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status */

    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_COOLER;


    /* set bit ON */

    tmpval |= CCD_BIT_COOLER;


    /* write change back to control register */

    outpw(base+CCD_REG01,tmpval);
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

STATUS set_temp_shutdown (HCCD ccd_handle, SHORT bitval)
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

    outpw(base+CCD_REG01,tmpval);
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

STATUS set_cooler_enable (HCCD ccd_handle, SHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status */

    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_COOLER;


    /* modify bit per control parameter */

    if (bitval)
        tmpval |= CCD_BIT_COOLER;


    /* write change back to control register */

    outpw(base+CCD_REG01,tmpval);
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

STATUS set_trigger (HCCD ccd_handle, SHORT bitval)
{
    USHORT tmpval, base;
    USHORT index = ccd_handle - 1;


    /* check handle and get base address */

    CHKHCCD();
    CHKBASE();


    /* get current CCD status */

    tmpval = camdata[index].reg[R01];
    tmpval &= CCD_MSK_TRIGGER;


    /* modify bit per control parameter */

    if (bitval)
        tmpval |= CCD_BIT_TRIGGER;


    /* write change back to control register */

    outpw(base+CCD_REG01,tmpval);
    camdata[index].reg[R01] = tmpval;

    return CCD_OK;
}



/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: aplow.c,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $"};
