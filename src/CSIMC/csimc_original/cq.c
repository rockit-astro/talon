/* code to manage a circular queue */

#include "sa.h"

void
CQinit(CQueue *cp)
{
    NINTR_OFF();
    cp->h = cp->t = 0;      /* match means empty */
    NINTR_ON();
}

int
CQn(CQueue *cp)
{
    Byte n;
    NINTR_OFF();
    n = (Byte)(cp->h - cp->t);  /* Byte type implies free modulo */
    NINTR_ON();
    return (n);
}

int
CQnfree(CQueue *cp)
{
    return((Byte)((QLEN-1)-CQn(cp)));
}

int
CQisEmpty(CQueue *cp)
{
    return (CQn(cp) == 0);
}

int
CQisFull(CQueue *cp)
{
    return (CQn(cp) == QLEN-1); /* h one-behind t means full */
}

/* put d on cp.
 * N.B. we do not check for overflow.. check first that !CQisFull().
 */
void
CQput(CQueue *cp, Byte d)
{
    NINTR_OFF();
    cp->q[++cp->h] = d;     /* type of h implies free modulo */
    NINTR_ON();
}

/* get oldest value in cp.
 * N.B. we do not check for underflow.. check first that !CQisEmpty().
 */
Byte
CQget(CQueue *cp)
{
    Byte b;
    NINTR_OFF();
    b = cp->q[++cp->t];     /* type of t implies a free modulo */
    NINTR_ON();
    return (b);
}

/* put a[na] on cp all at once.
 * N.B. we assume there is room so check first that CQnFree() >= na.
 */
void
CQputa (CQueue *cp, Byte a[], int na)
{
    Byte toend, n;

    NINTR_OFF();

    toend = (QLEN-1) - cp->h;
    n = na < toend ? na : toend;

    if (n > 0)
    {
        memcpy (&cp->q[(Byte)(cp->h+1)], a, n);
        cp->h += n;
        na -= n;
    }
    if (na > 0)
    {
        memcpy (&cp->q[(Byte)(cp->h+1)], a+n, na);
        cp->h += na;
    }
    NINTR_ON();
}

#ifdef TESTCQ

/* copies stdin to stdout via a CQueue.
 * tests all h/t cases if file size > 256**2.
 */

int
main (int ac, char *av[])
{
    CQueue cq, *cqp = &cq;
    int toggle = 0;
    int eof = 0;

    CQinit (cqp);

    while (1)
    {
        while (!CQisEmpty(cqp))
            putchar (CQget(cqp));
        if (eof)
            break;
        while (!CQisFull(cqp))
        {
            int n;
            if (toggle ^= 1)
            {
                Byte buf[QLEN];
                if ((n=fread(buf, sizeof(Byte), CQnfree(cqp), stdin))==0)
                {
                    eof = 1;
                    break;
                }
                else
                    CQputa(cqp, buf, n);
            }
            else
            {
                n = getchar();
                if (n == EOF)
                {
                    eof = 1;
                    break;
                }
                else
                    CQput (cqp, n);
            }
        }
    }

    return (0);
}
#endif /* TESTCQ */

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: cq.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
