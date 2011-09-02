/* code to manipulate Obs observing run descriptors.
 * changed 20020305 by jca:
 *			added FOCUS keyword to KWCode, KW and
 * 			wrote focus to Obs structure
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include <Xm/Xm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "strops.h"
#include "configfile.h"
#include "scan.h"

#include "telsched.h"

#define	PRIORITYDEF	100	/* default priority */

#define	MAXNAME		32
#define	MAXVALUE	80
#define	NREPORT		10	/* report progress after each many these */

/* N.B. these must be kept in sync with the kw table */
typedef enum {
    BINNING=0, CCDCALIB, COMMENT, COMPRESS,
    DEC, DECOFFSET, DURATION, EPOCH, FILTER, HASTART,
    IMAGEDIR, LSTDELTA, LSTSTART, OBSERVER, PRIORITY,
    RA, RAOFFSET, REPEAT, SHUTTER, SOURCE, SUBIMAGE, TITLE,
    BLOCK, BLOCKREPEAT, UTDATE,
    FOCUSOFF, FOCUSPOS, AUTOFOCUS, FIXEDALTAZ
} KWCode;

/* info about each keyword */
typedef struct {
    KWCode kwcode;		/* redundent check on valid index */
    char name[MAXNAME];		/* name of keyword */
    char value[MAXVALUE];	/* value for this keyword */
    int sawit;			/* set if seen by parser */
} KW;

static KW kw[] = {
    {BINNING,	"BINNING"},
    {CCDCALIB,	"CCDCALIB"},
    {COMMENT,	"COMMENT"},
    {COMPRESS,	"COMPRESS"},
    {DEC,	"DEC"},
    {DECOFFSET,	"DECOFFSET"},
    {DURATION,	"DURATION"},
    {EPOCH,	"EPOCH"},
    {FILTER,	"FILTER"},
    {HASTART,	"HASTART"},
    {IMAGEDIR,	"IMAGEDIR"},
    {LSTDELTA,	"LSTDELTA"},
    {LSTSTART,	"LSTSTART"},
    {OBSERVER,	"OBSERVER"},
    {PRIORITY,	"PRIORITY"},
    {RA,	"RA"},
    {RAOFFSET,	"RAOFFSET"},
    {REPEAT,	"REPEAT"},
    {SHUTTER,	"SHUTTER"},
    {SOURCE,	"SOURCE"},
    {SUBIMAGE,	"SUBIMAGE"},
    {TITLE,	"TITLE"},
    {BLOCK,	"BLOCK"},
    {BLOCKREPEAT, "BLOCKREPEAT"},
    {UTDATE,	"UTDATE"},
    {FOCUSOFF,	"FOCUSOFF"},
    {FOCUSPOS,	"FOCUSPOS"},
    {AUTOFOCUS,	"AUTOFOCUS"},
    {FIXEDALTAZ,"FIXEDALTAZ"},
};

#define	NKW	(sizeof(kw)/sizeof(kw[0]))

static KW *getKW (int i);
static void resetKW (void);
static KW *scanKW (char *name);
static int addObs (Obs **mopp, int *nopp);
static void growObs (Obs **mopp, int now, int add);
static int doSource (Obs mop[], int n, Obs *newop, char name[]);
static int setRADecEpoch (Obs *op);
static int mkargv (char *str, char *argv[]);
static void cleanStr (char *str);
static int UTDATE_nottoday (void);
static int isanumber (char *vp);

/* set this whenever see the LSTSTART keyword, then increment it
 * along as needed until see another. it is in hours. this is used to increment
 * the initial time for each scan in a block so the schedule tends to keep them
 * in order.
 */
static double lststart;

/* this macro is given the duration of a scan, in seconds, and returns the
 * number of hours the scan will occupy in the sorting's slot granularity.
 * use to increment lststart.
 */
#define	HRSDUR(dur) (( floor(SECS2SLOTS(dur)) * SECSPERSLOT)/3600.0)

/*
 * STO: Extended command hack -- SCH Comment argument passing
 * 10/29/09
 * This is the "sneaky" technique for passing information through an SCH file
 * into the executing telrun.sls file and finally out to the postprocess action.
 * This technique is required to issue certain commands for certain special features
 * that are automatically selected by SSON observers.  Since the use of SCH files for
 * scheduling is used here, the information must be conveyed via these files.
 * However, modification of the file format itself to add new fields would make
 * exchanges between other Talon sites on SSON dependent upon a specific version
 * of Talon and the SCH / SLS format.
 * By passing information through a specially formatted comment line, the original
 * file format can continue to be used between installations.
 *
 * This function scans the comment header block for the extended command line.
 * Call this just after opening the file.
 */
