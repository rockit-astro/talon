/* simple program to communicate with a CSIMC */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "csimc.h"

static void usage(void);
static void loadFile(void);

static char *pname;
static int verbose;

static char *host;
static int port = CSIMCPORT;
static char *filename;
static int node;
static int response;
static int wflag;
static int sawalarm;

int main(int ac, char *av[])
{
    pname = av[0];

    while ((--ac > 0) && ((*++av)[0] == '-'))
    {
        char *s;
        for (s = av[0] + 1; *s != '\0'; s++)
            switch (*s)
            {
            case 'f':
                if (ac < 2)
                    usage();
                filename = *++av;
                --ac;
                break;
            case 'h':
                if (ac < 2)
                    usage();
                host = *++av;
                --ac;
                break;
            case 'm':
                if (ac < 2)
                    usage();
                response = atoi(*++av);
                --ac;
                break;
            case 'n':
                if (ac < 2)
                    usage();
                node = atoi(*++av);
                --ac;
                break;
            case 'p':
                if (ac < 2)
                    usage();
                port = atoi(*++av);
                --ac;
                break;
            case 'r':
                response = 1;
                break;
            case 'v': /* verbose */
                verbose++;
                break;
            case 'w':
                if (ac < 2)
                    usage();
                wflag = atoi(*++av);
                --ac;
                break;
            default:
                usage();
                break;
            }
    }

    /* ac remaining args starting at av[0] */
    if (ac > 0)
        usage();

    loadFile();

    return (0);
}

static void usage()
{
    fprintf(stderr, "Usage: %s [options]\n", pname);
    fprintf(stderr, "Purpose: send command to CSIMC, optionally wait for response\n");
    fprintf(stderr, "$Revision: 1.2 $\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -f <f>: .cmc filename <f>; default is stdin\n");
    fprintf(stderr, " -h <h>: host TCP address; default 127.0.0.1\n");
    fprintf(stderr, " -m <l>: wait for and print <l> lines of response\n");
    fprintf(stderr, " -n <a>: load node address <a>; default 0\n");
    fprintf(stderr, " -p <p>: TCP port; default %d\n", CSIMCPORT);
    fprintf(stderr, " -r    : wait for and print one line of response\n");
    fprintf(stderr, " -v    : verbose\n");
    fprintf(stderr, " -w <s>: wait for <s> seconds of quiet then exit\n");

    exit(1);
}

static void onalarm(int dummy)
{
    sawalarm = 1;
}

static void loadFile()
{
    char buf[256];
    FILE *fp;
    int fd;

    /* connect to deamon */
    fd = csi_open(host, port, node);
    if (fd < 0)
    {
        perror("open");
        exit(1);
    }

    /* establish file */
    if (filename)
    {
        fp = fopen(filename, "r");
        if (!fp)
        {
            perror(filename);
            exit(1);
        }
    }
    else
    {
        fp = stdin;
        filename = "stdin";
    }

    /* choke it down */
    while (fgets(buf, sizeof(buf), fp))
        if (write(fd, buf, strlen(buf)) < 0)
        {
            perror(filename);
            exit(1);
        }

    /* wait for response lines back, or time out */
    if (wflag)
    {
        struct sigaction sa;
        sa.sa_handler = onalarm;
        sa.sa_flags = 0; /* do not restart */
        sigemptyset(&sa.sa_mask);
        sigaction(SIGALRM, &sa, NULL);
    }
    while (wflag || response-- > 0)
    {
        int n;
        if (wflag)
            alarm(wflag);
        for (n = 0; read(fd, &buf[n], 1) == 1 && buf[n] != '\n'; n++)
            continue;
        if (sawalarm)
            break;
        printf("%.*s\n", n, buf);
    }

    /* done */
    csi_close(fd);
    fclose(fp);
}
