/* functions to support the TELHOME env variable, and logging.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "strops.h"
#include "telenv.h"

static char *telhome;
static char telhome_def[] = "/usr/local/telescope";

static void getTELHOME(void);

/* just like fopen() except tries name literally first then prepended
 * with $TELHOME (or default if not defined), unless starts with /.
 */
FILE *telfopen(char *name, char *how)
{
    FILE *fp;

    if (!(fp = fopen(name, how)) && name[0] != '/')
    {
        char envname[1024];
        telfixpath(envname, name);
        fp = fopen(envname, how);
    }
    return (fp);
}

/* just like fopen() except tries name literally first then prepended
 * with $TELHOME (or default if not defined), unless starts with /.
 */
int telopen(char *name, int flags, ...)
{
    char envname[1024];
    int ret;

    if (flags & O_CREAT)
    {
        va_list ap;
        int mode;

        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
        ret = open(name, flags, mode);
        if (ret < 0 && name[0] != '/')
        {
            telfixpath(envname, name);
            ret = open(envname, flags, mode);
        }
    }
    else
    {
        ret = open(name, flags);
        if (ret < 0 && name[0] != '/')
        {
            telfixpath(envname, name);
            ret = open(envname, flags);
        }
    }

    return (ret);
}

/* convert the old path to the new path, allowing for TELHOME.
 * this is for cases when a pathname is used for other than open, such as
 * opendir, unlink, mknod, etc etc.
 * it is ok for caller to use the same buffer for each.
 */
void telfixpath(char *newp, char *old)
{
    char tmp[1024];

    getTELHOME();
    if (telhome && old[0] != '/')
        (void)sprintf(tmp, "%s/%s", telhome, old);
    else
        (void)strcpy(tmp, old);
    (void)strcpy(newp, tmp);
}

/* reopen stdout and stderr so they go to $TELHOME/archive/logs/<progname>.log
 * and are unbuffered for improved delivery reliability.
 * return 0 if ok, else -1.
 */
int telOELog(char *progname)
{
    char logpath[1024];
    int ok = -1;
    FILE *fp;

    /* just the basic name */
    progname = basenm(progname);

    /* connect to proper log file -- leave unchanged if trouble */
    getTELHOME();
    sprintf(logpath, "%s/archive/logs/%s.log", telhome, progname);
    if ((fp = fopen(logpath, "a")) != NULL)
    {
        /* can't trust freopen to fail gracefully */
        fclose(fp);
        freopen(logpath, "a", stdout);
        freopen(logpath, "a", stderr);
        setbuf(stdout, NULL);
        setbuf(stderr, NULL);
        ok = 0;
    }
    else
    {
        setbuf(stdout, NULL);
        setbuf(stderr, NULL);
        printf("%s: %s\n", logpath, strerror(errno));
    }

    return (ok);
}

/* return a pointer to a static string of the form YYYYMMDDHHMMSS
 * based on the given UTC time_t value.
 */
char *timestamp(time_t t)
{
    static char str[24];
    struct tm *tmp = gmtime(&t);

    if (!tmp)
        sprintf(str, "gmtime failed!"); /* N.B. same length */
    else
        sprintf(str, "%04d-%02d-%02dT%02d:%02d:%02d", tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour,
                tmp->tm_min, tmp->tm_sec);

    return (str);
}

/* rather like printf but prepends timestamp().
 * also appends \n if not in result.
 */
void daemonLog(char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    int l;

    /* start with time stamp */
    l = sprintf(buf, "%s INFO ", timestamp(time(NULL)));

    /* format the message */
    va_start(ap, fmt);
    l += vsprintf(buf + l, fmt, ap);
    va_end(ap);

    /* add \n if not already */
    if (l > 0 && buf[l - 1] != '\n')
    {
        buf[l++] = '\n';
        buf[l] = '\0';
    }

    /* log to fp */
    fputs(buf, stdout);
    fflush(stdout);
}

static void getTELHOME()
{
    if (telhome)
        return;
    telhome = getenv("TELHOME");
    if (!telhome)
        telhome = telhome_def;
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid,
                         "@(#) $RCSfile: telenv.c,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $"};
