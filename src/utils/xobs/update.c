/* update the display based on shared memory */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include <Xm/Xm.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <X11/keysym.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "strops.h"
#include "configfile.h"
#include "misc.h"
#include "telstatshm.h"
#include "xtools.h"

#include "xobs.h"
#include "widgets.h"

#define	SLOW_DT		(5./SPD)	/* days between "slow" updates */
#define	FAST_DT		(.2/SPD)	/* days between "fast" updates */
#define	COAST_DT	(2./SPD)	/* days for fast after busy */
#define	MAXTEMPERR	2		/* camtemp error to show WARN */

static void curPos (void);
static void noPos (void);
static void curTarg (void);
static void noTarg (void);
static void computeSunMoon(void);
static void showTime (void);
static void showSunMoon (void);
static void showFocus(void);
static void showScope(void);
static void showHL(void);
static void showWx(void);
static void showDome(void);
static void showTemps(void);

static char blank[] = " ";

/* update the display.
 * called periodically and on specific impulses.
 * if force, redraw everything, else just what seems timely.
 */
void
updateStatus(int force)
{

	static double last_slow, last_fast;
	static double last_tbusy, last_dbusy, last_obusy;
	static double last_wbusy;
	Now *np = &telstatshmp->now;
	int doslow = force || mjd > last_slow + SLOW_DT;
	int dofast = force || mjd > last_fast + FAST_DT;
	TelState ts = telstatshmp->telstate;
	DomeState ds = telstatshmp->domestate;
	DShState ss = telstatshmp->shutterstate;
	int busy;

	/* do once per swing through */
	soundIsOn();

	/* always do these at least occasionally */
	if (doslow) {
	    computeSunMoon();
	    showSunMoon();
	    last_slow = mjd;
	}

	/* always do these at least more often */
	if (dofast) {
	    showTime();
	    last_fast = mjd;
	}

	/* do these at least occasionally or especially often when busy */

	busy = telstatshmp->wxs.alert;
	if (doslow || busy || mjd < last_wbusy + COAST_DT) {
	    showWx();
	    showTemps();
	    if (busy)
		last_wbusy = mjd;
	}

	busy = ts==TS_SLEWING || ts==TS_HUNTING || ts==TS_LIMITING;
	if (doslow || busy || mjd < last_tbusy + COAST_DT) {
	    showSkyMap();
	    if (busy)
		last_tbusy = mjd;
	}

	busy = ds==DS_ROTATING || ds==DS_HOMING || ss==SH_OPENING || ss==SH_CLOSING;
	if (doslow || busy || mjd < last_dbusy + COAST_DT) {
	    showDome();
	    if (busy)
		last_dbusy = mjd;
	}

	busy = OMOT->cvel != 0;
	if (doslow || busy || mjd < last_obusy + COAST_DT) {
	    showFocus();
	    if (busy)
		last_obusy = mjd;
	}

	/* always be very responsive to the scope */
	showScope();
	showHL();
}

static void
curPos ()
{
	static int last_haver = -1;
	char buf[32];

	fs_sexa (buf, radhr(telstatshmp->CJ2kRA), 2, 36000);
	wtprintf (g_w[PCRA_W], "%s", buf);

	fs_sexa (buf, raddeg(telstatshmp->CJ2kDec), 4, 3600);
	wtprintf (g_w[PCDEC_W], "%s", buf);

	fs_sexa (buf, radhr(telstatshmp->CAHA), 2, 36000);
	wtprintf (g_w[PCHA_W], "%s", buf);

	fs_sexa (buf, raddeg(telstatshmp->Calt), 4, 3600);
	wtprintf (g_w[PCALT_W], "%s", buf);

	fs_sexa (buf, raddeg(telstatshmp->Caz), 4, 3600);
	wtprintf (g_w[PCAZ_W], "%s", buf);

	/* even have a rotator? */
	if (RMOT->have != last_haver) {
	    if (RMOT->have) {
		XtSetSensitive (g_w[CRL_W], True);
		XtSetSensitive (g_w[CR_W], True);
	    } else {
		XtSetSensitive (g_w[CRL_W], False);
		XtSetSensitive (g_w[CR_W], False);
		setLt (g_w[CRLT_W], LTIDLE);
	    }
	    last_haver = RMOT->have;
	}

	if (RMOT->have) {
	    double r = RMOT->cpos + telstatshmp->tax.R0;
	    fs_sexa (buf, raddeg(r), 3, 60);
	    wtprintf (g_w[CR_W], "%8s", buf);
	}
}

