/*==========================================================================*/
/* Apogee CCD Control API - High Level Access Functions                     */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   12/22/96    jmh     Created new API library files               */
/*   1.10    3/17/97    jmh     Mod. to allow selective use of flushing     */
/*   1.20    6/23/97    ecd     changes for linux                           */
/*   1.21    9/5/97     ecd     add ap_impact				    */
/*                                                                          */
/*==========================================================================*/

#include "apccd.h"
#include "apglob.h"
#include "apdata.h"
#include "aplow.h"

#include "aplinux.h"

/*--------------------------------------------------------------------------*/
/* check_parms                                                              */
/*                                                                          */
/* verify camera handle and control block. returns either CCD_OK or         */
/* CCD_ERROR.                                                               */
/*--------------------------------------------------------------------------*/

STATUS check_parms(HCCD ccd_handle)
{
    if ((ccd_handle < 1) || (ccd_handle > MAXCAMS)) {
        camdata[ccd_handle-1].error |= CCD_ERR_HCCD;
        return CCD_ERROR;
        }

    if (check_ccd(ccd_handle) != CCD_OK) {
        camdata[ccd_handle-1].error |= CCD_ERR_HCCD;
        return CCD_ERROR;
        }

    camdata[ccd_handle-1].error &= ~CCD_ERR_HCCD;

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* ccd_command                                                              */
/*                                                                          */
/* initiate a command operation in the controller. Optionally block until   */
/* the controller acknowledges the command.                                 */
/*--------------------------------------------------------------------------*/

STATUS ccd_command(HCCD ccd_handle, SHORT cmdcode, SHORT block, SHORT timeout)
{
    USHORT index = ccd_handle - 1;
    STATUS rc;

    cam_current = ccd_handle;

    if ((rc = check_parms(ccd_handle)) != CCD_OK) {
        cam_current = 0;
        return rc;
        }

    /* set wait time for cmd ack and call command function */

    switch (cmdcode) {
        case CCD_CMD_RSTSYS:
            reset_system(ccd_handle);
            break;
        case CCD_CMD_FLSTRT:
            start_flushing(ccd_handle);
            break;
        case CCD_CMD_FLSTOP:
            stop_flushing(ccd_handle);
            break;
        case CCD_CMD_LNSTRT:
            start_next_line(ccd_handle);
            break;
        case CCD_CMD_TMSTRT:
            start_timer_offset(ccd_handle);
            break;
        default:
            camdata[index].error |= CCD_ERR_PARM;
            cam_current = 0;
            return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_PARM;


    /* wait for cmd ack if block is TRUE */

    if (block == TRUE)
        if (poll_command_acknowledge(ccd_handle,timeout) != TRUE) {
            cam_current = 0;
            return CCD_ERROR;
            }

    cam_current = 0;
    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* load_ccd_data                                                            */
/*                                                                          */
/* transfer data from a CCD control block to a specific controller card.    */
/*                                                                          */
/* if mode = FLUSH then line count is entire CCD for flushing, else if      */
/* mode = EXPOSE then line count is bic                                     */
/*                                                                          */
/* if an error is returned and camdata[index].error is OK, then a timeout occurred   */
/*                                                                          */
/*--------------------------------------------------------------------------*/

STATUS load_ccd_data (HCCD ccd_handle, USHORT mode)
{
    USHORT index = ccd_handle-1;
    STATUS rc;
    SHORT  calcaic, calcair;
    SHORT  mod1, mod2;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    cam_current = ccd_handle;

    /* adjust aic if nescessary */

    calcaic = camdata[index].cols-camdata[index].bic-camdata[index].imgcols;

    if (camdata[index].aic != calcaic) {        /* see if they're the same  */
        camdata[index].aic = calcaic;           /* use calc'd if not        */
        }

    /* adjust air if nescessary */

    calcair = camdata[index].rows-camdata[index].bir-camdata[index].imgrows;

    if (camdata[index].air != calcair) {        /* see if they're the same  */
        camdata[index].air = calcair;           /* use calc'd if not        */
        }

    /* set up geometry for the read */

    mod1 = camdata[index].imgcols % camdata[index].hbin;
    mod2 = camdata[index].imgrows % camdata[index].vbin;
    camdata[index].colcnt = camdata[index].imgcols/camdata[index].hbin;
    camdata[index].rowcnt = camdata[index].imgrows/camdata[index].vbin;
    if (mod1) camdata[index].colcnt += 1;       /* round up if nesc.        */
    if (mod2) camdata[index].rowcnt += 1;

    /* transfer data from control block to controller card */

    rc = set_cable_length (ccd_handle,camdata[index].cable);
    if (rc != CCD_OK) {
        cam_current = 0;
        return CCD_ERROR;
        }

    rc = load_bic_count (ccd_handle,camdata[index].bic);
    if (rc != CCD_OK) {
        cam_current = 0;
        return CCD_ERROR;
        }

    rc = load_pixel_count (ccd_handle,camdata[index].colcnt);
    if (rc != CCD_OK) {
        cam_current = 0;
        return CCD_ERROR;
        }

    rc = load_aic_count (ccd_handle,calcaic);
    if (rc != CCD_OK) {
        cam_current = 0;
        return CCD_ERROR;
        }

    rc = load_horizontal_binning (ccd_handle,MAX_HBIN);
    if (rc != CCD_OK) {
        cam_current = 0;
        return CCD_ERROR;
        }

    rc = load_timer_and_vbinning (ccd_handle,camdata[index].timer,MAX_VBIN);
    if (rc != CCD_OK) {
        cam_current = 0;
        return CCD_ERROR;
        }

    /* YUK! Needed this to work with cheapy generic motherboard.
     * without it, the following commands would start a cooler shutdown!
     */
    ap_pause(5, index);

    if (mode == FLUSH) {
        rc = load_line_count (ccd_handle,camdata[index].rows);
        if (rc != CCD_OK) {
            cam_current = 0;
            return CCD_ERROR;
            }

        /* reset and flush the controller if in FLUSH mode */

        if (ccd_command(ccd_handle,CCD_CMD_RSTSYS,TRUE,5) != CCD_OK) {
            cam_current = 0;
            return CCD_ERROR;
            }

        if (ccd_command(ccd_handle,CCD_CMD_FLSTRT,TRUE,5) != CCD_OK) {
            cam_current = 0;
            return CCD_ERROR;
            }
        }
    else {  /* assume EXPOSE */
        rc = load_line_count (ccd_handle,camdata[index].bir);
        if (rc != CCD_OK) {
            cam_current = 0;
            return CCD_ERROR;
            }
        }

    /* Need this one too */
    ap_pause(5, index);

    cam_current = 0;
    return CCD_OK;
}

/*--------------------------------------------------------------------------*/
/* config_camera                                                            */
/*                                                                          */
/* load camera configuration data into the control data block. Resets and   */
/* flushes the controller so new parameters will take effect. may be used   */
/* in one of three modes:                                                   */
/*                                                                          */
/* this function may be used to reload the config file if 'use_config' is   */
/* true and all other input parameters are set to IGNORE. It may also be    */
/* used to force a reload of the controller parameters by setting the input */
/* parameters to IGNORE and 'use_config' to false.                          */
/*--------------------------------------------------------------------------*/

STATUS config_camera (HCCD  ccd_handle,
                      SHORT rows,
                      SHORT columns,
                      SHORT bir_count,
                      SHORT bic_count,
                      SHORT image_rows,
                      SHORT image_cols,
                      SHORT hbin,
                      SHORT vbin,
                      LONG  exp_time,
                      SHORT cable_length,
                      SHORT use_config)
{
    USHORT index = ccd_handle - 1;
    STATUS rc;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    cam_current = ccd_handle;

    /* see if caller wants to reload the config file */

    /* see if caller wants to change a parameter */

    if (rows != IGNORE)
        camdata[index].rows     = rows;

    if (columns != IGNORE)
        camdata[index].cols     = columns;

    if (bir_count != IGNORE)
        camdata[index].bir      = bir_count;

    if (bic_count != IGNORE)
        camdata[index].bic      = bic_count;

    if (image_rows != IGNORE)
        camdata[index].imgrows  = image_rows;

    if (image_cols != IGNORE)
        camdata[index].imgcols  = image_cols;

    if (hbin != IGNORE)
        camdata[index].hbin     = hbin;

    if (vbin != IGNORE)
        camdata[index].vbin     = vbin;

    if (exp_time != IGNORE)
        camdata[index].timer    = exp_time;

    if (cable_length != IGNORE)
        camdata[index].cable    = cable_length;

    /* always do a load, reset, and flush sequence */

    if (load_ccd_data(ccd_handle, FLUSH) != CCD_OK) {
        cam_current = 0;
        return CCD_ERROR;
        }

    cam_current = 0;
    return CCD_OK;
}

/*--------------------------------------------------------------------------*/
/* start_exposure                                                           */
/*                                                                          */
/* begins an exposure from a specified camera/controller. Optionally sets   */
/* the shutter and trigger operation modes.                                 */
/*--------------------------------------------------------------------------*/

STATUS start_exposure (HCCD   ccd_handle,
                       SHORT  shutter_en,
                       SHORT  trigger_mode,
                       SHORT  block,
                       SHORT  flushwait )
{
    SHORT  mod1, mod2;
    USHORT index = ccd_handle - 1;
    STATUS rc;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    cam_current = ccd_handle;

    /* set up geometry for the read */

    mod1 = camdata[index].imgcols % camdata[index].hbin;
    mod2 = camdata[index].imgrows % camdata[index].vbin;
    camdata[index].colcnt = camdata[index].imgcols/camdata[index].hbin;
    camdata[index].rowcnt = camdata[index].imgrows/camdata[index].vbin;
    if (mod1) camdata[index].colcnt += 1;       /* round up if nesc.        */
    if (mod2) camdata[index].rowcnt += 1;

    /* check for previous frame completion and stop flush when it's done */

    if (flushwait == TRUE)
        if (poll_frame_done(ccd_handle,20) != TRUE) {
            cam_current = 0;
            return CCD_ERROR;
            }

    stop_flushing(ccd_handle);

    rc = load_ccd_data(ccd_handle,EXPOSE);
    if (rc != CCD_OK) {
        cam_current = 0;
        return CCD_ERROR;
        }

    /* set the shutter and trigger modes */

    set_shutter_open (ccd_handle, FALSE);

    if (shutter_en == IGNORE)
        rc = set_shutter(ccd_handle,camdata[index].shutter_en);
    else
        rc = set_shutter(ccd_handle,shutter_en);

    if (trigger_mode == IGNORE)
        rc = set_trigger(ccd_handle,camdata[index].trigger_mode);
    else
        rc = set_trigger(ccd_handle,trigger_mode);

    if (rc != CCD_OK) {
        cam_current = 0;
        return rc;
        }

    /* start the exposure */

    cam_current = 0;
    return (ccd_command(ccd_handle, CCD_CMD_TMSTRT, block, 5));
}


/*--------------------------------------------------------------------------*/
/* check_line                                                               */
/*                                                                          */
/* tests the line done bit. Returns CCD_OK if it's set, otherwise returns   */
/* CCD_NOTRDY.                                                              */
/*--------------------------------------------------------------------------*/

STATUS check_line (HCCD ccd_handle)
{
    STATUS rc;

    cam_current = ccd_handle;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    if (poll_line_done(ccd_handle,NOWAIT) == TRUE) {
        cam_current = 0;
        return CCD_OK;
        }

    cam_current = 0;
    return CCD_NOTRDY;
}


/*--------------------------------------------------------------------------*/
/* check_image                                                              */
/*                                                                          */
/* test the frame done bit. Returns CCD_OK if it's set, otherwise return    */
/* CCD_NOTRDY.                                                              */
/*--------------------------------------------------------------------------*/

STATUS check_image (HCCD ccd_handle)
{
    STATUS rc;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    cam_current = ccd_handle;

    if (poll_frame_done(ccd_handle,NOWAIT) == TRUE) {
        cam_current = 0;
        return CCD_OK;
        }

    cam_current = 0;
    return CCD_NOTRDY;
}


/*--------------------------------------------------------------------------*/
/* acquire_image                                                            */
/*                                                                          */
/* offload image from CCD to caller-specified memory buffer. this function  */
/* must not be interrupted in order to maintain read timing.                */
/*--------------------------------------------------------------------------*/

STATUS acquire_image (HCCD   ccd_handle,
                      SHORT  caching,
                      PIMAGE buffer,
                      SHORT  airwait )
{
    STATUS rc;
    SHORT  i;
    USHORT index = ccd_handle - 1;
    PIMAGE outbuf;
    SHORT rowcnt, colcnt;
    int ourcols;
    CAMDATA *cdp = &camdata[index];
    int base = cdp->base;


    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    cam_current = ccd_handle;

    if (buffer == (PIMAGE) NULL) {
        camdata[index].error |= CCD_ERR_BUFF;
        cam_current = 0;
        return CCD_ERROR;
        }

    /* make sure CCD's in the correct state */

    if (poll_frame_done(ccd_handle,15) != TRUE) {  /* check for BIR done */
	printk ("apogee[%d]: initial frame done\n", index);
        camdata[index].error |= CCD_ERR_REG;
        cam_current = 0;
        return CCD_ERROR;
        }

    if (poll_line_done(ccd_handle,10) != TRUE) {
	printk ("apogee[%d]: initial line done\n", index);
        camdata[index].error |= CCD_ERR_REG;
        cam_current = 0;
        return CCD_ERROR;
        }

    /* faster access */

    rowcnt = camdata[index].rowcnt;
    colcnt = camdata[index].colcnt;

    /* N.B. the linux interface assumes binning rounds down although the
     *   Apogee firmware rounds up. So, we carefully guard outbuf to stay
     *   on the expected image dimensions.
     * N.B. no need to check for too many rows because we toss one away and
     *   the most round-down differs from round-up is 1.
     */

    ourcols = camdata[index].imgcols/camdata[index].hbin;

    /* N.B. if have slave read into each half row. */

    if (ap_slbase) {
	colcnt /= 2;
	ourcols /=2;
    }

    /* init user output buffer */

    outbuf = buffer;

    /* load real hbinning */

    load_horizontal_binning (ccd_handle,camdata[index].hbin);

    /* read 1 row of data after MAX_VBIN to align with image start */

    if ((i = camdata[index].bir % MAX_VBIN) != 0) {
	load_timer_and_vbinning (ccd_handle, camdata[index].timer, i);
	ccd_command(ccd_handle, CCD_CMD_LNSTRT, BLOCK, 5);
	if (poll_line_done(ccd_handle,10) != TRUE) {
	    printk ("apogee[%d]: BIR line done\n", index);
	    camdata[index].error |= CCD_ERR_REG;
	    cam_current = 0;
	    return CCD_ERROR;
	    }
	readu_data (base, cdp, outbuf, colcnt, ourcols);
	if (ap_slbase)
	    readu_data (ap_slbase, cdp, outbuf, colcnt, ourcols);
    }

    /* load real vbinning */

    load_timer_and_vbinning (ccd_handle, camdata[index].timer,
    							camdata[index].vbin);

    /* now read out the rest of the ROI data using cached or uncached modes */
    /* note that i is started at 1 to account for row already read above */

    if (camdata[index].caching || caching) {
        set_fifo_caching(ccd_handle,TRUE);
        for (i = 1; i < rowcnt; i++) {
            ccd_command(ccd_handle, CCD_CMD_LNSTRT, BLOCK, 5);
	    readu_data (base, cdp, outbuf, colcnt, ourcols);
	    outbuf += ourcols;
	    if (ap_slbase) {
		readu_data (ap_slbase, cdp, outbuf, colcnt, ourcols);
		outbuf += ourcols;
	        }
            assert_done_reading(ccd_handle);
            if (poll_line_done(ccd_handle,10) != TRUE) {
		printk ("apogee[%d]: caching line done row %d\n", index, i);
                camdata[index].error |= CCD_ERR_REG;
                cam_current = 0;
		set_fifo_caching(ccd_handle,FALSE);
                return CCD_ERROR;
                }
            camdata[index].error &= ~CCD_ERR_REG;
	    if ((i % ap_impact) == 0)
		schedule(); /* still hog, but politely */
            }
	set_fifo_caching(ccd_handle,FALSE);
        }
    else {
        for (i = 1; i < rowcnt; i++) {
            ccd_command(ccd_handle, CCD_CMD_LNSTRT, BLOCK, 5);
            if (poll_line_done(ccd_handle,10) != TRUE) {
		printk ("apogee[%d]: noncaching line done row %d\n", index, i);
                camdata[index].error |= CCD_ERR_REG;
                cam_current = 0;
                return CCD_ERROR;
                }
            camdata[index].error &= ~CCD_ERR_REG;
	    readu_data (base, cdp, outbuf, colcnt, ourcols);
	    outbuf += ourcols;
	    if (ap_slbase) {
		readu_data (ap_slbase, cdp, outbuf, colcnt, ourcols);
		outbuf += ourcols;
	        }
	    if ((i % ap_impact) == 0)
		schedule(); /* still hog, but politely */
            }
        }

    /* flush remainder of CCD rows */

    load_horizontal_binning (ccd_handle, MAX_HBIN);
    load_timer_and_vbinning (ccd_handle, camdata[index].timer, MAX_VBIN);
    flush_air (ccd_handle, airwait);

    cam_current = 0;
    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* flush_air                                                                */
/*                                                                          */
/* flushes the remaining rows after the ROI on the CCD. should be called    */
/* after data is read from the CCD.                                         */
/*--------------------------------------------------------------------------*/

STATUS flush_air (HCCD ccd_handle, SHORT airwait)
{
    STATUS rc;
    USHORT index = ccd_handle - 1;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    cam_current = ccd_handle;

    load_line_count(ccd_handle,camdata[index].air);
    ccd_command(ccd_handle, CCD_CMD_FLSTRT, NOBLOCK, 5);

    if (airwait == TRUE) {
        if (poll_frame_done(ccd_handle,15) != TRUE) {
            cam_current = 0;
            return CCD_ERROR;
            }

        /* now start regular flushing */

        if (load_ccd_data(ccd_handle, FLUSH) != CCD_OK) {
            cam_current = 0;
            return CCD_ERROR;
            }
        }

    cam_current = 0;
    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* set_temp                                                                 */
/*                                                                          */
/* set the target temperature of the dewer. the low-level call handles the  */
/* conversion from degrees C to CCD units. the function parameter takes one */
/* of three command values: CCD_TMP_SET, CCD_TMP_AMB, or CCD_TMP_OFF.       */
/*--------------------------------------------------------------------------*/

STATUS set_temp (HCCD ccd_handle, LONG desired_temp, SHORT function)
{
    STATUS rc;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    cam_current = ccd_handle;

    load_desired_temp(ccd_handle, desired_temp);

    switch (function) {
        case CCD_TMP_SET:
            set_temp_shutdown(ccd_handle, FALSE);
            set_cooler_enable(ccd_handle, TRUE);
            break;
        case CCD_TMP_AMB:
            set_temp_shutdown(ccd_handle, TRUE);
            set_cooler_enable(ccd_handle, TRUE);
            break;
        case CCD_TMP_OFF:
            set_temp_shutdown(ccd_handle, FALSE);
            set_cooler_enable(ccd_handle, FALSE);
            break;
        default:
            cam_current = 0;
            return CCD_ERROR;
        }

    cam_current = 0;
    return CCD_OK;
}

/*--------------------------------------------------------------------------*/
/* get_temp                                                                 */
/*                                                                          */
/* returns the status and value of the dewer temperature via the pointers   */
/* temp_status and temp_read. the value of temp_read is in degrees C. the   */
/* low-level function read_temp() handles the conversion from CCD units     */
/* to centigrade.                                                           */
/*--------------------------------------------------------------------------*/

STATUS get_temp (HCCD ccd_handle,PSHORT temp_status,PLONG temp_read)
{
    STATUS rc;
    USHORT tmpstat;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    cam_current = ccd_handle;

    /* fetch the temp status bits */

    if (poll_temp_status(ccd_handle,&tmpstat) != CCD_OK) {
        cam_current = 0;
        return CCD_ERROR;
        }

    /* fetch and save the temp value, in degrees C */
    *temp_read = read_temp(ccd_handle);


    /* interpret the temp status bits and set the status code */

    /* defines don't handle the 0 bits right -- do it here manually.
     * see what the bits mean by reading poll_temp_status().
     */
    if ((0x20 & tmpstat) == 0) {
        *temp_status = CCD_TMP_OFF;
        cam_current = 0;
        return CCD_OK;
        }

    if ((0x37 & tmpstat) == 0x20) {
        *temp_status = CCD_TMP_RDN;
        cam_current = 0;
        return CCD_OK;
        }

    if ((0x37 & tmpstat) == 0x24) {
        *temp_status = CCD_TMP_OK;
        cam_current = 0;
        return CCD_OK;
        }

    if ((0x37 & tmpstat) == 0x22) {
        *temp_status = CCD_TMP_MAX;
        cam_current = 0;
        return CCD_OK;
        }

    if ((0x37 & tmpstat) == 0x21) {
        *temp_status = CCD_TMP_STUCK;
        cam_current = 0;
        return CCD_OK;
        }

    if ((0x38 & tmpstat) == 0x30) {
        *temp_status = CCD_TMP_RUP;
        cam_current = 0;
        return CCD_OK;
        }

    if ((0x38 & tmpstat) == 0x38)
        *temp_status = CCD_TMP_DONE;

    cam_current = 0;
    return CCD_OK;
}

/*--------------------------------------------------------------------------*/
/* output_port                                                              */
/*                                                                          */
/* generate outputs on the auxilliary control lines.                        */
/*--------------------------------------------------------------------------*/

STATUS output_port (HCCD ccd_handle, SHORT bits)
{
    STATUS rc;
    USHORT i;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    cam_current = ccd_handle;

    for (i = 0; i < 8; i++) {
        if (bits & (1 << i))
            set_output_port(ccd_handle,i,1);
        else
            set_output_port(ccd_handle,i,0);
        }

    cam_current = 0;
    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* shutter                                                                  */
/*                                                                          */
/* set the shutter operation mode.                                          */
/*--------------------------------------------------------------------------*/

STATUS shutter (HCCD ccd_handle, SHORT state)
{
    STATUS rc;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    cam_current = ccd_handle;

    set_shutter(ccd_handle,state);

    cam_current = 0;
    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* get_timer                                                                */
/*                                                                          */
/* get the current timer and trigger status. trigger status is only valid   */
/* if the trigger mode is CCD_TRIGEXT, otherwise it will be returned as a   */
/* zero (FALSE) value.                                                      */
/*--------------------------------------------------------------------------*/

STATUS get_timer (HCCD ccd_handle,PSHORT trigger,PSHORT timer)
{
    STATUS rc;
    USHORT index = ccd_handle - 1;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    cam_current = ccd_handle;

    if (camdata[index].trigger_mode == CCD_TRIGEXT) {
        if ((rc = poll_got_trigger(ccd_handle,30)) != TRUE) {
            cam_current = 0;
            return CCD_ERROR;
            }
        else
            *trigger = rc;
        if ((rc = poll_timer_status(ccd_handle,10)) != TRUE) {
            cam_current = 0;
            return CCD_ERROR;
            }
        else
            *timer = rc;
        }
    else {
        if ((rc = poll_timer_status(ccd_handle,30)) != TRUE) {
            cam_current = 0;
            return CCD_ERROR;
            }
        else {
            *trigger = FALSE;
            *timer = rc;
            }
        }

    cam_current = 0;
    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* loopback                                                                 */
/*                                                                          */
/* attempts to verify presence of a controller card by toggling the trigger */
/* enable bit and checking for a response.                                  */
/*                                                                          */
/* Returns CCD_ERROR if no address match was found, else CCD_OK             */
/*                                                                          */
/*--------------------------------------------------------------------------*/

USHORT loopback(USHORT base)
{
    USHORT tmpval;

    /* attempt to clear the trigger enable bit at the target address */

    tmpval = inpw(base+0x008);
    tmpval &= 0xffdf;                   /* clear trigger bit            */
    outpw(base,tmpval);                 /* write it out to register 0   */
    tmpval = inpw(base+0x008);          /* get status from register 12  */

    if (tmpval & 0x0020)
        return CCD_ERROR;

    /* attempt to set the trigger enable bit at the target address */

    tmpval = inpw(base+0x008);
    tmpval |= 0x0020;                   /* enable trigger bit           */
    outpw(base,tmpval);                 /* write it out to register 0   */
    tmpval = inpw(base+0x008);          /* get status from register 12  */

    /* now clear it again if we can set it */

    if (tmpval & 0x0020) {
        tmpval = inpw(base+0x008);
        tmpval &= 0xffdf;               /* clear trigger bit            */
        outpw(base,tmpval);             /* write it out to register 0   */
        tmpval = inpw(base+0x008);      /* get status from register 12  */
        if (tmpval & 0x0020)
            return CCD_ERROR;
        }
    else
        return CCD_ERROR;

    return CCD_OK;
}

/*--------------------------------------------------------------------------*/
/* reset_flush                                                              */
/*                                                                          */
/* starts normal camera flushing, should be called after aborting any       */
/* camera operation.                                                        */
/*--------------------------------------------------------------------------*/

STATUS
reset_flush (HCCD ccd_handle)
{
    STATUS rc;
    USHORT index = ccd_handle - 1;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    rc = load_line_count (ccd_handle,camdata[index].rows);
	if (rc != CCD_OK) {
		return CCD_ERROR;
		}

    /* reset and flush the controller */

    if (ccd_command(ccd_handle,CCD_CMD_RSTSYS,BLOCK,5) != CCD_OK) {
    	return CCD_ERROR;
    	}

    if (ccd_command(ccd_handle,CCD_CMD_FLSTRT,BLOCK,5) != CCD_OK) {
    	return CCD_ERROR;
    	}

    return CCD_OK;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: apapi.c,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $"};
