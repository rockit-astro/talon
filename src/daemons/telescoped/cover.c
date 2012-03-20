/* handle the Cover channel */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "strops.h"
#include "telstatshm.h"
#include "running.h"
#include "misc.h"
#include "csimc.h"
#include "virmc.h"
#include "telenv.h"
#include "cliserv.h"
#include "tts.h"
#include "focustemp.h"

#include "teled.h"

/* Internal parameters */
#define MAXBUF 256

#define CS		(telstatshmp->coverstate)
#define CHAVE 	(CS!=CV_ABSENT)

static int COVERHAVE;
static double COVERTO;
static double cover_to;

static int cfd=-1;
static int COVERAXIS;

/* control and status connections */

/* functions to control covers */ 

static void (*active_func) (int first, ...);

static void cover_poll(void);
static void cover_reset(int first, ...);
static void cover_open(int first, ...);
static void cover_close(int first, ...);
static void cover_status(void); //IEEC

static void initCfg();

/* called when we receive a message from the Cover fifo.
 * if !msg just update things.
 */
void
cover_msg (msg)
	char *msg;
{
	/*
	   Manage FIFO messages for covers
	   */
	if(!msg) {
		cover_poll();
	}
	else if(strncasecmp(msg,"reset",5) == 0 ) {
		cover_reset(1);
	}
	else if(strncasecmp(msg,"coverClose",10) == 0 ) {
		cover_close(1);
	}
	else if(strncasecmp(msg,"coverOpen",9) == 0 )
		cover_open(1);
	else if(strncasecmp(msg,"status",6) == 0 ) { //IEEC
		cover_status();                          //IEEC
	}
	else
	{
	    fifoWrite (Cover_Id, -1, "Unknown command: %.20s", msg);
	}
	
}

void cover_reset(int first, ...) {
	if(first) {
		initCfg();
	}
}

void cover_init() {
	cfd = csiOpen(COVERAXIS);
	if (cfd == -1) return;

   	int status;
	status = csi_rix(cfd, "coverStatus();");
	if (status == 0) CS = CV_CLOSED;
	else if (status == 1) CS = CV_OPEN;
	else if (status == 2) CS = CV_CLOSING;
	else if (status == 3) CS = CV_OPENING;
	else CS = CV_OPENING;

	return;
}

void cover_close(int first, ...) {
	
	Now *np = &telstatshmp->now;
	char statusBuffer[MAXBUF];
	memset(statusBuffer, 0, MAXBUF);
	int n;
	
	if(!COVERHAVE) {
		fifoWrite(Cover_Id, -3, "No Cover to close");
		return;
	}
	
	if(first) {
		cover_to = mjd + COVERTO;
		active_func = cover_close;
	}
	
	if( CS!=CV_CLOSING ) {
		csi_w(cfd,"coverClose();");
		fifoWrite (Cover_Id, 2, "Starting close");
		CS = CV_CLOSING;
		active_func = cover_close;
		return;
	}
	
	if(mjd > cover_to) {
		fifoWrite(Cover_Id, -5, "Cover close timeout");
		CS=CV_IDLE;
		active_func = NULL;
		return;
	}
	
	if (!csiIsReady(cfd))
		return;
	
	if(csi_r (cfd, statusBuffer, sizeof(statusBuffer)) <= 0)
		return;
	
	if(!statusBuffer[0])
		return;
	
	n = atoi(statusBuffer);
	
	if(n<0) {
		fifoWrite (Cover_Id, n, "Close error: %s", statusBuffer+2); /* skip -n */
		CS=CV_IDLE;
		active_func=NULL;
		return;
	}
	
	fifoWrite(Cover_Id,0,"Close complete");
	CS = CV_CLOSED;
	active_func = NULL;
	/*
	if(first) {
		memset(statusBuffer, 0, MAXBUF);
		char statusBuffer[MAXBUF];
		csi_wr(cfd,statusBuffer,MAXBUF,"coverClose();");
		fifoWrite(Cover_Id, 0, statusBuffer);
	}
	*/
}

void cover_open(int first, ...) {

	Now *np = &telstatshmp->now;
	char statusBuffer[MAXBUF];
	memset(statusBuffer, 0, MAXBUF);
	int n;
	
	if(!COVERHAVE) {
		fifoWrite(Cover_Id, -3, "No Cover to open");
		return;
	}
	
	if(first) {
		cover_to = mjd + COVERTO;
		active_func = cover_open;
	}
	
	if( CS!=CV_OPENING ) {
		csi_w(cfd,"coverOpen();");
		fifoWrite (Cover_Id, 2, "Starting open");
		CS = CV_OPENING;
		active_func = cover_open;
		return;
	}
	
	if(mjd > cover_to) {
		fifoWrite(Cover_Id, -5, "Cover open timeout");
		CS = CV_IDLE;
		active_func = NULL;
		return;
	}
	
	if (!csiIsReady(cfd))
		return;
	
	if(csi_r (cfd, statusBuffer, sizeof(statusBuffer)) <= 0)
		return;
	
	if(!statusBuffer[0])
		return;
	
	n = atoi(statusBuffer);
	
	if(n<0) {
		fifoWrite (Cover_Id, n, "Open error: %s", statusBuffer+2); /* skip -n */
		CS=CV_IDLE;
		active_func=NULL;
		return;
	}
	
	fifoWrite(Cover_Id,0,"Open complete");
	CS = CV_OPEN;
	active_func = NULL;
}

void cover_poll() {
	if (active_func)
	    (*active_func)(0);
}
void initCfg() {
#define NCCFG   (sizeof(ccfg)/sizeof(ccfg[0]))
	//static int COVERHAVE;
	int n; 
	
	static CfgEntry ccfg[] = {
		{"COVERHAVE",	CFG_INT,	&COVERHAVE},
		{"COVERAXIS",	CFG_INT,	&COVERAXIS},
		{"COVERTO",	CFG_DBL,	&COVERTO},
	};
	
	n = readCfgFile (1, ccfn, ccfg, NCCFG);
	if (n != NCCFG) {
	    cfgFileError (ccfn, n, (CfgPrFp)tdlog, ccfg, NCCFG);
	    die();
	}
	
	COVERTO /= SPD;
	
	if(!COVERHAVE)
		CS = CV_ABSENT;
	
	cover_init();
}

void cover_status(void) 
{
    /* IEEC function to provide cover status through fifo calls */
   	int status;
    char buf[2];
    
    if(COVERHAVE)
    {
        if(csi_wr(cfd, buf, sizeof(buf), "coverStatus();")>0)
          status = atoi(buf);
        else
          status = -1;
        
        switch(status)
        {
            case 0:
                fifoWrite(Cover_Id, 0, "Covers are closed");
                break;
            case 1:
                fifoWrite(Cover_Id, 1, "Covers are open");
                break;
            case 2:
                fifoWrite(Cover_Id, 2, "Covers are closing");
                break;
            case 3:
                fifoWrite(Cover_Id, 3, "Covers are opening");
                break;
            default:
                fifoWrite(Cover_Id, -1, "Error retrieving covers status");
                break;
        }
    }
    else
      fifoWrite(Cover_Id, 0, "No covers defined");

	return;
}
