/*==========================================================================*/
/* Apogee CCD Control API - High Level Access Functions                     */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   12/22/96    jmh     Created new API library files               */
/*   2.00   03-21-2000  gkr     Integrated LINUX conditionals               */
/*                                                                          */
/*==========================================================================*/

#define ASMXFER
#define VFLUSH
#define HFLUSH

// ASM not compatible with PPI or NET interface
#if defined(_APG_PPI) || defined(_APG_NET) || defined(LINUX)
#undef ASMXFER
#endif

#ifdef _APGDLL
#include <windows.h>
#include <mmsystem.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef LINUX
#include <dos.h>
#include <conio.h>
#endif

#include "apccd.h"
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

void initialize_registers(HCCD hccd);

#ifdef WINUTIL
#include "util32.h"
DWORD dwApogeeClass = HIGH_CLASS;			// minimum priority class for acquire_image
DWORD dwApogeeThread = NORMAL_THREAD;		// minimum thread priority for acquire_image
DWORD dwSaveClass;
DWORD dwSaveThread;
DWORD dwSetClass;
DWORD dwSetThread;
BOOL bRestorePriority;
#endif

static void
SetPriority(void)
{
#ifdef WINUTIL
	bRestorePriority = Util32GetPriorityClass(&dwSaveClass) && Util32GetThreadPriority(&dwSaveThread);
	if (bRestorePriority) {
		dwSetClass = max(dwApogeeClass,dwSaveClass);
		dwSetThread = max(dwApogeeThread,dwSaveThread);
		Util32SetPriorityClass(dwSetClass);
		Util32SetThreadPriority(dwSetThread);
#ifdef _APG_NET
		net_priority(dwSetClass, dwSetThread);
#endif
	}
#endif
}

static void
RestorePriority(void)
{
#ifdef WINUTIL
	if (bRestorePriority) {
		Util32SetPriorityClass(dwSaveClass);
		Util32SetThreadPriority(dwSaveThread);
#ifdef _APG_NET
		net_priority(dwSaveClass, dwSaveThread);
#endif
	}
#endif
}

// Pixel value to store in bad lines
#define BAD_LINE			0x8000

/* global error status used by open_camera */
STATUS  APCCD_OpenErr;

// global default mode and test bits, modified by INI entries
USHORT	uDefaultMode = MODE_DEFAULT;
USHORT	uDefaultTest = TEST_DEFAULT;

// DLL Version number

STATUS DLLPROC
get_openerror ( void )
{
	return (APCCD_OpenErr);
}

#ifndef MINOR
#define MINOR	0x0043
#endif

USHORT DLLPROC
get_version ( void )
{
#ifdef LINUX
	return (0xA00 + MINOR);
#else
#ifdef WINUTIL
	return (Util32GetVersion());
#else
#ifdef WINVER
	return (0x300 + MINOR);
#else
	return (MINOR);
#endif
#endif
#endif
}

/* Interrupt mask handling */
#define PIC1	0x21
#define PIC2	0xA1

USHORT int_mask = 0;
USHORT old_mask = 0;

// user_mask
//   Apply user defined interrupt masks
static void
user_mask(void)
{
	if (!int_mask)
		return;
	old_mask = (_inp(PIC2) << 8) + _inp(PIC1);
	_outp(PIC1, _inp(PIC1) | (int_mask & 0xff));
	_outp(PIC2, _inp(PIC2) | (int_mask >> 8));
}

// user_mask
//   Remove user defined interrupt masks
static void
user_unmask(void)
{
	if (!int_mask)
		return;
	_outp(PIC1, (old_mask & 0xff));
	_outp(PIC2, (old_mask >> 8));
}

USHORT DLLPROC
set_mask(USHORT new_mask)
{
	USHORT tmp_mask = int_mask;

	int_mask = new_mask;

	return(tmp_mask);
}

/*--------------------------------------------------------------------------*/
/* activate_camera                                                          */
/*                                                                          */
/* perform any operations necessary to switch communication from one camera */
/* interface to another. Introduced to support multiple cameras using the   */
/* same base address.                                                       */
/*--------------------------------------------------------------------------*/

void
activate_camera(HCCD ccd_camera)
{
	static HCCD cam_current = -1;
	
	if (ccd_camera != cam_current) {
	    ptr_current = &(camdata[ccd_camera-1]);
#ifdef _APG_PPI
		ppi_init(ptr_current->base, ptr_current->reg_offset);
#endif
#ifdef _APG_NET
		net_init();
#endif
		cam_current = ccd_camera;
	}
}

// Calculate temperature from controller register value
static double
reg2deg (HCCD hccd, USHORT reg_val)
{
    USHORT index = hccd - 1;
    CAMDATA *pcd = &(camdata[index]);

	return((reg_val - pcd->temp_cal)/pcd->temp_scale);
}

static STATUS
send_config_data(HCCD hccd)
{
    USHORT index = hccd - 1;
    CAMDATA *pcd = &(camdata[index]);

	initialize_registers(hccd);

// send mode bits to controller
	if (load_mode_bits(hccd, pcd->mode) != CCD_OK) {
        APCCD_OpenErr = CCD_OPEN_MODE;
        return CCD_ERROR;
		}

// send test bits to controller
	if (load_test_bits(hccd, pcd->test) != CCD_OK) {
        APCCD_OpenErr = CCD_OPEN_TEST;
        return CCD_ERROR;
		}

// set cable length from INI value
   	if (set_cable_length(hccd, pcd->cable) != CCD_OK) {
        APCCD_OpenErr = CCD_OPEN_CABLE;
        return CCD_ERROR;
	    }

// set camera register from INI value
   	if (load_camera_register(hccd, pcd->camreg) != CCD_OK) {
        APCCD_OpenErr = CCD_OPEN_CAMREG;
        return CCD_ERROR;
	    }

	return (CCD_OK);
}

/*--------------------------------------------------------------------------*/
/* check_parms                          .                                   */
/*                                                                          */
/* verify camera handle and control block. returns either CCD_OK or         */
/* CCD_ERROR.                                                               */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
check_parms(HCCD ccd_handle)
{
    if ((ccd_handle < 1) || (ccd_handle > MAXCAMS))
        return CCD_ERROR;

    if (check_ccd(ccd_handle) != CCD_OK)
        return CCD_ERROR;

    camdata[ccd_handle-1].error &= ~CCD_ERR_HCCD;

    return CCD_OK;
}



/*--------------------------------------------------------------------------*/
/* ccd_command                                                              */
/*                                                                          */
/* initiate a command operation in the controller. Optionally block until   */
/* the controller acknowledges the command.                                 */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
ccd_command(HCCD ccd_handle, USHORT cmdcode, USHORT block, USHORT timeout)
{
    USHORT index = ccd_handle - 1;
    STATUS rc;

    if ((rc = check_parms(ccd_handle)) != CCD_OK) {
        return rc;
        }

	activate_camera(ccd_handle);
	
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
            return CCD_ERROR;
        }
    camdata[index].error &= ~CCD_ERR_PARM;


    /* wait for cmd ack if block is TRUE */

    if (block == TRUE)
        if (poll_command_acknowledge(ccd_handle,timeout) != TRUE) {
        	camdata[index].errorx = APERR_CMDACK;
            return CCD_ERROR;
            }

    return CCD_OK;
}

