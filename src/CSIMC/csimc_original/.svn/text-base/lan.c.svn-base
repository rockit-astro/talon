/* code to handle the lan channel */

#include "sa.h"

/* lan performance statistics */
static unsigned n_tx;       /* total packets originated  */
static unsigned n_ta;       /* packets acked on first try */
static unsigned n_tm;       /* packets acked after several tries */
static unsigned n_tf;       /* packets failed after all retries */
unsigned n_ri;          /* ours, checked ok, but ill-formed */
unsigned n_rd;          /* number of duplicates, based on sequence */

static Byte xseq[NADDR];    /* sequence for next tx packet, per addr */
static Byte xpktbsy;        /* set when xpkt is being filled or sent */

static int sendSout(void);

/* print lan stats since last time */
void
lanStats(void)
{
    printf ("  RS232:");
    printf ("%6u sent ", n_x1);
    n_x1 = 0;
    printf ("%6u rcvd ", n_r1);
    n_r1 = 0;
    printf ("%6u noisy", n_nf1);
    printf ("%6u frame", n_fe1);
    printf ("%6u ovrrn", n_nv1);
    printf ("%6u stray\n", n_si1);
    printf ("  RS485:");
    printf ("%6u sent ", n_x0);
    n_x0 = 0;
    printf ("%6u rcvd ", n_r0);
    n_r0 = 0;
    printf ("%6u noisy", n_nf0);
    printf ("%6u frame", n_fe0);
    printf ("%6u ovrrn", n_nv0);
    printf ("%6u stray\n", n_si0);
    printf ("Tx Pkts:");
    printf ("%6u total", n_tx);
    n_tx  = 0;
    printf ("%6u 1stry", n_ta);
    n_ta  = 0;
    printf ("%6u retry", n_tm);
    n_tm  = 0;
    printf ("%6u fail \n", n_tf);
    n_tf  = 0;
    printf ("Rx Pkts:");
    printf ("%6u total", n_rx);
    n_rx  = 0;
    printf ("%6u good ", n_rg);
    n_rg  = 0;
    printf ("%6u mine ", n_ru);
    n_ru  = 0;
    printf ("%6u dups ", n_rd);
    n_rd  = 0;
    printf ("%6u rogue\n", n_ri);
    n_ri  = 0;
}

/* send fatal msg to the logging node, with some extra factoids, then set WEDIE.
 * if WEDIE is already set, just return to avoid recursion of things are bad.
 * N.B. don't use printf since it is disabled once WEDIE is set.
 * N.B. this is only for threads. To just send a log note, use sendLogMsg().
 * N.B. take care msg[] is not very long.
 */
void
fatalError (char *msg)
{
    char buf[64];
    int n;

    /* seems like a prudent time to check */
    checkStack(2);

    /* mark us dead. avoid recursion if already being handled */
    if (pti.flags & TF_WEDIE)
        return;
    pti.flags |= TF_WEDIE;

    /* format some info with msg and log it */
    /* TODO: save something in EEPROM?? */
    n = &pti.mcs[NCSTACK] - pti.mincs;
    sprintf (buf, "Peer=%2d NStk=%-4d: %s\n", pti.peer, n, msg);
    xpktbsy = 0;    /* insist */
    sendLogMsg (buf);
}

#ifndef HOSTTEST

/* get one char from pti.sin, or wait if empty */
static int
get1Char(void)
{
    /* wait for char, or something weird */
    while (1)
    {
        if (pti.flags & (TF_INTERRUPT|TF_WEDIE))
            return (-1);
        if (CQisEmpty(&pti.sin))
            scheduler(0);
        else
            break;
    }

    return (CQget(&pti.sin));
}

/* put a character on the output queue.
 * flush to pti.to if q fills or sending \n.
 * also encode PESC sequences.
 * return c else -1 if trouble.
 * N.B. disabled if WEDIE is set
 */
