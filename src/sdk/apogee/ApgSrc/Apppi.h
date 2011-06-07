/*==========================================================================*/
/* Apogee CCD Control API - Interface I/O Functions                         */
/*                                                                          */
/* (c) 1998 GKR Computer Consulting                                         */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   03/13/98    gkr     Created                                     */
/*                                                                          */
/*==========================================================================*/

extern USHORT c0_repeat;
extern USHORT ecp_port;

#ifdef _APG_PPI

// Initialize PPI interface using "base" address and register offset
BOOL DLLPROC ppi_init(USHORT base, USHORT offset);

// Deinitialize PPI interface
void DLLPROC ppi_fini(void);

// Select camera register
void DLLPROC ppi_select(USHORT reg);

// Input word from camera register
USHORT DLLPROC ppi_inpw(USHORT reg);

// Output word to camera register
void DLLPROC ppi_outpw(USHORT reg, USHORT data);

#endif
