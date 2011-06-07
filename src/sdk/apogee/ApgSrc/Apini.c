/*==========================================================================*/
/* Apogee CCD INI Functions                                                 */
/*                                                                          */
/* revision   date       by                description                      */
/* -------- --------   ----     ------------------------------------------  */
/*   1.00   12/22/96    jmh     Created new API library files               */
/*   2.00   03-21-2000  gkr     Integrated LINUX conditionals               */
/*                                                                          */
/*==========================================================================*/

#ifdef _APGDLL
#include <windows.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "apccd.h"
#include "apglob.h"
#include "apdata.h"
#include "aplow.h"
#include "aperr.h"
#ifdef _APG_PPI
#include "apppi.h"
#endif
#ifdef _APG_NET
//#include "netio.h"
#include "apnet.h"
#endif

#include "config.h"

#ifdef LINUX
#include <unistd.h>
#include <ctype.h>
static int stricmp(char *s1, char *s2)
{
	int first, second;
	do {
		first = tolower(*s1);
		second = tolower(*s2);
		s1++;
		s2++;
	} while (first && (first == second));
	return (first - second);
}
#endif

static USHORT atoh(CHAR *s)
{
	USHORT val = 0;
	CHAR ch;
	
	while ((ch=*s++) != 0) {
		if ((ch >= '0') && (ch <= '9'))
			val = val * 16 + (USHORT) (ch - '0');
		else if ((ch >= 'a') && (ch <= 'f'))
			val = val * 16 + (USHORT) (ch - 'a' + 10);
		else if ((ch >= 'A') && (ch <= 'F'))
			val = val * 16 + (USHORT) (ch - 'A' + 10);
		else
			break;
	}
	return (val);
}

