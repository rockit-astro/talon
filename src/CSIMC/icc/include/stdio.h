#ifndef __STDIO_H
#define __STDIO_H
#include <_const.h>

#define stdin	0
#define stdout	0
#define stderr	0

int getchar(void);
int putchar(char);
int puts(CONST char *);
int printf(CONST char *, ...);
int sprintf(char *, CONST char *, ...);

#ifndef NULL
#define NULL 0
#endif

#endif
