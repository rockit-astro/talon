#ifndef __iofpslic_h
#define __iofpslic_h

/* Converted from Atmel provided header file for
 * ImageCraft ICCAVR compiler
 */

/* Staus Register  */
#define SREG	(*(volatile unsigned char *)0x5f)

/* Stack Pointer High */
#define SPH	(*(volatile unsigned char *)0x5e)

/* Stack Pointer Low */
#define SPL	(*(volatile unsigned char *)0x5d)

/*  External Interrupt Mask/Flag Register */
#define EIMF (*(volatile unsigned char *)0x5b)

/* Software Control Register */
#define SFTCR	(*(volatile unsigned char *)0x5a)

/* Timer/Counter Interrupt mask register */
#define TIMSK (*(volatile unsigned char *)0x59)

/*   */
#define TIFR (*(volatile unsigned char *)0x58)

/* */
#define I2CR(*(volatile unsigned char *)0x56)

/*MCU control/status register */
#define MCUR	(*(volatile unsigned char *)0x55)

/* */
#define TCCR0 (*(volatile unsigned char *)0x53)

/* */
#define TCNT0	(*(volatile unsigned char *)0x52)

/* */
#define OCR0	(*(volatile unsigned char *)0x51)

/* */
#define SFIOR	(*(volatile unsigned char *)0x50)

/* */
#define TCCR1A	(*(volatile unsigned char *)0x4f)

/* */
#define TCCR1B	(*(volatile unsigned char *)0x4e)

/* */
#define TCNT1H	(*(volatile unsigned char *)0x4d)

/* */
#define TCNT1L	(*(volatile unsigned char *)0x4c)

/* */
#define OCR1AH	(*(volatile unsigned char *)0x4B)

/* */
#define OCR1AL	(*(volatile unsigned char *)0x4A)

/* */
#define OCR1BH	(*(volatile unsigned char *)0x49)

/* */
#define OCR1BL	(*(volatile unsigned char *)0x48)

/* */
#define TCCR2	(*(volatile unsigned char *)0x47)

/* */
#define ASSR	(*(volatile unsigned char *)0x46)

/* */
#define ICR1H (*(volatile unsigned char *)0x45)

/* */
#define ICR1L	(*(volatile unsigned char *)0x44)

/* */
#define TCNT2	(*(volatile unsigned char *)0x43)

/* */
#define OCR2	(*(volatile unsigned char *)0x42)

/* */
#define WDTCR	(*(volatile unsigned char *)0x41)

/* */
#define UBRRHI	(*(volatile unsigned char *)0x40)

/* */
#define I2DR (*(volatile unsigned char *)0x3F)

/* */
#define I2AR (*(volatile unsigned char *)0x3E)

/* */
#define I2SR (*(volatile unsigned char *)0x3D)

/* */
#define I2BR (*(volatile unsigned char *)0x3C)

/* */
#define FPGAD (*(volatile unsigned char *)0x3B)

/* */
#define FPGAZ (*(volatile unsigned char *)0x3A)

/* */
#define FPGAY (*(volatile unsigned char *)0x39)

/* */
#define FPGAX (*(volatile unsigned char *)0x38)

/* */
#define FISUD (*(volatile unsigned char *)0x37)

/* */
#define FISUC (*(volatile unsigned char *)0x36)

/* */
#define FISUB (*(volatile unsigned char *)0x35)

/* */
#define FISUA (*(volatile unsigned char *)0x34)

/* */
#define FISCR (*(volatile unsigned char *)0x33)

/* */
#define PORTD (*(volatile unsigned char *)0x32)

/* */
#define DDRD (*(volatile unsigned char *)0x31)

/* */
#define PIND (*(volatile unsigned char *)0x31)

/* */
#define UDR0 (*(volatile unsigned char *)0x2C)

/* */
#define USCR0A (*(volatile unsigned char *)0x2B)

/* */
#define USCR0B (*(volatile unsigned char *)0x2A)

/* */
#define UBRR0 (*(volatile unsigned char *)0x29)

/* */
#define PORTE (*(volatile unsigned char *)0x27)

/* */
#define DDRE (*(volatile unsigned char *)0x26)

/* */
#define PINE (*(volatile unsigned char *)0x25)

/* */
#define UDR1 (*(volatile unsigned char *)0x23)

/* */
#define USCR1A (*(volatile unsigned char *)0x22)

/* */
#define USCR1B (*(volatile unsigned char *)0x21)

/* */
#define UBRR1 (*(volatile unsigned char *)0x20)


/* Interrupt  vector numbers for FPSLIC device */

#define FPGA_INT0_VEC 	            2
#define EXT_INT0_VEC                  3
#define FPGA_INT1_VEC              4
#define EXT_INT1_VEC                  5
#define FPGA_INT2_VEC               6
#define EXT_INT2_VEC                  7
#define FPGA_INT3_VEC               8
#define EXT_INT3_VEC                  9
#define TIM2_COMP_VEC             10
#define TIM2_OVF_VEC                 11
#define TIM1_CAPT_VEC              12
#define TIM1_COMPA_VEC          13
#define TIM1_COMPB_VEC          14
#define TIM1_OVF_VEC                 15
#define TIM0_COMP_VEC             16
#define TIM0_OVF_VEC                 17
#define FPGA_INT4_VEC              18
#define FPGA_INT5_VEC              19
#define FPGA_INT6_VEC              20
#define FPGA_INT7_VEC              21
#define UART0_RXC_VEC            22
#define UART0_DRE_VEC           23
#define UART0_TXC_VEC            24
#define FPGA_INT8_VEC              25
#define FPGA_INT9_VEC              26
#define FPGA_INT10_VEC            27
#define FPGA_INT11_VEC            28
#define UART1_RXC_VEC            29
#define UART1_DRE_VEC           30
#define UART1_TXC_VEC            31 
#define FPGA_INT12_VEC            32
#define FPGA_INT13_VEC            33
#define FPGA_INT14_VEC            34
#define FPGA_INT15_VEC            35
#define I2C_INT_VEC                      36



#endif  /* __iofpslic_h */
