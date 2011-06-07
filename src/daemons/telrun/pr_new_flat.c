/* program to take a flat */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telenv.h"
#include "telstatshm.h"
#include "configfile.h"
#include "strops.h"
#include "scan.h"

//#include "config.h"
#include "telrun.h"

static char dcfn[] = "archive/config/dome.cfg";
static char icfn[] = "archive/config/filter.cfg";

#if ENABLE_DOME_FLATS
static char lcfn[] = "archive/config/lights.cfg";
#endif

static time_t tmpTime;		/* used to save times from one step to next */
static int NFLAT;		/* config entry -- total to take */
static double FLATTAZ;		/* telescope azimuth for dome flat */
static double FLATTALT;		/* telescope altitude for dome flat */
static double FLATDAZ;		/* dome azimuth for dome flat */
static double DOMETOL;		/* max dome position tolerance, rads */
static int filter;		/* index into filinfo we are using */

/* filled with filter/focus/temp info */
static FilterInfo *filinfo;
static int nfilinfo;

static void readCfg(void);
static void scanError(void);
static int findFilter (char f);

#if ENABLE_DOME_FLATS
static double lights_on_to;
static double LTO;
#endif

typedef void *StepFuncP;
typedef StepFuncP (*StepFunc)(time_t now);

static StepFuncP ps_wait4Start(time_t n);
static StepFuncP ps_wait4Setup(time_t n);
static StepFuncP ps_wait4Exp(time_t n);

int
pr_new_flat(int first) 
{	
	static StepFuncP step;

	/* start over if first call */
	if (first) {
	    readCfg();			/* read config entry */
	    step = ps_wait4Start;	/* init sequencer */
	}

	/* run this step, next is its return, done when NULL */
	step = step ? (*(void *(*)())step)(time(NULL)) : NULL;
	return (step ? 0 : -1);
}

void
pr_flatSetup()
{
	Now *np = &telstatshmp->now;
	DomeState ds = telstatshmp->domestate;
	DShState ss = telstatshmp->shutterstate;
	char buf[64];

	/* insure we have everything */
	readCfg();

	/* start filter */
	filter = findFilter (cscan->filter);
	if (IMOT->have) {
	    buf[0] = cscan->filter;
	    buf[1] = '\0';
	    fifoWrite (Filter_Id, buf);
	}

	/* start telescope */
	sprintf (buf, "Alt:%g Az:%g", FLATTALT, FLATTAZ);
	fifoWrite (Tel_Id, buf);

	/* turn on the lights */
#if ENABLE_DOME_FLATS
	if (telstatshmp->lights >= 0
			&& telstatshmp->lights != filinfo[filter].flatlights) {
	    fifoWrite (Lights_Id, "%d", filinfo[filter].flatlights);
	    lights_on_to = mjd + LTO;
	}
#endif

	/* insure focus is stopped */
	if (OMOT->have && OMOT->cvel)
	    fifoWrite (Focus_Id, "Stop");

	/* insure roof is closed */
	if (ss != SH_ABSENT && ss != SH_CLOSING && ss != SH_CLOSED && ss != SH_IDLE)
	    fifoWrite (Dome_Id, "Close");

	/* insure dome is at correct az */
    if(FLATDAZ != 0.0) {
    	if ((ss == SH_ABSENT || ss == SH_CLOSED || ss==SH_IDLE) && ds == DS_STOPPED
	    		    && delra(telstatshmp->dometaz-FLATDAZ) > DOMETOL)
	        fifoWrite (Dome_Id, "Az:%g", FLATDAZ);
    }
}

static StepFuncP
ps_wait4Start(time_t n)
{
	/* check for too late */
	if (n > cscan->starttm + cscan->startdt) {
	    n -= cscan->starttm + cscan->startdt;
	    tlog (cscan, "Flat too late by %d secs", (int) n);
	    scanError();
	    return (NULL);
	}

	/* go! */

	/* N.B. we depend on bias to insure we don't start too early. */

	tlog (cscan, "Checking flat setup");
	pr_flatSetup();

	/* now wait for setup and time to start */
	tmpTime = n + SETUP_TO;
	return (ps_wait4Setup);
}

