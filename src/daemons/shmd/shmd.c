/* provide remote access to telstatshm
 * on master machine: rund shmd -m
 * on each remote:    rund shmd -s <master>
 *
 * N.B. byte order and struct layout assumed the same on each end
 */

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/param.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "csimc.h"
#include "misc.h"
#include "running.h"
#include "strops.h"
#include "telenv.h"
#include "telstatshm.h"

#define DEFPORT 7624 /* default tcp port */
#define DEFMS 100    /* default slave-side update ms */

#define REQ_CODE 'R' /* client requests shm returned */
static void usage(void);
static void masterMode(void);
static void slaveMode(void);
static int setupAsMaster(void);
static int newClient(int acceptfd);
static int handleClientRequest(int fd);
static void shmConnect(void);
static void slaveMode(void);
static int setupAsSlave(void);

static TelStatShm *telstatshmp; /* shared mem status segment */
static int port = DEFPORT;      /* tcp port to use */
static int updms = DEFMS;       /* slave update period */
static int mflag;               /* set if we are to be the master */
static int vflag;               /* set if want verbose */
static char *master;            /* if set, we r client connected here */
static char *me;                /* our program name */

int main(int ac, char *av[])
{
    me = basenm(av[0]);

    while ((--ac > 0) && ((*++av)[0] == '-'))
    {
        char *s;
        for (s = av[0] + 1; *s != '\0'; s++)
            switch (*s)
            {
            case 'm':
                mflag++;
                break;
            case 'p':
                if (ac < 2)
                    usage();
                port = atoi(*++av);
                ac--;
                break;
            case 's':
                if (ac < 2)
                    usage();
                master = *++av;
                ac--;
                break;
            case 'u':
                if (ac < 2)
                    usage();
                updms = atoi(*++av);
                ac--;
                break;
            case 'v':
                vflag++;
                break;
            default:
                usage();
            }
    }

    /* ac remaining args starting at av[0] */
    if (ac)
        usage();

    /* exactly one -m or -s */
    if (!!mflag == !!master)
        usage();
    if (mflag)
        masterMode();
    else
        slaveMode();

    return (0);
}

static void usage()
{
    fprintf(stderr, "Usage: %s [options]\n", me);
    fprintf(stderr, "Purpose: provide remote access to Talon shared memory status\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -m:        run as master on real system\n");
    fprintf(stderr, " -p port:   use tcp <port>; default is %d\n", DEFPORT);
    fprintf(stderr, " -s master: run as slave, connect to node <master>\n");
    fprintf(stderr, " -u ms:     slave updates every <ms>; default is %d\n", DEFMS);
    fprintf(stderr, " -v:        verbose\n");

    exit(1);
}

/* run as master.
 * connect to /existing/ telstatshm and offer copy service on tcp port.
 */
static void masterMode()
{
    int acceptfd;
    fd_set clients;
    int maxfdp1;

    shmConnect();
    acceptfd = setupAsMaster();
    FD_ZERO(&clients);
    FD_SET(acceptfd, &clients);
    maxfdp1 = acceptfd + 1;

    while (1)
    {
        fd_set fds;
        int i, n;

        fds = clients;
        if ((n = select(maxfdp1, &fds, NULL, NULL, NULL)) < 0)
        {
            daemonLog("select: %s\n", strerror(errno));
            exit(1);
        }

        while (n)
        {
            for (i = 0; i < maxfdp1; i++)
            {
                if (FD_ISSET(i, &fds))
                {
                    if (i == acceptfd)
                    {
                        int newfd = newClient(acceptfd);
                        FD_SET(newfd, &clients);
                        if (newfd + 1 > maxfdp1)
                            maxfdp1 = newfd + 1;
                    }
                    else
                    {
                        if (handleClientRequest(i) < 0)
                            FD_CLR(i, &clients);
                    }

                    n--;
                    break;
                }
            }
        }
    }
}

/* create the public master connection.
 * exit if trouble, else return the accept fd.
 */
