/* goodies to help out with a scan list (.sls) files and the Scan type. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "ccdcamera.h"
#include "scan.h"
#include "telenv.h"

#define	COMMENT1	'!'	/* beginning of comment */
#define	COMMENT2	'#'	/* another legal beginning of comment */
#define	FIRSTCOL	22	/* first column to use (0-based) */
#define	LASTLINE	21	/* last line number in a scan (0-based) */

int getExtVal(char *bp, Scan *sp);

/* read the next valid entry in the .sls file at fp.
 * if find one fill in *sp and return 0, else return -1.
 * call daemonLog() to report any really bogus lines.
 * if offset, also return offset of the status byte.
 */
int
readNextSLS (FILE *fp, Scan *sp, long *offset)
{
	char buf[1024], *bp = buf + FIRSTCOL;
	double tmp;
	int lineno;
	char *dp;
	int l;

	/* scan the lines, looking for first good set with nos 0..LASTLINE.
	 * if find a line you don't like, set lineno = 0 to skip the scan.
	 */
	lineno = 0;
	while (lineno <= LASTLINE && fgets (buf, sizeof(buf), fp)) {
	    /* basic checks for comments and being in sync */
	    if (buf[0] == COMMENT1 || buf[0] == COMMENT2)
		  continue;
	    if (atoi(buf) != lineno) {
		  lineno = 0;
		  continue;
	    }
	    /* strip off trailing newline -- beware long lines */
	    l = strlen (buf);
	    if (buf[l-1] == '\n')
		  buf[--l] = '\0';
	    else {
		  daemonLog ("Line too long: %d. Max: %d", l, sizeof(buf));
		  lineno = 0;
		  continue;
	    }
	    /* crack the field */
	    switch (lineno) {
	      case 0:
		  if (strlen(bp) != 1) {
            /* doesn't look right -- skip this scan */
	   	    daemonLog ("Bad .sls status code: %s", bp);
		    lineno = 0;
		    continue;
		  }
		  memset ((void *)sp, 0, sizeof(Scan));
		  sp->status = bp[0];	/* N(ew)/D(one)/F(ail) */
		  if (offset)
		      *offset = ftell(fp)-2;	/* back over nl too */
		  break;

	      case 1:
		  /* mjd was 25567.5 on 00:00:00 1/1/1970 UTC (UNIX' epoch) */
		  tmp = atof (bp);
		  sp->starttm = (time_t)((tmp - MJD0 - 25567.5)*SPD + 0.5);
		  break;

	      case 2:
		  sp->startdt = (int)floor(atof(bp)*60+.5);
		  break;

	      case 3:
		  ACPYZ (sp->schedfn, bp);
		  break;

	      case 4:
		  ACPYZ (sp->title, bp);
		  break;

	      case 5:
		  ACPYZ (sp->observer, bp);
		  break;

	      case 6:
		  ACPYZ (sp->comment, bp);
		  break;

	      case 7:
		  if (db_crack_line (bp, &sp->obj, buf) < 0) {
		      daemonLog ("Bad EDB: %s: %s", bp, buf);
		      lineno = 0;
		      continue;
		  }
		  break;

	      case 8:
		  if (scansex (bp, &tmp) < 0) {
		      daemonLog ("bad RAOffset format: %s", bp);
		      lineno = 0;
		      continue;
		  }
		  sp->rao = hrrad(tmp);
		  break;

	      case 9:
		  if (scansex (bp, &tmp) < 0) {
		      daemonLog ("bad DecOffset format: %s", bp);
		      lineno = 0;
		      continue;
		  }
		  sp->deco = degrad(tmp);
		  break;

          case 10:
		  if (sscanf (bp, "%d+%d", &sp->sx, &sp->sy) != 2) {
		      daemonLog ("Bad .sls frame Pos: %s", bp);
		      lineno = 0;
		      continue;
		  }
		  break;

          case 11:
		  if (sscanf (bp, "%dx%d", &sp->sw, &sp->sh) != 2) {
		      daemonLog ("Bad .sls frame Size: %s", bp);
		      lineno = 0;
		      continue;
		  }
		  break;

          case 12:
		  if (sscanf (bp, "%dx%d", &sp->binx, &sp->biny) != 2) {
		      daemonLog ("Bad .sls frame Binning: %s", bp);
		      lineno = 0;
		      continue;
    	  }
		  break;

	      case 13:
		  sp->dur = atof (bp);
		  break;

	      case 14:
		  if (ccdStr2SO (bp, &sp->shutter) < 0) {
		      daemonLog ("Bad shutter: %s", bp);
		      lineno = 0;
		      continue;
		  }
		  break;

	      case 15:
		  if (ccdStr2Calib (bp, &sp->ccdcalib) < 0) {
		      daemonLog ("Bad ccdcalib: %s", bp);
		      lineno = 0;
		      continue;
		  }
		  break;

	      case 16:
		  sp->filter = bp[0];
		  break;

	      case 17:
		  sp->compress = atoi (bp);
		  break;

// Extended function addition (3/7/2002)

	      case 18:
		  if(ccdStr2ExtAct(bp, &sp->ccdcalib) < 0) {
		      daemonLog("Bad Extended action: %s",bp);
		      lineno = 0;
		      continue;
		  }
		  break;

	      case 19:
		  if(getExtVal(bp, sp) < 0) {
		      daemonLog("Bad extended value: %s",bp);
		      lineno = 0;
		      continue;
		  }
		  break;

          case 20:
		  sp->priority = atoi (bp);
		  break;

	      case LASTLINE:
		  ACPYZ (sp->imagedn, bp);
		  dp = strrchr (sp->imagedn, '/');
		  if (!dp) {
		      daemonLog ("Full image pathname required: %s", bp);
		      lineno = 0;
		      //continue;
                return(-1);
		  }
		  *dp = '\0';			/* just dir in imagedn */
		  ACPYZ (sp->imagefn, dp+1);	/* just base name in imagefn*/
		  break;

	      default:
		  daemonLog ("Bug! Bogus switch case: %d", lineno);
		  return (-1);
	   } // end switch

	    /* advance expected line number */
	    lineno++;
	} // end  while

	/* ok iff found all the lines we expected */
	return (lineno == LASTLINE+1 ? 0 : -1);
}

