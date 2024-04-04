/* handle the direct scope controls */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>

#include <Xm/Xm.h>
#include <Xm/ToggleB.h>
#include <Xm/TextF.h>
#include <X11/keysym.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "strops.h"
#include "configfile.h"
#include "misc.h"
#include "telenv.h"
#include "telstatshm.h"
#include "xtools.h"

#include "xobs.h"
#include "widgets.h"


static int s_updating;	/* flag set when updating TFs from within */

static int findAll(void);
static void prAll (double ra, double dec, double e, double ha, double alt,
    double az);
static int getEpoch (char *epochstr, double *ep);
static void findAA (double ra, double dec, double e, double *hap, double *altp,
    double *azp);

void
s_here (Widget w, XtPointer client, XtPointer call)
{
	char buf[32];

    	s_updating = 1;

	fs_sexa (buf, radhr(telstatshmp->CJ2kRA), 3, 3600);
	wtprintf (g_w[TRA_W], buf);

	fs_sexa (buf, raddeg(telstatshmp->CJ2kDec), 3, 3600);
	wtprintf (g_w[TDEC_W], buf);

	wtprintf (g_w[TEPOCH_W], "%9.1f", 2000.0);

	fs_sexa (buf, radhr(telstatshmp->CAHA), 3, 3600);
	wtprintf (g_w[THA_W], buf);

	fs_sexa (buf, raddeg(telstatshmp->Calt), 3, 3600);
	wtprintf (g_w[TALT_W], buf);

	fs_sexa (buf, raddeg(telstatshmp->Caz), 3, 3600);
	wtprintf (g_w[TAZ_W], buf);

    	s_updating = 0;
}

void
s_stow (Widget w, XtPointer client, XtPointer call)
{
//	char altbuf[32], azbuf[32];

	if (!rusure (toplevel_w, "slew to the stow position"))
	    return;

/* Replaced -- new stow command in tel.c	
	fs_sexa (altbuf, raddeg(STOWALT), 3, 3600);
	fs_sexa (azbuf, raddeg(STOWAZ), 3, 3600);
	msg ("Slewing to Alt %s Az %s", altbuf, azbuf);

	XmTextFieldSetString (g_w[TALT_W], altbuf);
	XmTextFieldSetString (g_w[TAZ_W], azbuf);
	(void) findAll();

	fifoMsg (Tel_Id, "Alt:%.5f Az:%.6f", STOWALT, STOWAZ);
*/
	fifoMsg(Tel_Id, "Stow");	
}

void
s_service (Widget w, XtPointer client, XtPointer call)
{
	char altbuf[32], azbuf[32];

	if (!rusure (toplevel_w, "slew to the service position"))
	    return;

	fs_sexa (altbuf, raddeg(SERVICEALT), 3, 3600);
	fs_sexa (azbuf, raddeg(SERVICEAZ), 3, 3600);
	msg ("Slewing to Alt %s Az %s", altbuf, azbuf);

	XmTextFieldSetString (g_w[TALT_W], altbuf);
	XmTextFieldSetString (g_w[TAZ_W], azbuf);
	(void) findAll();

	fifoMsg (Tel_Id, "Alt:%.6f Az:%.6f", SERVICEALT, SERVICEAZ);
}

/* valueChanged from one of the pointing TFs.
 * client is one of GUIWidgets.
 * clear out the incompatable fields unless s_updating is set.
 */
void
s_edit (Widget w, XtPointer client, XtPointer call)
{
	static char blanks[] = "";
	GUIWidgets gw = (GUIWidgets)client;
	char *str;
	int i;

	/* ignore if updating from within */
	if (s_updating)
	    return;

	/* which we now do */
	s_updating = 1;

	switch (gw) {
	case TRA_W:
	    wtprintf (g_w[TALT_W], blanks);
	    wtprintf (g_w[TAZ_W], blanks);
	    wtprintf (g_w[THA_W], blanks);
	    break;
	case TDEC_W:
	    wtprintf (g_w[TALT_W], blanks);
	    wtprintf (g_w[TAZ_W], blanks);
	    break;
	case THA_W:
	    wtprintf (g_w[TALT_W], blanks);
	    wtprintf (g_w[TAZ_W], blanks);
	    wtprintf (g_w[TRA_W], blanks);
	    wtprintf (g_w[TEPOCH_W], blanks);
	    break;
	case TEPOCH_W:
	    wtprintf (g_w[TALT_W], blanks);
	    wtprintf (g_w[TAZ_W], blanks);
	    wtprintf (g_w[THA_W], blanks);
	    break;
	case TALT_W:	/* FALLTHRU */
	case TAZ_W:
	    wtprintf (g_w[TRA_W], blanks);
	    wtprintf (g_w[TDEC_W], blanks);
	    wtprintf (g_w[THA_W], blanks);
	    break;
	default:
	    fprintf (stderr, "Bogus widget: %d\n", gw);
	    break;
	}

	/* refresh ourselves -- cursor stomps on us otherwise */
	i = XmTextFieldGetInsertionPosition (g_w[gw]);
	str = XmTextFieldGetString (g_w[gw]);
	wtprintf (g_w[gw], blanks);
	wtprintf (g_w[gw], str);
	XmTextFieldSetInsertionPosition (g_w[gw], i);
	XtFree (str);

	/* finished updating */
	s_updating = 0;
}

