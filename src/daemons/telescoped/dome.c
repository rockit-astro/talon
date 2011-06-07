/*
 * dome.c
 *
 *  Created on: Feb 15, 2011
 *      Author: luis
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "strops.h"
#include "telstatshm.h"
#include "running.h"
#include "rot.h"
#include "misc.h"
#include "telenv.h"
#include "csimc.h"
#include "virmc.h"
#include "cliserv.h"
#include "tts.h"

#include "teled.h"  // will bring in buildcfg.h
enum DOME_TYPE
{
	DOME_TYPE_NONE = 0,
	DOME_TYPE_VIRTUAL,
	DOME_TYPE_CSI,
	DOME_TYPE_COMMHUB,
	DOME_TYPE_LAST
};

int dome_type = DOME_TYPE_NONE;

void dome_initCfg(void)
{
#define NDCFG   (sizeof(dcfg)/sizeof(dcfg[0]))

	static char DOMETYPE[64];

	static CfgEntry dcfg[] =
	{
	{ "DOMETYPE", CFG_STR, &DOMETYPE, sizeof(DOMETYPE) } };

	int n;
	/* read the file */
	n = readCfgFile(1, dcfn, dcfg, NDCFG);
	if (n != NDCFG)
	{
		cfgFileError(dcfn, n, (CfgPrFp) tdlog, dcfg, NDCFG);
		die();
	}

	if (!strncasecmp(DOMETYPE, "virtual", 7))
		dome_type = DOME_TYPE_VIRTUAL;
	else if (!strncasecmp(DOMETYPE, "csi", 3))
		dome_type = DOME_TYPE_CSI;
	else if (!strncasecmp(DOMETYPE, "commhub", 7))
		dome_type = DOME_TYPE_COMMHUB;
	else
		dome_type = DOME_TYPE_NONE;

	return;
}

void dome_msg(msg)
	char *msg;
{
	if (msg && strncasecmp(msg, "reset", 5) == 0)
	{
		dome_initCfg();
	}

	if (dome_type == DOME_TYPE_VIRTUAL)
	{
		csi_dome_msg(msg);
		return;
	}
	else if (dome_type == DOME_TYPE_CSI)
	{
		csi_dome_msg(msg);
		return;
	}
	else if (dome_type == DOME_TYPE_COMMHUB)
	{
		commhub_dome_msg(msg);
		return;
	}
	else
	{
		return;
	}

}