#ifdef SCAN_TRACE
void
pr_scan (Scan *sp)
{
	char buf[1024];

	printf ("%s\n", sp->schedfn);
	printf ("%s\n", sp->imagefn);
	printf ("%s\n", sp->imagedn);
	printf ("%s\n", sp->obj.o_name);
	printf ("%s\n", sp->comment);
	printf ("%s\n", sp->title);
	printf ("%s\n", sp->observer);
	db_write_line (&sp->obj, buf);
	printf ("%s\n", buf);
	printf ("%g %g\n", sp->rao, sp->deco);
	printf ("%s\n", ccdCalib2Str(sp->ccdcalib));
	printf ("%d %d %d %d %d %d %d %d %g %c\n",
		    sp->compress, sp->sx, sp->sy, sp->sw, sp->sh, sp->binx,
		    sp->biny, sp->shutter, sp->dur, sp->filter);
	printf ("%d %d %ld %d %c\n", sp->priority, sp->running, sp->starttm,
		    sp->startdt, sp->status);
    printf("%s (%f %f)\n",ccdExtAct2Str(sp->ccdcalib),sp->extval1,sp->extval2);
}
#endif // SCAN_TRACE


// Used by following two functions
static char ccdStr[32];

/* convert a CCDCalib into a string.
 * return NULL if c is not valid.
 * N.B. string returned is static.
 */
char *
ccdCalib2Str (CCDCalib c)
{
	switch (c.newc) {
	case CT_NONE:
	    switch (c.data) {
	    case CD_NONE:    return (NULL);
	    case CD_RAW:     strcpy (ccdStr, "NONE");    break;
	    case CD_COOKED:  strcpy (ccdStr, "CATALOG"); break;
	    }
	    break;

	case CT_BIAS:    strcpy (ccdStr, "BIAS");    break;
	case CT_THERMAL: strcpy (ccdStr, "THERMAL"); break;
	case CT_FLAT:    strcpy (ccdStr, "FLAT");    break;

//    default: strcpy(ccdStr,"ext"); return ccdStr;  // extended... will be reinterpreted by ccdExtAct2Str
	default: strcpy(ccdStr,"CATALOG"); return ccdStr;
	}

	if (c.data == CD_NONE)
	    strcat (ccdStr, " ONLY");

	return (ccdStr);
}

/* Convert a CCDCalib into an extended action, if it is one */
char * ccdExtAct2Str(CCDCalib c)
{
    switch(c.newc) {
        case CT_FOCUSPOS:
            strcpy(ccdStr,"FOCUSPOS");
            break;
        case CT_FOCUSOFF:
            strcpy(ccdStr,"FOCUSOFF");
            break;
        case CT_AUTOFOCUS:
            strcpy(ccdStr,"AUTOFOCUS");
            break;
        case CT_FIXEDALTAZ:
            strcpy(ccdStr,"FIXEDALTAZ");
            break;
        case CT_EXTCMD:
            strcpy(ccdStr,"CMDLINE");
            break;
        default:
	    strcpy(ccdStr,"");
	    break;
    }

    return (ccdStr);
}

