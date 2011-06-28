#ifndef __MACROS_H
#define __MACROS_H	1

#ifndef BIT
#define BIT(x)	(1 << (x))
#endif

#define WDR() 	asm("wdr")
#define SEI()	asm("sei")
#define CLI()	asm("cli")
#define NOP()	asm("nop")
#define _WDR() 	asm("wdr")
#define _SEI()	asm("sei")
#define _CLI()	asm("cli")
#define _NOP()	asm("nop")

// Serial Port Macros
// for 4 Mhz crystal!
#define BAUD9600	25
#define BAUD19K		12

#define UART_TRANSMIT_ON()	UCR |= 0x8
#define UART_TRANSMIT_OFF()	UCR &= ~0x8
#define UART_RECEIVE_ON()	UCR |= 0x10
#define UART_RECEIVE_OFF()	UCR &= ~0x10

#endif
