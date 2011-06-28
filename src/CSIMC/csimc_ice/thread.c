/* thread management, aka the "operating system" */

#include "sa.h"

static void chkDispatch(void);

/* anchor the PerThread block at the shared DPAGE area.
 * on the HC12 this is 0x1000 bytes long beginning at 0x7000.
 * _ucode[] here allows us to make sure total size is 0x1000.
 * N.B. icc12 only honors abs_address for initialized objects.
 */
#pragma abs_address:0x7000
PerThread pti = {0};
static Byte _ucode[0x1000 - sizeof(pti)] = {0};
#pragma end_abs_address

#define	RESB	4			/* ms to force resched, must be ^2 */

/* the heart of the multi-tasking environment.
 * call from any thread to voluntarily let some other thread run.
 * return in context of next ready thread (may be the same one, who knows).
 * the key is there are lots of copies of "pti", one for each thread,
 * all at the same location in memory, chosen depending on DPAGE.
 * we don't return to our caller until/unless it is chosen again.
 * there is one C stack per thread which is how setjmp/longjmp can be used in
 *   both directions as simple context changers.
 * if 'polite' the caller could really continue immediately and is just being
 *   a good citizen, else the caller knows what it wants is not available.
 * N.B. in a give thread, this must return before being called again.
 */
void
scheduler(int polite)
{
	static Byte toggle;		/* remember last sched bit */
	int thisthr, newthr;		/* current and new thread */
	Byte b0 = clocktick.B.b0&RESB;	/* get LSB, atomic w/o blocking intrs */

	/* first some periodic chores */
	checkStack(0);			/* check stack bounds */
	if (polite && (pti.flags & TF_INUSE) && b0 == toggle)
	    return;			/* only resched every other clock tick*/
	toggle = b0;			/* set next reschedule bit */
	resetCOP();			/* assure COP we are ok */
	onMotion();			/* check on motion work */
	chkDispatch();			/* check for and dispatch new rpkt */

	/* ok, now let's really decide the next thread */
	thisthr = getPTI();		/* save caller's thread */
	newthr = thisthr;		/* init candidate next thread */
	do {
	    newthr = (newthr+1)%NTHR;	/* round-robin algorithm */
	    if (newthr == thisthr) {	/* full circle means nothing INUSE */
		resetCOP();		/*   assure COP we are ok even if idle*/
		chkDispatch();		/*   good time to check for new data */
	    }
	    setPTI(newthr);		/* set up newthr's DPAGE .. */
	} while (!(pti.flags & TF_INUSE)); /* .. to check if it is in use */

	/* when get here, newthr is next: save old context, (re)instate new. */
	setPTI (thisthr);		/* restore callers context */
	NINTR_OFF();			/* no interrupts during set/longjmp */
	if (setjmp (pti.ctxt) == 0) {	/* now longjmp resumes caller */
	    setPTI (newthr);		/* set new pti */
	    if (pti.flags & TF_STARTED)	/* existing threads get .. */
		longjmp (pti.ctxt, 1);	/* .. resumed */
	    else {			/* new ones get .. */ 
		NINTR_ON();		/* interrupts are safe again */
		pti.startt = upticks + getClock(); /* N.B. brks if in runThrd!*/
		runThread();		/* .. started fresh. never returns */
	    }
	}
	NINTR_ON();
}

/* check for stack overflow, and record max for this thread.
 * N.B. don't call this from an interrupt handler since DPAGE and stack
 *   won't necessarily be consistent.
 */
void
checkStack(int where)
{
	char onstack, *stacknow = &onstack;

	/* record lowest (deepest) stack */
	if (stacknow < pti.mincs)
	    pti.mincs = stacknow;

	/* panic if stack overflows */
	if (stacknow < pti.mcs)
	    errFlash (15 | (where<<13));
}

/* get and set clocktick, atomicly.
 * this returns the number of clock ticks since boot (but note setClock()).
 * one tick occurs ever SPCT secs.
 */