int
putchar(char c)
{
    /* mute if dieing */
    if (pti.flags & TF_WEDIE)
        return (-1);

    /* this allows for very slow printf() */
    scheduler(1);

    /* put c on stdout queue, expanding if a special char */
    switch (c)
    {
        case PSYNC:
            if (CQnfree(&pti.sout) < 2 && sendSout() < 0)
                return (-1);
            CQput (&pti.sout, PESC);
            CQput (&pti.sout, PESYNC);
            break;
        case PESC:
            if (CQnfree(&pti.sout) < 2 && sendSout() < 0)
                return (-1);
            CQput (&pti.sout, PESC);
            CQput (&pti.sout, PEESC);
            break;
        default:
            if (CQisFull(&pti.sout) && sendSout() < 0)
                return (-1);
            CQput (&pti.sout, c);
            break;
    }

    /* send if full or just added nl */
    if ((CQisFull(&pti.sout) || c == '\n') && sendSout() < 0)
        return (-1);

    return (c);
}

#else

#include <sys/time.h>
#include <sys/types.h>

int
get1Char (void)
{
    static int before;
    struct timeval tv;
    fd_set rfd;

    if (!before)
    {
        setbuf (stdin, NULL);
        before = 1;
    }

    /* prompt if nothing pending */
    FD_ZERO(&rfd);
    FD_SET(0, &rfd);
    tv.tv_sec = 0;
    tv.tv_usec = 10000;
    if (select(1,&rfd,NULL,NULL,&tv) == 0)
        printf ("> ");

    return (getchar());
}

#endif /* HOSTTEST */

/* return next byte from input q of current thread, or -1 if interrupt.
 * if nothing there now block.
 * also decode PESC sequences.
 */
int
getChar(void)
{
    int c = get1Char();

    if (c == PESC)
        c = get1Char() == PESYNC ? PSYNC : PESC;

    return (c);
}

/* force a flush of pti.sout.
 * return 0 if ok, else -1
 */
int
oflush(void)
{
    return (sendSout());
}

/* wait until xpkt is free for this thread to start filling, then lock it
 */
static void
wait4xpkt(void)
{
    while (xpktbsy)
        scheduler(0);
    xpktbsy = 1;    /* no race, since only used in background */
}

/* wait for us to have the token */
static void
wait4Token (void)
{
    wanttoken = 1;
    while (!ourtoken)
        scheduler(0);
}

/* send xpkt, wait up to ACKWT for it to be acked.
 * return 0 if ok else -1.
 */
static int
sendWait4ACK(void)
{
    long tout;

    xacked = 0;
    sendXpkt();

    tout = getClock() + ACKWT;
    do
    {
        scheduler(0);
        if (xacked)
        {
            xacked = 0;
            return (0);
        }
    }
    while (getClock() < tout);

    return (-1);
}

/* xpkt is all set: wait for token, send, wait for ack; retry as necessary.
 * return -1 if killed or fail, without the token or xpkt. assume we lost the
 *   token because host may have sent ack but we missed it and given token away.
 * return 0 if ok but we will still have the token.
 * N.B. this is not for sending ACKs, for that use sendAck().
 */
static int
sendXpktW (void)
{
    int try;

    for (try = 0; try < MAXRTY; try++)
                {
                    if (pti.flags & TF_WEDIE)
                    {
                        xpktbsy = 0;
                        return (-1);    /* don't keep trying if we were killed */
                    }
                    wait4Token();
                    if (sendWait4ACK() == 0)
                        break;
                    giveUpToken();  /* only if retrying */
                }
    uiInc (&n_tx);      /* don't count if failed cause we were killed */
    wanttoken = 0;
    xpktbsy = 0;

    if (try == MAXRTY)
        {
            n_tf++;
            return (-1);
        }
    else
    {
        if (try == 0)
                uiInc (&n_ta);
        else
            uiInc (&n_tm);
        return (0);
    }
}

/* copy n bytes from src to dst, expanding any PSYNC to PESC+PESYNC and any
 * PESC to PESC+PEESC.
 * return length of dst.
 */
static int
cpyESC (Byte *dst, Byte *src, int n)
{
    Byte *dst0;

    for (dst0 = dst; --n >= 0; src++)
        switch (*src)
        {
            case PSYNC:
                *dst++ = PESC;
                *dst++ = PESYNC;
                break;
            case PESC:
                *dst++ = PESC;
                *dst++ = PEESC;
                break;
            default:
                *dst++ = *src;
                break;
        }

    return (dst - dst0);
}

/* fill dst with n bytes using as many as necessary from src collapsing any
 * PESC found.
 * return count of bytes used from src.
 */
int
cpyunESC (Byte *dst, Byte *src, int n)
{
    Byte *src0 = src;

    while (--n >= 0)
    {
        Byte s = *src++;
        if (s == PESC)
            s = *src++ == PESYNC ? PSYNC : PESC;
        *dst++ = s;
    }

    return (src - src0);
}


