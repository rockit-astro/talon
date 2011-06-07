/* code to manipulate the scan list file. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "strops.h"
#include "telenv.h"
#include "telstatshm.h"
//#include "db.h"
#include "scan.h"

#include "telrun.h"

static struct stat last_s;	/* last-known .sls file stat */

/* check whether the named file is materially different than last we knew.
 * if different return 0 else return -1
 */
int
newSLS (char *slsfn)
{
	struct stat s;
	int diff;

	if (stat (slsfn, &s) < 0)
	    memset ((void *)&s, 0, sizeof(s));

	diff = last_s.st_dev != s.st_dev
		    || last_s.st_ino != s.st_ino
		    || last_s.st_size != s.st_size
		    || last_s.st_mtime != s.st_mtime;

	last_s = s;

	return (diff ? 0 : -1);
}

/* search the given sls file for a New entry which matches sp. if find it,
 * mark the "status" line with code. return silently if can not find scan.
 * N.B. we assume code is one of N(ew)/D(one)/F(ail).
 * N.B. if modify the file, be sure to update last_s.
 */
void
markScan (char slsfn[], Scan *sp, int code)
{
	long offset;
	FILE *fp;
	Scan s;

	fp = telfopen (slsfn, "r+");
	if (!fp)
	    return;

	while (readNextSLS (fp, &s, &offset) == 0) {
	    /* never remark (in case several match) */
	    if (s.status != 'N')
		continue;

	    /* force a match for and hence ignore fields we might change */
	    s.running = sp->running;
	    s.starttm = sp->starttm;
	    s.status = sp->status;
	    s.shutter = sp->shutter;
	    if (memcmp ((void *)&s, (void *)sp, sizeof(Scan)) == 0) {
		fseek (fp, offset, 0);
		fputc (code, fp);
		fflush (fp);
		if (fstat (fileno(fp), &last_s) < 0)
		    memset ((void *)&last_s, 0, sizeof(s));
		break;
	    }
	}

	(void) fclose (fp);
}

/* find the first entry in slsfn marked New.
 * if find such return it at *sp and return 0, else return -1.
 */
int
findNew (char slsfn[], Scan *sp)
{
	int found;
	FILE *fp;

	found = 0;
	fp = telfopen (slsfn, "r");
	if (fp) {
	    while (readNextSLS (fp, sp, NULL) == 0) {
		if (sp->status == 'N') {
		    found = 1;
		    break;
		}
	    }
	    (void) fclose (fp);
	}

	return (found ? 0 : -1);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: fileio.c,v $ $Date: 2001/04/19 21:12:10 $ $Revision: 1.1.1.1 $ $Name:  $"};
