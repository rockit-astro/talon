#include <stdio.h>
#include <ctype.h>
#include <math.h>


#if defined(__STDC__)
#include <stdlib.h>
#include <string.h>
typedef const void * qsort_arg;
#else
typedef void * qsort_arg;
extern void *malloc(), *realloc();
#endif

#if defined(_POSIX_SOURCE)
#include <unistd.h>
#else
extern int read();
extern int close();
#endif

#ifndef SEEK_SET
#define	SEEK_SET 0
#define	SEEK_END 2
#endif

#include "P_.h"
#include "astro.h"
#include "circum.h"

extern FILE *fopenh P_((char *name, char *how));
extern Now *mm_get_now P_((void));
extern char *syserrstr P_((void));
extern int db_read P_((FILE *fp, int nodups));
extern double delra P_((double dra));
extern int openh P_((char *name, int flags, ...));
extern void all_newdb P_((int appended));
extern void db_update P_((Obj *op));
extern void pm_set P_((int p));
extern void watch_cursor P_((int want));
extern void xe_msg P_((char *msg, int app_modal));

static int db_add P_((Obj *newop));
static void db_init P_((void));

#define	MAXDBLINE	256	/* longest allowed db line */

#define	DBFIFO_MSG	'!'	/* introduces a message line from the DBFIFO */

/* This counter is incremented when we want to mark all the Obj derived entries
 * as being out-of-date. This works because then each of the age's will be !=
 * db_age. This is to eliminate ever calling obj_cir() under the same
 * circumstances for a given db object.
 * N.B. For this to work, call db_update() before using a Obj *.
 */
static ObjAge_t db_age;

/* the "database".
 * one such struct per object type. the space for objects is malloced in
 *   seperate chunks of DBCHUNK to avoid needing big contiguous memory blocks.
 * N.B. because the number is fixed and known, we use static storage for the
 *   NOBJ Objects for the PLANET type; see db_init().
 */
#define	DBCHUNK	256	/* number we malloc more of at once; a power of two
			 * might help the compiler optimize the divides and
			 * modulo arithmetic.
			 */
typedef struct {
    char **dblist;	/* malloced list of malloced DBCHUNKS arrays */
    int nobj;		/* number of objects actually in use */
    int nmem;		/* total number of objects for which we have room */
    int ncp;		/* nobj at time of last checkpoint */
    int size;		/* bytes per object */
} DBMem;
static DBMem db[NOBJTYPES];	/* this is the head for each object */
#define	DBINITED	(db[PLANET].dblist)	/* anything setup by db_init()*/

/* macro that returns the address of an object given its type and index */
#define	OBJP(t,n)	\
		((Obj *)(db[t].dblist[(n)/DBCHUNK] + ((n)%DBCHUNK)*db[t].size))

static void db_free P_((DBMem *dmp));

typedef char MaxNm[MAXNM];	/* eases readability of an array of these */

/* return number of objects in the database.
 * this includes the NOBJ basic objects.
 * N.B. this is expected to be inexpensive to call.
 */
int
db_n()
{
	DBMem *dmp;
	int n;

	if (!DBINITED)
	    db_init();

	for (n = 0, dmp = db; dmp < &db[NOBJTYPES]; dmp++)
	    n += dmp->nobj;
	return (n);
}

/* return the total current number of objects as of the last checkpoint.
 * this includes the NOBJ basic objects.
 */
int
db_n_cp()
{
	DBMem *dmp;
	int n;

	if (!DBINITED)
	    db_init();

	for (n = 0, dmp = db; dmp < &db[NOBJTYPES]; dmp++)
	    n += dmp->ncp;
	return (n);
}

/* given one of the basic ids in astro.h return pointer to its updated Obj in
 * the database.
 */
Obj *
db_basic(id)
int id;
{
	Obj *op;

	if (!DBINITED)
	    db_init();

	if (id < 0 || id >= NOBJ) {
	    printf ("db_basic(): bad id: %d\n", id);
	    exit (1);
	}

	op = OBJP(PLANET,id);
	if (op->o_type != UNDEFOBJ)
	    db_update(op);
	return (op);
}

