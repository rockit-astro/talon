#ifndef __STDDEF_H
#define __STDDEF_H
#ifndef NULL
#define NULL 0
#endif

#define offsetof(type, mbr)	((int)(&((type *)0)->mbr))
typedef int ptrdiff_t;
#ifndef __SIZE_T
#define __SIZE_T
typedef unsigned int size_t;
#endif
#endif
