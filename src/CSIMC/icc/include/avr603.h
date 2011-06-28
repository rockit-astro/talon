#ifndef __AVR603_H
#define __AVR603_H

#include "avr_common.h"

/* For the ATmega603/103 AVR microcontroller
 */
// SRAM Addresses
#define SREG 	(*(volatile unsigned char *)0x5F)
	#define	SREG_GIE	0x80		// Global Interrupt Enable. 
    #define	SREG_BCS	0x40		// Bit Copy Storage. 
	#define	SREG_HCF	0x20		// Half Carry Flag
	#define SREG_SB		0x10		// Sign Bit.
	#define SREG_TCO	0x08		// Twos Complement Overflow.
	#define	SREG_NEG	0x04		// Negative Flag. 
	#define SREG_ZF		0x02		// Zero Flag. 
	#define SREG_CF		0x01		// Carry Flag. 
	
// Stack Pointer (High/Low) 
#define SPH		(*(volatile unsigned char *)0x5E)
#define SPL		(*(volatile unsigned char *)0x5D)
	#define SP	(*(volatile unsigned int *)0x5D)

#define XDIV	(*(volatile unsigned char *)0x5C)		// ATmega Crystal Divide Register. 
	#define	XDIVEN		0x80	// Enable Crystal Divide. 
	#define	XDVAL		0x7F	// XTAL / (129 - XDVAL ). 

#define RAMPZ	(*(volatile unsigned char *)0x5B)
	#define	RAMPZ0		0x01		// Ram Page		 
#define EICR	(*(volatile unsigned char *)0x5A)
	#define EICR_L		0x00	// And for Low interrupt. 
	#define EICR_R		0x10	// AND for Falling edge. 
	#define EICR_F		0x11	// AND for Rising edge. 
	#define	ISC7		0xC0	// Interrupt 7 
	#define	ISC6		0x30	// Interrupt 6 
	#define	ISC5		0x0C	// Interrupt 5 
	#define	ISC4		0x03	// Interrupt 4 

#define EIMSK	(*(volatile unsigned char *)0x59)
	#define	EIM7		0x80	// External Interrupt 7 Enable 
	#define	EIM6		0x40
	#define EIM5		0x20
	#define EIM4		0x10
	#define EIM3		0x08
	#define	EIM2		0x04
	#define EIM1		0x02
	#define	EIM0		0x01

#define EIFR	(*(volatile unsigned char *)0x58)
	#define	EIF7		0x08
	#define EIF6		0x04
	#define EIF5		0x02
	#define EIF4		0x01

#define TIMSK	(*(volatile unsigned char *)0x57)
	#define	OCIE2		0x80	// Timer Counter 2 Output Compare Interrupt Enable. 
	#define	TOIE2		0x40	// Timer Counter 2 Overflow Interrupt Enable. 
	#define	TICIE1		0x20	// Timer Counter 1 Input Capture Interrupt Enable. 
	#define	OCIE1A		0x10	// Timer Counter 1 Output CompareA match Interrupt Enable. 
	#define	OCIE1B		0x08	// Timer Counter 1 Output CompareB Match Interrupt Enable. 
	#define	TOIE1		0x04	// Timer Counter 1 Overflow Interrupt Enable. 
	#define OCIE0		0x02	// Timer Counter 0 Output Compare Interrupt Enable. 
	#define TOIE0		0x01	// Timer Counter 0 Overflow Interrupt Enable. 

#define TIFR	(*(volatile unsigned char *)0x56)
	#define OCF2		0x08	// Output Compare Flag 2 
	#define TOV2		0x04	// Timer Counter 2 Overflow Flag. 
	#define	ICF1		0x02	// Input Capture Flag 1. 
	#define	OCF1A		0x01	// Output Compare Flag 1A. 
	#define	OCF1B		0x08	// Output Compare Flag 1B. 
	#define	TOV1		0x04	// Timer Counter 1 Overflow Flag. 
	#define OCF0		0x02	// Output Compare Flag 0. 
	#define TOV0		0x01	// Timer Counter 0 Overflow Flag. 

#define MCUCR	(*(volatile unsigned char *)0x55)
	#define	SRE		0x80	// External Ram Enable. 
	#define	SRW		0x40	// External Ram Wait State. 
	#define SME		0x20	// Sleep Enable. 
	#define	SM_IDLE	0x00	// Sleep - Idle 
	#define SM_PD	0x10	// Power Down. 
	#define SM_PS	0x18	// Power Save. 

#define MCUSR	(*(volatile unsigned char *)0x54)
	#define PORF	0x01	// Power on reset. 
	#define	EXTRF	0x02	// External Reset. 