int
getCommentEmbeddedCommand(FILE* fp, char* cmdout, int cmdsize)
{
    // look for "!! Extra:" as a comment command indicator
    char* cmdTag = "! Extra:";
    while(1)
    {
        char c = getc(fp);
        if(c > 32) // skip whitespace
        {
            if(c == '!' || c == '#')
            {
                // see if we start with ! Extra:
                char* p = cmdTag;
                int found = 1;
                while(*p)
                {
                    if(getc(fp) != *p++)
                    {
                        found = 0;
                        break;
                    }
                }
                if(found)
                {
                    // skip spaces
                    do {
                        c =  getc(fp);
                    } while(c == ' ' || c == '\t');

                    // read the command into cmdout
                    p = cmdout;
                    while(p && cmdsize--)
                    {
                        if(c == '\n' || c == '\r' || c == EOF) break;
                        if(c < 32) c = ' ';
                        *p++ = c;
                        c = getc(fp);
                    }
                    if(p) *p = 0;
                    // We have our result. Read to end of this line then return.
                    while( c != '\n' && c != EOF)
                    {
                        c = getc(fp);
                    }
                    // return with length of command read
                    return cmdout ? strlen(cmdout) : 0;
                }
                else
                {
                    // skip to end of line to dismiss remainder of comment
                    while(c != '\n' && c != EOF)
                    {
                        c = getc(fp);
                    }
                }
            }
            else
            {
                // if not a comment, then this is the first operational character
                // move the file position back one so we recapture it on subsequent reads
                // and return with -1 to indicate we are done scanning.
                fseek(fp, -1, SEEK_CUR);
                return -1;
            }
        }
    }
}


/* read the given filename of observing records and create an array of
 * Obs records. Return the address of the malloced array to *opp and
 * caller may just free() *opp when he's finished with it.
 * return the number of entries in it.
 * return -1 and print message to stderr if error.
 */
