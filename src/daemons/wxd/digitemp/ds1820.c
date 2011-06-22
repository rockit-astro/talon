/* -----------------------------------------------------------------------
   Dallas 1820 MicroLan interface
      
   Copyright 1997 by Nexus Computing

   All Rights Reserved
   
   -----------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <linux/serial.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <math.h>

#include "onewire.h"
#include "ds1820.h"

extern int debug;                         /* Debugging level */

/* ------------------------------------------------------------------------
   CRC table for Dallas 8-bit CRC calculations from App. Note #27
   ------------------------------------------------------------------------ */
unsigned char crc_table[] = {
     0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
   157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
    35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
   190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
    70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
   219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
   101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
   248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
   140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
    17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
   175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
    50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
   202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
    87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
   233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
   116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53,
};


/* ----------------------------------------------------------------------- *
   Send a Match ROM command to the indicated OneWire device
   
   Returns -1 if an error occurrs

 * ----------------------------------------------------------------------- */
int MatchROM( int fd, int timeout, struct _roms *rom_list, int rom )
{
  int	x;
  
  if( ( TouchReset( fd, timeout ) ) < 0 )
  {
    printf("Error initalizing OneWire device\n");
    return -1;
  }
  
  if( TouchByte( fd, timeout, 0x55 ) < 0 )
  {
    printf("Error issuing Match ROM command\n");
    return -1;
  }

  for( x = 0; x < 8; x++ )
  {
    if( TouchByte( fd, timeout, rom_list->roms[(rom * 8) + x] ) < 0 )
    {
      printf("Error Writing byte %d of ROM #%d\n", x, rom );
      return -1;
    }
  }
  
  return 0;
}


/* ----------------------------------------------------------------------- *
   Read the Scratchpad from a selected device
   
   It assumes that the device has already been selected by using a ROM
   match or skip ROM command.

 * ----------------------------------------------------------------------- */
int ReadScratchpad( int fd, int timeout, unsigned char *scpad )
{
  int 	x, byte;
  unsigned char crc;
  
  if( TouchByte( fd, timeout, 0xBE ) < 0 )
  {
    printf("Error issuing Read Scratchpad command\n");
    return -1;
  }
    
  for( x = 0; x < 9; x++ )
  {

    if( ( byte = TouchByte( fd, timeout, 0xFF ) ) < 0 )
    {
      printf("Error reading byte %d of scratchpad\n", x );
      return -1;
    }
    scpad[x] = (unsigned char) byte;
  }

  if( ( TouchReset( fd, 5 ) ) < 0 )
  {
    printf("Error initalizing OneWire device\n");
    return -1;
  }

  crc = 0;
  
  /* Check the checksum */
  for( x = 0; x < 9; x++ )
  {
    crc = crc_table[ crc ^ scpad[x] ];
  }
  
  if( crc != 0 )
  {
    printf("CRC Error (%02X) with Scratchpad:", crc);
    for( x = 0; x < 9; x++ )
    {
      printf("%02X ", scpad[x] );
    }
    printf(" -- Skipping it\n");  
    return -1;
  }
  
  return 0;
}


/* ----------------------------------------------------------------------- *
   Write Temperature high and low limits
 * ----------------------------------------------------------------------- */
int WriteLimits( int fd, int timeout, int tl, int th )
{
  
  if( TouchByte( fd, timeout, 0x4E ) < 0 )
  {
    printf("Error writing to scratchpad\n");
    return -1;
  }
  
  if( TouchByte( fd, timeout, th ) < 0 )
  {
    printf("Error writing TH limit\n");
    return -1;
  }
  
  if( TouchByte( fd, timeout, tl ) < 0 )
  {
    printf("Error writing TL limit\n");
    return -1;
  }

  if( ( TouchReset( fd, timeout ) ) < 0 )
  {
    printf("Error initalizing OneWire device\n");
    return -1;
  }
  
  return 0;
}


/* ----------------------------------------------------------------------- *
   Write the limits to the EEPROM
 * ----------------------------------------------------------------------- */
int WriteEE( int fd, int fail_time, int read_time )
{
  struct timeval	wait_tm;

  if( TouchByte( fd, fail_time, 0x48 ) < 0 )
  {
    printf("Error Writing TH/TL limits to EEPROM\n");
    return -1;
  }

  wait_tm.tv_usec = (long) read_time * 1000;
  wait_tm.tv_sec = (long) 0;
          
  /* Do the delay */
  select( 0, NULL, NULL, NULL, &wait_tm );

  if( ( TouchReset( fd, fail_time ) ) < 0 )
  {
    printf("Error initalizing OneWire device\n");
    return -1;
  }
  
  return 0;
}


/* ----------------------------------------------------------------------- *
   Copy the EEPROM to the scratchpad
 * ----------------------------------------------------------------------- */
