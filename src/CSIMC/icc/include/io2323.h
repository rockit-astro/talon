#ifndef __io2323_h
#define __io2323_h
/* Converted from Atmel provided header file for
 * ImageCraft ICCAVR compiler
 */

/* Input Pins, Port B */
#define PINB	(*(volatile unsigned char *)0x36)

/* Data Direction Register, Port B */
#define DDRB	(*(volatile unsigned char *)0x37)

/* Data Register, Port B */
#define PORTB	(*(volatile unsigned char *)0x38)

/* EEPROM Control Register */
#define EECR	(*(volatile unsigned char *)0x3C)

/* EEPROM Data Register */
#define EEDR	(*(volatile unsigned char *)0x3D)

/* EEPROM Address Register Low */
#define EEARL	(*(volatile unsigned char *)0x3E)
#define EEAR	(*(volatile unsigned char *)0x3E)

/* Watchdog Timer Control Register */
#define WDTCR	(*(volatile unsigned char *)0x41)

/* Timer/Counter 0 */
#define TCNT0	(*(volatile unsigned char *)0x52)

/* Timer/Counter 0 Control Register */
#define TCCR0	(*(volatile unsigned char *)0x53)

/* MCU Status Register */
#define MCUSR	(*(volatile unsigned char *)0x54)

/* MCU general Control Register */
#define MCUCR	(*(volatile unsigned char *)0x55)

/* Timer/Counter Interrupt Flag register */
#define TIFR	(*(volatile unsigned char *)0x58)

/* Timer/Counter Interrupt MaSK register */
#define TIMSK	(*(volatile unsigned char *)0x59)

/* General Interrupt Flag register */
#define GIFR	(*(volatile unsigned char *)0x5A)

/* General Interrupt MaSK register */
#define GIMSK	(*(volatile unsigned char *)0x5B)

/* Stack Pointer */
#define SP	(*(volatile unsigned int *)0x5D)
#define SPL	(*(volatile unsigned char *)0x5D)

/* Status REGister */
#define SREG	(*(volatile unsigned char *)0x5F)

/* General Interrupt MaSK register */ 
#define    INT0    6
#define    INTF0   6

/* General Interrupt Flag Register */                 
#define    TOIE0   1
#define    TOV0    1

/* MCU general Control Register */ 
#define    SE      5
#define    SM      4
#define    ISC01   1
#define    ISC00   0

/* Timer/Counter 0 Control Register */
#define    CS02    2
#define    CS01    1
#define    CS00    0

/* Watchdog Timer Control Register */
#define    WDTOE   4
#define    WDE     3
#define    WDP2    2
#define    WDP1    1
#define    WDP0    0

/* EEPROM Control Register */
#define    EEMWE   2
#define    EEWE    1
#define    EERE    0

/* Data Register, Port B */
#define    PB4     4
#define    PB3     3
#define    PB2     2
#define    PB1     1
#define    PB0     0

/* Data Direction Register, Port B */
#define    DDB4    4
#define    DDB3    3
#define    DDB2    2
#define    DDB1    1
#define    DDB0    0

/* Input Pins, Port B */
#define    PINB4   4
#define    PINB3   3
#define    PINB2   2
#define    PINB1   1
#define    PINB0   0

/* Pointer definition   */  
#define    XL     R26
#define    XH     R27
#define    YL     R28
#define    YH     R29
#define    ZL     R30
#define    ZH     R31

/* Contants */ 
#define    RAMEND    0xDF    // Last On-Chip SRAM Location
#define    XRAMEND   0xDF
#define    E2END     0x7F
#define    FLASHEND  0x07FF

#endif
