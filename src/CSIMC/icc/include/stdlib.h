#ifndef __STDLIB_H
#define __STDLIB_H
#include <_const.h>

#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1
#ifndef NULL
#define NULL	0
#endif
#include <limits.h>
#define RAND_MAX	INT_MAX

#ifndef __SIZE_T
#define __SIZE_T
typedef unsigned int size_t;
#endif

void abort(void);
int abs(int);
double atof(CONST char *);
int atoi(CONST char *);
long atol(CONST char *);
void *calloc(size_t, size_t);
void exit(int);
void free(void *);
void *malloc(size_t);
int rand(void);
void *realloc(void *, size_t);
void srand(unsigned);
long strtol(CONST char *, char **, int);
unsigned long strtoul(CONST char *, char **, int);
#endif

