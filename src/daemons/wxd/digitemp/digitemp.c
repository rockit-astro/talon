/* -----------------------------------------------------------------------
   DigiTemp Linux interface v1.0
      
   Copyright 1997 by Nexus Computing
   
     digitemp -i			Initalize .digitemprc file
     digitemp -s/dev/ttyS0		Set serial port (required)
     digitemp -f5			Set Fail timeout to 5 seconds
     digitemp -r500			Set Read timeout to 500mS
     digitemp -l/var/log/temperature	Send output to logfile
     digitemp -d			Dump sensors
     digitemp -v			Verbose mode
     digitemp -t0			Read Temperature
     digitemp -a			Read all Temperatures
     digitemp -d1                       Turn on Debug level 1

   =======================================================================
   01/14/99	A user in Sweden (and another in Finland) discovered a
   		long standing bug. In do_temp I should have been using
   		0x100 instead of 0xFF for the subtraction. This caused
   		temperatures below 0 degrees C to jump up 1 degree as
   		it decreased. This is fixed.
   		
		Changed version number to v1.2

   10/20/98	Adding new features from DOS version to keep things
   		consistent. Removing the debug command, not used anyway.
		Added a free() to error condition edit from read_rcfile()   		
		Set some cases of freeing to = NULL, also freed the rom
		list before doing a search rom (searchROM checks too, but
		this is the right place for it).

   08/31/98	Adding a check for family 0x10 so that we can read DS1820s
   		while they are on a network that includes other 1-wire
   		devices.
   		Fixed a problem with freeing uninitalized rom_list when
   		starting a SearchROM. Not sure why this never appeared
   		before.

   03/06/98     Adding a -d debug level to help figure out why this thing
                is no longer working.

   03/13/97     Error in CRC calculation. Wrong # of bytes.
   		Error with 3 sensors. Sometimes doesn't store correct ROM
   		data to .digitemprc -- need to malloc more memory dummy!

   03/08/97	Adding user defined timeouts for failure and for the
   		read delay.

   01/24/97	Changed over to correct baud rate and 6 bits @ 115.2k
   		ROM search function is now working. All low level code
   		is functioning except for Alarm Search. Starting to move
   		into a seperate object file with API for users to write
   		their own code with.
   		
   01/22/97	Working on ROM search routine, double cannot handle a full
   		64 bits for some reason, converting to 64 byte array for
   		each bit.

   01/19/97	Rewriting for new interface. This programs handles all the
   		low level communications with the temperature sensor using
   		the 115200k serial adapter.
   
   01/02/96 	Rewriting this code to be more user friendly
     
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

#include "onewire.h"
#include "ds1820.h"

extern char 	*optarg;              
extern int	optind, opterr, optopt;

char	serial_port[40], log_file[1024];
int	fail_time;				/* Failure timeout	*/
int	read_time;				/* Pause during read	*/
int     debug;                                  /* Debug Level          */

/* ----------------------------------------------------------------------- *
   Print out the program usage
 * ----------------------------------------------------------------------- */
void usage()
{
  printf("\nUsage: digitemp -s<device> [-i -d -l -r -v -t -p -a]\n");
  printf("                -i				Initalize .digitemprc file\n");
  printf("                -s/dev/ttyS0			Set serial port\n");
  printf("                -l/var/log/temperature	Send output to logfile\n");
  printf("                -f5                           Fail delay in S\n");
  printf("                -r500				Read delay in mS\n");
  printf("                -v				Verbose output\n");
  printf("                -t0				Read Sensor #\n");
  printf("                -a				Read all Sensors\n");
  printf("                -d1                           Turn on Debug level\n");
}


/* -----------------------------------------------------------------------
   Return the high-precision temperature value

   Calculated using formula from DS1820 datasheet

   Temperature   = scratch[0]
   Sign          = scratch[1]
   TH            = scratch[2]
   TL            = scratch[3]
   Count Remain  = scratch[6]
   Count Per C   = scratch[7]
   CRC           = scratch[8]
   
                   count_per_C - count_remain
   (temp - 0.25) * --------------------------
                       count_per_C

   If Sign is not 0x00 then it is a negative (Centigrade) number, and
   the temperature must be subtracted from 0xFF and multiplied by -1
      
   ----------------------------------------------------------------------- */
