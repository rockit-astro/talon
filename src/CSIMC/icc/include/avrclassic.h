#ifndef __AVRCLASSIC_H
#define __AVRCLASSIC_H

#include "avr_common.h"

/* Not all devices implement these registers.
 * Please refer to the Atmel device specifications
 *
 * BEWARE THAT THE BIT DEFINITIONS IS VALID ONLY FOR THE 4434/8535.
 * DOUBLE CHECK THE ATMEL DATA BOOK TO MAKE SURE THEY WORK FOR THE
 * DEVICES YOU ARE INTERESTED IN!!
 *
 */
/* IO Registers as memory mapped addresses
 */
#define SREG 	(*(volatile unsigned char *)0x5F)
	#define	SREG_GIE	0x80		/* Global Interrupt Enable. */
	#define	SREG_BCS	0x40		/* Bit Copy Storage. */
	#define	SREG_HCF	0x20		/* Half Carry Flag. */
	#define SREG_SB		0x10		/* Sign Bit. */
	#define SREG_TCO	0x08		/* Twos Complement Overflow. */
	#define	SREG_NEG	0x04		/* Negative Flag. */
	#define SREG_ZF		0x02		/* Zero Flag. */
	#define SREG_CF		0x01		/* Carry Flag. */
	
/* Stack Pointer (High/Low) */
#define SPH		(*(volatile unsigned char *)0x5E)
#define SPL		(*(volatile unsigned char *)0x5D)
	#define SP	(*(volatile unsigned int *)0x5D)

#define GIMSK	(*(volatile unsigned char *)0x5B)
#define GIFR	(*(volatile unsigned char *)0x5A)
#define TIMSK	(*(volatile unsigned char *)0x59)
#define TIFR	(*(volatile unsigned char *)0x58)

#define MCUCR	(*(volatile unsigned char *)0x55)
	#define	SRE		0x80	/* External Ram Enable. */
	#define	SRW		0x40	/* External Ram Wait State. */
	#define SME		0x20	/* Sleep Enable. */
	#define	SM_IDLE	0x00	/* Sleep - Idle */
	#define SM_PD	0x10	/* Power Down. */
	#define SM_PS	0x18	/* Power Save. */

#define MCUSR	(*(volatile unsigned char *)0x54)
	#define PORF	0x01	/* Power on reset. */
	#define	EXTRF	0x02	/* External Reset. */

#define TCCR0	(*(volatile unsigned char *)0x53)
	#define	CS00	0x00	/* Timer Counter Stopped. */
	#define CS01	0x01	/* Prescale = TCK0 */
	#define CS08	0x02	/* Prescale /8 */
	#define CS064	0x03	/* /64 */
	#define CS0256	0x04	/* /256 */
	#define CS01K	0x05	/* /1024 */
#define TCCR2	(*(volatile unsigned char *)0x45)
	#define	PWMX	0x40	/* PWM 0,2 Enable. */
	#define COMX_NC	0x00   	/* Compare Mode Disconnect output. */
	#define	COMX_T	0x10	/* Compare Mode Toggle Output. */
	#define COMX_C	0x20	/* Compare Mode Clear Output. */
	#define COMX_S	0x30	/* Compare Mode Set Output. */
	#define CTC02	0x08	/* Clear Timer 0 or 2 on compare match. */
	#define	CSX0	0x00	/* Timer Counter Stopped. */
	#define CSX1	0x01	/* Prescale = CK */
	#define CSX8	0x02	/*  /8 */
	#define CSX64	0x03	/* /64 */
	#define CSX256	0x04	/* /256 */
	#define CSX1K	0x05	/* /1024 */
	#define CS2F	0x06	/* External Falling Edge. */
	#define CS2R	0x07	/* External Rising Edge. */

#define TCNT0	(*(volatile unsigned char *)0x52)

