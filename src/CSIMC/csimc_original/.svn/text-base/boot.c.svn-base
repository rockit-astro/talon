/* EEPROM boot program for CSIMC Rev C.
 * on power up:
 *   don't trust RESET, wait for COP to jump to boot()
 *   check for and load a FLEX program in FLASH DPAGE 3.
 *   if FLEX load fails or shunt is installed run self-test forever.
 *   copy the main app from FLASH DPAGEs 1 and 2 into SRAM and jump to it,
 *     never to return.
 */

#include "sa.h"

/* use of global bss data is ok but can not depend on initialization, even
 * though we need at least one just for the linker.
 */

Word mmon = 0;          /* current motor mon value */
Word lastmmon;          /* last motor mon value */
int mmondt;         /* RTI interrupts since last motor mon chg */
Word rtiold;            /* set when RTI is synced with motor mon */
Word nstray;            /* stray interrupt counter */

/* FLEX programming info */
#define FLEX_PROGADDR   0x380
#define FLEX_CONF_DONE  0x10
#define FLEX_DEV_OE 0x20
#define FLEX_nCONFIG    0x40
#define FLEX_RDYnBSY    0x80


/** self test **********************************************************/

/* check sram from p0 up to p1.
 * never return if find problem
 */
void
sramSegment (Word p0, Word p1)
{
    volatile Word *ptr;

    for (ptr = (Word*)p0; ptr < (Word*)p1; ptr++)
    {
        volatile Word save = *ptr;
        Word pattern, pnot;
        /* sliding bit pattern */
        for (pattern = 1; pattern; pattern <<= 1)
        {
            *ptr = pattern;
            if (*ptr != pattern)
                errFlash (1);
            pnot = ~pattern;
            *ptr = pnot;
            if (*ptr != pnot)
                errFlash (2);
        }
        *ptr = save;
        if (*ptr != save)
            errFlash (3);
    }
}

/* test RS485 port with loopback.
 * also send char out 232 for fun but can't verify
 */
void
sc0Test(void)
{
    char probe = 'P';       /* distinctive 0x50 pattern */

    /* if see something from 232 echo, else send probe */
    if (!(SC1SR1 & 0x40))       /* xmtr should be immediately empty */
        errFlash(4);
    SC1DRL = (SC1SR1 & 0x20) ? SC1DRL : probe;

    /* send probe out 485, should get it back */
    if (!(SC0SR1 & 0x40))       /* xmtr should be empty */
        errFlash(5);
    PTTON();            /* 485 on */
    SC0DRL = probe;         /* send */
    spinDelay(1);           /* allow time to drain and read back */
    PTTOFF();           /* 485 off */
    if (SC0SR1 != 0xf0)     /* perfection should await us */
        errFlash(6);
    if (SC0DRL != probe)        /* should see probe */
        errFlash (7);
}

/* perform self-test forever */
void
selfTest (void)
{
    Long ml;
    int page;

    /* setup RS485 port for self-test */
    SC0BDH = 0x06;
    SC0BDL = 0x83;                  /* 300 b */
    SC0CR1 = 0x00;                  /* 8/1/N */
    SC0CR2 = 0x0c;                  /* TE RE */

    /* setup 232 port for exercising */
    SC1BDH = 0x00;
    SC1BDL = 0x0d;                  /* 38400 b */
    SC1CR1 = 0x00;                  /* 8/1/N */
    SC1CR2 = 0x0c;                  /* TE RE */

    /* use input's LEDS as more outputs */
    PORTJ  = 0xff;          /* start all dark */
    DDRJ   = 0xff;          /* outputs */

    /* reset counters, start motor @ 9.77Hz, enable RTI @ 977Hz */
    setEncZero();
    setMotZero();
    setMotDir(1);
    ml.H.u = 0;
    ml.H.l = 0xa3d;
    setMotVel (&ml);
    rtiold = 0;
    INTR_ON();

    /* interleave testing each page of SRAM with some other test.
     * motor/enc are tested from RTI for best time control.
     */
    for (page = 0; 1; page = (page+1)&0x1f)
    {
        /* bale if shunt removed */
        if (!ISSELFTEST())
            rebootViaCOP();

        /* map DPAGE page to 7000..8000 and test */
        DPAGE = page;
        resetCOP();
        sramSegment (0x7000, 0x8000);

        /* show some of DPAGE for life and add in board DIP switch */
        PORTH = ~((getBrdAddr()<<3) | (page&0x7));

        /* test RS485 */
        resetCOP();
        sc0Test();

        /* blink EncStatus LED for fun */
        AWORD(0x210) ^= 1;
    }
}