/* return the next sequence to use in PB_INFO */
static Byte
nxtSeq(void)
{
    return (((++xseq[pti.peer])<<PSQ_SHIFT)&PSQ_MASK);
}

/* build and send one SHELL packet worth of pti.sout.
 * PESC already expanded.
 * don't return until acked.
 * return 0 if ok else -1
 */
static int
sendSHELL(void)
{
    Byte brdaddr = getBrdAddr();
    Word sum;
    int ndata;

    /* wait our turn to use xpkt */
    wait4xpkt();

    /* build packet from pti.sout */
    xpkt[PB_SYNC] = PSYNC;
    xpkt[PB_TO] = pti.peer;
    xpkt[PB_FR] = brdaddr;
    xpkt[PB_INFO] = PT_SHELL | nxtSeq();
    for (sum = ndata = 0; ndata < PMXDAT && !CQisEmpty(&pti.sout); ndata++)
        sum += xpkt[PB_DATA + ndata] = CQget(&pti.sout);
    xpkt[PB_COUNT] = ndata;
    xpkt[PB_HCHK] = chkSum (xpkt, PB_NHCHK);
    if (ndata > 0)
        xpkt[PB_DCHK] = fixChksum (sum);

    /* go */
    if (sendXpktW() < 0)
    {
        fatalError ("SendSHELL");
        return (-1);
    }
    giveUpToken();
    return (0);
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
    xacklen = PB_HSZ;

    /* start transmitting */
    resendAck();
}

/* send ACK for rpkt knowing it is of type PB_GETVAR
 * record seq in rseq[].
 */
static void
sendGETAck(VType *vp)
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
    xackpkt[PB_COUNT] = cpyESC(&xackpkt[PB_DATA], (Byte*)vp, sizeof(VType));
    xackpkt[PB_HCHK] = chkSum (xackpkt, PB_NHCHK);
    xackpkt[PB_DCHK] = chkSum (&xackpkt[PB_DATA], xackpkt[PB_COUNT]);
    xacklen = PB_NZHSZ+xackpkt[PB_COUNT];

    /* start transmitting */
    resendAck();
}

/* grab token, build and send one GETVAR packet.
 * don't return until acked and have value back ok.
 * just die if fail
 */
void
sendGETVAR (int to, VRef r, VType *vp)
{
    Byte brdaddr = getBrdAddr();

    /* wait our turn to use xpkt */
    wait4xpkt();

    /* build packet from pti.sout */
    xpkt[PB_SYNC] = PSYNC;
    xpkt[PB_TO] = to;
    xpkt[PB_FR] = brdaddr;
    xpkt[PB_INFO] = PT_GETVAR | nxtSeq();
    xpkt[PB_COUNT] = cpyESC(&xpkt[PB_DATA], (Byte*)&r, sizeof(VRef));
    xpkt[PB_HCHK] = chkSum (xpkt, PB_NHCHK);
    xpkt[PB_DCHK] = chkSum (&xpkt[PB_DATA], xpkt[PB_COUNT]);

    /* go */
    if (sendXpktW() < 0)
        fatalError ("SendGETVAR");
    else
    {
        (void) cpyunESC ((Byte*)vp, rpkt.data, sizeof(VType));
        giveUpToken();
    }
}

/* grab token, build and send one SETVAR packet.
 * don't return until acked.
 * just die if fail.
 */
void
sendSETVAR (int to, VRef r, VType v)
{
    Byte brdaddr = getBrdAddr();
    int n;

    /* wait our turn to use xpkt */
    wait4xpkt();

    /* build packet from pti.sout */
    xpkt[PB_SYNC] = PSYNC;
    xpkt[PB_TO] = to;
    xpkt[PB_FR] = brdaddr;
    xpkt[PB_INFO] = PT_SETVAR | nxtSeq();
    n = cpyESC(&xpkt[PB_DATA], (Byte*)&r, sizeof(VRef));
    xpkt[PB_COUNT] = n + cpyESC(&xpkt[n+PB_DATA], (Byte*)&v, sizeof(VType));
    xpkt[PB_HCHK] = chkSum (xpkt, PB_NHCHK);
    xpkt[PB_DCHK] = chkSum (&xpkt[PB_DATA], xpkt[PB_COUNT]);

    /* go */
    if (sendXpktW() < 0)
        fatalError ("SendSETVAR");
    else
        giveUpToken();
}

