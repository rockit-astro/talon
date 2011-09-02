/* sort a collection of Obs scans, in place, into a good observing order.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <Xm/Xm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "scan.h"

#include "telsched.h"

#define	NSLOTS		(3600*24/SECSPERSLOT)
#define	SLOTMOD(i)	(((i)+NSLOTS)%NSLOTS)	/* works off both ends */
#define	SLOTNUM(utc)	SLOTMOD(((int)floor(((utc)-utcdusk)/24.*NSLOTS+.5)))
#define	SLOTUTC(n)	fmod((n)*24.0/NSLOTS+utcdusk, 24.0)

static void sortscans_p (Obs *op, int nop, double lngcor, double today);
static int get_lststart (Obs op[], int nop, double lngcor, double today,
    Obs *xop[]);
static int get_evening (Obs op[], int nop, Obs *xop[]);
static int get_morning (Obs op[], int nop, Obs *xop[]);
static int get_night (Obs op[], int nop, Obs *xop[]);
static int get_circumpolar (Obs op[], int nop, Obs *xop[]);
static int srch_backwards (int start, int dur, Obs *a[NSLOTS], int iglight);
static int srch_forwards (int start, int dur, Obs *a[NSLOTS], int iglight);
static int srch_specific (int start, int dur, Obs *a[NSLOTS], int iglight);
static void slots_copy (Obs **dst, Obs **src, int nobs);
static void slots_blockout (Obs **a, int first, int last, Obs *op);
static void slot_fill (int start, int n, Obs *op);
static int obssort_utc (const void  *item0, const void *item1);
static int obssort_priority (const void  *item0, const void *item1);
static int obspsort_risetm (const void *item0, const void *item1);
static int obspsort_settm (const void *item0, const void *item1);
static void obs_blockout (Obs **a, Obs *op);
static void dump_slots (char *name, Obs **s, int n);

/* define DUMP_SLOTS to get printfs of slot settings */
#undef	DUMP_SLOTS

static double utcdawn, utcdusk;
static int dawnslot, duskslot;
static Obs **slots;		/* master array for entire night */

#define	OPLIGHT	((Obs *)1)	/* used to mark slots outside dusk..dawn */
#define RS_ODD	(RS_CIRCUMPOLAR|RS_NEVERUP|RS_NOSET|RS_ERROR)

/* given an array of Obs sort them into a good observing order.
 * turn on the unsorted bit until we find a place for each.
 *
 * we do it by making an array of *Obs. each slot stands for a time slice of
 * SECSPERSLOT and there are enough for a whole 24 hour period. the pointers all
 * start as NULL. there are generic functions that scan through the slots list
 * looking for suitable vacancies. once assigned, things do not move. a
 * priroty scheme makes use of this property by sorting the list of Obs first
 * by priority then working with only common subsets with same priority at a
 * time. when all the objects have been assigned, set their utcstart times
 * based on the location of the pointer in the slots array and sort the
 * original Obs array one last time so list is in UTC order.
 *
 * N.B. the first entry in the slots array is the moment of dusk today.
 * all times are biased this way so we don't have to deal with time wrap
 * around.
 *
 * N.B. the slots[] array *could* just be a bitmap but we make it point to
 * the Obs assigned to that slot in case we need to check something about it.
 * 
 * for each priority, here is the order things are added:
 * 1) scan for Obs with LSTSTART and assign based solely on that.
 * 2) get evening objects, sort by increasing set time, merge from dusk.
 * 3) get morning objects, sort by decreasing rise time, merge back from dawn.
 * 4) get night objects, merge either side of transit time.
 * 5) get circumpolar objects, merge by transit time as possible.
 * 6) all remaining objects are daytime; just set utctstart to transit time.
 *
 * N.B. we assume op->scan.obj and op->rs are already filled in.
 */