/* set our official copy of the given user-defined object to *op */
void
db_setuserobj(id, op)
int id;	/* OBJXYZ */
Obj *op;
{
	if (id == OBJX || id == OBJY || id == OBJZ) {
	    Obj *bop = OBJP(PLANET,id);
	    *bop = *op;
	} else {
	    printf ("db_setuserobj(): bad id: %d\n", id);
	    exit (1);
	}
}

/* mark all db objects as old
 */
void
db_invalidate()
{
	if (!DBINITED)
	    db_init();

	db_age++;	/* ok if wraps */
}

/* initialize the given DBScan for a database scan. mask is a collection of
 *   *M masks for the desired types of objects. op/nop describe a list of
 *   ObjF which will also be scanned in addition to what is in the database.
 * the idea is to call this once, then repeatedly call db_scan() to get all
 *   objects in the db of those types, then those is op (if any).
 * return NULL when there are no more.
 * N.B. nothing should be assumed as to the order these are returned.
 */
void
db_scaninit (sp, mask, op, nop)
DBScan *sp;
int mask;
ObjF *op;
int nop;
{
	if (!DBINITED)
	    db_init();

	sp->t = 0;
	sp->n = 0;
	sp->m = mask;
	sp->op = op;
	sp->nop = nop;
}

/* fetch the next object.
 * N.B. the s_ fields are *not* updated -- call db_update() when you need that.
 */
Obj *
db_scan (sp)
DBScan *sp;
{
	Obj *op;

	if (!DBINITED)
	    db_init();

    	/* check op/nop only after everything else is complete */
    doop:
	if (sp->t == NOBJTYPES) {
	    if (sp->op && sp->n < sp->nop)
		return ((Obj*)&sp->op[sp->n++]);
	    else
		return (NULL);
	}

	/* outter loop is just to skip over undefined user objects, if any. */
	do {
	    /* advance to the next type if we have scanned all the ones of
	     * the current type or if the type is not desired.
	     */
	    while (sp->n >= db[sp->t].nobj || !((1<<sp->t) & sp->m)) {
		sp->n = 0;
		if (++sp->t == NOBJTYPES) {
		    /* no more in the real db -- try op */
		    goto doop;
		}
	    }

	    op = OBJP(sp->t, sp->n);
	    sp->n++;
	} while (op->o_type == UNDEFOBJ);

	return (op);
}

/* see to it that all the s_* fields in the given db object are up to date.
 * always recompute the user defined objects because we don't know when
 * they might have been changed.
 * N.B. it is ok to call this even if op is not actually in the database
 *   although we guarantee an actual update occurs if it's not.
 */
void
db_update(op)
Obj *op;
{
	static char me[] = "db_update()";

	if (!DBINITED)
	    db_init();

	if (op->o_type == UNDEFOBJ) {
	    printf ("%s: called with UNDEFOBJ pointer\n", me);
	    exit (1);
	} 
	if ((int)op->o_type >= NOBJTYPES) {
	    printf ("%s: called with bad pointer\n", me);
	    exit (1);
	} 

	if (op->o_age != db_age ||
		    op == OBJP(PLANET,OBJX) || op == OBJP(PLANET,OBJY ||
					       op == OBJP(PLANET,OBJZ))) {
	    if (obj_cir (mm_get_now(), op) < 0) {
		printf ("%s: bad object\n", me);
		exit(1);
	    }
	    op->o_age = db_age;
	}
}

/* delete all but the basic objects.
 */
void
db_del_all()
{
	DBMem *dmp;

	if (!DBINITED)
	    db_init();

	for (dmp = db; dmp < &db[NOBJTYPES]; dmp++) {
	    /* N.B. PLANET entries are fixed -- not malloced */
	    if (dmp == &db[PLANET])
		continue;
	    db_free (dmp);
	}
}

