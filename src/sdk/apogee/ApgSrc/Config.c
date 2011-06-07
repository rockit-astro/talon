#ifdef _APGDLL
#include <windows.h>
#endif

#include <stdio.h>                                   
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>

#ifndef LINUX
#include <dos.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include "config.h"

#ifdef LINUX
static int strnicmp(char *s1, char *s2, int len)
{
	int first, second;
	int pos=0;
	
	do {
		first = tolower(*s1);
		second = tolower(*s2);
		s1++;
		s2++;
		pos++;
	} while (first && (first == second) && (pos < len));
	return (first - second);
}
#endif

static void trimstr(char *s)
{
    register char *p;

    p = s + (strlen(s) - 1);
    while (isspace(*p))
        p--;
    *(++p) = '\0';
}


/*--------------------------------------------------------------------------*/
/* CfgGet                                                                   */
/*                                                                          */
/* Retrieve a parameter from an INI file. Returns a status code (see the    */
/* file config.h) and the paramter string in retbuff.                       */
/*--------------------------------------------------------------------------*/

short CfgGet ( char  *ininame,
               char  *inisect,
               char  *iniparm,
               char  *retbuff,
               short bufflen,
               short *parmlen)
{
    short gotsect;
    FILE  *inifp;
    char tbuf[256];
    char  *ss, *eq, *ps, *vs, *ptr;

    /* attempt to open INI file */ 

    if ((inifp = fopen(ininame,"r")) == NULL)
        return(CFG_NOFILE);

    /* find the target section  */  

    gotsect = 0;
    while (fgets(tbuf,256,inifp) != NULL) {
        if ((ss = strchr(tbuf,'[')) != NULL) {
            if (strnicmp(ss+1,inisect,strlen(inisect)) == 0) {
                gotsect = 1;
                break;
                }
            }
        }

    if (!gotsect) {                             /* section not found        */
        fclose(inifp);
        return(CFG_NOSECT);
        }

    while (fgets(tbuf,256,inifp) != NULL) {     /* find parameter in sect   */ 

        if ((ptr = strrchr(tbuf,'\n')) != NULL) /* remove newline if there  */        
            *ptr = '\0';

        ps = tbuf+strspn(tbuf," \t");           /* find the first non-blank */

        if (*ps == ';')                         /* Skip line if comment     */
            continue;

        if (*ps == '[') {                       /* Start of next section    */
            fclose(inifp);
            return(CFG_NOPARM);
            }

        eq = strchr(ps,'=');                    /* Find '=' sign in string  */

        if (eq)
            vs = eq + 1 + strspn(eq+1," \t");   /* Find start of value str  */
        else
            continue;

        /* found the target parameter */

        if (strnicmp(ps,iniparm,strlen(iniparm)) == 0) {

            fclose(inifp);
                                                
            if ((ptr = strchr(vs,';')) != NULL) /* cut off an EOL comment   */
                *ptr = '\0';

            if ((short)strlen(vs) > bufflen - 1) {     /* not enough buffer space  */
                strncpy(retbuff,vs,bufflen - 1);
                retbuff[bufflen - 1] = '\0';    /* put EOL in string        */
                *parmlen = bufflen;
                return(CFG_HAVEMORE);
                }
            else {
                strcpy(retbuff,vs);             /* got it                   */
                trimstr(retbuff);               /* trim any trailing blanks */
                *parmlen = strlen(retbuff);
                return(CFG_OK);
                }
            }
        }

    fclose(inifp);
    return(CFG_NOPARM);                         /* parameter not found      */
}


/*--------------------------------------------------------------------------*/
/* CfgAdd                                                                   */
/*                                                                          */
/* Adds a parameter to an INI file. If the section does not exist,it is     */
/* added before the parameter. If the file does not exists,it is created.   */
/*--------------------------------------------------------------------------*/

short CfgAdd (char  *ininame,
              char  *inisect,
              char  *iniparm,
              char  *inival)
{
    FILE *inifp,*tmpfp;
    char tbuf[256];
    char tname[] = "ITXXXXXX";
    char *ss,*ps;


    if ((inifp = fopen(ininame,"r")) == NULL) {
        if ((inifp = fopen(ininame,"w")) == NULL)
            return(CFG_NOFILE);
        fprintf(inifp,"\n[%s]\n",inisect);
        fprintf(inifp,"  %s = %s\n",iniparm,inival);
        fclose(inifp);
        return CFG_OK;
        }

    if (mktemp(tname) == NULL) {
        fclose(inifp);
        return(CFG_NOFILE);
        }

    if ((tmpfp = fopen(tname,"w")) == NULL) {
        fclose(inifp);
        return(CFG_NOFILE);
        }

    /* First find the right section */

    while (fgets(tbuf,256,inifp) != NULL) {

        if ((ss = strchr(tbuf,'[')) != NULL) {

            if (strnicmp(ss+1,inisect,strlen(inisect)) == 0) {

                /* write out the section line */

                fputs(tbuf,tmpfp);

                /* See if the parameter is already there... */

                while (fgets(tbuf,256,inifp) != NULL) {

                    ps = tbuf + strspn(tbuf," \t");  /* find non-blank      */

                    if (*ps == '[')             /* got next section instead */
                        break;

                    if (strnicmp(ps,iniparm,strlen(iniparm)) == 0) {
                        fclose(inifp);
                        fclose(tmpfp);
                        unlink(tname);
                        return(CFG_HAVEPARM);
                        }

                    fputs(tbuf,tmpfp);          /* save line and go again   */
                    }

                /* Create and write out the new parameter line */
                fprintf(tmpfp,"  %s = %s\n",iniparm,inival);

                if (!feof(inifp))
                    fputs(tbuf,tmpfp);          /* output parameter line    */

                /* read the rest of the original file to the temp file */

                while (fgets(tbuf,256,inifp) != NULL)
                    fputs(tbuf,tmpfp);

                fclose(inifp);
                fclose(tmpfp);

                remove(ininame);                /* make temp file new INI   */
                rename(tname,ininame);

                return(CFG_OK);
                } 
            else
                fputs(tbuf,tmpfp);
            } 
        else
            fputs(tbuf,tmpfp);
        }

    /* if we make it to here then we need add the section and the parm */

    fprintf(tmpfp,"\n[%s]\n",inisect);
    fprintf(tmpfp,"  %s = %s\n",iniparm,inival);

    fclose(inifp);
    fclose(tmpfp);

    remove(ininame);                            /* make temp file new INI   */
    rename(tname,ininame);

    return(CFG_OK);
}