static void
noPos ()
{
	wtprintf (g_w[PCRA_W], blank);
	wtprintf (g_w[PCDEC_W], blank);
	wtprintf (g_w[PCHA_W], blank);
	wtprintf (g_w[PCALT_W], blank);
	wtprintf (g_w[PCAZ_W], blank);
	wtprintf (g_w[PCDAZ_W], blank);
	wtprintf (g_w[CR_W], blank);
}

static void
curTarg ()
{
	char buf[32];
	double tmp;

	/* target */

	fs_sexa (buf, radhr(telstatshmp->DJ2kRA), 2, 36000);
	wtprintf (g_w[PTRA_W], "%s", buf);

	fs_sexa (buf, raddeg(telstatshmp->DJ2kDec), 4, 3600);
	wtprintf (g_w[PTDEC_W], "%s", buf);

	fs_sexa (buf, radhr(telstatshmp->DAHA), 2, 36000);
	wtprintf (g_w[PTHA_W], "%s", buf);

	fs_sexa (buf, raddeg(telstatshmp->Dalt), 4, 3600);
	wtprintf (g_w[PTALT_W], "%s", buf);

	fs_sexa (buf, raddeg(telstatshmp->Daz), 4, 3600);
	wtprintf (g_w[PTAZ_W], "%s", buf);

	/* differences */

	tmp = delra (telstatshmp->CJ2kRA - telstatshmp->DJ2kRA);
	fs_sexa (buf, radhr(tmp), 2, 36000);
	wtprintf (g_w[PDRA_W], "%s", buf);

	tmp = telstatshmp->CJ2kDec - telstatshmp->DJ2kDec;
	fs_sexa (buf, raddeg(tmp), 4, 3600);
	wtprintf (g_w[PDDEC_W], "%s", buf);

	tmp = delra (telstatshmp->CAHA - telstatshmp->DAHA);
	fs_sexa (buf, radhr(tmp), 2, 36000);
	wtprintf (g_w[PDHA_W], "%s", buf);

	tmp = telstatshmp->Calt - telstatshmp->Dalt;
	fs_sexa (buf, raddeg(tmp), 4, 3600);
	wtprintf (g_w[PDALT_W], "%s", buf);

	tmp = telstatshmp->Caz - telstatshmp->Daz;
	fs_sexa (buf, raddeg(tmp), 4, 3600);
	wtprintf (g_w[PDAZ_W], "%s", buf);
}

static void
noTarg ()
{
	wtprintf (g_w[PTRA_W], blank);
	wtprintf (g_w[PTDEC_W], blank);
	wtprintf (g_w[PTHA_W], blank);
	wtprintf (g_w[PTALT_W], blank);
	wtprintf (g_w[PTAZ_W], blank);

	wtprintf (g_w[PDRA_W], blank);
	wtprintf (g_w[PDDEC_W], blank);
	wtprintf (g_w[PDHA_W], blank);
	wtprintf (g_w[PDALT_W], blank);
	wtprintf (g_w[PDAZ_W], blank);
}

static void
showTime()
{
	Now *np = &telstatshmp->now;
	double dawn, dusk;
	double tmp, lst;
	double d;
	int m, y;
	int rsstatus;
	char buf[64];

	tmp = utc_now (np);
	fs_sexa (buf, tmp, 2, 3600);
	wtprintf (g_w[IUT_W], "  %s", buf);

	tmp -= tz;
	range (&tmp, 24.0);
	fs_sexa (buf, tmp, 2, 3600);
	wtprintf (g_w[ILT_W], "  %s", buf);

	mjd_cal (mjd, &m, &d, &y);
	wtprintf (g_w[IUTD_W], "%2d-%s-%d", (int)d, monthName(m), y);

	wtprintf (g_w[IJD_W], "%11.3f", telstatshmp->now.n_mjd+MJD0);

	now_lst (np, &lst);
	fs_sexa (buf, lst, 2, 3600);
	wtprintf (g_w[ILST_W], "  %s", buf);

	twilight_cir (np, SUNDOWN, &dawn, &dusk,&rsstatus);
	dawn = mjd_hr (dawn);
	dusk = mjd_hr (dusk);
	fs_sexa (buf, dawn, 2, 60);
	wtprintf (g_w[IDAWN_W], "  %s UT", buf);
	fs_sexa (buf, dusk, 2, 60);
	wtprintf (g_w[IDUSK_W], "  %s UT", buf);
	tmp = mjd_hr(telstatshmp->now.n_mjd);

	if (tmp > dusk || tmp < dawn) {
	    setColor (g_w[IDAWN_W], XmNbackground, uneditableColor);
	    setColor (g_w[IDUSK_W], XmNbackground, uneditableColor);
	} else {
	    setColor (g_w[IDAWN_W], XmNbackground, ltcolors[LTACTIVE]);
	    setColor (g_w[IDUSK_W], XmNbackground, ltcolors[LTACTIVE]);
	}
}