/* requires an object or ra/dec/epoch */
void
s_track (Widget w, XtPointer client, XtPointer call)
{
	if (findAll() < 0)
	    return;

	if (!rusure (toplevel_w, "slew to and track the new location"))
	    return;

    String rastr, decstr, epochstr;
    double ra, dec;

    /* gather ra/dec/epoch */
    rastr = XmTextFieldGetString (g_w[TRA_W]);
    decstr = XmTextFieldGetString (g_w[TDEC_W]);
    epochstr = XmTextFieldGetString (g_w[TEPOCH_W]);

    /* convert -- findAll() assures us they are all good */
    (void) scansex (rastr, &ra);
    ra = hrrad (ra);
    (void) scansex (decstr, &dec);
    dec = degrad(dec);

    if (strcwcmp (epochstr, "eod") == 0) {
		fifoMsg (Tel_Id, "RA:%.6f Dec:%.6f", ra, dec);
		msg ("Hunting for %s %s", rastr, decstr);
    } else {
		double ep = atof (epochstr);
		fifoMsg (Tel_Id, "RA:%.6f Dec:%.6f Epoch:%g", ra, dec, ep);
		msg ("Hunting for %s %s %g", rastr, decstr, ep);
    }

    XtFree (rastr);
    XtFree (decstr);
    XtFree (epochstr);
}


/* requires alt/az */
void
s_goto (Widget w, XtPointer client, XtPointer call)
{
	String altstr, azstr;
	double alt, az;

	if (findAll() < 0)
	    return;

	if (!rusure (toplevel_w, "slew to the new location"))
	    return;

	altstr = XmTextFieldGetString (g_w[TALT_W]);
	azstr = XmTextFieldGetString (g_w[TAZ_W]);
	(void) scansex (altstr, &alt);
	alt = degrad(alt);
	(void) scansex (azstr, &az);
	az = degrad(az);

	msg ("Slewing to %s %s", altstr, azstr);
	fifoMsg (Tel_Id, "Alt:%.6f Az:%.6f", alt, az);

	XtFree (altstr);
	XtFree (azstr);
}

/* look among the various text fields and make sure they are all
 * in agreement, and fill in any blank ones.
 * return 0 if ok, else -1.
 */
