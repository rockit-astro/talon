#ifndef __iom603_h
#define __iom603_h
/* Converted from Atmel provided header file for
 * ImageCraft ICCAVR compiler
 */

/* Input Pins, Port F */
#define PINF	(*(volatile unsigned char *)0x20)

/* Input Pins, Port E */
#define PINE	(*(volatile unsigned char *)0x21)

/* Data Direction Register, Port E */
#define DDRE	(*(volatile unsigned char *)0x22)

/* Data Register, Port E */
#define PORTE	(*(volatile unsigned char *)0x23)

/* ADC Low Byte */
#define ADC	(*(volatile unsigned int *)0x24)
#define ADCL	(*(volatile unsigned char *)0x24)
#define ADCH	(*(volatile unsigned char *)0x25)

/* ADC Control and status register */
#define ADCSR	(*(volatile unsigned char *)0x26)

/* ADC Multiplexer select */
#define ADMUX	(*(volatile unsigned char *)0x27)

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

/* Data Register, Port C */
#define PORTC	(*(volatile unsigned char *)0x35)

/* Input Pins, Port B */
#define PINB	(*(volatile unsigned char *)0x36)

/* Data Direction Register, Port B */
#define DDRB	(*(volatile unsigned char *)0x37)

/* Data Register, Port B */
#define PORTB	(*(volatile unsigned char *)0x38)

/* Input Pins, Port A */
#define PINA	(*(volatile unsigned char *)0x39)

/* Data Direction Register, Port A */
#define DDRA	(*(volatile unsigned char *)0x3A)

/* Data Register, Port A */
#define PORTA	(*(volatile unsigned char *)0x3B)

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

/* Timer2 Output Compare Register */
#define OCR2	(*(volatile unsigned char *)0x43)

/* Timer/Counter 2 */
#define TCNT2	(*(volatile unsigned char *)0x44)

/* Timer/Counter 2 Control register */ 
#define TCCR2	(*(volatile unsigned char *)0x45)

/* T/C 1 Input Capture Register */
#define ICR1	(*(volatile unsigned char *)0x46)
#define ICR1L	(*(volatile unsigned char *)0x46)
#define ICR1H	(*(volatile unsigned char *)0x47)

/* Timer/Counter1 Output Compare Register B */ 
#define OCR1B	(*(volatile unsigned int *)0x48)
#define OCR1BL	(*(volatile unsigned char *)0x48)
#define OCR1BH	(*(volatile unsigned char *)0x49)

/* Timer/Counter1 Output Compare Register A */
#define OCR1A	(*(volatile unsigned int *)0x4A)
#define OCR1AL	(*(volatile unsigned char *)0x4A)
#define OCR1AH	(*(volatile unsigned char *)0x4B)

/* Timer/Counter 1 */
#define TCNT1	(*(volatile unsigned int *)0x4C)
#define TCNT1L	(*(volatile unsigned char *)0x4C)
#define TCNT1H	(*(volatile unsigned char *)0x4D)

/* Timer/Counter 1 Control and Status Register */
#define TCCR1B	(*(volatile unsigned char *)0x4E)

/* Timer/Counter 1 Control Register */
#define TCCR1A	(*(volatile unsigned char *)0x4F)

/* Timer/Counter 0 Asynchronous Control & Status Register */
#define ASSR	(*(volatile unsigned char *)0x50)

/* Output Compare Register 0 */
#define OCR0	(*(volatile unsigned char *)0x51)

/* Timer/Counter 0 */
#define TCNT0	(*(volatile unsigned char *)0x52)

/* Timer/Counter 0 Control Register */
#define TCCR0	(*(volatile unsigned char *)0x53)

/* MCU Status Register */
#define MCUSR	(*(volatile unsigned char *)0x54)

/* MCU general Control Register */
#define MCUCR	(*(volatile unsigned char *)0x55)

/* Timer/Counter Interrupt Flag Register */
#define TIFR	(*(volatile unsigned char *)0x56)

/* Timer/Counter Interrupt MaSK register */
#define TIMSK	(*(volatile unsigned char *)0x57)

/* Èxternal Interrupt Flag Register */
#define EIFR	(*(volatile unsigned char *)0x58)

