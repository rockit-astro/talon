#ifndef __HC16_H
#define __HC16_H	1

/* these routines access memory in a different page
 * typically used for accessing the HW registers
 */
extern unsigned char peek(int zk, unsigned int addr);
extern unsigned short peekw(int zk, unsigned int addr);
extern void poke(int zk, unsigned int addr, unsigned char byt);
extern void pokew(int zk, unsigned int addr, unsigned int wor);

/* These values are for a 16Mhz clock
 */
typedef enum {
	BAUD9600=0x37,
	} BaudRate;
void setbaud(BaudRate);

#ifndef INTR_ON
#define INTR_ON()	asm("andp #0xFF1F")
#define INTR_OFF()	asm("orp #0x00E0")
#endif

#ifndef bit
#define bit(x)	(1 << (x))
#endif

#ifdef _SCI
#define SCCR0	0xFC08	/* SCI control register 0 */
#define SCCR1	0xFC0A	/* SCI control register 1 */
#define SCSR	0xFC0C	/* SCI status register */
#define SCDR	0xFC0E	/* SCI data register (FULL WORD) */
#endif

#endif
