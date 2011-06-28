/* this code supports the interrupts, mostly LAN traffic.
 */

#include "sa.h"

Long clocktick;			/* incremented each SPCT second */
long upticks;			/* ticks since boot */

/* stats */
unsigned n_r0, n_r1;		/* total bytes received from each */
unsigned n_x0, n_x1;		/* total bytes send from each */
unsigned n_rx;                  /* total packets seen */
unsigned n_ru;                  /* total packets seen for us, sans ACKs */
unsigned n_rg;                  /* packets seen and properly checksummed */
unsigned n_si0, n_si1;		/* stray comm interrupts */
unsigned n_nv0, n_nv1;		/* overrun errors */
unsigned n_nf0, n_nf1;		/* noisy bytes */ 
unsigned n_fe0, n_fe1;		/* framing errors */
unsigned maxcop;		/* largest COP interval seen */
static Word lastcop;		/* last clocktick.H.l, for measuring COP */

/* comm flags and buffers */
Byte ourtoken;			/* set when we have the token */
Byte wanttoken;			/* set when we want the token */
Word nstray;			/* count stray interrupts */
Byte weRgateway;		/* our 232 is the host gateway */

/* transmit packet and related info */
Byte xpkt[PMXLEN];		/* tx packet, for all but ACKs */
Byte xacked;			/* set when see proper ACK come in for xpkt */
Byte xpktlen;			/* total bytes to be sent in xpktptr */
Byte xackpkt[PB_NZHSZ+GETACKSZ];/* dedicated ACK transmission buffer */
Byte xacklen;			/* length of xackpkt */
Byte acksent;			/* flag if care to wait for ACK sent */
static Byte xpkt0nxt;		/* index of next xpktptr Byte to send to SC0 */
static Byte xpkt1nxt;		/* index of next xpktptr Byte to send to SC1 */
static Byte *xpktptr;		/* either xpkt or xackpkt */
static Byte xtokpkt[2];		/* dedicated BROKTOK transmission buffer */

/* receive packet and related info */
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

/* scheme to nest interrupt protection.
 * N.B. this allows nesting, but they must still be balanced.
 * N.B. use NINTR_PUSH at the start and NINTR_POP before returning from any
 *   interrupt routines which also use the ON/OFF pairings.
 */
static Word intrnest;
void NINTR_OFF(void) { if (!intrnest++) INTR_OFF(); }
void NINTR_ON(void)  { if (--intrnest == 0) INTR_ON(); }
void NINTR_PUSH(void) { intrnest += 1; }
void NINTR_POP(void) { intrnest -= 1; }

/* increment an unsigned int but don't let it overflow */
void
uiInc (unsigned *p)
{
	if (*p < UINT_MAX)
	    (*p) += 1;
}

/* compute check sum on the given array */
int
chkSum (Byte p[], int n)
{
	Word sum;

	for (sum = 0; n > 0; --n)
	    sum += *p++;
	return (fixChksum(sum));
}

/* start transmitting a packet. always on 485, 232 if we are the gateway.
 * N.B. assumes xpktptr and xpktlen are already set up
 */
static void
startXmtr (void)
{
	if (weRgateway) {	/* 232 only if we are gateway */
	    xpkt1nxt = 0;	/* init count of bytes sent to SC1 */
	    SC1CR2 = 0x6c;	/* TCIE causes an immediate interrupt */
	}
	xpkt0nxt = 0;		/* init count of bytes sent to SC0 */
	PTTON();		/* turn on 485 transmitter */
	SC0CR2 = 0x48;		/* TCIE causes an immediate intr; rcvr off */
}

/* resend xackpkt */
void
resendAck()
{
	xpktptr = xackpkt;
	xpktlen = xacklen;
	acksent = 0;
	startXmtr();
}

/* give up the token */
void
giveUpToken(void)
{
	ourtoken = 0;
	wanttoken = 0;
	xtokpkt[0] = PSYNC;
	xtokpkt[1] = BROKTOK;
	xpktptr = xtokpkt;
	xpktlen = 2;
	startXmtr();
}