int
readObsFile (fn, opp)
char *fn;
Obs **opp;
{
	char *base = basenm (fn);
	char name[MAXNAME];
	char value[MAXVALUE];
	int nobs;
	Obs *mop;	/* real malloced list */
	int nop;	/* total number of entries in mop[] */
	int err;	/* set on error: 1 adds msg, 2 assumes alrdy */
	int new;	/* set when expecting to start processing a new run */
	int blockn;	/* index into mop where block repeat begins */
	double blockd;	/* value of BLOCK, in hours */
	FILE *fp;
	KW *kwp;
	int cmdRes;
	char cmdFound[1024];

	fp = fopen (fn, "r");
	if (!fp) {
	    msg ("Can not open %s: %s", base, strerror(errno));
	    return (-1);
	}

	// scan for our embedded ! Extra: command format
	cmdRes = getCommentEmbeddedCommand(fp, cmdFound, sizeof(cmdFound));

	nobs = 0;
	mop = NULL;
	nop = 0;
	err = 0;
	new = 1;
	blockn = -1;	/* illegal until see BLOCK <label> */
	resetKW();

	/* keep reading until eof or err.
	 * make now Obs entries when see "/" or eof.
	 */
	while (!err) {
	    int saweof;
	    int s;

	    /* read next pair */
	    s = nextPair (fp, fn, name, sizeof(name), value, sizeof(value));
	    saweof = s == -1;
	    if ((saweof && !new) || s == -2) {
            msg ("Unexpected End-of-File");
            err = 2;
            break;
	    }
	    if (saweof && new)
	        break;
	    if (s == -3) {
            msg ("Internal parsing error");
            err = 2;
            break;
	    }

	    /* clean out bogus characters */
	    cleanStr (name);
	    cleanStr (value);

	    /* make new Obs entries when see eof or "/"
	     * otherwise just keep recording new keyword info
	     */
	    if (saweof || name[0] == '/') {


            /* end of this run set.
             * build up more Obs in mop from the current set of keywords.
             */
            if (addObs (&mop, &nop) < 0) {
                /* we assume addObs already wrote a message */
                err = 2;
                break;
            }

            if (((++nobs) % NREPORT) == 0)
                msg ("Read %d objects ...", nobs);

            if (saweof)
                break;

            new = 1;

            /* require both RA/DEC each time or neither */
            kwp = getKW (RA); kwp->sawit = 0;
            kwp = getKW (DEC); kwp->sawit = 0;

	    } else {

            /* scan for name in list of keywords.
             * store and continue if find it.
             */
            kwp = scanKW (name);
            if (!kwp) {
                err = 2;
                break;
            }
            strcpy (kwp->value, value);
            kwp->sawit = 1;

            /* check for the BLOCK-related words.
             * they don't count as "new".
             */

            if (kwp == getKW (BLOCK)) {
                /* save where this block starts */
                blockn = nop;

                /* crack the delay time and save */
                if (scansex (value, &blockd) < 0) {
                    msg ("Bad BLOCK time interval: %s. Should be H:M:S",
                                            value);
                    err = 2;
                    break;
                }
            }

            if (kwp == getKW (BLOCKREPEAT)) {
                int blksz = nop - blockn;	/* Obs in one block set */
                int dupcnt = atoi(value)-1;	/* times to repeat block */
                int ngrow = blksz*dupcnt;	/* total more Obs to add */
                int i, j;

                /* first some sanity checks */
                if (isanumber(value) < 0) {
                    msg ("Bad BLOCKREPEAT format.. must be a number");
                    err = 2;
                    break;
                }
                if (!new) {
                    msg ("Unexpected BLOCKREPEAT -- check for final /");
                    err = 2;
                    break;
                }
                if (dupcnt < 0) {
                    msg ("BLOCKREPEAT count must be > 0");
                    err = 2;
                    break;
                }
                if (blockn < 0) {
                    msg ("Saw BLOCKREPEAT before BLOCK keyword");
                    err = 2;
                    break;
                }

                /* need lststart to implement the block interval.
                 * if none supplied by user, we center about transit.
                 * leave for schedular to float if dupcnt is 0.
                 */
                if (dupcnt > 0) {
                    for (j = 0; j < blksz; j++) {
                        Obs *op = mop+blockn+j;
                        double lststart;

                        if (op->lststart != NOTIME)
                        continue;

                        /* TODO: can be much more clever here */
                        computeCir (op);
                        lststart = radhr(op->scan.obj.s_ra);
                        lststart -= blockd*dupcnt/2;
                        range (&lststart, 24.0);

                        op->lststart = lststart;
                        op->utcstart = lst2utc (lststart);
                    }
                }

                /* ok, block looks reasonable -- grow mop and copy,
                 * incrementing start times by blockd.
                 */
                growObs (&mop, nop, ngrow);
                for (i = 0; i < dupcnt; i++) {
                    for (j = 0; j < blksz; j++) {
                        Obs *src = mop+blockn+j;
                        Obs *dst = mop+nop+(i*blksz)+j;
                        cpyObs (dst, src);
                        dst->lststart = src->lststart + blockd*(i+1);
                        range (&dst->lststart, 24.0);
                        dst->utcstart = lst2utc (dst->lststart);
                    }
                }

                /* now update nop */
                nop += ngrow;

                /* reset blockn */
                blockn = -1;

                /* this run is now processed */
                new = 1;
            } else {
                /* saw some other keyword */
                new = 0;

                /* remember LSTSTART just when see it, then increment
                 * as various scans get added until see it again.
                 */
                if (kwp == getKW (LSTSTART)) {
                    if (scansex (kwp->value, &lststart) < 0) {
                        msg ("Bad LST format: %s. Should be H:M:S",
                                        kwp->value);
                        err = 2;
                    }
                    /* truncate to the previous scheduling slot */
                    lststart = floor((3600*lststart)/SECSPERSLOT) *
                                        SECSPERSLOT/3600.0;
                }
            }
	    }
	}

	/* when get here, either err is > 0 or we've seen eof and all was ok.
	 * err == 1 means we have not yet presented a diagnostic message;
	 * err == 2 means we have so don't write another more general one.
	 */

	fclose (fp);

	if (err < 2 && nop == 0) {
	    msg ("No entries found in %s", base);
	    err = 2;
	}

	if (err) {
	    if (err < 2)
		msg ("Error in %s near \"%s=%s\"", base, name, value);
	    if (mop)
		free ((char *)mop);
	    return (-1);
	} else {
	    Obs *allp;
	    int nallp;
	    int fnbase;
	    int i;

	    /* add the sch filename to all entries */
	    for (i = 0; i < nop; i++)
		ACPYZ (mop[i].scan.schedfn, base);

	    /* check for other sch files with same name */
	    get_obsarray (&allp, &nallp);
	    for (fnbase = i = 0; i < nallp; i++)
		if (strncasecmp ((allp++)->scan.imagefn, base, 3) == 0)
		    fnbase++;

	    /* add the image filename code to all entries.
	     * @@@ will get filled in with day-of-year code when this is
	     * actually used.
	     */
	    for (i = 0; i < nop; i++)
		sprintf (mop[i].scan.imagefn, "%.3s@@@%02x", base, fnbase+i);

	    // STO: 11/13/09: Fix bug... (recently introduced? How?)
	    // Multiple filter designations are not getting the proper ccdcalib values set. Copy to all.
        for (i = 1; i < nop; i++) {
            mop[i].scan.ccdcalib = mop[0].scan.ccdcalib;
        }

        // STO: 10/29/09 Add the parsed extra command to all entries
        // if we didn't use a ccd calib or focus override, then if we have an argument, consider it CT_EXTCMD
        if(cmdRes > 0) {
            for(i=0; i<nop; i++) {
                if(mop[i].scan.ccdcalib.newc == CT_NONE) mop[i].scan.ccdcalib.newc = CT_EXTCMD;
                strcpy(mop[i].scan.extcmd, cmdFound);
            }
        }

	    *opp = mop;
	    return (nop);
	}
}

/* copy src to dst */
void
cpyObs (dst, src)
Obs *dst, *src;
{
	memcpy ((char *)dst, (char *)src, sizeof(Obs));
}

/* init the Obs at op with default values. */
void
initObs (op)
Obs *op;
{
	memset ((char *)op, 0, sizeof(Obs));
	op->scan.priority = PRIORITYDEF;
	op->scan.shutter = CCDSO_Open;
	op->lststart = NOTIME;
	op->utcstart = NOTIME;
}

/* check the list of filter names in the av list of ac strings.
 * if all OK then return 0, else print msg and return -1.
 * actually, av[i] need not be a string -- we only ever use the first char.
 * N.B. we also convert av[i][0] to uppercase IN PLACE.
 */