float do_temp( unsigned char *scratch )
{
  float temp, hi_precision;

  if( scratch[1] == 0 )
  {
    temp = (int) scratch[0] >> 1;
  } else {
    temp = -1 * (int) (0x100-scratch[0]) >> 1;
  }
  
  temp -= 0.25;

  hi_precision = (int) scratch[7] - (int) scratch[6];

  hi_precision = hi_precision / (int) scratch[7];

  temp = temp + hi_precision;

  return temp;
}


/* -----------------------------------------------------------------------
   Convert degrees C to degrees F
   ----------------------------------------------------------------------- */
float c2f( float temp )
{
  return 32 + ((temp*9)/5);
}


/* -----------------------------------------------------------------------
   Log one line of text to the logfile with the current date and time
   ----------------------------------------------------------------------- */
int log_line( char *buf )
{
  unsigned char	temp[1024];
  time_t	mytime;
  int 		fd;
  
  if( (fd = open( log_file, O_CREAT | O_WRONLY | O_APPEND,
                            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH ) ) == -1 )
  {
    printf("Error opening logfile: %s\n", log_file );
    return -1;
  }

  mytime = time(NULL);

  strftime( temp, 1024, "%b %d %H:%M:%S ", localtime( &mytime ) );
  
  strcat( temp, buf );
  write( fd, temp, strlen( temp ) );
  close( fd );

  return 0;
}
  

/* -----------------------------------------------------------------------
   Read the temperature from one sensor
   ----------------------------------------------------------------------- */
int read_temp( int fd, struct _roms *rom_list, int sensor, int opts )
{
  char		temp[1024];
  unsigned char	scratch[9];
  int		x;
  float		temp_c;
    
  x = 0;  
  
  MatchROM( fd, fail_time, rom_list, sensor );
  ReadTemperature( fd, fail_time, read_time );

  MatchROM( fd, fail_time, rom_list, sensor );
  if( ReadScratchpad( fd, fail_time, scratch ) < 0 )
  {
    printf("Error reading Scratchpad\n");
    return -1;
  }

  /* Convert data to temperature */
  temp_c = do_temp( scratch );

  /* Write the data to the logfile or console */
  sprintf( temp, "Sensor %d C: %3.2f F: %3.2f\n", sensor, temp_c, c2f(temp_c) );

  /* If file logging is enabled, log it with a timestamp */
  if( log_file[0] != 0 )
  {
    log_line( temp );
  } else {
    printf("%s", temp );
  }

  /* If verbose mode is enabled, show the DS1820's internal registers */
  if( opts & 0x0004 )
  {
    if( log_file[0] != 0 )
    {
      sprintf( temp, "  Temperature   : 0x%02X\n", scratch[0] );
      sprintf( temp, "  Sign          : 0x%02X\n", scratch[1] );
      sprintf( temp, "  TH            : 0x%02X\n", scratch[2] );
      sprintf( temp, "  TL            : 0x%02X\n", scratch[3] );
      sprintf( temp, "  Remain        : 0x%02X\n", scratch[6] );
      sprintf( temp, "  Count Per C   : 0x%02X\n", scratch[7] );
      sprintf( temp, "  CRC           : 0x%02X\n", scratch[8] );
    } else {
      printf("  Temperature   : 0x%02X\n", scratch[0] );
      printf("  Sign          : 0x%02X\n", scratch[1] );
      printf("  TH            : 0x%02X\n", scratch[2] );
      printf("  TL            : 0x%02X\n", scratch[3] );
      printf("  Remain        : 0x%02X\n", scratch[6] );
      printf("  Count Per C   : 0x%02X\n", scratch[7] );
      printf("  CRC           : 0x%02X\n", scratch[8] );
    }
  }
  
  return 0;
}


/* -----------------------------------------------------------------------
   Read the temperaturess for all the connected sensors

   Step through all the sensors in the list of serial numbers
   ----------------------------------------------------------------------- */