static void
computeSunMoon()
{
	Now *np = &telstatshmp->now;

	(void) memset ((void *)&moonobj, 0, sizeof(moonobj));
	moonobj.o_type = PLANET;
	moonobj.pl.pl_code = MOON;
	(void) obj_cir (np, &moonobj);

	(void) memset ((void *)&sunobj, 0, sizeof(sunobj));
	sunobj.o_type = PLANET;
	sunobj.pl.pl_code = SUN;
	(void) obj_cir (np, &sunobj);
}

static void
showSunMoon ()
{
	Widget w;
	Obj *op;

	op = &moonobj;
	w = g_w[IMOON_W];
	wtprintf (w, "%2.0f%% %2s %+.0f", op->s_phase, cardDirName(op->s_az),
							    raddeg(op->s_alt));
	setColor (w, XmNbackground, op->s_alt > 0 ? ltcolors[LTACTIVE]
						  : uneditableColor);

	op = &sunobj;
	w = g_w[ISUN_W];
	wtprintf (w, "   %3s %+.0f", cardDirName(op->s_az), raddeg(op->s_alt));
	setColor (w, XmNbackground, op->s_alt > 0 ? ltcolors[LTACTIVE]
						  : uneditableColor);
}

static void
showFocus()
{
	double tmp;

	if (!OMOT->have)
	    return;

	/* show microns from home */
	tmp = OMOT->step*OMOT->cpos/OMOT->focscale/(2*PI);
	wtprintf (g_w[CFO_W], "%8.1f", tmp);

	/* show status */
	if (OMOT->cvel != 0)
	    setLt (g_w[CFOLT_W], LTACTIVE);
	else {
	    /* ok if within 1 step */
	    if (OMOT->haveenc) tmp = OMOT->estep*(OMOT->cpos - OMOT->dpos)/(2*PI);
		else tmp = OMOT->step*(OMOT->cpos - OMOT->dpos)/(2*PI);
	    setLt (g_w[CFOLT_W], fabs(tmp) < 1.5 ? LTOK : LTWARN);
	}
}

static void
showScope()
{
	switch (telstatshmp->telstate) {
	case TS_STOPPED:
	    setLt(g_w[STLT_W],LTIDLE); setLt(g_w[SSLT_W],LTIDLE);
	    setLt (g_w[CRLT_W], LTIDLE);
	    curPos();
	    noTarg();
	    break;

	case TS_SLEWING:
	    setLt(g_w[STLT_W],LTIDLE); setLt(g_w[SSLT_W],LTOK);
	    if (RMOT->have)
		setLt (g_w[CRLT_W], RMOT->cvel ? LTACTIVE : LTOK);
	    curPos();
	    curTarg();
	    break;

	case TS_HUNTING:
	    setLt(g_w[STLT_W],LTACTIVE); setLt(g_w[SSLT_W],LTOK);
	    if (RMOT->have)
		setLt (g_w[CRLT_W], RMOT->cvel ? LTACTIVE : LTOK);
	    curPos();
	    curTarg();
	    break;

	case TS_TRACKING:
	    setLt(g_w[STLT_W],LTOK); setLt(g_w[SSLT_W],LTIDLE);
	    if (RMOT->have) setLt (g_w[CRLT_W], LTOK);
	    curPos();
	    curTarg();
	    break;

	case TS_HOMING:
	    setLt(g_w[STLT_W],LTIDLE); setLt(g_w[SSLT_W],LTIDLE);
	    if (RMOT->have)
		setLt (g_w[CRLT_W], RMOT->cvel ? LTACTIVE : LTOK);
	    noPos();
	    noTarg();
	    break;

	case TS_LIMITING:
	    setLt(g_w[STLT_W],LTIDLE); setLt(g_w[SSLT_W],LTIDLE);
	    if (RMOT->have)
		setLt (g_w[CRLT_W], RMOT->cvel ? LTACTIVE : LTOK);
	    curPos();
	    noTarg();
	    break;
	}
}

