/* code to read xephem database files.
 * plus special handling for speedy reading of the sao catalog.
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "catalogs.h"
#include "strops.h"
#include "telenv.h"

static int sao_catalog (FILE *fp);
static int do_sao (FILE *fp, char name[], Obj *op, char m[]);

static int caseCharCmp(char a, char b)
{
	return toupper(a) == toupper(b) ? 1 : 0;
}
/* Compare the name(s) from a possibly multiple designation EDB entry
   with the given name we are searching for, and return > 0 if we match
*/
int checkNameMatch(char* edbLine, char* testName)
{
	// edb entries are delimited by | and field ends at ,
	// We skip all whitespace in both testName and edb entries
	// edb entries ignore anything following a (
	int nameNum = 1;
	char* t = testName;
	while (*t <= 32) t++; // skip any initial whitespace

	// Look for a designation in the given name.  This is all digits followed by a space,
	// or for comets, followed by a P or C
	char designation[80];
	char edesignation[80];
	memset(designation,0,sizeof(designation));
	char* tt = t;
	char* d = t;
	char* ds = designation;
	while (*d && isdigit(*d))
	{
		*ds++ = *d++;
	}
	if (ds != designation)
	{
		if (*d != ' ') *ds = toupper(*d); // include trailing P or C (presumably)
		if (*d) d++;
	}
	t = d;

	// count the non-whitespace length of our source name
	int srcLen = 0;
	while (*t && *t != '(')
	{
		if (*t > 32) srcLen++;
		t++;
	}
	t = d;
	char* st = t;
	char* p = edbLine;
	while (*p && *p != ',' && *p != '|')
	{
		char* w = p;
		char* t = st;
		int mc = 0;
		while (*w <= 32) w++;

		// Now look for a designation in the edbLine.
		memset(edesignation,0,sizeof(edesignation));
		d = w;
		while (*d && *d <= 32) d++; // skip initial whitespace
		ds = edesignation;
		while (*d && isdigit(*d))
		{
			*ds++ = *d++;
		}
		if (ds != edesignation)
		{
			if (*d != ' ') *ds = toupper(*d);
			if (*d) d++;
		}
		w = d;

		// convert MPC notation of reusing / for discoverer to a ( for our parsing
		int first = 0;
		while (*d && *d != '(' && *d != ',')
		{
			if (*d == '/')
			{
				if (first)
				{
					*d = '(';
					break;
				}
				else first = 1;
			}
			d++;
		}
		
		// do same for input
		first = 0;
		while (*t && *t != '(' && *t != ',')
		{
			if (*t == '/')
			{
				if (first)
				{
					*t = '(';
					break;
				}
				else first = 1;
			}
			t++;
		}
		t = st;

		while (*w && *w <= 32) w++;
		while (*t && *t <= 32) t++;
		int desMatch = (strlen(designation) == 0 || strcmp(designation, edesignation) == 0);
		if (!desMatch)
		{
			if (strncmp(designation,w,4) == 0) desMatch = 1; // keep trying if we typed in a year, not a designation
			if (desMatch)
			{
				t = tt;
				srcLen += strlen(designation);
			}
		}
		if (desMatch)
		{
			while (*w && *t && *t != '(' && *w != '(' && caseCharCmp(*w,*t) && mc < MAXNM)
			{
				mc++;
				t++;
				w++;
				while (*w && *w <= 32) w++;
				while (*t && *t <= 32) t++;
			}
		}
		if (desMatch && (mc == srcLen || mc >= MAXNM))
		{
		    while(mc < srcLen && *w >= 32 && *w != '(' && *w != '|' && *w != ',') w++;
			if (mc && *w >= 32 && *w != '(' && *w != '|' && *w != ',')
			{
				return 0;
			}
			return nameNum;
		}
		else
		{
			while (*w && *w != ',' && *w != '|') w++;
			if (*w == '|') w++;
			p = w;
			nameNum++;
		}
	}
	return 0;
}

