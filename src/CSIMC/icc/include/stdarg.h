#ifndef __STDARG_H
#define __STDARG_H
typedef char *va_list;
char *_va_start(char *, void *, int);
#define va_start(ap, last_arg)	(ap = _va_start(0, &last_arg, sizeof(last_arg)))
#define va_arg(ap, type)		(ap += sizeof (type), ((type *)ap)[-1])
#define va_end(ap)
#endif
