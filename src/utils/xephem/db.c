/* code to manage what the outside world sees as the db_ interface.
 */

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
extern char *strchr();
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

#include <X11/Intrinsic.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "db.h"

extern XtAppContext xe_app;

extern FILE *fopenh P_((char *name, char *how));
extern Now *mm_get_now P_((void));
extern char *getPrivateDir P_((void));
extern char *getShareDir P_((void));
extern char *getXRes P_((char *name, char *def));
extern char *syserrstr P_((void));
extern double delra P_((double dra));
extern int openh P_((char *name, int flags, ...));
extern int db_load1 P_((void));
extern void all_newdb P_((int appended));
extern void db_read P_((char *fn, int nodups));
extern void db_update P_((Obj *op));
extern void obj_set P_((Obj *op, int dbidx));
extern void pm_set P_((int p));
extern void setXRes P_((char *name, char *val));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));

extern void watch_cursor P_((int want));
extern void xe_msg P_((char *msg, int app_modal));

static int db_objadd P_((Obj *newop));
static void db_init P_((void));
static void dbfifo_cb P_((XtPointer client, int *fdp, XtInputId *idp));
static DBCat *db_catadd P_((char *filename));

#define	MAXDBLINE	256	/* longest allowed db line */

#define	DBFIFO_MSG	'!'	/* introduces a message line from the DBFIFO */

/* Category for ours and dbmenu's resources in the Save system */
char dbcategory[] = "Data Base";

/* This counter is incremented when we want to mark all the Obj derived entries
 * as being out-of-date. This works because then each of the age's will be !=
 * db_age. This is to eliminate ever calling obj_cir() under the same
 * circumstances for a given db object.
 * N.B. For this to work, call db_update() not obj_cir().
 */
static ObjAge_t db_age = 1;	/* insure all objects are initially o-o-d */

/* the "database".
 * one such struct per object type. the space for objects is malloced in
 *   seperate chunks of DBCHUNK to avoid needing big contiguous memory blocks.
 *   the space is never freed, the total is just reduced and can be reused.
 * N.B. deleting by catalog depends on all objects from a given catalog being
 *   appended contiguously within their respective ObjType list.
 * N.B. because the number is fixed and known, we use static storage for the
 *   NOBJ Objects for the PLANET type, not malloced storage; see db_init().
 */
#define	DBCHUNK	256	/* number we malloc more of at once; a power of two
			 * might help the compiler optimize the divides and
			 * modulo arithmetic.
			 */
typedef struct {
    char **dblist;	/* malloced list of malloced DBCHUNKS arrays */
    int nobj;		/* number of objects actually in use */
    int nmem;		/* total number of objects for which we have room */
    int size;		/* bytes per object */
} DBMem;
static DBMem db[NOBJTYPES];	/* this is the head for each object */

/* return true if the database has been initialized */
#define	DBINITED	(db[PLANET].dblist)	/* anything setup by db_init()*/

/* macro that returns the address of an object given its type and index */
#define	OBJP(t,n)	\
		((Obj *)(db[t].dblist[(n)/DBCHUNK] + ((n)%DBCHUNK)*db[t].size))

/* keep track of the catalogs.
 * the array is kept sorted by name.
 */
static DBCat *dbcat;		/* malloced array, one for each loaded catalog*/
static int ndbcat;		/* number of entries in dbcat[] */
static char dbifres[] = "DBinitialFiles";	/* init files resource name */

/* Size quantizer to keep objects allocated in the heap worst-case-aligned.
 * This assumes that double will be the largest primitive data type
 * on all systems.  Not elegant but gets it done for the moment.
 * make it a no-op if you are sure your architecture won't benefit.
 * Credit to: Monty Brandenberg, mcbinc@world.std.com.
 */
#define DB_SIZE_ROUND(s)        \
			  (((s) + sizeof(double) - 1) & ~(sizeof(double) - 1))

/* db fifo fd and XtAddInput id.
 */
static int db_fifofd = -1;
static XtInputId db_fifoid;
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

/* load each file of objects listed in the DBinitialFiles resource
 * and inform all modules of the update.
 * support leading ~ and / else assume in ShareDir.
 */
