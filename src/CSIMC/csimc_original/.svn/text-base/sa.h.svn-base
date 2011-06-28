/* master include file for the CSIMC stand-alone code */

#ifndef _SA_H
#define _SA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <math.h>

#include "ver.h"

#ifndef HOSTTEST
#include <hc12.h>
#else
#include <limits.h>

/* yes, I know definitions in include files are a BAD THING */
int PORTJ, PORTH, DPAGE, PORTT, ATDSTAT, KWIFJ, KWIEJ, KPOLJ, SC1CR2, PORTE;
int PORTF, SC0CR2, SC0SR1, SC0DRL, SC1SR1, SC1DRL, SC1CR2, RTIFLG, COPRST;
int SC1BDH, SC1BDL, SC1CR1, DDRJ;
unsigned char _data_end, _bss_start, _bss_end, _text_start, _text_end;

#define INTR_ON()
#define INTR_OFF()
#define asm(x)
#endif  /* HOSTTEST */

#include "csimc.h"

#define AWORD(x)    (*(volatile Word *)(x))
#define ABYTE(x)    (*(volatile Byte *)(x))

/* Longs are to avoid using long's in EEPROM because it drags in a big lib.
 * they also fabricating much smaller/faster code in some cases.
 * N.B. the union depends on big-endian order.
 */
typedef union
{
    struct
    {
        Byte b3, b2, b1, b0;
    } B;
    struct
    {
        Word u, l;
    } H;
    long l;
} Long;

/* var.c ********************************************************************/

typedef long VType;     /* type to use for a stack element */

#define NNAME   7       /* sig chars in func and param names */
#define NCNAME  4       /* sig chars in core names (NNAME - "@NN") */
#define NFUNP   10      /* max number of formal function params */
#define NTMPV   10      /* max temp variables within a function */
#define NUSRV   26      /* number of global user variables */

/* variable reference packing.
 * includes index, class and node address.
 * if address matches our board, it is local else send to given node.
 * would use a struct with bitfields but awkward to save that on the STACK or
 *   within an opcode.
 */
typedef Word VRef;      /* packed varable reference info */
typedef enum
{
    VC_CORE = 0,        /* a core variable */
    VC_USER,            /* a user variable, a-z */
    VC_FPAR,            /* a func parameter */
    VC_FTMP,            /* a func temp */
} VClass;           /* stored in VRef at VR_CLASSSHIFT */
#define VR_IDXSHIFT 0   /* bit position of index */
#define VR_IDXBITS  6   /* n bits for index */
#define VR_IDXMASK  ((1<<VR_IDXBITS)-1)   /* mask, when right-justified */
#define VR_CLASSSHIFT   6   /* bit position of VClass */
#define VR_CLASSBITS    2   /* n bits for VClass */
#define VR_CLASSMASK    ((1<<VR_CLASSBITS)-1) /* mask, when right-justified */
#define VR_ADDRSHIFT    8   /* bit position for node addr (if VR_NETREF) */
#define VR_ADDRBITS 5   /* n bits for node address (if VR_NETREF) */
#define VR_ADDRMASK ((1<<VR_ADDRBITS)-1)  /* mask, when right-justified */
#define BADVREF     255 /* return from v_gr() if no match */

extern VRef vgr (char name[], char fpn[][NNAME+1], int nfpn);
extern VType vgv (VRef r);
extern VType vsv (VRef r, VType v);

/* the core variables.
 * each can have a function for getting/setting if necessary.
 * N.B. must match order of CV_* defines
 */
typedef void (*GCVarF)(VType *vp);
typedef int (*SCVarF)(VType *vp);
typedef struct
{
    char name[NCNAME+1];    /* name (well, first NCNAME+'\0' anyway) */
    GCVarF g;           /* helper func to "get", NULL to just use v */
    SCVarF s;           /* helper func to "set", NULL to just use v */
    VType v;            /* value, or as per g/s; init to default */
} CVar;
extern CVar cvar[];     /* core variables */
extern int ncvar;       /* number of core variables */

/* used for very quick access to core variables.
 * N.B. must be in alphabetical order and match cvar[].
 */
