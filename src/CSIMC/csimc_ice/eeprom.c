/* this is the code that lives in EEPROM.
 */

#include "sa.h"

#ifndef HOSTTEST

Long clocktick;			/* incremented each SPCT second */

/* for NINTR_ON/OFF */
static Word intrnest;

/* stats */
unsigned n_rx;                  /* total packets seen */
unsigned n_ru;                  /* total packets seen for us */
unsigned n_rg;                  /* packets seen and properly checksummed */
unsigned n_si0, n_si1;		/* stray comm interrupts */
unsigned n_nv0, n_nv1;		/* overrun errors */
unsigned n_nf0, n_nf1;		/* noisy bytes */ 
unsigned n_fe0, n_fe1;		/* framing errors */
unsigned maxcop;		/* largest COP interval seen */

/* comm flags and buffers */
Byte ourtoken;			/* set when we have the token */
Byte wanttoken;			/* set when we want the token */
Word nstray;			/* count stray interrupts */

Byte xpkt[PMXLEN];		/* tx packet, for all but ACKs */
Byte xpktbsy;			/* set when xpkt is being filled or sent */
Byte xacked;			/* set when see proper ACK come in for xpkt */
static Byte xpktlen;		/* total bytes to be sent in xpktptr */
static Byte xpkt0nxt;		/* index of next xpktptr Byte to send to SC0 */
static Byte xpkt1nxt;		/* index of next xpktptr Byte to send to SC1 */
static Byte *xpktptr;		/* either xpkt or xackpkt */
static Byte xackpkt[PB_HSZ];	/* dedicated ACK transmission buffer */
static Byte xtokpkt[2];		/* dedicated BROKTOK transmission buffer */
static Byte acksent;		/* flag if care to wait for ACK sent */

Pkt rpkt;                       /* rx packet */
Byte rpktrdy;			/* rpkt dispatch semaphore */
Byte rseq[NADDR];        	/* sequence of last rx packet ACKed, per addr */
static Byte rpktlen;		/* rpkt bytes seen so far */
static Byte nrpktd;		/* bytes of rpkt.data[] seen so far */
static Word rchksum;		/* running checksum */

/* queue for bridging from RS232 to 485.. needed to get PTT right */
#define	NGW	16		/* queue length.. 3 seems enough :-) */
static Byte gw0h, gw0t;		/* head: last added. tail: next to send */
static Byte gw0q[NGW];		/* queue itself */

/* self-test stuff */
static Word lastmml;		/* last motormon.l; for self-test */
static Word lastmmc;		/* clock.L.l @ lastmm; for self-test */
static Word sci0cl;		/* another timer for SCI testing */

static Word lastcop;		/* last clocktick.H.l, for measuring COP */

/* N.B. everything beyond this line goes into EEPROM */
#pragma abs_address:0xf000


/* filler ************************************************************/

/* These really have no business here but we might as well fill up EEPROM
 * to free up more RAM. So much for structured programming in the embedded
 * world.
 */

/* mark the given thread as having been interrupted.
 * return 0 if addr really exists, else -1.
 */
int
intrThread(int addr)
{
	int savthr = getPTI();
	int ret;

	if ((ret = setThread (addr)) == 0)
	    pti.flags |= TF_INTERRUPT;

	setPTI (savthr);
	return (ret);
}

/* kill the pti with from == addr.
 * return 0 if addr really exists, else -1.
 */
int
killThread(int addr)
{
	int savthr = getPTI();
	int ret = setThread (addr);

	if (ret == 0)
	    pti.flags |= TF_WEDIE;

	setPTI (savthr);
	return (ret);
}

/* mark all threads for death */
void
killAllThreads(void)
{
	int savthr = getPTI();
	int i;

	for (i = 0; i < NTHR; i++) {
	    setPTI (i);
	    pti.flags |= TF_WEDIE;
	}

	setPTI(savthr);
}

/* check for and dispatch new data in rpkt.
 * N.B. this routine must reset rpktrdy before returning.
 * N.B. nothing from here may call scheduler until rpktrdy is cleared.
 */
