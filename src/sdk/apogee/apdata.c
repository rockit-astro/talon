/*==========================================================================*/
/* Apogee CCD Control API Internal Data Structures Management Functions     */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   12/22/96    jmh     Created new API library files               */
/*   1.01    1/25/97    jmh     Modified camdata structure slightly         */
/*   1.02    6/23/97    ecd     changes for linux                           */
/*                                                                          */
/*==========================================================================*/

#include "apccd.h"
#include "apdata.h"


/*==========================================================================*/
/* Global data                                                              */
/*==========================================================================*/

CAMDATA     camdata[MAXCAMS];       /* control data for CCD controllers     */
USHORT      cam_current;            /* current active camera                */
USHORT      cam_count;              /* number of currently open cameras     */



/*==========================================================================*/
/* Data management functions                                                */
/*==========================================================================*/


/*--------------------------------------------------------------------------*/
/* check_ccd                                                                */
/*                                                                          */
/* Verifies that a CCD handle is valid by checking it against the control   */
/* data structures. Returns CCD_ERROR is handle is invalid.                 */
/*--------------------------------------------------------------------------*/

STATUS check_ccd (HCCD ccd_handle)
{
    USHORT i;

    for (i = 0; i < MAXCAMS; i++) {
        if (camdata[i].handle == ccd_handle)
            return CCD_OK;
        }

    return CCD_ERROR;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: apdata.c,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $"};