static int
findAll()
{
	Now *np = &telstatshmp->now;
	String rastr, decstr, hastr, epochstr;
	String altstr, azstr;
	char buf[1024];
	int ral, decl, hal, epochl;
	int altl, azl;
	int objl;
	int ret = -1;

	/* disable valueChanged callback */
	s_updating = 1;

	/* gather all the fields */
	altstr = XmTextFieldGetString (g_w[TALT_W]);
	altl = strlen (altstr);
	azstr = XmTextFieldGetString (g_w[TAZ_W]);
	azl = strlen (azstr);
	rastr = XmTextFieldGetString (g_w[TRA_W]);
	ral = strlen (rastr);
	decstr = XmTextFieldGetString (g_w[TDEC_W]);
	decl = strlen (decstr);
	hastr = XmTextFieldGetString (g_w[THA_W]);
	hal = strlen (hastr);
	epochstr = XmTextFieldGetString (g_w[TEPOCH_W]);
	epochl = strlen (epochstr);

	/* see what we can do */
	if (altl && azl) {
	    /* given alt/az, find other stuff */
	    double ep, alt, az, ha, lst, ra, dec;

	    if (scansex (altstr, &alt) < 0) {
		msg ("Bad Alt format");
		goto out;
	    }
	    if (alt < -90 || alt > 90) {
		msg ("Alt must be -90 .. 90");
		goto out;
	    }
	    alt = degrad(alt);

	    if (scansex (azstr, &az) < 0) {
		msg ("Bad Az format");
		goto out;
	    }
	    if (az < 0 || az >= 360) {
		msg ("Az must be 0 .. 360");
		goto out;
	    }
	    az = degrad(az);

	    if (getEpoch (epochstr, &ep) < 0)
		goto out;

	    unrefract (temp, pressure, alt, &alt);
	    aa_hadec (lat, alt, az, &ha, &dec);
	    now_lst (np, &lst);
	    ra = hrrad(lst) - ha;
	    range (&ra, 2*PI);

	    if (ep != EOD)
		ap_as (np, ep, &ra, &dec);
		
	    prAll (ra, dec, ep, ha, alt, az);
	    ret = 0;

	} else if (ral && decl) {

	    /* given ra/dec find the rest */
	    double alt, az, ra, dec, ha;
	    double ep;

	    if (scansex (rastr, &ra) < 0) {
		msg ("Bad RA format");
		goto out;
	    }
	    if (ra < 0 || ra >= 24) {
		msg ("RA must be 0 .. 24");
		goto out;
	    }
	    ra = hrrad(ra);

	    if (scansex (decstr, &dec) < 0) {
		msg ("Bad Dec format");
		goto out;
	    }
	    if (dec < -90 || dec > 90) {
		msg ("Dec must be -90 .. 90");
		goto out;
	    }
	    dec = degrad(dec);

	    if (getEpoch (epochstr, &ep) < 0)
		goto out;

	    findAA (ra, dec, ep, &ha, &alt, &az);

	    prAll (ra, dec, ep, ha, alt, az);
	    ret = 0;

	} else if (hal && decl) {

	    /* given HA and dec, find the rest */
	    double alt, az, ha, dec, lst, ra;

	    if (scansex (hastr, &ha) < 0) {
		msg ("Bad HA format");
		goto out;
	    }
	    if (ha < -12 || ha > 12) {
		msg ("HA must be -12 .. 12");
		goto out;
	    }
	    ha = hrrad (ha);

	    if (scansex (decstr, &dec) < 0) {
		msg ("Bad Dec format");
		goto out;
	    }
	    if (dec < -90 || dec > 90) {
		msg ("Dec must be -90 .. 90");
		goto out;
	    }
	    dec = degrad(dec);

	    now_lst (np, &lst);
	    ra = hrrad(lst) - ha;
	    range (&ra, 2*PI);
	    findAA (ra, dec, EOD, &ha, &alt, &az);

	    prAll (ra, dec, EOD, ha, alt, az);
	    ret = 0;

	} else {

	    msg ("Conflicting combination");
	}

    out:
	XtFree (altstr);
	XtFree (azstr);
	XtFree (rastr);
	XtFree (decstr);
	XtFree (epochstr);
	XtFree (hastr);

	/* enable valueChanged callback */
	s_updating = 0;

	return (ret);
}

static void
prAll (double ra, double dec, double e, double ha, double alt, double az)
{
	char buf[32];

	fs_sexa (buf, radhr(ra), 3, 3600);
	wtprintf (g_w[TRA_W], buf);
	fs_sexa (buf, raddeg(dec), 3, 3600);
	wtprintf (g_w[TDEC_W], buf);
	fs_sexa (buf, raddeg(alt), 3, 3600);
	wtprintf (g_w[TALT_W], buf);
	fs_sexa (buf, raddeg(az), 3, 3600);
	wtprintf (g_w[TAZ_W], buf);
	fs_sexa (buf, radhr(ha), 3, 3600);
	wtprintf (g_w[THA_W], buf);
	if (e == EOD)
	    wtprintf (g_w[TEPOCH_W], "%9s", "EOD");
	else {
	    double y;
	    mjd_year (e, &y);
	    wtprintf (g_w[TEPOCH_W], "%9.1f", y);
	}
}

/* crack user's epoch, beware of gunk.
 * return as mjd.
 * return 0 if ok, else -1
 */
static int
getEpoch (char *epochstr, double *ep)
{
	double e;

	if (sscanf (epochstr, "%lf", &e) == 1) {
	    if (e <= 100 || e > 3000) {
		msg ("Suspicious epoch: %g", e);
		return (-1);
	    }
	    wtprintf (g_w[TEPOCH_W], "%9.1f", e);
	    year_mjd (e, ep);
	} else if (!strcwcmp (epochstr, "EOD")) {
	    *ep = EOD;
	} else if (strlen(epochstr) == 0) {
	    *ep = J2000;
	    wtprintf (g_w[TEPOCH_W], "%9.1f", 2000.);
	} else {
	    msg ("Bogus Epoch");
	    return (-1);
	}

	return (0);
}

static void
findAA (double ra, double dec, double e, double *hap, double *altp, double *azp)
{
	Now *np = &telstatshmp->now;
	Obj o;

	memset ((void *)&o, 0, sizeof(o));
	o.o_type = FIXED;

	if (e == EOD) {
	    radec2ha (np, ra, dec, hap);
	    ap_as (np, J2000, &ra, &dec);
	    o.f_RA = ra;
	    o.f_dec = dec;
	    o.f_epoch = J2000;
	    obj_cir (np, &o);
	} else {
	    Now n = *np;

	    n.n_epoch = e;
	    radec2ha (np, ra, dec, hap);
	    o.f_RA = ra;
	    o.f_dec = dec;
	    o.f_epoch = e;
	    obj_cir (np, &o);
	}

	*altp = o.s_alt;
	*azp = o.s_az;
}
