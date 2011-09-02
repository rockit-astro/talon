/* changed 20020305 by jca:
 *			added focusVal to Obs structure
 */

/* info about one scan to sort */
typedef struct {
    /* basic scan definition */
    Scan scan;

    /* other information we just use locally */
    double lststart;	/* LSTSTART, hrs; NOTIME if don't care */
    char date[11];	/* mm/dd/yyyy UT date to run, or "" */
    RiseSet rs;		/* rise/set info */
    double utcstart;	/* utc when this run should start, else NOTIME
			 * if don't care. this is only set when
			 * lststart is set or a set of scans is sorted.
			 */
    char yoff[50];	/* iff off: why this scan is turned off */

    /* state info */
    int done : 1;	/* set when done */
    int off : 1;	/* set when turned off */
    int elig : 1;	/* set when eligible (astro/phys ok to view) */

    /* flag used during sorting */
    int sorted : 1;	/* set when a spot has been picked for this scan */
    
#ifndef CHKSCH
    /* set to the shell of an edit window while it's in effect */
    Widget editsh;
#endif /* CHKSCH */
} Obs;

#define	NOTIME		(-49289.0)	/* any unlikely exact number */

/* scheduling granularity */
#define	SECSPERSLOT	20		/* scheduling granularity, secs */
#define	SECS2SLOTS(dur)	((int)ceil((dur+CAMDIG_MAX)/SECSPERSLOT))
#define	TOTSECS(dur)	(SECS2SLOTS(dur)*SECSPERSLOT)

/* info about the widgets to display and control an Obs record */

/* one per each field within the Obs struct that we want to display.
 */
enum {
    OD_NAME = 0, OD_RA, OD_DEC, OD_EPOCH, OD_HA, OD_ALT, OD_AZ,
    OD_DUR, OD_FILT, OD_RISETM, OD_TRANSTM, OD_SETTM, OD_UTCSTART,
    OD_N
};

#ifndef CHKSCH
typedef struct {
    Widget container;	/* whatever contains the other stuff */
    Widget off;		/* first pushbutton */
    Widget edit;	/* second pushbutton */
    Widget f[OD_N];	/* the various field widgets */
    Widget editsh;	/* shell of edit menu for this object */
} ObsDsp;

extern void setObsDspEmpty (ObsDsp *odp);
extern void setObsDspColor (ObsDsp *odp, Obs *op);
extern void initObsDsp (ObsDsp *odp, Widget parent, char *name);
extern void fillTimeDObsDsp (ObsDsp *odp, Obs *op, Now *np);
extern void fillFixedObsDsp (ObsDsp *odp, Obs *op);

extern void opencat_cb (Widget w, XtPointer client, XtPointer call);
extern void stdp_cb (Widget w, XtPointer client, XtPointer call);
extern void pting_cb (Widget w, XtPointer client, XtPointer call);
extern void camcal_cb (Widget w, XtPointer client, XtPointer call);
extern Widget toplevel_w;
#endif /* CHKSCH */

extern double elapsedt (double t1, double t2);
extern double lst2utc (double l);
extern int at_night (double utc);
extern int dateformatok (char *s);
extern int dateistoday (char *s);
extern int in_timeorder (double t1, double t2, double t3);
extern int is_evening (Obs *op);
extern int is_morning (Obs *op);
extern int is_night (Obs *op);
extern void illegalCCDCalib (char str[]);
extern void illegalCCDSO (char str[]);
extern int legalFilters (int ac, char *av[]);
extern int readCatFile (char *fn, Obs **opp);
extern int readObsFile (char *fn, Obs **opp);
extern int searchCatEntry (Obs *op);
extern int wantinsls (Obs *op);
extern void addSchedEntries (Obs *newobs, int nnewobs);
extern void computeCir (Obs *op);
extern void cpyObs (Obs *dst, Obs *src);
extern void dawnduskToday (double *mjddawnp, double *mjdduskp);
extern void deleteAllSchedEntries (void);
extern void deleteSchedEntries (char *schedfn);
extern void editRun (Obs *op);
extern void get_obsarray (Obs **opp, int *nopp);
extern void initObs (Obs *op);
extern void manageCatFileMenu(void);
extern void manageSLSFileMenu(void);
extern void manageDeleteSchedMenu(void);
extern void msg (char *fmt, ...);
extern void newRun (void);
extern int print_sls(Now *np, Obs *workop, int nworkop, char *dir, char *fn);
extern int print_summary(Now *np, Obs *op, int nop, char *fn);
extern void sg_manage(void);
extern void sg_update(void);
extern void sortscans (Now *np, Obs *workop, int nworkop);
extern void updateDeleteSchedMenu(void);
extern void updateScrolledList(void);
extern void watch_cursor(int want);

extern FilterInfo *filterp;
extern Now now;
extern char *myclass;
extern char FDEFLT;
extern char MESHFILTER;
extern char imdir[];
extern double CAMDIG_MAX;
extern double LSTDELTADEF;
extern double MAXALT;
extern double MAXDEC;
extern double MAXHA;
extern double MESHEXPTIME;
extern double MINALT;
extern double PTGRAD;
extern double THERMDUR;
extern int COMPRESSH;
extern int DEFBIN, DEFIMW, DEFIMH;
extern int IGSUN;
extern int MESHCOMP;
extern int NBIAS;
extern int NFLAT;
extern int NTHERM;
extern int nfilt;
extern double PHOTBDUR;
extern double PHOTVDUR;
extern double PHOTRDUR;
extern double PHOTIDUR;

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: telsched.h,v $ $Date: 2002/03/12 16:46:21 $ $Revision: 1.3 $ $Name:  $
 */
