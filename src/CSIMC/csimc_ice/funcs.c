/* code to implement the built-in functions
 * see do_icall() in exec.c for calling circumstances.
 */

#include "sa.h"

/* built-in function stats().
 * print some statistics about the node
 */
static int
bif_stats(void)
{
	verStats();
	timeStats();
	maxMalloc();
	ufuncStats();
	lanStats();
	threadStats();
	return (0);	/* purely academic */
}

/* Heart of built-in printf.
 * format and send a message to pti.to using pti.sout.
 * PC points to 0-terminated format string, PC is moved past when we return.
 * params are at SP[1..nparams] == [right .. left]
 */
static int
printfHeart(int nparams)
{
	typedef enum { INNORM, INBS, INFMT } PrState;
	char *fmt = (char*)PC;		/* get fmt string */
	char *fmt0 = fmt;		/* save for count */
	char buf[10], *bp;
	PrState prs;
	char c;

	/* basic chore is scan for %d and %x and turn into %ld and %lx.
	 * also check for \. pass through anything else.
	 */
	prs = INNORM;
	while ((c = *fmt++) != '\0') {
	    switch (prs) {
	    case INNORM:
		if (c == '\\')
		    prs = INBS;
		else if (c == '%') {
		    bp = buf;
		    *bp++ = c;
		    prs = INFMT;
		} else
		    break;	/* print */
		continue;
	    case INBS:
		if (c == 'n')
		    c = '\n';
		prs = INNORM;
		break;
	    case INFMT:
		if (bp - buf < (sizeof(buf)-3)) {
		    if (c == 'd' || c == 'x') {
			*bp++ = 'l';
			*bp++ = c;
			*bp++ = '\0';
			if (printf (buf, PEEK(nparams--)) < 0)
			    return (-1);
			prs = INNORM;
		    } else if (c == '%') {
			prs = INNORM;
			break;	/* print */
		    } else {
			*bp++ = c;
		    }
		}
		continue;
	    }

	    /* if get here, print c */
	pc:
	    if (putchar (c) < 0)
		return (-1);
	}

	/* advance past fmt */
	PC += fmt - fmt0;

	/* ok */
	return (0);
}

/* SP[0] points to nparams, which are then at SP[1..nparams]
 */
static int
bif_printf (void)
{
	if (printfHeart(PEEK(0)) < 0)
	    fatalError ("printf");
	return (0);	/* purely academic */
}

static int
bif_stop (void)
{
	killMotion(0);
	return (0);	/* purely academic */
}

/* track a coarse given by a table of positions at fixed time intervals.
 * SP[nparams]: t0
 * SP[nparams-1]: dt
 * SP[nparams-2]: p0
 * SP[nparams-3]: p1
 *      ...
 * SP[1]: p[nparams-3]
 * SP[0]: nparams.
 * put another way, there are (nparams-2) positions in the table, located in 
 *   increasing time order at SP[nparams-2] .. SP[1].
 */
static int
trackHeart (int encoder)
{
	int np = SP[0];
	return (startTrack (encoder, SP[np], SP[np-1], &SP[1], np-2));
}

static int
bif_etrack (void)
{
	if (!cvg(CV_ESTEPS)) {
	    printf ("No esteps\n");
	    return (-1);
	}
	return (trackHeart(1));
}

static int
bif_mtrack (void)
{
	return (trackHeart(0));
}

/*ICE*/
static int
bif_xtrack (void)
{
	int np = SP[0];
	xstartTrack(SP[np], SP[np-1], SP[np-2], SP[np-3], SP[np-4], SP[np-5]);
	return 0;
}


static int bif_xgetvar (void)
{
	xgetvar(SP[1]);
	return 0;
}
static int bif_xsetvar (void)
{
	xsetvar(SP[3], SP[2], SP[1]);
	return 0;
}


static int
bif_xpos (void)
{
	int np = SP[0];
	if (np<2) printf("%d\n", xpos_get_size());
	else xpos_add(SP[2], SP[1]);
	return 0;
}

static int
bif_xdelta (void)
{
	int np = SP[0];
	if (np<2) printf("%d\n", xdelta_get_size());
	else xdelta_add(SP[2], SP[1]);
	return 0;
}


static IFSTblE ifstbl[] = {
    /* Name		Minargs	Maxargs	Func */
    {"stats",		0,	0,		bif_stats},
    {"printf",		0,	255,	bif_printf},
    {"stop",		0,	0,		bif_stop},
    {"etrack",		4,	255,	bif_etrack},
    {"mtrack",		4,	255,	bif_mtrack},

    /*ICE*/
    {"xgetvar",		1,	1,		bif_xgetvar},
    {"xsetvar",		3,	3,		bif_xsetvar},
    {"xtrack",		6,	6,		bif_xtrack},
    {"xpos",		0,	2,		bif_xpos},
    {"xdelta",		0,	2,		bif_xdelta},
    /*ICE*/

};
#define	NIFSTBL	(sizeof(ifstbl)/sizeof(ifstbl[0]))

/* search ifstbl for the named built-in function.
 * return pointer to an IFSTblE else NULL.
 */
IFSTblE *
findIFunc (char name[])
{
	int i;

	for (i = 0; i < NIFSTBL; i++)
	    if (strncmp (name, ifstbl[i].name, NNAME) == 0)
		return (&ifstbl[i]);
	return (NULL);
}

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: funcs.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
