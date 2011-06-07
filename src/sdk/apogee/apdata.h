/*==========================================================================*/
/* Apogee CCD Control API Internal Data Structures                          */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   12/22/96    jmh     Created new API library files               */
/*   1.10    6/23/97    ecd     changes for linux                           */
/*                                                                          */
/*==========================================================================*/


#ifndef _apdata
#define _apdata


#include "apccd.h"


/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  Geometry paramter and binning                                           */
/*                                                                          */
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
    SHORT       mode;           /* mode bits                                */
    SHORT       test;           /* test bits                                */
    SHORT       rows;           /* chip geometry - total rows (lines)       */
    SHORT       cols;           /* chip geometry - total columns (pix/row)  */
    SHORT       topb;           /* top rows hidden, >= 2                    */
    SHORT       botb;           /* bottom rows hidden, >= 2                 */
    SHORT       leftb;          /* left columns hidden, >= 2                */
    SHORT       rightb;         /* right columns hidden, >= 2               */
    SHORT       imgrows;        /* rows in ROI                              */
    SHORT       imgcols;        /* pixel width of ROI                       */
    SHORT       rowcnt;         /* binned ROI rows                          */
    SHORT       colcnt;         /* binned ROI columns                       */
    SHORT       aic;            /* after-image columns to skip              */
    SHORT       bic;            /* before-image columns to skip             */
    SHORT       hbin;           /* horizontal binning factor                */
    SHORT       vbin;           /* vertical binning factor                  */
    SHORT       bir;            /* before-image line count                  */
    SHORT       air;            /* after-image line count                   */
    SHORT       shutter_en;     /* shutter mode, enable or disable          */
    SHORT       trigger_mode;   /* trigger mode, external or normal         */
    SHORT       caching;        /* FIFO caching on/off                      */
    LONG	target_temp;    /* desired final temp of dewer              */
    LONG        timer;          /* timer count value                        */
    LONG	tscale;         /* timer calibration scaling factor *FP_SCALE*/
    SHORT       cable;          /* cable length mode, long or short         */
    LONG	temp_cal;       /* temp calibration factor                  */
    LONG	temp_scale;     /* temp scaling factor * FP_SCALE           */
    USHORT      reg[12];        /* controller register mirrors              */
    SHORT	pixbias;	/* 0 or 32768				    */
    USHORT      camreg;         /* Gdata for serial output to cam head      */
    USHORT      error;          /* contains error bits for particular chan  */
    USHORT	*tmpbuf;	/* vmalloc'd row buffer in kernel space     */
    } CAMDATA;

#define	FP_SCALE	100	/* fixed point scale factor		    */

/*--------------------------------------------------------------------------*/
/* control structure allocation control                                     */
/*--------------------------------------------------------------------------*/

void   init_camdata             (HCCD ccd_handle);
STATUS check_ccd                (HCCD ccd_handle);
STATUS load_ccd_data            (HCCD ccd_handle, USHORT mode);


#endif




/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: apdata.h,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $
 */