#define TCCR1A	(*(volatile unsigned char *)0x4F)
	#define	COM1A_T	0x40	/* ToggleOC1A output line. */
	#define COM1A_C	0x80	/* Clear OC1A output line. */
	#define	COM1A_S	0xC0	/* Set   OC1A output line. */
	#define COM1B_T	0x10	/* ToggleOC1B output line. */
	#define COM1B_C	0x20	/* Clear OC1B output line. */
	#define	COM1B_S	0x30	/* Set   OC1B output line. */
	#define	PWM_8	0x01	/* 8 bit pwm mode. */
	#define PWM_9	0x02	/* 9 bit pwm mode. */
	#define PWM_10	0x03	/* 10 bit pwm mode. */

#define TCCR1B	(*(volatile unsigned char *)0x4E)
	#define ICNC1	0x80	/* Input noise canceller. */
	#define	ICES1	0x40	/* Select rising edge. */
	#define	CTC1	0x08	/* Clear Counter Timer 1 on Compare match. */
	#define CS1_1	0x01	/* Prescale /1. */
	#define CS1_8	0x02	/* Prescale /8. */
	#define	CS1_64	0x03	/* Prescale /64. */
	#define CS1_256	0x04	/* Prescale /256. */
	#define CS1_1k	0X05	/* Prescale /1024. */
	#define CS1_XR	0x06	/* External Pin T1, Rising edge. */
	#define CS1_XF	0x07	/* External Pin T1, Falling edge. */

/* Timer Counter 1 (High, Low)	 */
#define TCNT1H	(*(volatile unsigned char *)0x4D)
#define TCNT1L	(*(volatile unsigned char *)0x4C)
    #define TCNT1 (*(volatile unsigned int *)0x4C) 

/* Timer Counter 1 Output Compare Register (A,B,High,Low) */
#define OCR1AH	(*(volatile unsigned char *)0x4B)
#define OCR1AL	(*(volatile unsigned char *)0x4A)
#define OCR1BH	(*(volatile unsigned char *)0x49)
#define OCR1BL	(*(volatile unsigned char *)0x48)
    #define OCR1A (*(volatile unsigned int *)0x4A) 
    #define OCR1B (*(volatile unsigned int *)0x48) 

#define ICR1H	(*(volatile unsigned char *)0x45)
#define ICR1L	(*(volatile unsigned char *)0x44)
	#define ICR1 	(*(volatile unsigned int *)0x44) 

/* Watchdog Timer Control Register. */
#define WDTCR	(*(volatile unsigned char *)0x41)
	#define	WDTOE	0x10	/* Turn Off Enable. */
	#define	WDE		0x08	/* Enable. */
	#define	WDP16K	0x00	/* 16K Cycles. */
	#define WDP32K	0x01	/* 32K */
	#define	WDP64K	0x02	/* 64K */
	#define	WDP128K	0x03	/* 128K */
	#define WDP256K	0x04	/* 256K */
	#define WDP512K	0x05	/* 512K */
	#define	WDP_1M	0x06	/* 1024K */
	#define WDP_2M	0x07	/* 2048K */

/* EPROM Read Write Address (Low, High) */
#define EEARH	(*(volatile unsigned char *)0x3F)
#define EEARL	(*(volatile unsigned char *)0x3E)
	/* EEPROM, 16 bit address */
	#define EEAR	(*(volatile unsigned int *)0x3E)

/* EPROM Data Register. */
#define EEDR	(*(volatile unsigned char *)0x3D)
/* EPROM Control Register. */
#define EECR	(*(volatile unsigned char *)0x3C)
	#define	EERIE	0x08	/* EPROM Ready Interrupt Enable. */
	#define	EEMWE	0x04	/* EPROM Master Write Enable. */
	#define	EEWE	0x02	/* EPROM Write Enable. */
	#define EERE	0x01	/* EPROM Read Enable. */

#define PORTA	(*(volatile unsigned char *)0x3B)
#define DDRA	(*(volatile unsigned char *)0x3A)
#define PINA	(*(volatile unsigned char *)0x39)