void
db_loadinitial()
{
	char *fns;		/* value of DBinitialFiles */
	char dbicpy[2048];	/* local copy of dir */
	char *fnf[128];		/* ptrs into dbicpy[] at each ' ' */
	int nfn;
	int i;

	/* get the initial list of files, if any */
	fns = getXRes (dbifres, NULL);
	if (!fns)
	    return;

	/* work on a copy since we are about to break into fields */
	(void) sprintf (dbicpy, "%.*s", (int)sizeof(dbicpy)-1, fns);
	nfn = get_fields (dbicpy, ' ', fnf);
	if (nfn > XtNumber(fnf)) {
	    /* we exit because we've clobbered our stack by now!
	     * TODO: pass the size of fnf to get_fields().
	     */
	    printf ("Too many entries in %s. Max is %d\n", dbifres,
							    XtNumber(fnf));
	    exit (1);
	}

	/* read each catalog.
	 * N.B. get_fields() will return 1 even if there are no fields.
	 */
	for (i = 0; i < nfn && fnf[i][0] != '\0'; i++)
	    db_read (fnf[i], 1);

	/* all new */
	all_newdb(0);
}

/* make the current set of databases the new default */
static void
db_setinitial()
{
	DBCat *dbcp;
	char buf[2048];
	int l;

	l = 0;
	buf[0] = '\0';
	for (dbcp = dbcat; dbcp < &dbcat[ndbcat]; dbcp++)
	    l += sprintf (buf+l, " %s", dbcp->name);

	setXRes (dbifres, buf);
}

/* set our official copy of the given user-defined object to *op */
void
db_setuserobj(id, op)
int id;	/* OBJXYZ */
Obj *op;
{
	if (id == OBJX || id == OBJY || id == OBJZ) {
	    Obj *bop = OBJP(PLANET,id);
	    memcpy ((void *)bop, (void *)op, sizeof(Obj));
	} else {
	    printf ("db_setuserobj(): bad id: %d\n", id);
	    exit (1);
	}
}

/* search for a loaded catalog with the given name.
 * if find it return poiter to DBCat, else return NULL.
 */
DBCat *
db_catfind (name)
char *name;
{
	DBCat *dbcp;
	char *base;

	/* find just the basename */
	while (*name == ' ')
	    name++;
	for (base = name+strlen(name); base > name && base[-1] != '/'; --base)
	    continue;

	for (dbcp = dbcat; dbcp < &dbcat[ndbcat]; dbcp++)
	    if (!strcmp (dbcp->name, base))
		return (dbcp);
	return (NULL);
}

/* delete the entries associated with deldbcp then remove from list.
 */
void
db_catdel (deldbcp)
DBCat *deldbcp;
{
	DBCat *dbcp;
	int t, n;

	/* sanity check deldbcp is in dbcat[] */
	if (deldbcp < dbcat || deldbcp >= &dbcat[ndbcat]) {
	    printf ("Bug! Bad catdel deldbcp: %d\n", deldbcp - dbcat);
	    exit(1);
	}

	/* copy each Obj after the ones loaded by this catalog down over the
	 * entries from this catalog, then decrement the total
	 */
	for (t = 0; t < NOBJTYPES; t++) {
	    if ((n = deldbcp->n[t]) > 0) {
		int src = deldbcp->start[t] + n;
		int size = db[t].size;
		int nobj = db[t].nobj;

		/* copy separately as they might be in different chunks */
		while (src < nobj) {
		    memcpy (OBJP(t,src-n), OBJP(t,src), size);
		    src++;
		}
		db[t].nobj -= n;
	    }
	}

	/* adjust starts down by amounts in deldbcp if start after its start */
	for (dbcp = dbcat; dbcp < &dbcat[ndbcat]; dbcp++)
	    for (t = 0; t < NOBJTYPES; t++)
		if (dbcp->start[t] > deldbcp->start[t])
		    dbcp->start[t] -= deldbcp->n[t];

	/* remove deldbcp from dbcat.
	 * N.B. deldbcp not valid after this
	 */
	memcpy (deldbcp, deldbcp+1, (&dbcat[--ndbcat] - deldbcp)*sizeof(DBCat));

	/* update GUI */
	db_newcatmenu (dbcat, ndbcat);
	db_setinitial();
}

/* mark all db objects as out-of-date
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
		char buf[64];
		(void) sprintf (buf, "%s: no longer valid", op->o_name);
		xe_msg (buf, 0);
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

	/* free memory for each type */
	for (dmp = db; dmp < &db[NOBJTYPES]; dmp++) {
	    /* N.B. PLANET entries are fixed -- not malloced */
	    if (dmp == &db[PLANET])
		continue;	/* N.B. except planets! */
	    db_free (dmp);
	}

	/* no more catalogs */
	ndbcat = 0;
	db_newcatmenu (dbcat, ndbcat);
	db_setinitial();
}

