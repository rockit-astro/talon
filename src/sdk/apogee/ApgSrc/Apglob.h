/*==========================================================================*/
/* Apogee CCD Control API Internal Data Structures                          */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   12/22/96    jmh     Created new API library files               */
/*                                                                          */
/*==========================================================================*/

#include "apccd.h"
#include "apdata.h"

#ifndef _apglob
#define _apglob

#ifdef __cplusplus
extern "C" {
#endif

extern CAMDATA  camdata[MAXCAMS];   /* control data for CCD controllers     */
extern SLICEDATA slicedata[MAXCAMS];/* slice data for CCD controllers       */
extern CAMDATA *ptr_current;        /* pointer to current active camera     */
extern USHORT   cam_count;          /* number of currently open cameras     */
extern USHORT   uDefaultMode;		/* Default mode bits */
extern USHORT   uDefaultTest;		/* Default test bits */

extern void activate_camera(HCCD ccd_camera);

#ifdef __cplusplus
}
#endif

#endif