static long _c;
long
getClock(void)
{
	NINTR_OFF();
	asm ("movw _clocktick+2, __c+2");
	asm ("movw _clocktick, __c");
	NINTR_ON();
	return (_c);
}

/* set incremental clock to newticks,
 * adjust upticks to real ticks since boot = upticks + clockticks
 */
void
setClock(long newticks)
{
	upticks += clocktick.l - newticks;

	_c = newticks;
	NINTR_OFF();
	asm ("movw __c, _clocktick");
	asm ("movw __c+2, _clocktick+2");
	NINTR_ON();
}

/* display the largest several pieces of heap space.
 * N.B. in theory we should not call printf() while hogging mem in case it
 *   reschedules a task that needs to run the compiler, but rather than build a
 *   stack string we call oflush() knowing our printf will not fill sout[].
 */
void
maxMalloc(void)
{
#define	NPIECES	8
	char *ptr[NPIECES];
	size_t total;
	int i;

	/* search for biggest pieces, total */
	printf (" Memory:");
	oflush();
	total = 0;
	for (i = 0; i < NPIECES; i++) {
	    size_t b = 1, t = 32767;
	    size_t s;
	    char *p;

	    /* binary search -- s will be 1+ largest malloc */
	    while (b <= t) {
		s = (t+b)/2;
		p = malloc (s);
		if (p) {
		    free (p);
		    b = s+1;
		} else
		    t = s-1;
	    }

	    /* grab and hold until finished */
	    s--;
	    ptr[i] = s > 0 ? malloc (s) : 0;
	    if (ptr[i]) {
		total += s;
		if (i > 0)
		    printf (" +");
		printf (" %u", s);
	    }
	}

	/* free all */
	for (i = 0; i < NPIECES; i++)
	    if (ptr[i])
		free (ptr[i]);

	printf (" = %u\n", total);
#undef	NPIECES
}

/* mark the thread talking with the given addr as having been interrupted.
 */
static void
dispatchPT_INTR(int addr)
{
	int savthr = setThread(addr);
	if (savthr < 0)
	    return;
	pti.flags |= TF_INTERRUPT;
	CQinit (&pti.sin);
	setPTI (savthr);
}

/* mark the pti talking with the given addr for death.
 */
static void
dispatchPT_KILL(int addr)
{
	int savthr = setThread(addr);
	if (savthr < 0)
	    return;
	pti.flags |= TF_WEDIE;
	setPTI (savthr);
}

/* check for and dispatch new data in rpkt.
 * N.B. this routine must reset rpktrdy before returning.
 * N.B. nothing from here may call scheduler until rpktrdy is cleared.
 */
static void
chkDispatch(void)
{
	/* nothing to do if no rpkt */
	if (!rpktrdy)
	    return;

	/* just ack again if acked fr before (rpkt can't be ack) */
	if ((rpkt.info & PSQ_MASK) == rseq[rpkt.fr] &&
						rpkt.fr == xackpkt[PB_TO]) {
	    resendAck();
	    n_rd++;
	} else {
	    /* dispatch based on type */
	    switch (rpkt.info & PT_MASK) {
	    case PT_SHELL:
		if (dispatchSHELL() == 0)
		    sendAck();
		break; 
	    case PT_BOOTREC:
		/* call special handler in PPAGE 4 -- never returns */
		stopMot();		/* safe side */
		resetCOP();		/* give loader a head start */
		asm ("ldd #%rpkt");	/* first arg goes in D register */
		asm ("call 0x8c00,4");	/* set PPAGE to 4, call s/a loader */
		errFlash (16);		/* should never return */
		break;
	    case PT_INTR:
		dispatchPT_INTR (rpkt.fr);
		sendAck();
		break;
	    case PT_KILL:
		dispatchPT_KILL (rpkt.fr);
		sendAck();
		break;
	    case PT_ACK:
		/* handled above */
		break;
	    case PT_REBOOT:
		/* should never get here but just in case */
		rebootViaCOP();
		break;
	    case PT_PING:
		sendAck();
		break;
	    case PT_SETVAR:
		dispatchSETVAR();
		sendAck();
		break;
	    case PT_GETVAR:
		dispatchGETVAR();
		/* includes special ACK */
		break;
	    case PT_SERDATA:
		if (dispatchSERDATA() == 0)
		    sendAck();
		break;
	    case PT_SERSETUP:
		if (dispatchSERSETUP() == 0)
		    sendAck();
		break;
	    default:
		n_ri++;
		break;
	    }
	}

	/* in any case, free rpkt */
	rpktrdy = 0;
}

