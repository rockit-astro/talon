/* code to manage motor and encoder, and other hw i/o */

#include "sa.h"

/* addresses of FLEX functions */
#define	MOTCON		0x200
#define	MOTMON		0x204
#define	ENCMON		0x208
#define	MRESET		0x20c
#define	ERESET		0x20e
#define	ENCERR		0x210
#define	FLEXVER		0x23e

/* latch the motmon and encoder values for casual reading */
static void
melatch(void)
{
	PORTE &= ~0x10;
	PORTE |= 0x10;
}

/* read encoder value */
void
getEncPos (Long *ep)
{
	melatch();
	ep->H.u = AWORD(ENCMON);
	ep->H.l = AWORD(ENCMON+2);
}

/* return 1 if encoder is complaining, else 0 */
void
getEncStatus (int *sp)
{
	*sp = AWORD(ENCERR) & 1;
}

/* reset encoder error indication */
void
resetEncStatus (void)
{
	AWORD(ENCERR) = 0;
}

/* reset encoder counter to 0. */
void
setEncZero (void)
{
	AWORD(ERESET) = 1;
	AWORD(ERESET) = 0;
	melatch();
}

/* get motor step counter */
void
getMotPos (Long *mp)
{
	melatch();
	mp->H.u = AWORD(MOTMON);
	mp->H.l = AWORD(MOTMON+2);
}

/* set motor step counter to 0 */
void
setMotZero(void)
{
	AWORD(MRESET) = 1;
	AWORD(MRESET) = 0;
	melatch();
}

/* get current commanded motor step rate */
void
getMotVel (Long *mp)
{
	mp->H.u = AWORD(MOTCON);
	mp->H.l = AWORD(MOTCON+2);
}

/* set a new commanded motor step rate */
void
setMotVel (Long *vp)
{
	AWORD(MOTCON+0) = vp->H.u;
	AWORD(MOTCON+2) = vp->H.l;
}

/* set a new commanded motor direction, based on sign of dir */
void
setMotDir (int dir)
{
	/* N.B. check that this compiles to change PORTF atomically, since PTT
	 * is also in PORTF and it can change from an interrupt
	 */
	if (dir >= 0)
	    PORTF |= 0x40;
	else
	    PORTF &= ~0x40;
}

/* get a commanded motor direction, based on sign of dir */
int
getMotDir (void)
{
	return ((PORTF & 0x40) ? 1 : -1);
}

/* get board dip switches */
int
getBrdAddr (void)
{
	return (PORTT & 0x1f);
}

/* return the version code of the Altera firmware */
Word
getFlexVer (void)
{
	return (AWORD(FLEXVER));
}

/* handy function to stop all motor pulses */
void
stopMot(void)
{
	Long L;

	L.H.u = L.H.l = 0;
	setMotVel (&L);
}

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: motenc.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
