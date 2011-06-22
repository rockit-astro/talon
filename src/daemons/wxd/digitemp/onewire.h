/* -----------------------------------------------------------------------
   Low Level Dallas MicroLan interface software
   
   Copyright 1997 by Brian C. Lane
   All rights Reserved
   
   -----------------------------------------------------------------------*/
int Setup( char *ttyname );
int TouchReset( int fd, int timeout );
int TouchBits( int fd, int timeout, int nbits, unsigned char outch  );
int TouchByte( int fd, int timeout, unsigned char outch  );

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: onewire.h,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $
 */