/* print an elapsed time, dt, in clock ticks as HHHH:MM */
static void
prTime (long dt)
{
	long min;
	int dtmin, dthr;

	min = f2l(dt*(SPCT/60.));
	dthr = min/60;
	dtmin = min%60;
	printf ("%4d:%02d", dthr, dtmin);
}

/* print some time stats */
void
timeStats(void)
{
	static long lastup;
	long up;

	/* print some time stats */
	up = upticks + getClock();
	printf ("   Time: %5d motlat %3d%% COP ", n_st, maxcop/10);
	prTime (up - lastup);
	printf (" dT  ");
	prTime (up);
	printf (" up\n");

	/* reset some stats */
	lastup = up;
	n_st = 0;
	maxcop = 0;
}

/* print stats about each thread in use, then reset some */
void
threadStats(void)
{
	int savthr = getPTI();
	long t0 = upticks + getClock();
	int i;

	/* print the per-thread stats */
	for (i = 0; i < NTHR; i++) {
	    setPTI (i);
	    if (pti.flags & TF_INUSE) {
		/* gather in thread i, but print in savthr */
		int peer = pti.peer;
		int PTI = getPTI();
		int cts = pti.yyp.maxnyyvs;
		int rts = &STACK(NSTACK) - pti.minsp;
		int ncs = &pti.mcs[NCSTACK] - pti.mincs;
		int tty = pti.flags & TF_RS232;
		long tup = t0 - pti.startt;

		setPTI(savthr);
		printf ("Peer %2d:", peer);
		printf ("%6d TId  ", PTI);
		printf ("%5ld%% CTStk", 100L*cts/YYSTACKSIZE);
		printf ("%5ld%% RTStk", 100L*rts/NSTACK);
		printf ("%5ld%% CStk", 100L*ncs/NCSTACK);
		prTime (tup);
		printf (" up");
		if (tty)
		    printf ("    (232)");
		printf ("\n");
	    }
	}

	setPTI (savthr);
}

/* issue prompt to current thread, if enabled and stdin is empty.
 * then if "pri" issue primary prompt, else secondary.
 */
void
prompt(int pri)
{
	/* TODO: sin should be empty but previous \n still there */
	if (CQn(&pti.sin) <= 1 &&
			(pti.flags & (TF_WEDIE|TF_PROMPT)) == TF_PROMPT) {
	    printf ("%02d%c ", getBrdAddr(), pri ? '>' : '.');
	    oflush();
	}
}

/* assuming pti is set up, init a fresh thread to talk with node addr */
static void
initThread(int addr)
{
#ifndef HOSTTEST
	memset (&pti, 0, 0x1000);
	pti.minsp = &STACK(NSTACK);
	pti.peer = addr;
	pti.flags = TF_INUSE;
#endif /* HOSTTEST */
}

/* free any memory in use by this thread */
static void
freeThread (void)
{
	if (pti.mcs) {
	    free (pti.mcs);
	    pti.mcs = 0;
	}
}

/* call _once_ to clear out all thread data structs */
void
initAllThreads(void)
{
	int i;

	for (i = 0; i < NTHR; i++) {
	    setPTI (i);
	    memset (&pti, 0, 0x1000);
	}
}

