/* this is a stand-alone boot loader.
 * stored in FLASH PPAGE 4 but copies to RAM at 0x0c00 to run.
 * listens to both ports for BOOT records for us and ACKs them.
 * it "returns" by letting the COP expire and reset the new code.
 */

#include "sa.h"

#define EETRY   4           /* times to try writting eeprom */
#define FLASHPAGE   0x8000      /* FLASH addr for chip select */
#define FLASHADDR   0x8c00      /* where we live in FLASH */
#define RAMADDR     0x0c00      /* where we execute in RAM */

/* link wrt execution address */
#pragma abs_address:0x0c00

void saLoader (Pkt *pkp);
int doBoots (void);
int loadPkt (Pkt *pkp);
void sendChar (Byte d);
void giveUpToken(void);
void write_eeprom (Byte *addr, Byte b);
void write_flashB (Word addr, Word data);
void erase_flash (void);
void sendACK (Pkt *pkp);
void resetCOP(void);
void spinDelay(int n);
void errFlash (int p);
int getBrdAddr (void);

/* entry point to start stand-alone loader for booting.
 * this is called via CALL instruction to 0x8c00/PPAGE 4 by main app when
 *   see first BOOT packet.
 * copy the real loader out of PPAGE 4 to RAMADDR then call.
 * N.B. we are using main's stack and rpkt which we hope we do not hammer.
 * N.B. call no helper funcs from here.. we are linked for 0x0c00 but running
 *   from 0x8c00!
 */
void
loadSALoader(Pkt *pkp)
{
    Byte *src, *dst, *end;
    Word start;

    /* no interrupts, just polling, since we might be changing vectors */
    INTR_OFF();
    PTTOFF();
    SC0CR2 = 0x0c;
    SC1CR2 = NO232GW() ? 0 : 0x0c;

    /* copy from here (FLASH) to RAM */
    PORTH = ~1;
    src = (Byte *)FLASHADDR;
    dst = (Byte *)RAMADDR;
    end = src + ((Word)getBrdAddr-(Word)loadSALoader+100);
    while (src < end)
        *dst++ = *src++;
    PORTH = ~2;

    /* jump to saLoader(), now in RAM a little after RAMADDR */
    start = RAMADDR + ((Word)saLoader - (Word)loadSALoader);
    (*(void (*)(Pkt *))start)(pkp);

    /* big trouble */
    while (1)
        PORTH = 22;
}

/* the real standalone loader.
 * we really execute from RAM.
 */
void
saLoader (Pkt *pkp)
{
    /* start by erasing flash (talk about burning bridges!) */
    resetCOP();
    PORTH = ~3;
    erase_flash();

    /* load the first BOOT packet */
    resetCOP();
    PORTH = ~4;
    (void) loadPkt (pkp);
    resetCOP();
    PORTH = ~5;
    sendACK (pkp);
    PORTH = ~6;

    /* load the rest until see BT_EXEC */
    do
    {
        resetCOP();
    }
    while (doBoots() == 0);

    /* let COP timeout to restart new code */
    while (1)
        continue;
}

/* watch for BOOT records and handle them.
 * no need to check for dups since it is harmless to do them again.
 * return -1 if see a BT_EXEC record, else return 0.
 */
