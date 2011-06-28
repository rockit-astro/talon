#ifndef __io4433_h
#define __io4433_h
/* Converted from Atmel provided header file for
 * ImageCraft ICCAVR compiler
 */

/* UART Baud Rate Register high */
#define UBRRH	(*(volatile unsigned char *)0x23)

/* ADC Data register */
#define ADC	(*(volatile unsigned int *)0x24)
#define ADCL	(*(volatile unsigned char *)0x24)
#define ADCH	(*(volatile unsigned char *)0x25)

/* ADC Control and Status Register */
#define ADCSR	(*(volatile unsigned char *)0x26)

/* ADC MUX */
#define ADMUX	(*(volatile unsigned char *)0x27)

/* Analog Comparator Control and Status Register */
#define ACSR	(*(volatile unsigned char *)0x28)

/* UART Baud Rate Register */
#define UBRR	(*(volatile unsigned char *)0x29)
#define UBRRL	(*(volatile unsigned char *)0x29)

/* UART Control Register */
#define UCR	(*(volatile unsigned char *)0x2A)

/* UART Status Register */
#define USR	(*(volatile unsigned char *)0x2B)

/* UART I/O Data Register */
#define UDR	(*(volatile unsigned char *)0x2C)

/* SPI Control Register */
#define SPCR	(*(volatile unsigned char *)0x2D)

/* SPI Status Register */
#define SPSR	(*(volatile unsigned char *)0x2E)

/* SPI I/O Data Register */
#define SPDR	(*(volatile unsigned char *)0x2F)

/* Input Pins, Port D */
#define PIND	(*(volatile unsigned char *)0x30)

/* Data Direction Register, Port D */
#define DDRD	(*(volatile unsigned char *)0x31)

/* Data Register, Port D */
#define PORTD	(*(volatile unsigned char *)0x32)

/* Input Pins, Port C */
#define PINC	(*(volatile unsigned char *)0x33)

/* Data Direction Register, Port C */
#define DDRC	(*(volatile unsigned char *)0x34)

/* Data Register, Port C */
#define PORTC	(*(volatile unsigned char *)0x35)

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
#define EEAR	(*(volatile unsigned int *)0x3E)
#define EEARL	(*(volatile unsigned char *)0x3E)
#define EEARH	(*(volatile unsigned char *)0x3F)

/* Watchdog Timer Control Register */
#define WDTCR	(*(volatile unsigned char *)0x41)

/* T/C 1 Input Capture Register */
#define ICR1	(*(volatile unsigned int *)0x46)
#define ICR1L	(*(volatile unsigned char *)0x46)
#define ICR1H	(*(volatile unsigned char *)0x47)

/* Timer/Counter1 Output Compare Register A */
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

/* MCU general Status Register */
#define MCUSR	(*(volatile unsigned char *)0x54)

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
#define SPH	(*(volatile unsigned char *)0x5E)

/* Status REGister */
#define SREG	(*(volatile unsigned char *)0x5F)

/* MCU general Status Register */    
#define    WDRF        3
#define    BORF        2
#define    EXTRF       1
#define    PORF        0

/* General Interrupt MaSK register */
#define    INT1        7
#define    INT0        6

/* General Interrupt Flag Register */
#define    INTF1       7
#define    INTF0       6                   

/* Timer/Counter Interrupt MaSK register */
#define    TOIE1       7
#define    OCIE1       6 
#define    TICIE1      3
#define    TOIE0       1

/* Timer/Counter Interrupt Flag register */
#define    TOV1         7
#define    OCF1         6
#define    ICF1         3
#define    TOV0         1

/* MCU general Control Register */ 
#define    SE           5
#define    SM           4
#define    ISC11        3
#define    ISC10        2
#define    ISC01        1
#define    ISC00        0

/* Timer/Counter 0 Control Register */
#define    CS02         2
#define    CS01         1
#define    CS00         0

/* Timer/Counter 1 Control Register */
#define    COM11        7
#define    COM10        6
#define    PWM11        1
#define    PWM10        0