/* delete all objects back to the last checkpoint.
 * we don't actually free memory.
 */
void
db_del_cp ()
{
	DBMem *dmp;

	if (!DBINITED)
	    db_init();

	for (dmp = db; dmp < &db[NOBJTYPES]; dmp++)
	    dmp->nobj = dmp->ncp;
}

/* set the checkpoint to the current set of objects in the db.
 */
void
db_set_cp()
{
	DBMem *dmp;

	if (!DBINITED)
	    db_init();

	for (dmp = db; dmp < &db[NOBJTYPES]; dmp++)
	    dmp->ncp = dmp->nobj;
}

/* set the checkpoints back to 0.
 * N.B. never fiddle with PLANET's count.
 */
void
db_clr_cp()
{
	DBMem *dmp;

	if (!DBINITED)
	    db_init();

	for (dmp = db; dmp < &db[NOBJTYPES]; dmp++) {
	    /* N.B. PLANET entries are fixed -- not malloced */
	    if (dmp != &db[PLANET])
		dmp->ncp = 0;
	}
}

/* compare 2 pointers to MaxNm's in qsort fashion */
static int
name_cmpf (s1, s2)
qsort_arg s1, s2;
{
	return (strcmp (*(MaxNm *)s1, *(MaxNm *)s2));
}

/* see whether name is in names[nnames] (which is sorted in ascending order).
 * if so, return 0, else -1
 */
static int
chk_name (name, names, nnames)
char *name;
MaxNm *names;
int nnames;
{
	int t, b, m, s;

	/* check for no names at all */
	if (nnames <= 0)
	    return (-1);

	/* binary search */
	t = nnames - 1;
	b = 0;
	while (b <= t) {
	    m = (t+b)/2;
	    s = strcmp (name, names[m]);
	    if (s == 0)
		return (0);
	    if (s < 0)
		t = m-1;
	    else
		b = m+1;
	}

	return (-1);
}

/* read the given database file into memory.
 * add to the existing list.
 * if nodups, skip objects whose name already appears in memory.
 * stop gracefully if we run out of memory.
 * return 0 if all ok, else -1.
 */
int
db_read (fp, nodups)
FILE *fp;
int nodups;
{
	char buf[MAXDBLINE];
	MaxNm *names = 0;
	int nnames = 0;
	long len;

	if (!DBINITED)
	    db_init();

	/* set up to run the progress meter based on file position */
	(void) fseek (fp, 0L, SEEK_END);
	len = ftell (fp);
	(void) fseek (fp, 0L, SEEK_SET);
	pm_set (0);

	if (nodups) {
	    /* build sorted list of existing names. 
	     * N.B. actually copy the names in case the real db moves.
	     */
	    DBScan dbs;
	    int mask = ALLM;
	    Obj *op;

	    nnames = 0;
	    names = (MaxNm *) malloc (db_n() * sizeof(MaxNm));
	    if (!names) {
		xe_msg ("No memory to check dups", 1);
		return (-1);
	    }
	    for (db_scaninit(&dbs,mask,NULL,0); (op = db_scan(&dbs)) != NULL; )
		(void) strcpy (names[nnames++], op->o_name);
	    if (nnames > db_n()) {
		printf ("db_read: too many objects! %d %d\n", db_n(), nnames);
		exit (1);
	    }

	    qsort ((void *)names, nnames, sizeof(MaxNm), name_cmpf);
	}

	/* read each line from the file and add good ones to the db */
	while (fgets (buf, sizeof(buf), fp)) {
	    char whynot[256];
	    Obj o;

	    pm_set ((int)(ftell(fp)*100/len)); /* update progress meter */

	    if (db_crack_line (buf, &o, whynot) < 0) {
		if (whynot[0] != '\0')
		    printf ("Bad db: %s: %s\n", buf, whynot);
		continue;
	    }

	    if (nodups) {
		char c, nm[MAXNM];
		int i;

		for (i = 0; i < (MAXNM-1) && (c = buf[i]) != ','; i++)
		    nm[i] = c;
		nm[i] = '\0';
		if (chk_name (nm, names, nnames) == 0)
		    continue;
	    }

	    if (db_add (&o) < 0)
		break;
	}

	/* finished with names */
	if (names)
	    free ((void *)names);

	/* should have hit eof -- if not, other trouble */
	return (feof(fp) ? 0 : -1);
}