void
chkDispatch(void)
{
	/* nothing to do if no rpkt */
	if (!rpktrdy)
	    return;

	/* just ack again if acked before (rpkt can't be ack) */
	if ((rpkt.info & PSQ_MASK) == rseq[rpkt.fr]) {
	    sendAck();
	    n_rd++;
	} else {
	    /* dispatch based on type */
	    switch (rpkt.info & PT_MASK) {
	    case PT_ROUTINE:
		if (dispatchRoutine() == 0)
		    sendAck();
		break; 
	    case PT_BOOTREC:
		/* must be handled from EEPROM..
		 * reboot by letting COP timeout.
		 */
		while(1)
		    continue;
		break;
	    case PT_INTR:
		if (intrThread (rpkt.fr) == 0)
		    sendAck();
		break;
	    case PT_KILL:
		if (killThread (rpkt.fr) == 0)
		    sendAck();
		break;
	    case PT_ACK:
		/* handled above */
		break;
	    case PT_KILLALL:
		killAllThreads();
		/* N.B. not acked */
		break;
	    case PT_PING:
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


/* misc support ************************************************************/

/* scheme to nest interrupt protection.
 * N.B. this allows nesting, but they must still be balanced.
 */
void NINTR_OFF(void) { INTR_OFF(); ++intrnest; }
void NINTR_ON(void)  { if (--intrnest == 0) INTR_ON(); }

/* increment an unsigned int but don't let it overflow */
void
uiInc (unsigned *p)
{
	if (*p < UINT_MAX)
	    (*p) += 1;
}


/* board hardware ************************************************************/

/* these use Long instead of long because the latter drags too much into EEPROM
 */

/* addresses of Altera chips */
#define	ENCOBASE	0x200
#define	MCONBASE	0x300
#define	MMONBASE	0x380

/* read encoder value */
void
getEncPos (Long *ep)
{
	AWORD(ENCOBASE+6) = 0;
	AWORD(ENCOBASE+6) = 1;
	ep->H.u = AWORD(ENCOBASE);
	ep->H.l = AWORD(ENCOBASE+2);
}

/* return 1 if encoder is complaining, else 0 */
void
getEncStatus (int *sp)
{
	*sp = AWORD(ENCOBASE+4) & 1;
}

/* encoder error bit is lateched, this lets you reset it */
void
resetEncStatus (void)
{
	AWORD(ENCOBASE+4) = 0;
}

/* reset encoder counter to 0. */
void
setEncZero (void)
{
	AWORD(ENCOBASE+8) = 0;
}

/* get motor step counter */
void
getMotPos (Long *mp)
{
	AWORD(MMONBASE+4) = 0;			/* arm latch */
	AWORD(MMONBASE+4) = 1;			/* grab now */
	mp->H.u = AWORD(MMONBASE);
	mp->H.l = AWORD(MMONBASE+2);
}

/* set motor step counter to 0 */
void
setMotZero(void)
{
	AWORD(MMONBASE+6) = 0;
}

/* get current commanded motor step rate */
void
getMotVel (Long *mp)
{
	mp->H.u = AWORD(MCONBASE);
	mp->H.l = AWORD(MCONBASE+2);
}

/* set a new commanded motor step rate */
void
setMotVel (Long *vp)
{
	AWORD(MCONBASE+0) = vp->H.u;
	AWORD(MCONBASE+2) = vp->H.l;
}

/* set a new commanded motor direction, based on sign of dir */
void
setMotDir (int dir)
{
	/* N.B. check that this compiles to chage PORTF atomically, since PTT
	 * is also in PORTF and it can change from an interrupt
	 */
	if (dir >= 0)
	    PORTF |= 0x40;
	else
	    PORTF &= ~0x40;
}

/* get a commanded motor direction, based on sign of dir */
int
getMotDir (void)
{
	return ((PORTF & 0x40) ? 1 : -1);
}

/* get board dip switches */
int
getBrdAddr (void)
{
	return (PORTT & 0x1f);
}


/* self-test ****************************************************************/

static void setupSelfTest(void);
static void selfTest1Pass (void);
static void memCheck (void);
static void spinDelay (int n);

#define	CNTTM	10		/* count up time, secs */
#define MOTHZ	(255./CNTTM)	/* motor rate to count to 255 */

static int
abs (int x)
{
	return (x < 0 ? -x : x);
}

/* run self tests forever if enabled, else return */
static void
selfTest(void)
{
	/* shunt in == tests on */
	DDRT = 0;
	if (PORTT & 0x20)
	    return;

	/* setup */
	setupSelfTest();

	/* go forever */
	while (1)
	    selfTest1Pass();
}

/* do some tests on the encoder */
static void
encTest(void)
{
	Long L;

	AWORD(ENCOBASE+6) = 0;		/* clear latch */
	if (AWORD(ENCOBASE+6) != 0)	/* confirm */
	    errFlash (-1);
	AWORD(ENCOBASE+6) = 1;		/* set latch */
	if (AWORD(ENCOBASE+6) != 1)	/* confirm */
	    errFlash (-2);

	setEncZero();			/* reset enc */
	getEncPos (&L);			/* get value */
	if (L.H.u || L.H.l)		/* insure 0 */
	    errFlash (-3);
}

/* do some basic checks on MCON and MMON */
static void
motconmonTest(void)
{
	Long L;

	/* stop motcon, reset motmon, confirm it reads back 0 */
	L.H.u = L.H.l = 0;
	setMotVel (&L);
	setMotZero();
	getMotPos (&L);
	if (L.H.u || L.H.l)
	    errFlash (-4);

	/* now count down and wait for 1 count, motmon should read back -1.
	 * this is really to check for stuck bits, not to check speed accuracy.
	 */
	setMotDir(-1);
	L.H.u = 0;
	L.H.l = ((Word)(MOTCONRATE*(1./.16) + .5));  /* allow for dely slop*/
	setMotVel (&L);
	spinDelay (1);			/* delay .12 sec */
	getMotPos (&L);
	if (L.H.u != 0xffff || L.H.l != 0xffff)
	    errFlash (-5);

	/* check that motmon latch tracks -- really checking bus access */
	AWORD(MMONBASE+4) = 1;		/* set latch */
	if (AWORD(MMONBASE+4) != 1)	/* confirm 1 */
	    errFlash (-6);
	AWORD(MMONBASE+4) = 0;		/* clear latch */
	if (AWORD(MMONBASE+4) != 0)	/* confirm 0 */
	    errFlash (-7);
}

/* set up motcon and motmon for another loop */
static void
setupPass(void)
{
	Long L;

	setMotDir(1);			/* count up */
	L.H.u = 0;			/* set motor rate */
	L.H.l = (Word)(MOTCONRATE*MOTHZ);
	setMotVel (&L);
	setMotZero();			/* reset motor mon */

	lastmml = 0;			/* init motormon history */
	lastmmc = clocktick.H.l;	/* init clock history */
}

/* set things up for the self-test */
static void
setupSelfTest(void)
{
	COPCTL = 0;			/* shut off COP */
	DDRJ = 0xff;			/* turn Inputs into outputs */
	PORTJ = 0;			/* lights on for safety */
	PORTH = 0;			/* lights on for safety */
	setEncZero();			/* reset encoder */
	resetEncStatus();		/* reset encoder status */
	setupPass();			/* init counter and speed */

	/* set up 485 port for slow polled output */
	SC0BDH = 0x11;
	SC0BDL = 0xc1;			/* 110 baud ~= 100ms/char */
	SC0CR1 = 0x80;			/* RXD general I/O, 8/1/N */
	SC0CR2 = 0x0c;			/* TE RE */
	DDRS = 3;			/* rx line as output, tx still on */
}

/* set porth, reversing bits 2/3 because they are reversed on the PCB :( */
static void
setPORTH (Word w)
{
	PORTH = (w & ~0x0c) | ((w & 0x04) << 1) | ((w & 0x08) >> 1);
}

/* do one self-test pass */
static void
selfTest1Pass (void)
{
	int encerr;
	Long L;

	/* show just enc or some enc+board address on input LEDS, depending on
	 * err or jumper;
	 * show MMON on output LEDs.
	 */
	getEncStatus (&encerr);		/* get encoder status.. 0 is ok */
	encerr |= !(PORTT & 0x40);	/* or fake it if JP4 3/4 in */
	getMotPos (&L);			/* read motor mon */
	setPORTH (~L.H.l);		/* show on output LEDs (as + logic) */
	if (encerr) {
	    PORTJ = ~((L.H.l&0xe0)|(PORTT&0x1f)); /* mix with board address */
	    resetEncStatus();		/* get a fresh look next time */
	} else {
	    Long E;
	    getEncPos (&E);
	    PORTJ = ~(E.H.l>>8);	/* show enc count / 256 */
	}

	/* check that mmon is changing at correct rate. */
	if (abs(clocktick.H.l - lastmmc) > ((Word)(1./(MOTHZ*SPCT)+.5))) {
	    /* usually see 1 motmon change, 2 if window lands right on top */
	    Word mdiff = abs(L.H.l - lastmml) - 1;
	    if (mdiff > 1)
		errFlash(-8);
	    lastmml = L.H.l;
	    lastmmc = clocktick.H.l;
	}

	/* after count up/down 255 counts, do other misc tests, and repeat */
	if (L.H.u) {			/* underflow, time for other stuff */
	    PORTH = 0;			/* output lights on look better */
	    memCheck();			/* handy place to repeat mem tests */
	    encTest();			/* test enc */
	    motconmonTest();		/* test motcon and motmon */
	    setupPass();		/* start another round */
	} else if (L.H.l >= 0xff) {	/* turn around after 255 */
	    setMotDir(-1);
	}

	/* do something with the 485 LEDs...
	 * send a character to use D10, and D9 (PTT) when sending.
	 * PTT also tri-states the 485 receiver so we can turn on the SC0 RXD
	 * line from our side to light D8 for fun.
	 */
	if (SC0SR1 & 0x40) {
	    if (clocktick.H.l >= sci0cl) {
		PTTON();		/* PTT on */
		SC0DRL = 'P';		/* send 0x50 and nice pattern */
		PORTS = 1;		/* drive D8 since PTT tristates RX */
		sci0cl = clocktick.H.l + 200; /* repeat in 200 ms */
	    } else {
		PORTS = 0;		/* D8 off */
		PTTOFF();		/* PTT off (RX driven again) */
	    }
	}
}


/* check mem from p0 up to p1.
 * never return if find problem
 */
static void
memSegment (Word p0, Word p1)
{
	volatile Word *ptr;

	for (ptr = (Word*)p0; ptr < (Word*)p1; ptr++) {
	    volatile Word save = *ptr;
	    Word bit = 1;
	    int i;
	    for (i = 0; i < 16; i++) {
		*ptr = bit;
		if (*ptr != bit)
		    errFlash (-9);
		bit <<= 1;
	    }
	    *ptr = save;
	    if (*ptr != save)
		errFlash (-10);
	}
}

/* test all of memory */
static void
memCheck(void)
{
	Byte page, pagesave;

	INTR_OFF();					/* clock is in here */
	memSegment (0x0400, 0x7000);
	INTR_ON();

	pagesave = DPAGE;
	for (page = 0; page < 0x20; page++) {
	    DPAGE = page;
	    memSegment (0x7000, 0x8000);
	}
	DPAGE = pagesave;

	pagesave = PPAGE;
	for (page = 0; page < 7; page++) {		/* 7 is at c000..ffff*/
	    PPAGE = page;
	    memSegment (0x8000, 0xc000);
	}
	PPAGE = pagesave;

	memSegment (0xc000, 0xeffe - NSTACK);		/* avoid stack */
}

/* 112ms delay per n
 * N.B. look at all uses if change
 */
static void
spinDelay (int n)
{
	unsigned i;

	while (--n >= 0) {
	    resetCOP();
	    for (i = 0; i < 60000U; i++)
		continue;
	}
}

/* blink lights with value v.
 * if v is negative, blink forever, else blink 5 times then reboot.
 */
void
errFlash (int v)
{
	int n;

	INTR_OFF();
	PTTOFF();

	/* use input's LED's.. they are all in a row and easier to decipher */
	DDRJ = 0xff;

	/* find how long */
	if (v < 0) {
	    v = -v;
	    n = -1;
	} else {
	    n = 5;
	}

	/* invert for LEDs */
	v = ~v;

	/* blink v n times */
	while (n < 0 || n-- > 0) {
	    PORTJ = ~0;
	    setPORTH (~0);
	    spinDelay (5);
	    PORTJ = v;	
	    setPORTH (v);
	    spinDelay (5);
	}

	/* let COP time out when reach 0 */
	while (1)
	    continue;
}


/* LAN *******************************************************************/

/* compute check sum on the given array */
int
chkSum (Byte p[], int n)
{
	Word sum;

	for (sum = 0; n > 0; --n)
	    sum += *p++;
	return (fixChksum(sum));
}

/* return value of rpkt.data[*ip], accounting for PESC, and update *ip. */
static Byte
explodeData (int *ip)
{
	Byte b = rpkt.data[(*ip)++];

	if (b == PESC) {
	    switch (b = rpkt.data[(*ip)++]) {
	    case PESYNC: return (PSYNC);
	    case PEESC: return (PESC);
	    default: return (b); /* ? */
	    }
	}
	return (b);
}

/* rpkt contains a BOOT packet, which means it contains BootIm records.
 * at this point, we are unable to load EEPROM since we are execing from it;
 * EE is loaded by a different boot loader which accompanies fresh downloads;
 *   we jump to it (forever) when see BT_EXEC.
 * don't forget rpkt.data has been PESC expanded.
 */
static void
rpktBootrec (void)
{
	int i;

	/* scan for and process each BootIm */
	for (i = 0; i < rpkt.count; /* incremented in body */ ) { 
	    BootIm bim;
	    Byte *addr;
	    int j;

	    resetCOP();

	    /* get next BootIm */
	    bim.type = explodeData (&i);
	    bim.len = explodeData (&i);
	    bim.addrh = explodeData (&i);
	    bim.addrl = explodeData (&i);
	    addr = (Byte *)(((Word)(bim.addrh) << 8) | (bim.addrl));

	    /* can't handle EEPROM from here */
	    if (bim.type & BT_EEPROM)
		errFlash (11);

	    /* set PC to brave new world if see EXEC */
	    if (bim.type & BT_EXEC) {
		sendAck();
		while (!acksent)			/* let ACK drain */
		    continue;
		(*((void (*)())addr))();		/* never returns */
		errFlash (12);				/* can't happen */
	    }

	    /* BT_DATA just gets copied out */
	    for (j = 0; j < bim.len; j++)
		*addr++ = explodeData (&i);
	}

	/* Ack */
	sendAck();
}

/* start transmitting a packet on both ports.
 * N.B. assumes xpktptr and xpktlen are already set up
 */
static void
startXmtr (void)
{
	xpkt0nxt = 0;			/* init count of bytes sent to SC0 */
	xpkt1nxt = 0;			/* init count of bytes sent to SC0 */
	SC1CR2 = 0x6c;                  /* TCIE causes an immediate interrupt */
	PTTON();			/* turn on 485 transmitter */
	SC0CR2 = 0x6c;                  /* TCIE causes an immediate interrupt */
}

/* send an ACK for rpkt.
 * record seq in rseq[].
 */
void
sendAck(void)
{
	Byte seq = rpkt.info & PSQ_MASK;
	Byte rfr = rpkt.fr;

	/* record last seq from fr node we have ACKed */
	rseq[rfr] = seq;

	/* build xackpkt */
	xackpkt[PB_SYNC] = PSYNC;
	xackpkt[PB_TO] = rfr;
	xackpkt[PB_FR] = getBrdAddr();
	xackpkt[PB_INFO] = PT_ACK | seq;
	xackpkt[PB_COUNT] = 0;
	xackpkt[PB_HCHK] = chkSum (xackpkt, PB_NHCHK);

	/* start transmitting */
	xpktptr = xackpkt;
	xpktlen = PB_HSZ;
	acksent = 0;
	startXmtr();
}

/* return total Bytes in the given packet */
static int
pktSize (Byte pkt[])
{
	int ndata = pkt[PB_COUNT];
	return (ndata ? (PB_HSZ + 1) + ndata : PB_HSZ);
}

/* xpkt is filled, start the transmitter */
void
sendXpkt(void)
{
	xpktptr = xpkt;
	xpktlen = pktSize (xpkt);
	startXmtr();
}

Word
fixChksum (Word sum)
{
	while (sum > 255)
	    sum = (sum & 0xff) + (sum >> 8);
	if (sum == PSYNC) 
	    sum = 1;
	return (sum);
}

/* received a good packet, but not yet known whether ours.
 * if ours, handle ACK here, else defer to background dispatch.
 * N.B. this occurs at interrupt level.
 */
static void
rxChkDispatch ()
{
	/* just return if not ours; and update some stats */
	uiInc (&n_rg);			/* good packet */
	if (!(rpkt.to == BRDCA || rpkt.to == getBrdAddr()))
	    return;			/* not ours */
	uiInc (&n_ru);			/* ours */

	if ((rpkt.info & PT_MASK) == PT_ACK) {
	    /* ACKs are "dispatched" right here */
	    if (rpkt.fr == xpkt[PB_TO] &&
	    		((rpkt.info & PSQ_MASK) == (xpkt[PB_INFO] & PSQ_MASK)))
		xacked = 1;		/* proclaim xpkt has been acked */
	    rpktlen = 0;		/* done with rpkt in any case */
	} else {
	    rpktrdy = 1;		/* ready for background dispatch */
	}
}

/* give up the token */
void
giveUpToken(void)
{
	ourtoken = 0;
	xpktptr = xtokpkt;
	xtokpkt[0] = PSYNC;
	xtokpkt[1] = BROKTOK;
	xpktlen = 2;
	startXmtr();
}

/* just received a token, t.
 * give right back if it's ours and don't want it.
 */
static void
rxToken (Byte t)
{
	if (tok2addr(t) == getBrdAddr()) {
	    if (wanttoken)
		ourtoken = 1;
	    else
		giveUpToken();
	}
}

/* call with a new incoming byte, add to rpkt.
 * when get a complete packet for us mark it for dispatch.
 */
static void
rxChar (Byte d)
{
	/* preserve rpkt until dispatched */
	if (rpktrdy)
	    return;

	if (d == PSYNC) {
	    /* saw SYNC -- always start over */
	    rpkt.sync = PSYNC;
	    rchksum = PSYNC;
	    rpktlen = 1;
	} else {
	    /* handle bytes subsequent to known-good SYNC */
	    switch (rpktlen) {
	    case 1:				/* To */
		if (ISNTOK(d)) {
		    rxToken (d);		/* saw a token packet */
		    rpktlen = 0;		/* start over */
		} else {
		    rpkt.to = d;		/* continue with normal pkt */
		    rchksum += d;
		    rpktlen = 2;
		}
		break;
	    case 2:				/* From */
		uiInc (&n_rx);			/* here so don't count tokens */
		rpkt.fr = d;
		rchksum += d;
		rpktlen = 3;
		break;
	    case 3:				/* Info */
		rpkt.info = d;
		rchksum += d;
		rpktlen = 4;
		break;
	    case 4:				/* Data Count */
		if (d > PMXDAT)
		    rpktlen = 0;		/* must be illegal */
		else {
		    rpkt.count = d;
		    rchksum += d;
		    rpktlen = 5;
		}
		break;
	    case 5:				/* Header Checksum */
		rpkt.hchk = d;			/* proposed header checksum */
		if (fixChksum (rchksum) == d) {	/* if header checksum ok */
		    if (rpkt.count == 0) {	/* if control packet */
			rxChkDispatch();	/* dispatch now if ours */
			rpktlen = 0;		/* done regardless */
		    } else			/* else */
			rpktlen = 6;		/* proceed with data */
		} else
		    rpktlen = 0;		/* bad header checksum */
		break;
	    case 6:				/* Data Checksum */
		rpkt.dchk = d;			/* claimed data checksum */
		rchksum = 0;			/* restart for data */
		rpktlen = 7;			/* now collect data */
		nrpktd = 0;			/* n data bytes counter */
		break;
	    case 7:				/* Data */
		rpkt.data[nrpktd++] = d;	/* another byte of data */
		rchksum += d;			/* accumulate checksum */
		if (nrpktd >= rpkt.count) {	/* if have all data */
		    if (fixChksum (rchksum) == rpkt.dchk) /* if good data */
			rxChkDispatch();	/* dispatch if ours */
		    rpktlen = 0;		/* wait for SYNC again */
		}
		break;
	    default:
		break;
	    }
	}
}

/* interrupt handler for SCI0, the 485 lan */
#pragma interrupt_handler onSCI0
static void
onSCI0 (void)
{
	Byte s;

	/* get status */
	s = SC0SR1;

	/* receiver? */
	if (s & 0x20) {
	    Byte d = SC0DRL;		/* read data, also clears status bits */

	    /* bridge to SCI1. loop is rarely necessary :-) */
	    while (!(SC1SR1 & 0x80))	/* wait for TDRE pipeline ready */
		continue;
	    SC1DRL = d;			/* load char */
	    SC1CR2 = 0x2c;		/* no tx interrupt necessary */

	    /* process */
	    rxChar(d);

	    /* log integrity */
	    if (s & 0x0f) {
		if (s & 0x08) n_nv0++;
		if (s & 0x04) n_nf0++;
		if (s & 0x02) n_fe0++;
	    }
	}

	/* transmitter? */
	if (s & 0x40) {
	    if (gw0h != gw0t) {
		SC0DRL = gw0q[gw0t];
		gw0t = (gw0t+1)%NGW;
	    } else if (xpkt0nxt < xpktlen) {  /* if more in xpktptr[] for us */
		SC0DRL = xpktptr[xpkt0nxt++]; /*   send next and clr intrpt */
	    } else {
		PTTOFF();		/*   PTT off */
		SC0CR2 = 0x2c;		/*   clear tx interrupt */
		if (xpktptr == xackpkt) /*   if completed sending an ACK */
		    acksent = 1;	/*     so indicate */
	    }
	}

	/* hello? */
	if (!(s & 0x60))
	    n_si0++;
}

/* interrupt handler for SCI1, the 232 lan */
#pragma interrupt_handler onSCI1
static void
onSCI1 (void)
{
	Byte s;

	/* get status */
	s = SC1SR1;

	/* receiver? */
	if (s & 0x20) {
	    Byte d = SC1DRL;		/* read data, also clears status bits */

	    /* bridge to SCI0 through queue */
	    gw0q[gw0h] = d;
	    gw0h = (gw0h+1)%NGW;
	    PTTON();			/* PTT on */
	    SC0CR2 = 0x6c;		/* start xmtr */

	    /* process */
	    rxChar(d);

	    /* log integrity */
	    if (s & 0x0f) {
		if (s & 0x08) n_nv1++;
		if (s & 0x04) n_nf1++;
		if (s & 0x02) n_fe1++;
	    }
	}

	/* transmitter? */
	if (s & 0x40) {
	    if (xpkt1nxt < xpktlen) {	/* if more in xpktptr for us */
		SC1DRL = xpktptr[xpkt1nxt++];	/* send next and clr intrpt */
	    } else {
		SC1CR2 = 0x2c;		/* clear tx interrupt */
		if (xpktptr == xackpkt)	/* if completed sending an ACK */
		    acksent = 1;	/*   so indicate */
	    }
	}

	/* hello? */
	if (!(s & 0x60))
	    n_si1++;
}



/* initialization ************************************************************/

static void
stopMot(void)
{
	Long L;

	L.H.u = L.H.l = 0;

	setMotVel (&L);
}

static void
zeroBSS(void)
{
	char *ptr;

	for (ptr = &_bss_start; ptr < &_bss_end; )
	    *ptr++ = 0;
}


/* interrupts ***************************************************************/

/* clock interrupt:
 * increment clock, plus whatever
 */
#pragma interrupt_handler onRTI
static void
onRTI (void)
{
	/* clear interrupt */
	RTIFLG = 0x80;

	/* inc clock -- only takes 2uS! */
	asm ("inc _clocktick+3");
	asm ("bne _RTImisc");
	asm ("inc _clocktick+2");
	asm ("bne _RTImisc");
	asm ("inc _clocktick+1");
	asm ("bne _RTImisc");
	asm ("inc _clocktick");
	asm ("bpl _RTImisc");
	asm ("clr _clocktick");

	asm ("_RTImisc:");
	/* put here anything else wanted on each clock tick */
}

/* entry point on RESET or COP.
 * set up processor from scratch.
 */
#pragma interrupt_handler _start
static void
_start(void)
{
	/* set stack pointer just below EEPROM */
	asm ("lds #$effe");

	/* setup internal resources.
	 * 1st write to these locks them permanently.
	 * vectors at ff80..ffff
	 */
	INITRG = 0x00;	/* registers block 0..1ff */
	INITRM = 0x08;	/* on-chip 1k 800..bff */
	INITEE = 0xf1;	/* EEPROM f000..ffff */

	/* start in Normal Single Chip. switch to Normal Exp Wide.
	 * N.B. this by itself will not move EEPROM
	 */
	MODE = 0xe0;
	MODE = 0xe0;

	/* enable chip selects so that:
	 * 0: CS0:  encoder  = 200..2ff
	 * 1: CS1:  motorcon = 300..37f
	 * 2: CS2:  motormon = 380..3ff
	 * 3: CS3:  485 PTT output: 0 if listen
	 * 4: CSP0: 8000..ffff
	 * 5: CSP1: motor direction output: 1 is + direction 
	 * 6: CSD:  0000..7fff
	 * 7: not used
	 * no clock stretching.
	 */
	PORTF = 0x77;
	DDRF = 0x48;
	CSCTL0 = 0x37;
	CSCTL1 = 0x10;
	CSSTR0 = 0x00;
	CSSTR1 = 0x00;

	/* enable expanded D and P paging:
	 * PPAGE 7 is always mapped to c000..ffff.
	 * Put PPAGE 0 at 8000..b777
	 * (leaves pages 1..6=96KB of ext mem unused)
	 * DPAGE 0..1f selects one of 32 4k pages in 7000..7fff.
	 * the 7 DPAGES 10..16 also appear at 0000..6777,
	 *   although 0..03ff are covered by registers and CS0/1/2.
	 * so to avoid these, only use DPAGE set to 0..f and 17..1f
	 */
	PPAGE = 0x00;
	DPAGE = 0x00;
	WINDEF = 0xc0;
	MXAR = 0x0f;
	MISC = 0x00;

	/* enable ByteLane and R/W bus signals.
	 * ECLK used for EncReset until next Altera rev, then not used.
	 * ARSIE used for Encoder direction input.
	 */
	PEAR = 0x1c;
	DDRE = 0x10;

	/* PORTH misc outputs,
	 * PORTJ misc inputs.
	 */
	DDRH = 0xff;
	PORTH = 0x00;
	DDRJ = 0x00;

	/* more misc init */
	spinDelay(5);		/* hey, without it MCON fails its speed test*/
	zeroBSS();		/* zero globals */
	stopMot();		/* make sure no stray pulses */
	RTICTL = 0x81;		/* set clock rate to SPCT */
	INTR_ON();		/* enable clock, serial interrupts */

	/* run self-test forever if JP4 1-2 is installed */
	selfTest();

	/* start ATD system in 8-channel auto-scan mode */
	ATDCTL2 = 0xc0;
	ATDCTL5 = 0x70;

	/* setup the 485 lan on SC0 for interrupt receive */
	SC0BDH = 0x00;		/* 38.4 Kbaud */
	SC0BDL = 0x0d;		/* 38.4 Kbaud */
	SC0CR1 = 0x00;		/* 8data, 1stop, no parity */
	SC0CR2 = 0x2c;		/* RIE TE RE. TCIE only when want to send */

	/* setup the 232 local port SC1 for interrupt receive */
	SC1BDH = 0x00;		/* 38.4 Kbaud */
	SC1BDL = 0x0d;		/* 38.4 Kbaud */
	SC1CR1 = 0x00;		/* 8data, 1stop, no parity */
	SC1CR2 = 0x2c;		/* RIE TE RE. TCIE only when want to send */

	/* lock in COP */
	COPCTL = 0x07;		/* 1 second COP */

	/* this temporary main loop can only handle seeing BOOT and PING
	 * records. it breaks out if it sees anything else, the idea being
	 * that by the time any such are seen the rest of the system has been
	 * booted up.
	 */
	while (1) {
	    resetCOP();
	    if (rpktrdy) {
		switch (rpkt.info & PT_MASK) {
		case PT_BOOTREC:
		    if ((rpkt.info & PSQ_MASK) == rseq[rpkt.fr])
			sendAck();
		    else
			rpktBootrec();
		    rpktrdy = 0;
		    break;
		case PT_PING:
		    sendAck();
		    rpktrdy = 0;
		    break;
		default:
		    /* leave rpktrdy for full-up dispatch */
		    goto out;
		}
	    }
	}
    out:

	/* now ready for the real world.
	 * init malloc out to first stack just below EEPROM.
	 * then call the "real" main.
	 * sneak a counter in between to test for reboots.
	 */
	AWORD(0xeffe - NCSTACK) += 1;
	_NewHeap (&_bss_end, (void*)(0xeffe - NCSTACK - 2));
	main();

	/* let COP reset if ever get here */
	while (1)
	    continue;
}

/* keep the COP from resetting */
void
resetCOP(void)
{
	Word c;

	/* basic goosey */
	COPRST = 0x55;
	COPRST = 0xaa;

	/* record longest interval. beware of first and wrap */
	NINTR_OFF();
	c = clocktick.H.l;
	NINTR_ON();
	if (lastcop && c > lastcop) {
	    Word d = c - lastcop;
	    if (d > maxcop)
		maxcop = d;
	}
	lastcop = c;
}

#pragma interrupt_handler onKWJ
/* capture hardware position counters */
static void
onKWJ(void)
{
	Long e, m;

	/* capture each h/w counter */
	getEncPos (&e);
	getMotPos (&m);

	/* avoid using longs, and compiles much smaller too */
	((Long *)&cvg(CV_MTRIG))->H.u = m.H.u;
	((Long *)&cvg(CV_MTRIG))->H.l = m.H.l;
	((Long *)&cvg(CV_ETRIG))->H.u = e.H.u;
	((Long *)&cvg(CV_ETRIG))->H.l = e.H.l;

	/* clear only the interrupt from the cause bit */
	KWIEJ &= ~KWIFJ;
}

#pragma interrupt_handler onStray
static void
onStray(void)
{
	nstray++;
}

/* see errata for HC812AV4 mask 01H73K at
 * http://www.mcu.motsps.com/lit/errata/12err/a4_01h73k.html.
 * DESCRIPTION:
 *   When an I-type interrupt occurs at the same time as an RTI interrupt, the
 *   program address at $FFC0 will be fetched.
 * WORKAROUND:
 *   Point the $FFC0 vector to a return from interrupt (RTI) instruction.
 *   This will get the program returned to start processing the proper
 *   interrupt as quickly as possible.
 */
#pragma abs_address:0xffc0
#pragma interrupt_handler onBug
static void
onBug(void)
{
}

#pragma abs_address:0xffce
static void (*interrupt_vectors[])(void) = {
    onStray,	/* Key Wakeup H */
    onKWJ,	/* Key Wakeup J */
    onStray,	/* ATD */
    onSCI1,	/* SCI 1 */
    onSCI0,	/* SCI 0 */
    onStray,	/* SPI */
    onStray,	/* PAIE */
    onStray,	/* PAO */
    onStray,	/* TOF */
    onStray,	/* TC7 */
    onStray,	/* TC6 */
    onStray,	/* TC5 */
    onStray,	/* TC4 */
    onStray,	/* TC3 */
    onStray,	/* TC2 */
    onStray,	/* TC1 */
    onStray,	/* TC0 */
    onRTI,	/* RTI */
    onStray,	/* IRQ */
    onStray,	/* XIRQ */
    onStray,	/* SWI */
    onStray,	/* ILLOP */
    _start,	/* COP */
    onStray,	/* CLM */
    _start	/* RESET */
};
#pragma end_abs_address

#else

/* stubs */
void errFlash (int g, int n) { exit (n); }
void getEncPos (Long *ep) { ep->H.u = 0; ep->H.l = 123; };
void getEncStatus (int *sp) { *sp = 0; }
void setEncZero (void) { }
void getMotPos (Long *mp) {mp->H.u = 0; mp->H.l = 234; }
void getMotVel (Long *mp) {mp->H.u = 0; mp->H.l = 345; }
void setMotVel (Long *vp) { }
void setMotDir (int dir) { }
void setMotZero (void) { }
int getMotDir (void) { return (1); }
int getBrdAddr (void) { return(2); }

#endif /* HOSTTEST */

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: eeprom.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
