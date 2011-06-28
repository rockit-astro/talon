#ifndef __HC912B32_H
#define __HC912B32_H	1

/* base address of register block, change this if you relocate the register
 * block. This is for 912B32, 812A4 contains a subset.
 * Edited by Sebastian Valouch  
 *
 *
/
#define _IO_BASE	0
#define _P(off)		*(unsigned char volatile *)(_IO_BASE + off)
#define _LP(off)	*(unsigned short volatile *)(_IO_BASE + off)

#define PORTA	_P(0x00)
#define	PORTB	_P(0x01)
#define	DDRA	_P(0x02)
#define	DDRB	_P(0x03)
#define	PORTE	_P(0x08)
#define	DDRE	_P(0x09)
#define	PEAR	_P(0x0A)
#define	MODE	_P(0x0B)
#define	PUCR	_P(0x0C)
#define	RDRIV	_P(0x0D)
#define	INITRM	_P(0x10)
#define	INITRG	_P(0x11)
#define	INITEE	_P(0x12)
#define	MISC	_P(0x13)
#define	RTICTL	_P(0x14)
#define	RTIFLG	_P(0x15)
#define	COPCTL	_P(0x16)
#define	COPRST	_P(0x17)
#define	ITST0	_P(0x18)
#define	ITST1	_P(0x19)
#define	ITST2	_P(0x1A)
#define	ITST3	_P(0x1B)
#define	INTCR	_P(0x1E)
#define	HPRIO	_P(0x1F)
#define	BRKCT0	_P(0x20)
#define	BRKCT1	_P(0x21)
#define	BRKAH	_P(0x22)
#define	BRKAL	_P(0x23)
#define	BRKDH	_P(0x24)
#define	BRKDL	_P(0x25)
#define	PWCLK	_P(0x40)
#define	PWPOL	_P(0x41)
#define	PWEN	_P(0x42)
#define	PWPRES	_P(0x43)
#define	PWSCAL0	_P(0x44)
#define	PWSCNT0	_P(0x45)
#define	PWSCAL1	_P(0x46)
#define	PWSCNT1	_P(0x47)
#define	PWCNT0	_P(0x48)
#define	PWCNT1	_P(0x49)
#define	PWCNT2	_P(0x4A)
#define	PWCNT3	_P(0x4B)
#define	PWPER0	_P(0x4C)
#define	PWPER1	_P(0x4D)
#define	PWPER2	_P(0x4E)
#define	PWPER3	_P(0x4F)
#define	PWDTY0	_P(0x50)
#define	PWDTY1	_P(0x51)
#define	PWDTY2	_P(0x52)
#define	PWDTY3	_P(0x53)
#define	PWCTL	_P(0x54)
#define	PWTST	_P(0x55)
#define	PORTP	_P(0x56)
#define	DDRP	_P(0x57)
#define	ATDCTL0	_P(0x60)
#define	ATDCTL1	_P(0x61)
#define	ATDCTL2	_P(0x62)
#define	ATDCTL3	_P(0x63)
#define	ATDCTL4	_P(0x64)
#define	ATDCTL5	_P(0x65)
#define	ATDSTAT	_LP(0x66)
#define	ATDTEST	_LP(0x68)
#define	PORTAD	_P(0x6F)
#define	ADR0H	_P(0x70)
#define	ADR1H	_P(0x72)
#define	ADR2H	_P(0x74)
#define	ADR3H	_P(0x76)
#define	ADR4H	_P(0x78)
#define	ADR5H	_P(0x7A)
#define	ADR6H	_P(0x7C)
#define	ADR7H	_P(0x7E)
#define	TIOS	_P(0x80)
#define	CFORC	_P(0x81)
#define	OC7M	_P(0x82)
#define	OC7D	_P(0x83)
#define	TCNT	_LP(0x84)
#define	TSCR	_P(0x86)
#define	TQCR	_P(0x87)
#define	TCTL1	_P(0x88)
#define	TCTL2	_P(0x89)
#define	TCTL3	_P(0x8A)
#define	TCTL4	_P(0x8B)
#define	TMSK1	_P(0x8C)
#define	TMSK2	_P(0x8D)
#define	TFLG1	_P(0x8E)
#define	TFLG2	_P(0x8F)
#define	TC0		_LP(0x90)
#define	TC1		_LP(0x92)
#define	TC2		_LP(0x94)
#define	TC3		_LP(0x96)
#define	TC4		_LP(0x98)
#define	TC5		_LP(0x9A)
#define	TC6		_LP(0x9C)
#define	TC7		_LP(0x9E)
#define	PACTL	_P(0xA0)
#define	PAFLG	_P(0xA1)
#define	PACNT	_LP(0xA2)
#define	TIMTST	_P(0xAD)
#define	PORTT	_P(0xAE)
#define	DDRT	_P(0xAF)
#define SC0BD	_LP(0xC0)
#define	SC0BDH	_P(0xC0)
#define	SC0BDL	_P(0xC1)
#define	SC0CR1	_P(0xC2)
#define	SC0CR2	_P(0xC3)
#define	SC0SR1	_P(0xC4)
#define	SC0SR2	_P(0xC5)
#define	SC0DRH	_P(0xC6)
#define	SC0DRL	_P(0xC7)
#define SC1BD	_LP(0xC8)
#define	SC1BDH	_P(0xC8)
#define	SC1BDL	_P(0xC9)
#define	SC1CR1	_P(0xCA)
#define	SC1CR2	_P(0xCB)
#define	SC1SR1	_P(0xCC)
#define	SC1SR2	_P(0xCD)
#define	SC1DRH	_P(0xCE)
#define	SC1DRL	_P(0xCF)
#define	SP0CR1	_P(0xD0)
#define	SP0CR2	_P(0xD1)
#define	SP0BR	_P(0xD2)
#define	SP0SR	_P(0xD3)
#define	SP0DR	_P(0xD5)
#define	PORTS	_P(0xD6)
#define	DDRS	_P(0xD7)
#define	EEMCR	_P(0xF0)
#define	EEPROT	_P(0xF1)
#define	EETST	_P(0xF2)
#define	EEPROG	_P(0xF3)
#define	FEELCK	_P(0xF4)
#define	FEEMCR	_P(0xF5)
#define	FEETST	_P(0xF6)
#define	FEECTL	_P(0xF7)
#define	BCR1	_P(0xF8)
#define	BSVR	_P(0xF9)
#define	BCR2	_P(0xFA)
#define	BDR	_P(0xFB)
#define	BARD	_P(0xFC)
#define	DLCSCR	_P(0xFD)
#define	PORTDLC	_P(0xFE)
#define	DDRDLC	_P(0xFF)
/* These values are for a 8Mhz clock
 */
typedef enum {
	BAUD38K = 13, BAUD19K = 26, BAUD14K = 35,
	BAUD9600 = 52, BAUD4800 = 104, BAUD2400 = 208, 
	BAUD1200 = 417, BAUD600 = 833, BAUD300 = 2273
	} BaudRate;
void setbaud(BaudRate);

#ifndef INTR_ON
#define INTR_ON()	asm("cli")
#define INTR_OFF()	asm("sei")
#endif

#ifndef bit
#define bit(x)	(1 << (x))
#endif

#ifdef _SCI
/* SCI bits */
#define TE		bit(3)
#define RE		bit(2)
#define TDRE	bit(7)
#define TC		bit(6)
#define RDRF	bit(5)
#define T8		bit(6)
#define R8		bit(7)
#endif

#ifdef _SPI
/* SPI bits */
#define MSTR	bit(4)
#define SPE		bit(6)
#define SPIF	bit(7)
#endif

#ifdef _EEPROM
/* EEPROM */
#define EEPGM	bit(0)
#define EELAT	bit(1)
#endif

#endif
