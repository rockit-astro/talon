/* code to bridge to a digitemp */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telstatshm.h"
#include "telenv.h"

#include "onewire.h"
#include "ds1820.h"

static void init(char *tty);
static double do_temp (unsigned char *scratch);
static int read_temp (int fd, struct _roms *rom_list, int sensor, double *tp);

/* just for legacy onewire.c */
int debug;

#define	FAILTIME	5	/* failure timeout, secs */
#define	READTIME	500	/* read delay, ms */

static int fd = -1;		/* tty filedes */
static struct _roms rom_list;	/* info about each sensor */

/* read the temperature of each sensor on tty and load into wp.
 * if !tty, create fake data.
 * if any trouble, use daemonLog() and exit.
 */
void
digitemp (char *tty, WxStats *wp)
{
	int i;

	if (!tty) {
	    wp->auxtmask = 0x7;
	    wp->auxt[0] = 10;
	    wp->auxt[1] = -10;
	    wp->auxt[2] = 25.5;
	    return;
	}

	if (fd < 0)
	    init(tty);

	wp->auxtmask = 0;

	for (i = 0; i < rom_list.max; i++) {
	    double t;

	    if (read_temp (fd, &rom_list, i, &t) < 0) {
		daemonLog ("Trouble with sensor %d", i);
		exit (1);
	    }
	    wp->auxtmask |= (1<<i);
	    wp->auxt[i] = t;
	}
}

/* open the given tty, fill rom_list and fd.
 * if any trouble, use daemonLog() and exit.
 */
static void
init(char *tty)
{

	/* init */
	memset (&rom_list, 0, sizeof(rom_list));

	/* Initalize the interface to the DS1820 */
	fd = Setup (tty);
	if (fd < 0) {
	    daemonLog ("failure opening %s", tty);
	    exit (1);
	}

	/* initalize/find the sensors */
	if (SearchROM (fd, FAILTIME, &rom_list) < 0) {
	    daemonLog ("Error Searching for aux sensors");
	    exit (1);
	}

	/* too weird if none found */
	if (rom_list.max <= 0) {
	    daemonLog ("HAVEAUX but none found");
	    exit(1);
	}

	/* ok */
}

/* -----------------------------------------------------------------------
   Return the high-precision temperature value

   Calculated using formula from DS1820 datasheet

   Temperature   = scratch[0]
   Sign          = scratch[1]
   TH            = scratch[2]
   TL            = scratch[3]
   Count Remain  = scratch[6]
   Count Per C   = scratch[7]
   CRC           = scratch[8]
   
                   count_per_C - count_remain
   (temp - 0.25) * --------------------------
                       count_per_C

   If Sign is not 0x00 then it is a negative (Centigrade) number, and
   the temperature must be subtracted from 0x100 and multiplied by -1
      
   ----------------------------------------------------------------------- */
static double
do_temp (unsigned char *scratch)
{
	double t, hi_precision;

	if (scratch[1] == 0)
	    t = (int) scratch[0] >> 1;
	else
	    t = -1 * (int) (0x100-scratch[0]) >> 1;
	t -= 0.25;
	hi_precision = (int) scratch[7] - (int) scratch[6];
	hi_precision = hi_precision / (int) scratch[7];
	t = t + hi_precision;

	return (t);
}

/* Read the temperature from one sensor connected to fd.
 * return 0 if with *tp set to temp in degrees C, else -1.
 */
static int
read_temp (int fd, struct _roms *rom_list, int sensor, double *tp)
{
	unsigned char	scratch[9];

	MatchROM (fd, FAILTIME, rom_list, sensor);
	ReadTemperature (fd, FAILTIME, READTIME);

	MatchROM (fd, FAILTIME, rom_list, sensor);
	if (ReadScratchpad (fd, FAILTIME, scratch) < 0)
	    return (-1);

	*tp = do_temp (scratch) ;
	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: digitemp.c,v $ $Date: 2001/04/19 21:12:11 $ $Revision: 1.1.1.1 $ $Name:  $"};
