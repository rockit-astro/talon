/* entry point for the main application.
 * until then, see boot() in boot.c
 */

#include "sa.h"

#ifdef HOSTTEST

int
main (int ac, char *av[])
{
	/* just run the 1 shell */
	pti.flags = TF_INUSE;
	runThread();

	/* should never get here */
	fprintf (stderr, "Oh no!\n");

	return (1);
}

#else

void
main(void)
{
	/* remove EEPROM from map, init stack, BSS and heap */
	INITEE = 0;
	asm ("lds #%_text_start-10");	/* hedge against compiler tricks */
	zeroBSS();
	_NewHeap (&_bss_end, (void*)(&_text_start - NCSTACK));

	/* build thread base and go forever */
	resetCOP();
	initAllThreads();
	scheduler(0);

	/* should never get here */
	errFlash(14);
}


/* boot jumps through 0x8000 to start main. */
#pragma abs_address:0x8000
void (*addr_of_main)() = main;
#pragma end_abs_address

#endif	/* HOSTTEST */

/* misc goodies */
long
f2l (float f)
{
	if (f < 0)
	    return ((long)(f - 0.5));
	return ((long)(f + 0.5));
}

int
fsign (float x)
{
	return (x < 0 ? -1 : (x > 0 ? 1 : 0));
}

void
verStats(void)
{
	Byte me = getBrdAddr();
	int i;
	printf ("   ICE Version %s\n", ICE_VERSION);
	printf ("   Node:");
	printf ("%6u version", VERSION);
	printf ("%4x FLEX  ", getFlexVer());
	for (i = 5; --i >= 0; )
	    printf ("%d", (me >> i)&1);
	printf ("=%d myaddr", me);
	if (weRgateway)
	    printf ("    Gateway");
	printf ("\n");
}

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: main.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
