/* handy functions used both by boot and main app.
 */

#include "sa.h"

/* zero the unitialized data segment */
void
zeroBSS(void)
{
    unsigned char *ptr;

    for (ptr = &_bss_start; ptr < &_bss_end; )
        *ptr++ = 0;
}

/* utility to delay about 112ms per n */
void
spinDelay(int n)
{
    Word i;

    while (n--)
    {
        for (i=0; i < 60000U; i++)
            continue;
        resetCOP();
    }
}

/* flash pattern p on output LEDS. if self-test pin is installed go until
 * removed then reboot. if not installed reboot after a few flashes.
 * if go forever without self-test system could not be rebooted via LAN.
 * TODO: store in EE somewhere?
 */
void
errFlash(int p)
{
    int wasst = ISSELFTEST();
    int i;

    /* stabilize other systems */
    stopMot();
    PTTOFF();
    INTR_OFF();

    for (i = 0; ISSELFTEST() || (!wasst && i < 10); i++)
    {
        PORTH = ~p;
        spinDelay(5);
        PORTH = ~0;
        spinDelay(5);
    }

    rebootViaCOP();
}

/* return value of data[*ip], accounting for PESC, and update *ip. */
Byte
explodeData (Byte *data, int *ip)
{
    Byte b = data[(*ip)++];

    if (b == PESC)
        b = data[(*ip)++] == PESYNC ? PSYNC : PESC;
    return (b);
}

/* reboot by letting the COP expire */
void
rebootViaCOP(void)
{
    while (1)
        continue;
}

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: utils.c,v $ $Date: 2001/04/19 21:11:58 $ $Revision: 1.1.1.1 $ $Name:  $
 */
