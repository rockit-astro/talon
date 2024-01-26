#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <Xm/Xm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telstatshm.h"
#include "configfile.h"

#include "xobs.h"

/* variables set from the config files -- see initCfg() */
double SUNDOWN;
double STOWALT;
double STOWAZ;
double SERVICEALT;
double SERVICEAZ;
int OffTargPitch;
int OffTargDuration;
int OffTargPercent;
int OnTargPitch;
int OnTargDuration;
int OnTargPercent;
int BeepPeriod;
char BANNER[80];

static char tscfn[] = "archive/config/telsched.cfg";
static char tcfn[] = "archive/config/telescoped.cfg";

void
initCfg()
{
#define NTSCFG   (sizeof(tscfg)/sizeof(tscfg[0]))

	static CfgEntry tscfg[] = {
	    {"SUNDOWN",		CFG_DBL, &SUNDOWN},
	    {"STOWALT",		CFG_DBL, &STOWALT},
	    {"STOWAZ",		CFG_DBL, &STOWAZ},
	    {"SERVICEALT",	CFG_DBL, &SERVICEALT},
	    {"SERVICEAZ",	CFG_DBL, &SERVICEAZ},
	    {"OffTargPitch",	CFG_INT, &OffTargPitch},
	    {"OffTargDuration",	CFG_INT, &OffTargDuration},
	    {"OffTargPercent",	CFG_INT, &OffTargPercent},
	    {"OnTargPitch",	CFG_INT, &OnTargPitch},
	    {"OnTargDuration",	CFG_INT, &OnTargDuration},
	    {"OnTargPercent",	CFG_INT, &OnTargPercent},
	    {"BeepPeriod",	CFG_INT, &BeepPeriod},
	    {"BANNER",		CFG_STR, BANNER, sizeof(BANNER)},
	};
	char buf[1024];
	int n;

	/* read stuff from telsched.cfg */
	n = readCfgFile (1, tscfn, tscfg, NTSCFG);
	if (n != NTSCFG) {
	    cfgFileError (tscfn, n, NULL, tscfg, NTSCFG);
	    die();
	}
}
