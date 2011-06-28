/* this program takes .s19 files and produces a single .cmf file on stdout.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "csimc.h"

static void usage (void);
static void doBootIm (char *fn, int type);
static void addEEimage (char *fn);
static void addFLASHimage (int ppage, char *fn);
static void addEXEC (int addr);
static void setPPAGE (int ppage);
static void eeInMap (int whether);
static void doDump (void);

static char *pname;
static unsigned int estart, eend;
static unsigned int fstart, fend;

#define LOWFLASH    0x8000      /* lowest possible FLASH address */
#define LOWEEPROM   0xf000      /* lowest possible EEPROM address */

#define PPAGE       0x35        /* PPAGE control register */
#define INITEE      0x12        /* INITEE control register */

int
main (int ac, char *av[])
{
    pname = av[0];

    while ((--ac > 0) && ((*++av)[0] == '-'))
    {
        char *s;
        for (s = av[0]+1; *s != '\0'; s++)
            switch (*s)
            {
                case 'd':
                    doDump();
                    return(0);
                case 'e':
                    if (ac < 2)
                        usage();
                    addEEimage (*++av);
                    ac--;
                    break;
                case 'f':
                    if (ac < 3)
                        usage();
                    addFLASHimage ((int)strtol(av[1], NULL, 0), av[2]);
                    av += 2;
                    ac -= 2;
                    break;
                case 'x':
                    if (ac < 2)
                        usage();
                    addEXEC ((int)strtol(*++av, NULL, 0));
                    ac--;
                    break;
                default:
                    usage();
            }
    }

    if (ac > 0)
        usage();

    return (0);
}

static void
usage()
{
    fprintf (stderr, "Usage: %s [-d] | [-x addr] .. [-e eeprom.s19] .. [-f PPAGE flash.s19] .. \n", pname);
    fprintf(stderr, "Purpose: to convert S-Records into one composite .cmf format on stdout.\n");
    fprintf(stderr, "Purpose: can also dump a .cmf file in readable format.\n");
    fprintf(stderr, "  -d dumps a .cmf on stdin in a readable format to stdout.\n");
    fprintf(stderr, "  -e flags the given .s19 is for EEPROM\n");
    fprintf(stderr, "  -f flags the given .s19 is for FLASH in the given PPAGE\n");
    fprintf(stderr, "  -x adds an EXEC record for the given addr\n");

    exit (1);
}

static void
addEEimage (char *fn)
{
    eeInMap(1);
    doBootIm (fn, BT_EEPROM);
}

static void
addEXEC (int addr)
{
    BootIm bim;

    bim.type = BT_EXEC;
    bim.len = 0;
    bim.addrh = addr >> 8;
    bim.addrl = addr & 0xff;
    write (1, &bim, sizeof(bim));

    /* no data */
}

static void
addFLASHimage (int ppage, char *fn)
{
    eeInMap(0);
    setPPAGE (ppage);
    doBootIm (fn, BT_FLASH);
}

/* add the given file of given type.
 * N.B. we never set BT_FLASH is below 8000 or BT_EEPROM is below f000.
 * exit if trouble
 */
static void
doBootIm (char *fn, int type)
{
    FILE *fp;
    char line[128];
    int lineno;

    /* open */
    fp = fopen (fn, "r");
    if (!fp)
    {
        fprintf (stderr, "%s: %s\n", fn, strerror(errno));
        exit(1);
    }

    /* read and convert */
    lineno = 0;
    while (fgets (line, sizeof(line), fp))
    {
        unsigned int len, addrh, addrl;
        BootIm bim;

        lineno++;

        if (sscanf (line, "S1%2x%2x%2x", &len, &addrh, &addrl) == 3)
        {
            /* data record: S1LLAAAADDDDDDDDCC
             * LL:   2 chars = 1 byte, number of data bytes including ADC
             * AAAA: 4 chars = 2 byte, address
             * CC:   2 chars = 1 byte, checksum
             */
            Byte data[128], *dp = data;
            unsigned int datum;
            unsigned int addr;
            int i;

            if (len < 4)
            {
                fprintf (stderr, "%s: No data, line %d\n", fn, lineno);
                exit (1);
            }
            bim.type = BT_DATA | type;
            addr = (addrh << 8) | (addrl);
            if (addr < LOWFLASH)
                bim.type &= ~BT_FLASH;
            if (addr < LOWEEPROM)
                bim.type &= ~BT_EEPROM;
            bim.len = len - 3;
            bim.addrh = addrh;
            bim.addrl = addrl;

            for (i = 0; i < bim.len; i++)
            {
                unsigned data;
                if (sscanf (&line[8+2*i], "%2x", &datum) != 1)
                {
                    fprintf (stderr, "%s: Bad data, line %d\n", fn, lineno);
                    exit (1);
                }
                *dp++ = datum;
            }

            write (1, &bim, sizeof(bim));
            write (1, data, bim.len);
        }

        else if (sscanf (line, "S9%2x%2x%2x", &len, &addrh, &addrl) == 3)
        {
            /* exec record: S1LLAAAACC
             * LL:   2 chars = 1 byte, number of data bytes including AC
             * AAAA: 4 chars = 2 byte, address
             * CC:   2 chars = 1 byte, checksum
             */

            if (len != 3)
            {
                fprintf (stderr, "%s: Bogus S9, line %d\n", fn, lineno);
                exit (1);
            }
            if (addrh == 0 && addrl == 0)
                continue;       /* just a NOP from linker */

            bim.type = BT_EXEC;
            bim.len = 0;
            bim.addrh = addrh;
            bim.addrl = addrl;

            write (1, &bim, sizeof(bim));
        }

        else
        {
            fprintf (stderr, "%s: Unknown record, line %d\n", fn, lineno);
            exit (1);
        }
    }
}

/* write a record to set PPAGE */
static void
setPPAGE (int ppage)
{
    BootIm bim;
    Byte datum;

    bim.type = BT_DATA;
    bim.len = 1;
    bim.addrh = 0;
    bim.addrl = PPAGE;
    write (1, &bim, sizeof(bim));
    datum = ppage;
    write (1, &datum, bim.len);
}

/* write a record to control whether EEPROM is in map */
static void
eeInMap (int whether)
{
    BootIm bim;
    Byte datum;

    bim.type = BT_DATA;
    bim.len = 1;
    bim.addrh = 0;
    bim.addrl = INITEE;
    write (1, &bim, sizeof(bim));
    datum = whether;
    write (1, &datum, bim.len);
}

/* dump a .cmf on stdin to stdout in some readable form */
static void
doDump (void)
{
    Byte buf[128];
    BootIm bim;

    /* first line is version */
    if (fgets (buf, sizeof(buf), stdin))
        printf ("Version %s", buf);

    while (fread (&bim, sizeof(bim), 1, stdin) == 1)
    {
        printf ("%c", bim.type & BT_DATA ? 'D' : ' ');
        printf ("%c", bim.type & BT_EXEC ? 'X' : ' ');
        printf ("%c", bim.type & BT_EEPROM ? 'E' : ' ');
        printf ("%c", bim.type & BT_FLASH ? 'F' : ' ');
        printf (" %2d@%04x:", bim.len, ((int)bim.addrh << 8) | bim.addrl);
        if (bim.len > 0)
        {
            if (fread (buf, bim.len, 1, stdin) == 1)
            {
                int i;
                for (i = 0; i < bim.len; i++)
                    printf (" %02x", buf[i]);
            }
        }
        printf ("\n");
    }
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: s19tocmf.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $"};
