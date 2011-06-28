#ifndef __ASSERT_H
#define __ASSERT_H
#ifdef NDEBUG
#define assert(c)	((void)0)
#else
void _assert(char *, char *, int);
#define assert(c)	((c) ? (void)0 : _assert(#c, __FILE__, __LINE__))
#endif
#endif