int
legalFilters (ac, av)
int ac;
char *av[];
{
	static char f[20];
	int i, fl;

	/* first time, init f][ with filter codes */
	if (f[0] == '\0')
	    for (i = 0; i < nfilt; i++)
		f[i] = filterp[i].name[0];

	fl = strlen(f);
	while (--ac >= 0) {
	    char filt = av[ac][0];
	    if (islower(filt))
		av[ac][0] = filt = (char)toupper(filt);
	    for (i = 0; i < fl; i++)
		if (f[i] == filt)
		    break;
	    if (i == fl) {
		msg ("Unknown FILTER. Choose from %s", f);
		return (-1);
	    }
	}

	return (0);
}

/* return 0 if s looks like a reasonable m/d/y format, else -1 */
int
dateformatok (char *s)
{
	int m, d, y;
	Obs *op;

	if (strlen(s) >= sizeof(op->date)
				|| sscanf (s, "%d/%d/%d", &m, &d, &y) != 3)
	    return (-1);
	if (m < 1 || m > 12 || d < 1 || d > 31 || y < 1990 || y > 2010)
	    return (-1);
	return (0);
}

/* the string str is already known to be an invalid representation for a
 * CCDCalib structure. print an error message which suggests proper ones..
 */
void
illegalCCDCalib (str)
char *str;
{
	char *names[CAL_NSTR];
	char buf[128];
	int i, l;

	ccdCalibStrs (names);
	l = sprintf (buf, "CCDCALIB: Choose from");
	for (i = 0; i < XtNumber(names); i++)
	    l += sprintf (buf+l, "%s%s", i?", ":" ", names[i]);
	msg (buf);
}

/* the string str is already known to be an invalid representation for a
 * CCDShutterOptions. print an error message which suggests proper ones..
 */
void
illegalCCDSO (str)
char *str;
{
	char *names[CCDSO_NSTR];
	char buf[128];
	int i, l;

	ccdSOStrs (names);
	l = sprintf (buf, "SHUTTER: Choose from");
	for (i = 0; i < XtNumber(names); i++)
	    l += sprintf (buf+l, "%s%s", i?", ":" ", names[i]);
	msg (buf);
}

/* add more Obs to *mopp using the current set of keyword values.
 * increment *nopp and update *mopp as we add to the list.
 * return 0 if all ok, else print msg and return -1.
 * N.B. *mopp may be NULL.
 * N.B. if UTDATE is not today return 0 but without having added anything.
 */
