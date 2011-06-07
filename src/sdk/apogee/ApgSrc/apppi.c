/*==========================================================================*/
/* Apogee CCD Control API - PPI Interface I/O Functions                     */
/*                                                                          */
/* (c) 1998 GKR Computer Consulting                                         */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   03/13/98    gkr     Created                                     */
/*   1.01   10/06/99    gkr     Modified for c0_repeat to hold data latch   */
/*   1.02   11/24/99    gkr     Modified to use byte I/O                    */
/*                                                                          */
/*==========================================================================*/

#ifdef _APGDLL
#include <windows.h>
#include <time.h>
#endif

#ifndef LINUX
#include <conio.h>
#endif

#include "apccd.h"

// comment out to prevent ppi_select from being called at each I/O
#define ALWAYS_SELECT

static USHORT cam_reg = 0xffff;
static USHORT ctr_reg;
USHORT c0_repeat = 1;
USHORT ecp_port = 0;

static USHORT ppi_base;
#define ppi_stat	(ppi_base+1)
#define ppi_ctrl	(ppi_base+2)

#define INPPI(a)	(_inp((USHORT)(a)) & 0xFF)
#define OUTPPI(a,b)	_outp((USHORT)(a),(USHORT)(b))

static USHORT ppi_offset;

#define C0			1
#define C1			2
#define C2			4
#define C3			8
#define C5			32
#define C7			128

BOOL DLLPROC
ppi_init(USHORT base, USHORT offset)
{
	ppi_base = base;
	ppi_offset = offset;
// force next ppi_select
	cam_reg = 0xffff;
	if (ecp_port)
		OUTPPI(base+0x402, 0x34);
	return (TRUE);
}

void DLLPROC
ppi_fini(void)
{
}

void DLLPROC
ppi_select(USHORT port)
{
	USHORT i;

	// Save base address to compare
	cam_reg = port;

	// Subtract base and add register offset to get camera register
	port = port - ppi_base + ppi_offset;

	// Prepare control register for register write
	ctr_reg = C1 | C2 | C3;
	OUTPPI(ppi_ctrl,ctr_reg);

	// Write register low byte
	OUTPPI(ppi_base,port);

	// Latch and hold register value
	ctr_reg |= C0;
	for (i=0; i<c0_repeat; i++)
		OUTPPI(ppi_ctrl,ctr_reg);
	ctr_reg &= ~C0;
	OUTPPI(ppi_ctrl,ctr_reg);

	// Prepare for data transfers
	ctr_reg &= ~C1;
	OUTPPI(ppi_ctrl,ctr_reg);
}

static USHORT
ppi_readw(void)
{
	USHORT data;
	USHORT i;
	
	// Prepare control register for data read
	ctr_reg = C2 | C3 | C5 | C7;
	OUTPPI(ppi_ctrl,ctr_reg);
	
	// Read from register
	ctr_reg &= ~C2;
	OUTPPI(ppi_ctrl,ctr_reg);
	
	// Latch data
	ctr_reg |= C0;
	for (i=0; i<c0_repeat; i++)
		OUTPPI(ppi_ctrl,ctr_reg);
	
	// Read low byte
	data = INPPI(ppi_base);
	
	// Reset latch
	ctr_reg &= ~C0;
	OUTPPI(ppi_ctrl,ctr_reg);
	
	// Select high byte
	ctr_reg &= ~C3;
	OUTPPI(ppi_ctrl,ctr_reg);
	
	// Latch data
	ctr_reg |= C0;
	for (i=0; i<c0_repeat; i++)
		OUTPPI(ppi_ctrl,ctr_reg);
	
	// Read high byte
	data |= (INPPI(ppi_base) << 8);
	
	// Prepare for next read
	ctr_reg &= ~C0;
	OUTPPI(ppi_ctrl,ctr_reg);
	ctr_reg |= C2;
	OUTPPI(ppi_ctrl,ctr_reg);
	ctr_reg |= C3;
	OUTPPI(ppi_ctrl,ctr_reg);

	return (data);
}

static void
ppi_writew(USHORT word)
{
	USHORT i;

	// Prepare control register for data write
	ctr_reg = C2 | C3;
	OUTPPI(ppi_ctrl, ctr_reg);

	// Write data low byte
	OUTPPI(ppi_base, word & 0xff);

	// Latch data
	ctr_reg |= C0;
	for (i=0; i<c0_repeat; i++)
		OUTPPI(ppi_ctrl,ctr_reg);
	ctr_reg &= ~C0;
	OUTPPI(ppi_ctrl,ctr_reg);
	
	// Select data high byte
	ctr_reg &= ~C3;
	OUTPPI(ppi_ctrl,ctr_reg);
	
	// Write data high byte
	OUTPPI(ppi_base,word >> 8);
	
	// Latch data
	ctr_reg |= C0;
	for (i=0; i<c0_repeat; i++)
		OUTPPI(ppi_ctrl,ctr_reg);
	ctr_reg &= ~C0;
	OUTPPI(ppi_ctrl,ctr_reg);
}

USHORT DLLPROC
ppi_inpw(USHORT port)
{
#ifndef ALWAYS_SELECT
	if (port != cam_reg)
#endif
		ppi_select(port);
	return(ppi_readw());
}

void DLLPROC
ppi_outpw(USHORT port, USHORT data)
{
#ifndef ALWAYS_SELECT
	if (port != cam_reg)
#endif
		ppi_select(port);
	ppi_writew(data);
}
