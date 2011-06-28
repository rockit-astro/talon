#ifndef __IOTINY15_H
#define __IOTINY15_H

/* COPIED FROM IOTINY12.H */
/* COPIED FROM IOTINY11.H */
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

/* Analog Comparator Control and Status Register */
unsigned int ioloc ACSR = 0x08;

/* MCU general Status Register */    
#define    EXTRF       1
#define    PORF        0

/* General Interrupt MaSK register */
#define    INT0        6
#define    PCIE        5

/* General Interrupt Flag Register */
#define    INTF0       6
#define    PCIF        5                   

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

/* Analog Comparator Control and Status Register */
#define    ACD      	7
#define    ACO      	5
#define    ACI      	4
#define    ACIE     	3
#define    ACIS1    	1
#define    ACIS0    	0

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
#define    FLASHEND  0x1FF

/* NEW IOTINY12 STUFF */

unsigned int ioloc OSCCAK = 0x31;
unsigned int ioloc EEAR = 0x1E;
unsigned int ioloc EEDR = 0x1D;
unsigned int ioloc EECR = 0x1C;

/* MCUSR MCU Status Register */
#define WDRF		3
#define BORF		2

/* MCUCR */
#define PUD			6

/* EECR */
#define EERIE		3
#define EEMWE		2
#define EEWE		1
#define EERE		0

/* NEW IOTINY15.H STUFF */

unsigned int ioloc TCCR1 = 0x30;
unsigned int ioloc TCNT1 = 0x2F;
unsigned int ioloc OCR10 = 0x2E;
unsigned int ioloc OCR11 = 0x2D;
unsigned int ioloc ADMUX = 0x07;
unsigned int ioloc ADCSR = 0x06;
unsigned int ioloc ADCH = 0x05;
unsigned int ioloc ADCL = 0x04;

/* TCCR1 */
#define CTC1		7
#define PWM1		6
#define COM11		5
#define COM10		4
#define CS13		3
#define CS12		2
#define CS11		1
#define CS10		0

/* ADMUX */
#define REFS1		7
#define REFS0		6
#define MUX2		2
#define MUX1		1
#define MUX0		0

/* ADCSR */
#define ADEN		7
#define ADSC		6
#define ADFR		5
#define ADIF		4
#define ADIE		3
#define ADPS2		2
#define ADPS1		1
#define ADPS0		0
#endif