#define TCCR0	(*(volatile unsigned char *)0x53)
	#define	CS0_0	0x00	// Timer Counter Stopped. 
	#define CS0_1	0x01	// Prescale = TCK0 
	#define CS0_8	0x02	// Prescale /8 
	#define CS0_32	0x03	// Prescale /32 
	#define CS0_64	0x04	// Prescale /64 
	#define CS0_128	0x05	// Prescale /128 
	#define CS0_256	0x06	// Prescale /256 
	#define CS0_1024	0x07	// Prescale /1024 
#define TCCR2	(*(volatile unsigned char *)0x45)
	#define	PWMX	0x40	// PWM 0,2 Enable. 
	#define COMX_NC	0x00   	// Compare Mode Disconnect output. 
	#define	COMX_T	0x10	// Compare Mode Toggle Output. 
	#define COMX_C	0x20	// Compare Mode Clear Output. 
	#define COMX_S	0x30	// Compare Mode Set Output. 
	#define CTC02	0x08	// Clear Timer 0 or 2 on compare match. 
	#define	CS2_0	0x00	// Timer Counter Stopped. 
	#define CS2_1	0x01	// Prescale = CK 
	#define CS2_8	0x02	// Prescale /8 
	#define CS2_64	0x03	// Prescale /64 
	#define CS2_256	0x04	// Prescale /256 
	#define CS2_1024	0x05	// Prescale /1024 
	#define CS2F	0x06	// External Falling Edge.
	#define CS2R	0x07	// External Rising Edge. 

#define TCNT0	(*(volatile unsigned char *)0x52)
#define OCR0	(*(volatile unsigned char *)0x51)
#define ASSR	(*(volatile unsigned char *)0x50)

#define TCCR1A	(*(volatile unsigned char *)0x4F)
	#define	COM1A_T	0x40	// ToggleOC1A output line. 
	#define COM1A_C	0x80	// Clear OC1A output line. 
	#define	COM1A_S	0xC0	// Set   OC1A output line. 
	#define COM1B_T	0x10	// ToggleOC1B output line. 
	#define COM1B_C	0x20	// Clear OC1B output line. 
	#define	COM1B_S	0x30	// Set   OC1B output line. 
	#define	PWM_8	0x01	// 8 bit pwm mode. 
	#define PWM_9	0x02	// 9 bit pwm mode. 
	#define PWM_10	0x03	// 10 bit pwm mode. 

#define TCCR1B	(*(volatile unsigned char *)0x4E)
	#define ICNC1	0x80	// Input noise canceller. 
	#define	ICES1	0x40	// Select rising edge. 
	#define	CTC1	0x08	// Clear Counter Timer 1 on Compare match. 
	#define CS1_1	0x01	// Prescale /1. 
	#define CS1_8	0x02	// Prescale /8. 
	#define	CS1_64	0x03	// Prescale /64. 
	#define CS1_256	0x04	// Prescale /256. 
	#define CS1_1024	0x05	// Prescale /1024. 
	#define CS1XR	0x06	// External Pin T1, Rising edge. 
	#define CS1XF	0x07	// External Pin T1, Falling edge. 

// Timer Counter 1 (High, Low)	 
#define TCNT1H	(*(volatile unsigned char *)0x4D)
#define TCNT1L	(*(volatile unsigned char *)0x4C)
    #define TCNT1 (*(volatile unsigned int *)0x4C) 

// Timer Counter 1 Output Compare Register (A,B,High,Low) 
#define OCR1AH	(*(volatile unsigned char *)0x4B)
#define OCR1AL	(*(volatile unsigned char *)0x4A)
#define OCR1BH	(*(volatile unsigned char *)0x49)
#define OCR1BL	(*(volatile unsigned char *)0x48)
    #define OCR1A (*(volatile unsigned int *)0x4A) 
    #define OCR1B (*(volatile unsigned int *)0x48) 

// Timer Counter 1 Input Capture Register (High,Low) 
#define ICR1H	(*(volatile unsigned char *)0x47)
#define ICR1L	(*(volatile unsigned char *)0x46)
	#define ICR1 	(*(volatile unsigned int *)0x46) 

// Timer Counter 2 Register. 
#define TCNT2	(*(volatile unsigned char *)0x44)
// Timer Counter 2 Output Compare Register. 
#define OCR2	(*(volatile unsigned char *)0x43)

// Watchdog Timer Control Register. 
#define WDTCR	(*(volatile unsigned char *)0x41)
	#define	WDTOE	0x10	// Turn Off Enable. 
	#define	WDE		0x08	// Enable. 
	#define	WDP16K	0x00	// 16K Cycles. 
	#define WDP32K	0x01	// 32K 
	#define	WDP64K	0x02	// 64K 
	#define	WDP128K	0x03	// 128K 
	#define WDP256K	0x04	// 256K 
	#define WDP512K	0x05	// 512K 
	#define	WDP1024K	0x06	// 1024K 
	#define WDP2048K	0x07	// 2048K 