static int
addObs (mopp, nopp)
Obs **mopp;
int *nopp;
{
	Obs *bop;	/* base within *mopp we are dealing with */
	int ndup;	/* number of Obs we are adding */
	int argc;
	char *argv[20];
	KW *kwp;
	double tmp;
	int rep;
	int n;
	int i;

	/* first a quick check that the date will be allowed at all */
	if (UTDATE_nottoday())
	    return(0);

	/* first do the keywords that can cause replication by virtue of having
	 * subvalues. this will establish ndup.
	 */
	kwp = getKW (DURATION);
	if (!kwp->sawit) {
	    msg ("Must have DURATION keyword.");
	    return (-1);
	}
	argc = mkargv (kwp->value, argv);
	if (argc < 1) {
	    msg ("DURATION requires at least one value.");
	    return (-1);
	}
	ndup = argc;
	growObs (mopp, *nopp, ndup);
	bop = *mopp + *nopp;
	for (i = 0; i < ndup; i++) {
	    tmp = atof (argv[i]);
	    if (tmp <= 0) {
		msg ("Each DURATION must be > 0.");
		return (-1);
	    }
	    bop[i].scan.dur = tmp;
	}

	kwp = getKW (FILTER);
	if (!kwp->sawit) {
	    msg ("Must have FILTER keyword.");
	    return (-1);
	}
	argc = mkargv (kwp->value, argv);
	if (argc < 1) {
	    msg ("FILTER requires at least one value.");
	    return (-1);
	}
	if (legalFilters (argc, argv) < 0)
	    return (-1);
	/* allow for more entries in FILTER than DURATION */
	while (ndup < argc) {
	    growObs (mopp, *nopp+ndup, 1);
	    bop = *mopp + *nopp;
	    cpyObs (&bop[ndup], &bop[ndup-1]);
	    ndup++;
	}
	for (i = 0; i < ndup; i++) {
	    /* allow for more entries in DURATION than FILTER */
	    bop[i].scan.filter = i<argc ? argv[i][0] : bop[i-1].scan.filter;
	}

	/* now duplicate this whole ndup collection REPEAT-1 more times */
	kwp = getKW (REPEAT);
	if (isanumber(kwp->value) < 0) {
	    msg ("Bad REPEAT format.. must be a number");
	    return (-1);
	}
	rep = atoi (kwp->value);
	if (rep <= 0) {
	    msg ("REPEAT must be at least 1.");
	    return (-1);
	}
	if (rep > 1) {
	    int r;
	    growObs (mopp, *nopp+ndup, ndup*(rep-1));
	    bop = *mopp + *nopp;
	    for (r = 1; r < rep; r++)
		for (i = 0; i < ndup; i++)
		    cpyObs (&bop[i + ndup*r], &bop[i]);
	    ndup = ndup*rep;
	}

	/* now do the keywords which just set simple values -- the list
	 * will not grow any more now.
	 */

	kwp = getKW (BINNING);
	argc = mkargv (kwp->value, argv);
	if (argc != 2) {
	    msg ("BINNING requires <binx>,<biny>.");
	    return (-1);
	}
	if (atoi(argv[0]) <= 0 || atoi(argv[1]) <= 0) {
	    msg ("Bad BINNING value. Example: 2,2.");
	    return (-1);
	}
	for (i = 0; i < ndup; i++) {
	    bop[i].scan.binx = atoi(argv[0]);
	    bop[i].scan.biny = atoi(argv[1]);
	}

	kwp = getKW (SHUTTER);
	if (kwp->sawit) {
	    if (ccdStr2SO (kwp->value, &bop[0].scan.shutter) < 0) {
		illegalCCDSO (kwp->value);
		return (-1);
	    }
	    for (i = 1; i < ndup; i++)
		bop[i].scan.shutter = bop[0].scan.shutter;

	    kwp = getKW (COMMENT);
	    for (i = 0; i < ndup; i++)
		ACPYZ (bop[i].scan.comment, kwp->value);
	}

	kwp = getKW (COMPRESS);
	if (kwp->value[0] == 'Y' || kwp->value[0] == 'y')
	    n = 1;
	else if (kwp->value[0] == 'N' || kwp->value[0] == 'n')
	    n = 0;
	else if (isanumber(kwp->value) < 0) {
	    msg ("Bad COMPRESS format.. must be a number");
	    return (-1);
	}
	n = atoi (kwp->value);
	for (i = 0; i < ndup; i++)
	    bop[i].scan.compress = n;

	kwp = getKW (UTDATE);
	if (kwp->value[0] && dateformatok (kwp->value) < 0) {
	    msg ("UTDATE invalid: Format is MM/DD/YYYY.");
	    return (-1);
	}
	for (i = 0; i < ndup; i++)
	    ACPYZ (bop[i].date, kwp->value);

	kwp = getKW (IMAGEDIR);
	for (i = 0; i < ndup; i++)
	    ACPYZ (bop[i].scan.imagedn, kwp->value);

	kwp = getKW (LSTDELTA);
	if (isanumber(kwp->value) < 0) {
	    msg ("Bad LSTDELTA format.. must be a number");
	    return(-1);
	}
	tmp = atof(kwp->value);
	if (tmp < 0) {
	    msg ("LSTDELTA must be >= 0");
	    return (-1);
	}
	for (i = 0; i < ndup; i++)
	    bop[i].scan.startdt = (int)floor(tmp*60+.5); /* user used minutes */

	kwp = getKW (LSTSTART);
	if (kwp->value[0]) {
	    KW *hakwp = getKW (HASTART);

	    if (hakwp->value[0]) {
		msg ("Can not set both LSTSTART and HASTART");
		return (-1);
	    }

	    /* we actually use the running lststart */
	    for (i = 0; i < ndup; i++) {
		/* mimic the sorter's slotting granularity */
		bop[i].lststart = lststart;
		bop[i].utcstart = lst2utc (lststart);
		lststart += HRSDUR(bop[i].scan.dur);
		range (&lststart, 24.0);
	    }
	}

	kwp = getKW (OBSERVER);
	if (!kwp->sawit || kwp->value[0] == '\0') {
	    msg ("OBSERVER is a required field.");
	    return (-1);
	}
	for (i = 0; i < ndup; i++)
	    ACPYZ (bop[i].scan.observer, kwp->value);

	kwp = getKW (PRIORITY);
	if (kwp->sawit) {
	    int p = atoi (kwp->value);
	    if (isanumber(kwp->value) < 0) {
		msg ("Bad PRIORITY format.. must be a number");
		return (-1);
	    }
	    for (i = 0; i < ndup; i++)
		bop[i].scan.priority = p;
	}

	kwp = getKW (RAOFFSET);
	if (kwp->sawit) {
	    if (scansex (kwp->value, &tmp) < 0) {
		msg ("Bad RAOFFSET format: %s", kwp->value);
		return (-1);
	    }
	    tmp = hrrad(tmp);
	    for (i = 0; i < ndup; i++)
		bop[i].scan.rao = tmp;
	}

	kwp = getKW (DECOFFSET);
	if (kwp->sawit) {
	    if (scansex (kwp->value, &tmp) < 0) {
		msg ("Bad DECOFFSET format: %s", kwp->value);
		return (-1);
	    }
	    tmp = degrad(tmp);
	    for (i = 0; i < ndup; i++)
		bop[i].scan.deco = tmp;
	}

	kwp = getKW (SUBIMAGE);
	argc = mkargv (kwp->value, argv);
	if (argc != 4) {
	    msg ("SUBIMAGE requires SX,SY,SW,SH");
	    return (-1);
	}
	for (i = 0; i < ndup; i++) {
	    int ti;

	    ti = atoi(argv[0]);
	    if (ti < 0 || isanumber(argv[0]) < 0) {
		msg ("SUBIMAGE SX must be >= 0");
		return (-1);
	    }
	    bop[i].scan.sx = ti;

	    ti = atoi(argv[1]);
	    if (ti < 0 || isanumber(argv[1]) < 0) {
		msg ("SUBIMAGE SY must be >= 0");
		return (-1);
	    }
	    bop[i].scan.sy = ti;

	    ti = atoi(argv[2]);
	    if (ti < 1) {
		msg ("SUBIMAGE SW must be >= 1");
		return (-1);
	    }
	    bop[i].scan.sw = ti;

	    ti = atoi(argv[3]);
	    if (ti < 1) {
		msg ("SUBIMAGE SH must be >= 1");
		return (-1);
	    }
	    bop[i].scan.sh = ti;
	}

	kwp = getKW (TITLE);
	if (!kwp->sawit) {
	    msg ("Must have TITLE keyword.");
	    return (-1);
	}
	for (i = 0; i < ndup; i++)
	    ACPYZ (bop[i].scan.title, kwp->value);

	kwp = getKW (CCDCALIB);
	if (ccdStr2Calib (kwp->value, &bop[0].scan.ccdcalib) < 0) {
	    illegalCCDCalib (kwp->value);
	    return (-1);
	}

	/* crack the source name.
	 * do first one then copy to the dups.
	 * N.B. doSource also handles RA/DEC/EPOCH.
	 * N.B. just pass *nopp to doSource else it finds the new one in mop!
	 */
	kwp = getKW (SOURCE);
	if (doSource (*mopp, *nopp, bop, kwp->value) < 0)
	    return (-1);
	for (i = 1; i < ndup; i++)
	    bop[i].scan.obj = bop[i-1].scan.obj;

	/* Now we have a known bop->scan.obj, ie, we have digested SOURCE and/or
	 * EPOCH/RA/DEC.
	 */

	kwp = getKW (HASTART);
	if (kwp->value[0]) {
	    KW *lstkwp = getKW (LSTSTART);

	    if (lstkwp->value[0]) {
		msg ("Can not set both LSTSTART and HASTART");
		return (-1);
	    }

	    /* get HASTART into tmp, in hours */
	    if (scansex (kwp->value, &tmp) < 0) {
		msg ("Bad HA format: %s. Should be H:M:S", kwp->value);
		return (-1);
	    }

	    /* get RA now so we can find lststart from HASTART */
	    computeCir (bop);
	    tmp += radhr(bop->scan.obj.s_ra);	/* lst = ha + ra */
	    range (&tmp, 24.0);

	    for (i = 0; i < ndup; i++) {
		bop[i].lststart = tmp;
		bop[i].utcstart = lst2utc (tmp);
		tmp += HRSDUR(bop[i].scan.dur);
		range (&tmp, 24.0);
	    }
	}

	/* force non-data camera scans to have a fixed time */
	if (bop[0].scan.ccdcalib.data == CD_NONE && bop[0].lststart == NOTIME) {
	    msg ("Pure camera calibration scan must have a start time");
	    return (-1);
	}

    /* Parse special features -- overloads ccdcalib with new flag meanings, rao/deco hold values */
    /* set focus to a specific number of microns */
    kwp = getKW(FOCUSPOS);
    if (kwp->sawit) {
        double f;
        if (isanumber(kwp->value) < 0) {
	        msg ("Bad FOCUSPOS format.. must be a number");
	        return (-1);
	    }
        f = atof(kwp->value);
        for(i = 0; i < ndup; i++) {
            bop[i].scan.ccdcalib.newc = CT_FOCUSPOS;
//            bop[i].scan.extval1 = f;
            sprintf(bop[i].scan.extcmd, "%g", f);
        }
    }
    /* set focus to a +/= offset of microns from current position */
    kwp = getKW(FOCUSOFF);
    if (kwp->sawit) {
        double f;
        if (isanumber(kwp->value) < 0) {
	        msg ("Bad FOCUSOFF format.. must be a number");
	        return (-1);
	    }
        f = atof(kwp->value);
        for(i = 0; i < ndup; i++) {
            bop[i].scan.ccdcalib.newc = CT_FOCUSOFF;
            //bop[i].scan.extval1 = f;
            sprintf(bop[i].scan.extcmd, "%g", f);
        }
    }
    /* Call for autofocus to be performed */
    kwp = getKW(AUTOFOCUS);
    if (kwp->sawit) {
        double f;
        if(!strcasecmp(kwp->value,"ON")) {
            f = 1;
        } else if(!strcasecmp(kwp->value,"OFF")) {
            f = 0;
        } else if(!strcasecmp(kwp->value,"PERFORM")) {
            f = 2;
        } else {
	        msg ("Bad AUTOFOCUS value %s",kwp->value);
	        return (-1);
	    }
        for(i = 0; i < ndup; i++) {
            bop[i].scan.ccdcalib.newc = CT_AUTOFOCUS;
            //bop[i].scan.extval1 = f;
            sprintf(bop[i].scan.extcmd, "%g", f);
        }
    }

    /* slew to a given alt/az location and take exposure w/o tracking */
    kwp = getKW(FIXEDALTAZ);
    if (kwp->sawit) {
    	argc = mkargv (kwp->value, argv);
	    if (argc != 2) {
	        msg ("FIXEDALTAZ requires <altitude>, <azimuth> (in degrees DDD:MM:SS)");
    	    return (-1);
	    }
        if(scansex(argv[0], &tmp) < 0) {
            msg("Bad Altitude format: %s", argv[0]);
            return(-1);
        }
        if(tmp <= 0) {
            msg("Altitude must be greater than 0");
            return(-1);
        }
        //bop[0].scan.extval1 = degrad(tmp);
        sprintf(bop[0].scan.extcmd, "%g", degrad(tmp));
        if(scansex(argv[1], &tmp) < 0) {
            msg("Bad Azimuth format: %s", argv[1]);
            return(-1);
        }
        //bop[0].scan.extval2 = degrad(tmp);
        sprintf(bop[0].scan.extcmd, "%s, %g", bop[0].scan.extcmd, degrad(tmp));


    	for (i = 0; i < ndup; i++) {
//	        bop[i].scan.extval1 = bop[0].scan.extval1;
//	        bop[i].scan.extval2 = bop[0].scan.extval2;
    	    strcpy(bop[i].scan.extcmd, bop[0].scan.extcmd);
            bop[i].scan.ccdcalib.newc = CT_FIXEDALTAZ;
    	}
    }
    /* -------------------------------- */

	/* whew!! */
	*nopp += ndup;
	return (0);
}