void
sortscans (np, op, nop)
Now *np;
Obs *op;
int nop;
{
	double today = mjd_day (np->n_mjd);
	double lngcor = radhr (np->n_lng);
	double mjddawn, mjddusk;
	Obs *pop;	/* first of block of objects in op[] with priority p */
	int i;

	if (nop <= 0)
	    return;

	/* get today's dawn and dusk times and slots handy */
	dawnduskToday (&mjddawn, &mjddusk);
	utcdawn = mjd_hr(mjddawn);
	utcdusk = mjd_hr(mjddusk);
	dawnslot = SLOTNUM(utcdawn);
	duskslot = SLOTNUM(utcdusk);	/* this will always be 0 */

	/* get zeroed slots array */
	slots = (Obs **) calloc (NSLOTS, sizeof(Obj *));

	/* permanently block out everything outside dusk..dawn */
	slots_blockout (slots, dawnslot, NSLOTS-1, OPLIGHT);

	/* set sorted to false unless object is off.
	 */
	for (i = 0; i < nop; i++)
	    op[i].sorted = op[i].off;

	/* sort by priority */
	qsort ((void *)op, nop, sizeof(Obs), obssort_priority);

	/* perform the basic assignment algorithm on blocks of successively
	 * worse priority
	 */
	pop = op;
	do {
	    int p = pop->scan.priority;
	    int npop = 1;
	    while (&pop[npop] < &op[nop] && pop[npop].scan.priority == p)
		npop++;
	    sortscans_p (pop, npop, lngcor, today);
	    pop += npop;
	} while (pop < &op[nop]);

	/* now that utcstart is set for all obs we care about sort the real
	 * op[] array again to order everything by start time.
	 */
	qsort ((void *)op, nop, sizeof(Obs), obssort_utc);

	free ((char *)slots);
}

/* return the elapsed time from time t1 to time t2, each in hours.
 * allow for wrapping at 24 hours.
 */
double
elapsedt (t1, t2)
double t1, t2;
{
        double d = t2 - t1;

	if (d < -12) d += 24;
	if (d > 12)  d -= 24;
	return (d);
}


/* return 1 if t2 is strictly between t1 and t3, else 0 */
int
in_timeorder (t1, t2, t3)
double t1, t2, t3;
{
	double dt12, dt23;

	dt12 = t2 - t1;
	if (t2 < t1) dt12 += 24;
	dt23 = t3 - t2;
	if (t3 < t2) dt23 += 24;
	return (dt12 + dt23 < 24);
}

/* return 1 if the given utc occurs between dusk and dawn else 0
 * this is done as: utc is after dusk and dawn is after utc.
 */
int
at_night (utc)
double utc;
{
	double mjddawn, mjddusk;
	int utcslot;

	dawnduskToday (&mjddawn, &mjddusk);
	utcdawn = mjd_hr (mjddawn);
	utcdusk = mjd_hr (mjddusk);
	dawnslot = SLOTNUM(utcdawn);
	utcslot = SLOTNUM (utc);

	/* this assumes duskslot would be 0 */
	return (utcslot <= dawnslot);
}

/* if the given object is an "evening" object, ie, dusk occurs
 * sometime between when it transits and when it sets, then return the number
 * of slots its sets after dusk; else return 0.
 * transit < dusk < set
 */
int
is_evening (op)
Obs *op;
{
	int setslot, translot;

	if (op->rs.rs_flags & RS_ODD)
	    return (0);

	/* arrange all times such that rise < trans < set */
	translot = SLOTNUM(mjd_hr(op->rs.rs_trantm));
	setslot  = SLOTNUM(mjd_hr(op->rs.rs_settm));
	if (setslot < translot)
	    setslot += NSLOTS;

	/* duskslot is 0 by defn, but to test range wrap forward */
	if (/* translot < NSLOTS && */ NSLOTS < setslot)
	    return (setslot - NSLOTS);
	else
	    return (0);
}

