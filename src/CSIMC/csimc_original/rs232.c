/* this code supports the rs232 connection as a general purpose port.
 */

#include "sa.h"

/* traffic flow is managed in its own thread, marked with TF_RS232.
 */
Byte rs232peer;         /* host addr that wants chars, 0 if none */
Byte rs232pti;          /* thread assigned to handle traffic */

#define FLUSH232    128 /* delay to let chars accumulate, ms, ^2 */

/* set up RS232 comm for host at rpkt.fr at the given baud rate
 * and assign a new thread.
 * N.B. this just does the work, it does not check if it is appropriate.
 */
static void
newRS232thread (Word baud)
{
    int savthr = setThread(rpkt.fr);

    /* set up UART */
    baud = (Word)(500000./baud+.5); /* buad to hc12 setting */
    SC1BDH = baud >> 8;
    SC1BDL = baud;
    SC1CR1 = 0x00;      /* 8data, 1stop, no parity */
    SC1CR2 = 0x2c;      /* RIE TE RE. TCIE only when want to send */

    /* init state */
    pti.flags |= TF_RS232;
    rs232peer = rpkt.fr;
    rs232pti = getPTI();

    /* ok */
    setPTI (savthr);
}

/* check for any traffic that has come in from the 232 line and send to
 * host if so.
 */
void
runRS232(void)
{
    static Byte rs232tick;
    Byte b0 = clocktick.B.b0&FLUSH232;  /* fast atomic LSB */

    /* allow some chars to accumulate to avoid flood of tiny packets.
     * TODO: this pace will not drain sin fast enough if over 9600 baud
     */
    if (CQisEmpty(&pti.sin) || b0 == rs232tick)
        return;
    rs232tick = b0;

    sendSERDATA();
    if (pti.flags & TF_WEDIE)
        endRS232();
}

/* the rs232 thread has been killed. do any cleanup.
 */
void
endRS232(void)
{
    rs232peer = 0;
}

/* rpkt is PT_SERSETUP. desired baud rate is in Data.
 * create new thread but enforce can only have one.
 */
int
dispatchSERSETUP(void)
{
    Word baud;

    /* allow only one and not if we are gateway */
    if (weRgateway || rs232peer)
        return (-1);

    (void) cpyunESC ((Byte*)&baud, rpkt.data, sizeof(baud));
    newRS232thread (baud);
    return(0);
}

/* rpkt is PT_SERDATA. data is for outbound 232.
 */
int
dispatchSERDATA(void)
{
    int savthr;
    int i;

    /* must be set up, no intruders and no way if we are the gateway */
    if (!rs232peer || rs232peer != rpkt.fr || weRgateway)
        return (-1);

    /* switch to 232 thread */
    savthr = setThread(rpkt.fr);

    /* copy out of rpkt into sout. tough luck if fills */
    for (i = 0; i < rpkt.count; /* incremented in body */ )
    {
        Byte b = explodeData (rpkt.data, &i);
        if (CQisFull(&pti.sout))
            break;
        CQput (&pti.sout, b);
    }

    /* prime xmtr if idle */
    if (SC1SR1 & 0x40)      /* if xmtr idle */
        SC1CR2 = 0x6c;      /* TIE causes an immediate interrupt */

    /* done */
    setPTI (savthr);
    return (0);
}

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: rs232.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