int ReadEE( int fd, int timeout )
{
  if( TouchByte( fd, timeout, 0xB8 ) < 0 )
  {
    printf("Error reading TL/TH limits from EEPROM\n");
    return -1;
  }

  if( ( TouchReset( fd, timeout ) ) < 0 )
  {
    printf("Error initalizing OneWire device\n");
    return -1;
  }

  return 0;
}


/* ----------------------------------------------------------------------- *
   Read the status of the Power Status
 * ----------------------------------------------------------------------- */
int ReadPWR( int fd, int timeout )
{
  int	p;
  
  if( TouchByte( fd, timeout, 0xB4 ) < 0 )
  {
    printf("Error reading power supply status\n");
    return -1;
  }
  
  p = TouchBits( fd, timeout, 1, 0xFF );

  if( p < 0 )
  {
    printf("Error reading Power Status\n");
    return -1;
  }

  if( ( TouchReset( fd, timeout ) ) < 0 )
  {
    printf("Error initalizing OneWire device\n");
    return -1;
  }

  return p;
}

    
/* ----------------------------------------------------------------------- *
   ReadTemperature - Do a Temperature conversion
   
   Commands DS1820 to do a temperature conversion and then sleeps for 1
   second to make sure it has completed, and then does a master reset.

   Assumes that the device has been previously selected.
 * ----------------------------------------------------------------------- */
int ReadTemperature( int fd, int fail_time, int read_time )
{
  struct timeval	wait_tm;

  if( TouchByte( fd, fail_time, 0x44 ) < 0 )
  {
    printf("Error issuing Read Temperature command\n");
    return -1;
  }

  wait_tm.tv_usec = (long) read_time * 1000;
  wait_tm.tv_sec = (long) 0;
          
  /* Do the delay */
  select( 0, NULL, NULL, NULL, &wait_tm );

  if( ( TouchReset( fd, fail_time ) ) < 0 )
  {
    printf("Error initalizing OneWire device\n");
    return -1;
  }

  return 0;
}


/* ----------------------------------------------------------------------- *
   Skip ROM command
   
   This function resets the TouchMemory device and sends a skip ROM
   command. This is only useful if there is one device connected to the
   bus.
 * ----------------------------------------------------------------------- */
int SkipROM( int fd, int timeout )
{
  if( ( TouchReset( fd, timeout ) ) < 0 )
  {
    printf("Error initalizing OneWire device\n");
    return -1;
  }
  
  if( TouchByte( fd, timeout, 0xCC ) < 0 )
  {
    printf("Error issuing Skip ROM command\n");
    return -1;
  }
  
  return 0;
}


/* ----------------------------------------------------------------------- *
   Read the ROM number of the single connected device
 * ----------------------------------------------------------------------- */
int ReadROM( int fd, int timeout )
{
  int x;
  
  if( ( TouchReset( fd, timeout ) ) < 0 )
  {
    printf("Error initalizing OneWire device\n");
    return -1;
  }

  if( TouchByte( fd, timeout, 0x33 ) < 0 )
  {
    printf("Error issuing Read ROM command\n");
    return -1;
  }
  
  printf("ROM #");
  for( x = 0; x < 8; x++ )
  {
    printf("%02X", TouchByte( fd, timeout, 0xFF ) );
  }
  printf("\n");  

  return 0;
}


/* ----------------------------------------------------------------------- *
   SearchROM - Find the 64 bit serial numbers of all attached devices
   
   serials is a pointer to an array of 64 bit serial numbers
   maxs is the maximum number of serial numbers to support

  -------------------------------------------------------------------------
  Get the Serial numbers of all connected devices
    Each SN is 8 bytes long and is stored into EEPROM in order found.
    The found serial numbers are displayed as they are found and written
    to the EEPROM.
 
  1. Initalize SNsr which keeps track of unknown bits
  2. Initalize SN which is the current serial number
  3. Do a reset
  4. Write a ROM Search (0xF0) command
  5. Start search loop
  6.   Read 2 bits
  7.   If %10 then write a 1 to DS1820 and write a zero to the SR
  8.   If %01 then write a 0 to DS1820 and write a zero to the SR
  9.   If %11 then nothing is connected and we have an error
  10.  If %00 and SR == 1, write 1 to DS1820 and write 1 to SR
  11.  If %00 and SR == 0, write 0 to DS1820 and write 0 to SR
  12.  Repeat 64 times and write final serial # to RS232
  13.  Check the SR for all zeros. If so, then we are done.
  13.  Set all bits past the last 1 in the SR to 1 and clear the last 1
  14.  Repeat steps 2 thru 13
 
  Returns the number of serial devices found, or -1 if there was an error
 
  -------------------------------------------------------------------------

   ----------------------------------------------------------------------- */
