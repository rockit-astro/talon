/* program to execute a "regular" scan.
 * this is one with CCDCALIB either NONE or CATALOG.
 * waiting for download and postproc is broken off into a separate program.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telenv.h"
#include "telstatshm.h"
#include "scan.h"

#include "telrun.h"

static int pr_startPP (int first);
static int postProcess(void);
static void logStart(time_t n);

static char pplog[] = "archive/logs/postprocess.log";	/* pp log file */

static time_t tmpTime;		/* used to save times from one step to next */
static Scan bkg_scan;		/* used for background program */

typedef void *StepFuncP;
typedef StepFuncP (*StepFunc)(time_t now);

static StepFuncP ps_wait4Start(time_t n);
static StepFuncP ps_wait4Setup(time_t n);
static StepFuncP ps_wait4Exp (time_t n);

/* program to process a regular scan. called periodically by main_loop().
 * first is only set when just starting this scan.
 * return 0 if making progress, -1 on error or finished.
 * once expose completes, a separate program waits for download and starts
 *   postprocressing.
 * N.B. we are responsible for all our own logging and cleanup on errors.
 * N.B. due to circumstances beyond our control we may never get called again.
 */
int
pr_regscan (int first)
{
	static StepFuncP step;

	/* start over if first call */
	if (first) {
	    step = ps_wait4Start;	/* init sequencer */
	}

	/* run this step, next is its return */
	step = step ? (*(void *(*)())step)(time(NULL)) : NULL;

	if (!step) {
	    cscan->running = 0;
	    cscan->starttm = 0;
	    return (-1);
	}

	/* still working */
	return (0);
}