/* compare 2 pointers to MaxNm's in qsort fashion */
static int
maxnm_cmpf (s1, s2)
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
 * add to the existing list of objects.
 * if nodups, skip objects whose name already appears in memory.
 * add a new catalog entry, sorted by catalog name, update GUI.
 * stop gracefully if we run out of memory.
 * keep operator informed.
 * look in several places for file.
 * if enabled and only one object in whole file, preload into an ObjXYZ.
 */
void
db_read (fn, nodups)
char *fn;
int nodups;
{
	char buf[MAXDBLINE];
	char fullfn[1024];
	char msg[1024];
	MaxNm *names = 0;
	DBCat *dbcp;
	int nnames = 0;
	int nobjs;
	Obj o;
	char *base;
	FILE *fp;
	long len;
	int fok;

	if (!DBINITED)
	    db_init();

	/* skip any leading blanks */
	while (*fn == ' ')
	    fn++;

	/* open the file.
	 * try looking for fn in several places.
	 */
	fp = fopenh (fn, "r");
	if (fp)
	    goto ok;
	sprintf (fullfn, "%s/%s", getPrivateDir(), fn);
	fp = fopenh (fullfn, "r");
	if (fp)
	    goto ok;
	sprintf (fullfn, "%s/catalogs/%s", getShareDir(), fn);
	fp = fopenh (fullfn, "r");
	if (fp)
	    goto ok;
	(void) sprintf (msg, "%s:\n%s", fn, syserrstr());
	xe_msg (msg, 1);
	return;

	/* report just base of fn */
    ok:
	for (base = fn+strlen(fn); base > fn && base[-1] != '/'; --base)
	    continue;

	/* set up to run the progress meter based on file position */
	(void) fseek (fp, 0L, SEEK_END);
	len = ftell (fp);
	(void) fseek (fp, 0L, SEEK_SET);
	pm_set (0);

	if (nodups) {
	    /* build sorted list of existing names for faster searching */
	    int dbn = db_n();
	    int mask = ALLM & ~PLANETM;
	    DBScan dbs;
	    Obj *op;

	    nnames = 0;
	    names = (MaxNm *) malloc (dbn*sizeof(MaxNm));
	    if (!names) {
		xe_msg ("No memory to check dups", 1);
		fclose (fp);
		return;
	    }
	    for (db_scaninit(&dbs,mask,NULL,0); (op = db_scan(&dbs)) != NULL; ){
		if (nnames < dbn) {
		    (void) strcpy (names[nnames++], op->o_name);
		} else {
		    printf ("Bug! too many objects! %d %d\n", dbn, nnames);
		    exit (1);
		}
	    }

	    qsort ((void *)names, nnames, sizeof(MaxNm), maxnm_cmpf);
	}

	/* get a fresh catalog entry */
	dbcp = db_catadd(base);
	if (!dbcp) {
	    xe_msg ("No memory for new catalog", 1);
	    if (names)
		free ((void *)names);
	    fclose(fp);
	    return;
	}

	/* read each line from the file and add good ones to the db */
	nobjs = 0;
	while (fgets (buf, sizeof(buf), fp)) {
	    char whynot[128];

	    pm_set ((int)(ftell(fp)*100/len)); /* update progress meter */

	    if (db_crack_line (buf, &o, whynot) < 0) {
		if (whynot[0] != '\0') {
		    (void) sprintf (msg,"%s: Bad edb:\n %s:%s",base,buf,whynot);
		    xe_msg (msg, 0);
		}
		continue;
	    }

	    if (o.o_type == PLANET)
		continue;

	    if (nodups && chk_name (o.o_name, names, nnames) == 0)
		continue;

	    if (db_objadd (&o) < 0) {
		xe_msg ("No more memory", 1);
		fclose(fp);
		db_catdel (dbcp);
		if (names)
		    free ((void *)names);
		return;
	    }
	    dbcp->n[o.o_type]++;
	    nobjs++;
	}

	/* clean up */
	if (names)
	    free ((void *)names);
	fok = !ferror(fp);
	fclose(fp);

	/* check for trouble */
	if (!fok) {
	    sprintf (msg, "%s:\n%s", base, syserrstr());
	    xe_msg (msg, 1);
	    db_catdel (dbcp);
	    return;
	}

	/* check for noop */
	if (nobjs == 0) {
	    db_catdel (dbcp);
	    if (nodups)
		sprintf (msg, "%s contains no new data", base);
	    else
		sprintf (msg, "%s contains no data", base);
	    xe_msg (msg, 1);
	}

	/* catalog GUI update is sufficient feedback if added ok */
	db_newcatmenu (dbcat, ndbcat);
	db_setinitial();

	/* auto set to ObjXY or Z if exactly 1 in catalog */
	if (nobjs == 1 && db_load1()) {
	    static int uidx[] = {OBJX, OBJY, OBJZ};
	    static char uobjn[] = "XYZ";
	    int i;

	    for (i = 0; i < XtNumber(uidx); i++) {
		Obj *uop = db_basic(uidx[i]);
		if (i == XtNumber(uidx)-1 || uop->o_type == UNDEFOBJ ||
					    (uop->o_type == o.o_type &&
					    !strcmp(uop->o_name, o.o_name))) {
		    char buf[54];
		    obj_set (&o, uidx[i]);
		    sprintf (buf, "Assigned %s to Obj%c", o.o_name, uobjn[i]);
		    xe_msg (buf, 1);
		    break;
		}
	    }
	}
}