/* External Interrupt MaSK register */
#define EIMSK	(*(volatile unsigned char *)0x59)

/* External Interrupt Control Register */
#define EICR	(*(volatile unsigned char *)0x5A)

/*RAM Page Z select register */
#define RAMPZ	(*(volatile unsigned char *)0x5B)

/* XDIV Divide control register */
#define XDIV	(*(volatile unsigned char *)0x5C)

/* Stack Pointer */
#define SP	(*(volatile unsigned int *)0x5D)
#define SPL	(*(volatile unsigned char *)0x5D)
#define SPH	(*(volatile unsigned char *)0x5E)

/* Status REGister */
#define SREG	(*(volatile unsigned char *)0x5F)

/* XDIV Divide control register*/
#define    XDIVEN       7
#define    XDIV6        6
#define    XDIV5        5
#define    XDIV4        4
#define    XDIV3        3
#define    XDIV2        2
#define    XDIV1        1
#define    XDIV0        0

/* RAM Page Z select register */
#define     RAMPZ0      0

/* External Interrupt Control Register */ 
#define    ISC71        7
#define    ISC70        6
#define    ISC61        5
#define    ISC60        4
#define    ISC51        3
#define    ISC50        2
#define    ISC41        1
#define    ISC40        0

/* External Interrupt MaSK register */
#define    INT7         7
#define    INT6         6   
#define    INT5         5
#define    INT4         4
#define    INT3         3
#define    INT2         2
#define    INT1         1
#define    INT0         0

/* Èxternal Interrupt Flag Register */
#define    INTF7        7
#define    INTF6        6
#define    INTF5        5
#define    INTF4        4

/* Timer/Counter Interrupt MaSK register */ 
#define    OCIE2        7
#define    TOIE2        6
#define    TICIE1       5
#define    OCIE1A       4
#define    OCIE1B       3
#define    TOIE1        2
#define    OCIE0        1
#define    TOIE0        0

/* Timer/Counter Interrupt Flag Register */
#define    OCF2         7
#define    TOV2         6
#define    ICF1         5
#define    OCF1A        4
#define    OCF1B        3
#define    TOV1         2
#define    OCF0         1
#define    TOV0         0      

/* MCU general Control Register */
#define    SRE          7
#define    SRW          6
#define    SE           5
#define    SM1          4
#define    SM0          3

/* MCU Status Register */ 
#define    EXTRF        1
#define    PORF         0

/* Timer/Counter 0 Control Register */
#define    PWM0         6
#define    COM01        5
#define    COM00        4
#define    CTC0         3
#define    CS02         2
#define    CS01         1
#define    CS00         0

/* Timer/Counter 0 Asynchronous Control & Status Register */ 
#define    AS0          3
#define    TCN0UB       2
#define    OCR0UB       1
#define    TCR0UB       0 

/* Timer/Counter 1 Control Register */ 
#define    COM1A1       7
#define    COM1A0       6
#define    COM1B1       5
#define    COM1B0       4
#define    PWM11        1
#define    PWM10        0

/* Timer/Counter 1 Control and Status Register */
#define    ICNC1        7
#define    ICES1        6
#define    CTC1         3
#define    CS12         2
#define    CS11         1
#define    CS10         0

/* Timer/Counter 2 Control register */ 
#define    PWM2         6
#define    COM21        5
#define    COM20        4
#define    CTC2         3
#define    CS22         2
#define    CS21         1
#define    CS20         0

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

/* Data Register, Port A */
#define    PA7          7
#define    PA6          6
#define    PA5          5
#define    PA4          4
#define    PA3          3
#define    PA2          2
#define    PA1          1
#define    PA0          0

/* Data Direction Register, Port A */ 
#define    DDA7         7
#define    DDA6         6
#define    DDA5         5
#define    DDA4         4
#define    DDA3         3
#define    DDA2         2
#define    DDA1         1
#define    DDA0         0

/* Input Pins, Port A */ 
#define    PINA7        7
#define    PINA6        6
#define    PINA5        5
#define    PINA4        4
#define    PINA3        3
#define    PINA2        2
#define    PINA1        1
#define    PINA0        0