static void
showHL()
{
	setLt(g_w[SHLT_W], ANY_HOMING   ? LTACTIVE : LTIDLE);
	setLt(g_w[SLLT_W], ANY_LIMITING ? LTACTIVE : LTIDLE);
}

static void
showWx()
{
	Now *np = &telstatshmp->now;
	WxStats *wp = &telstatshmp->wxs;

	if (time(NULL) - wp->updtime < 30) {
	    Widget w;

	    w = g_w[IWIND_W];
	    wtprintf (w, "    %d KPH", wp->wspeed);
	    setColor (w,XmNbackground,wp->alert & WXA_MAXW ? ltcolors[LTWARN]
	    						     : uneditableColor);

	    w = g_w[IWDIR_W];
	    if (wp->wdirstr[0])
		wtprintf (w, "     %s", wp->wdirstr);
	    else
		wtprintf (w, "     %d", wp->wdir);

	    w = g_w[IHUM_W];
	    wtprintf (w, "   %d %%RH", wp->humidity);
	    setColor (w,XmNbackground,wp->alert & WXA_MAXH ? ltcolors[LTWARN]
	    						     : uneditableColor);

	    w = g_w[IRAIN_W];
	    wtprintf (w, "   %.1f mm", wp->rain/10.);
	    setColor (w,XmNbackground,wp->alert & WXA_RAIN ? ltcolors[LTWARN]
	    						     : uneditableColor);

	    w = g_w[ITEMP_W];
	    wtprintf (w, "%8.1f C", np->n_temp);
	    setColor (w, XmNbackground, wp->alert & (WXA_MINT|WXA_MAXT)
					? ltcolors[LTWARN] : uneditableColor);

	    wtprintf (g_w[IPRES_W], "  %.0f mBar", np->n_pressure);

	    setLt (g_w[SWLT_W], wp->alert ? LTWARN : LTOK);

	} else {
	    setLt (g_w[SWLT_W], LTIDLE);
	    wtprintf (g_w[IWIND_W], blank);
	    wtprintf (g_w[IWDIR_W], blank);
	    wtprintf (g_w[IHUM_W], blank);
	    wtprintf (g_w[IRAIN_W], blank);

	    wtprintf (g_w[ITEMP_W], " (%.1f C)", np->n_temp);
	    wtprintf (g_w[IPRES_W], "(%.0f mBar)", np->n_pressure);
	}
}

