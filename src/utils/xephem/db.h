/* catalog glue between db.c and dbmenu.c */

/* keep track of the indeces effected for each ObjType loaded by each catalog.
 * N.B. this works because all objects from a given catalog are contiguous
 *   within each their respective ObjType.
 */
#define	MAXCATNM	32	/* max catalog file name (just the base) */

typedef struct {
    char name[MAXCATNM];	/* name of catalog */
    int start[NOBJTYPES];	/* index of first entry from this catalog */
    int n[NOBJTYPES];		/* number of entries */
} DBCat;

extern char dbcategory[];

extern DBCat *db_catfind P_((char *name));
extern void db_newcatmenu P_((DBCat a[], int na));
extern void db_catdel P_((DBCat *dbcp));
extern void db_del_all P_((void));
