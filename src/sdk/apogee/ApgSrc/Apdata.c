/*==========================================================================*/
/* Apogee CCD Control API Internal Data Structures Management Functions     */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   12/22/96    jmh     Created new API library files               */
/*   1.01    1/25/97    jmh     Modified camdata structure slightly         */
/*                                                                          */
/*==========================================================================*/

#ifdef _APGDLL
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "apccd.h"
#include "apdata.h"


/*==========================================================================*/
/* Global data                                                              */
/*==========================================================================*/

CAMDATA     camdata[MAXCAMS];       /* control data for CCD controllers     */
SLICEDATA   slicedata[MAXCAMS];     /* control data for CCD slices          */
CAMDATA    *ptr_current;            /* pointer to current active camera     */
USHORT      cam_count;              /* number of currently open cameras     */


/*==========================================================================*/
/* Data management functions                                                */
/*==========================================================================*/

/*--------------------------------------------------------------------------*/
/* init_ccd                                                                 */
/*                                                                          */
/* Initializes the CCD control sub-system by clearing the control data      */
/* structures and resetting the control variables. CAUTION: There's nothing */
/* to prevent this routine from being called _after_ the system is up and   */
/* running. It WILL clobber everything, and it WILL do it with extreme      */
/* prejudice. It should only be used when the CCD sub-system is first       */
/* initialized. You've been warned.                                         */
/*--------------------------------------------------------------------------*/

void init_ccd (void)
{
    USHORT i;

    /* make sure all CCD structures are cleared */

    for (i = 0; i < MAXCAMS; i++) {
    	memset(&camdata[i], 0, sizeof(CAMDATA));
        }

    cam_count   = 0;
}

/*--------------------------------------------------------------------------*/
/* allocate_ccd                                                             */
/*                                                                          */
/* Locates an open control block in the structure array and assigns a       */
/* handle value which is derived from the block's location in the array.    */
/* Returns a null handle value (zero) if there are no free blocks.          */
/*--------------------------------------------------------------------------*/

HCCD allocate_ccd(void)
{
    USHORT i;

    /* locate first free control block and use it */

    for (i = 0; i < MAXCAMS; i++) {
        if (camdata[i].handle == 0) {
            camdata[i].handle = i + 1;
            cam_count++;
            return i + 1;
            }
        }

    return 0;
}


/*--------------------------------------------------------------------------*/
/* delete_ccd                                                               */
/*                                                                          */
/* Locates an allocated block using the CCD handle value and then clears it */
/* for re-use. If the handle was not found CCD_ERROR is returned.           */
/*--------------------------------------------------------------------------*/

STATUS delete_ccd (HCCD ccd_handle)
{
    USHORT i;

    /* find specified control block and clear it */

    for (i = 0; i < MAXCAMS; i++) {
        if (camdata[i].handle == ccd_handle) {
        	memset(&camdata[i], 0, sizeof(CAMDATA));
            return CCD_OK;
            }
        }

    return CCD_ERROR;
}


/*--------------------------------------------------------------------------*/
/* check_ccd                                                                */
/*                                                                          */
/* Verifies that a CCD handle is valid by checking it against the control   */
/* data structures. Returns CCD_ERROR is handle is invalid.                 */
/*--------------------------------------------------------------------------*/

STATUS check_ccd (HCCD ccd_handle)
{
    USHORT i;

    for (i = 0; i < MAXCAMS; i++)
        if (camdata[i].handle == ccd_handle)
            return CCD_OK;

    return CCD_ERROR;
}
