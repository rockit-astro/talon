#ifndef __STRING_H
#define __STRING_H
#include <_const.h>

#ifndef __SIZE_T
#define __SIZE_T
typedef unsigned int size_t;
#endif

void *memchr(void *, int, size_t);
int memcmp(void *, void *, size_t);
void *memcpy(void *, void *, size_t);
void *memmove(void *, void *, size_t);
void *memset(void *, int, size_t);

char *strcat(char *, CONST char *);
char *strchr(CONST char *, int);
int strcmp(CONST char *, CONST char *);
int strcoll(CONST char *, CONST char *);
char *strcpy(char *, CONST char *);
size_t strcspn(CONST char *, CONST char *);
size_t strlen(CONST char *);
char *strncat(char *, CONST char *, size_t);
int strncmp(CONST char *, CONST char *, size_t);
char *strncpy(char *, CONST char *, size_t);
char *strpbrk(CONST char *, CONST char *);
char *strrchr(CONST char *, int);
size_t strspn(CONST char *, CONST char *);
char *strstr(CONST char *, CONST char *);
/*
char *strtok(char *, char *);
size_t strxfrm(char *, char *, size_t);
*/
#endif