/* just like yyparse except we manage its malloced work spaces */
static int
callyyparse(void)
{
	extern int yyparse(void);
	int ret;

	pti.yyp.yyss = (short *) malloc (YYSTACKSIZE * sizeof(short));
	if (!pti.yyp.yyss) {
	    fatalError ("No mem for yyss");
	    return (-1);
	}
	pti.yyp.yyvs = (YYSTYPE *) malloc (YYSTACKSIZE * sizeof(YYSTYPE));
	if (!pti.yyp.yyvs) {
	    fatalError ("No mem for yyvs");
	    return (-1);
	}

	ret = yyparse();

	if (pti.yyp.fpn)
	    free (pti.yyp.fpn);
	pti.yyp.fpn = NULL;
	pti.yyp.nfpn = 0;
	if (pti.yyp.bigstring)
	    free (pti.yyp.bigstring);
	pti.yyp.bigstring = NULL;
	pti.yyp.nbigstring = 0;

	free (pti.yyp.yyss);
	pti.yyp.yyss = NULL;
	free (pti.yyp.yyvs);
	pti.yyp.yyvs = NULL;

	return (ret);
}

/* the brain of the multitasking environment.
 * service the current thread until it dies.
 * call scheduler to keep everyone moving along.
 * when we die clear INUSE and call scheduler one last time -- we never return.
 * good thing too, since we blow off the old and set up a new stack frame.
 */
void
runThread(void)
{
	char *stk;

	/* create and install a new stack */
	pti.flags |= TF_STARTED;
	stk = malloc (NCSTACK);
	if (!stk) {
	    fatalError ("no mem for stack");
	    goto outtahere;
	}
	memset (stk, 1, NCSTACK);	/* N.B. reboots if fill with 0's!! */
	pti.mcs = stk;
	stk += NCSTACK-10;		/* hedge against compiler tricks */
	pti.mincs = stk;
	asm ("lds %stk");
	asm ("ldx %stk");

	/* run until die */
	while (1) {

	    /* next step */

	    if (pti.flags & TF_EXECUTING) {
		if (execute_1() < 0)
		    pti.flags &= ~TF_EXECUTING;
	    } else if (pti.flags & TF_RS232) {
		runRS232();
	    } else {
		/* compiling */
		initFrame();
		initPC();
		prompt(1);
		if (callyyparse() == 0) {
		    initFrame();
		    initPC();
		    pti.flags |= TF_EXECUTING;
		}
	    }

	    /* check states */

	    if (pti.flags & TF_WEDIE)
		break;

	    if (pti.flags & TF_INTERRUPT)
		pti.flags &= ~(TF_EXECUTING|TF_INTERRUPT);

	    /* other turn */
	    scheduler(1);
	}

	/* if get here, this thread is dead */
    outtahere:
	freeThread();
	if (pti.flags & TF_RS232)
	    endRS232();
	pti.flags &= ~TF_INUSE;
	scheduler(0);				/* never returns */
	errFlash (17);				/* but just in case :-) */
}

/* given a logical thread number, 1..NTHR-1, set up DPAGE/pti for that thread.
 * or return such a number for the current DPAGE/thread.
 * the hole is due to the way the CSD chip selects works in addrs 0000..6fff.
 * N.B. no error checking here.. this is just the mach dep part.
 */
void
setPTI (int thr)
{
	DPAGE = thr < 16 ? thr : thr+7;
}

int
getPTI(void)
{	
	return (DPAGE < 16 ? DPAGE : DPAGE-7);
}

/* set pti to the one talking with addr.
 * if first time, init a new one.
 * return the original pti.
 * if none and no more, restore orig PTI and return -1.
 */
int
setThread (Byte addr)
{
	int savthr = getPTI();
	int free, i;

	/* find existing, or new to use */
	free = -1;
	for (i = 0; i < NTHR; i++) {
	    setPTI(i);
	    if (pti.flags & TF_INUSE) {
		if (pti.peer == addr)
		    return (savthr);
	    } else
		free = i;
	}

	/* beware of running out */
	if (free < 0) {
	    setPTI (savthr);
	    return (-1);
	}

	/* create new */
	setPTI (free);
	initThread(addr);
	return(savthr);

}

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: thread.c,v $ $Date: 2001/04/19 21:11:58 $ $Revision: 1.1.1.1 $ $Name:  $
 */
