/* add FITS header fields using info in telstatshm */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telstatshm.h"
#include "fits.h"
#include "running.h"
#include "misc.h"

#include "ccdcamera.h"
#include "scan.h"
#include "telenv.h"

#include "telfits.h"


/* add some fields from telstatshmp */
void
addShmFITS (FImage *fip, TelStatShm *telstatshmp)
{
	Now *np = &telstatshmp->now;
	Scan *sp = &telstatshmp->scan;
	char buf[128];
	char shutter[128];	
	double tmp;

//********************************************************************
//December 20, 2009 - Kevin Casteels
	switch(telstatshmp->shutterstate) {
		case SH_ABSENT:
			sprintf(shutter,"Absent");
			break;
		case SH_IDLE:
			sprintf(shutter, "Idle");
			break;	
		case SH_OPENING:
			sprintf(shutter, "Opening");
			break;
		case SH_CLOSING:
			sprintf(shutter, "Closing");
			break;
		case SH_OPEN:
			sprintf(shutter, "Open");
			break;
		case SH_CLOSED:
			sprintf(shutter, "Closed");
			break;
		default:
			sprintf(shutter, "Error");
			break;
	};
	setStringFITS (fip, "PROPCODE", sp->title, "Proposal code");//Proposal code: 10025
	setStringFITS (fip, "TARGETID", sp->obj.o_name, "Target group ID");//Target group id
	setStringFITS (fip, "BLOCKID", sp->observer, "Observation block ID");//Observersation block id
	setStringFITS (fip, "IMAGEID", sp->comment, "Image ID");//Image number within its block
	setStringFITS (fip, "DOMESTAT", shutter, "Dome status");
	if(sp->ccdcalib.newc)
		setStringFITS (fip, "OBSTYPE", ccdCalib2Str(sp->ccdcalib), "Observation type");
	else
		setStringFITS (fip, "OBSTYPE", "Manual", "Observation type");
	//Future variables to include in the headers:
	//setStringFITS (fip, "DEFOCUS", RoboDIMM->seeing, "Yes if defocused");
	//setStringFITS (fip, "PHOTOMET", CLOUD->value, "Cloud sensor");
	//setStringFITS (fip, "SEEING", RoboDIMM->seeing, "DIMM measured seeing");	
	//setStringFITS (fip, "RDNOISE", sp->comment, "Read out noise");
	//setStringFITS (fip, "GAIN", &sp->shutter, "CCD camera gain");

//**********************************************************************
	now_lst (np, &tmp);
	fs_sexa (buf, tmp, 2, 3600);
	setStringFITS (fip, "LST", buf, "Local sidereal time");

	fs_sexa (buf, raddeg(telstatshmp->CPA), 4, 3600);
	setStringFITS (fip, "POSANGLE", buf, "Position angle, degrees, +W");

	fs_sexa (buf, raddeg (lat), 3, 3600);
	setStringFITS (fip, "LATITUDE", buf, "Site Latitude, degrees +N");
	fs_sexa (buf, raddeg (lng), 4, 3600);
	setStringFITS (fip, "LONGITUD", buf, "Site Longitude, degrees +E");
	fs_sexa (buf, raddeg (telstatshmp->Calt), 3, 3600);
	setStringFITS (fip, "ELEVATIO", buf, "Degrees above horizon");
	fs_sexa (buf, raddeg (telstatshmp->Caz), 3, 3600);
	setStringFITS (fip, "AZIMUTH", buf, "Degrees E of N");
	fs_sexa (buf, radhr (telstatshmp->CAHA), 3, 360000);
	setStringFITS (fip, "HA", buf, "Local Hour Angle");
	fs_sexa (buf, radhr (telstatshmp->CARA), 3, 360000);
	setStringFITS (fip, "RAEOD", buf, "Nominal center Apparent RA");
	fs_sexa (buf, raddeg (telstatshmp->CADec), 3, 36000);
	setStringFITS (fip, "DECEOD",buf,"Nominal center Apparent Dec");
	fs_sexa (buf, radhr (telstatshmp->CJ2kRA), 3, 360000);
	setStringFITS (fip, "RA", buf, "Nominal center J2000 RA");
	fs_sexa (buf, raddeg (telstatshmp->CJ2kDec), 3, 36000);
	setStringFITS (fip, "DEC",buf,"Nominal center J2000 Dec");
	fs_sexa (buf, radhr (telstatshmp->DJ2kRA), 3, 36000);
	setStringFITS (fip, "OBJRA", buf, "Target center J2000 RA");
	fs_sexa (buf, raddeg (telstatshmp->DJ2kDec), 3, 36000);
	setStringFITS (fip, "OBJDEC",buf,"Target center J2000 Dec");
	setRealFITS (fip, "EPOCH", 2000., 10, "RA/Dec epoch, years (obsolete)");
	setRealFITS (fip, "EQUINOX", 2000.0, 10, "RA/Dec equinox, years");
	if (telstatshmp->minfo[TEL_IM].have)
	    buf[0] = telstatshmp->filter;
	else
	    buf[0] = 'C';
	buf[1] = '\0';
	setStringFITS (fip, "FILTER", buf, "Filter code");

	setIntFITS (fip, "CAMTEMP", telstatshmp->camtemp, "Camera temp, C");

	setRealFITS (fip, "RAWHENC", telstatshmp->minfo[TEL_HM].cpos, 8,
					    "HA Encoder, rads from home");
	setRealFITS (fip, "RAWDENC", telstatshmp->minfo[TEL_DM].cpos, 8,
					"Dec Encoder, rads from home");
	if (telstatshmp->minfo[TEL_RM].have)
	    setRealFITS (fip, "RAWRSTP", telstatshmp->minfo[TEL_RM].cpos, 8,
					"Field Motor, rads from home");
	if (telstatshmp->minfo[TEL_OM].have) {
	    MotorInfo *mip = &telstatshmp->minfo[TEL_OM];
	    setRealFITS (fip, "RAWOSTP", mip->cpos, 8,
					    "Focus Motor, rads from home");
	    tmp = mip->step/((2*PI)*mip->focscale)*mip->cpos;
	    setRealFITS (fip, "FOCUSPOS", tmp, 8, "Focus pos from home, um");
	}
	if (telstatshmp->minfo[TEL_IM].have)
	    setRealFITS (fip, "RAWISTP", telstatshmp->minfo[TEL_IM].cpos, 8,
					"Filter Motor, rads from home");

	/* weather data if not too old */
	if (time(NULL) - telstatshmp->wxs.updtime < 30) {
	    sprintf (buf, "Weather at UT %s", asctime(gmtime(&telstatshmp->wxs.updtime)));
	    buf[strlen(buf)-1] = '\0';	/* chop off \n */
	    setCommentFITS (fip, "COMMENT", buf);
	    setRealFITS (fip, "WXTEMP", temp, 3, "Ambient air temp, C");
	    setRealFITS (fip, "WXPRES", pressure, 6, "Atm pressure, mB");
	    setIntFITS (fip, "WXWNDSPD", telstatshmp->wxs.wspeed,
							    "Wind speed, kph");
	    setIntFITS (fip, "WXWNDDIR", telstatshmp->wxs.wdir,
						    "Wind dir, degs E of N");
	    setIntFITS (fip, "WXHUMID", telstatshmp->wxs.humidity,
						"Outdoor humidity, percent");
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: telfits.c,v $ $Date: 2002/09/20 03:32:59 $ $Revision: 1.3 $ $Name:  $"};