// EPROM Read Write Address (Low, High) 
#define EEARH	(*(volatile unsigned char *)0x3F)
#define EEARL	(*(volatile unsigned char *)0x3E)
	// EEPROM, 16 bit address 
	#define EEAR	(*(volatile unsigned int *)0x3E)

// EPROM Data Register. 
#define EEDR	(*(volatile unsigned char *)0x3D)
// EPROM Control Register. 
#define EECR	(*(volatile unsigned char *)0x3C)
	#define	EERIE	0x08	// EPROM Ready Interrupt Enable. 
	#define	EEMWE	0x04	// EPROM Master Write Enable. 
	#define	EEWE	0x02	// EPROM Write Enable. 
	#define EERE	0x01	// EPROM Read Enable. 

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

// SPI Data Register 
#define SPDR	(*(volatile unsigned char *)0x2F)	// SPI Data Register. 
// SPI Status Register 
#define SPSR	(*(volatile unsigned char *)0x2E)
	#define	SPIF	0x80	// SPI Interrupt Flag. 
	#define	WCOL	0x40	// Write Collision. 
// SPI Control Register. 
#define SPCR	(*(volatile unsigned char *)0x2D)
	#define	SPIE	0x80	// SPI Interrupt Enable. 
	#define NSPIE	0x7F	// ~SPIE 
	#define SPE		0x40	// SPI Enable. 
	#define NSPE	0xBF	// ~SPE 
	#define	DORD	0x20	// LSbit first if 1. 
	#define	MSTR	0x10	// Master 1 vs Slave 0. 
	#define	CPOL	0x08	// Clock idles high when set, low when 0 
	#define	CPHA	0x04	// Clock Phase 0 = Setup High Latch Low 1 = setup low latch high. 
	#define	SPR4  	0x00	// clock rate / 4 
	#define SPR16	0x01	// clock rate /16 
	#define SPR64	0x02	// clock rate /64 
	#define SPR128	0x03	// clock rate /128 

// UART Data Register. 
#define UDR		(*(volatile unsigned char *)0x2C)
// UART Status Register. 
#define USR		(*(volatile unsigned char *)0x2B)
	#define	RXC		0x80	// Receive Complete.	(Read UDR clears) 
	#define	TXC		0x40	// Transmit Complete.	(Write TXC = 1 clears) 
	#define	UDRE	0x20	// Data Register Empty. 
	#define FE		0x10	// Framing Error. 
	#define	DOR		0x08	// Overrun Error. 
// UART Control Register.	 
#define UCR		(*(volatile unsigned char *)0x2A)
	#define	RXCIE	0x80	// Receive Interrupt Enable. 
	#define NRXCIE	0x7F	// ~RXCIE. 
	#define	TXCIE	0x40	// Transmit Interrupt Enable. 
	#define	NTXCIE	0xBF	// ~TXCIE. 
	#define	UDRIE	0x20	// Data Register Interrupt Enable. 
	#define NUDRIE	0xDF
	#define	RXEN	0x10	// Receive Enable. 
	#define	TXEN	0x08	// Transmit Enable. 
	#define	CHR9	0x04	// 9 Bit Characters. 
	#define	RXB8	0x02	// Receive bit 8. 
	#define	TXB8	0x01	// Transmit bit 8. 
// UART Baud Rate Register. 
#define UBRR	(*(volatile unsigned char *)0x29)

// Analog Comparator Control and Status Register. 
#define ACSR	(*(volatile unsigned char *)0x28)
	#define	ACD		0x80	// Analog Comparator Disable. 
	#define	ACO		0x20	// Analog Comparator Output. 
	#define	ACI		0x10	// Analog Comparator Interrupt Flag. 
	#define	ACIE	0x08	 
	#define	ACIC	0x04	 
	#define	ACIT	0x00	// Interrupt on Toggle. 
	#define	ACIF	0x02	// Interrupt on Falling Edge. 
	#define	ACIR	0x03	// Interrupt on Rising Edge. 
	#define	NACIR	0xFC	// Not ACIR	- Clear.	 

// ADC Multiplexer Select Register. 
#define ADMUX	(*(volatile unsigned char *)0x27)
// ADC Control and Status Register. 
#define ADCSR	(*(volatile unsigned char *)0x26)
  #define ADEN 0x80
  #define ADSC 0x40
  #define ADFR 0x20
  #define ADIF 0x10
  #define ADIE 0x08
  #define ADPS2 0x04
  #define ADPS1 0x02
  #define ADPS0 0x01		
// ADC Data Register (High, Low) 
#define ADCH	(*(volatile unsigned char *)0x25)
#define ADCL	(*(volatile unsigned char *)0x24)
  #define ADC  (*(volatile unsigned int *)0x24) 

#define PORTE	(*(volatile unsigned char *)0x23)
#define DDRE	(*(volatile unsigned char *)0x22)
#define PINE	(*(volatile unsigned char *)0x21)

#define PINF	(*(volatile unsigned char *)0x20)
#endif