/* handler for RT interrupt during self test */
#pragma interrupt_handler onRTI
static void
onRTI(void)
{
    Long ml, el;
    int mdir;

    /* clear interrupt */
    RTIFLG = 0x80;

    /* get and show current values */
    getEncPos(&el);
    getMotPos(&ml);
    mmon = ml.H.l & 0xf;
    mdir = getMotDir();
    PORTJ = ~(mmon | ((el.H.l >> 4) & 0xf0));

    /* see if mmon is changing at the right rate.
     * N.B. this assumes RTI is running @ 100x motor rate.
     */
    if (!rtiold)
    {
        /* sync mmondt counter with first motcon change */
        if (mmon != lastmmon)
        {
            mmondt = 0;
            lastmmon = mmon;
            rtiold = 1;
        }
    }
    else if (lastmmon != mmon)
    {
        if (abs(mmondt-100) > 1)    /* duration can jitter by 1 */
            errFlash(10);       /* changed but at wrong rate */
        mmondt = 0;         /* restart timer/counter */
        lastmmon = mmon;        /* remember as last mot pos */
    }
    else if (mmondt > 100)
    {
        errFlash(11);       /* no change for too long */
    }
    else
    {
        mmondt++;           /* increment timer/counter */
    }

    /* turn around at each displayed portion extreme */
    if (mmon == 0xf && mdir > 0)
        setMotDir(-1);
    else if (mmon == 0 && mdir < 0)
        setMotDir(1);
}


/* FLEX programming *********************************************************/

/* load a byte into the FLEX, manually operating chip select nCS.
 * N.B. it is a word addressed device so LSB is at higher addr.
 */
void
writeFlex (int b)
{
    int i;

    for (i = 0; i < 10; i++)
        if (PORTS & FLEX_RDYnBSY)
            break;
    if (i == 10)
        errFlash(8);        /* can not negotiate with FLEX */

    PORTF &= ~4;
    ABYTE(FLEX_PROGADDR+1) = b;
    PORTF |= 4;
}

/* check FLASH PPAGE 3 for sensible FLEX program.
 * if find, load and return 0.
 * if do not find, return -1.
 * if something more severe is wrong, flash forever (never return).
 */
int
loadFlex(void)
{
    Byte *prog;
    Word sum;
    Word n;
    Word i;

    /* setup */
    PORTS &= ~FLEX_DEV_OE;  /* disable outputs until finished */
    PORTS &= ~FLEX_nCONFIG; /* reset */
    spinDelay(1);       /* wait */
    PORTS |= FLEX_nCONFIG;  /* enable programming */
    spinDelay(1);       /* wait */
    PPAGE = 3;      /* select FLEX page, in FLASH PPAGE 3 */
    n = AWORD(0x8000);  /* get program byte count */
    prog = (Byte*)0x8004;   /* start of FLEX program */
    sum = 0;        /* init check sum */
    resetCOP();

    /* load FLEX and accumulate sum */
    for (i = 0; i < n; i++)
    {
        Byte byte = *prog++;
        writeFlex (byte);
        sum += byte;
    }
    resetCOP();

    /* check sum and whether FLEX bought the program */
    if (sum != AWORD(0x8002))
        return (-1);
    if (!(PORTS & FLEX_CONF_DONE))
        errFlash (9);
    PORTS |= FLEX_DEV_OE;   /* now can enable outputs */
    return (0);
}


/* boot point *********************************************************/

/* keep the COP from resetting */
void
resetCOP(void)
{
    COPRST = 0x55;
    COPRST = 0xaa;
}

