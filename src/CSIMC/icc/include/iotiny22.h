
#ifndef __IOTINY22_H
#define __IOTINY22_H
/* COPIED FROM IOTINY11.h */
/* deleted ADSR */

/* Status REGister */
unsigned int ioloc SREG = 0x3F;

/* General Interrupt MaSK register */
unsigned int ioloc GIMSK = 0x3B;

/* General Interrupt Flag Register */
unsigned int ioloc GIFR = 0x3A;

/* Timer/Counter Interrupt MaSK register */
unsigned int ioloc TIMSK = 0x39;

/* Timer/Counter Interrupt Flag register */
unsigned int ioloc TIFR = 0x38;

/* MCU general Control Register */
unsigned int ioloc MCUCR = 0x35;

/* MCU Status Register */
unsigned int ioloc MCUSR = 0x34;

/* Timer/Counter 0 Control Register */
unsigned int ioloc TCCR0 = 0x33;

/* Timer/Counter 0 */
unsigned int ioloc TCNT0 = 0x32;

/* Watchdog Timer Control Register */
unsigned int ioloc WDTCR = 0x21;

/* Data Register, Port B */
unsigned int ioloc PORTB = 0x18;

/* Data Direction Register, Port B */
unsigned int ioloc DDRB = 0x17;

/* Input Pins, Port B */
unsigned int ioloc PINB = 0x16;

/* MCU general Status Register */    
#define    EXTRF       1
#define    PORF        0

/* General Interrupt MaSK register */
#define    INT0        6

/* General Interrupt Flag Register */
#define    INTF0       6

/* Timer/Counter Interrupt MaSK register */
#define    TOIE0       1

/* Timer/Counter Interrupt Flag register */
#define    TOV0         1 

/* MCU general Control Register */ 
#define    SE           5
#define    SM           4
#define    ISC01        1
#define    ISC00        0

/* Timer/Counter 0 Control Register */
#define    CS02         2
#define    CS01         1
#define    CS00         0

/* Watchdog Timer Control Register */                         
#define    WDTOE        4
#define    WDE          3
#define    WDP2         2
#define    WDP1         1
#define    WDP0         0    

/* Data Register, Port B */  
#define    PB4      4
#define    PB3      3
#define    PB2      2
#define    PB1      1
#define    PB0      0

/* Data Direction Register, Port B */
#define    DDB4     4
#define    DDB3     3
#define    DDB2     2
#define    DDB1     1
#define    DDB0     0

/* Input Pins, Port B */
#define    PINB4    4
#define    PINB3    3
#define    PINB2    2
#define    PINB1    1
#define    PINB0    0

/* Contants */ 
#define    FLASHEND  0x3FF

/* NEW IOTINY22.H STUFF */

/* Stack pointer */
unsigned int ioloc SPL = 0x3D;

unsigned int ioloc EEAR = 0x1E;
unsigned int ioloc EEDR = 0x1D;
unsigned int ioloc EECR = 0x1C;

/* EECR */
#define EEMWE		2
#define EEWE		1
#define EERE		0

#endif