/* Convert extended values into a string representation based upon type of extension */
char *extActValueStr(Scan *sp)
{
    static char str[1024];
	double alt,az;

	// Convert to extval1, extval2 equivalents from new string argument format (10/29/09) for backward compatibility
	strcpy(str, sp->extcmd);
	double ext1=0, ext2=0;
	char* pc = strtok(str, ", ");
	if(pc)
	{
	    ext1 = strtod(pc, NULL);
	    pc += strlen(pc);
	    while(*pc && (*pc == ',' || *pc <=32 )) pc++;
	    ext2 = strtod(pc, NULL);
	}

    switch(sp->ccdcalib.newc) {
        case CT_FOCUSPOS:
        case CT_FOCUSOFF:
            //sprintf(str,"%.1f",sp->extval1);
            sprintf(str,"%.1f",ext1);
            break;
        case CT_AUTOFOCUS:
            /*
            if(sp->extval1 == 0) { strcpy(str,"OFF"); }
            else if(sp->extval1 == 1) { strcpy(str,"ON"); }
            else if(sp->extval1 == 2) { strcpy(str,"PERFORM"); }
            */
            if(ext1 == 0) { strcpy(str,"OFF"); }
            else if(ext1 == 1) { strcpy(str,"ON"); }
            else if(ext1 == 2) { strcpy(str,"PERFORM"); }
            else { strncpy(str, sp->extcmd, sizeof(str)-1); str[sizeof(str)-1] = 0; } // allow command to be passed literally too
            break;
        case CT_FIXEDALTAZ:
//            alt = raddeg(sp->extval1);
//            az = raddeg(sp->extval2);
            alt = raddeg(ext1);
            az = raddeg(ext2);
            if(alt == NO_ALTAZ_ENTRY) { // magic value: means nothing was entered
	            strcpy(str,"");
            } else {
            	sprintf(str,"%g, %g",alt,az);
            }
            break;
        case CT_EXTCMD:
            strncpy(str, sp->extcmd, sizeof(str)-1);
            str[sizeof(str)-1] = 0;
            break;
        default:
            strcpy(str,"");
            break;
    }

    return (str);
}

/* convert the given string to the CCDCalib at *cp.
 * return 0 if ok, else -1
 */
int
ccdStr2Calib (char *s1, CCDCalib *cp)
{
    char s[40];
    int i;
    strcpy(s,s1);
    i = strlen(s);
    while(i && s[i] <=32) {
        s[i] = 0;
        i--;
    }
	if (!strcasecmp (s, "NONE")) {
	    cp->newc = CT_NONE;
	    cp->data = CD_RAW;
	} else if (!strcasecmp (s, "CATALOG")) {
	    cp->newc = CT_NONE;
	    cp->data = CD_COOKED;
	} else if (!strcasecmp (s, "BIAS")) {
	    cp->newc = CT_BIAS;
	    cp->data = CD_COOKED;
	} else if (!strcasecmp (s, "THERMAL")) {
	    cp->newc = CT_THERMAL;
	    cp->data = CD_COOKED;
	} else if (!strcasecmp (s, "FLAT")) {
	    cp->newc = CT_FLAT;
	    cp->data = CD_COOKED;
	} else if (!strcasecmp (s, "BIAS ONLY")) {
	    cp->newc = CT_BIAS;
	    cp->data = CD_NONE;
	} else if (!strcasecmp (s, "THERMAL ONLY")) {
	    cp->newc = CT_THERMAL;
	    cp->data = CD_NONE;
	} else if (!strcasecmp (s, "FLAT ONLY")) {
	    cp->newc = CT_FLAT;
	    cp->data = CD_NONE;
	} else if (!strcasecmp(s,"ext")) {
        // ext is okay, but do nothing. Ext Act will set values
    } else
	    return (-1);

	return (0);
}