typedef enum
{
    CV_AD0, CV_AD1, CV_AD2, CV_AD3, CV_AD4, CV_AD5, CV_AD6, CV_AD7,
    CV_CLOCK,
    CV_EERROR, CV_EPOS, CV_ESIGN, CV_ESTEPS, CV_ETPOS, CV_ETRIG, CV_ETVEL,
    CV_EVEL,
    CV_HOMEBIT,
    CV_IEDGE, CV_ILEVEL, CV_IPOLAR,
    CV_KV,
    CV_LIMACC,
    CV_MAXACC, CV_MAXVEL, CV_MDIR, CV_MPOS,
    CV_MSTEPS, CV_MTPOS, CV_MTRIG, CV_MTVEL, CV_MVEL, CV_MYADDR,
    CV_NLIMBIT,
    CV_OLEVEL, CV_ONTRACK,
    CV_PEER, CV_PLIMBIT, CV_PROMPT,
    CV_TIMEOUT, CV_TOFFSET, CV_TRACE,
    CV_VASCALE, CV_VERSION,
    CV_WORKING,
    CV_N
} CVIds;

/* N.B. following are only for non-compiler code, e.g. internal code
 * where fast access is required. They do *not* invoke any side effects.
 */
#define cvg(id)     (cvar[id].v)    /* access a core var v-value FAST */
#define cvgw(id)    (((Long*)(&cvg(id)))->H.l) /* get just lower word */
#define cvs(id,x)   (cvg(id)=(x))   /* set a core var v-value FAST */
#define cvsw(id,x)  (cvgw(id)=(x))  /* set just lower word */

extern int atLimit (int motdir);
extern void resetLimit (int motdir);


/* cq.c ********************************************************************/

#define QLEN    (1<<(8*sizeof(Byte)))   /* allows free modulos */
typedef struct
{
    Byte q[QLEN];       /* queue */
    Byte h;         /* head: inc then add */
    Byte t;         /* tail: inc then get */
} CQueue;

extern void CQinit(CQueue *cp);
extern int  CQn(CQueue *cp);
extern int  CQnfree(CQueue *cp);
extern int  CQisEmpty(CQueue *cp);
extern int  CQisFull(CQueue *cp);
extern void CQput(CQueue *cp, Byte d);
extern Byte CQget(CQueue *cp);
extern void CQputa (CQueue *cp, Byte a[], int na);

/* lex and yacc **************************************************************/

typedef union
{
    char s[NNAME+1];
    VType v;
} YYSType;
#define YYSTYPE YYSType
extern int yylex (void);

/* thread.c ****************************************************************/

typedef Byte Opcode;        /* size of one raw opcode, sans any params */
typedef short BrType;       /* size of Branch opcode offsets */
typedef char *CPType;       /* a generic pointer that can be incremented */

/* per-thread yyparse support */
typedef char NameSpc[NNAME+1];
typedef struct
{
    NameSpc *fpn;       /* malloced set of function parameter names */
    int nfpn;           /* number in fpn[] */
    int ntmpv;          /* number temps used by new function */
    char *bigstring;        /* malloced by lex to collect a STRING */
    int nbigstring;     /* total chars in bigstring */
    int yylen;          /* used by lex.c */
    int base;           /* used by lex.c */
    int state;          /* used by lex.c */
    char push;          /* used by lex.c */
    char hist;          /* used by lex.c */
    int yynerrs;        /* pulled straight from mcg.tab.c */
    int yyerrflag;      /* ... */
    YYSTYPE yyval;      /* ... */
    YYSTYPE yylval;     /* ... */
    int yychar;         /* ... */
    short *yyssp;       /* ... (just a pointer, not malloced) */
    YYSTYPE *yyvsp;     /* ... (just a pointer, not malloced) */
    short *yyss;        /* these are malloced [YYSTACKSIZE] */
    YYSTYPE *yyvs;      /* these are malloced [YYSTACKSIZE] */
    int maxnyyvs;       /* used to find largest usage */
} PerYYparse;

/* per-thread info */
#define YYSTACKSIZE     150 /* "compile-time" stack size .. chg@will*/
#define NSTACK      100     /* n VTypes for "run-time" stack.. chg@will */
#define NCSTACK     800 /* n bytes for C stack .. chg@will */
typedef struct
{
    char *mcs;          /* malloced stack used by C during this thread*/
    char *mincs;        /* used to gather deepest C stack stat */
    long startt;        /* clock tick when this thread started */
    Byte flags;         /* see below [much smaller than bitfields :-(]*/
    Byte peer;          /* addr we communicate with */
    jmp_buf ctxt;       /* C context, used to context-switch threads */
    CQueue sin;         /* stdin buffer from `from' (or rs232 rcv) */
    CQueue sout;        /* stdout buffer to `to' (or rs232 xmt) */
    PerYYparse yyp;     /* yyparse state */
    VType stack[NSTACK];    /* yacc stack */
    VType *sp;          /* yacc stack pointer, points into stack[] */
    VType *minsp;       /* used to gather largest stack stat */
    VType *fp;          /* yacc frame pointer, points into stack[] */
    CPType pc;          /* yacc pc, points into ucode[] or a fstbl */
    Byte ucode[1];      /* yacc code. Must Be Last: actual arry follws*/
} PerThread;
extern PerThread pti;