int SearchROM( int fd, int timeout, struct _roms *rom_list )
{
  unsigned char	crc, shift_reg[64], serial_num[64], rom_buf[8];
  int		bits, serial_idx, done, x, byte;
  
  
  serial_idx 	= 0;

  /* Initalize shift register for all changed bits */
  for( x = 0; x < 64; x++ )
  {
    shift_reg[x] = 1;
  }

  /* Clear out the ROM buffer */
  for( x = 0; x < 8; x++ )
  {
    rom_buf[x] = 0;
  }
  
  /* Free any allocated ROM memory */
  if( rom_list->roms != NULL )
    free( rom_list->roms );
  rom_list->roms = NULL;
  rom_list->max = 0;
  
  done = 0;
  while( !done )
  {
    if( ( TouchReset( fd, timeout ) ) < 0 )
    {
      printf("Error initalizing OneWire device\n");
      return -1;
    }
    TouchByte( fd, timeout, 0xF0 );	/* ROM Search Command		*/

    /* Find the serial number for one ROM */
    for( x = 0; x < 64; x++ )
    {
        /* Read 2 bits from the devices */
      bits = TouchBits( fd, timeout, 2, 0xFF );

      switch( bits & 0xC0 )
      {
        case 0x00 :	/* Unknown bit at this point. If shift reg. 	*/
        		/* has a 1 then write a 1 to the DS and 1 to SN	*/
        		/* If SR has a zero then write o to DS		*/
        		
    			if( shift_reg[x] == 1 )
    			{
    			  TouchBits( fd, timeout, 1, 0xFF );
			  shift_reg[x] = 1;
			  serial_num[x] = 1;
    			} else {
    			  TouchBits( fd, timeout, 1, 0x00 );
			  shift_reg[x] = 0;
			  serial_num[x] = 0;
    			}
    			break;

        case 0x40 : 	/* All have a 1 bit in this posiiton */
    			TouchBits( fd, timeout, 1, 0xFF );
    			shift_reg[x] = 0;	/* Known data point	*/
    			serial_num[x] = 1;	/* 1 bit at this point	*/
    			break;
    			
        case 0x80 :	/* All have a 0 bit in this position */
        		TouchBits( fd, timeout, 1, 0x00 );
        		shift_reg[x] = 0;	/* Known data point	*/
        		serial_num[x] = 0;	/* 0 bit at this point	*/
    			break;
    			
        case 0xC0 :	/* Error, nothing is connected! */
        		return -1;

    			break;

    				
        default	   :	/* Major malfunction! */
    			break;
      } /* switch */
    } /* for */

    /* Each byte is in order, but the bits are bit reversed */
    /* Unscramble things and stick it into the ROM # buffer */
    byte = 0;
    for( x = 0; x < 64; x++ )
    {
      if( serial_num[x] == 1 )
        byte += pow( 2, (x % 8) );
      
      if( ( x % 8 ) == 7 )
      {
        rom_buf[x / 8] = byte;
        byte = 0;
      }
    }

    /* Check the checksum on the ROM # found and move to next if ok */
    crc = 0;
    /* Check the checksum */
    for( x = 0; x < 8; x++ )
    {
      crc = crc_table[ crc ^ rom_buf[x] ];
    }
  
    if( (crc == 0x00) )
    {
      /* 08/31/98 bcl -- Check for DS1820 family 0x10 */
      if( rom_buf[0] == 0x10 )
      {
        rom_list->max++;			/* One more ROM		*/

        /* CRC passed, allocate some memory and add it to the rom_list */
        if( ( rom_list->roms = realloc( rom_list->roms, rom_list->max * 8 ) ) == NULL )
        {
          printf("Error allocating memory for serial #\n");
          return -1;
        } 

        /* Move the rom buffer into its final location in rom_list */
        for( x = 0; x < 8; x++ )
        {
          rom_list->roms[(serial_idx*8) + x] = rom_buf[x];
        }

        serial_idx++;
      }
    } else {
      printf("CRC Error (%02X) with ROM #", crc);
      for( x = 0; x < 8; x++ )
      {
        printf("%02X ", (unsigned char) rom_list->roms[(serial_idx * 8) + x] );
      }
      printf(" -- Skipping it\n");  
    }
    
    done = 1;				/* Done until proven not	*/
    /* Fill with 1's up to right most 1 and clar it */
    for( x = 0; x < 64; x++ )
    {
      if( shift_reg[63-x] == 0 )
      {
        shift_reg[63-x] = 1;		/* Fill with 1's	*/
      } else {
        shift_reg[63-x] = 0;		/* Clear rightmost 1	*/
        done=0;				/* Not done yet		*/
        break;        			/* Quit the loop	*/
      }
    }
  }

  if( ( TouchReset( fd, timeout ) ) < 0 )
  {
    printf("Error initalizing OneWire device\n");
    return -1;
  }
  return serial_idx;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: ds1820.c,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $"};
