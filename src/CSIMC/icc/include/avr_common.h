#ifndef __AVR_COMMON_H
#define __AVR_COMMON_H

#ifndef BIT
#define BIT(x)	(1 << (x))
#endif

#define WDR() asm("WDR")

// Serial Port Macros
#define BAUD9600	25
#define BAUD19K		12

#define UART_TRANSMIT_ON()	UCR |= 0x8
#define UART_TRANSMIT_OFF()	UCR &= ~0x8
#define UART_RECEIVE_ON()	UCR |= 0x10
#define UART_RECEIVE_OFF()	UCR &= ~0x10

int EEPROMwrite( int location, unsigned char);
unsigned char EEPROMread( int);

#endif
