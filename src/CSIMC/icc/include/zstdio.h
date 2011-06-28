#ifndef	ZSTDIO_H
#define	ZSTDIO_H

#undef TRUE
#define TRUE 1

#undef FALSE
#define FALSE 0 

ioloc unsigned int __OutPort = 0x32;

extern 	void zprintd ( int, unsigned int, unsigned int );
extern 	void zprintx ( int, unsigned int, unsigned int );

#define printd( number )   zprintd( number, FALSE, (unsigned) number )
#define printud( number )  zprintd( number, TRUE, (unsigned) number ) 

#define printx( number )   zprintx( number, FALSE, (unsigned) number )
#define printux( number )  zprintx( number, TRUE, (unsigned) number ) 

#define putchar(c)  { __OutPort = c; }
#define putconst(c) { char ch = c; OutPort = ch; }

#endif