#ifdef TEST_INI
void main(void)
{
    short plen, rc;
    char  retbuf[256];

    printf("Creating/Appending INI file\n");

    rc = CfgAdd ("test.ini","section1","parm1","TRUE");
    if (rc != CFG_OK)
        goto ERROR1;
    rc = CfgAdd ("test.ini","section1","parm2","TRUE");
    if (rc != CFG_OK)
        goto ERROR1;
    rc = CfgAdd ("test.ini","section1","parm3","TRUE");
    if (rc != CFG_OK)
        goto ERROR1;
    rc = CfgAdd ("test.ini","section1","parm4","TRUE");
    if (rc != CFG_OK)
        goto ERROR1;
    rc = CfgAdd ("test.ini","section1","parm5","TRUE");
    if (rc != CFG_OK)
        goto ERROR1;
    rc = CfgAdd ("test.ini","section2","parm1","TRUE");
    if (rc != CFG_OK)
        goto ERROR1;
    rc = CfgAdd ("test.ini","section2","parm2","TRUE");
    if (rc != CFG_OK)
        goto ERROR1;
    rc = CfgAdd ("test.ini","section2","parm3","TRUE");
    if (rc != CFG_OK)
        goto ERROR1;
    rc = CfgAdd ("test.ini","section2","parm4","TRUE");
    if (rc != CFG_OK)
        goto ERROR1;
    rc = CfgAdd ("test.ini","section2","parm5","TRUE");
    if (rc != CFG_OK)
        goto ERROR1;

    printf("Reading INI file\n\n");

    rc = CfgGet ("test.ini","section1","parm1",retbuf,sizeof(retbuf),&plen);
    if (rc != CFG_OK)
        goto ERROR2;
    printf("section1: parm1 %s %d\n",retbuf,plen); 
    rc = CfgGet ("test.ini","section1","parm2",retbuf,sizeof(retbuf),&plen);
    if (rc != CFG_OK)
        goto ERROR2;
    printf("section1: parm2 %s %d\n",retbuf,plen); 
    rc = CfgGet ("test.ini","section1","parm3",retbuf,sizeof(retbuf),&plen);
    if (rc != CFG_OK)
        goto ERROR2;
    printf("section1: parm3 %s %d\n",retbuf,plen); 
    rc = CfgGet ("test.ini","section1","parm4",retbuf,sizeof(retbuf),&plen);
    if (rc != CFG_OK)
        goto ERROR2;
    printf("section1: parm4 %s %d\n",retbuf,plen); 
    rc = CfgGet ("test.ini","section1","parm5",retbuf,sizeof(retbuf),&plen);
    if (rc != CFG_OK)
        goto ERROR2;
    printf("section1: parm5 %s %d\n",retbuf,plen); 
    rc = CfgGet ("test.ini","section2","parm1",retbuf,sizeof(retbuf),&plen);
    if (rc != CFG_OK)
        goto ERROR2;
    printf("section2: parm1 %s %d\n",retbuf,plen); 
    rc = CfgGet ("test.ini","section2","parm2",retbuf,sizeof(retbuf),&plen);
    if (rc != CFG_OK)
        goto ERROR2;
    printf("section2: parm2 %s %d\n",retbuf,plen); 
    rc = CfgGet ("test.ini","section2","parm3",retbuf,sizeof(retbuf),&plen);
    if (rc != CFG_OK)
        goto ERROR2;
    printf("section2: parm3 %s %d\n",retbuf,plen); 
    rc = CfgGet ("test.ini","section2","parm4",retbuf,sizeof(retbuf),&plen);
    if (rc != CFG_OK)
        goto ERROR2;
    printf("section2: parm4 %s %d\n",retbuf,plen); 
    rc = CfgGet ("test.ini","section2","parm5",retbuf,sizeof(retbuf),&plen);
    if (rc != CFG_OK)
        goto ERROR2;
    printf("section2: parm5 %s %d\n",retbuf,plen); 

    printf("\n\nTest Complete\n");
    return;

    ERROR1:

    printf("INI creation failed. Error code = %d\n",rc)
    return;

    ERROR2:

    printf("INI readback failed. Error code = %d\n",rc);
    return;
}
#endif
