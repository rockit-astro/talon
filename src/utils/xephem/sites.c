/* code to read and manipulate the file of sites.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#if defined(__STDC__)
#include <stdlib.h>
typedef const void * qsort_arg;
#else
typedef void * qsort_arg;
extern void *malloc(), *realloc();
#endif

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/SelectioB.h>
#include <Xm/FileSB.h>
#include <Xm/List.h>
#include <Xm/TextF.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "sites.h"

extern Widget	toplevel_w;
#define XtD     XtDisplay(toplevel_w)
extern Colormap xe_cm;
extern char maincategory[];

extern FILE *fopenh P_((char *name, char *how));
extern Now *mm_get_now P_((void));
extern char *getPrivateDir P_((void));
extern char *getShareDir P_((void));
extern char *syserrstr P_((void));
extern void defaultTextFN P_((Widget w, int setcols, char *x, char *y));
extern void e_update P_((Now *np, int force));
extern void get_xmstring P_((Widget w, char *resource, char **txtp));
extern void hlp_dialog P_((char *tag, char *deflt[], int ndeflt));
extern void mm_setsite P_((Site *sp, int update));
extern void mm_sitename P_((char *name));
extern void prompt_map_cb P_((Widget w, XtPointer client, XtPointer call));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void set_xmstring P_((Widget w, char *resource, char *text));
extern void sr_reg P_((Widget w, char *res, char *cat, int autosav));
extern void wtip P_((Widget w, char *tip));
extern void xe_msg P_((char *msg, int app_modal));

static void canonstr P_((char *str, char *canon));
static void create_sq_w P_((void));
static void sites_fillsl P_((void));
static int sites_read_file P_((void));
static int sites_cmpf P_((qsort_arg v1, qsort_arg v2));
static void sites_scroll P_((int i));
static void sq_set_cb P_((Widget w, XtPointer client, XtPointer call));
static void sq_newfn_cb P_((Widget w, XtPointer client, XtPointer call));
static void sq_browse_cb P_((Widget w, XtPointer client, XtPointer call));
static void sq_cancel_cb P_((Widget w, XtPointer client, XtPointer call));
static void sq_search_cb P_((Widget w, XtPointer client, XtPointer call));
static void sq_dblclick_cb P_((Widget w, XtPointer client, XtPointer call));
static void sq_click_cb P_((Widget w, XtPointer client, XtPointer call));
static void sq_help_cb P_((Widget w, XtPointer client, XtPointer call));

static Widget sq_w;		/* sites query form dialog */
static Widget sql_w;		/* sites query scrolled list */
static Widget fsb_w;            /* FSB */
static Widget fntf_w;		/* file name text field */
static Widget srtf_w;		/* search text field */
static Widget settf_w;		/* set text field */

#define	MAXSITELLEN	512	/* maximum site file line length */
static Site *sites;		/* malloced list of Sites */
static int nsites;		/* number of entries in sites array */

/* if sites has been set return nsites and give caller our sites array,
 * else try again to read a sites file. if still no luck, return -1.
 * N.B. caller can only use the array returned until another site file is read.
 */
int
sites_get_list (sipp)
Site **sipp;
{
	if (!sites && sites_read_file() < 0)
	    return (-1);

	if (sipp)
	    *sipp = sites;
	return (nsites);
}

/* let user choose a site from a scrolled list, if there is one */
void
sites_query()
{
	if (!sites && sites_read_file() < 0)
	    return;

	sites_fillsl ();
	XtManageChild (sq_w);
}

/* search the sites list for one containing str, ignoring case, whitespace
 * and punct. start after the current selection in the list, if any.
 * return index into sites[] if find, else return -1.
 */
