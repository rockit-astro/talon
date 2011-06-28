/* code to handle the local rs232 connection */

#include "sa.h"

#define LLEN    80

static char *linebuf;
static char *lineend;
static char *lp;
static char *nexttoecho;

static void lprx(char c);
static void lptx(void);

static void
initLP(void)
{
    SC1BDH = 0x00;
    SC1BDL = 0x09;          /* 56KB */
    SC1CR1 = 0x00;          /* 8data, 1stop, no parity */
    SC1CR2 = 0x2c;          /* RIE TE RE */
    nexttoecho = lp = linebuf = malloc (LLEN);
    lineend = &linebuf[LLEN-2]; /* leave room for \r\n */
    if (!lp)
        errFlash(9);
}

void
LPpr2 (char *msg, unsigned p1, unsigned p2)
{
    char buf[32];
    char c;

    while (c = *msg++)
        putcharLP (c);
    sprintf (buf, " %5u=%04x %5u=%04x\n", p1, p1, p2, p2);
    msg = buf;
    while (c = *msg++)
        putcharLP (c);
}

void
LPpr2f (char *msg, float f1, float f2)
{
    char buf[32];
    char c;

    while (c = *msg++)
        putcharLP (c);
    sprintf (buf, " %f %f\n", f1, f2);
    msg = buf;
    while (c = *msg++)
        putcharLP (c);
}

/* print one char on local port */
void
putcharLP (char c)
{
    if (!lp)
        initLP();

    if (c == '\n')
        putcharLP ('\r');
    while ((SC1SR1 & 0x80) == 0)
        continue;
    SC1DRL = c;
}

#pragma interrupt_handler onSCI1
void
onSCI1 (void)
{
    Byte s = SC1SR1;

    if (s & 0x80)
        lptx();

    if (s & 0x3f)
    {
        char c = SC1DRL;
        if (s & 0x20)
            lprx(c);
    }
}

/* transmitter-ready from local port */
static void
lptx(void)
{
    if (nexttoecho < lp)
        SC1DRL = *nexttoecho++;
    else
        SC1CR2 = 0x2c;      /* clear TIE */
}

static void
echo(void)
{
    SC1CR2 = 0xac;          /* set TIE */
}

/* receiver-ready from local port */
static void
lprx(char c)
{
    int thr;
    int n;

    switch (c)
    {
        case '\n': /* FALLTHRU */
        case '\r': /* end of line */
            *lp++ = '\r';
            *lp++ = '\n';
            echo();
            thr = getPTI();
            setThread (getBrdAddr());
            n = lp - linebuf;
            if (CQnfree(&pti.sin) >= n)
                CQputa (&pti.sin, (Byte *) linebuf, n);
            else
            {
                char *sp;
                for (sp = linebuf; sp < lp && !CQisFull(&pti.sin); sp++)
                    CQput (&pti.sin, *sp);
            }
            setPTI(thr);
            nexttoecho = lp = linebuf;
            break;

        case CSIMCD_INTR:
            intrThread (getBrdAddr());
            putcharLP ('^');
            putcharLP ('C');
            putcharLP ('\n');
            nexttoecho = lp = linebuf;
            break;

        case '\t':
            if (lp < lineend)
            {
                do
                {
                    *lp++ = ' ';
                }
                while (lp < lineend && ((lp - linebuf)%8) != 1);
            }
            echo();
            break;

        case '\b':  /* FALLTHRU */
        case 0177:
            if (lp > linebuf)
            {
                putcharLP ('\b');
                putcharLP (' ');
                putcharLP ('\b');
                nexttoecho = --lp;
            }
            else
                putcharLP ('\a');
            break;

        default:
            if (lp < lineend)
            {
                *lp++ = c;
                echo();
            }
            else
                putcharLP ('\a');

            break;
    }
}

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: lp.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