/*--------------------------------------------------------------------------*/
/* config_load                                                              */
/*                                                                          */
/* Load configuration data for a particular CCD controller from a standard  */
/* INI style data file.                                                     */
/* gkr - Modified to define default values for any absent INI entry         */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
config_load(HCCD ccd_handle, PCHAR cfgname)
{
    SHORT   plen, rc, gotcfg;
    USHORT  index = ccd_handle-1;
    USHORT  tmpval1;
    DOUBLE  tmpval2;
    CHAR    retbuf[256];
	CAMDATA	*cdp;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

	// Set up camera structure pointer
	cdp = &(camdata[index]);

    if (strlen(cfgname) == 0) {
    	cdp->errorx = APERR_CFGNAME;
        return CCD_ERROR;
    }

    gotcfg = FALSE;

    /* System */
    if (CfgGet (cfgname, "system", "base", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        cdp->base = atoh(retbuf);
        }
    else {
    	cdp->errorx = APERR_CFGBASE;
        return CCD_ERROR;           /* base address MUST be defined */
    	}

#ifdef _APG_NET
    if (CfgGet (cfgname, "system", "net_server", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
		strcpy(szServer, retbuf);
		}

    if (CfgGet (cfgname, "system", "net_port", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        tmpval1 = (USHORT) atoi(retbuf);
        if (tmpval1 > 1023)
        	uiPort = tmpval1;
    	}
#endif

#ifdef _APG_PPI
	cdp->reg_offset = 0;
    if (CfgGet (cfgname, "system", "reg_offset", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        cdp->reg_offset = (USHORT) atoi(retbuf);
        }

	c0_repeat = 1;
    if (CfgGet (cfgname, "system", "c0_repeat", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if (tmpval1 > 0)
        	c0_repeat = tmpval1;
    	}

    ecp_port = 0;
    if (CfgGet (cfgname, "system", "ecp", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        if (!stricmp("ON",retbuf) || !stricmp("TRUE",retbuf))
            ecp_port = 1;
        }
#endif

    cdp->mode = MODE_DEFAULT;
    if (CfgGet (cfgname, "system", "mode", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if (tmpval1 < 16)
                cdp->mode = uDefaultMode = tmpval1;
        }

	cdp->test = TEST_DEFAULT;
    if (CfgGet (cfgname, "system", "test", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if (tmpval1 < 16)
            cdp->test = uDefaultTest = tmpval1;
        }
    
	// Test for old style interface, affects VBIN handling
    cdp->old_cam = (cdp->mode == 0) && (cdp->test == 0);

    cdp->shutter_en = 0;
    if (CfgGet (cfgname, "system", "shutter", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        cdp->shutter_en = 0;
        if (!stricmp("ON",retbuf) || !stricmp("TRUE",retbuf))
            cdp->shutter_en = 1;
        }

    cdp->trigger_mode = 0;
    if (CfgGet (cfgname, "system", "trigger", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        cdp->trigger_mode = 0;
        if (!stricmp("ON",retbuf) || !stricmp("TRUE",retbuf))
            cdp->trigger_mode = 1;
        }

    cdp->caching = 0;
    if (!cdp->old_cam && (CfgGet (cfgname, "system", "caching", retbuf, sizeof(retbuf), &plen) == CFG_OK)) {
        gotcfg = TRUE;
        cdp->caching = 0;
        if (!stricmp("ON",retbuf) || !stricmp("TRUE",retbuf))
            cdp->caching = 1;
        }

	cdp->gain = 0;
    if (CfgGet (cfgname, "system", "gain", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        if (!stricmp("ON",retbuf) || !stricmp("TRUE",retbuf))
            cdp->gain = 1;
        }

    cdp->cable = 0;
    if (CfgGet (cfgname, "system", "cable", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        cdp->cable = 0;
        if (!stricmp("LONG",retbuf))
            cdp->cable = 1;
        }

	cdp->frame_timeout = 20;
    if (CfgGet (cfgname, "system", "frame_timeout", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if (tmpval1 > 0)
        	cdp->frame_timeout = tmpval1;
    	}
	
	cdp->line_timeout = 1;
    if (CfgGet (cfgname, "system", "line_timeout", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if (tmpval1 > 0)
        	cdp->line_timeout = tmpval1;
    	}
	
	cdp->slice = 0;
    if (CfgGet (cfgname, "system", "slice", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        if (!stricmp("ON",retbuf) || !stricmp("TRUE",retbuf))
            cdp->slice = 1;
    	}
	
	cdp->slice_time = 200;
    if (CfgGet (cfgname, "system", "slice_time", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if (tmpval1 > 0)
        	cdp->slice_time = tmpval1;
    	}
	
	cdp->data_bits = 16;
    if (CfgGet (cfgname, "system", "data_bits", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        switch (tmpval1) {
	        case 16:
    	    case 14:
        	case 12:
	        	cdp->data_bits = tmpval1;
	        }
    	}

    cdp->port_bits = 8;
    if (CfgGet (cfgname, "system", "port_bits", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if ((tmpval1 > 0) && (tmpval1 < 9))
        	cdp->port_bits = tmpval1;
        }

    cdp->tscale = 1.0;
    if (CfgGet (cfgname, "system", "tscale", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        cdp->tscale = (DOUBLE) atof(retbuf);
        }

    /* Geometry */

    if (CfgGet (cfgname, "geometry", "rows", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if ((tmpval1 >= 1) && (tmpval1 <= 4096))
            cdp->rows = tmpval1;
        }
    else {
    	cdp->errorx = APERR_CFGROWS;
        return CCD_ERROR;           /* rows MUST be defined */
       	}

    if (CfgGet (cfgname, "geometry", "columns", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if ((tmpval1 >= 1) && (tmpval1 <= 4096))
            cdp->cols = tmpval1;
        }
    else {
    	cdp->errorx = APERR_CFGCOLS;
        return CCD_ERROR;           /* columns MUST be defined */
        }

    cdp->bir = 3;
    if (CfgGet (cfgname, "geometry", "bir", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if ((tmpval1 >= 1) && (tmpval1 <= 4096))
            cdp->bir = tmpval1;
        }

    cdp->bic = 5;
    if (CfgGet (cfgname, "geometry", "bic", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if ((tmpval1 >= 1) && (tmpval1 <= 4096))
            cdp->bic = tmpval1;
        }

    cdp->skipr = 2;
    if (CfgGet (cfgname, "geometry", "skipr", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        cdp->skipr = tmpval1;
        }
    if (cdp->skipr >= cdp->bir) {
    	cdp->errorx = APERR_SKIPR;
        return CCD_ERROR;           /* bir MUST be larger than skipr */
        }

    cdp->skipc = 4;
    if (CfgGet (cfgname, "geometry", "skipc", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        cdp->skipc = (USHORT) atoi(retbuf);
        }
    if (cdp->skipc >= cdp->bic) {
    	cdp->errorx = APERR_SKIPC;
        return CCD_ERROR;           /* bic MUST be larger than skipc */
        }

    cdp->imgrows = cdp->rows - cdp->bir - 1;
    if (CfgGet (cfgname, "geometry", "imgrows", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if ((tmpval1 >= 1) && (tmpval1 <= 4096))
            cdp->imgrows = tmpval1;
        }

    cdp->imgcols = cdp->cols - cdp->bic - 1;
    if (CfgGet (cfgname, "geometry", "imgcols", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if ((tmpval1 >= 1) && (tmpval1 <= 4096))
            cdp->imgcols = tmpval1;
        }

    cdp->vflush = cdp->vbin = 1;
    if (!cdp->old_cam && (CfgGet (cfgname, "geometry", "vflush", retbuf, sizeof(retbuf), &plen) == CFG_OK)) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if ((tmpval1 >= 1) && (tmpval1 <= 64))
            cdp->vflush = tmpval1;
        }

    cdp->hflush = cdp->hbin = 1;
    if (!cdp->old_cam && (CfgGet (cfgname, "geometry", "hflush", retbuf, sizeof(retbuf), &plen) == CFG_OK)) {
        gotcfg = TRUE;
        tmpval1 = (USHORT) atoi(retbuf);
        if ((tmpval1 >= 1) && (tmpval1 <= 8))
            cdp->hflush = tmpval1;
        }

    /* Temperature */

    cdp->target_temp = 0.0;
    if (CfgGet (cfgname, "temp", "target", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        tmpval2 = (DOUBLE) atof(retbuf);
        if ((tmpval2 >= -40.0) && (tmpval2 <= 40.0))
            cdp->target_temp = tmpval2;
        }

    cdp->temp_backoff = 2.0;
    if (CfgGet (cfgname, "temp", "backoff", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        cdp->temp_backoff = (DOUBLE) atof(retbuf);
        }

    cdp->temp_cal = 0.0;
    if (CfgGet (cfgname, "temp", "cal", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        cdp->temp_cal = (DOUBLE) atof(retbuf);
        }

    cdp->temp_scale = 1.0;
    if (CfgGet (cfgname, "temp", "scale", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        cdp->temp_scale = (DOUBLE) atof(retbuf);
        }
    if (cdp->temp_scale == 0.0) {
    	cdp->errorx = APERR_TSCALE;
		return (CCD_ERROR);
		}    

    /* Camera Register */

    cdp->camreg = 0x80;
    if (cdp->gain)
    	if (CfgGet (cfgname, "camreg", "gain", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        	gotcfg = TRUE;
	        if (!strncmp(retbuf,"1",1) && cdp->gain)
	            cdp->camreg |= 0x08;
	        }

    if (CfgGet (cfgname, "camreg", "opt1", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        if (!strncmp(retbuf,"1",1))
            cdp->camreg |= 0x04;
        }

    if (CfgGet (cfgname, "camreg", "opt2", retbuf, sizeof(retbuf), &plen) == CFG_OK) {
        gotcfg = TRUE;
        if (!strncmp(retbuf,"1",1))
            cdp->camreg |= 0x02;
        }

    if ((cdp->gotcfg == FALSE) && (gotcfg == FALSE)) {
    	cdp->errorx = APERR_NOCFG;
        return CCD_ERROR;
        }

    return CCD_OK;
}


/*--------------------------------------------------------------------------*/
/* config_save                                                              */
/*                                                                          */
/* Save configuration data for a particular CCD to a standard INI style     */
/* data file.                                                               */
/*--------------------------------------------------------------------------*/

STATUS DLLPROC
config_save(HCCD ccd_handle, PCHAR cfgname)
{
    SHORT   rc;
    USHORT  index = ccd_handle-1;
    CHAR    pbuff[256];
    CAMDATA *cdp;

    if ((rc = check_parms(ccd_handle)) != CCD_OK)
        return rc;

    if ((strlen(cfgname) == 0) || (cfgname[0] == '\0'))
        return CCD_ERROR;

    unlink(cfgname);

	cdp = &(camdata[index]);

    sprintf(pbuff,"%x",cdp->base);
    CfgAdd (cfgname, "system",   "base",    pbuff);

    sprintf(pbuff,"%d",cdp->mode);
    CfgAdd (cfgname, "system",   "mode",    pbuff);

    sprintf(pbuff,"%d",cdp->test);
    CfgAdd (cfgname, "system",   "test",    pbuff);

    sprintf(pbuff,"%s",cdp->shutter_en?"ON":"OFF");
    CfgAdd (cfgname, "system",   "shutter", pbuff);

    sprintf(pbuff,"%s",cdp->trigger_mode?"ON":"OFF");
    CfgAdd (cfgname, "system",   "trigger", pbuff);

    sprintf(pbuff,"%s",cdp->caching?"ON":"OFF");
    CfgAdd (cfgname, "system",   "caching", pbuff);

    sprintf(pbuff,"%s",cdp->cable?"LONG":"SHORT");
    CfgAdd (cfgname, "system",   "cable",   pbuff);

    sprintf(pbuff,"%d",cdp->gain?1:0);
    CfgAdd (cfgname, "system",   "gain",   pbuff);

    sprintf(pbuff,"%d",cdp->data_bits);
    CfgAdd (cfgname, "system",   "data_bits",   pbuff);

    sprintf(pbuff,"%d",cdp->port_bits);
    CfgAdd (cfgname, "system",   "port_bits",   pbuff);

    sprintf(pbuff,"%f",cdp->tscale);
    CfgAdd (cfgname, "system",   "tscale",  pbuff);

    sprintf(pbuff,"%d",cdp->rows);
    CfgAdd (cfgname, "geometry", "rows",    pbuff);

    sprintf(pbuff,"%d",cdp->cols);
    CfgAdd (cfgname, "geometry", "columns", pbuff);

    sprintf(pbuff,"%d",cdp->imgcols);
    CfgAdd (cfgname, "geometry", "imgcols", pbuff);

    sprintf(pbuff,"%d",cdp->imgrows);
    CfgAdd (cfgname, "geometry", "imgrows", pbuff);

    sprintf(pbuff,"%d",cdp->bic);
    CfgAdd (cfgname, "geometry", "bic",     pbuff);

    sprintf(pbuff,"%d",cdp->bir);
    CfgAdd (cfgname, "geometry", "bir",     pbuff);

    sprintf(pbuff,"%d",cdp->hbin);
    CfgAdd (cfgname, "geometry", "hbin",    pbuff);

    sprintf(pbuff,"%d",cdp->vbin);
    CfgAdd (cfgname, "geometry", "vbin",    pbuff);

    sprintf(pbuff,"%f",cdp->target_temp);
    CfgAdd (cfgname, "temp",     "target",  pbuff);

    sprintf(pbuff,"%f",cdp->temp_backoff);
    CfgAdd (cfgname, "temp",     "backoff", pbuff);

    sprintf(pbuff,"%f",cdp->temp_cal);
    CfgAdd (cfgname, "temp",     "cal",     pbuff);

    sprintf(pbuff,"%f",cdp->temp_scale);
    CfgAdd (cfgname, "temp",     "scale",   pbuff);

    sprintf(pbuff,"%s",cdp->camreg & 0x08?"1":"0");
    CfgAdd (cfgname, "camreg",   "gain",    pbuff);

    sprintf(pbuff,"%s",cdp->camreg & 0x04?"1":"0");
    CfgAdd (cfgname, "camreg",   "opt1",    pbuff);

    sprintf(pbuff,"%s",cdp->camreg & 0x02?"1":"0");
    CfgAdd (cfgname, "camreg",   "opt2",    pbuff);

    return CCD_OK;
}