static void
showDome()
{
	static int last_godome = -1, last_goshutter = -1;
	DomeState ds = telstatshmp->domestate;
	LtState domelt, sol, scl, col, ccl;
	int go;

	/* first check whether to allow operator access.
	 */
	go = ds != DS_ABSENT && xobs_alone;
	if (go != last_godome) {
	    if (go) {
		XtSetSensitive (g_w[DAUTO_W], True);
		XtSetSensitive (g_w[DAZ_W], True);
		XtVaSetValues (g_w[DAZ_W], XmNbackground, editableColor, NULL);
		XtSetSensitive (g_w[DAZL_W], True);
	    } else {
		XtSetSensitive (g_w[DAUTO_W], False);
		XtSetSensitive (g_w[DAZ_W], False);
		XtVaSetValues (g_w[DAZ_W], XmNbackground, uneditableColor,NULL);
		XtSetSensitive (g_w[DAZL_W], False);
	    }
	    last_godome = go;
	}
	go = telstatshmp->shutterstate != SH_ABSENT && xobs_alone;
	if (go != last_goshutter) {
	    if (go) {
		XtSetSensitive (g_w[DOPEN_W], True);
		XtSetSensitive (g_w[DCLOSE_W], True);
	    } else {
		XtSetSensitive (g_w[DOPEN_W], False);
		XtSetSensitive (g_w[DCLOSE_W], False);
	    }
	    last_goshutter = go;
	}

	switch (ds) {
	case DS_ABSENT:
	    domelt = LTIDLE;
	    break;

	case DS_ROTATING:	/* FALLTHRU */
	case DS_HOMING:
	    domelt = LTACTIVE;
	    break;

	case DS_STOPPED:
	    if (delra(telstatshmp->dometaz - telstatshmp->domeaz) <= DOMETOL)
		domelt = LTOK;
	    else if (telstatshmp->shutterstate == SH_OPEN)
		domelt = LTWARN; /* open but in wrong position */
	    else
		domelt = LTIDLE;
	    break;

	default:
	    domelt = LTIDLE;
	    break;
	}
	setLt (g_w[DAZLT_W], domelt);

	switch (telstatshmp->shutterstate) {
	case SH_ABSENT:  sol = LTIDLE;   scl = LTIDLE;   break;
	case SH_IDLE:    sol = LTIDLE;   scl = LTIDLE;   break;
	case SH_OPENING: sol = LTACTIVE; scl = LTIDLE;   break;
	case SH_CLOSING: sol = LTIDLE;   scl = LTACTIVE; break;
	case SH_OPEN:    sol = LTOK;     scl = LTIDLE;   break;
	case SH_CLOSED:  sol = LTIDLE;   scl = LTOK;     break;
	default:	 sol = LTIDLE;   scl = LTIDLE;   break;
	}
	setLt (g_w[DOLT_W], sol);
	setLt (g_w[DCLT_W], scl);


	switch (telstatshmp->coverstate) {
	case CV_ABSENT:  col = LTIDLE;   ccl = LTIDLE;   break;
	case CV_IDLE:    col = LTIDLE;   ccl = LTIDLE;   break;
	case CV_OPENING: col = LTACTIVE; ccl = LTIDLE;   break;
	case CV_CLOSING: col = LTIDLE;   ccl = LTACTIVE; break;
	case CV_OPEN:    col = LTOK;     ccl = LTIDLE;   break;
	case CV_CLOSED:  col = LTIDLE;   ccl = LTOK;     break;
	default:	 col = LTIDLE;   ccl = LTIDLE;   break;
	}
	setLt (g_w[COLT_W], col);
	setLt (g_w[CVLT_W], ccl);


	XmToggleButtonSetState (g_w[DAUTO_W], telstatshmp->autodome !=0, False);

	if (ds != DS_ABSENT) {
	    char buf[128];
	    double tmp;

	    if (ds != DS_HOMING) {
		fs_sexa (buf, raddeg(telstatshmp->domeaz), 4, 3600);
		wtprintf (g_w[PCDAZ_W], "%s", buf);
		if (!XtIsSensitive (g_w[DAZ_W]))
		    wtprintf (g_w[DAZ_W], "%.3s", buf);
	    } else
		wtprintf (g_w[PCDAZ_W], blank);

	    if (telstatshmp->autodome || ds == DS_ROTATING || ds == DS_HOMING) {
		fs_sexa (buf, raddeg(telstatshmp->dometaz), 4, 3600);
		wtprintf (g_w[PTDAZ_W], "%s", buf);
	    } else
		wtprintf (g_w[PTDAZ_W], blank);

	    if (telstatshmp->autodome || ds == DS_ROTATING) {
		tmp = delra (telstatshmp->domeaz - telstatshmp->dometaz);
		fs_sexa (buf, raddeg(tmp), 4, 3600);
		wtprintf (g_w[PDDAZ_W], "%s", buf);
	    } else
		wtprintf (g_w[PDDAZ_W], blank);
	}
}

static void
showTemps()
{
	/* TODO be more legit about knowing we can show only 3 */
	static int itw[MAUXTP] = {IT1_W, IT2_W, IT3_W};
	    
	WxStats *wp = &telstatshmp->wxs;
	int valid = time(NULL) - wp->updtime < 30;
	int i;

	for (i = 0; i < MAUXTP; i++)
	    if (valid && (wp->auxtmask & (1<<i)))
		wtprintf (g_w[itw[i]], "%8.1f C", wp->auxt[i]);
	    else
		wtprintf (g_w[itw[i]], blank);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: update.c,v $ $Date: 2007/02/25 23:31:22 $ $Revision: 1.2 $ $Name:  $"};
