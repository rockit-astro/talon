/* prototypes for functions in catalogs.c */

extern int searchCatalog (char catdir[], char catalog[], char source[],
    Obj *op, char message[]);
extern int readCatalog (char fn[], Obj **opp, char message[]);
extern int searchDirectory (char catdir[], char source[], Obj *op, char m[]);

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: catalogs.h,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $
 */