/* if the given object is a "morning" object, ie, dawn occurs
 * sometime between when it rises and when it transits, then return the number
 * of slots its rises before dawn; else return 0.
 * rise < dawn < transit
 */
int
is_morning (op)
Obs *op;
{
	int riseslot, translot;
	double mjddawn, mjddusk;

	if (op->rs.rs_flags & RS_ODD)
	    return (0);

	dawnduskToday (&mjddawn, &mjddusk);
	utcdawn = mjd_hr (mjddawn);
	utcdusk = mjd_hr (mjddusk);

	/* arrange all times such that rise < trans < set */
	dawnslot = SLOTNUM(utcdawn);
	translot = SLOTNUM(mjd_hr(op->rs.rs_trantm));
	riseslot = SLOTNUM(mjd_hr(op->rs.rs_risetm));
	if (riseslot > translot)
	    riseslot -= NSLOTS;

	if (riseslot < dawnslot && dawnslot < translot)
	    return (dawnslot - riseslot);
	else
	    return (0);
}

/* return 1 if the given object is a "night" object, ie, transits
 * sometime between dusk and dawn.
 * else return 0.
 * dusk < transit < dawn 
 */
int
is_night (op)
Obs *op;
{
	double mjddawn, mjddusk;
	int translot;

	if (op->rs.rs_flags & RS_ODD)
	    return (0);

	dawnduskToday (&mjddawn, &mjddusk);
	utcdawn = mjd_hr (mjddawn);
	utcdusk = mjd_hr (mjddusk);

	dawnslot = SLOTNUM(utcdawn);
	translot = SLOTNUM(mjd_hr(op->rs.rs_trantm));

	/* this works because duskslot is == 0 */
	return (translot < dawnslot);
}

/* sort a subportion of the overall obs array which are all known to be at the
 * same priority.
 */