/* add m more entries to the malloced array of n Obs at *mopp.
 * N.B. allow for *mopp being NULL.
 * exit if fail :-)
 */
static void
growObs (mopp, n, m)
Obs **mopp;
int n;
int m;
{
	int i;

	if (*mopp == NULL) {
	    *mopp = (Obs *)calloc (m, sizeof(Obs));
	    n = 0;
	} else
	    *mopp = (Obs *)realloc (*mopp, sizeof(Obs) *(n+m));

	if (!*mopp) {
	    printf ("Can not malloc %d Obs entries\n", (n+m));
	    exit (1);
	}

	for (i = n; i < n+m; i++)
	    initObs (&(*mopp)[i]);
}

/* return pointer to the KW for item i.
 * check for internal consistency.
 */
static KW *
getKW (i)
int i;
{
	KW *kwp = &kw[i];

	if (kwp->kwcode != i) {
	    printf ("bug! kw[%d].kwcode = %d\n", i, kwp->kwcode);
	    exit(1);
	}

	return (kwp);
}

/* reset all the keyword history back to default condition */
static void
resetKW()
{
	KW *kwp;

	/* reset all the values to null string and reset the sawit flags */
	for (kwp = kw; kwp < &kw[NKW]; kwp++) {
	    kwp->value[0] = '\0';
	    kwp->sawit = 0;
	}

	/* install a few special defaults */
	kwp = getKW (BINNING);
	sprintf (kwp->value, "%d,%d", DEFBIN, DEFBIN);
	kwp = getKW (COMPRESS);
	sprintf (kwp->value, "%d", COMPRESSH);
	kwp = getKW (LSTDELTA);
	sprintf (kwp->value, "%g", LSTDELTADEF);
	kwp = getKW (PRIORITY);
	sprintf (kwp->value, "%d", PRIORITYDEF);
	kwp = getKW (REPEAT);
	strcpy (kwp->value, "1");
	kwp = getKW (SUBIMAGE);
	sprintf (kwp->value, "0,0,%d,%d", DEFIMW, DEFIMH);
	kwp = getKW (CCDCALIB);
	strcpy (kwp->value, "CATALOG");
	kwp = getKW (SOURCE);
	strcpy (kwp->value, "<No name>");
	kwp = getKW (IMAGEDIR);
	strcpy (kwp->value, imdir);
}

