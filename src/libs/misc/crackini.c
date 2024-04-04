/* code to crack a windows-style .ini file. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "crackini.h"

/* read the given ini file.
 * if ok, return pointer to a malloced IniList, else return NULL.
 */
IniList *
iniRead (char *fn)
{
	FILE *fp;
	IniList *listp;
	char section[1024];
	char buf[1024];

	/* open the file */
	fp = fopen (fn, "r");
	if (!fp)
	    return (NULL);

	/* start things off */
	listp = (IniList *) malloc (sizeof(IniList));
	/* always malloc 1 so we can always realloc() later */
	listp->list = (char **)malloc(sizeof(char *));
	listp->nlist = 0;

	/* read each line */
	section[0] = '\0';
	while (fgets (buf, sizeof(buf), fp)) {
	    char name[256], valu[256];
	    char *bp = buf;

	    while (isspace(*bp))
		bp++;

	    if (sscanf(bp, "[%[^]]]", section) == 1) {
		/* found a section */
		continue;
	    }

	    if (sscanf(bp, "%[^= \t] = %s", name, valu) == 2) {
		/* found a name=value */
		char *new;

		new = malloc(strlen(section) + strlen(name) + strlen(valu) + 3);
		sprintf (new, "%s.%s=%s", section, name, valu);
		listp->list = (char **) realloc ((char *)listp->list,
					    (++listp->nlist) * sizeof(char *));
		listp->list[listp->nlist-1] = new;
	    }
	}

	/* done with file */
	fclose (fp);

	/* return values */
	return (listp);
}

/* search for the given section/name in listp, ignoring case.
 * return value portion if found, else NULL
 */
char *
iniFind (IniList *listp, char *section, char *name)
{
	char full[1024];
	int nfull;
	char **ep;

	nfull = sprintf (full, "%s.%s=", section, name);

	for (ep = &listp->list[listp->nlist]; --ep >= listp->list; ) {
	    if (strncasecmp (full, *ep, nfull) == 0)
		return (&(*ep)[nfull]);
	}

	return (NULL);
}

/* free an IniList */
void
iniFree (IniList *listp)
{
	while (--(listp->nlist) >= 0)
	    free (listp->list[listp->nlist]);
	free ((char *)listp);
}