int read_all( int fd, struct _roms *rom_list, int opts )
{
  int x;
  
  for( x = 0; x < rom_list->max; x++ )
    read_temp( fd, rom_list, x, opts );  

  return 0;
}


/* -----------------------------------------------------------------------
   Read a .digitemprc file from the current directory

   The rc file contains:
   
   TTY <serial>
   LOG <logfilepath>
   FAIL_TIME <time in seconds>
   READ_TIME <time in mS>
   SENSORS <number of ROM lines>
   Multiple ROM x <serial number in bytes> lines
   
   ----------------------------------------------------------------------- */
int read_rcfile( char *fname, struct _roms *rom_list )
{
  FILE	*fp;
  char	temp[80];
  char	*ptr;
  int	sensors, x;
  
  sensors = 0;
  
  if( ( fp = fopen( fname, "r" ) ) == NULL )
  {
    return -1;
  }
  
  while( fgets( temp, 80, fp ) != 0 )
  {
    if( (temp[0] == '\n') || (temp[0] == '#') )
      continue;
      
    ptr = strtok( temp, " \t\n" );
    
    if( strncasecmp( "TTY", ptr, 3 ) == 0 )
    {
      ptr = strtok( NULL, " \t\n" );
      strcpy( serial_port, ptr );
    } else if( strncasecmp( "LOG", ptr, 3 ) == 0 ) {
      ptr = strtok( NULL, " \t\n" );
      strcpy( log_file, ptr );
    } else if( strncasecmp( "FAIL_TIME", ptr, 9 ) == 0 ) {
      ptr = strtok( NULL, " \t\n");
      fail_time = atoi( ptr );
    } else if( strncasecmp( "READ_TIME", ptr, 9 ) == 0 ) {
      ptr = strtok( NULL, " \t\n");
      read_time = atoi( ptr );
    } else if( strncasecmp( "SENSORS", ptr, 7 ) == 0 ) {
      ptr = strtok( NULL, " \t\n" );
      sensors = atoi( ptr );
      
      /* Reserve some memory for the list */
      if( ( rom_list->roms = malloc( sensors * 8 ) ) == NULL )
      {
        return -1;
      }
      rom_list->max = sensors;

    } else if( strncasecmp( "ROM", ptr, 3 ) == 0 ) {
      ptr = strtok( NULL, " \t\n" );
      sensors = atoi( ptr );
      
      /* Read the 8 byte ROM address */
      for( x = 0; x < 8; x++ )
      {
        ptr = strtok( NULL, " \t\n" );
        rom_list->roms[(sensors * 8) + x] = atoi( ptr );
      }
    } else {
      printf("Error reading .digitemprc\n");
      fclose( fp );
      return -1;
    }
  }
  
  fclose( fp ); 

  return 0;
}


/* -----------------------------------------------------------------------
   Write a .digitemprc file, it contains:
   
   TTY <serial>
   LOG <logfilepath>
   FAIL_TIME <time in seconds>
   READ_TIME <time in mS>
   SENSORS <number of ROM lines>
   Multiple ROM x <serial number in bytes> lines

   ----------------------------------------------------------------------- */
int write_rcfile( char *fname, struct _roms *rom_list )
{
  FILE	*fp;
  int	x, y;

  if( ( fp = fopen( fname, "wb" ) ) == NULL )
  {
    return -1;
  }
  
  fprintf( fp, "TTY %s\n", serial_port );
  if( log_file[0] != 0 )
    fprintf( fp, "LOG %s\n", log_file );

  fprintf( fp, "FAIL_TIME %d\n", fail_time );		/* Seconds	*/
  fprintf( fp, "READ_TIME %d\n", read_time );		/* mSeconds	*/
  
  fprintf( fp, "SENSORS %d\n", rom_list->max );

  for( x = 0; x < rom_list->max; x++ )
  {
    fprintf( fp, "ROM %d ", x );
    
    for( y = 0; y < 8; y++ )
    {
      fprintf( fp, "%d ", rom_list->roms[(x * 8) + y] );
    }
    fprintf( fp, "\n" );
  }

  fclose( fp );
  
  return 0;
}