static void
sortscans_p (pop, npop, lngcor, today)
Obs *pop;
int npop;
double lngcor;
double today;
{
	Obs **xop = (Obs **) calloc (npop, sizeof(Obs *)); /* extract list */
	Obs *candidate[NSLOTS];				   /* tmp slots */
	int nxop;
	int i;

	/* zero out */
	memset ((void *)candidate, 0, sizeof(candidate));

	/* get all unsorted objects with LSTSTART set into xop and merge into
	 *   slots.
	 * this also sets utcstart from lststart along the way.
	 * if can't fit, turn off and set utcstart to ut based on lststart.
	 */
	nxop = get_lststart (pop, npop, lngcor, today, xop);
	for (i = 0; i < nxop; i++) {
	    Obs *op = xop[i];
	    int nslots = SECS2SLOTS(op->scan.dur);
	    int wantslot = SLOTNUM (op->utcstart);
	    int gotslot;

	    /* simply search around anchor */
	    gotslot = srch_specific (wantslot, nslots, slots, 1);

	    /* only allow to slip by no more than startdt */
	    if (gotslot < 0) {
		op->sorted = 1;
		op->off = 1;
		ACPYZ (op->yoff, "Schedule is too full");
	    } else if (abs(gotslot - wantslot)*SECSPERSLOT > op->scan.startdt) {
		op->sorted = 1;
		op->off = 1;
		ACPYZ (op->yoff, "Nothing available within tolerance");
	    } else
		slot_fill (gotslot, nslots, op);
	}

	/* get all unsorted evening objects into xop, sorted by increasing set
	 *   time.
	 * then merge into slot[] and set utcstart based on the slot found.
	 * if can't fit, turn off and set utcstart to dusk
	 */
	nxop = get_evening (pop, npop, xop);
	for (i = 0; i < nxop; i++) {
	    Obs *op = xop[i];
	    int nslots = SECS2SLOTS(op->scan.dur);
	    int gotslot;

	    slots_copy (candidate, slots, NSLOTS);
	    obs_blockout (candidate, op);
	    gotslot = srch_forwards (duskslot, nslots, candidate, 0);
	    if (gotslot < 0) {
		op->sorted = 1;
		op->off = 1;
		op->utcstart = utcdusk;
		ACPYZ (op->yoff, "No times left for evening object");
	    } else
		slot_fill (gotslot, nslots, op);
	}

	/* get all unsorted morning objects into xop, sorted by decreasing
	 *   rise time.
	 * then merge into slot[] and set utcstart based on the slot found.
	 * if can't fit, turn off and set utcstart to dawn
	 */
	nxop = get_morning (pop, npop, xop);
	for (i = 0; i < nxop; i++) {
	    Obs *op = xop[i];
	    int nslots = SECS2SLOTS(op->scan.dur);
	    int gotslot;

	    slots_copy (candidate, slots, NSLOTS);
	    obs_blockout (candidate, op);
	    gotslot = srch_backwards (dawnslot, nslots, candidate, 0);
	    if (gotslot < 0) {
		op->sorted = 1;
		op->off = 1;
		op->utcstart = utcdawn;
		ACPYZ (op->yoff, "No times left for morning object");
	    } else
		slot_fill (gotslot, nslots, op);
	}

	/* get all unsorted objects that transit between dusk and dawn into xop.
	 * then merge into slot[] and set utcstart based on the slot found.
	 * if can't fit, turn off and set utcstart to transit time.
	 */
	nxop = get_night (pop, npop, xop);
	for (i = 0; i < nxop; i++) {
	    Obs *op = xop[i];
	    int nslots = SECS2SLOTS(op->scan.dur);
	    int gotslot;

	    slots_copy (candidate, slots, NSLOTS);
	    obs_blockout (candidate, op);
	    gotslot = srch_specific(SLOTNUM(mjd_hr(op->rs.rs_trantm)), nslots,
								candidate, 0);
	    if (gotslot < 0) {
		op->sorted = 1;
		op->off = 1;
		op->utcstart = mjd_hr(op->rs.rs_trantm);
		ACPYZ (op->yoff, "No times left for night object");
	    } else
		slot_fill (gotslot, nslots, op);
	}

	/* get all unsorted circumpolar objects into xop.
	 * then merge into slot[] starting at middle of the night.
	 * if can't fit, turn off and set utcstart to transit time.
	 */
	nxop = get_circumpolar (pop, npop, xop);
	for (i = 0; i < nxop; i++) {
	    Obs *op = xop[i];
	    int nslots = SECS2SLOTS(op->scan.dur);
	    int gotslot, transslot;
	    
	    slots_copy (candidate, slots, NSLOTS);
	    obs_blockout (candidate, op);
	    transslot = SLOTNUM(mjd_hr(op->rs.rs_trantm));
	    if (duskslot <= transslot && transslot <= dawnslot) {
		/* transits at night */
	        gotslot = srch_specific(transslot, nslots, candidate, 0);
	    } else if (transslot - dawnslot < NSLOTS - transslot) {
		/* transits nearer dawn than dusk */
		gotslot = srch_backwards (dawnslot, nslots, candidate, 0);
	    } else {
		/* transits nearer dusk than dawn */
		gotslot = srch_forwards (duskslot, nslots, candidate, 0);
	    }

	    if (gotslot < 0) {
		op->sorted = 1;
		op->off = 1;
		op->utcstart = mjd_hr(op->rs.rs_trantm);
		ACPYZ (op->yoff, "No times left for circumpolar object");
	    } else
		slot_fill (gotslot, nslots, op);
	}

	/* turn off everything that is not sorted by now */
	for (i = 0; i < npop; i++) {
	    Obs *op = &pop[i];
	    if (!op->sorted) {
		op->sorted = 1;
		op->off = 1;
		if (op->rs.rs_flags & RS_NEVERUP)
		    ACPYZ (op->yoff, "Never up");
		else
		    ACPYZ (op->yoff, "Never up at night");
	    }
	}

	free ((char *)xop);
}