int
sites_search (str)
char *str;
{
	int *poslist, poscount;
	int startpos;
	char s1[MAXSITELLEN];
	int i, n;

	if (!sites && sites_read_file() < 0)
	    return (-1);

	/* decide where to start */
	if (XmListGetSelectedPos (sql_w, &poslist, &poscount) == True) {
	    startpos = poslist[0]%nsites;	/* 1-based so already +1 */
	    XtFree ((char *)poslist);
	} else
	    startpos = 0;

	/* make canonized copy of str in s1 */
	canonstr (str, s1);

	/* after doing the same for each name in sites[], check each
	 * possible offset location starting at startpos.
	 */
	for (n = 0, i = startpos; n < nsites; n++, i = (i+1)%nsites) {
	    Site *sip = &sites[i];
	    char s2[MAXSITELLEN];

	    canonstr (sip->si_name, s2);
	    if (strstr (s2, s1))
		return (i);
	}

	return (-1);
}

/* fill ab[maxn] with an abbreviated version of full.
 * N.B. allow for full == NULL or full[0] == '\0'.
 */
void
sites_abbrev (full, ab, maxn)
char *full, ab[];
int maxn;
{
	int fl;
	int n;

	/* check edge conditions */
	if (!full || (fl = strlen(full)) == 0)
	    return;

	/* just copy if it all fits ok */
	if (fl < maxn-1) {
	    (void) strcpy (ab, full);
	    return;
	}

	/* clip off words from the right until short enough.
	 * n is an index, not a count.
	 */
	for (n = fl-1; n >= maxn-4; ) {
	    while (n > 0 && isalnum(full[n]))
		n--;
	    while (n > 0 && (ispunct(full[n]) || isspace(full[n])))
		n--;
	}
	(void) sprintf (ab, "%.*s...", n+1, full);
}

/* copy str to canon (up to MAXSITELLEN) removing all whitespace, punctuation
 * and converting all lower case to uppercase.
 */
static void
canonstr (str, canon)
char *str;
char *canon;
{
	int i;
	char c;

	for (i = 0; (c = *str++) != '\0'; ) {
	    if (isspace(c) || ispunct(c))
		continue;
	    if (islower(c))
		c = toupper(c);
	    if (i < MAXSITELLEN-1)
		canon[i++]  = c;
	}
	canon[i] = '\0';
}