/* get things moving towards taking the scan defined in cscan */
void
pr_regSetup()
{
	Scan *sp = cscan;
	char buf[1024];
	int n;
    long unow;
    int afFlag;
    static int autofocus_flag = 1;

    // Convert to extval1, extval2 equivalents from new string argument format (10/29/09) for backward compatibility
    strcpy(buf, sp->extcmd);
    double ext1=0, ext2=0;
    char* pc = strtok(buf, ", ");
    if(pc)
    {
        ext1 = strtod(pc, NULL);
        pc += strlen(pc);
        while(*pc && (*pc == ',' || *pc <=32 )) pc++;
        ext2 = strtod(pc, NULL);
    }

	/* start telescope */

    if(sp->ccdcalib.newc == CT_FIXEDALTAZ) {
//      if(sp->extval1 == NO_ALTAZ_ENTRY) {
    	if(ext1 == NO_ALTAZ_ENTRY) {
    		// compute the location at time of scan
			Now hereNow = telstatshmp->now;
			/* mjd was 25567.5 on 00:00:00 1/1/1970 UTC (UNIX' epoch) */
			hereNow.n_mjd = 25567.5 + (sp->starttm / SPD);
		    obj_cir (&hereNow, &sp->obj);
		    tlog(sp,"Object info: name: %s ra: %g dec:%g alt:%g az: %g",
		    	sp->obj.any.co_name,sp->obj.any.co_ra,sp->obj.any.co_dec,
		    	sp->obj.any.co_alt,sp->obj.any.co_az);
//            sp->extval1 = sp->obj.any.co_alt;
//            sp->extval2 = sp->obj.any.co_az;
		    ext1 = sp->obj.any.co_alt;
		    ext2 = sp->obj.any.co_az;
    	}
//        tlog(sp, "Moving to Alt:%.6f Az:%.6f",raddeg(sp->extval1),raddeg(sp->extval2));
//        n = sprintf(buf,"Alt:%g Az:%g",sp->extval1,sp->extval2);
        tlog(sp, "Moving to Alt:%.6f Az:%.6f",raddeg(ext1),raddeg(ext2));
        n = sprintf(buf,"Alt:%g Az:%g",ext1,ext2);
    } else {
		tlog(sp, "Offsetting scope by dRA:%.6f dDec:%.6f", sp->rao, sp->deco);
		n = sprintf (buf, "dRA:%.6f dDec:%.6f #", sp->rao, sp->deco);
		db_write_line (&sp->obj, buf+n);
    }

	fifoWrite (Tel_Id, buf);

	/* start filter */
	if (IMOT->have) {
	    buf[0] = sp->filter;
	    buf[1] = '\0';
	    fifoWrite (Filter_Id, buf);
	}

	// our latched autofocus flag
	afFlag = 1;

	/* insure focus is participating */
    if(sp->ccdcalib.newc == CT_FOCUSPOS) {
//      tlog(sp, "Setting focus to %.1f",sp->extval1);
        tlog(sp, "Setting focus to %.1f",ext1);
        unow = OMOT->cpos*OMOT->step/(2*PI*OMOT->focscale);
//      fifoWrite(Focus_Id,"%.1f",sp->extval1 - unow);
        fifoWrite(Focus_Id,"%.1f",ext1 - unow);
        afFlag = 0;
		telstatshmp->autofocus = 0;
    } else if(sp->ccdcalib.newc == CT_FOCUSOFF) {
//        tlog(sp, "Offsetting focus by %.1f",sp->extval1);
//        fifoWrite(Focus_Id,"%.1f",sp->extval1);
        tlog(sp, "Offsetting focus by %.1f",ext1);
        fifoWrite(Focus_Id,"%.1f",ext1);
        afFlag = 0;
		telstatshmp->autofocus = 0;
    }
    else if(sp->ccdcalib.newc == CT_AUTOFOCUS) {
//        if(sp->extval1 == 2) {
          if(ext1 == 2) {
            // perform autofocus
            tlog(sp, "Autofocus performed");
            autofocus_flag = 1;
			telstatshmp->autofocus = 1;
            // TODO
        }
        else {
            // enable/disable autofocus mode
//            autofocus_flag = sp->extval1;
            autofocus_flag = ext1;
            tlog(sp, "Autofocus set to %d",autofocus_flag);
        }
    }
    if (OMOT->have && !telstatshmp->autofocus && (afFlag && autofocus_flag))
	    fifoWrite (Focus_Id, "Auto");

	/* insure dome is open and going */
	if (telstatshmp->shutterstate != SH_ABSENT &&
				telstatshmp->shutterstate != SH_OPEN &&
				telstatshmp->shutterstate != SH_OPENING)
	    fifoWrite (Dome_Id, "Open");
	if (telstatshmp->domestate != DS_ABSENT && !telstatshmp->autodome)
	    fifoWrite (Dome_Id, "Auto");
}

/* decide whether to start cscan */
static StepFuncP
ps_wait4Start(time_t n)
{
	Scan *sp = cscan;
	char buf[64];

	/* check for nominally near starttm */
	if (n > sp->starttm + sp->startdt) {
	    n -= sp->starttm + sp->startdt;
	    tlog (sp, "Too late by %d secs", (int) n);
	    markScan (scanfile, sp, 'F');
	    return (NULL);
	}
	if (n < sp->starttm - SETUP_TO)
	    return (ps_wait4Start);		/* waiting */

	if (n >= sp->starttm) // if no time to setup
	{
		n -= sp->starttm;
	    tlog (sp, "Starting regscan too late by %d secs", (int) n);
	    markScan (scanfile, sp, 'F');
		return(NULL);	  // abort
	}

	/* go! */
	strcpy (buf, timestamp(sp->starttm));	/* save from tlog */
	tlog (sp, "Checking scan setup.. scheduled at %s", buf);
	pr_regSetup();

	/* set a trigger for the start time */
	setTrigger (sp->starttm);

	/* now wait for setup and time to start */
	tmpTime = n + SETUP_TO;
	return (ps_wait4Setup);
}