int
doBoots (void)
{
    Byte rpkt[PMXLEN];
    int wantack;
    Byte b;
    int i;

    /* require SYNC first */
    if (getChar() != PSYNC)
    {
        PORTH = ~7;
        return (0);
    }
    rpkt[PB_SYNC] = PSYNC;

sync:

    /* read rest of header -- or might be token, which we never need */
    if ((b = getChar()) == PSYNC) goto sync;
    if (ISNTOK(b))
    {
        if (tok2addr(b) == getBrdAddr())
            giveUpToken();  /* we never want to keep the token */
        return (0);
    }
    if (b == BROKTOK)
        return (0);
    rpkt[PB_TO] = b;
    if ((rpkt[PB_FR] = getChar()) == PSYNC) goto sync;
    if ((rpkt[PB_INFO] = getChar()) == PSYNC) goto sync;
    if ((rpkt[PB_COUNT] = getChar()) == PSYNC) goto sync;
    if ((rpkt[PB_HCHK] = getChar()) == PSYNC) goto sync;

    /* no good if bad checksum, not BOOT or not for us */
    if (rpkt[PB_HCHK] != chkSum (rpkt, PB_NHCHK))
        return (0);
    if ((rpkt[PB_INFO] & PT_MASK) != PT_BOOTREC)
        return (0);
    wantack = rpkt[PB_TO] == getBrdAddr();
    if (!wantack)
        return (0);

    /* read data portion */
    if ((rpkt[PB_DCHK] = getChar()) == PSYNC) goto sync;
    for (i = 0; i < rpkt[PB_COUNT]; i++)
        if ((rpkt[PB_DATA+i] = getChar()) == PSYNC) goto sync;
    if (rpkt[PB_DCHK] != chkSum (rpkt+PB_DATA, rpkt[PB_COUNT]))
        return (0);

    /* yes! install new code */
    i = loadPkt ((Pkt *)rpkt);
    PORTH -= 1;

    /* send ack */
    if (wantack)
        sendACK((Pkt *)rpkt);

    /* return whether we saw EXEC */
    PTTOFF();
    return (i);
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

/* load the BootIm records in pkp.
 * if find a BT_EXEC return -1, else 0.
 */
int
loadPkt (Pkt *pkp)
{
    int i;

    for (i = 0; i < pkp->count; /* incremented in body */ )
    {
        BootIm bim;
        Byte *addr;
        int j;

        resetCOP();

        bim.type = explodeData (pkp->data, &i);
        bim.len = explodeData (pkp->data, &i);
        bim.addrh = explodeData (pkp->data, &i);
        bim.addrl = explodeData (pkp->data, &i);
        addr = (Byte *)(((Word)(bim.addrh) << 8) | (bim.addrl));

        if (bim.type & BT_EXEC)
            return ((Word)addr == (Word)saLoader ? 0 : -1);

        for (j = 0; j < bim.len; j++, addr++)
        {
            Byte b = explodeData (pkp->data, &i);
            if (*addr != b)
            {
                if (bim.type & BT_EEPROM)
                {
                    int k;
                    for (k = 0; k < EETRY; k++)
                    {
                        write_eeprom (addr, b);
                        if (*addr == b)
                            break;
                    }
                    if (k == EETRY)
                        errFlash (19);
                }
                else if (bim.type & BT_FLASH)
                {
                    if (!(((Word)addr)&1) && j < bim.len-1)
                    {
                        Byte b2 = explodeData (pkp->data, &i);
                        Word data = (((Word)b) << 8) | ((Word)b2);
                        write_flashW ((Word)addr, data);
                        j++;
                        addr++;
                    }
                    else
                        write_flashB ((Word)addr, b);
                }
                else
                    *addr = b;
            }
        }
    }

    return (0);
}

/* send ACK for pkp */
void
sendACK (Pkt *pkp)
{
    Byte ackpkt[PB_HSZ];
    int i;

    ackpkt[PB_SYNC] = PSYNC;
    ackpkt[PB_TO] = pkp->fr;
    ackpkt[PB_FR] = getBrdAddr();
    ackpkt[PB_INFO] = PT_ACK | (pkp->info & PSQ_MASK);
    ackpkt[PB_COUNT] = 0;
    ackpkt[PB_HCHK] = chkSum (ackpkt, PB_NHCHK);

    PTTON();
    for (i = 0; i < PB_HSZ; i++)
        sendChar (ackpkt[i]);
    PTTOFF();
}

/* start sending char d to 485 lan.
 * N.B. beware 485 loopback.
 * N.B. our caller must handle PTT.
 */
void
send485 (Byte d)
{
    SC0CR2 = 0x08;          /* 485 xmtr on, rcvr off */
    while (!(SC0SR1 & 0x80))    /* wait for async xmt rdy */
        resetCOP();
    SC0DRL = d;         /* send */
}

/* start sending char d to 232 gateway.
 */
void
send232 (Byte d)
{
    if (NO232GW())
        return;
    while (!(SC1SR1 & 0x80))    /* wait for async xmt rdy */
        resetCOP();
    SC1DRL = d;         /* send */
}

/* send char d to each port, wait until both gone.
 * N.B. our caller must handle PTT.
 */
void
sendChar (Byte d)
{
    send232 (d);
    send485 (d);

    while (!(SC0SR1 & 0x40) || (!NO232GW() && !(SC1SR1 & 0x40)))
        resetCOP();
}


/* wait for a char from either comm line.
 * echo to other line then return it.
 */
int
getChar (void)
{
    Byte d;

    while (1)
    {
        resetCOP();

        d = SC0SR1;

        /* 485 xmt complete? */
        if (d & 0x40)
        {
            PTTOFF();
            SC0CR2 = 0x04;          /* 485 xmtr off, rcvr on */
        }

        /* 485 incoming? */
        if (d & 0x20)
        {
            d = SC0DRL;
            send232 (d);
            return (d);
        }

        /* 232 incoming? */
        if (!NO232GW() && (SC1SR1 & 0x20))
        {
            d = SC1DRL;
            PTTON();
            send485 (d);
            return (d);
        }
    }
}

/* give back token */
void
giveUpToken (void)
{
    PTTON();
    sendChar (PSYNC);
    sendChar (BROKTOK);
    PTTOFF();
}

/* delay about 10 ms */
void
eesoak (void)
{
    unsigned i;

    for (i = 0; i < 5000; i++)
        continue;
}

/* write byte b at EEPROM addr */
void
write_eeprom (unsigned char *addr, unsigned char b)
{
    resetCOP();

    /* set up for EEPROM write */
    EEPROT = 0;     /* disable protection */
    INITEE = 1;     /* insure in map */

    /* first erase */
    EEPROG = 0x96;      /*  EELAT !EEPGM */
    *addr = 0;      /* write anything */
    EEPROG = 0x97;      /*  EELAT  EEPGM */
    eesoak ();      /* soak */
    EEPROG = 0x96;      /*  EELAT !EEPGM */
    EEPROG = 0x90;      /* !EELAT !EEPGM */

    /* then program */
    EEPROG = 0x92;      /*  EELAT !EEPGM */
    *addr = b;      /* write data */
    EEPROG = 0x93;      /*  EELAT  EEPGM */
    eesoak ();      /* soak */
    EEPROG = 0x92;      /*  EELAT !EEPGM */
    EEPROG = 0x90;      /* !EELAT !EEPGM */

    resetCOP();
}

/* write 2 bytes of data into FLASH at addr.
 * N.B. addr must be even.
 */
void
write_flashW (Word addr, Word data)
{
    int i;

    /* FLASH addr on page boundry */
    Word base = FLASHPAGE;

    /* enter Program mode then write desired word */
    AWORD(base + 0xaaa) = 0xaa;
    AWORD(base + 0x554) = 0x55;
    AWORD(base + 0xaaa) = 0xa0;
    AWORD(addr) = data;

    for (i = 0; i < 10000; i++)
        if (AWORD(addr) == data)
            return;
    errFlash (20);
}

/* write 1 Byte of data into FLASH at addr */
void
write_flashB (Word addr, Word data)
{
    Word other;

    /* can really only write words */
    if (addr & 1)
    {
        addr -= 1;
        other = ABYTE(addr);
        data = (other << 8) | data;
    }
    else
    {
        other = ABYTE(addr+1);
        data = (data << 8) | other;
    }

    write_flashW (addr, data);
}

/* erase all of FLASH */
void
erase_flash()
{
    int i;

    /* FLASH addr on page boundry */
    Word base = FLASHPAGE;

    /* reset */
    AWORD(base) = 0xf0;

    /* send Chip Erase command */
    AWORD (base + 0xaaa) = 0xaa;
    AWORD (base + 0x554) = 0x55;
    AWORD (base + 0xaaa) = 0x80;
    AWORD (base + 0xaaa) = 0xaa;
    AWORD (base + 0x554) = 0x55;
    AWORD (base + 0xaaa) = 0x10;

    /* wait for RDnBY = bit 1 in PORTF.
     * takes 5-6 seconds.
     */
    for (i = 0; i < 100; i++)
    {
        spinDelay(1);
        if (PORTF & 2)
            return;
    }
    errFlash (21);
}

/* compute check sum on the given array */
int
chkSum (Byte p[], int n)
{
    Word sum;

    for (sum = 0; n > 0; --n)
        sum += *p++;
    while (sum > 255)
        sum = (sum & 0xff) + (sum >> 8);
    if (sum == PSYNC)
        sum = 1;
    return (sum);
}

/* keep the COP from resetting */
void
resetCOP(void)
{
    COPRST = 0x55;
    COPRST = 0xaa;
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

/* flash pattern p on output LEDS for a while then reboot.
 * TODO: store in EE somewhere?
 */
void
errFlash(int p)
{
    int i;

    PTTOFF();

    for (i = 0; i < 10; i++)
    {
        PORTH = ~p;
        spinDelay(5);
        PORTH = ~0;
        spinDelay(5);
    }

    /* let COP timeout to reboot */
    while (1)
        continue;
}

/* get board dip switches.
 * N.B. must be last, and small.. used to determine size of load!
 */
int
getBrdAddr (void)
{
    return (PORTT & 0x1f);
}

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: saloader.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