/* make the site selection dialog */
static void
create_sq_w()
{
	Widget w, br_w, cl_w;
	Arg args[20];
	int n;
	
	/* create outter form dialog */

	n = 0;
	XtSetArg (args[n], XmNautoUnmanage, False);  n++;
	XtSetArg (args[n], XmNverticalSpacing, 7); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 7); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	sq_w = XmCreateFormDialog(toplevel_w, "Sites", args, n);
	set_something (sq_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (sq_w, XmNmapCallback, prompt_map_cb, NULL);
	XtAddCallback (sq_w, XmNhelpCallback, sq_help_cb, NULL);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "xephem Site Selection"); n++;
	XtSetValues (XtParent(sq_w), args, n);

	/* make Close and Help across the bottom */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 20); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 40); n++;
	cl_w = XmCreatePushButton (sq_w, "Close", args, n);
	wtip (cl_w, "Close this window");
	XtAddCallback (cl_w, XmNactivateCallback, sq_cancel_cb, NULL);
	XtManageChild (cl_w);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 60); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 80); n++;
	w = XmCreatePushButton (sq_w, "Help", args, n);
	wtip (w, "Get more info about this window");
	XtAddCallback (w, XmNactivateCallback, sq_help_cb, NULL);
	XtManageChild (w);

	/* file browse PB */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, cl_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 22); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	br_w = XmCreatePushButton (sq_w, "BPB", args, n);
	set_xmstring (br_w, XmNlabelString, "Browse...");
	wtip (br_w, "Choose another sites file");
	XtAddCallback (br_w, XmNactivateCallback, sq_browse_cb, NULL);
	XtManageChild (br_w);

	/* current file */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, br_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 20); n++;
	w = XmCreatePushButton (sq_w, "FNL", args, n);
	XtAddCallback (w, XmNactivateCallback, sq_newfn_cb, NULL);
	set_xmstring (w, XmNlabelString, "File");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, br_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNcolumns, 30); n++;
	fntf_w = XmCreateTextField (sq_w, "Filename", args, n);
	defaultTextFN (fntf_w, 1, getShareDir(), "auxil/xephem.sit");
	sr_reg (fntf_w, NULL, maincategory, 1);
	wtip (fntf_w, "Site file name");
	XtAddCallback (fntf_w, XmNactivateCallback, sq_newfn_cb, NULL);
	XtManageChild (fntf_w);

	/* make a PB and TF to enter the set string above that */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, fntf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 20); n++;
	w = XmCreatePushButton (sq_w, "Set", args, n);
	XtAddCallback (w, XmNactivateCallback, sq_set_cb, NULL);
	wtip (w, "Set Main site to string at right");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, fntf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNcolumns, 30); n++;
	settf_w = XmCreateTextField (sq_w, "SetTF", args, n);
	wtip (settf_w, "Candidate site");
	XtAddCallback (settf_w, XmNactivateCallback, sq_set_cb, NULL);
	XtManageChild (settf_w);

	/* make a PB and TF to enter the search string above that */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, settf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 20); n++;
	w = XmCreatePushButton (sq_w, "Search", args, n);
	XtAddCallback (w, XmNactivateCallback, sq_search_cb, NULL);
	wtip (w, "Search for next Site containing string at right");
	XtManageChild (w);

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, settf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNleftWidget, w); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNcolumns, 30); n++;
	srtf_w = XmCreateTextField (sq_w, "SearchTF", args, n);
	wtip (srtf_w, "Candidate search string");
	XtAddCallback (srtf_w, XmNactivateCallback, sq_search_cb, NULL);
	XtManageChild (srtf_w);

	/* make the scrolled list at the top */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, srtf_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNselectionPolicy, XmBROWSE_SELECT); n++;
	sql_w = XmCreateScrolledList (sq_w, "SiteSL", args, n);
	wtip (sql_w, "Sites.. click to copy below, double-click to Set too");
	XtAddCallback (sql_w, XmNdefaultActionCallback, sq_dblclick_cb, NULL);
	XtAddCallback (sql_w, XmNbrowseSelectionCallback, sq_click_cb, NULL);
	XtManageChild (sql_w);
}

/* shift the scrolled list so sites[i] is selected and visible */
static void
sites_scroll (i)
int i;
{
	XmListSetPos (sql_w, i+1);	 	/* scroll it to top */
	XmListSelectPos (sql_w, i+1, False);	/* just highlight it */
}

/* read the sites file named in fntf_w and set sites and nsites if
 * successful to the list sorted by name.
 * return 0 if ok, else write to xe_msg() and return -1.
 */
