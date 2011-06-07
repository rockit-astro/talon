/* handle the Lights channel */

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

#include <sys/types.h>  /* for Socket data types */
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <netinet/in.h> /* for IP Socket data types */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <string.h>     /* for memset() */


#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "strops.h"
#include "telstatshm.h"
#include "running.h"
#include "misc.h"
#include "telenv.h"
#include "csimc.h"
#include "cliserv.h"
#include "tts.h"

#include "teled.h"

/* the current activity, if any */
static void (*active_func) (int first, ...);

static void lights_on (void);
static void lights_off (int first);
static void lights_poll (void);
static void lights_reset (void);

static void open_lights(void);
static void close_lights(void);
static void a_set (int intensity);

struct sock_info {
	int sock;
	struct sockaddr_in addr;
	int outlet;
	char login[64];
	char passwd[64];
};

struct sock_info sockinfo;

static int canClose = 1;
static double close_to = 0;
static double CLOSETO = 0.0;
static char ldcfn[] = "archive/config/lights.cfg";

/* called when we receive a message from the Lights fifo.
 * if msg just ignore.
 */
/* ARGSUSED */
void
lights_msg (msg)
char *msg;
{
	int intensity;

	/* no polling required */
	if (!msg) {
	    lights_poll();
	    return;
	}
	
	/* do reset before checking for `have' to allow for new config file */
	if (strncasecmp (msg, "reset", 5) == 0) {
	    lights_reset();
	    return;
	}

	/* ignore if none */
	if (telstatshmp->lights < 0)
	    return;

	/* setup? */
	if (canClose<0) {
	    tdlog ("Lights command before initial Reset: %s", msg?msg:"(NULL)");
	    return;
	}

	/* crack command -- including some oft-expected pseudnyms */
	if (sscanf (msg, "%d", &intensity) == 1) {
		if (intensity > 0)
	    		lights_on();
		else
			lights_off(1);
	}
	else if (strncasecmp (msg, "stop", 4) == 0)
	    lights_off (1);
	else if (strncasecmp (msg, "off", 3) == 0)
	    lights_off (1);
	else
	    fifoWrite (Lights_Id, -1, "Unknown command: %.20s", msg);
	return;
}

static void
lights_poll()
{
	if(active_func) {
		 Now *np = &telstatshmp->now;
		(*active_func)(0);
		tdlog("Waiting to close the lights");
		tdlog("MJD: %lf",mjd);
		tdlog("Close TO: %lf",close_to);
	}
}

static void
lights_on ()
{
	Now *np = &telstatshmp->now;
	if((mjd > close_to) && (telstatshmp->lights==0)) {
		close_to = mjd + CLOSETO;
	}
	if (telstatshmp->lights < 0)
	    fifoWrite (Lights_Id, -3, "No lights configured");
	else {
		open_lights();
		telstatshmp->lights = 1;
		fifoWrite (Lights_Id, 0, "Ok, lights now on");
		active_func = NULL;
	}
	return;
}

static void
lights_off(int first)
{
	Now *np = &telstatshmp->now;
	if (telstatshmp->lights < 0) {
	    fifoWrite (Lights_Id, -3, "No lights configured");
	    active_func = NULL;
	   	return;
	}
	if(first){
		if(mjd > close_to){
			tdlog("Actively trying to close the lights");
			close_lights();
			telstatshmp->lights = 0;
			active_func = NULL;
			toTTS ("The lights are now off.");
			fifoWrite (Lights_Id, 0, "Ok, lights now off");
			return;
		} else {
			active_func = (void *)lights_off;
			return;
		}
	}
	else if(mjd > close_to){
		tdlog("Passively trying to close the lights");
		close_lights();
		telstatshmp->lights = 0;
		active_func=NULL;
		toTTS ("The lights are now off.");
		fifoWrite (Lights_Id, 0, "Ok, lights now ff");
		return;
	}
	else
		active_func = (void *)lights_off;
}