/* assuming we can open it ok, connect the db fifo to a callback.
 * we close and reopen each time we are called.
 */
void
db_connect_fifo()
{
	static char fn[256];

	/* close if currently open */
	if (db_fifofd >= 0) {
	    XtRemoveInput (db_fifoid);
	    (void) close (db_fifofd);
	    db_fifofd = -1;
	}

	/* open for read/write. this assures open will never block, that
	 * reads (and hence select()) WILL block if it's empty, and let's
	 * processes using it come and go as they please.
	 */
	if (fn[0] == '\0')
	    (void) sprintf(fn,"%s/fifos/xephem_db_fifo", getShareDir());
	db_fifofd = openh (fn, 2);
	if (db_fifofd < 0) {
	    char msg[256];
	    (void) sprintf (msg, "%s: %s\n", fn, syserrstr());
	    xe_msg (msg, 0);
	    return;
	}

	/* wait for messages */
	db_fifoid = XtAppAddInput(xe_app, db_fifofd, (XtPointer)XtInputReadMask,
						    dbfifo_cb, (XtPointer)fn);
}

/* allocate *newop to the appropriate list, growing if necessary.
 * return 0 if ok, -1 if no more memory.
 * N.B we do *not* validate newop in any way.
 */
static int
db_objadd (newop)
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

	    /* grow list of chunks */
	    newdbl = dmp->dblist ? (char **) realloc (dmp->dblist, newdblsz)
				 : (char **) malloc (newdblsz);
	    if (!newdbl)
		return (-1);

	    /* add 1 chunk */
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
	db[FIXED].size		= DB_SIZE_ROUND(sizeof(ObjF));
	db[ELLIPTICAL].size	= DB_SIZE_ROUND(sizeof(ObjE));
	db[HYPERBOLIC].size	= DB_SIZE_ROUND(sizeof(ObjH));
	db[PARABOLIC].size	= DB_SIZE_ROUND(sizeof(ObjP));
	db[EARTHSAT].size	= DB_SIZE_ROUND(sizeof(ObjES));
	db[PLANET].size		= DB_SIZE_ROUND(sizeof(Obj)); /* *NOT* ObjPl */

	/* init the planets.
	 * because the number is fixed, we use static storage for the NOBJ
	 * Objects for the PLANET type.
	 */
	db[PLANET].dblist = plan_dblist;
	db[PLANET].nmem = NOBJ;
	db[PLANET].nobj = NOBJ;
	for (i = MERCURY; i <= MOON; i++) {
	    Obj *op = OBJP (PLANET, i);
	    op->o_type = PLANET;
	    (void) strncpy (op->o_name, planet_names[i], sizeof(op->o_name)-1);
	    op->pl.pl_code = i;
	}

	/* init the catalog. malloc something so we can use realloc now */
	dbcat = (DBCat *) malloc (sizeof(DBCat));
	ndbcat = 0;

	/* register the initial files list */
	sr_reg (0, dbifres, dbcategory, 0);
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
}

/* called whenever there is input readable from the db fifo.
 * read and crack what we can.
 * be prepared for partial lines split across reads.
 * N.B. do EXACTLY ONE read -- don't know that more won't block.
 * set the watch cursor while we work and call all_newdb() when we're done.
 *   we guess we are "done" when we end up without a partial line.
 */