/* scan the list of keywords for the one that best matches name.
 * we ignore case and allow partial name matches if it is unique.
 * return pointer into kw[] if find a good one, else write msg and return NULL.
 */
static KW *
scanKW (name)
char *name;
{
	KW *kwp, *kwmatch;
	int l = strlen(name);

	for (kwp = kw, kwmatch = NULL; kwp < &kw[NKW]; kwp++)
	    if (strncasecmp (name, kwp->name, l) == 0) {
		if (strcasecmp (name, kwp->name) == 0) {
		    /* exact match always works */
		    kwmatch = kwp;
		    break;
		}
		if (kwmatch) {
		    char buf[4096];
		    sprintf(buf, "Keyword \"%s\" is ambiguous: could be", name);
		    for (kwp = kw; kwp < &kw[NKW]; kwp++)
			if (strncasecmp (name, kwp->name, l) == 0)
			    sprintf (buf+strlen(buf), " %s", kwp->name);
		    msg ("%s", buf);
		    return (NULL);
		}
		kwmatch = kwp;
	    }

	if (!kwmatch) {
	    msg ("Keyword not found: %s", name);
	    return (NULL);
	}

	return (kwmatch);
}

/* set obj.o_name to name.
 * if RA/DEC/EPOCH are set we set f_RA/f_Dec/f_EPOCH and are done.
 * scan though mop[n] and reuse if already there.
 * look up name in catalogs and fill in *newop.
 * if ccdcalib.data == CD_NONE we don't care at all.
 * return 0 if ok, else print msg and return -1.
 */