/* Thread flags */
#define TF_INUSE    0x01    /* this thread is in use */
#define TF_STARTED  0x02    /* this thread has started and can be resumed */
#define TF_EXECUTING    0x04    /* this thread is executing (AOT compiling) */
#define TF_INTERRUPT    0x08    /* basically go back to compiling */
#define TF_WEDIE    0x10    /* this thread will now die */
#define TF_PROMPT   0x20    /* this thread should issue prompts */
#define TF_TRACE    0x40    /* this thread should trace its actions */
#define TF_RS232    0x80    /* this thread is for the rs232 port */

#define PC  (pti.pc)    /* shorthand for yacc pc */
#define SP  (pti.sp)    /* shorthand for yacc sp */
#define FP  (pti.fp)    /* shorthand for yacc fp */
#define STACK(i) (pti.stack[i]) /* reference i entries into stack[] */

#define PUSH(x) ((*--SP)=(VType)(x))    /* push x onto the stack */
#define POP()   (*SP++)     /* pop top off the stack */
#define PEEK(x) (SP[x])     /* peek at stack location. current is at 0 */

#define initPC()    (PC = (CPType)pti.ucode)

extern void scheduler(int polite);
extern void fatalError (char *msg);
extern void threadStats(void);
extern void timeStats(void);
extern void maxMalloc(void);
extern void runThread(void);
extern void intrThread(int addr);
extern void prompt (int primary);
extern void setPTI(int thr);
extern int getPTI(void);
extern int setThread(Byte addr);
extern long getClock(void);
extern void setClock(long clockticks);
extern void checkStack (int where);
extern void initAllThreads(void);

#ifndef HOSTTEST
#define NTHR        8   /* can be as high as 25: 0..f + 17..1f */
#else
#define NTHR        1   /* we don't try to fake the DPAGE feature */
#endif  /* HOSTTEST */

/* exec.c ********************************************************************/

extern int execute_1(void);
extern void initFrame(void);

typedef enum
{
    OC_HALT=0,  OC_PUSHC,   OC_PUSHV,   OC_PUSHR,   OC_NOT,
    OC_COMP,    OC_UMINUS,  OC_ASSIGN,  OC_ADDOP,   OC_SUBOP,
    OC_MULTOP,  OC_DIVOP,   OC_MODOP,   OC_OROP,    OC_XOROP,
    OC_ANDOP,   OC_LSHOP,   OC_RSHOP,   OC_PREINC,  OC_PREDEC,
    OC_POSTINC, OC_POSTDEC, OC_MULT,    OC_DIV,     OC_MOD,
    OC_ADD, OC_SUB,     OC_LSHIFT,  OC_RSHIFT,  OC_AND,
    OC_OR,  OC_XOR,     OC_LT,      OC_LE,      OC_EQ,
    OC_NE,  OC_GE,      OC_GT,      OC_LAND,    OC_LOR,
    OC_BRT, OC_BRF,     OC_BR,      OC_POPS,    OC_UCALL,
    OC_URETURN, OC_ICALL,
    OC_N
} OpCode;

/* funcs.c *******************************************************************/

/* one symbol table entry for a user function.
 * can be reused by setting name[0] == '\0'
 * TODO: guard against deleting funcs used by other funcs?
 */
typedef struct
{
    char name[NNAME+1];     /* up to first NNAME chars of name, or "" */
    Byte *code;         /* malloc'd list of instructions, or NULL */
    int size;           /* number of bytes of code */
    Byte nargs;         /* number of expected args */
    Byte ntmpv;         /* number of stack locations need for $ temps */
} UFSTblE;

/* one symbol table entry for a built-in function */
typedef int (*BIType)(void);    /* type of each built-in func */
typedef struct
{
    char name[NNAME+1];     /* up to first NNAME chars of name, or '\0' */
    Byte minnargs, maxnargs;    /* range of number of args */
    BIType addr;        /* address of C code */
} IFSTblE;

extern UFSTblE *findUFunc (char name[]);
extern UFSTblE *newUFunc (void);
extern int freeUFunc (char name[]);
extern IFSTblE *findIFunc (char name[]);
extern void ufuncStats (void);

/* ****** intr.c **********************************************************/

#define PTTON()         (PORTF |= 0x08)         /* PTT on */
#define PTTOFF()        (PORTF &= ~0x08)        /* PTT off */

extern void NINTR_OFF(void);
extern void NINTR_ON(void);

