/*
**  Header file : eeprom.h
*/

#ifndef		__EEPROM__H
#define		__EEPROM__H

// These work for devices with more than 256 bytes of EEPROM
int EEPROMwrite( int location, unsigned char);
unsigned char EEPROMread( int);

// These work for devices with 256 or less bytes of EEPROM
int _256EEPROMwrite( int location, unsigned char);
unsigned char _256EEPROMread( int);

#endif