static int
sites_read_file ()
{
	char buf[MAXSITELLEN];
	char msg[1024];
	int newn;
	Site *news;
	String fn;
	FILE *fp;

	/* create fntf_w if first call */
	if (!fntf_w)
	    create_sq_w();

	/* open the sites file */
	fn = XmTextFieldGetString (fntf_w);
	fp = fopenh (fn, "r");
	if (!fp) {
	    (void) sprintf(msg, "%s:\n%s", fn, syserrstr());
	    xe_msg (msg, 1);
	    XtFree (fn);
	    return (-1);
	}

	/* read each entry, building up list */
	news = NULL;
	newn = 0;
	while (fgets (buf, sizeof(buf), fp) != NULL) {
	    char name[MAXSITELLEN];
	    char tzdefn[128];
	    int latd, latm, lats;
	    int lngd, lngm, lngs;
	    char latNS, lngEW;
	    double lt, lg;
	    double ele;
	    Site *sp;
	    int l;
	    int nf;

	    /* read line.. skip if not complete. tz is optional */
	    tzdefn[0] = '\0';
	    nf = sscanf (buf, "%[^;]; %3d %2d %2d %c   ; %3d %2d %2d %c   ;%lf ; %s",
				    name, &latd, &latm, &lats, &latNS,
				    &lngd, &lngm, &lngs, &lngEW, &ele, tzdefn);
	    if (nf < 10)
		continue;

	    /* strip trailing blanks off name */
	    for (l = strlen (name); --l >= 0; )
		if (isspace(name[l]))
		    name[l] = '\0';
		else
		    break;

	    /* crack location */
	    lt = degrad (latd + latm/60.0 + lats/3600.0);
	    if (latNS == 'S')
		lt = -lt;
	    lg = degrad (lngd + lngm/60.0 + lngs/3600.0);
	    if (lngEW == 'W')
		lg = -lg;

	    /* extend news array */
	    news = (Site *) XtRealloc ((void *)news, (newn+1)*sizeof(Site));
	    sp = &news[newn++];

	    /* fill a new Site record */
	    memset ((void *)sp, 0, sizeof(Site));
	    sp->si_lat = (float)lt;
	    sp->si_lng = (float)lg;
	    sp->si_elev = (float)ele;
	    (void) strncpy (sp->si_tzdefn, tzdefn, sizeof(sp->si_tzdefn)-1);
	    (void) strncpy (sp->si_name, name, sizeof(sp->si_name)-1);
	}

	/* finished reading file */
	(void) fclose (fp);

	/* retain old list if nothing in new file */
	if (newn == 0) {
	    sprintf (msg, "%s:\nContains no sites", fn);
	    XtFree (fn);
	    return (-1);
	}

	/* sort by name and install */
	qsort ((void *)news, newn, sizeof(Site), sites_cmpf);
	if (sites)
	    XtFree ((void*)sites);
	sites = news;
	nsites = newn;

	return (0);
}

/* (re)fill the scrolled list sql_w with the set of sites.
 * also tell earth view to redraw.
 */
static void
sites_fillsl ()
{
	XmString *xms;
	int i;

	/* build array of XmStrings for fast updating of the ScrolledList */
	xms = (XmString *) XtMalloc (nsites * sizeof(XmString));
	for (i = 0; i < nsites; i++)
	    xms[i] = XmStringCreateSimple (sites[i].si_name);
	XmListDeleteAllItems (sql_w);
	XmListAddItems (sql_w, xms, nsites, 0);

	/* finished with the XmStrings table */
	for (i = 0; i < nsites; i++)
	    XmStringFree (xms[i]);
	XtFree ((void *)xms);

	/* update earth list */
	e_update (mm_get_now(), 1);
}

/* compare name portions of two pointers to Sites in qsort fashion.
 */
static int
sites_cmpf (v1, v2)
qsort_arg v1;
qsort_arg v2;
{
	char *name1 = ((Site *)v1)->si_name;
	char *name2 = ((Site *)v2)->si_name;

	return (strcmp (name1, name2));
}

/* called when CR is hit in the Set text field /or/ from Set PB.
 */
/* ARGSUSED */
static void
sq_set_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char *str = XmTextFieldGetString (settf_w);
	int i;

	/* if text field matches a site, use the full definition, else just
	 * use the simple string.
	 */
	for (i = 0; i < nsites; i++) {
	    if (!strcmp (sites[i].si_name, str)) {
		sites_scroll (i);
		mm_setsite(&sites[i], 0);
		break;
	    }
	}
	if (i == nsites)
	    mm_sitename (str);

	XtFree (str);
}

/* callback from Enter in the file name TF /or/ from File PB */
/* ARGSUSED */
static void
sq_newfn_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (sites_read_file() == 0)
	    sites_fillsl ();
}

/* callback from the FSB Close PB */
/* ARGSUSED */
static void
sq_fsbcancel_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (fsb_w)
	    XtUnmanageChild (fsb_w);
}

