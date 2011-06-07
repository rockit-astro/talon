/* Copyright (C) 1998 Gregory K. Remington */
#define CLASS_MAX			4
#define THREAD_MAX			7

#ifndef DLLPROC
#define DLLPROC FAR PASCAL
#endif

extern BOOL DLLPROC Util32AllowIO(void);
extern BOOL DLLPROC Util32SetPriorityClass(DWORD dwPclass);
extern BOOL DLLPROC Util32GetPriorityClass(DWORD FAR *pdwPclass);
extern BOOL DLLPROC Util32SetThreadPriority(DWORD dwPthread);
extern BOOL DLLPROC Util32GetThreadPriority(DWORD FAR *pdwPthread);
extern unsigned short DLLPROC Util32GetVersion(void);
