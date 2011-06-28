#ifndef __HC11_H
#define __HC11_H	1

/* base address of register block, change this if you relocate the register
 * block. This is from an A8. May need to be changed for other HC11 members
 * or if you relocate the IO base address.
 */
#define _IO_BASE	0x1000
#define PORTA	*(unsigned char volatile *)(_IO_BASE + 0x00)
#define PIOC	*(unsigned char volatile *)(_IO_BASE + 0x02)
#define PORTC	*(unsigned char volatile *)(_IO_BASE + 0x03)
#define PORTB	*(unsigned char volatile *)(_IO_BASE + 0x04)
#define PORTCL	*(unsigned char volatile *)(_IO_BASE + 0x05)
#define DDRC	*(unsigned char volatile *)(_IO_BASE + 0x07)
#define PORTD	*(unsigned char volatile *)(_IO_BASE + 0x08)
#define DDRD	*(unsigned char volatile *)(_IO_BASE + 0x09)
#define PORTE	*(unsigned char volatile *)(_IO_BASE + 0x0A)
#define CFORC	*(unsigned char volatile *)(_IO_BASE + 0x0B)
#define OC1M	*(unsigned char volatile *)(_IO_BASE + 0x0C)
#define OC1D	*(unsigned char volatile *)(_IO_BASE + 0x0D)
#define TCNT	*(unsigned short volatile *)(_IO_BASE + 0x0E)
#define TIC1	*(unsigned short volatile *)(_IO_BASE + 0x10)
#define TIC2	*(unsigned short volatile *)(_IO_BASE + 0x12)
#define TIC3	*(unsigned short volatile *)(_IO_BASE + 0x14)
#define TOC1	*(unsigned short volatile *)(_IO_BASE + 0x16)
#define TOC2	*(unsigned short volatile *)(_IO_BASE + 0x18)
#define TOC3	*(unsigned short volatile *)(_IO_BASE + 0x1A)
#define TOC4	*(unsigned short volatile *)(_IO_BASE + 0x1C)
#define TOC5	*(unsigned short volatile *)(_IO_BASE + 0x1E)
#define TCTL1	*(unsigned char volatile *)(_IO_BASE + 0x20)
#define TCTL2	*(unsigned char volatile *)(_IO_BASE + 0x21)
#define TMSK1	*(unsigned char volatile *)(_IO_BASE + 0x22)
#define TFLG1	*(unsigned char volatile *)(_IO_BASE + 0x23)
#define TMSK2	*(unsigned char volatile *)(_IO_BASE + 0x24)
#define TFLG2	*(unsigned char volatile *)(_IO_BASE + 0x25)
#define PACTL	*(unsigned char volatile *)(_IO_BASE + 0x26)
#define PACNT	*(unsigned char volatile *)(_IO_BASE + 0x27)
#define SPCR	*(unsigned char volatile *)(_IO_BASE + 0x28)
#define SPSR	*(unsigned char volatile *)(_IO_BASE + 0x29)
#define SPDR	*(unsigned char volatile *)(_IO_BASE + 0x2A)
#define BAUD	*(unsigned char volatile *)(_IO_BASE + 0x2B)
#define SCCR1	*(unsigned char volatile *)(_IO_BASE + 0x2C)
#define SCCR2	*(unsigned char volatile *)(_IO_BASE + 0x2D)
#define SCSR	*(unsigned char volatile *)(_IO_BASE + 0x2E)
#define SCDR	*(unsigned char volatile *)(_IO_BASE + 0x2F)
#define ADCTL	*(unsigned char volatile *)(_IO_BASE + 0x30)
#define ADR1	*(unsigned char volatile *)(_IO_BASE + 0x31)
#define ADR2	*(unsigned char volatile *)(_IO_BASE + 0x32)
#define ADR3	*(unsigned char volatile *)(_IO_BASE + 0x33)
#define ADR4	*(unsigned char volatile *)(_IO_BASE + 0x34)
#define OPTION	*(unsigned char volatile *)(_IO_BASE + 0x39)
#define COPRST	*(unsigned char volatile *)(_IO_BASE + 0x3A)
#define PPROG	*(unsigned char volatile *)(_IO_BASE + 0x3B)
#define HPRIO	*(unsigned char volatile *)(_IO_BASE + 0x3C)
#define INIT	*(unsigned char volatile *)(_IO_BASE + 0x3D)
#define TEST1	*(unsigned char volatile *)(_IO_BASE + 0x3E)
#define CONFIG	*(unsigned char volatile *)(_IO_BASE + 0x3F)

unsigned int read_sci(void);
unsigned char read_spi(void);
void write_eeprom(unsigned char *addr, unsigned char c);
void write_sci(unsigned int);
void write_spi(unsigned char);

/* These values are for a 8Mhz clock
 * 0x3? set the SCP1|SCP0 to 0x3
 */
typedef enum {
	BAUD9600 = 0x30, BAUD4800 = 0x31, BAUD2400 = 0x32, 
	BAUD1200 = 0x33, BAUD600 = 0x34, BAUD300 = 0x35
	} BaudRate;
void setbaud(BaudRate);

#ifndef INTR_ON
#define INTR_ON()	asm("	cli")
#define INTR_OFF()	asm("	sei")
#endif

#ifndef bit
#define bit(x)	(1 << (x))
#endif

#ifdef _SCI
/* SCI bits */
#define RDRF	bit(5)
#define TDRE	bit(7)
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