/* search the given catalog in catdir for the given source.
 * to help with asteroids, if source starts with a number, just require match
 *   to that, else skip any leading number in candidate.
 * the catalog name need not include the suffix and we allow any case.
 * if found fill in *op and return 0 else fill in m and return -1.
 */
int
searchCatalog (catdir, catalog, source, op, m)
char catdir[];
char catalog[];
char source[];
Obj *op;
char m[];
{
	char path[1024];
	char buf[512];
	struct dirent *dirent;
	DIR *dir;
	FILE *fp;

	/* check for easy planet name first */
	if (source && db_chk_planet (source, op) == 0)
		return (0);

	/* additional sanity check things */
	if (!catdir || catdir[0] == '\0' || !catalog || catalog[0] == '\0'
	        || !source || source[0] == '\0')
	{
		sprintf (m, "No dir, catalog or source");
		return (-1);
	}

	/* open and scan catdir for catalog, any case */
	dir = opendir (catdir);
	if (!dir)
	{
		sprintf (m, "%s: %s", catdir, strerror(errno));
		return (-1);
	}
	dirent = NULL;
	for (fp = NULL; !fp && (dirent = readdir (dir)) != NULL; )
	{
		/* see if d_name works */
		if (strcasecmp (dirent->d_name, catalog))
		{
			(void) sprintf (buf, "%s.edb", catalog);
			if (strcasecmp (dirent->d_name, buf))
				continue;
		}

		(void) sprintf (path, "%s/%s", catdir, dirent->d_name);
		fp = telfopen (path, "r");
	}

	(void) closedir (dir);

	if (!fp)
	{
		sprintf (m, "Can not find catalog '%s' in '%s'", catalog, catdir);
		return (-1);
	}

	/* first check for sao catalog */
	if (sao_catalog (fp))
	{
		int s = do_sao (fp, source, op, m);
		fclose (fp);
		return (s);
	}

	/* scan for source, ignoring whitespace and case up to MAXNM-1 chars */
	while (fgets (buf, sizeof(buf), fp) != NULL)
	{
		int match = checkNameMatch(buf, source);

		if (match)
		{
			/* quick name sanity check passes -- now work harder */
			buf[strlen(buf)-1] = '\0';
			if (db_crack_line_name (buf, op, NULL, match-1) == 0)
			{
				/* yes! */
				fclose (fp);
				return (0);
			}
		}
	}

	/* nope */
	fclose (fp);
	sprintf (m, "`%s' not found in `%s'", source, catalog);
	return (-1);
}

/* read the given filename of .edb records and create an array of
 * Obj records. Set the address of the malloced array to *opp and
 * return the number of entries in it.
 * return -1 and fill m if trouble.
 * N.B. caller must free (*opp) iff return >= 0.
 * N.B. fn[] is converted to all lower case IN PLACE.
 */
int
readCatalog (fn, opp, m)
char fn[];
Obj **opp;
char m[];
{
#define OBJCHUNK    64  /* number of Obj we malloc each time */
	Obj *opa;       /* pointer to malloced array of nmem Obj */
	int nmem;       /* number that can fit in opa[] */
	int nobs;       /* number actually used in opa[] */
	char buf[512];
	FILE *fp;

	fp = telfopen (fn, "r");
	if (!fp)
	{
		sprintf (m, "%s: %s", fn, strerror(errno));
		return (-1);
	}

	/* grab an initial pool of memory */
	nmem = OBJCHUNK;
	opa = (Obj *) malloc (nmem * sizeof(Obj));
	if (!opa)
	{
		sprintf (m, "%s: No memory", fn);
		return (-1);
	}
	nobs = 0;

	/* read each entry */
	while (fgets (buf, sizeof(buf), fp) != NULL)
	{
		Obj *op = &opa[nobs];

		buf[strlen(buf)-1] = '\0';
		if (db_crack_line (buf, op, NULL) == 0)
		{
			/* good one -- add it to the list */
			if (++nobs == nmem)
			{
				char *newopa;

				nmem += OBJCHUNK;
				newopa = realloc ((void *)opa, nmem * sizeof(Obj));
				if (!newopa)
				{
					sprintf (m, "%s: No more memory", fn);
					break;
				}
				else
					opa = (Obj *) newopa;
			}
		}
	}

	fclose (fp);

	if (nobs < 0)
		free ((void *)opa);
	else
		*opp = opa;

	return (nobs);
}

