/* -----------------------------------------------------------------------
   Low Level Dallas Semiconductor onewire interface software
   For use with the DS9097 style adapter (4 diodes and a resistor). Not
   with the 9097-U which uses a DS2480 interface chip.
   
   Copyright 1997 by Nexus Computing
   All rights Reserved
   
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


struct _roms {
	unsigned char	*roms;			/* Array of 8 bytes	*/
	int		max;			/* Maximum number	*/
};

struct	termios		term, orig_term;
struct	serial_struct	serial;


/* ----------------------------------------------------------------------- *
   Setup - Setup the tty for 115.2k operation

   returns the fd if successful and -1 if it failed
 * ----------------------------------------------------------------------- */
int Setup( char *ttyname )
{
  int	fd;
  
  /* Open the serial port, turning on the temperature sensor */  
  if( (fd = open( ttyname, O_RDWR) ) == -1 )
  {
    printf("Error opening tty: %s\n", ttyname );
    return -1;
  }

  if(ioctl( fd, TIOCGSERIAL, &serial ) < 0 )
  {
    printf("TIOCGSERIAL failed!\n");
    close( fd );
    return -1;
  }

  /* Get the current device settings */
  if(tcgetattr( fd, &term ) < 0 )
  {
    printf("Error with tcgetattr\n" );
    close( fd );
    return -1;
  }

  serial.flags = (serial.flags & ~ASYNC_SPD_MASK) | ASYNC_SPD_CUST;
  serial.custom_divisor = 1;	/* 115200bps				*/
  
  ioctl( fd, TIOCSSERIAL, &serial );
  
  term.c_lflag = 0;
  term.c_iflag = 0;
  term.c_oflag = 0;
  
  term.c_cc[VMIN] = 1;		/* 1 byte at a time, no timer		*/
  term.c_cc[VTIME] = 0;

  term.c_cflag = CS6 | CREAD | HUPCL | CLOCAL | B38400;

  cfsetispeed( &term, B38400 );	/* Set input speed to 115.2k	*/
  cfsetospeed( &term, B38400 );	/* Set output speed to 115.2k	*/
  
  if(tcsetattr( fd, TCSANOW, &term ) < 0 )
  {
    printf("Error with tcsetattr\n");
    close( fd );
    return -1;
  }

  /* Flush the input and output buffers */
  tcflush( fd, TCIOFLUSH );

  return fd;
}


/* ----------------------------------------------------------------------- *
   Reset the One Wire device and look for a presence pulse from it
   
   returns:	0 - No Presence/device detected
   		1 - Presence Detected no alarm
   		2 - Alarm followed by pressence
   		3 - Short to Ground
   		4 - Serial device error

   This expects the device to already be opened and pass the fd of it
   
 * ----------------------------------------------------------------------- */
int TouchReset( int fd, int timeout )
{
  fd_set		readset;
  struct timeval	wait_tm;
  int			stat;
  unsigned char		wbuff, c;

 /* Flush the input and output buffers */
  tcflush( fd, TCIOFLUSH );
 
  serial.custom_divisor = 11;	/* 10472bps				*/
  ioctl( fd, TIOCSSERIAL, &serial );

  term.c_cflag = CS8 | CREAD | HUPCL | CLOCAL | B38400;

  term.c_cflag &= ~(CSIZE|PARENB);
  				/* Clear size bits, parity check off	*/
  
  term.c_cflag |= CS8;		/* 8 bits per byte			*/

  if(tcsetattr( fd, TCSANOW, &term ) < 0 )
  {
    printf("Error with tcsetattr\n");
    close( fd );
    return -1;
  }

  /* Send the reset pulse */
  wbuff = 0xF0;
  write( fd, &wbuff, 1 );
  
  /* Wait for a response or timeout */
  FD_ZERO( &readset );

  /* Timeout after 5 seconds */
  wait_tm.tv_usec = 0;
  wait_tm.tv_sec = timeout;
        
  c = 0;
  stat = 0;
  
  FD_SET( fd, &readset );
  
  /* Read byte if it doesn't timeout first */
  if( select( fd+1, &readset, NULL, NULL, &wait_tm ) > 0 )
  {
    /* Is there something to read? */
    if( FD_ISSET( fd, &readset ) )
    {
      read( fd, &c, 1 );

      /* If c is still zero, then it is a short to ground */
      if( c == 0 )
        stat = 3;
 
      /* If the byte read is not what we sent, check for an error	*/
      /* For now just return a 1 and discount any errors?? 		*/
      if( c != 0xF0 )
      {
        stat = 1;			/* Got a response of some type	*/
      } else {
        stat = 0;			/* No device responding		*/
      }
    }
  } else {
    stat = 0;				/* Timed out			*/
  }
    
  serial.custom_divisor = 1;	/* 115200bps				*/
  ioctl( fd, TIOCSSERIAL, &serial );

  term.c_cflag &= ~(CSIZE|PARENB);
  				/* Clear size bits, parity check off	*/
  
  term.c_cflag |= CS6;		/* 6 bits per byte			*/

  if(tcsetattr( fd, TCSANOW, &term ) < 0 )
  {
    printf("Error with tcsetattr\n");
    close( fd );
    return -1;
  }

  if( stat == 1 )
    return 1;
  else
    return -1;
}


/* ----------------------------------------------------------------------- *
   TouchBits - Read/Write a number of bits to the TouchMemory device

   Expects that the tty is set up for 115.2k operation
   
   returns the read byte if one received
 * ----------------------------------------------------------------------- */
int TouchBits( int fd, int timeout, int nbits, unsigned char outch  )
{
  unsigned char		c, inch, sendbit, Mask;
  fd_set		readset;
  struct timeval	wait_tm;
  int			x;
  
  Mask = 1;
  inch = 0;

  /* Flush the input and output buffers */
  tcflush( fd, TCIOFLUSH );

  /* Get first bit ready to be sent */
  sendbit = (outch & 0x01) ? 0xFF : 0x00;
  
  /* Send the bits/get the bits */
  for( x = 0; x < nbits; x++ )
  {
    write( fd, &sendbit, 1 );		/* Send the bit			*/
    
    Mask <<= 1;				/* Next bit			*/
    
    sendbit = (outch & Mask) ? 0xFF : 0x00;
    
    inch >>= 1;
    
    /* Get the incoming bit if there is one */
    for(;;)
    {

      /* Wait for a response or timeout */
      FD_ZERO( &readset );

      /* Timeout after 5 seconds */
      wait_tm.tv_usec = 0;
      wait_tm.tv_sec = timeout;
        
      c = 0;
  
      FD_SET( fd, &readset );
  
      /* Read byte if it doesn't timeout first */
      if( select( fd+1, &readset, NULL, NULL, &wait_tm ) > 0 )
      {
        /* Is there something to read? */
        if( FD_ISSET( fd, &readset ) )
        {
          read( fd, &c, 1 );
          inch |= (c & 0x01) ? 0x80 : 0x00;
          break;
        }
      } else {
        return 0xFF;
      }  
    }
  }
    
  return inch;
}


/* ----------------------------------------------------------------------- *
   TouchByte - Read/Write a byte to the TouchMemory device

   Expects that the tty is set up for 115.2k operation
   
   returns the read byte if one received
 * ----------------------------------------------------------------------- */
int TouchByte( int fd, int timeout, unsigned char outch  )
{
  return TouchBits( fd, timeout, 8, outch );
}



/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: onewire.c,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $"};