extern void uiInc (unsigned int *p);

#define LONGASN(d,s)    (((Long*)&(d))->H.u = ((Long*)&(s))->H.u,   \
             ((Long*)&(d))->H.l = ((Long*)&(s))->H.l)

extern Long clocktick;
extern long upticks;

#define GETACKSZ    (2*sizeof(VType)) /* n bytes in ACK for GETVAR, + ESC */

extern Byte xpkt[PMXLEN];
extern Byte xacked;
extern Byte xpktlen;
extern Byte xackpkt[PB_NZHSZ+GETACKSZ];
extern Byte xacklen;
extern Byte acksent;
extern Pkt rpkt;
extern Byte weRgateway;
extern Byte rpktrdy;
extern void sendAck (void);
extern void resendAck (void);
extern void sendXpkt (void);
extern Byte rseq[NADDR];
extern unsigned maxcop;
extern Byte ourtoken;
extern Byte wanttoken;

/* ****** motenc.c **********************************************************/

extern void getEncPos (Long *ep);
extern void getEncStatus (int *sp);
extern void resetEncStatus (void);
extern void setEncZero (void);
extern void getMotPos (Long *mp);
extern void getMotVel (Long *mp);
extern void setMotVel (Long *vp);
extern void setMotDir (int dir);
extern void setMotZero (void);
extern int getMotDir (void);
extern int getBrdAddr (void);
extern Word getFlexVer (void);
extern void stopMot (void);

extern void _NewHeap(void *start, void *end);
extern Byte _data_end, _bss_start, _bss_end, _text_start, _text_end;

/* lan.c ********************************************************************/

extern unsigned n_rd;
extern unsigned n_ri;
extern unsigned n_rx;
extern unsigned n_ru;
extern unsigned n_rg;
extern unsigned n_r0, n_r1;
extern unsigned n_x0, n_x1;
extern unsigned n_nv0, n_nv1;
extern unsigned n_nf0, n_nf1;
extern unsigned n_fe0, n_fe1;
extern unsigned n_si0, n_si1;

extern int oflush (void);
extern void lanStats(void);
extern void sendLogMsg (char *);
extern void sendSERDATA(void);
extern int dispatchSHELL(void);
extern void dispatchSETVAR(void);
extern void dispatchGETVAR(void);
extern int getChar (void);
extern void sendSETVAR (int to, VRef r, VType v);
extern void sendGETVAR (int to, VRef r, VType *vp);
extern int cpyunESC (Byte *dst, Byte *src, int n);

#define TOKMASTER   0
#define addr2token(a)   ((a) | 0x20)
#define token2addr(a)   ((a) & ~0x20)

/* rs232.c ********************************************************************/

extern Byte rs232peer;
extern Byte rs232pti;
extern void endRS232(void);
extern void runRS232(void);
extern int dispatchSERSETUP(void);
extern int dispatchSERDATA(void);
#define NO232GW()   (!(PORTT&0x40))     /* true when 232 pin in */

/* main.c *****************************************************************/

#ifndef HOSTTEST
extern void main (void);
#else
extern int main (int ac, char *av[]);
#endif
extern long f2l (float f);
extern int fsign (float x);
extern void verStats(void);

/* motion.c *****************************************************************/

#define SPCT        .001024 /* seconds per clock tick */
#define MSPCT       1.024   /* handy mseconds per clock tick */
#define MOTCONRATE  268.435 /* MOTCON = steps/sec * MOTCONRATE */

extern int n_st;

extern void onMotion(void);
extern int etpos (VType *vp);
extern int etvel (VType *vp);
extern int mtpos (VType *vp);
extern int mtvel (VType *vp);
extern void gepos (VType *vp);
extern void gevel (VType *vp);
extern void gmpos (VType *vp);
extern void gmvel (VType *vp);
extern void gmdir (VType *vp);
extern int startTrack (int enc, VType t0, VType dt, VType *pos, int npos);
extern void killMotion(int emerg);
extern void syncNow(void);

/* boot.c *****************************************************************/

#define ISSELFTEST()    (!(PORTT & 0x20))   /* true when selftest pin in */

extern Word fixChksum (Word sum);
extern int chkSum (Byte p[], int n);
extern void errFlash (int v);
extern void resetCOP (void);
extern void giveUpToken(void);

/* utils.c *****************************************************************/

extern void zeroBSS(void);
extern void spinDelay(int n);
extern void errFlash(int p);
extern Byte explodeData (Byte *data, int *ip);
extern void rebootViaCOP(void);

/* ver.c *****************************************************************/
extern unsigned int version;

#endif /* _SA_H */

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: sa.h,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