/* callback from the FSB Ok PB */
/* ARGSUSED */
static void
sq_fsbok_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (fsb_w) {
	    char *fn;

	    get_xmstring (fsb_w, XmNdirSpec, &fn);
	    XmTextFieldSetString (fntf_w, fn);
	    XtFree (fn);
	    XtUnmanageChild (fsb_w);
	    if (sites_read_file() == 0)
		sites_fillsl ();
	}
}

/* callback from the Browse PB */
/* ARGSUSED */
static void
sq_browse_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	if (!fsb_w) {
	    Widget w;
	    Arg args[20];
	    int n;

	    n = 0;
	    XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	    XtSetArg (args[n], XmNtitle, "Sites File browser"); n++;
	    XtSetArg (args[n], XmNwidth, 400); n++;
	    XtSetArg (args[n], XmNmarginHeight, 10); n++;
	    XtSetArg (args[n], XmNmarginWidth, 10); n++;
	    fsb_w = XmCreateFileSelectionDialog (toplevel_w, "SFSB", args, n);
	    XtAddCallback (fsb_w, XmNmapCallback, prompt_map_cb, NULL);
	    XtAddCallback (fsb_w, XmNokCallback, sq_fsbok_cb, NULL);
	    XtAddCallback (fsb_w, XmNcancelCallback, sq_fsbcancel_cb, NULL);
	    set_xmstring (fsb_w, XmNcancelLabelString, "Close");

	    /* set default dir and pattern */
	    set_xmstring (fsb_w, XmNdirectory, getPrivateDir());
	    set_xmstring (fsb_w, XmNpattern, "*.sit");

	    w = XmFileSelectionBoxGetChild (fsb_w, XmDIALOG_HELP_BUTTON);
	    XtUnmanageChild (w);
	}

	XtManageChild (fsb_w);
}

/* called when the Cancel button is hit */
/* ARGSUSED */
static void
sq_cancel_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (sq_w);
}

/* called when CR is hit in the search text field /or/ from Search PB */
/* ARGSUSED */
static void
sq_search_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	char *str;
	int i;

	str = XmTextFieldGetString (srtf_w);
	i = sites_search (str);
	if (i < 0)
	    xe_msg ("No matching site found.", 1);
	else {
	    /* Set and scroll */
	    XmTextFieldSetString (settf_w, sites[i].si_name);
	    sites_scroll (i);
	}
	XtFree (str);
}

/* called when an item in the scrolled list is double-clicked */
/* ARGSUSED */
static void
sq_dblclick_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int *pos, npos;
	int free;

	/* put into Set field and install in Main, all at once */
	if ((free = XmListGetSelectedPos (sql_w, &pos, &npos)) && 
			npos == 1 && pos[0] > 0 && pos[0] <= nsites) {
	    Site *sip = &sites[pos[0]-1]; /* pos is 1-based */
	    XmTextFieldSetString (settf_w, sip->si_name);
	    mm_setsite(sip, 0);
	} else {
	    xe_msg ("Bogus list selection", 1);
	}

	if (free)
	    XtFree ((char *)pos);
}

/* called when an item in the scrolled list is single-clicked */
/* ARGSUSED */
static void
sq_click_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	int *pos, npos;
	int free;

	/* copy to set field */
	if ((free = XmListGetSelectedPos (sql_w, &pos, &npos)) && 
			npos == 1 && pos[0] > 0 && pos[0] <= nsites) {
	    Site *sip = &sites[pos[0]-1]; /* pos is 1-based */
	    XmTextFieldSetString (settf_w, sip->si_name);
	} else {
	    xe_msg ("Bogus list selection", 1);
	}

	if (free)
	    XtFree ((char *)pos);
}

/* callback from the Help button
 */
/* ARGSUSED */
static void
sq_help_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	static char *msg[] = {
"Load and select from a list of sites."
};

	hlp_dialog ("MainMenu -- sites dialog", msg, XtNumber(msg));
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: sites.c,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $"};