/* search through op[] for all unsorted entries with lststart set and add to
     xop.
 * N.B. if we find lststart, we also set utcstart.
 * return the number of such.
 */
static int
get_lststart (op, nop, lngcor, today, xop)
Obs op[];
int nop;
double lngcor, today;	/* used to compute utcs from lst */
Obs *xop[];
{
	int nxop;

	for (nxop = 0; --nop >= 0; op++)
	    if (!op->sorted && op->lststart != NOTIME) {
		double olst = op->lststart;
		olst -= lngcor;
		range (&olst, 24.0);
		gst_utc (today, olst, &op->utcstart);
		xop[nxop++] = op;
	    }

	return (nxop);
}

/* search through op[] for all unsorted entries and lststart not set such that
 *   transit < dusk < set.
 * sort and return the number of such.
 * be mindful that many objects are both morning and evening; choose the side
 * with the most gap.
 */
static int
get_evening (op, nop, xop)
Obs op[];
int nop;
Obs *xop[];
{
	int nxop;

	for (nxop = 0; --nop >= 0; op++)
	    if (!op->sorted && op->lststart == NOTIME) {
		int e = is_evening (op);
		if (e > 0) {
		    int m = is_morning (op);
		    if (m == 0 || e > m)
			xop[nxop++] = op;
		}
	    }

	qsort ((void *)xop, nxop, sizeof(Obs *), obspsort_settm);
	return (nxop);
}

/* search through op[] for all unsorted entries and lststart not set such that
 *    rise < dawn < transit.
 * sort and return the number of such.
 * be mindful that many objects are both morning and evening; choose the side
 * with the most gap.
 */
static int
get_morning (op, nop, xop)
Obs op[];
int nop;
Obs *xop[];
{
	int nxop;

	for (nxop = 0; --nop >= 0; op++)
	    if (!op->sorted && op->lststart == NOTIME) {
		int m = is_morning (op);
		if (m > 0) {
		    int e = is_evening(op);
		    if (e == 0 || m > e)
			xop[nxop++] = op;
		}
	    }

	qsort ((void *)xop, nxop, sizeof(Obs *), obspsort_risetm);
	return (nxop);
}

/* search through op[] for all unsorted entries and lststart not set such that
 *   dusk < transit < dawn.
 * return the number of such.
 */
static int
get_night (op, nop, xop)
Obs op[];
int nop;
Obs *xop[];
{
	int nxop;

	for (nxop = 0; --nop >= 0; op++)
	    if (!op->sorted && op->lststart == NOTIME && is_night (op))
		xop[nxop++] = op;

	return (nxop);
}

/* search through op[] for all unsorted entries and lststart not set that
 *   never set.
 * return the number of such.
 */
static int
get_circumpolar (op, nop, xop)
Obs op[];
int nop;
Obs *xop[];
{
	int nxop;

	for (nxop = 0; --nop >= 0; op++)
	    if (!op->sorted && op->lststart == NOTIME
					&& (op->rs.rs_flags & (RS_CIRCUMPOLAR)))
		xop[nxop++] = op;

	return (nxop);
}

/* start at a[start] and search backwards to find the index of the first gap of
 * dur empty slots. take care to wrap at NSLOTS. if iglight, take slots
 * marked with OPLIGHT as being available.
 * return its index else -1 if none.
 */
static int
srch_backwards (start, dur, a, iglight)
int start, dur;
Obs *a[NSLOTS];
int iglight;
{
	int i;		/* a[] index */
	int n = 0;	/* length of hole found so far */

	i = start;
	do {
	    if (a[i] && (!iglight || a[i] != OPLIGHT))
		n = 0;
	    else
		n++;
	    if (n >= dur)
		return (i);
	    i = SLOTMOD(i-1);
	} while (i != start);

	return (-1);
}

/* start at a[start] and search forwards to find the index of the first gap of
 * dur empty slots. take care to wrap at NSLOTS. if iglight, take slots
 * marked with OPLIGHT as being available.
 * return its index else -1 if none.
 */
