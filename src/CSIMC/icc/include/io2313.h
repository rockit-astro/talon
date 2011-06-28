#ifndef __io2313_h
#define __io2313_h
/* Converted from Atmel provided header file for
 * ImageCraft ICCAVR compiler
 */

/* Analog Comparator Control and Status Register */
#define ACSR	(*(volatile unsigned char *)0x28)

/* UART Baud Rate Register */
#define UBRR	(*(volatile unsigned char *)0x29)

/* UART Control Register */
#define UCR	(*(volatile unsigned char *)0x2A)

/* UART Status Register */
#define USR	(*(volatile unsigned char *)0x2B)

/* UART I/O Data Register */
#define UDR	(*(volatile unsigned char *)0x2C)

/* Input Pins, Port D */
#define PIND	(*(volatile unsigned char *)0x30)

/* Data Direction Register, Port D */
#define DDRD	(*(volatile unsigned char *)0x31)

/* Data Register, Port D */
#define PORTD	(*(volatile unsigned char *)0x32)

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

/* EEPROM Address Register */
#define EEAR	(*(volatile unsigned char *)0x3E)
#define EEARL	(*(volatile unsigned char *)0x3E)

/* Watchdog Timer Control Register */
#define WDTCR	(*(volatile unsigned char *)0x41)

/* T/C 1 Input Capture Register */
#define ICR1	(*(volatile unsigned int *)0x44)
#define ICR1L	(*(volatile unsigned char *)0x44)
#define ICR1H	(*(volatile unsigned char *)0x45)

/* Output Compare Register 1 */
#define OCR1	(*(volatile unsigned int *)0x4A)
#define OCR1L	(*(volatile unsigned char *)0x4A)
#define OCR1H	(*(volatile unsigned char *)0x4B)

/* Timer/Counter 1 */
#define TCNT1	(*(volatile unsigned int *)0x4C)
#define TCNT1L	(*(volatile unsigned char *)0x4C)
#define TCNT1H	(*(volatile unsigned char *)0x4D)

/* Timer/Counter 1 Control and Status Register */
#define TCCR1B	(*(volatile unsigned char *)0x4E)

/* Timer/Counter 1 Control Register */
#define TCCR1A	(*(volatile unsigned char *)0x4F)

/* Timer/Counter 0 */
#define TCNT0	(*(volatile unsigned char *)0x52)

/* Timer/Counter 0 Control Register */
#define TCCR0	(*(volatile unsigned char *)0x53)

/* MCU general Control Register */
#define MCUCR	(*(volatile unsigned char *)0x55)

/* Timer/Counter Interrupt Flag register */
#define TIFR	(*(volatile unsigned char *)0x58)

/* Timer/Counter Interrupt MaSK register */
#define TIMSK	(*(volatile unsigned char *)0x59)

/* General Interrupt Flag Register */
#define GIFR	(*(volatile unsigned char *)0x5A)

/* General Interrupt MaSK register */
#define GIMSK	(*(volatile unsigned char *)0x5B)

/* Stack Pointer */
#define SP	(*(volatile unsigned int *)0x5D)
#define SPL	(*(volatile unsigned char *)0x5D)

/* Status REGister */
#define SREG	(*(volatile unsigned char *)0x5F)

/* Stack Pointer */ 
#define    SP7     7
#define    SP6     6
#define    SP5     5
#define    SP4     4
#define    SP3     3
#define    SP2     2
#define    SP1     1
#define    SP0     0

/* General Interrupt MaSK register */
#define    INT1    7
#define    INT0    6

/* General Interrupt Flag Register */
#define    INTF1   7
#define    INTF0   6

/* Timer/Counter Interrupt MaSK register */                 
#define    TOIE1   7
#define    OCIE1A  6
#define    TICIE   3
#define    TOIE0   1

/* Timer/Counter Interrupt Flag register */                   
#define    TOV1    7
#define    OCF1A   6
#define    ICF1    3
#define    TOV0    1

/* MCU general Control Register */ 
#define    SE      5
#define    SM      4
#define    ISC11   3
#define    ISC10   2
#define    ISC01   1
#define    ISC00   0

/* Timer/Counter 0 Control Register */
#define    CS02    2
#define    CS01    1
#define    CS00    0

/* Timer/Counter 1 Control Register */
#define    COM1A1  7
#define    COM1A0  6
#define    PWM11   1
#define    PWM10   0

/* Timer/Counter 1 Control and Status Register */
#define    ICNC1   7
#define    ICES1   6
#define    CTC1    3
#define    CS12    2
#define    CS11    1
#define    CS10    0

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
#define    PB7     7
#define    PB6     6
#define    PB5     5
#define    PB4     4
#define    PB3     3
#define    PB2     2
#define    PB1     1
#define    PB0     0

/* Data Direction Register, Port B */
#define    DDB7    7
#define    DDB6    6
#define    DDB5    5
#define    DDB4    4
#define    DDB3    3
#define    DDB2    2
#define    DDB1    1
#define    DDB0    0

/* Input Pins, Port B */
#define    PINB7   7
#define    PINB6   6
#define    PINB5   5
#define    PINB4   4
#define    PINB3   3
#define    PINB2   2
#define    PINB1   1
#define    PINB0   0

/* Data Register, Port D */
#define    PD6     6
#define    PD5     5
#define    PD4     4
#define    PD3     3
#define    PD2     2
#define    PD1     1
#define    PD0     0

/* Data Direction Register, Port D */
#define    DDD6    6
#define    DDD5    5
#define    DDD4    4
#define    DDD3    3
#define    DDD2    2
#define    DDD1    1
#define    DDD0    0

/* Input Pins, Port D */
#define    PIND6   6
#define    PIND5   5
#define    PIND4   4
#define    PIND3   3
#define    PIND2   2
#define    PIND1   1
#define    PIND0   0

/* UART Status Register */
#define    RXC     7
#define    TXC     6
#define    UDRE    5
#define    FE      4
#define    OVR     3    /*This definition differs from the databook    */

/* UART Control Register */
#define    RXCIE   7
#define    TXCIE   6
#define    UDRIE   5
#define    RXEN    4
#define    TXEN    3
#define    CHR9    2
#define    RXB8    1
#define    TXB8    0

/* Analog Comparator Control and Status Register */ 
#define    ACD     7
#define    ACO     5
#define    ACI     4
#define    ACIE    3
#define    ACIC    2
#define    ACIS1   1
#define    ACIS0   0

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