/* search all catalogs in catdir for the given source, ignoring white and case.
 * if found fill in *op and return 0 else fill in m and return -1.
 */
int
searchDirectory (catdir, source, op, m)
char catdir[];
char source[];
Obj *op;
char m[];
{
	struct dirent *dirent;
	DIR *dir;

	/* check for easy planet name first */
	if (source && db_chk_planet (source, op) == 0)
		return (0);

	/* additional sanity check things */
	if (!catdir || catdir[0] == '\0' || !source || source[0] == '\0')
	{
		sprintf (m, "No directory or source");
		return (-1);
	}

	/* open catdir */
	dir = opendir (catdir);
	if (!dir)
	{
		sprintf (m, "%s: %s", catdir, strerror(errno));
		return (-1);
	}

	/* scan for and search each catalog */
	while ((dirent = readdir (dir)) != NULL)
	{
		int l = strlen (dirent->d_name);
		if (l > 4 && !strcasecmp (dirent->d_name+(l-4), ".edb"))
			if (searchCatalog (catdir, dirent->d_name, source, op, m) == 0)
				break;
	}
	(void) closedir (dir);

	if (dirent)
		return (0);

	sprintf (m, "not found anywhere in %s", basenm(catdir));
	return (-1);
}

/* return 1 if file starts with SAO, else 0.
 * N.B. we always return with fp rewound.
 */
static int
sao_catalog (FILE *fp)
{
	char buf[128];

	rewind (fp);
	if (fgets (buf, sizeof(buf), fp) == NULL)
		return (0);
	rewind (fp);
	if (strncasecmp (buf, "SAO", 3) == 0)
		return (1);
	return (0);
}

/* search the SAO catalog for the given entry;
 * if found, fill in *op and return 0, else fill m and return -1.
 * N.B. we assume the file is sorted in increasing order.
 */
static int
do_sao (FILE *fp, char name[], Obj *op, char m[])
{
#define MAXSAO  258997L
	long s, s1, s2;
	char line[128];
	long nname, ns;

	/* check the name */
	if (strncasecmp (name, "SAO", 3))
	{
		sprintf (m, "Bad SAO name: %s", name);
		return (-1);
	}

	/* get the sao number we are looking for */
	nname = atol (name+3);
	if (nname <= 0 || nname > MAXSAO)
	{
		sprintf (m, "Bad SAO number: %ld (range is 1 .. %ld)",nname,MAXSAO);
		return (-1);
	}

	/* set s1 and s2 to first and last possible file offset */
	s1 = 0;
	fseek (fp, 0L, SEEK_END);
	s2 = ftell (fp) - 1;

	do
	{
		s = (s1 + s2)/2;
		fseek (fp, s, SEEK_SET);
		do  /* align to nearest beginning of line */
			fgets (line, sizeof(line), fp);
		while (strncasecmp (line, "SAO", 3) != 0);
		ns = atol (line+3);
		if (nname < ns)
			s2 = s;
		else
			s1 = s;
	}
	while (nname != ns && s2 > s1);

	if (nname == ns)
	{
		line[strlen(line)-1] = '\0';    /* discard trailing \n */
		if (db_crack_line (line, op, NULL) == 0)
			return (0);
	}

	sprintf (m, "SAO number %ld not found.", nname);
	return (-1);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: catalogs.c,v $ $Date: 2002/03/12 16:45:41 $ $Revision: 1.2 $ $Name:  $"};