static int
srch_forwards (start, dur, a, iglight)
int start, dur;
Obs *a[NSLOTS];
int iglight;
{
	int i;		/* a[] index */
	int n = 0;	/* length of hole found so far */

	i = start;
	do {
	    if (a[i] && (!iglight || a[i] != OPLIGHT))
		n = 0;
	    else
		n++;
	    if (n >= dur)
		return (SLOTMOD(i-(n-1)));	/* index of beginning of hole */
	    i = SLOTMOD(i+1);
	} while (i != start);

	return (-1);
}

/* start at a[start] and search forwards and backwards for the index
 * of the first gap of dur empty slots. if iglight, take slots
 * marked with OPLIGHT as being available.
 * return its index else -1 if none.
 */
static int
srch_specific (start, dur, a, iglight)
int start, dur;
Obs *a[NSLOTS];
int iglight;
{
	int iback, iforw;

	iback = srch_backwards (start, dur, a, iglight);
	iforw = srch_forwards (start, dur, a, iglight);
	if (iback < 0 && iforw < 0)
	    return (-1);	/* neither way found anything */
	if (iback < 0)
	    return (iforw);	/* only slot was forwards */
	if (iforw < 0)
	    return (iback);	/* only slot was backwards */

	/* both good; use shortest, allowing for wrap */
	if (iback > start)
	    iback -= NSLOTS;
	if (iforw < start)
	    iforw += NSLOTS;

	if (start - iback < iforw - start)
	    return (SLOTMOD(iback));
	else
	    return (SLOTMOD(iforw));
}

/* copy nobs from src to dst */
static void
slots_copy (dst, src, nobs)
Obs **dst, **src;
int nobs;
{
	while (--nobs >= 0)
	    *dst++ = *src++;
}

/* fill the given slots array from first through last with op.
 * N.B. if first > last then start at first, go to end and wrap to front.
 */
static void
slots_blockout (a, first, last, op)
Obs *a[NSLOTS];
int first, last;
Obs *op;
{
	if (first < 0 || first >= NSLOTS || last < 0 || last >= NSLOTS) {
	    fprintf (stderr, "slots_blockout(...,%d,%d)\n", first, last);
	    exit(1);
	}

	if (first > last) {
	    while (first < NSLOTS)
		a[first++] = op;
	    first = 0;
	}

	while (first <= last)
	    a[first++] = op;
}

/* save start in op, then set the n slots[] starting with start to op and set
 * op->sorted to true.
 * N.B. take care at NSLOTS.
 */
static void
slot_fill (start, n, op)
int start, n;
Obs *op;
{
	op->utcstart = SLOTUTC(start);
	while (--n >= 0)
	    slots[(start++)%NSLOTS] = op;
	op->sorted = 1;
}

/* qsort function for an array of Obs.
 * sort by increasing utcstart.
 * we use slot numbers so dusk comes out at the head of the resulting list.
 * N.B. don't use SLOTNUM to avoid rounding to the same slot.
 */
static int
obssort_utc (item0, item1)
const void  *item0, *item1;
{
	double utc0 = ((Obs *)item0)->utcstart;
	double utc1 = ((Obs *)item1)->utcstart;
	
	if (utc0 < utc1)
	    return (-1);
	if (utc0 > utc1)
	    return (1);
	return (0);

}

/* qsort function for an array of Obs.
 * sort by increasing priority.
 * we use slot numbers so dusk comes out at the head of the resulting list.
 */
static int
obssort_priority (item0, item1)
const void  *item0, *item1;
{
	Obs *op0 = (Obs *)item0;
	Obs *op1 = (Obs *)item1;

	return (op0->scan.priority - op1->scan.priority);
}

/* qsort function for an array of Obs *.
 * sort by decreasing rise time.
 * N.B. we assume the objects do indeed have a rs.rs_risetm value
 */