static int
doSource (mop, n, newop, name)
Obs mop[];
int n;
Obs *newop;
char name[];
{
	int nsaw;
	Obs *op;
	KW *kwp;

	ACPYZ (newop->scan.obj.o_name, name);

	if (newop->scan.ccdcalib.data == CD_NONE) {
	    newop->scan.obj.o_type = FIXED;
	    newop->scan.obj.f_RA = 0.0;
	    newop->scan.obj.f_dec = 0.0;
	    newop->scan.obj.f_epoch = J2000;
	    return (0);
	}

	nsaw = 0;
	kwp = getKW(RA);    nsaw += kwp->sawit ? 1 : 0;
	kwp = getKW(DEC);   nsaw += kwp->sawit ? 1 : 0;

	if (nsaw == 2) {
	    kwp = getKW(EPOCH);
	    if (!kwp->sawit) {
		msg ("Set EPOCH at least once when using RA/DEC.");
		return (-1);
	    }
	    return (setRADecEpoch (newop));
	}

	if (nsaw > 0) {
	    msg("Set Source/RA/Dec/Epoch or just Source");
	    return (-1);
	}

	/* reuse if source is already in mop.
	 * N.B. newop is in mop[] too !
	 */
	for (op = mop; op < &mop[n]; op++) {
	    if (op == newop)
		continue;
	    if (!strcasecmp (op->scan.obj.o_name, name)) {
		newop->scan.obj = op->scan.obj;
		return (0);
	    }
	}

	/* scan catalogs for source */
	if (searchCatEntry (newop) < 0) {
	    /* searchCatEntry already printed a diagnostic */
	    return (-1);
	}

	return (0);
}

/* fill f_RA/Dec/epoch from RA/DEC/EPOCH keywords.
 * return 0 if ok else msg and -1.
 */
static int
setRADecEpoch (op)
Obs *op;
{
	KW *kwp;
	double tmp;

	kwp = getKW (RA);
	if (kwp->sawit) {
	    if (scansex (kwp->value, &tmp) < 0) {
		msg ("Bad RA format: %s", kwp->value);
		return (-1);
	    }
	    tmp = hrrad(tmp);
	    op->scan.obj.f_RA = tmp;
	    op->scan.obj.o_type = FIXED;
	}

	kwp = getKW (DEC);
	if (kwp->sawit) {
	    if (scansex (kwp->value, &tmp) < 0) {
		msg ("Bad DEC format: %s", kwp->value);
		return (-1);
	    }
	    tmp = degrad(tmp);
	    op->scan.obj.f_dec = tmp;
	    op->scan.obj.o_type = FIXED;
	}

	kwp = getKW (EPOCH);
	if (kwp->sawit) {
	    year_mjd (atof (kwp->value), &tmp);
	    op->scan.obj.f_epoch = tmp;
	    op->scan.obj.o_type = FIXED;
	}

	return (0);
}

/* make a local copy of str and then load argv[] up with each segment of it
 * separated by commas.
 * N.B. the local copy is not reclaimed until we are called again.
 * return the total number of segments found.
 */
static int
mkargv (str, argv)
char *str;
char *argv[];
{
	static char *copy;
	int argc;
	char c;

	if (copy)
	    free (copy);
	copy = malloc (strlen(str)+1);
	strcpy (copy, str);
	str = copy;

	argv[0] = str;
	argc = 0;
	do {
	    c = *str++;
	    if (c == ',' || c == '\0') {
		str[-1] = '\0';
		argv[++argc] = str;
	    }
	} while (c);

	return (argc);
}

/* go through str and look for any nonprintable characters.
 * if find any, sqeeze them out, IN PLACE.
 */
static void
cleanStr (str)
char *str;
{
	char c, *op;

	for (op = str; (c = *str++) != '\0'; )
	    if (isprint (c))
		*op++ = c;

	*op = '\0';
}

/* return 1 if UTDATE is set and it does not match today, else 0.
 * (if UTDATE is not set at all we return 0).
 */
static int
UTDATE_nottoday()
{
	KW *kwp = getKW (UTDATE);

	if (kwp->sawit && dateformatok (kwp->value) == 0 &&
						dateistoday(kwp->value) < 0) {
	    return (1);
	}
	return (0);
}

/* return 0 if vp is a value numeric string, else -1 */
static int
isanumber (char *vp)
{
	double d;

	return (sscanf (vp, "%lf", &d) == 1 ? 0 : -1);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: obs.c,v $ $Date: 2002/03/12 16:46:21 $ $Revision: 1.3 $ $Name:  $"};
