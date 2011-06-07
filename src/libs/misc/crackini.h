typedef struct {
    char **list;
    int nlist;
} IniList;

extern IniList *iniRead (char *fn);
extern char *iniFind (IniList *listp, char *section, char *name);
extern void iniFree (IniList *listp);

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: crackini.h,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $
 */