static StepFuncP
ps_wait4Setup(time_t n)
{
	Now *np = &telstatshmp->now;
	DomeState ds = telstatshmp->domestate;
	int camok = telstatshmp->camstate == CAM_IDLE;
	int telok = telstatshmp->telstate == TS_STOPPED;
	int filok = FILTER_READY;
#if ENABLE_ROTATING_DOME
	int domok = (ds == DS_ABSENT) || (ds == DS_STOPPED  && delra(telstatshmp->domeaz-FLATDAZ) <= DOMETOL);
#else
	int domok = (ds == DS_ABSENT) || (ds == DS_STOPPED);
#endif

#if ENABLE_DOME_FLATS
	int lightok = (mjd > lights_on_to) && (telstatshmp->lights > 0);
#else
	int lightok = 1;
#endif
	int allok;
	Scan *sp = cscan;
	char fullpath[1024];

    // ignore dome rotation if flat az is not defined
    if(FLATDAZ == 0.0) domok = 1;
    allok = camok && telok && filok && domok && lightok;
	tlog(sp, "All ok: %d; CamOK: %d; TelOK: %d; FilOK: %d; DomOK: %d; LightOK: %d\n", allok, camok,telok,filok,domok, lightok);
    // but we do want to be sure the door is closed
    //domok &= (telstatshmp->shutterstate == SH_CLOSED);

	/* trouble if not finished within SETUP */
	if (!allok && n > tmpTime) {
	    tlog (sp, "Flat setup timed out for%s%s%s%s%s",
						    camok ? "" : " camera",
						    telok ? "" : " telescope",
						    filok ? "" : " filter",
						    domok ? "" : " dome");
	    all_stop(1);
	    return (NULL);
	}

	/* don't wait past allowable dt in any case */
	if (n > sp->starttm + sp->startdt) {
	    n -= sp->starttm + sp->startdt;
	    tlog (sp, "Flat setup too late by %d secs", (int) n);
	    all_stop(1);
	    return (NULL);
	}

	/* wait until everything ready */
	if (!allok || n < sp->starttm)
	    return (ps_wait4Setup);

	/* go! */

	/* N.B. we never publish (set starttm/running) because either bias
	 * did already (if not taking data) or regscan will (if taking data).
	 */
	 
	sp->starttm = n;		/* real start time */
	sp->running = 1;		/* we are under way */

	/* start camera */
	sprintf (fullpath, "%s/%s", sp->imagedn, sp->imagefn);
	fifoWrite (Cam_Id,
		    "Expose %d+%dx%dx%d %dx%d %g %d %d %s\n%s\n%s\n%s\n%s\n",
			sp->sx, sp->sy, sp->sw, sp->sh, sp->binx, sp->biny,
			    sp->dur, sp->shutter, sp->priority, fullpath,
			sp->obj.o_name,
			sp->comment,
			sp->title,
			sp->observer);

	/* save estimate of finish time */
	tmpTime = n + (int)floor(sp->dur + 0.5);

	/* ok */
	return (ps_wait4Exp);
}