/* Convert string to extended action for ccd calib */
int ccdStr2ExtAct(char *s1, CCDCalib *cp)
{
    char s[40];
    int i;

    strcpy(s,s1);
    i = strlen(s);
    while(i && s[i] <=32) {
        s[i] = 0;
        i--;
    }
    if(!strcasecmp(s, "")) {
    	return 0;
    }
    cp->data = CD_NONE;
    if(!strcasecmp(s, "FOCUSPOS")) {
        cp->newc = CT_FOCUSPOS;
    }
    else if(!strcasecmp(s,"FOCUSOFF")) {
        cp->newc = CT_FOCUSOFF;
    }
    else if(!strcasecmp(s,"AUTOFOCUS")) {
        cp->newc = CT_AUTOFOCUS;
    }
    else if(!strcasecmp(s,"FIXEDALTAZ")) {
        cp->newc = CT_FIXEDALTAZ;
    }
    else if(!strcasecmp(s,"CMDLINE")) {
        cp->newc = CT_EXTCMD;
    }
    else
        return(-1);

    // in all existing ext. cases, we want normal (CATALOG) ccd calibration
	cp->data = CD_COOKED;
    return (0);
}

/* Parse the values from the ext value line */
int getExtVal(char *bp, Scan *sp)
{
    char s[1024];
    int i;
    char *p; //, *chkp;

    strcpy(s,bp);
    i = strlen(s);
    while(i && s[i] <=32) {
        s[i] = 0;
        i--;
    }

	// The new command format is really just copied across. Old code commented out here for historical reference only (STO 10/29/09)

    if(sp->ccdcalib.newc == CT_FIXEDALTAZ) {
        if(!strcasecmp(s,"")) {
    //        sp->extval1 = NO_ALTAZ_ENTRY;  // code value that means that nothing was entered here
    //        sp->extval2 = 0;
            sprintf(sp->extcmd, "%d, 0", NO_ALTAZ_ENTRY);
            return 0;
        }
        /*
    	chkp = s;
    	sp->extval1 = degrad(strtod(s, &chkp));
    	if(chkp == s) {
    		sp->extval1 = NO_ALTAZ_ENTRY; // code value that means that nothing was entered here
    	}
    	else if(p) sp->extval2 = degrad(strtod(p, NULL));
    	*/
        strcpy(sp->extcmd, s);

    } else if(sp->ccdcalib.newc == CT_AUTOFOCUS) {
        /*
        if(!strcasecmp(s,"ON")) sp->extval1 = 1;
        else if(!strcasecmp(s,"OFF")) sp->extval1 = 0;
        else if(!strcasecmp(s,"PERFORM")) sp->extval1 = 2;
        else return(-1);
        */
        strcpy(sp->extcmd, s);
    } else {
        /*
    	sp->extval1 = strtod(s, NULL);
    	if(p) sp->extval2 = strtod(p, NULL);
    	*/
        strcpy(sp->extcmd, s);
    }

    return(0);
}

/* return all possible legal CCDCalib string representations */
void
ccdCalibStrs (char *names[CAL_NSTR])
{
	names[0] = "NONE";
	names[1] = "CATALOG";
	names[2] = "BIAS";
	names[3] = "THERMAL";
	names[4] = "FLAT";
	names[5] = "BIAS ONLY";
	names[6] = "THERMAL ONLY";
	names[7] = "FLAT ONLY";
}

/* convert CCDShutterOptions o to a string.
 * return NULL if o is not valid.
 * N.B. string returned is static.
 */
char *
ccdSO2Str (CCDShutterOptions o)
{
	switch (o) {
	case CCDSO_Closed:	return ("Closed");
	case CCDSO_Open:	return ("Open");
	case CCDSO_Dbl:		return ("Dbl Expose");
	case CCDSO_Multi:	return ("Multi Expose");
	default:		return (NULL);
	}
}

/* convert the given string to the CCDShutterOptions at *op.
 * return 0 if ok, else -1
 */
int
ccdStr2SO (char *s, CCDShutterOptions *op)
{
	if (!strcasecmp (s, "closed"))
	    *op = CCDSO_Closed;
	else if (!strcasecmp (s, "open"))
	    *op = CCDSO_Open;
	else if (!strcasecmp (s, "dbl expose"))
	    *op = CCDSO_Dbl;
	else if (!strcasecmp (s, "multi expose"))
	    *op = CCDSO_Multi;
	else
	    return (-1);

	return (0);
}

/* return all possible legal CCDShutterOption string representations */
void
ccdSOStrs (char *names[CCDSO_NSTR])
{
	names[0] = "Closed";
	names[1] = "Open";
	names[2] = "Dbl Expose";
	names[3] = "Multi Expose";
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: scan.c,v $ $Date: 2006/08/27 20:24:51 $ $Revision: 1.7 $ $Name:  $"};
