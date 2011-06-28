/*
**  Header file : spi.h
*/
#if defined(_AVR)
/* SPI header for AVR */
#ifndef		__SPI__H
#define		__SPI__H

/*
**  Constants for SpiInit()
*/
#define     SPI_DATA_ORDER_MSB_FIRST    0x00
#define     SPI_DATA_ORDER_LSB_FIRST    0x40
#define     SPI_CLOCK_POLARITY          0x08     
#define     SPI_CLOCK_PHASE             0x04     
#define     SPI_CLOCK_RATE_DIV4         0x00
#define     SPI_CLOCK_RATE_DIV16        0x01
#define     SPI_CLOCK_RATE_DIV64        0x02
#define     SPI_CLOCK_RATE_DIV128       0x03

void SpiInit(unsigned char);
void SpiWriteByte(unsigned char);
unsigned char SpiReadByte(void);
unsigned char SpiReadDataReg(void);

#endif

/*
**  End of Header file
*/
#endif
