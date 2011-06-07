/*==========================================================================*/
/* Apogee CCD Control API Extended Errors                                   */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   03/20/98    gkr     Created                                     */
/*                                                                          */
/*==========================================================================*/

#ifndef _aperror
#define _aperror

#define	APERR_NONE		0
#define	APERR_CFGNAME	1
#define	APERR_CFGBASE	2
#define	APERR_CFGROWS	3
#define	APERR_CFGCOLS	4
#define	APERR_NOCFG		5
#define APERR_FRAME		6
#define APERR_CALCAIC	7
#define APERR_CALCAIR	8
#define APERR_AIC		9
#define APERR_BIC		10
#define APERR_HBIN		11
#define APERR_VBIN		12
#define APERR_COLCNT	13
#define APERR_ROWCNT	14
#define APERR_CMDACK	15
#define APERR_RSTSYS	16
#define APERR_FLSTRT	17
#define APERR_SKIPR		18
#define APERR_CLOSE		19
#define APERR_TRIGOFF	20
#define APERR_LINACK	21
#define APERR_LINE		22
#define APERR_PDATA		23
#define APERR_TEMP		24
#define APERR_TSCALE	25
#define APERR_SLICE     26
#define APERR_CFGSLICE  27
#define APERR_SKIPC     28

#endif