#pragma interrupt_handler boot
void
boot(void)
{
    /* setup internal resource map.
     * 1st write to these locks them permanently (except EEON)
     * vectors at ff80..ffff
     */
    INITRG = 0x00;  /* registers block at 0..1ff */
    INITRM = 0x08;  /* on-chip RAM at 800..bff */
    INITEE = 0xf1;  /* on-chip EEPROM at f000..ffff */

    /* hw starts us in Normal Single Chip. switch to Normal Exp Wide now.
     * N.B. this by itself will not move EEPROM
     */
    MODE = 0xe0;    /* first write is ignored */
    MODE = 0xe0;

    /* enable chip selects so that:
     * PORTF0: CS0:  FLEX select while operating, 200..3ff
     * PORTF1: In:   FLASH RYnBY
     * PORTF2: Out:  FLEX select while programming
     * PORTF3: Out:  485 PTT output: 1 to talk, 0 to listen
     * PORTF4: CSD:  SRAM at 0000..7fff
     * PORTF5: CSP0: FLASH 8000 .. ffff
     * PORTF6: Out:  motor direction output: 1 is + direction
     * PORTF7: not used
     * no clock stretching.
     */
    CSCTL0 = 0x31;
    CSCTL1 = 0x10;
    CSSTR0 = 0x00;
    CSSTR1 = 0x00;
    PORTF  = 0x04;
    DDRF   = 0x4c;

    /* enable expanded D and P paging:
     * PPAGE 1f is same as c000..ffff.
     * DPAGE 0..1f selects one of 32 4k pages in 7000..7fff.
     * the 7 DPAGES 10..16 also appear at 0000..6777,
     *   (although 0..03ff are covered by registers and CS0)
     * so to avoid these, only use DPAGE set to 0..f and 17..1f
     */
    PPAGE  = 0x00;
    DPAGE  = 0x00;
    WINDEF = 0xc0;
    MXAR   = 0x0f;
    MISC   = 0x00;

    /* enable ByteLane and R/W bus signals.
     * PORTE4: Out: 0->1 to latch mot/enc counters
     */
    PEAR   = 0x1c;
    PORTE  = 0x10;
    DDRE   = 0x10;

    /* PORTH misc outputs,
     * PORTJ misc inputs.
     */
    DDRH   = 0xff;
    PORTH  = 0x00;
    DDRJ   = 0x00;

    /* more basic init */
    RTICTL = 0x81;          /* set clock rate to SPCT */
    COPCTL = 0x07;          /* lock in 1 second COP */

    /* set up PORTS0-3 for SC0/1, 4-7 for FLEX */
    SP0CR1 = 0x00;          /* disable SPI */
    DDRS   = 0x6a;          /* SC0, SC1 and FLEX directions */

    /* get ready for C */
    asm ("lds #0x0c00");    /* use onboard RAM to hide from SRAM self-test*/
    zeroBSS();      /* zero globals */

    /* run self-test if no FLEX program or shunt installed */
    if (loadFlex() < 0 || ISSELFTEST())
        selfTest();
    stopMot();      /* just to be safe */

    /* commited now to starting application */

    /* start ATD system in 8-channel auto-scan mode */
    ATDCTL2 = 0xc0;
    ATDCTL5 = 0x70;

    /* setup the 485 lan on SC0 for interrupt receive */
    SC0BDH = 0x00;      /* 38.4 Kbaud */
    SC0BDL = 0x0d;      /* 38.4 Kbaud */
    SC0CR1 = 0x00;      /* 8data, 1stop, no parity */
    SC0CR2 = 0x2c;      /* RIE TE RE. TCIE only when want to send */

    /* setup the 232 local port SC1 for interrupt receive */
    SC1BDH = 0x00;      /* 38.4 Kbaud */
    SC1BDL = 0x0d;      /* 38.4 Kbaud */
    SC1CR1 = 0x00;      /* 8data, 1stop, no parity */
    SC1CR2 = 0x2c;      /* RIE TE RE. TCIE only when want to send */

    /* set stack now where no page ever writes: the DPAGE PTI */
    asm ("lds #0x7f00");    /* hedge down to allow for compiler tricks */

    /* copy main app to SRAM from FLASH then jump to new main via 8000 */
    resetCOP();     /* buy time for all the copying */
    PPAGE = 1;
    memcpy ((void*)0x0400, (void*)0x8400, 0x3c00);  /* avoid registers */
    PPAGE = 2;
    memcpy ((void*)0x4000, (void*)0x8000, 0x3000);  /* avoid PTI */
    PPAGE = 0;
    resetCOP();     /* give main a running chance */
    (*(void (*)())AWORD(0x8000))(); /* good bye boot, hello app */
}


/* other interrupt handlers *************************************************/

/* handler for stray interrupts */
#pragma interrupt_handler onStray
static void
onStray(void)
{
    nstray++;
}

/* handler for Reset */
#pragma interrupt_handler reset
void
reset(void)
{
    /* don't trust reset.. wait for COP */
    while (1)
        continue;
}

#pragma interrupt_handler onILLOP
static void
onILLOP(void)
{
    errFlash (12);  /* same as in intr.c */
}

/* see errata for HC812AV4 mask 01H73K at
 * http://www.mcu.motsps.com/lit/errata/12err/a4_01h73k.html.
 * DESCRIPTION:
 *   When an I-type interrupt occurs at the same time as an RTI interrupt, the
 *   program address at $FFC0 will be fetched.
 * WORKAROUND:
 *   Put an RTI at $FFC0. This will get the program returned to start
 *   processing the proper interrupt as quickly as possible.
 */
#pragma abs_address:0xffc0
#pragma interrupt_handler onBug
static void
onBug(void)
{
}

/* interrupt vector table */
#pragma abs_address:0xffce
static void (*interrupt_vectors[])(void) =
{
    onStray,    /* Key Wakeup H */
    onStray,    /* Key Wakeup J */
    onStray,    /* ATD */
    onStray,    /* SCI 1 */
    onStray,    /* SCI 0 */
    onStray,    /* SPI */
    onStray,    /* PAIE */
    onStray,    /* PAO */
    onStray,    /* TOF */
    onStray,    /* TC7 */
    onStray,    /* TC6 */
    onStray,    /* TC5 */
    onStray,    /* TC4 */
    onStray,    /* TC3 */
    onStray,    /* TC2 */
    onStray,    /* TC1 */
    onStray,    /* TC0 */
    onRTI,  /* RTI */
    onStray,    /* IRQ */
    onStray,    /* XIRQ */
    onStray,    /* SWI */
    onILLOP,    /* ILLOP */
    boot,   /* COP */
    onStray,    /* CLM */
    reset,  /* RESET */
};

#pragma end_abs_address

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: boot.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