/* ARGSUSED */
static void
dbfifo_cb (client, fdp, idp)
XtPointer client;       /* file name */
int *fdp;               /* pointer to file descriptor */
XtInputId *idp;         /* pointer to input id */
{
	static char partial[MAXDBLINE];	/* partial line from before */
	static int npartial;		/* length of stuff in partial[] */
	char buf[16*1024];		/* nice big read gulps */
	char *name = (char *)client;	/* fifo filename */
	char msg[1024];			/* error message buffer */
	int nr;				/* number of bytes read from fifo */

	/* turn on the watch cursor if there's no prior line */
	if (!npartial)
	    watch_cursor (1);

	/* catch up where we left off from last time */
	if (npartial)
	    (void) strcpy (buf, partial);

	/* read what's available up to the room we have left.
	 * if we have no room left, it will look like an EOF.
	 */
	nr = read (db_fifofd, buf+npartial, sizeof(buf)-npartial);

	if (nr > 0) {
	    char c, *lp, *bp, *ep;	/* last line, current, end */

	    /* process each whole line */
	    ep = buf + npartial + nr;
	    for (lp = bp = buf; bp < ep; ) {
		c = *bp++;
		if (c == '\n') {
		    bp[-1] = '\0';		      /* replace nl with EOS */
		    if (*lp == DBFIFO_MSG) {
			(void)sprintf (msg, "DBFIFO message: %s", lp+1);
			xe_msg (msg, 0);
		    } else {
			Obj o;
			if (db_crack_line (lp, &o, NULL) < 0) {
			    (void) sprintf (msg, "Bad DBFIFO line: %s", lp);
			    xe_msg (msg, 0);
			} else {
			    if (o.o_type == PLANET) {
				(void) sprintf (msg,
				    "Planet %s ignored from DBFIFO",o.o_name);
				xe_msg (msg, 0);
			    } else if (db_objadd (&o) < 0)
				xe_msg ("No more memory for DBFIFO", 0);
			}
		    }
		    lp = bp;
		}
	    }

	    /* save any partial line for next time */
	    npartial = ep - lp;
	    if (npartial > 0) {
		if (npartial > sizeof(partial)) {
		    (void)sprintf(msg,"Discarding long line in %.100s.\n",name);
		    xe_msg (msg, 0);
		    npartial = 0;
		} else {
		    *ep = '\0';
		    (void) strcpy (partial, lp);
		}
	    }

	} else {
	    if (nr < 0)
		(void) sprintf (msg, "Error reading %.150s: %.50s.\n",
							name, syserrstr());
	    else 
		(void) sprintf (msg, "Unexpected EOF on %.200s.\n", name);
	    xe_msg (msg, 1);
	    XtRemoveInput (db_fifoid);
	    (void) close (db_fifofd);
	    db_fifofd = -1;
	    npartial = 0;
	}

	/* if there is not likely to be more coming inform everyone about all
	 * the new stuff and turn off the watch cursor.
	 */
	if (!npartial) {
	    all_newdb (1);
	    watch_cursor (0);
	}
}

/* compare 2 pointers to DBCat's by name in qsort fashion */
static int
db_catcmpf (d1, d2)
qsort_arg d1, d2;
{
	return (strcmp (((DBCat *)d1)->name, ((DBCat *)d2)->name));
}

/* allocate a new DBCat in dbcat[] and init with name, sorted by name.
 * name is already just the basename of a full path.
 * return pointer if ok, else NULL if no more memory.
 */
static DBCat *
db_catadd (name)
char *name;
{
	DBCat *dbcp;
	int i;

	/* make room for another */
	dbcp = (DBCat *) realloc (dbcat, (ndbcat+1)*sizeof(DBCat));
	if (!dbcp)
	    return (NULL);
	dbcat = dbcp;
	dbcp = &dbcat[ndbcat++];
	(void) sprintf (dbcp->name, "%.*s", (int)sizeof(dbcp->name)-1, name);

	/* init the type counters -- leave key to find after search */
	for (i = 0; i < NOBJTYPES; i++) {
	    dbcp->start[i] = db[i].nobj;
	    dbcp->n[i] = 0;
	}
	dbcp->n[0] = -1;

	/* sort by name */
	qsort ((void *)dbcat, ndbcat, sizeof(DBCat), db_catcmpf);

	/* find key again */
	for (dbcp = dbcat; dbcp < &dbcat[ndbcat]; dbcp++)
	    if (dbcp->n[0] == -1) {
		dbcp->n[0] = 0;
		return (dbcp);
	    }

	/* eh?? */
	printf ("Bug! catalog disappeared after sorting: %s\n", name);
	exit(1);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: db.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