/* wait for all required systems to come ready then start camera */
static StepFuncP
ps_wait4Setup(time_t n)
{
	int camok = telstatshmp->camstate == CAM_IDLE;
	int telok = telstatshmp->telstate == TS_TRACKING;
	int filok = FILTER_READY;
	int focok = FOCUS_READY;
	int domok = DOME_READY;
    // We can't rely on DOME_READY, 'cause the dome may be rotating to stay aligned with the tracking, so just check door
    // Update for Turkey dome: this was causing exposures to begin well before dome was in place.
    // Going back to DOME_READY above.
    //int domok = (telstatshmp->shutterstate == SH_ABSENT || telstatshmp->shutterstate == SH_OPEN);
	int allok;
	Scan *sp = cscan;
	char fullpath[1024];

    if(sp->ccdcalib.newc == CT_FIXEDALTAZ) {
        telok = telstatshmp->telstate == TS_STOPPED;
    }

    allok = camok && telok && filok && focok && domok;

	/* trouble if not finished within SETUP */
	if (!allok && n > tmpTime) {
	    tlog (sp, "Setup timed out for%s%s%s%s%s",
						    focok ? "" : " focus",
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
	    tlog (sp, "Setup too late by %d secs", (int) n);
	    all_stop(1);
	    return (NULL);
	}

	/* start no sooner than original start time */
	if (!allok || n < sp->starttm)
	    return (ps_wait4Setup);

	/* Ok! */

	/* publish */
	logStart(n);
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

/* wait for exposure to finish -- ok if idle or digitizing */
static StepFuncP
ps_wait4Exp (time_t n)
{
	int telok = telstatshmp->telstate == TS_TRACKING;
	int filok = FILTER_READY;
	int focok = FOCUS_READY;
	int domok = 1; // ignore dome at this point... DOME_READY;
	Scan *sp = cscan;

        if(sp->ccdcalib.newc == CT_FIXEDALTAZ) {
            telok = telstatshmp->telstate == TS_STOPPED;
        }

	/* double-check support systems */
	if (!telok || !filok || !domok || !focok) {
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
	if (n <= tmpTime || telstatshmp->camstate == CAM_EXPO)
	    return (ps_wait4Exp);	/* waiting... */

	/* exposure is finished: copy scan info to bkg_scan and start a
	 * new program to wait for download and start postprocessing.
	 * marking 'D' is really just to prevent this scan from being repeated.
	 * can still be marked 'F' if starting postprocess fails.
	 */
    fifoWrite (Tel_Id, "Stop");	/* stop tracking */
	tlog (sp, "Exposure complete.. starting download of %s", sp->imagefn);
	markScan (scanfile, sp, 'D');
	bkg_scan = *sp;
	tmpTime = n + CAMDIG_MAX;	/* estimate time at download complete */
	if (addProgram (pr_startPP, 1) < 0) {
	    tlog (&bkg_scan, "Error starting postprocess");
	    markScan (scanfile, sp, 'F');
	}

	/* all up to post processing now */
	return (NULL);
}

/* program to wait for download to complete and start postprocessing.
 * first is only set when just starting this action.
 * return 0 if making progress, -1 on error or finished.
 * N.B. we are responsible for all our own logging and cleanup on errors.
 * N.B. due to circumstances beyond our control we may never get called again.
 * N.B. scan being worked on here is bkg_scan, not cscan!
 */
static int
pr_startPP (int first)
{
	Scan *sp = &bkg_scan;

	switch (telstatshmp->camstate) {
	case CAM_EXPO:	/* on to next already -- well, ours is done then */
	case CAM_IDLE:
	    if (postProcess() == 0) {
		markScan (scanfile, sp, 'D');
		return (-1);	/* remove from program q */
	    }
	    break;

	case CAM_READ:
	    if (time(NULL) < tmpTime)
		return (0);	/* stay in program q */
	    tlog (sp, "Camera READING too long");
	    break;
	}

	/* trouble if get here -- already logged why */
	markScan (scanfile, sp, 'F');
	return (-1);	/* remove from program q */
}


/* helper funcs */

/* start the post-processing work for bkg_scan.
 * return 0 if gets off to a good start, else -1.
 */
static int
postProcess ()
{
	Scan *sp = &bkg_scan;
	char cmd[3072];
	char quotedExt[1032];
	char pp[1024];
	int s;

	if(sp->extcmd[0])
	{
	    strcpy(quotedExt, " \"");
	    strcat(quotedExt, sp->extcmd);
	    strcat(quotedExt, "\"");
	}
	else
	{
	    quotedExt[0] = 0;
	}

	telfixpath (pp, pplog);
	tlog (sp, "Starting postprocess %s cor=%d scale=%d, extcmd =%s", sp->imagefn,
			    sp->ccdcalib.data == CD_COOKED, sp->compress, quotedExt);
	sprintf (cmd, "nice postprocess %s/%s %d %d%s >> %s 2>&1 &",
						sp->imagedn, sp->imagefn,
			    sp->ccdcalib.data == CD_COOKED, sp->compress,
			    quotedExt,  // pass this as a new post process parameter
			    pp);
	if ((s = system(cmd)) != 0) {
	    tlog (sp, "Postprocess script failed: %d", s);
	    return (-1);
	}
	return (0);
}

/* log all the starting telescope particulars */
static void
logStart(time_t n)
{
	MotorInfo *mip;
	Scan *sp = cscan;
	char rabuf[32], decbuf[32];
	char altbuf[32], azbuf[32];
	char buf[256], *bp;
	int late;
	int bl;

	/* lead-in message */
	late = n - sp->starttm;
	if (late)
	    tlog (sp, "Starting exposure.. late by %d sec%s", late,
							late == 1 ? "" : "s");
	else
	    tlog (sp, "Starting exposure on time");

	/* log object definition, sans name since that is from sp already  */
	db_write_line (&sp->obj, buf);
	bp = strchr (buf, ',');
	if (bp)
	    tlog (sp, "%s", bp+1);

	/* log telescope and camera info for sure */
	fs_sexa (rabuf, radhr(telstatshmp->CJ2kRA), 3, 36000);
	fs_sexa (decbuf, raddeg(telstatshmp->CJ2kDec), 3, 36000);
	fs_sexa (altbuf, raddeg(telstatshmp->Calt), 3, 36000);
	fs_sexa (azbuf, raddeg(telstatshmp->Caz), 3, 36000);

	tlog (sp, "  Telescope RA/Dec: %s %s J2000", rabuf, decbuf);
	if (sp->rao || sp->deco) {
	    fs_sexa (rabuf, radhr(sp->rao), 3, 36000);
	    fs_sexa (decbuf, raddeg(sp->deco), 3, 36000);
	    tlog (sp, "     Offset RA/Dec: %s %s", rabuf, decbuf);
	}
	tlog (sp, "  Telescope Alt/Az: %s %s", altbuf, azbuf);

	tlog (sp, "  Camera: %c +%d+%dx%dx%d %dx%d %gs %s Pr%d %s",
			sp->filter, sp->sx, sp->sy, sp->sw, sp->sh, sp->binx,
		    sp->biny, sp->dur, ccdSO2Str(sp->shutter), sp->priority,
		    sp->imagefn);

	/* remaining are optional parts */
	bl = 0;

	mip = IMOT;
	if (mip->have)
	    bl += sprintf (buf+bl, " Filter: %c", telstatshmp->filter);

	mip = OMOT;
	if (mip->have)
	    bl += sprintf (buf+bl, " Focus: %g",
				mip->step * mip->cpos / mip->focscale / (2*PI));

	if (telstatshmp->domestate != DS_ABSENT) {
	    fs_sexa (rabuf, raddeg(telstatshmp->domeaz), 4, 3600);
	    bl += sprintf (buf+bl, " Dome: %s", rabuf);
	}

	switch (telstatshmp->shutterstate) {
	case SH_ABSENT:
	    break;
	case SH_OPEN:
	    bl += sprintf (buf+bl, " Roof: open");
	    break;
	default:
	    bl += sprintf (buf+bl, " Roof: %d", telstatshmp->shutterstate);
	    break;
	}

	if (bl > 0)
	    tlog (sp, " %s", buf);

}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: pr_regscan.c,v $ $Date: 2003/01/07 04:24:38 $ $Revision: 1.8 $ $Name:  $"};