#define PORTB	(*(volatile unsigned char *)0x38)
#define DDRB	(*(volatile unsigned char *)0x37)
#define PINB	(*(volatile unsigned char *)0x36)

#define PORTC	(*(volatile unsigned char *)0x35)

#define DDRC	(*(volatile unsigned char *)0x34)
#define PINC	(*(volatile unsigned char *)0x33)

#define PORTD	(*(volatile unsigned char *)0x32)
#define DDRD	(*(volatile unsigned char *)0x31)
#define PIND	(*(volatile unsigned char *)0x30)
/* SPI Data Register */
#define SPDR	(*(volatile unsigned char *)0x2F)	/* SPI Data Register. */
/* SPI Status Register */
#define SPSR	(*(volatile unsigned char *)0x2E)
	#define	SPIF	0x80	/* SPI Interrupt Flag. */
	#define	WCOL	0x40	/* Write Collision. */
/* SPI Control Register. */
#define SPCR	(*(volatile unsigned char *)0x2D)
	#define	SPIE	0x80	/* SPI Interrupt Enable. */
	#define NSPIE	0x7F	/* ~SPIE */
	#define SPE		0x40	/* SPI Enable. */
	#define NSPE	0xBF	/* ~SPE */
	#define	DORD	0x20	/* LSbit first if 1. */
	#define	MSTR	0x10	/* Master 1 vs Slave 0. */
	#define	CPOL	0x08	/* Clock idles high when set, low when 0 */
	#define	CPHA	0x04	/* Clock Phase 0 = Setup High Latch Low 1 = setup low latch high. */
	#define	SPR4  	0x00	/* clock rate / 4 */
	#define SPR16	0x01	/* /16 */
	#define SPR64	0x02	/* /64 */
	#define SPR128	0x03	/* /128 */
/* UART Data Register. */
#define UDR		(*(volatile unsigned char *)0x2C)
/* UART Status Register. */
#define USR		(*(volatile unsigned char *)0x2B)
	#define	RXC		0x80	/* Receive Complete.	(Read UDR clears) */
	#define	TXC		0x40	/* Transmit Complete.	(Write TXC = 1 clears) */
	#define	UDRE	0x20	/* Data Register Empty. */
	#define FE		0x10	/* Framing Error. */
	#define	DOR		0x08	/* Overrun Error. */
/* UART Control Register.	 */
#define UCR		(*(volatile unsigned char *)0x2A)
	#define	RXCIE	0x80	/* Receive Interrupt Enable. */
	#define NRXCIE	0x7F	/* ~RXCIE. */
	#define	TXCIE	0x40	/* Transmit Interrupt Enable. */
	#define	NTXCIE	0xBF	/* ~TXCIE. */
	#define	UDRIE	0x20	/* Data Register Interrupt Enable. */
	#define NUDRIE	0xDF
	#define	RXEN	0x10	/* Receive Enable. */
	#define	TXEN	0x08	/* Transmit Enable. */
	#define	CHR9	0x04	/* 9 Bit Characters. */
	#define	RXB8	0x02	/* Receive bit 8. */
	#define	TXB8	0x01	/* Transmit bit 8. */
/* UART Baud Rate Register. */
#define UBRR	(*(volatile unsigned char *)0x29)
/* Analog Comparator Control and Status Register. */
#define ACSR	(*(volatile unsigned char *)0x28)
	#define	ACD		0x80	/* Analog Comparator Disable. */
	#define	ACO		0x20	/* Analog Comparator Output. */
	#define	ACI		0x10	/* Analog Comparator Interrupt Flag. */
	#define	ACIE	0x08	/* */
	#define	ACIC	0x04	/* */
	#define	ACIT	0x00	/* Interrupt on Toggle. */
	#define	ACIF	0x02	/* Interrupt on Falling Edge. */
	#define	ACIR	0x03	/* Interrupt on Rising Edge. */
	#define	NACIR	0xFC	/* Not ACIR	- Clear.	 */

#endif