static StepFuncP
ps_wait4Exp (time_t n)
{
	DomeState ds = telstatshmp->domestate;
	int telok = telstatshmp->telstate == TS_STOPPED;
	int filok = FILTER_READY;
	int focok = FOCUS_READY;
#if ENABLE_ROTATING_DOME
	int domok = (ds == DS_ABSENT) || (ds == DS_STOPPED  && delra(telstatshmp->domeaz-FLATDAZ) <= DOMETOL);
#else
	int domok = (ds == DS_ABSENT) || (ds == DS_STOPPED);
#endif

	#if ENABLE_DOME_FLATS
	int lightok = telstatshmp->lights > 0;
#else
	int lightok = 1;
#endif
	Scan *sp = cscan;
	/* double-check support systems */
	if (!telok || !filok || !domok || !focok || !lightok) {
	    tlog (sp, "Error during exposure from%s%s%s%s",
						    focok ? "" : " focus",
						    telok ? "" : " telescope",
						    filok ? "" : " filter",
						    domok ? "" : " dome");
	    all_stop(1);
	    return (NULL);
	}

	/* wait for completion..
	 * we could be a little early -- or so fast it is still IDLE
	 */
	if (n <= tmpTime || telstatshmp->camstate == CAM_EXPO || telstatshmp->camstate == CAM_READ)
	    return (ps_wait4Exp);	/* waiting... */

	/* exposure is finished: copy scan info to bkg_scan and start a
	 * new program to wait for download and start postprocessing.
	 * marking 'D' is really just to prevent this scan from being repeated.
	 * can still be marked 'F' if starting postprocess fails.
	 */
	markScan (scanfile, sp, 'D');
	sp->running = 0;
	sp->starttm = 0;
	if(sp->extcmd[0] == 'L') {
		fifoWrite (Lights_Id,"off");
	}
	
	/* all up to post processing now */
	return (NULL);
}

static void
readCfg()
{
#define NCCFG   (sizeof(ccfg)/sizeof(ccfg[0]))
#define NDCFG   (sizeof(dcfg)/sizeof(dcfg[0]))
#define NLCFG	(sizeof(lcfg)/sizeof(lcfg[0]))
	
	static CfgEntry ccfg[] = {
	    {"NFLAT",   CFG_INT, &NFLAT},
	};

	static CfgEntry dcfg[] = {
	    {"DOMETOL",	 CFG_DBL, &DOMETOL},
	    {"FLATTAZ",	 CFG_DBL, &FLATTAZ},
	    {"FLATTALT", CFG_DBL, &FLATTALT},
	    {"FLATDAZ",	 CFG_DBL, &FLATDAZ},
	};

#if ENABLE_DOME_FLATS
	static CfgEntry lcfg[] = {
		{"LCLOSETO", CFG_DBL, &LTO},
	};
#endif

	char buf[1024];
	int n;

	n = readCfgFile (0, ccfn, ccfg, NCCFG);
	if (n != NCCFG) {
	    tlog (cscan, "%s: no %s", ccfn, ccfg[0].name);
	    die();
	}

	n = readCfgFile (0, dcfn, dcfg, NDCFG);
	if (n != NDCFG) {
	    tlog (cscan, "%s: no %s, %s, %s or %s", dcfn, dcfg[0].name,
				    dcfg[1].name, dcfg[2].name, dcfg[3].name);
	    die();
	}

#if ENABLE_DOME_FLATS
	n = readCfgFile (0, lcfn, lcfg, NLCFG);
	if (n != NLCFG) {
	    tlog (cscan, "%s: no %s", lcfn, lcfg[0].name);
	    die();
	}
	
	LTO /= SPD;
#endif

    nfilinfo = readFilterCfg (0, icfn, &filinfo, NULL, buf);
	if (nfilinfo <= 0) {
	    if (nfilinfo < 0)
		tlog (cscan, "%s: %s", basenm(icfn), buf);
	    else
		tlog (cscan, "%s: no entries", basenm(icfn));
	    die();
	}
}

static void
scanError()
{
	markScan (scanfile, cscan, 'F');
	cscan->running = 0;
	cscan->starttm = 0;
#if ENABLE_DOME_FLATS
	if (telstatshmp->lights > 0)
	    fifoWrite (Lights_Id, "0");
#endif
}

/* find filter f in filinfo and return index */
static int
findFilter (char f)
{
	char capf = islower(f) ? toupper(f) : f;
	int i;

	for (i = 0; i < nfilinfo; i++) {
	    char capn = filinfo[i].name[0];
	    if (islower(capn))
		capn = toupper(capn);
	    if (capn == capf)
		return (i);
	}

	/* ?? */
	tlog (cscan, "No config entry for filter %c", f);
	return (0);
}