/* build and send a SERDATA packet from pti.sin.
 * just die if fail
 */
void
sendSERDATA(void)
{
    Byte brdaddr = getBrdAddr();
    Word sum;
    int ndata;

    /* wait our turn to use xpkt */
    wait4xpkt();

    /* build packet from rs232rxq */
    xpkt[PB_SYNC] = PSYNC;
    xpkt[PB_TO] = rs232peer;
    xpkt[PB_FR] = brdaddr;
    xpkt[PB_INFO] = PT_SERDATA | nxtSeq();
    for (sum=ndata=0; ndata < (PMXDAT-1) && !CQisEmpty(&pti.sin); ndata++)
    {
        Byte buf[2], q = CQget(&pti.sin);
        int nbuf = cpyESC (buf, &q, 1);
        sum += xpkt[PB_DATA + ndata] = buf[0];
        if (nbuf > 1)
            sum += xpkt[PB_DATA + ++ndata] = buf[1];
    }
    xpkt[PB_COUNT] = ndata;
    xpkt[PB_HCHK] = chkSum (xpkt, PB_NHCHK);
    xpkt[PB_DCHK] = fixChksum (sum);

    /* go */
    if (sendXpktW() < 0)
        fatalError ("SendSERDATA");
    else
        giveUpToken();
}

/* send a buffer to LOGADR.
 * don't return until acked.
 * reboot if fail (!)
 */
void
sendLogMsg (char *buf)
{
    /* wait our turn to use xpkt */
    wait4xpkt();

    /* build packet from pti.sout */
    xpkt[PB_SYNC] = PSYNC;
    xpkt[PB_TO] = LOGADR;
    xpkt[PB_FR] = getBrdAddr();
    xpkt[PB_INFO] = PT_SHELL | nxtSeq();
    xpkt[PB_COUNT] = cpyESC(&xpkt[PB_DATA], (Byte*)buf, strlen(buf));
    xpkt[PB_HCHK] = chkSum (xpkt, PB_NHCHK);
    xpkt[PB_DCHK] = chkSum (&xpkt[PB_DATA], xpkt[PB_COUNT]);

    /* go */
    if (sendXpktW() < 0)
    {
        /* can't complain.. might as well reboot */
        rebootViaCOP();
    }

    /* ok */
    giveUpToken();
}

/* send all of pti.sout.
 * PESC already expanded.
 * don't return until complete.
 * return 0 if ok else -1
 */
static int
sendSout(void)
{
    while (CQn(&pti.sout))
        if (sendSHELL() < 0)
            return (-1);
    return (0);
}

/* SHELL packet rpkt is data for pti[rpkt.fr].sin.
 * allocate a new thread if none running now for rpkt.fr.
 * if not enough room for whole packet, too bad; make sender try again.
 *   as rude as this sounds, _any_ buffer space we supply could be overrun.
 *   plus, there is no mechanism by which to use only a portion of rpkt.
 * return 0 if ok, else -1.
 */
int
dispatchSHELL(void)
{
    int savthr = setThread(rpkt.fr);
    int ret;

    if (savthr < 0)
        return (-1);

    if (CQnfree(&pti.sin) >= rpkt.count)
    {
        CQputa (&pti.sin, rpkt.data, rpkt.count);
        ret = 0;
    }
    else
    {
        ret = -1;
    }

    setPTI (savthr);
    return (ret);
}

/* rpkt is PT_SETVAR with ref and value in Data.
 * extract and set the given variable.
 */
void
dispatchSETVAR(void)
{
    int n;
    VRef r;
    VType v;

    n = cpyunESC ((Byte*)&r, rpkt.data, sizeof(VRef));
    (void) cpyunESC ((Byte*)&v, rpkt.data+n, sizeof(VType));
    vsv (r, v);
}

/* rpkt is PT_GETVAR ref in Data.
 * send our value back in ACK.
 */
void
dispatchGETVAR(void)
{
    VRef r;
    VType v;

    /* get the ref then the value */
    (void) cpyunESC ((Byte*)&r, rpkt.data, sizeof(VRef));
    v = vgv (r);

    /* send back as special ACK */
    sendGETAck (&v);
}

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: lan.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
