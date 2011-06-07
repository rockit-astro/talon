/*==========================================================================*/
/* Apogee CCD Control API Internal Data Structures                          */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   12/22/96    jmh     Created new API library files               */
/*                                                                          */
/*==========================================================================*/


#ifndef _apdata
#define _apdata


#include "apccd.h"



/*--------------------------------------------------------------------------*/
/* control structure allocation control                                     */
/*--------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

void   init_ccd                 (void);
HCCD   allocate_ccd             (void);
STATUS delete_ccd               (HCCD ccd_handle);
STATUS check_ccd                (HCCD ccd_handle);

#ifdef __cplusplus
}
#endif

#endif



