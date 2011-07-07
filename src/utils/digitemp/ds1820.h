/* -----------------------------------------------------------------------
   Dallas 1820 MicroLan interface
      
   Copyright 1997 by Brian C. Lane
   All Rights Reserved
   
   -----------------------------------------------------------------------*/
struct _roms {
	unsigned char	*roms;			/* Array of 8 bytes	*/
	int		max;			/* Maximum number	*/
};



int MatchROM( int fd, int timeout, struct _roms *rom_list, int rom );
int ReadScratchpad( int fd, int timeout, unsigned char *scpad );
int WriteLimits( int fd, int timeout, int tl, int th );
int WriteEE( int fd, int fail_time, int read_time );
int ReadEE( int fd, int timeout );
int ReadPWR( int fd, int timeout );
int ReadTemperature( int fd, int fail_time, int read_time );
int SkipROM( int fd, int timeout );
int ReadROM( int fd, int timeout );
int SearchROM( int fd, int timeout, struct _roms *rom_list );

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: ds1820.h,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $
 */
