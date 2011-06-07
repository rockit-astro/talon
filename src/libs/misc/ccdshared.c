#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccdcamera.h"

/* Functions that are shared by at least two CCD drivers */



/* Used by CCDServer and auxcam to parse the response from a camera
 * temperature query. Given a response string, it fills the *tp
 * struct with the appropriate information.
 */
void parseTemperatureString(char *msg, CCDTempInfo *tp)
{
    char *p;

    // STO 10-22-02: Extend this to support return of status string also

    /* First, read an integer temperature value */
    p = strtok(msg," ");
    if(!p) {
        p = msg;
    }
    tp->t = atoi(p);

    /* Then read an optional status string */
    p = strtok(NULL," \r\n\0");
    if(p) {
        if(!strcasecmp(p,"ERR")) {
            tp->s = CCDTS_ERR;
        }
        else if(!strcasecmp(p,"UNDER")) {
            tp->s = CCDTS_UNDER;
        }
        else if(!strcasecmp(p,"OVER")) {
            tp->s = CCDTS_OVER;
        }
        else if(!strcasecmp(p,"OFF")) {
            tp->s = CCDTS_OFF;
        }
        else if(!strcasecmp(p,"RDN")) {
            tp->s = CCDTS_RDN;
        }
        else if(!strcasecmp(p,"RUP")) {
            tp->s = CCDTS_RUP;
        }
        else if(!strcasecmp(p,"STUCK")) {
            tp->s = CCDTS_STUCK;
        }
        else if(!strcasecmp(p,"MAX")) {
            tp->s = CCDTS_MAX;
        }
        else if(!strcasecmp(p,"AMB")) {
            tp->s = CCDTS_AMB;
        }
        else if(!strcasecmp(p,"COOLING")) {
            // this generic return will assume we are ramping down...
            tp->s = CCDTS_RDN;
        }
        else {
            tp->s = CCDTS_AT; 	// default / old version behavior
        }	    	
    }
    else{
        tp->s = CCDTS_AT;	// Default / old version behavior
    }
}


/* Used by CCDServer and auxcam to parse the response of a CCD size
 * request. Takes response in *msg and fills *cep with appropriate values.
 * Returns 0 on success, or -1 if there was an error parsing *msg.
 */
int parseCCDSize(char *msg, CCDExpoParams *cep)
{
    if (sscanf (msg, "%d %d %d %d", 
        &cep->sw, &cep->sh, &cep->bx, &cep->by) != 4)
    {
        return (-1);
    }

    return (0);
}