static int setupAsMaster()
{
    struct sockaddr_in serv_socket;
    int reuse = 1;
    int serv_fd;

    /* make socket endpoint */
    if ((serv_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        daemonLog("socket: %s\n", strerror(errno));
        exit(1);
    }

    /* bind port for any IP address */
    memset(&serv_socket, 0, sizeof(serv_socket));
    serv_socket.sin_family = AF_INET;
    serv_socket.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_socket.sin_port = htons(port);
    if (setsockopt(serv_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
    {
        daemonLog("setsockopt: %s\n", strerror(errno));
        exit(1);
    }
    if (bind(serv_fd, (struct sockaddr *)&serv_socket, sizeof(serv_socket)) < 0)
    {
        daemonLog("bind: %s\n", strerror(errno));
        exit(1);
    }

    /* willing to accept connections with a backlog of 5 pending */
    if (listen(serv_fd, 5) < 0)
    {
        daemonLog("listen: %s\n", strerror(errno));
        exit(1);
    }

    /* serv_fd is ready */
    return (serv_fd);
}

/* new client connecting on acceptfd.
 * return its new private fd, else exit
 */
static int newClient(int acceptfd)
{
    struct sockaddr_in cli_socket;
    int cli_len, cli_fd;

    /* get a private connection to new client */
    cli_len = sizeof(cli_socket);
    cli_fd = accept(acceptfd, (struct sockaddr *)&cli_socket, &cli_len);
    if (cli_fd < 0)
    {
        daemonLog("accept: %s\n", strerror(errno));
        exit(1);
    }

    /* make nonblocking so we never get hung up */
    if (fcntl(cli_fd, F_SETFL, O_NONBLOCK) < 0)
    {
        daemonLog("O_NONBLOCK: %s\n", strerror(errno));
        exit(1);
    }

    /* ok */
    if (vflag)
    {
        long addr = ntohl(cli_socket.sin_addr.s_addr);
        daemonLog("New client at IP %d.%d.%d.%d on fd %d\n", 0xff & (addr >> 24), 0xff & (addr >> 16),
                  0xff & (addr >> 8), 0xff & (addr >> 0), cli_fd);
    }
    return (cli_fd);
}

/* handle request from client on fd.
 * return 0 if ok, -1 if EOF or error.
 */
static int handleClientRequest(int fd)
{
    char c;
    int n;

    n = read(fd, &c, 1);
    if (n == 0)
    {
        if (vflag)
            daemonLog("EOF from fd %d\n", fd);
        close(fd);
        return (-1);
    }
    if (n < 0)
    {
        daemonLog("Error from %d: %s\n", fd, strerror(errno));
        close(fd);
        return (-1);
    }

    /* 1 char -- must be REQ_CODE for now */
    if (c != REQ_CODE)
    {
        daemonLog("Bogus request code from %d: 0x%x\n", fd, c);
        close(fd);
        return (-1);
    }
    if (write(fd, telstatshmp, sizeof(TelStatShm)) < 0)
    {
        daemonLog("write %d: %s\n", fd, strerror(errno));
        close(fd);
        return (-1);
    }

    /* ok */
    return (0);
}

/* connect to telstatshm, creating if necessary.
 * exit if trouble.
 */
static void shmConnect()
{
    int len = sizeof(TelStatShm);
    int shmid;
    long addr;

    /* open/create */
    shmid = shmget(TELSTATSHMKEY, len, 0664);
    if (shmid < 0)
    {
        if (vflag)
            daemonLog("no existing shm -- trying to create\n");
        shmid = shmget(TELSTATSHMKEY, len, 0664 | IPC_CREAT);
        if (shmid < 0)
        {
            daemonLog("shm: %s\n", strerror(errno));
            exit(1);
        }
    }

    /* connect */
    addr = (long)shmat(shmid, (void *)0, 0);
    if (addr == -1)
    {
        daemonLog("shmat: %s", strerror(errno));
        exit(1);
    }

    /* global */
    telstatshmp = (TelStatShm *)addr;
    if (vflag)
        daemonLog("connected to shm. Size = %d\n", sizeof(TelStatShm));
}

/* run as slave connected to master/port.
 * create new TelStatShm.
 * query master every udphz.
 */
static void slaveMode()
{
    char c = REQ_CODE;
    TelStatShm tmpshm;
    char *ptr;
    int tot, n;
    int fd;

    shmConnect();
    fd = setupAsSlave();

    while (1)
    {
        if (write(fd, &c, 1) < 0)
        {
            daemonLog("write: %s\n", strerror(errno));
            exit(1);
        }
        ptr = (char *)&tmpshm;
        for (tot = 0; tot < sizeof(TelStatShm); tot += n)
        {
            if ((n = read(fd, ptr + tot, sizeof(TelStatShm) - tot)) < 0)
            {
                daemonLog("read: %s\n", strerror(errno));
                exit(1);
            }
        }
        memcpy(telstatshmp, &tmpshm, sizeof(TelStatShm)); /* all at once */
        usleep(updms * 1000);
    }
}

/* connect to the master shmd.
 * exit if trouble, else return the fd.
 */
static int setupAsSlave()
{
    struct sockaddr_in cli_socket;
    struct hostent *hp;
    int cli_fd, len;

    /* get host name running server */
    if (!(hp = gethostbyname(master)))
    {
        daemonLog("gethostbyname(%s): %s\n", master, strerror(errno));
        exit(1);
    }

    /* create a socket */
    if ((cli_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        daemonLog("socket: %s\n", strerror(errno));
        exit(1);
    }

    /* connect to the server */
    len = sizeof(cli_socket);
    memset(&cli_socket, 0, len);
    cli_socket.sin_family = AF_INET;
    cli_socket.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    cli_socket.sin_port = htons(port);
    if (connect(cli_fd, (struct sockaddr *)&cli_socket, len) < 0)
    {
        daemonLog("connect: %s\n", strerror(errno));
        exit(1);
    }

    /* ready */
    if (vflag)
        daemonLog("Connected to %s\n", master);
    return (cli_fd);
}
