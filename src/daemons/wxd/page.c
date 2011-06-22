/* create an output from a template file */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <termios.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telstatshm.h"
#include "telenv.h"
#include "running.h"
#include "strops.h"
#include "misc.h"
#include "tts.h"
#include "configfile.h"

#include "dd.h"

#define	MAXLIN	1024		/* max line length */
#define	TOKSTART	'%'	/* must preceed a template token to activate */

/* keywords in html files */
char DT_TPL[] = "WXDATE";
char WS_TPL[] = "WINDSPEED";
char WD_TPL[] = "WINDDIR";
char WN_TPL[] = "WINDDIRNAME";
char HM_TPL[] = "HUMIDITY";
char RN_TPL[] = "RAIN";
char PR_TPL[] = "PRESSURE";
char TM_TPL[] = "TEMPERATURE";

/* read template tfn and create output ofn.
 */
void
createPage (char *tfn, char *ofn, WxStats *wp, double t, double p)
{
	char line[MAXLIN];
	char *sp, *lp;
	FILE *tfp, *ofp;

	/* open template */
	tfp = fopen (tfn, "r");
	if (!tfp) {
	    daemonLog ("%s: %s\n", tfn, strerror(errno));
	    exit (1);
	}

	/* create page */
	ofp = fopen (ofn, "w");
	if (!ofp) {
	    daemonLog ("%s: %s\n", ofn, strerror(errno));
	    exit (1);
	}

	/* copy with substitutions. */
	while (fgets (lp = line, sizeof(line), tfp)) {
	    if ((sp = strchr(lp,TOKSTART)) && strstr (sp+1, DT_TPL)) {
		time_t t;
		time (&t);
		fprintf (ofp, "%.*s%s", sp-lp, lp, ctime(&t));
		lp = sp + strlen(DT_TPL) + 2;
	    }
	    if ((sp = strchr(lp,TOKSTART)) && strstr (sp+1, WS_TPL)) {
		fprintf (ofp, "%.*s%3d KPH", sp-lp, lp, wp->wspeed);
		lp = sp + strlen(WS_TPL) + 2;
	    }
	    if ((sp = strchr(lp,TOKSTART)) && strstr (sp+1, WD_TPL)) {
		fprintf (ofp, "%.*s%3d EofN", sp-lp, lp, wp->wdir);
		lp = sp + strlen(WD_TPL) + 2;
	    }
	    if ((sp = strchr(lp,TOKSTART)) && strstr (sp+1, WN_TPL)) {
		fprintf (ofp, "%.*s%s", sp-lp, lp,
						cardDirName(degrad(wp->wdir)));
		lp = sp + strlen(WN_TPL) + 2;
	    }
	    if ((sp = strchr(lp,TOKSTART)) && strstr (sp+1, HM_TPL)) {
		fprintf (ofp, "%.*s%3d %%", sp-lp, lp, wp->humidity);
		lp = sp + strlen(HM_TPL) + 2;
	    }
	    if ((sp = strchr(lp,TOKSTART)) && strstr (sp+1, RN_TPL)) {
		fprintf (ofp, "%.*s%4.1f mm", sp-lp, lp, wp->rain/10.);
		lp = sp + strlen(RN_TPL) + 2;
	    }
	    if ((sp = strchr(lp,TOKSTART)) && strstr (sp+1, PR_TPL)) {
		fprintf (ofp, "%.*s%5.2f inHg", sp-lp, lp, p/33.86);
		lp = sp + strlen(PR_TPL) + 2;
	    }
	    if ((sp = strchr(lp,TOKSTART)) && strstr (sp+1, TM_TPL)) {
		fprintf (ofp, "%.*s%4.1f F", sp-lp, lp, (9./5.)*t + 32);
		lp = sp + strlen(TM_TPL) + 2;
	    }
	    fputs (lp, ofp);
	}

	fclose (tfp);
	fclose (ofp);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: page.c,v $ $Date: 2001/04/19 21:12:11 $ $Revision: 1.1.1.1 $ $Name:  $"};