/* Timer/Counter 1 Control and Status Register */
#define    ICNC1        7
#define    ICES1        6
#define    CTC1         3
#define    CS12         2
#define    CS11         1
#define    CS10         0

/* Watchdog Timer Control Register */                         
#define    WDTOE        4
#define    WDE          3
#define    WDP2         2
#define    WDP1         1
#define    WDP0         0    

/* EEPROM Control Register */
#define    EERIE        3
#define    EEMWE        2
#define    EEWE         1
#define    EERE         0

/* SPI Control Register */
#define    SPIE       7  
#define    SPE       6
#define    DORD       5
#define    MSTR       4
#define    CPOL       3
#define    CPHA       2
#define    SPR1       1
#define    SPR0       0

/* SPI Status Register */
#define    SPIF      7
#define    WCOL       6

/* UART Status Register */
#define    RXC        7
#define    TXC        6
#define    UDRE       5
#define    FE         4
#define    OVR        3    /*This definition differs from the databook    */

                        /*definition to avoid problems with the OR instruction */
#define    MPCM       0

/* UART Control Register */
#define    RXCIE      7
#define    TXCIE      6
#define    UDRIE      5
#define    RXEN       4
#define    TXEN       3
#define    CHR9       2
#define    RXB8       1
#define    TXB8       0

/* Analog Comparator Control and Status Register */
#define    ACD        7
#define    AINBG        6
#define    ACO        5
#define    ACI        4
#define    ACIE       3
#define    ACIC       2
#define    ACIS1      1
#define    ACIS0      0

/* ADC MUX */
#define    ACDBG        6
#define    MUX2   2
#define    MUX1       1
#define    MUX0       0

/* ADC Control and Status Register */
#define    ADEN       7
#define    ADSC       6
#define    ADFR       5
#define    ADIF       4
#define    ADIE       3
#define    ADPS2      2
#define    ADPS1      1
#define    ADPS0      0  

/* Data Register, Port B */  
#define    PB5      5
#define    PB4      4
#define    PB3      3
#define    PB2      2
#define    PB1      1
#define    PB0      0

/* Data Direction Register, Port B */
#define    DDB5     5
#define    DDB4     4
#define    DDB3     3
#define    DDB2     2
#define    DDB1     1
#define    DDB0     0

/* Input Pins, Port B */
#define    PINB5    5
#define    PINB4    4
#define    PINB3    3
#define    PINB2    2
#define    PINB1    1
#define    PINB0    0

/* Data Register, Port C */
#define    PC5      5
#define    PC4      4
#define    PC3      3
#define    PC2      2
#define    PC1      1
#define    PC0      0

/* Data Direction Register, Port C */
#define    DDC5     5
#define    DDC4     4
#define    DDC3     3
#define    DDC2     2
#define    DDC1     1
#define    DDC0     0

/* Input Pins, Port C */
#define    PINC5    5
#define    PINC4    4
#define    PINC3    3
#define    PINC2    2
#define    PINC1    1
#define    PINC0    0

/* Data Register, Port D */
#define    PD7      7
#define    PD6      6
#define    PD5      5
#define    PD4      4
#define    PD3      3
#define    PD2      2
#define    PD1      1
#define    PD0      0

/* Data Direction Register, Port D */
#define    DDD7     7
#define    DDD6     6
#define    DDD5     5
#define    DDD4     4
#define    DDD3     3
#define    DDD2     2
#define    DDD1     1
#define    DDD0     0

/* Input Pins, Port D */
#define    PIND7     7
#define    PIND6     6
#define    PIND5     5
#define    PIND4     4
#define    PIND3     3
#define    PIND2     2
#define    PIND1     1
#define    PIND0     0

/* Pointer definition   */
#define    XL     R26
#define    XH     R27
#define    YL     R28
#define    YH     R29
#define    ZL     R30
#define    ZH     R31

/* Contants */
#define    RAMEND   0xDF    /*Last On-Chip SRAM location*/

#define    RAMEND   0xDF    /*Last On-Chip SRAM location*/
#define    XRAMEND  0xDF
#define    E2END    0x7F
#define    FLASHEND 0x7FF

#endif