static int
obspsort_risetm (item0, item1)
const void *item0, *item1;
{
	Obs **op0 = (Obs **)item0;
	Obs **op1 = (Obs **)item1;

	return (SLOTNUM(mjd_hr((*op1)->rs.rs_risetm)) -
					SLOTNUM(mjd_hr((*op0)->rs.rs_risetm)));
}

/* qsort function for an array of Obs *.
 * sort by increasing set time.
 * N.B. we assume the objects do indeed have a rs.rs_risetm value
 */
static int
obspsort_settm (item0, item1)
const void *item0, *item1;
{
	Obs **op0 = (Obs **)item0;
	Obs **op1 = (Obs **)item1;

	return (SLOTNUM(mjd_hr((*op0)->rs.rs_settm)) -
					SLOTNUM(mjd_hr((*op1)->rs.rs_settm)));
}

/* block out the slots in oa[] for which op should not be scheduled.
 * this includes suff like op is set and scope limitations.
 */
static void
obs_blockout (Obs **oa, Obs *op)
{
	double zhole = PI/2 - MAXALT;

	/* skip pure camera cal objects -- their locations are irrevalant */
	if (op->scan.ccdcalib.data == CD_NONE)
	    return;

	/* block out when too low */
	if (!(op->rs.rs_flags & RS_ODD))
	    slots_blockout (oa, SLOTNUM(mjd_hr(op->rs.rs_settm)),
				    SLOTNUM(mjd_hr(op->rs.rs_risetm)), op);
	dump_slots (op->scan.obj.o_name, oa, NSLOTS);

	/* block out when outside HA range */
	slots_blockout (oa, SLOTNUM(lst2utc(radhr(op->scan.obj.s_ra+MAXHA))),
		       SLOTNUM(lst2utc(radhr(op->scan.obj.s_ra-MAXHA))), op);
	dump_slots (op->scan.obj.o_name, oa, NSLOTS);

	/* block out when in zenith hole */
	if (fabs (op->scan.obj.s_dec - now.n_lat) < zhole) {
	    /* passes through hole.. now block out either side of transit */
	    double start, stop;
	    double a, b;

	    a = zhole;				/* zenith hole radius */
	    b = PI/2 - now.n_lat;		/* pole to zenith */

	    if (b <= a) {
		/* _always_ in the hole :-( */
		start = 0;
		stop = 23.99;
	    } else {
		/* find polar angle from zenith to worst edge of zenith hole.
		 * scale this to 2 hours to know time within hole.
		 */
		double transit = mjd_hr(op->rs.rs_trantm);
		double c = PI/2 - op->scan.obj.s_dec;/*pole to worst hole edge*/
		double cA, dt;

		cA = (cos(a) - cos(b)*cos(c))/(sin(b)*sin(c));
		if (cA >  1.0) cA =  1.0;
		if (cA < -1.0) cA = -1.0;
		dt = 24.*acos(cA)/(2*PI);	/* half time op is in hole */
		/* block out either side of transit */
		start = transit - dt;
		range (&start, 24.0);
		stop = transit + dt;
		range (&stop, 24.0);
	    }

	    slots_blockout (oa, SLOTNUM(start), SLOTNUM(stop), op);
	    dump_slots (op->scan.obj.o_name, oa, NSLOTS);
	}
}

/* go through slots array s[n] and print ranges where value changes */
#ifndef DUMP_SLOTS
/* ARGSUSED */
#endif
static void
dump_slots (name, s, n)
char *name;
Obs **s;
int n;
{
#ifdef DUMP_SLOTS
	int i;
	int last;

	last = -1;
	printf ("%13s: ", name);
	for (i = 0; i < n; i++) {
	    int current = *s++ != 0;
	    if (current != last)
		printf (" %g/%s", SLOTUTC(i), current ? "On " : "Off");
	    last = current;
	}
	printf ("\n");
#endif
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: sortscans.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