/* Data Register, Port B */ 
#define    PB7          7
#define    PB6          6
#define    PB5          5
#define    PB4          4
#define    PB3          3
#define    PB2          2
#define    PB1          1
#define    PB0          0

/* Data Direction Register, Port B */
#define    DDB7         7
#define    DDB6         6
#define    DDB5         5
#define    DDB4         4
#define    DDB3         3
#define    DDB2         2
#define    DDB1         1
#define    DDB0         0

/* Input Pins, Port B */
#define    PINB7        7
#define    PINB6        6
#define    PINB5        5
#define    PINB4        4
#define    PINB3        3
#define    PINB2        2
#define    PINB1        1
#define    PINB0        0

/* Data Register, Port C */
#define    PC7          7
#define    PC6          6
#define    PC5          5
#define    PC4          4
#define    PC3          3
#define    PC2          2
#define    PC1          1
#define    PC0          0

/* Data Register, Port D */
#define    PD7          7
#define    PD6          6
#define    PD5          5
#define    PD4          4
#define    PD3          3
#define    PD2          2
#define    PD1          1
#define    PD0          0

/* Data Direction Register, Port D */
#define    DDD7         7
#define    DDD6         6
#define    DDD5         5
#define    DDD4         4
#define    DDD3         3
#define    DDD2         2
#define    DDD1         1
#define    DDD0         0

/* Input Pins, Port D */
#define    PIND7        7
#define    PIND6        6
#define    PIND5        5
#define    PIND4        4
#define    PIND3        3
#define    PIND2        2
#define    PIND1        1
#define    PIND0        0

/* Data Register, Port E */
#define    PE7          7
#define    PE6          6
#define    PE5          5
#define    PE4          4
#define    PE3          3
#define    PE2          2
#define    PE1          1
#define    PE0          0

/* Data Direction Register, Port E */
#define    DDE7         7
#define    DDE6         6
#define    DDE5         5
#define    DDE4         4
#define    DDE3         3
#define    DDE2         2
#define    DDE1         1
#define    DDE0         0

/* Input Pins, Port E */
#define    PINE7        7
#define    PINE6        6
#define    PINE5        5
#define    PINE4        4
#define    PINE3        3
#define    PINE2        2
#define    PINE1        1
#define    PINE0        0

/* Input Pins, Port F */
#define    PINF7        7
#define    PINF6        6
#define    PINF5        5
#define    PINF4        4
#define    PINF3        3
#define    PINF2        2
#define    PINF1        1
#define    PINF0        0

/* SPI Status Register */ 
#define    SPIF         7
#define    WCOL         6

/* SPI Control Register */
#define    SPIE         7
#define    SPE          6
#define    DORD         5
#define    MSTR         4
#define    CPOL         3
#define    CPHA         2
#define    SPR1         1
#define    SPR0         0

/* UART Status Register */
#define    RXC          7
#define    TXC          6
#define    UDRE         5
#define    FE           4
#define    OVR          3    /*This definition differs from the databook    */

/* UART Control Register */
#define    RXCIE        7
#define    TXCIE        6
#define    UDRIE        5
#define    RXEN         4
#define    TXEN         3
#define    CHR9         2
#define    RXB8         1
#define    TXB8         0

/* Analog Comparator Control and Status Register */
#define    ACD          7
#define    ACO          5
#define    ACI          4
#define    ACIE         3
#define    ACIC         2
#define    ACIS1        1
#define    ACIS0        0

/* ADC Control and status register */
#define    ADEN         7
#define    ADSC         6
#define    ADFR         5
#define    ADIF         4
#define    ADIE         3
#define    ADPS2        2
#define    ADPS1        1
#define    ADPS0        0

/* ADC Multiplexer select */
#define    MUX2         2
#define    MUX1         1
#define    MUX0         0

/* Pointer definition */
#define    XL       r26
#define    XH       r27
#define    YL       r28
#define    YH       r29
#define    ZL       r30
#define    ZH       r31

/* Constants */
#define    RAMEND   0x0FFF    // Last On-Chip SRAM Location
#define    XRAMEND  0xFFFF
#define    E2END    0x07FF
#define    FLASHEND 0xFFFF

#endif