/*--------------------------------------------------------------------------*/
/* get_error                                                                */
/*                                                                          */
/* get and reset the indicated camera error status value                    */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
get_error(HCCD ccd_handle, PUSHORT error)
{
    STATUS rc;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

	// Store and reset error status value
	*error = camdata[ccd_handle-1].error;
	camdata[ccd_handle-1].error = 0;

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

static STATUS
load_ccd_data (HCCD ccd_handle, USHORT mode)
{
    USHORT index = ccd_handle-1;
    STATUS rc;
    USHORT calcaic, calcair;
    USHORT maxvbin, maxhbin, calcbir;
    CAMDATA *pcd;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

	pcd = &(camdata[index]);

	activate_camera(ccd_handle);
	
    /* calculate aic */

	if (pcd->cols > pcd->bic + pcd->imgcols)
    	calcaic = pcd->cols - pcd->bic - pcd->imgcols;
    else {
    	pcd->errorx = APERR_CALCAIC;
        pcd->error |= CCD_ERR_SIZE;
        return CCD_ERROR;
    	}
	pcd->aic = calcaic;
	
    /* calculate air */

	if (pcd->rows > pcd->bir + pcd->imgrows)
	    calcair = pcd->rows - pcd->bir - pcd->imgrows;
    else {
    	pcd->errorx = APERR_CALCAIR;
        pcd->error |= CCD_ERR_SIZE;
        return CCD_ERROR;
    	}
	pcd->air = calcair;

    /* set up geometry for the read */

	pcd->colcnt = pcd->imgcols/pcd->hbin;
    pcd->rowcnt = pcd->imgrows/pcd->vbin;

	/* set up column geometry */

    rc = load_aic_count (ccd_handle, calcaic);
    if (rc != CCD_OK) {
    	return CCD_ERROR;
    	}

	/* check that bic > skipc */

	if (pcd->bic <= pcd->skipc) {
		pcd->errorx = APERR_SKIPC;
        pcd->error |= CCD_ERR_SIZE;
        return CCD_ERROR;
    	}

    rc = load_bic_count (ccd_handle, pcd->bic);
    if (rc != CCD_OK) {
    	return CCD_ERROR;
    	}

#ifdef HFLUSH
	maxhbin = max(pcd->hflush, pcd->hbin);
#else
	maxhbin = pcd->hbin;
#endif
    rc = load_horizontal_binning (ccd_handle, maxhbin);
    if (rc != CCD_OK) {
    	return CCD_ERROR;
    	}

    rc = load_pixel_count (ccd_handle, (USHORT)(pcd->cols / maxhbin + 1));
	if (rc != CCD_OK) {
		return CCD_ERROR;
		}

	/* set up row geometry */

#ifdef VFLUSH
	maxvbin = max(pcd->vflush, pcd->vbin);
#else
	maxvbin = pcd->vbin;
#endif

    if (mode == FLUSH) {
	    rc = load_line_count (ccd_handle, pcd->rows);
		if (rc != CCD_OK) {
			return CCD_ERROR;
			}

	    rc = load_timer_and_vbinning (ccd_handle, pcd->timer, maxvbin);
	    if (rc != CCD_OK) {
	    	return CCD_ERROR;
	    	}

        /* reset and then flush the controller */

        if (ccd_command(ccd_handle,CCD_CMD_RSTSYS,BLOCK,5) != CCD_OK) {
        	pcd->errorx = APERR_RSTSYS;
	    	return CCD_ERROR;
	    	}

        if (ccd_command(ccd_handle,CCD_CMD_FLSTRT,BLOCK,5) != CCD_OK) {
        	pcd->errorx = APERR_FLSTRT;
	    	return CCD_ERROR;
	    	}
        }
    else {  /* EXPOSE or SLICE mode */

		/* check that bir > skipr */

		if (pcd->bir <= pcd->skipr) {
			pcd->errorx = APERR_SKIPR;
	        pcd->error |= CCD_ERR_SIZE;
	        return CCD_ERROR;
	    	}
	
		calcbir = pcd->bir - pcd->skipr;

		pcd->oflush = 0;

#ifdef VFLUSH
		if (!pcd->old_cam && (maxvbin > 1)) {
			if ((calcbir / maxvbin) > 1) {
				pcd->oflush = calcbir % maxvbin;
				if (!pcd->oflush)
					pcd->oflush = maxvbin;
				calcbir -= maxvbin;
				}
		    else
		    	maxvbin = 1;
		}
#endif

   		rc = load_timer_and_vbinning (ccd_handle, pcd->timer, maxvbin);
	    if (rc != CCD_OK) {
	    	return CCD_ERROR;
	    	}

        rc = load_line_count (ccd_handle, calcbir);
        if (rc != CCD_OK) {
        	return CCD_ERROR;
        	}
        }

    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* loopback                                                                 */
/*                                                                          */
/* attempts to verify presence of a controller card by toggling the trigger */
/* enable bit and checking for a response.                                  */
/*                                                                          */
/* Returns -1 if no address match was found                                 */
/*                                                                          */
/*--------------------------------------------------------------------------*/

USHORT DLLPROC
loopback(USHORT base)
{
    USHORT tmpval;

    /* attempt to clear the trigger enable bit at the target address */

    tmpval = INPW(base+0x008);
    tmpval &= 0xffdf;                   /* clear trigger bit            */
    OUTPW(base,tmpval);                 /* write it out to register 0   */
    tmpval = INPW(base+0x008);          /* get status from register 12  */

    if (tmpval & 0x0020)
        return CCD_ERROR;

    /* attempt to set the trigger enable bit at the target address */

    tmpval = INPW(base+0x008);
    tmpval |= 0x0020;                   /* enable trigger bit           */
    OUTPW(base,tmpval);                 /* write it out to register 0   */
    tmpval = INPW(base+0x008);          /* get status from register 12  */

    /* now clear it again if we can set it */

    if (tmpval & 0x0020) {
        tmpval = INPW(base+0x008);
        tmpval &= 0xffdf;               /* clear trigger bit            */
        OUTPW(base,tmpval);             /* write it out to register 0   */
        tmpval = INPW(base+0x008);      /* get status from register 12  */
        if (tmpval & 0x0020)
            return CCD_ERROR;
        }
    else
        return CCD_ERROR;

    return CCD_OK;
}

/*--------------------------------------------------------------------------*/
/* open_camera                                                              */
/*                                                                          */
/* allocate a control data block for a controller/camera, optionally set    */
/* mode and test bit fields, and load the configuration data. Returns a     */
/* handle to the control data block if successful, 0 if not.                */
/*--------------------------------------------------------------------------*/

HCCD DLLPROC
open_camera (USHORT mode, USHORT test, PCHAR cfgname)
{
    HCCD    hccd;
    USHORT  index;
    CHAR    fname[_MAX_PATH];
	unsigned int len;
    CAMDATA *pcd;

#ifdef WINUTIL
	if (!Util32AllowIO()) {
		APCCD_OpenErr = CCD_OPEN_NTIO;
		return 0;
	}
#endif
	
    if ((hccd = allocate_ccd()) == 0) {     /* allocate control block       */
        APCCD_OpenErr = CCD_OPEN_HCCD;
        return 0;
        }

    if (cfgname == (PCHAR) NULL) {          /* cannot have null cfg name    */
        delete_ccd(hccd);
        APCCD_OpenErr = CCD_OPEN_CFGNAME;
        return 0;
        }

    len = strlen(cfgname);
    if ((len == 0) || (len >= _MAX_PATH)) { /* cannot have empty/long cfg name    */
        delete_ccd(hccd);
        APCCD_OpenErr = CCD_OPEN_CFGNAME;
        return 0;
        }

    strcpy(fname,cfgname);                  /* load config data, see if OK  */
    if (config_load(hccd,fname) != CCD_OK) {
        delete_ccd(hccd);
        APCCD_OpenErr = CCD_OPEN_CFGDATA;
        return 0;
        }

    index = hccd - 1;                       /* normalize channel index      */

	activate_camera(hccd);

    pcd = &(camdata[index]);                /* get camdata pointer          */

    
    pcd->gotcfg = TRUE;                     /* mark cfg read as OK          */

    if (loopback(pcd->base) != CCD_OK) {    /* check for valid base addr    */
        APCCD_OpenErr = CCD_OPEN_LOOPTST;
        return 0;
        }

    strcpy(pcd->cfgname,fname);             /* set config name in CB        */

    if (mode != IGNORE)                     /* handle mode and test bits    */
        pcd->mode = mode;
    else
        pcd->mode = uDefaultMode;

    if (test != IGNORE)
        pcd->test = test;
    else
        pcd->test = uDefaultTest;

	/* send config data to controller */
	if (send_config_data(hccd) != CCD_OK) {
        APCCD_OpenErr = CCD_OPEN_CFGSEND;
        return 0;
		}

    /* xfer ccd data to controller */
    if (load_ccd_data(hccd, FLUSH) != CCD_OK) {
        APCCD_OpenErr = CCD_OPEN_LOAD;
        return 0;
        }

	/* set min/max desired temp */
	pcd->temp_min = reg2deg(hccd, 0);
	pcd->temp_max = reg2deg(hccd, 255);

    /* Reset extended error value */
    pcd->errorx = APERR_NONE;
    
    return hccd;
}


/*--------------------------------------------------------------------------*/
/* close_camera                                                             */
/*                                                                          */
/* deallocate a control data block and mark it as available                 */
/*                                                                          */
/* Error return with camdata[index].error = OK means chan deallocate failed */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
close_camera (HCCD ccd_handle)
{
    STATUS rc;
    USHORT index = ccd_handle - 1;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    if (delete_ccd(ccd_handle) != CCD_OK) { /* deallocate control block     */
    	camdata[index].errorx = APERR_CLOSE;
        return CCD_ERROR;
        }

#ifdef _APG_PPI
	ppi_fini();
#endif
#ifdef _APG_NET
	net_fini();
#endif

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

STATUS DLLPROC
config_camera (HCCD   ccd_handle,
               USHORT bic_count,
               USHORT bir_count,
               USHORT col_count,
               USHORT row_count,
               USHORT hbin,
               USHORT vbin,
               ULONG  timer,
               USHORT cable_length,
               USHORT use_config)
{
    USHORT index = ccd_handle - 1;
    STATUS rc;
    CAMDATA *pcd;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;
    
    pcd = &(camdata[index]);
    
	activate_camera(ccd_handle);
	
    /* see if caller wants to reload the config file */

    if (use_config == TRUE) {
        if (config_load(ccd_handle,pcd->cfgname) != CCD_OK) {
            pcd->error |= CCD_ERR_CFG;
            pcd->gotcfg = FALSE;
            return CCD_ERROR;
            }
        pcd->error &= ~CCD_ERR_CFG;

		/* send non-camera config data to controller */
		if (send_config_data(ccd_handle) != CCD_OK) {
            return CCD_ERROR;
			}

        }

    /* see if caller wants to change a parameter */

    if (bic_count != IGNORE)
        pcd->bic = bic_count;

    if (bir_count != IGNORE)
        pcd->bir = bir_count;

	// Take care of binning, so img* can be calculated

    if (hbin != IGNORE)
        pcd->hbin = hbin;

    if (vbin != IGNORE)
        pcd->vbin = vbin;

	// img* are unbinned counts

    if (col_count != IGNORE)
        pcd->imgcols = col_count * pcd->hbin;

    if (row_count != IGNORE)
        pcd->imgrows = row_count * pcd->vbin;

    if (timer != IGNORE)
        pcd->timer = timer;

    if ((cable_length != IGNORE) && (pcd->cable != cable_length)) {
    	if (set_cable_length(ccd_handle, cable_length) != CCD_OK) {
    		return CCD_ERROR;
    	}
        pcd->cable = cable_length;
    }

    return CCD_OK;
}

/*--------------------------------------------------------------------------*/
/* start_exposure                                                           */
/*                                                                          */
/* begins an exposure from a specified camera/controller. Optionally sets   */
/* the shutter and trigger operation modes.                                 */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
start_exposure (HCCD   ccd_handle,
                USHORT shutter_en,
                USHORT trigger_mode,
                USHORT block,
                USHORT flushwait )
{
    USHORT index = ccd_handle - 1;
    USHORT frame_timeout;
    STATUS rc;
    CAMDATA *pcd;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

	pcd = &(camdata[index]);
	
	activate_camera(ccd_handle);
	
    /* set up geometry for the read */

    pcd->colcnt = pcd->imgcols/pcd->hbin;
    pcd->rowcnt = pcd->imgrows/pcd->vbin;

    /* check for previous frame completion and stop flush when it's done */

    if (flushwait == TRUE) {
	    frame_timeout = (USHORT)((pcd->timer * 10L / pcd->tscale + 999L) / 1000L) + pcd->frame_timeout;
        if (poll_frame_done(ccd_handle,frame_timeout) != TRUE) {
        	pcd->errorx = APERR_FRAME;
            return CCD_ERROR;
            }
		}

    stop_flushing(ccd_handle);

    reset_system(ccd_handle);

    /* set the shutter and trigger modes */

    if (shutter_en == IGNORE)
        rc = set_shutter_enable(ccd_handle,pcd->shutter_en);
    else
        rc = set_shutter_enable(ccd_handle,shutter_en);
    if (rc != CCD_OK) {
        return rc;
        }

    if (trigger_mode == IGNORE)
        rc = set_trigger(ccd_handle,pcd->trigger_mode);
    else
        rc = set_trigger(ccd_handle,trigger_mode);
    if (rc != CCD_OK) {
        return rc;
        }

    /* start the exposure */

    rc = load_ccd_data(ccd_handle,EXPOSE);
    if (rc != CCD_OK) {
        return CCD_ERROR;
        }

    return (ccd_command(ccd_handle, CCD_CMD_TMSTRT, block, 5));
}


/*--------------------------------------------------------------------------*/
/* check_exposure                                                           */
/*                                                                          */
/* tests the exposure done bit. Returns CCD_OK if it's set, otherwise       */
/* returns CCD_NOTRDY.                                                      */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
check_exposure (HCCD ccd_handle)
{
    STATUS rc;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

	activate_camera(ccd_handle);
	
    if (poll_timer_status(ccd_handle,NOWAIT) == TRUE) {
        return CCD_OK;
        }

    return CCD_NOTRDY;
}

/*--------------------------------------------------------------------------*/
/* check_trigger                                                            */
/*                                                                          */
/* tests the trigger bit. Returns CCD_OK if it's set, otherwise             */
/* returns CCD_ERROR or CCD_NOTRDY.                                         */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
check_trigger (HCCD ccd_handle)
{
    USHORT index = ccd_handle - 1;
    STATUS rc;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

	activate_camera(ccd_handle);
	
	if (camdata[index].trigger_mode != CCD_TRIGEXT) {
		camdata[index].errorx = APERR_TRIGOFF;
		camdata[index].error = CCD_ERR_REG;
		return CCD_ERROR;
		}

    if (poll_got_trigger(ccd_handle,NOWAIT) == TRUE) {
        return CCD_OK;
        }

    return CCD_NOTRDY;
}

/*--------------------------------------------------------------------------*/
/* check_line                                                               */
/*                                                                          */
/* tests the line done bit. Returns CCD_OK if it's set, otherwise returns   */
/* CCD_NOTRDY.                                                              */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
check_line (HCCD ccd_handle)
{
    STATUS rc;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

	activate_camera(ccd_handle);
	
    if (poll_line_done(ccd_handle,NOWAIT) == TRUE) {
        return CCD_OK;
        }

    return CCD_NOTRDY;
}


/*--------------------------------------------------------------------------*/
/* check_image                                                              */
/*                                                                          */
/* test the frame done bit. Returns CCD_OK if it's set, otherwise return    */
/* CCD_NOTRDY.                                                              */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
check_image (HCCD ccd_handle)
{
    STATUS rc;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

	activate_camera(ccd_handle);
	
    if (poll_frame_done(ccd_handle,NOWAIT) == TRUE) {
        return CCD_OK;
        }

    return CCD_NOTRDY;
}

static STATUS
skip_rows(HCCD ccd_handle, USHORT caching, PDATA buffer)
{
    USHORT index = ccd_handle - 1;
    USHORT lines;
	USHORT tmpval;
    USHORT Reg1, Reg9, Reg12;
    CAMDATA *pcd = &(camdata[index]);
	int fCaching;
    USHORT pixcnt;
#ifdef ASMXFER
	USHORT fifo_count = pcd->skipc;
	USHORT line_count = pcd->colcnt;
#endif

	// Set vertical binning to user value and return if no rows to skip
	if (!(pcd->skipr + pcd->oflush)) {
	    if (!pcd->old_cam && (load_timer_and_vbinning (ccd_handle, pcd->timer, pcd->vbin) != CCD_OK))
   			return CCD_ERROR;
		return (CCD_OK);
	}

#ifdef VFLUSH
	// Set vertical binning to 1 for offset and skipped rows
	if (pcd->vflush != 1) {
    	if (!pcd->old_cam && (load_timer_and_vbinning (ccd_handle, pcd->timer, 1) != CCD_OK))
    		return CCD_ERROR;
    }
#endif

#ifdef HFLUSH
	// Set horizontal binning for image area
    if (load_horizontal_binning (ccd_handle, pcd->hbin) != CCD_OK)
    	return CCD_ERROR;
    if (load_pixel_count (ccd_handle, (USHORT)(pcd->colcnt + pcd->skipc)) != CCD_OK)
		return CCD_ERROR;
#endif

	// Set up local variables
    Reg1 = pcd->base + CCD_REG01;
    Reg9 = pcd->base + CCD_REG09;
    Reg12 = pcd->base + CCD_REG12;
	fCaching = (pcd->caching && caching);
	pixcnt = pcd->colcnt + pcd->skipc;

//	Initial line data skipped

	lines = pcd->skipr + pcd->oflush;
	while (lines) {
		if (fCaching)
    		set_fifo_caching(ccd_handle,TRUE);

#ifdef VFLUSH
		if (lines == pcd->skipr) {
			// Last line - reset vertical binning for image area
		    if (!pcd->old_cam && (load_timer_and_vbinning (ccd_handle, pcd->timer, pcd->vbin) != CCD_OK))
    			return CCD_ERROR;
    		}
#endif

		// Start delivering next line
	    tmpval = INPW(Reg12);
		OUTPW(Reg1, tmpval | CCD_BIT_NEXT);
		OUTPW(Reg1, tmpval & ~CCD_BIT_NEXT);

		// Wait for command acknowledge
    	if (poll_command_acknowledge(ccd_handle,5) != TRUE) {
    		pcd->errorx = APERR_LINACK;
        	pcd->error |= CCD_ERR_REG;
	        return CCD_ERROR;
    	    }
           
		if (fCaching) {
			// Transfer last line
			if (lines == 1) {
#ifdef ASMXFER
				if (fifo_count)
					_asm {
					mov	 cx,fifo_count
					mov  dx,Reg9
asm1:
					in   ax,dx
					dec  cx
					jnz  asm1
					}
#else
#ifdef _APG_NET
				net_discwn(Reg9, (ULONG) pcd->skipc);
#else
				// Discard fifo data
				tmpval = pcd->skipc;
				while (tmpval--)
					(void) INPW(Reg9);
#endif
#endif
#ifdef ASMXFER
#ifdef WIN32
				_asm {
				mov ebx,dword ptr [buffer]
				mov cx,line_count
				mov dx,Reg9
asm2:
				in  ax,dx
				mov word ptr [ebx],ax
				add ebx,2
				dec cx
				jnz asm2
				mov dword ptr [buffer],ebx
				}
#else
				_asm {
				push di
				les  di,buffer
				mov	 cx,line_count
				mov  dx,Reg9
asm2:
				in   ax,dx
				mov  word ptr es:[di],ax
			    mov  bx,es
			    add  di,2
			    sbb  ax,ax
			    and  ax,8
			    add  bx,ax
			    mov  es,bx
				dec  cx
				jnz  asm2
				mov  word ptr [buffer],di
				mov  word ptr [buffer+2],es
				pop  di
				}
#endif
#else
				tmpval = pcd->colcnt;
#ifdef _APG_NET
				net_inpwn(Reg9, (ULONG) tmpval, buffer);
				buffer += tmpval;
#else
				// Transfer line data
				while (tmpval--)
					*buffer++ = INPW(Reg9);
#endif
#endif
			}

			// Assert line/row done
	    	tmpval = INPW(Reg12);
			OUTPW(Reg1, tmpval | CCD_BIT_RDONE);
			OUTPW(Reg1, tmpval & ~CCD_BIT_RDONE);
		}
		
		// Wait for next line done
        if (poll_line_done(ccd_handle,pcd->line_timeout) != TRUE) {
    		pcd->errorx = APERR_LINE;
            pcd->error |= CCD_ERR_REG;
            return CCD_ERROR;
            }

		if (!fCaching && (lines == 1)) {
#ifdef ASMXFER
			if (fifo_count)
				_asm {
				mov	 cx,fifo_count
				mov  dx,Reg9
asm3:
				in   ax,dx
				dec  cx
				jnz  asm3
				}
#else
#ifdef _APG_NET
			net_discwn(Reg9, (ULONG) pcd->skipc);
#else
			// Discard fifo data
			tmpval = pcd->skipc;
			while (tmpval--)
				(void) INPW(Reg9);
#endif
#endif
#ifdef ASMXFER
#ifdef WIN32
			_asm {
			mov ebx,dword ptr [buffer]
			mov cx,line_count
			mov dx,Reg9
asm4:
			in  ax,dx
			mov word ptr [ebx],ax
			add ebx,2
			dec cx
			jnz asm4
			mov dword ptr [buffer],ebx
			}
#else
			_asm {
			push di
			les  di,buffer
			mov	 cx,line_count
			mov  dx,Reg9
asm4:
			in   ax,dx
			mov  word ptr es:[di],ax
		    mov  bx,es
		    add  di,2
		    sbb  ax,ax
		    and  ax,8
		    add  bx,ax
		    mov  es,bx
			dec  cx
			jnz  asm4
			mov  word ptr [buffer],di
			mov  word ptr [buffer+2],es
			pop  di
			}
#endif
#else
			tmpval = pcd->colcnt;
#ifdef _APG_NET
			net_inpwn(Reg9, (ULONG) tmpval, buffer);
			buffer += tmpval;
#else
			// Transfer line data
			while (tmpval--)
				*buffer++ = INPW(Reg9);
#endif
#endif
		}

    	lines--;
	}

#ifdef VFLUSH
	if (!pcd->skipr) {
		// Last line, no skips - reset vertical binning for image area
		if (!pcd->old_cam && (load_timer_and_vbinning (ccd_handle, pcd->timer, pcd->vbin) != CCD_OK))
			return CCD_ERROR;
		}
#endif

	return (CCD_OK);
}

/*--------------------------------------------------------------------------*/
/* acquire_line                                                             */
/*                                                                          */
/* reads a single line of data from the CCD to a user-specified buffer.     */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
acquire_line (HCCD ccd_handle,
              USHORT caching,
              PLINE line_buffer,
              USHORT firsttime)
{
    STATUS rc;
    USHORT j;
    USHORT index = ccd_handle - 1;
    USHORT Reg1, Reg9, Reg12;
    PLINE  outbuf;
	USHORT frame_timeout;
	USHORT tmpval;
    static USHORT line_ok;
    static USHORT line_timeout;
	static USHORT use_cache;
    CAMDATA *pcd;
#ifdef ASMXFER
	USHORT fifo_count;
	USHORT line_count;
#endif

    if ((rc = check_parms(ccd_handle)) != CCD_OK) {
        return rc;
        }

	activate_camera(ccd_handle);
	
	pcd = &(camdata[index]);

	pcd->error = 0;

#ifdef ASMXFER
	fifo_count = pcd->skipc;
	line_count = pcd->colcnt;
#endif

    if (line_buffer == (PLINE) NULL) {
        pcd->errorx = APERR_PDATA;
        pcd->error |= CCD_ERR_BUFF;
        return CCD_ERROR;
        }

// mask interrupts if the application requests
	user_mask();

// Set up camera register addresses
    Reg1 = pcd->base + CCD_REG01;
    Reg9 = pcd->base + CCD_REG09;
    Reg12 = pcd->base + CCD_REG12;

// and temporary output buffer pointer
    outbuf = line_buffer;

    if (firsttime == TRUE) {
// First line requires special handling
// Initialize line timeout flags
    	line_timeout = FALSE;
    	line_ok = TRUE;
		use_cache = (pcd->caching && caching);

// make sure CCD's in the correct state
// Wait for initial frame done
	    frame_timeout = (USHORT)((pcd->timer * 10L / pcd->tscale + 999L) / 1000L) + pcd->frame_timeout;
        if (poll_frame_done(ccd_handle,frame_timeout) != TRUE) {
	        user_unmask();
	        pcd->errorx = APERR_FRAME;
	        pcd->error |= CCD_ERR_REG;
            return CCD_ERROR;
            }

// Wait for initial line done, store bad line value on timeout
        if (poll_line_done(ccd_handle,pcd->line_timeout) != TRUE) {
        	line_timeout = TRUE;
			j = pcd->colcnt;
			while (j--)
    		    *outbuf++ = BAD_LINE;
	        user_unmask();
	        pcd->errorx = APERR_LINE;
	        pcd->error |= CCD_ERR_REG;
            return CCD_ERROR;
            }

// Discard skipped rows, transfer first image row
		if (skip_rows(ccd_handle, caching, line_buffer) != CCD_OK) {
	        user_unmask();
	        return CCD_ERROR;
			}
        }
		if (!(pcd->skipr + pcd->oflush)) {
// Discard fifo pipeline pixels
#ifdef ASMXFER
			if (fifo_count)
				_asm {
				mov	 cx,fifo_count
				mov  dx,Reg9
asm0:
				in   ax,dx
				dec  cx
				jnz  asm0
				}
#else
#ifdef _APG_NET
			net_discwn(Reg9, (ULONG) pcd->skipc);
#else
			j = pcd->skipc;
			while (j--)
			    (void) INPW(Reg9);
#endif
#endif
	
// Transfer initial line
#ifdef ASMXFER
#ifdef WIN32
			_asm {
			mov ebx,dword ptr [outbuf]
			mov cx,line_count
			mov dx,Reg9
asm1:
			in  ax,dx
			mov word ptr [ebx],ax
			add ebx,2
			dec cx
			jnz asm1
			mov dword ptr [outbuf],ebx
			}
#else
			_asm {
			push di
			les  di,outbuf
			mov	 cx,line_count
			mov  dx,Reg9
asm1:
			in   ax,dx
			mov  word ptr es:[di],ax
		    mov  bx,es
		    add  di,2
		    sbb  ax,ax
		    and  ax,8
		    add  bx,ax
		    mov  es,bx
			dec  cx
			jnz  asm1
			mov  word ptr [outbuf],di
			mov  word ptr [outbuf+2],es
			pop  di
			}
#endif
#else
			j = pcd->colcnt;
#ifdef _APG_NET
			net_inpwn(Reg9, (ULONG) j, outbuf);
			outbuf += j;
#else
			while (j--)
			    *outbuf++ = INPW(Reg9);
#endif
#endif
		}
	else {
// 2..nth lines
// Wait for line cache
		if (use_cache) {
// Start delivering next line
		    tmpval = INPW(Reg12);
		    OUTPW(Reg1, tmpval | CCD_BIT_NEXT);
			OUTPW(Reg1, tmpval & ~CCD_BIT_NEXT);

// Wait for command acknowledge
	        if (poll_command_acknowledge(ccd_handle,5) != TRUE) {
		        user_unmask();
		        pcd->errorx = APERR_LINACK;
	            pcd->error |= CCD_ERR_REG;
	            return CCD_ERROR;
	            }

// Discard fifo pipeline pixels
#ifdef ASMXFER
			if (fifo_count)
				_asm {
				mov	cx,fifo_count
				mov dx,Reg9
asm2:
				in  ax,dx
				dec cx
				jnz asm2
				}
#else
#ifdef _APG_NET
			net_discwn(Reg9, (ULONG) pcd->skipc);
#else
			j = pcd->skipc;
			while (j--)
			    (void) INPW(Reg9);
#endif
#endif

// Transfer line
            if (line_ok) {
#ifdef ASMXFER
#ifdef WIN32
				_asm {
				mov ebx,dword ptr [outbuf]
				mov cx,line_count
				mov dx,Reg9
asm3:
				in  ax,dx
				mov word ptr [ebx],ax
				add ebx,2
				dec cx
				jnz asm3
				mov dword ptr [outbuf],ebx
				}
#else
				_asm {
				push di
				les  di,outbuf
				mov	 cx,line_count
				mov  dx,Reg9
asm3:
				in   ax,dx
				mov  word ptr es:[di],ax
			    mov  bx,es
			    add  di,2
			    sbb  ax,ax
			    and  ax,8
			    add  bx,ax
			    mov  es,bx
				dec  cx
				jnz  asm3
				mov  word ptr [outbuf],di
				mov  word ptr [outbuf+2],es
				pop  di
				}
#endif
#else
				j = pcd->colcnt;
#ifdef _APG_NET
				net_inpwn(Reg9, (ULONG) j, outbuf);
				outbuf += j;
#else
				while (j--)
				    *outbuf++ = INPW(Reg9);
#endif
#endif
        	} else {
#ifdef ASMXFER
#ifdef WIN32
				_asm {
				mov ebx,dword ptr [outbuf]
				mov cx,line_count
asm4:
				mov word ptr [ebx],BAD_LINE
				add ebx,2
				dec cx
				jnz asm4
				mov dword ptr [outbuf],ebx
				}
#else
				_asm {
				push di
				les  di,outbuf
				mov	 cx,line_count
asm4:
				mov  word ptr es:[di],BAD_LINE
			    mov  bx,es
			    add  di,2
			    sbb  ax,ax
			    and  ax,8
			    add  bx,ax
			    mov  es,bx
				dec  cx
				jnz  asm4
				mov  word ptr [outbuf],di
				mov  word ptr [outbuf+2],es
				pop  di
				}
#endif
#else
	            j = pcd->colcnt;
            	while (j--)
                	*outbuf++ = BAD_LINE;
#endif
			}

// Assert line/row done
		    tmpval = INPW(Reg12);
    		OUTPW(Reg1, tmpval | CCD_BIT_RDONE);
    		OUTPW(Reg1, tmpval & ~CCD_BIT_RDONE);

// Wait for next line done, store bad line value on timeout
            if (poll_line_done(ccd_handle,pcd->line_timeout) != TRUE) {
            	line_ok = FALSE;
		        user_unmask();
		        pcd->errorx = APERR_LINE;
                pcd->error |= CCD_ERR_REG;
                return CCD_ERROR;
                }
            line_ok = TRUE;
            }
        else {
// Start delivering next line
		    tmpval = INPW(Reg12);
		    OUTPW(Reg1, tmpval | CCD_BIT_NEXT);
    		OUTPW(Reg1, tmpval & ~CCD_BIT_NEXT);

// Wait for command acknowledge
	        if (poll_command_acknowledge(ccd_handle,5) != TRUE) {
		        user_unmask();
		        pcd->errorx = APERR_LINACK;
                pcd->error |= CCD_ERR_REG;
	            return CCD_ERROR;
	            }

            if (poll_line_done(ccd_handle,pcd->line_timeout) != TRUE) {
#ifdef ASMXFER
#ifdef WIN32
				_asm {
				mov ebx,dword ptr [outbuf]
				mov cx,line_count
asm5:
				mov word ptr [ebx],BAD_LINE
				add ebx,2
				dec cx
				jnz asm5
				mov dword ptr [outbuf],ebx
				}
#else
				_asm {
				push di
				les  di,outbuf
				mov	 cx,line_count
asm5:
				mov  word ptr es:[di],BAD_LINE
			    mov  bx,es
			    add  di,2
			    sbb  ax,ax
			    and  ax,8
			    add  bx,ax
			    mov  es,bx
				dec  cx
				jnz  asm5
				mov  word ptr [outbuf],di
				mov  word ptr [outbuf+2],es
				pop  di
				}
#endif
#else
	            j = pcd->colcnt;
            	while (j--)
                	*outbuf++ = BAD_LINE;
#endif
		        user_unmask();
		        pcd->errorx = APERR_LINE;
                pcd->error |= CCD_ERR_REG;
                return CCD_ERROR;
                }

#ifdef ASMXFER
			if (fifo_count)
				_asm {
				mov	cx,fifo_count
				mov dx,Reg9
asm6:
				in  ax,dx
				dec cx
				jnz asm6
				}
#else
#ifdef _APG_NET
			net_discwn(Reg9, (ULONG) pcd->skipc);
#else
			j = pcd->skipc;
			while (j--)
			    (void) INPW(Reg9);
#endif
#endif
#ifdef ASMXFER
#ifdef WIN32
			_asm {
			mov ebx,dword ptr [outbuf]
			mov cx,line_count
			mov dx,Reg9
asm7:
			in  ax,dx
			mov word ptr [ebx],ax
			add ebx,2
			dec cx
			jnz asm7
			mov dword ptr [outbuf],ebx
			}
#else
			_asm {
			push di
			les  di,outbuf
			mov	 cx,line_count
			mov  dx,Reg9
asm7:
			in   ax,dx
			mov  word ptr es:[di],ax
		    mov  bx,es
		    add  di,2
		    sbb  ax,ax
		    and  ax,8
		    add  bx,ax
		    mov  es,bx
			dec  cx
			jnz  asm7
			mov  word ptr [outbuf],di
			mov  word ptr [outbuf+2],es
			pop  di
			}
#endif
#else
			j = pcd->colcnt;
#ifdef _APG_NET
			net_inpwn(Reg9, (ULONG) j, outbuf);
			outbuf += j;
#else
			while (j--)
			    *outbuf++ = INPW(Reg9);
#endif
#endif
            }
        }

    user_unmask();

    return CCD_OK;
}

STATUS flush_rows (HCCD ccd_handle, USHORT rows, USHORT airwait)
{
    USHORT index = ccd_handle - 1;
    CAMDATA *pcd;
#ifdef HFLUSH
	USHORT maxhbin;
#endif

	activate_camera(ccd_handle);
	
    pcd = &(camdata[index]);
    
    if (load_line_count(ccd_handle, rows) != CCD_OK) {
	    return CCD_ERROR;
    	}

#ifdef VFLUSH
	if (load_timer_and_vbinning (ccd_handle, pcd->timer, pcd->vflush) != CCD_OK) {
    	return CCD_ERROR;
    	}
#endif

#ifdef HFLUSH
	maxhbin = max(pcd->hflush, pcd->hbin);
    if (load_horizontal_binning (ccd_handle, maxhbin) != CCD_OK) {
    	return CCD_ERROR;
    	}
	if (load_pixel_count (ccd_handle, (USHORT)(pcd->cols / maxhbin + 1)) != CCD_OK) {
		return CCD_ERROR;
		}
#endif

    ccd_command(ccd_handle, CCD_CMD_FLSTRT, NOBLOCK, 5);

	if (airwait == TRUE) {
	    if (poll_frame_done(ccd_handle,pcd->frame_timeout) != TRUE) {
	        pcd->errorx = APERR_FRAME;
	        return CCD_ERROR;
	        }

	    /* now start regular flushing */

    	if (load_ccd_data(ccd_handle, FLUSH) != CCD_OK) {
    	    return CCD_ERROR;
    	    }
    	}

    return CCD_OK;
}

/*--------------------------------------------------------------------------*/
/* flush_air                                                                */
/*                                                                          */
/* flushes the remaining rows after the ROI on the CCD. Should be called    */
/* after all data is read from the CCD using acquire_line.                  */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC flush_air (HCCD ccd_handle, USHORT airwait)
{
    STATUS rc;
    USHORT index = ccd_handle - 1;
    CAMDATA *pcd;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;
    
    pcd = &(camdata[index]);

	return (flush_rows(ccd_handle, pcd->air, airwait));
}

/*--------------------------------------------------------------------------*/
/* flush_image                                                              */
/*                                                                          */
/* flushes the entire CCD. Should be called after long intra-exposure       */
/* delays or after aborting any type of exposure.                           */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC flush_image (HCCD ccd_handle, USHORT airwait)
{
    STATUS rc;
    USHORT index = ccd_handle - 1;
    CAMDATA *pcd;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;
    
    pcd = &(camdata[index]);

	return (flush_rows(ccd_handle, pcd->rows, airwait));
}

/*--------------------------------------------------------------------------*/
/* acquire_image                                                            */
/*                                                                          */
/* offload image from CCD to caller-specified memory buffer. this function  */
/* must not be interrupted in order to maintain read timing.                */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
acquire_image (HCCD ccd_handle,
			   USHORT caching,
			   PDATA buffer,
			   USHORT airwait )
{
    STATUS rc;
#ifdef ASMXFER
    PDATA outbuf;
#else
    USHORT register j;
    PDATA register outbuf;
#endif
    USHORT register i;
    USHORT index = ccd_handle - 1;
    USHORT Reg1, Reg9, Reg12;
	USHORT tmpval;
    USHORT line_ok;
    USHORT line_timeout;
    USHORT frame_timeout;
    CAMDATA *pcd;
#ifdef ASMXFER
	USHORT fifo_count;
	USHORT line_count;
#endif
#ifdef ABORT
	static USHORT counter=10;
#endif

    if ((rc = check_parms(ccd_handle)) != CCD_OK) {
        return rc;
        }
    
	activate_camera(ccd_handle);
	
    pcd = &(camdata[index]);

    pcd->error = 0;

#ifdef ASMXFER
    fifo_count = pcd->skipc;
    line_count = pcd->colcnt;
#endif
    
    if (buffer == (PDATA) NULL) {
		pcd->errorx = APERR_PDATA;
        pcd->error |= CCD_ERR_BUFF;
        return CCD_ERROR;
        }

    /* make sure CCD's in the correct state */
    // Wait for frame done
    frame_timeout = (USHORT)((pcd->timer * 10L  / pcd->tscale + 999L) / 1000L) + pcd->frame_timeout;
    if (poll_frame_done(ccd_handle, frame_timeout) != TRUE) {
		pcd->errorx = APERR_FRAME;
        pcd->error |= CCD_ERR_REG;
        return CCD_ERROR;
        }

	// Wait for initial line done
    if (poll_line_done(ccd_handle, pcd->line_timeout) != TRUE) {
		pcd->errorx = APERR_LINE;
        pcd->error |= CCD_ERR_REG;
        return CCD_ERROR;
        }

	// increase process priority to minimize CPU time dependence
	SetPriority();

// mask interrupts if the application requests
	user_mask();

// Set up camera register addresses
    Reg1 = pcd->base + CCD_REG01;
    Reg9 = pcd->base + CCD_REG09;
    Reg12 = pcd->base + CCD_REG12;

// and temporary output buffer pointer
    outbuf = buffer;

// Initialize line_timeout flag
	line_timeout = FALSE;

// Discard skipped rows
	if (skip_rows(ccd_handle, caching, buffer) != CCD_OK) {
        user_unmask();
		RestorePriority();
	    return CCD_ERROR;
		}

	if (pcd->skipr + pcd->oflush)
		outbuf += pcd->colcnt;
	else {
// Discard fifo pipeline pixels
#ifdef ASMXFER
		if (fifo_count)
			_asm {
			mov	 cx,fifo_count
			mov  dx,Reg9
asm0:
			in   ax,dx
			dec  cx
			jnz  asm0
			}
#else
#ifdef _APG_NET
		net_discwn(Reg9, pcd->skipc);
#else
		j = pcd->skipc;
		while (j--)
		    (void) INPW(Reg9);
#endif
#endif

// Transfer initial line
#ifdef ASMXFER
#ifdef WIN32
		_asm {
		mov ebx,dword ptr [outbuf]
		mov cx,line_count
		mov dx,Reg9
asm1:
		in  ax,dx
		mov word ptr [ebx],ax
		add ebx,2
		dec cx
		jnz asm1
		mov dword ptr [outbuf],ebx
		}
#else
		_asm {
		push di
		les  di,outbuf
		mov	 cx,line_count
		mov  dx,Reg9
asm1:
		in   ax,dx
		mov  word ptr es:[di],ax
	    mov  bx,es
	    add  di,2
	    sbb  ax,ax
	    and  ax,8
	    add  bx,ax
	    mov  es,bx
		dec  cx
		jnz  asm1
		mov  word ptr [outbuf],di
		mov  word ptr [outbuf+2],es
		pop  di
		}
#endif
#else
		j = pcd->colcnt;
#ifdef _APG_NET
		net_inpwn(Reg9, (ULONG)j, outbuf);
		outbuf += j;
#else
		while (j--)
		    *outbuf++ = INPW(Reg9);
#endif
#endif
	}

    /* now read out the rest of the ROI data using cached or uncached modes */

    if (pcd->caching && caching) {
    	line_ok = TRUE;
//        set_fifo_caching(ccd_handle,TRUE);
        i = pcd->rowcnt - 1;
        while (i--) {
// Start delivering next line
		    tmpval = INPW(Reg12);
		    OUTPW(Reg1, tmpval | CCD_BIT_NEXT);
    		OUTPW(Reg1, tmpval & ~CCD_BIT_NEXT);
// Wait for command acknowledge
	        if (poll_command_acknowledge(ccd_handle,5) != TRUE) {
		        user_unmask();
				RestorePriority();
				pcd->errorx = APERR_LINACK;
	            return CCD_ERROR;
	            }

// Discard fifo pipeline pixels
#ifdef ASMXFER
			if (fifo_count)
				_asm {
				mov	cx,fifo_count
				mov dx,Reg9
asm2:
				in  ax,dx
				dec cx
				jnz asm2
				}
#else
#ifdef _APG_NET
			net_discwn(Reg9, (ULONG)pcd->skipc);
#else
			j = pcd->skipc;
			while (j--)
			    (void) INPW(Reg9);
#endif
#endif

// Transfer line
            if (line_ok) {
#ifdef ASMXFER
#ifdef WIN32
				_asm {
				mov ebx,dword ptr [outbuf]
				mov cx,line_count
				mov dx,Reg9
asm3:
				in  ax,dx
				mov word ptr [ebx],ax
				add ebx,2
				dec cx
				jnz asm3
				mov dword ptr [outbuf],ebx
				}
#else
				_asm {
				push di
				les  di,outbuf
				mov	 cx,line_count
				mov  dx,Reg9
asm3:
				in   ax,dx
				mov  word ptr es:[di],ax
			    mov  bx,es
			    add  di,2
			    sbb  ax,ax
			    and  ax,8
			    add  bx,ax
			    mov  es,bx
				dec  cx
				jnz  asm3
				mov  word ptr [outbuf],di
				mov  word ptr [outbuf+2],es
				pop  di
				}
#endif
#else
				j = pcd->colcnt;
#ifdef _APG_NET
				net_inpwn(Reg9, j, outbuf);
				outbuf += j;
#else
				while (j--)
				    *outbuf++ = INPW(Reg9);
#endif
#endif
            } else {
#ifdef ASMXFER
#ifdef WIN32
				_asm {
				mov ebx,dword ptr [outbuf]
				mov cx,line_count
asm4:
				mov word ptr [ebx],BAD_LINE
				add ebx,2
				dec cx
				jnz asm4
				mov dword ptr [outbuf],ebx
				}
#else
				_asm {
				push di
				les  di,outbuf
				mov	 cx,line_count
asm4:
				mov  word ptr es:[di],BAD_LINE
			    mov  bx,es
			    add  di,2
			    sbb  ax,ax
			    and  ax,8
			    add  bx,ax
			    mov  es,bx
				dec  cx
				jnz  asm4
				mov  word ptr [outbuf],di
				mov  word ptr [outbuf+2],es
				pop  di
				}
#endif
#else
	            j = pcd->colcnt;
            	while (j--)
                	*outbuf++ = BAD_LINE;
#endif
            }

#ifdef ABORT
			if (counter)
				counter--;
			if (counter == 0)
				TerminateApp(NULL, UAE_BOX);
#endif

// Assert line/row done
		    tmpval = INPW(Reg12);
    		OUTPW(Reg1, tmpval | CCD_BIT_RDONE);
    		OUTPW(Reg1, tmpval & ~CCD_BIT_RDONE);

// Wait for next line done, store bad line value on timeout
            if (poll_line_done(ccd_handle,pcd->line_timeout) != TRUE) {
            	if (line_timeout) {
				    user_unmask();
					RestorePriority();
		            return CCD_ERROR;
		        } else
		        	line_timeout = TRUE;
				pcd->errorx = APERR_LINE;
                pcd->error |= CCD_ERR_REG;
                line_ok = FALSE;
                }
            else {
            	line_timeout = FALSE;
            	line_ok = TRUE;
            	}
            }
        set_fifo_caching(ccd_handle,FALSE);
        }
    else {
        i = pcd->rowcnt - 1;
        while (i--) {
// Start delivering next line
		    tmpval = INPW(Reg12);
		    OUTPW(Reg1, tmpval | CCD_BIT_NEXT);
    		OUTPW(Reg1, tmpval & ~CCD_BIT_NEXT);
// Wait for command acknowledge
	        if (poll_command_acknowledge(ccd_handle,5) != TRUE) {
			    user_unmask();
				RestorePriority();
				pcd->errorx = APERR_LINACK;
	            return CCD_ERROR;
	            }
// Wait for line done, store bad line value on timeout
            if (poll_line_done(ccd_handle,pcd->line_timeout) != TRUE) {
            	if (line_timeout) {
				    user_unmask();
					RestorePriority();
		            return CCD_ERROR;
		        } else
		        	line_timeout = TRUE;
				pcd->errorx = APERR_LINE;
                pcd->error |= CCD_ERR_REG;
#ifdef ASMXFER
#ifdef WIN32
				_asm {
				mov ebx,dword ptr [outbuf]
				mov cx,line_count
asm5:
				mov word ptr [ebx],BAD_LINE
				add ebx,2
				dec cx
				jnz asm5
				mov dword ptr [outbuf],ebx
				}
#else
				_asm {
				push di
				les  di,outbuf
				mov	 cx,line_count
asm5:
				mov  word ptr es:[di],BAD_LINE
			    mov  bx,es
			    add  di,2
			    sbb  ax,ax
			    and  ax,8
			    add  bx,ax
			    mov  es,bx
				dec  cx
				jnz  asm5
				mov  word ptr [outbuf],di
				mov  word ptr [outbuf+2],es
				pop  di
				}
#endif
#else
	            j = pcd->colcnt;
            	while (j--)
                	*outbuf++ = BAD_LINE;
#endif
                }
            else {
            	line_timeout = FALSE;
#ifdef ASMXFER
				if (fifo_count)
					_asm {
					mov	cx,fifo_count
					mov dx,Reg9
asm6:
					in  ax,dx
					dec cx
					jnz asm6
					}
#else
#ifdef _APG_NET
				net_discwn(Reg9, (ULONG)pcd->skipc);
#else
				j = pcd->skipc;
				while (j--)
				    (void) INPW(Reg9);
#endif
#endif
#ifdef ASMXFER
#ifdef WIN32
				_asm {
				mov ebx,dword ptr [outbuf]
				mov cx,line_count
				mov dx,Reg9
asm7:
				in  ax,dx
				mov word ptr [ebx],ax
				add ebx,2
				dec cx
				jnz asm7
				mov dword ptr [outbuf],ebx
				}
#else
				_asm {
				push di
				les  di,outbuf
				mov	 cx,line_count
				mov  dx,Reg9
asm7:
				in   ax,dx
				mov  word ptr es:[di],ax
			    mov  bx,es
			    add  di,2
			    sbb  ax,ax
			    and  ax,8
			    add  bx,ax
			    mov  es,bx
				dec  cx
				jnz  asm7
				mov  word ptr [outbuf],di
				mov  word ptr [outbuf+2],es
				pop  di
				}
#endif
#else
				j = pcd->colcnt;
#ifdef _APG_NET
				net_inpwn(Reg9, (ULONG)j, outbuf);
				outbuf += j;
#else
				while (j--)
				    *outbuf++ = INPW(Reg9);
#endif
#endif
	            }
            }
        }

// unmask interrupts if they were masked
    user_unmask();

// restore normal priority
	RestorePriority();

    /* flush remainder of CCD rows */
    flush_air(ccd_handle, airwait);

    if (pcd->error != 0)
    	return CCD_ERROR;

    return CCD_OK;
}

#define CCD_BIT_FAST	0x8
#define CCD_MSK_FAST	0x7

STATUS DLLPROC
config_slice  (HCCD   ccd_handle,
               USHORT bic_count,
               USHORT bir_count,
               USHORT col_count,
               USHORT row_count,
               USHORT hbin,
               USHORT vbin)
{
    USHORT index = ccd_handle - 1;
    STATUS rc;
    SLICEDATA *psd;
    CAMDATA *pcd;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;
    
	activate_camera(ccd_handle);
	
	pcd = &(camdata[index]);
    psd = &(slicedata[index]);

	/* Check if chip supports slice acquisitions */
	
    if (!pcd->slice) {
		pcd->errorx = APERR_SLICE;
        pcd->error |= CCD_ERR_CFG;
        return CCD_ERROR;
        }

    /* see if caller wants to change a parameter */

    if (bic_count != IGNORE)
        psd->bic = bic_count;

    if (bir_count != IGNORE)
        psd->bir = bir_count;

	// Take care of binning, so img* can be calculated

    if (hbin != IGNORE)
        psd->hbin = hbin;

    if (vbin != IGNORE)
        psd->vbin = vbin;

	// img* are unbinned counts

    if (col_count != IGNORE) {
    	psd->colcnt = col_count;
        psd->imgcols = col_count * psd->hbin;
        }

    if (row_count != IGNORE) {
    	psd->rowcnt = row_count;
        psd->imgrows = row_count * psd->vbin;
        }

	psd->valid = TRUE;

    return CCD_OK;
}

/* (re)Load camera registers necessary for slice processing */

static STATUS
load_slice_registers(HCCD ccd_handle, USHORT slice)
{
    CAMDATA *pcd = &(camdata[ccd_handle-1]);
    SLICEDATA *psd = &(slicedata[ccd_handle-1]);
    USHORT uiTemp;

	if (slice) {
		uiTemp = pcd->cols - psd->bic - pcd->skipc - psd->imgcols;
		load_aic_count(ccd_handle, uiTemp);
		load_bic_count(ccd_handle, psd->bic);
		load_horizontal_binning(ccd_handle, psd->hbin);
		load_pixel_count (ccd_handle, (USHORT)(psd->imgcols / psd->hbin));
		load_timer_and_vbinning (ccd_handle, pcd->timer, psd->vbin);
		}
	else
		load_ccd_data(ccd_handle,SLICE);

	return CCD_OK;
}

STATUS DLLPROC
acquire_slice (HCCD ccd_handle,
			   USHORT caching,
			   PDATA buffer,
			   USHORT reverse_enable,
			   USHORT shutter_gate,
			   USHORT overshoot,
			   USHORT fast)
{
    USHORT register j;
    PDATA register outbuf;
    USHORT register i;
    STATUS rc;
    USHORT index = ccd_handle - 1;
    CAMDATA *pcd;
    SLICEDATA *psd;
    USHORT Reg1, Reg9, Reg12;
	USHORT tmpval;
#ifdef _APGDLL
    DWORD dwShutterOK;
#endif

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

	activate_camera(ccd_handle);
	
	pcd = &(camdata[index]);
	psd = &(slicedata[index]);
	
	/* Check if chip supports slice acquisitions */
	
    if (!pcd->slice) {
		pcd->errorx = APERR_SLICE;
        pcd->error |= CCD_ERR_CFG;
        return CCD_ERROR;
        }

	/* Check if slice config valid */
	
    if (!psd->valid) {
		pcd->errorx = APERR_CFGSLICE;
        pcd->error |= CCD_ERR_CFG;
        return CCD_ERROR;
        }

	/* Make sure buffer is defined */

    if (buffer == (PDATA) NULL) {
		pcd->errorx = APERR_PDATA;
        pcd->error |= CCD_ERR_BUFF;
        return CCD_ERROR;
        }
	outbuf = buffer;

	set_slice_amp(ccd_handle, 1);

	set_slice_delay(ccd_handle, 1);

	stop_flushing(ccd_handle);
	
	load_slice_registers(ccd_handle, 1);

	if (fast)
		load_mode_bits(ccd_handle, (USHORT)(pcd->mode | CCD_BIT_FAST));

	if (shutter_gate) {
		set_shutter_enable(ccd_handle, 0);
#ifdef _APGDLL
		dwShutterOK = timeGetTime() + pcd->slice_time;
#endif
		}

// Set up camera register addresses
    Reg1 = pcd->base + CCD_REG01;
    Reg9 = pcd->base + CCD_REG09;
    Reg12 = pcd->base + CCD_REG12;

	i = psd->rowcnt;
	while (i) {
// Start delivering next line
	    tmpval = INPW(Reg12);
	    OUTPW(Reg1, tmpval | CCD_BIT_NEXT);
		OUTPW(Reg1, tmpval & ~CCD_BIT_NEXT);
			
// Wait for line done
	    if (poll_line_done(ccd_handle, pcd->line_timeout) != TRUE) {
			pcd->errorx = APERR_LINE;
	        pcd->error |= CCD_ERR_REG;
	        return CCD_ERROR;
	    	}

// Discard fifo pipeline pixels
#ifdef _APG_NET
		net_discwn(Reg9, (ULONG) pcd->skipc);
#else
		j = pcd->skipc;
		while (j--)
		    (void) INPW(Reg9);
#endif

// Transfer line data
		j = psd->colcnt;
#ifdef _APG_NET
		net_inpwn(Reg9, (ULONG)j, outbuf);
		outbuf += j;
#else
		while (j--)
		    *outbuf++ = INPW(Reg9);
#endif

		if (i == psd->rowcnt)
			set_slice_delay(ccd_handle, 0);
	
		i--;
	}
			
	if (overshoot > 0) {
		load_timer_and_vbinning (ccd_handle, pcd->timer, overshoot);
		// Start delivering next line
	    tmpval = INPW(Reg12);
	    OUTPW(Reg1, tmpval | CCD_BIT_NEXT);
		OUTPW(Reg1, tmpval & ~CCD_BIT_NEXT);
		// Wait for line done
	    if (poll_line_done(ccd_handle,pcd->line_timeout) != TRUE) {
			pcd->errorx = APERR_LINE;
	        pcd->error |= CCD_ERR_REG;
	        return CCD_ERROR;
	    	}
		}

	if (reverse_enable) {
		set_reverse_enable(ccd_handle, 1);
		load_timer_and_vbinning (ccd_handle, pcd->timer, psd->rowcnt);
		// Start delivering next line
	    tmpval = INPW(Reg12);
	    OUTPW(Reg1, tmpval | CCD_BIT_NEXT);
		OUTPW(Reg1, tmpval & ~CCD_BIT_NEXT);
		// Wait for line done
	    if (poll_line_done(ccd_handle, pcd->line_timeout) != TRUE) {
			pcd->errorx = APERR_LINE;
	        pcd->error |= CCD_ERR_REG;
	        return CCD_ERROR;
	    	}
		set_reverse_enable(ccd_handle, 0);
		}

	if (shutter_gate) {
#ifdef _APGDLL
		while (timeGetTime() < dwShutterOK)
			;
#endif
		set_shutter_enable(ccd_handle, 1);
		}

	if (fast)
		load_mode_bits(ccd_handle, (USHORT)(pcd->mode & CCD_MSK_FAST));

	load_slice_registers(ccd_handle, 0);

	set_slice_amp(ccd_handle, 0);

    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* reset_flush                                                              */
/*                                                                          */
/* starts normal camera flushing, should be called after aborting any       */
/* camera operation.                                                        */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
reset_flush (HCCD ccd_handle)
{
    STATUS rc;
    USHORT index = ccd_handle - 1;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

	activate_camera(ccd_handle);
	
    rc = load_line_count (ccd_handle,camdata[index].rows);
	if (rc != CCD_OK) {
		return CCD_ERROR;
		}

    /* reset and flush the controller */

    if (ccd_command(ccd_handle,CCD_CMD_RSTSYS,BLOCK,5) != CCD_OK) {
    	camdata[index].errorx = APERR_RSTSYS;
    	return CCD_ERROR;
    	}

    if (ccd_command(ccd_handle,CCD_CMD_FLSTRT,BLOCK,5) != CCD_OK) {
    	camdata[index].errorx = APERR_FLSTRT;
    	return CCD_ERROR;
    	}

    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* set_temp                                                                 */
/*                                                                          */
/* set the target temperature of the dewer. the low-level call handles the  */
/* conversion from degrees C to CCD units. the function parameter takes one */
/* of three command values: CCD_TMP_SET, CCD_TMP_AMB, or CCD_TMP_OFF.       */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
set_temp (HCCD ccd_handle,DOUBLE desired_temp,USHORT function)
{
    STATUS rc;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

	activate_camera(ccd_handle);
	
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
            return CCD_ERROR;
        }

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

STATUS DLLPROC
get_temp (HCCD ccd_handle,PSHORT temp_status,PDOUBLE temp_read)
{
    STATUS rc;
    USHORT tmpstat;
    USHORT tmpread;
    USHORT index = ccd_handle - 1;
    DOUBLE cal, scale;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

	activate_camera(ccd_handle);
	
    /* fetch the temp status bits */

    if (poll_temp_status(ccd_handle,&tmpstat) != TRUE) {
        return CCD_ERROR;
        }

    /* fetch and store the temp value, in degrees C */

    cal  = camdata[index].temp_cal;
    scale= camdata[index].temp_scale;
    tmpread = read_temp(ccd_handle);
    *temp_read = (tmpread - cal) / scale;

    /* interpret the temp status bits and set the status code */

	// test enable bit
    if (!(CCD_TMPOFF & tmpstat)) {
        *temp_status = CCD_TMP_OFF;
        return CCD_OK;
        }

	// enable = 1
	
	// test shutdown bit
    if ((CCD_TMPDONE & tmpstat) == CCD_TMPAMB) {
        *temp_status = CCD_TMP_RUP;
        return CCD_OK;
        }

    if ((CCD_TMPDONE & tmpstat) == CCD_TMPDONE) {
        *temp_status = CCD_TMP_DONE;
        return CCD_OK;
        }

    // enable = 1, shutdown = 0
    
    // test state bits
    if ((CCD_TMPSTUCK & tmpstat) == CCD_TMPSTUCK) {
        *temp_status = CCD_TMP_STUCK;
        return CCD_OK;
        }

    if ((CCD_TMPMAX & tmpstat) == CCD_TMPMAX) {
        *temp_status = CCD_TMP_MAX;
        return CCD_OK;
        }

    if ((CCD_TMPOK & tmpstat) == CCD_TMPOK) {
        *temp_status = CCD_TMP_OK;
        return CCD_OK;
        }

   	if (tmpread < camdata[index].reg[4])
       	*temp_status = CCD_TMP_RUP;
    else
       	*temp_status = CCD_TMP_RDN;

    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* get_timer                                                                */
/*                                                                          */
/* get the current timer and trigger status. trigger status is only valid   */
/* if the trigger mode is CCD_TRIGEXT, otherwise it will be returned as a   */
/* zero (FALSE) value.                                                      */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
get_timer (HCCD ccd_handle,PSHORT trigger,PSHORT timer)
{
    STATUS rc;
    USHORT index = ccd_handle - 1;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

	activate_camera(ccd_handle);
	
    if (camdata[index].trigger_mode == CCD_TRIGEXT) {
        rc = poll_got_trigger(ccd_handle,NOWAIT);
        *trigger = rc;
        }

    rc = poll_timer_status(ccd_handle,NOWAIT);
    *timer = rc;

    return CCD_OK;
}

STATUS DLLPROC
test_image (USHORT columns,
			USHORT rows,
			PDATA buffer)
{
	ULONG dwPix;
	USHORT uiVal = 0;

	dwPix = ((ULONG)columns) * rows;
	while (dwPix--)
		*buffer++ = uiVal++;
	return CCD_OK;
}

#ifdef WINUTIL
STATUS DLLPROC
get_acquire_priority_class(DWORD FAR *dwClass)
{
	*dwClass = dwApogeeClass;
	return (CCD_OK);
}

STATUS DLLPROC
set_acquire_priority_class(DWORD dwClass)
{
	if (dwClass >= CLASS_MAX)
		return (CCD_ERROR);
	dwApogeeClass = dwClass;
	return (CCD_OK);
}

STATUS DLLPROC
get_acquire_thread_priority(DWORD FAR *dwThread)
{
	*dwThread = dwApogeeThread;
	return (CCD_OK);
}

STATUS DLLPROC
set_acquire_thread_priority(DWORD dwThread)
{
	if (dwThread >= THREAD_MAX)
		return (CCD_ERROR);
	dwApogeeThread = dwThread;
	return (CCD_OK);
}
#endif
