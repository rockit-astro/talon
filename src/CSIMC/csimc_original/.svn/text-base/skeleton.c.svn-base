#include "sa.h"
#include <stddef.h>

/* delay about 10 ms */
static void
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
    /* disable EEPROM protection */
    EEPROT = 0;

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
}

int x = 0;

void
main(void)
{
    Word addr;

    for (addr = 0xf120; addr < 0xf130; addr++)
        write_eeprom ((Byte *)addr, 0x0);
    while (1);
}

static void
zeroBSS(void)
{
    char *ptr;

    for (ptr = &_bss_start; ptr < &_bss_end; )
        *ptr++ = 0;
}

#pragma interrupt_handler _start
void
_start(void)
{
    /* set stack pointer just below EEPROM */
    asm ("lds #$effe");

    /* no COP */
    COPCTL = 0x00;

    /* setup internal resources.
     * 1st write to these locks them permanently.
     * vectors at ff80..ffff
     */
    INITRG = 0x00;  /* registers block 0..1ff */
    INITRM = 0x08;  /* on-chip 1k 800..bff */
    INITEE = 0xf1;  /* EEPROM f000..ffff */

    /* start in Normal Single Chip. switch to Normal Exp Wide.
     * N.B. this by itself will not move EEPROM
     */
    MODE = 0xe0;
    MODE = 0xe0;

    /* enable chip selects so that:
     * CS0:  encoder  = 200..2ff
     * CS1:  motorcon = 300..37f
     * CS2:  motormon = 380..3ff
     * CS3:  485 PTT output
     * CSP0: 8000..ffff
     * CSP1: motor direction output
     * CSD:  0000..7fff
     * no clock stretching.
     */
    PORTF = 0x37;
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
    PORTH = 0xff;
    DDRJ = 0x00;

    zeroBSS();

    /* go */
    main();
}

#pragma interrupt_handler onStray
static void
onStray(void)
{
}

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
    onStray,    /* RTI */
    onStray,    /* IRQ */
    onStray,    /* XIRQ */
    onStray,    /* SWI */
    onStray,    /* ILLOP */
    onStray,    /* COP */
    onStray,    /* CLM */
    _start  /* RESET */
};

#pragma end_abs_address

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: skeleton.c,v $ $Date: 2001/04/19 21:11:58 $ $Revision: 1.1.1.1 $ $Name:  $
 */