/* return total Bytes in the given packet */
static int
pktSize (Byte pkt[])
{
	int ndata = pkt[PB_COUNT];
	return (ndata ? PB_NZHSZ + ndata : PB_HSZ);
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
 * are is true if packet came from 232, false if from 485.
 * if ours, handle ACK and REBOOT here, others defer to background dispatch.
 * N.B. this occurs at interrupt level.
 */
static void
rxChkDispatch (Byte fr232)
{
	/* just return if not ours; and update some stats */
	uiInc (&n_rg);			/* good packet */
	weRgateway |= fr232;		/* latch when get pkt from 232 */
	if (!(rpkt.to == BRDCA || rpkt.to == getBrdAddr()))
	    return;			/* not ours */

	if ((rpkt.info & PT_MASK) == PT_ACK) {
	    /* ACKs are "dispatched" right here */
	    if (rpkt.fr == xpkt[PB_TO] &&
		    ((rpkt.info & PSQ_MASK) == (xpkt[PB_INFO] & PSQ_MASK)))
		xacked = 1;		/* proclaim xpkt has been acked */
	    rpktlen = 0;		/* done with rpkt in any case */
	} else if ((rpkt.info & PT_MASK) == PT_REBOOT) {
	    /* a reboot is likely of grave concern so do it right here */
	    INTR_ON();			/* allow LAN bridge to drain */
	    rebootViaCOP();
	} else {
	    rpktrdy = 1;		/* ready for background dispatch */
	    uiInc (&n_ru);		/* our non-ACKs */
	}
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
 * also a flag to say whether from 485 or 232.
 * when get a complete packet for us mark it for dispatch.
 */
static void
rxChar (Byte d, Byte fr232)
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
	    case 1:				/* To or token */
		if (d == BROKTOK) {
		    rpktlen = 0;		/* saw BROKTOK.. never ours */
		} else if (ISNTOK(d)) {
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
			rxChkDispatch(fr232);	/* dispatch now if ours */
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
			rxChkDispatch(fr232);	/* dispatch if ours */
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

	    /* shadow to 232 if we are the gateway */
	    if (weRgateway) {
		/* loop is rarely necessary :-) */
		while (!(SC1SR1 & 0x80))/* wait for TDRE pipeline ready */
		    continue;
		SC1DRL = d;		/* load char */
		SC1CR2 = 0x2c;		/* no tx interrupt necessary */
	    }

	    /* process */
	    rxChar(d, 0);
	    uiInc (&n_r0);

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
		SC0DRL = gw0q[gw0t];	/* send next, at tail */
		gw0t = (gw0t+1)%NGW;	/* remove from tail */
	    } else if (xpkt0nxt < xpktlen) {  /* if more in xpktptr[] for us */
		SC0DRL = xpktptr[xpkt0nxt++]; /*   send next and clr intrpt */
	    } else {
		PTTOFF();		/* done.. PTT off */
		SC0CR2 = 0x2c;		/* clear tx interrupt, enable rcvr */
		if (xpktptr == xackpkt) /* if completed sending an ACK */
		    acksent = 1;	/* so indicate */
	    }
	    uiInc (&n_x0);
	}

	/* hello? */
	if (!(s & 0x60))
	    n_si0++;
}

/* interrupt handler for SCI1, the 232 tty port */
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

	    if (rs232peer) {
		/* put on rs232pti incoming queue -- actually sent from bkgd */
		int savthr = getPTI();
		setPTI (rs232pti);
		NINTR_PUSH();
		if (!CQisFull(&pti.sin))
		    CQput (&pti.sin, d);
		NINTR_POP();
		setPTI (savthr);
	    } else if (!NO232GW()) {
		/* bridge to SCI0 through queue if we are the gateway */
		if (weRgateway) {
		    gw0q[gw0h] = d;		/* add d to head of q */
		    gw0h = (gw0h+1)%NGW;	/* increment head */
		    PTTON();			/* PTT on */
		    SC0CR2 = 0x48;		/* start xmtr, rcvr off */
		}

		/* process anyway to bootstrap gateway-ness */
		rxChar(d, 1);
	    }
	    uiInc (&n_r1);

	    /* log integrity */
	    if (s & 0x0f) {
		if (s & 0x08) n_nv1++;
		if (s & 0x04) n_nf1++;
		if (s & 0x02) n_fe1++;
	    }
	}

	/* transmitter? */
	if (s & 0x40) {
	    if (rs232peer) {
		/* send another char from rs232 pti outbound q else done */
		/* FYI: this `if' path takes 50us, so max baud is 20k */
		int savthr = getPTI();
		setPTI (rs232pti);
		NINTR_PUSH();
		if (!CQisEmpty(&pti.sout)) {
		    SC1DRL = CQget (&pti.sout);	/* send next and clr intr */
		    uiInc (&n_x1);		/* counter */
		} else {
		    SC1CR2 = 0x2c;		/* clear tx intr */
		}
		NINTR_POP();
		setPTI (savthr);
	    } else {
		/* send next 232 char if any */
		if (xpkt1nxt < xpktlen) {	/* if more in xpktptr for us */
		    SC1DRL = xpktptr[xpkt1nxt++]; /* send next and clr intr */
		    uiInc (&n_x1);		/* counter */
		} else {
		    SC1CR2 = 0x2c;		/* clear tx intr */
		    if (xpktptr == xackpkt)	/* if finished sending an ACK */
			acksent = 1;		/*   so indicate */
		}
	    }
	}

	/* hello? */
	if (!(s & 0x60))
	    n_si1++;
}

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

/* come here when want to restart */
#pragma interrupt_handler restart
static void
restart(void)
{
	/* let COP reset if ever get here */
	rebootViaCOP();
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

#pragma interrupt_handler onILLOP
static void
onILLOP(void)
{
	errFlash (12);	/* same as in boot.c */
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
    onILLOP,	/* ILLOP */
    restart,	/* COP */
    onStray,	/* CLM */
    restart	/* RESET */
};
#pragma end_abs_address

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: intr.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