/* allocate *newop to the appropriate list, growing if necessary.
 * return 0 if ok, -1 if no more memory.
 * N.B we do *not* validate newop in any way.
 */
static int
db_add (newop)
Obj *newop;
{
	int t = newop->o_type;
	DBMem *dmp = &db[t];
	Obj *op;

	/* allocate another chunk if this type can't hold another one */
	if (dmp->nmem <= dmp->nobj) {
	    int ndbl = dmp->nmem/DBCHUNK;
	    int newdblsz = (ndbl + 1) * sizeof(char *);
	    char **newdbl;
	    char *newchk;

	    newdbl = dmp->dblist ? (char **) realloc (dmp->dblist, newdblsz)
				 : (char **) malloc (newdblsz);
	    if (!newdbl)
		return (-1);

	    newchk = malloc (dmp->size * DBCHUNK);
	    if (!newchk) {
		free ((char *)newdbl);
		return (-1);
	    }

	    newdbl[ndbl] = newchk;

	    dmp->dblist = newdbl;
	    dmp->nmem += DBCHUNK;
	}

	op = OBJP (t, dmp->nobj);
	dmp->nobj++;
	memcpy ((void *)op, (void *)newop, dmp->size);

	return (0);
}

/* set up the basic database.
 */
static void
db_init()
{
	/* these must match the order in astro.h */
	static char *planet_names[] = {
	    "Mercury", "Venus", "Mars", "Jupiter", "Saturn",
	    "Uranus", "Neptune", "Pluto", "Sun", "Moon",
	};
	static Obj plan_objs[NOBJ];	/* OBJXYZ can be anything so use Obj */
	static char *plan_dblist[1] = {(char *)plan_objs};
	int i;

	/* init the object sizes.
	 * N.B. must do this before using the OBJP macro
	 */
	db[UNDEFOBJ].size	= 0;
	db[FIXED].size		= sizeof(ObjF);
	db[ELLIPTICAL].size	= sizeof(ObjE);
	db[HYPERBOLIC].size	= sizeof(ObjH);
	db[PARABOLIC].size	= sizeof(ObjP);
	db[EARTHSAT].size	= sizeof(ObjES);
	db[PLANET].size		= sizeof(Obj); /* *NOT* ObjPl */

	/* init the planets.
	 * because the number is fixed, we use static storage for the NOBJ
	 * Objects for the PLANET type.
	 */
	db[PLANET].dblist = plan_dblist;
	db[PLANET].nmem = NOBJ;
	db[PLANET].nobj = NOBJ;
	db[PLANET].ncp = NOBJ;
	for (i = MERCURY; i <= MOON; i++) {
	    Obj *op = OBJP (PLANET, i);
	    op->o_type = PLANET;
	    (void) strncpy (op->o_name, planet_names[i], sizeof(op->o_name)-1);
	    op->pl.pl_code = i;
	}
}

/* free all the memory associated with the given DBMem */
static void
db_free (dmp)
DBMem *dmp;
{
	if (dmp->dblist) {
	    int i;
	    for (i = 0; i < dmp->nmem/DBCHUNK; i++)
		free (dmp->dblist[i]);
	    free ((char *)dmp->dblist);
	    dmp->dblist = NULL;
	}

	dmp->nobj = 0;
	dmp->nmem = 0;
	dmp->ncp = 0;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: db.c,v $ $Date: 2001/04/19 21:12:06 $ $Revision: 1.1.1.1 $ $Name:  $"};