/* ----------------------------------------------------------------------- *
   DigiTemp main routine
   
   Parse command line options, run functions
 * ----------------------------------------------------------------------- */
int main( int argc, char *argv[] )
{
  int		sensor;			/* Single sensor to read*/
  char		c;
  int		opts;			/* Bitmask of flags	*/
  int		fd;
  int		x, y, stat;
  struct _roms	rom_list;		/* Attached Roms	*/

  fd = 0;

  printf("DigiTemp v1.0 Copyright 1997 by Nexus Computing\n\n");

  if( argc == 1 )
  {
    usage();
    return -1;
  }

  serial_port[0] = 0;			/* No default port		*/
  log_file[0] = 0;			/* No default log file		*/
  fail_time = 5;			/* 5 Second fail default	*/
  read_time = 500;			/* 500mS read delay		*/
  sensor = 0;				/* First sensor			*/

  memset (&rom_list, 0, sizeof(rom_list));
  
  
  /* Read the .digitemprc file first, then let the command line
     arguments override them. If no .digitemprc is found, set up for
     1 sensors (Increased by the SearchROM routine).
  */
  read_rcfile( ".digitemprc", &rom_list );

  /* Command line options override any .digitemprc options temporarily	*/
  opts = 0;
  c = 0;
  while( c != -1 )
  {
    c = getopt(argc, argv, "?hiavr:f:s:l:t:d:");
    
    if( c == -1 )
      break;

    switch( c )
    {
      case 'i':	opts |= 0x0001;			/* Initalize the s#'s	*/
      		break;
      		
      case 'r':	read_time = atoi(optarg);	/* Read delay in mS	*/
      		break;
      		
      case 'f': fail_time = atoi(optarg);	/* Fail delay in S	*/
      		break;
      		
      case 'v': opts |= 0x0004;			/* Verbose		*/
      		break;
      		
      case 's': if(optarg)			/* Serial port		*/
      		{
      		  strcpy( serial_port, optarg );
      		}
      		break;
      		
      case 'l': if(optarg)			/* Log Filename		*/
      		{
      		  strcpy( log_file, optarg );
      		}
      		break;
      		
      case 't':	if(optarg)			/* Sensor #		*/
      		{
      		  sensor = atoi(optarg);
      		  opts |= 0x0020;
      		}
      		break;

      case 'd': if(optarg)
	        {
		  debug = atoi(optarg);
		} else {
		  debug = 1;
		}
      		
      case 'a': opts |= 0x0040;			/* Read All sensors	*/
		break;
		
      case ':':
      case 'h':
      case '?': usage();
      		exit(-1);
      		break;
    
      default:	break;
    }
  }

  if( opts == 0 )				/* Need at least 1	*/
  {
    usage();
    return -1;
  }

  /* Initalize the interface to the DS1820 */
  if( ( fd = Setup( serial_port ) ) < 0 )
  {
    printf("Error initalizing %s\n", serial_port );
    return -1;
  }

  /* First, should we initalize the sensors? */
  /* This should store the serial numbers to the .digitemprc file */
  if( opts & 0x0001 )
  {
    if( ( stat = SearchROM( fd, fail_time, &rom_list ) ) == -1 )
      printf("Error Searching for ROMs\n");
  
    if( rom_list.max > 0 )
    {
      for( x = 0; x < rom_list.max; x++ )
      {
        printf("ROM #%d : ", x );
        for( y = 0; y < 8; y++ )
        {
          printf("%02X", rom_list.roms[(x * 8) + y] );
        }
        printf("\n");
      }
      write_rcfile( ".digitemprc", &rom_list );
    }
  }
  
  /* Should we read one sensor? */
  if( opts & 0x0020 )
  {
    read_temp( fd, &rom_list, sensor, opts );
  }
  
  /* Should we read all connected sensors? */
  if( opts & 0x0040 )
  {
    read_all( fd, &rom_list, opts );
  }
  
  if( rom_list.max > 0 )
    free( rom_list.roms );
  close( fd );
  
  return 0;  
}
