/* an interface to a text-to-speech system.
 */

#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "telenv.h"
#include "tts.h"

static char ttsfifo[] = "comm/tts.in";

static FILE *openTTS(void);

/* send the message to the TTS.
 * return 0 if ok, else -1
 */
int toTTS(char *fmt, ...)
{
    FILE *fp;

    fp = openTTS();
    if (fp)
    {
        char buf[1024];
        va_list ap;
        int n;

        va_start(ap, fmt);
        n = vsprintf(buf, fmt, ap);
        va_end(ap);

        if (n > 0 && buf[n - 1] != '\n')
        {
            buf[n++] = '\n';
            buf[n] = '\0';
        }
        fputs(buf, fp);
        fclose(fp);
        return (0);
    }
    return (-1);
}

static FILE *openTTS()
{
    struct stat st;
    int fd;

    fd = telopen(ttsfifo, O_WRONLY | O_NONBLOCK);
    if (fd < 0)
        return (NULL);
    if (fstat(fd, &st) < 0 || !S_ISFIFO(st.st_mode))
    {
        close(fd);
        return (NULL);
    }

    return (fdopen(fd, "w"));
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid,
                         "@(#) $RCSfile: tts.c,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $"};
