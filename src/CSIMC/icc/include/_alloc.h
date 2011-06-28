typedef struct cell_hdr
	{
	struct cell_hdr *next;
	void *EndAddr;
	int size;
#ifdef DEBUG
	unsigned int InUse;
#endif
	} CELL_HDR;

extern CELL_HDR *__FreeList;

#define _BND	1

#define NEW_SIZE(s, e)		(((char *)e) - (((char *)s) + sizeof (CELL_HDR)))
#define REAL_SIZE(siz)		((siz) + sizeof (CELL_HDR))
#define INCR_SIZE(s, siz)	((char *)(s) + REAL_SIZE(siz))
#define DELTA				(sizeof (CELL_HDR) + 8)

#define GET_HDR(p) 			(CELL_HDR *)((char *)p - sizeof (CELL_HDR))

extern CELL_HDR *__FreeList;