static void
lights_reset()
{
#define NLDCFG  (sizeof(ldcfg)/sizeof(ldcfg[0]))
	static int LHAVE=0;
	static char LADDR[16],LLOGIN[64], LPASSWD[64];
	static int LPORT, LOUTLET;
	static CfgEntry ldcfg[] = {
	    {"LHAVE",	CFG_INT, &LHAVE},
	    {"LPORT",	CFG_INT, &LPORT},
	    {"LOUTLET",	CFG_INT, &LOUTLET},
	    {"LADDR",	CFG_STR, LADDR, sizeof(LADDR)},
	    {"LLOGIN", 	CFG_STR, LLOGIN, sizeof(LLOGIN)},
	    {"LPASSWD",	CFG_STR, LPASSWD, sizeof(LPASSWD)},
	    {"LCLOSETO", CFG_DBL, &CLOSETO},
	};
	int n;
	/* read params */
	n = readCfgFile (1, ldcfn, ldcfg, NLDCFG);
	if (n != NLDCFG) {
	    cfgFileError (ldcfn, n, (CfgPrFp)tdlog, ldcfg, NLDCFG);
	    die();
	}
	tdlog("I can read the file");
	/* close if open */
	CLOSETO /= SPD;
	telstatshmp->lights = -1;
	canClose = 1;
	close_to = 0.0;
	/* (re)open if have lights */
	if (!LHAVE) {
	    fifoWrite (Lights_Id, 0, "Not installed");
	    return;
	}
	telstatshmp->lights=0;
	//sockinfo.sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	
	memset(&sockinfo.addr, 0, sizeof(sockinfo.addr));
	
	sockinfo.addr.sin_family      = AF_INET;             /* Internet address family */	
	sockinfo.addr.sin_addr.s_addr = inet_addr(LADDR);   /* Server IP address */    	
	sockinfo.addr.sin_port        = htons(LPORT); /* Server port */
	strcpy(sockinfo.login,LLOGIN);
	strcpy(sockinfo.passwd,LPASSWD);
	sockinfo.outlet = LOUTLET;
	fifoWrite (Lights_Id, 0, "Reset complete");
}

void open_lights() 
{
	if (virtual_mode) return;

	sockinfo.sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(connect(sockinfo.sock, (struct sockaddr *) &sockinfo.addr, sizeof(sockinfo.addr)) < 0) {
		tdlog("Error connecting to socket to open the lights");
		return;
	}
	char buffer[256];
	sprintf(buffer,"%s\r\n",sockinfo.login);
	send(sockinfo.sock,buffer,strlen(buffer),0);
	bzero(buffer,256);
	sleep(1);
	bzero(buffer,256);
	recv(sockinfo.sock,buffer,sizeof(buffer),0);
	bzero(buffer,256);
	sprintf(buffer,"%s\r\n",sockinfo.passwd);
	send(sockinfo.sock,buffer,strlen(buffer),0);
	bzero(buffer,256);
	sprintf(buffer,"on %d\r\n",sockinfo.outlet);
	send(sockinfo.sock,buffer,strlen(buffer),0);
	close(sockinfo.sock);
}

void close_lights()
{
	if (virtual_mode) return;

	sockinfo.sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	int err = connect(sockinfo.sock, (struct sockaddr *) &sockinfo.addr, sizeof(sockinfo.addr));
	if( err < 0) {
		tdlog("Error connecting to socket to close the lights");
		tdlog(strerror(err));
		return;
	}
	char buffer[256];
	sprintf(buffer,"%s\r\n",sockinfo.login);
	send(sockinfo.sock,buffer,strlen(buffer),0);
	bzero(buffer,256);
	sleep(1);
	bzero(buffer,256);
	recv(sockinfo.sock,buffer,sizeof(buffer),0);
	bzero(buffer,256);
	sprintf(buffer,"%s\r\n",sockinfo.passwd);
	send(sockinfo.sock,buffer,strlen(buffer),0);
	bzero(buffer,256);
	sprintf(buffer,"off %d\r\n",sockinfo.outlet);
	send(sockinfo.sock,buffer,strlen(buffer),0);
	close(sockinfo.sock);
}

/* low level light control */
static void
a_set (int i)
{
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: lights.c,v $ $Date: 2001/04/19 21:12:09 $ $Revision: 1.1.1.1 $ $Name:  $"};
