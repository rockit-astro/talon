/* daemon to interface the CSIMC network to client processes.
 *
 * The terms "read" and "write" are used wrt the tty/network connection.
 *
 * Connection management:
 *   advertise presence on current host at a certain TCP/IP port.
 *   create 1 socket fd per client, each connecting one host/node pair.
 *   basically just pass data to/from each fd/node pair.
 *   CSIMCD_INTR from a client fd causes sending its node PT_INTR.
 *   EOF from a client fd causes sending its node KILL.
 *   opens FOR_REBOOT broadcasts PT_REBOOT to all nodes and closes all clients.
 *   anything but PT_SHELL/ACK from a node: send message to fd then close.
 *   also listen for special LOGADR packets and log those.
 *   if we die for any reason we issue a network-wide reboot.
 *
 * The topology is a token ring. We allow clients (host processes) to connect
 * to nodes via the csi_* api, which implements a socket/server mechanism to
 * contact us. Being a token ring, only one node may transmit at a time. We
 * also serve as the token broker. The token is given in turn to each node
 * address (0..31) we have ever connected to. Then it is set to 32 which means
 * it is our turn to let our clients originate packets. We let each client
 * send up to one packet before we give up the token. Being token based,
 * all packets originating here are synchronous, so we can just wait around
 * for their ACK, making the code read more linearly. When a new connection
 * is made to us, we Ping the new target node to confirm it is alive hence we
 * only allow new connections while we have the token. Nodes can talk to us
 * (our clients) at any time though so we must always be listening to the LAN
 * tty connection.
 *
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "configfile.h"
#include "csimc.h"
#include "running.h"
#include "strops.h"
#include "telenv.h"

#define SPEED B38400 /* cflag for tty speed */
#define MAXV 5       /* max verbose */
#define SOPWAIT 50   /* socket open wait time, secs */

#define TOKWT 5000 /* ms to wait for token back */

typedef struct
{
    int inuse : 1;  /* this info cell is in use */
    int cfdset : 1; /* cfd is in clset */
    int cfd;        /* client fd, if cfdset */
    int toaddr;     /* node address */
    OpenWhy why;    /* goal of connect */
} CInfo;

static void usage(char *me);
static void initCfg(void);
static int selectI(int n, fd_set *rp, fd_set *wp, fd_set *xp, struct timeval *tp);
static size_t readI(int fd, void *buf, size_t n);
static size_t writeI(int fd, const void *buf, size_t n);
static void openTTY(void);
static void announce(void);
static void mainLoop(void);
static void initPty(void);
static void openPty(char pty[], int addr, int baud);
static void reopenPty(int cfd);
static void advanceToken(void);
static void checkClients(void);
static void wait4TokenBack(void);
static void newClient();
static void newShell(CInfo *cip);
static void newReboot(CInfo *cip);
static void newBoot(CInfo *cip);
static void newSerial(CInfo *cip, int baud);
static int sendConfirmPing(CInfo *cip);
static int readLANpacket(char *what, int nto, int from);
static void rpktDispatch(void);

static void initCInfo(void);
static void sendAck(void);
static int sendXpkt(void);
static void buildCtrlPkt(int from, int to, PktType t);
static void sendPkt(Byte pkt[], int retry);
static void sendCurToken(void);
static int chkSum(Byte p[], int n);
static void clientMsg(int fd);
static int buildShellXPkt(int fd);
static int buildSerialXPkt(int fd);
static int buildBootXPkt(int fd);
static void closecfd(int cfd);
static void breakAllConnections(void);
static void breakConnections(int to);
static void dump(Byte *p, int n);
static int pktSize(Byte pkt[]);
static void logAddr(int fr);
static char *p2tstr(Pkt *pktp);
static void onVerboseSig(int dummy);
static void onExit(void);
static void onBye(int signo);
static int sendBaud(int cfd, int baud);
static char *why2str(OpenWhy why);
static CInfo *newCInfo(void);
static CInfo *CFD2CIP(int fd);

static char tty_def[30] = "/dev/ttyS0";             /* default tty onto network */
static char *tty = tty_def;                         /* tty we actually use */
static int port = CSIMCPORT;                        /* default IP port */
static char cfg_def[] = "archive/config/csimc.cfg"; /* default config file */
static char *cfg = cfg_def;                         /* config file we actually use */

static fd_set clset;            /* each client fd. host addr = fd + MAXNA */
static int maxclset = -1;       /* largest fd set in clset, -1 if empty */
static int listenfd;            /* universal listening post */
static int ttyfd;               /* tty fd once open */
static Byte rpkt[PMXLEN];       /* packet being received from CSIMC network */
static int rpktlen;             /* bytes in packet received so far */
static Byte rseq[NADDR][NADDR]; /* seq of last rx packet acked, [fr][to] */
static Byte xpkt[PMXLEN];       /* packet being transmitted to CSIMC network */
static Byte xseq[NADDR];        /* sequence for next tx packet, per net addr. */
static int verbose;             /* higher to log more details, up to MAXV */
static int mflag;               /* do not lock.. allow multiple instances */
static char livenodes[NNODES];  /* set as discover each node */
static int curtoken = BROKTOK;  /* current token */

/* connection info and handle conversions.
 * N.B. host address is index into cinfo[] biased by NNODES.
 */
static CInfo cinfo[NHOSTS];                /* connection info */
#define HA2CIP(ha) (&cinfo[(ha)-NNODES])   /* host addr to CInfo* */
#define HA2CFD(ha) (HA2CIP(ha)->cfd)       /* host addr to client fd */
#define CIP2HA(cip) ((cip)-cinfo + NNODES) /* CInfo* to host addr */
#define CFD2HA(fd) (CIP2HA(CFD2CIP(fd)))   /* client fd to host addr */

#define isOurToken() (curtoken == BROKTOK)

/* conjure the next sequence to be used for a packet to the given node */
#define XSEQ(addr) (((++xseq[addr]) << PSQ_SHIFT) & PSQ_MASK)

int main(int ac, char *av[])
{
    char *me = basenm(av[0]);

    /* check args */
    while ((--ac > 0) && ((*++av)[0] == '-'))
    {
        char *s;
        for (s = av[0] + 1; *s != '\0'; s++)
            switch (*s)
            {
            case 'c':
                if (ac < 2)
                    usage(me);
                cfg = *++av;
                ac--;
                break;
            case 'i':
                if (ac < 2)
                    usage(me);
                port = atoi(*++av);
                ac--;
                break;
            case 'm':
                mflag++;
                break;
            case 't':
                if (ac < 2)
                    usage(me);
                tty = *++av;
                ac--;
                break;
            case 'v':
                verbose++;
                break;
            default:
                usage(me);
            }
    }

    /* shouldn't be any more args */
    if (ac > 0)
        usage(me);

    /* only ever one of us unless explicitly allowed */
    if (!mflag && lock_running(me) < 0)
    {
        daemonLog("%s: Already running", me);
        exit(0);
    }

    /* set log now to proper place */
    telOELog(me);

    /* a few signal issues */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, onVerboseSig);
    signal(SIGTERM, onBye);
    signal(SIGINT, onBye);
    signal(SIGQUIT, onBye);

    /* init defaults */
    initCfg();

    /* open tty, announce socket, init any pty's */
    openTTY();
    announce();
    initCInfo();
    initPty();

    /* infinite service loop */
    atexit(onExit);
    while (1)
        mainLoop();

    return (0);
}

static void usage(char *me)
{
    fprintf(stderr, "%s: [options]\n", me);
    fprintf(stderr, "Purpose: provide host app access to CSIMC network\n");
    fprintf(stderr, "$Revision: 1.2 $\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -c f    alternate config file <f>. default is %s\n", cfg_def);
    fprintf(stderr, " -i p    listen on port <p>; default is %d\n", CSIMCPORT);
    fprintf(stderr, " -m      allow multiple instances for multiple LANs\n");
    fprintf(stderr, " -t tty  alternate <tty>. default is %s\n", tty_def);
    fprintf(stderr, " -v      verbose; up to %d; SIGHUP also bumps\n", MAXV);
    fprintf(stderr, "           0: always show errors..\n");
    fprintf(stderr, "           1: plus basic actions..\n");
    fprintf(stderr, "           2: plus packet contents.. \n");
    fprintf(stderr, "           3: plus raw tty input.. \n");
    fprintf(stderr, "           4: plus tokens.. \n");
    fprintf(stderr, "           5: plus host traffic. \n");

    exit(1);
}

#if 0
static void
ptt(int on)
{
	int arg;

	arg = on ? TIOCM_RTS : 0;
	if (ioctl (ttyfd, TIOCMSET, &arg) < 0) {
	    daemonLog ("%s: TIOCMSET: %s\n", tty, strerror(errno));
	    exit (2);
	}
}
#endif

/* read config file, if any */
static void initCfg(void)
{
    read1CfgEntry(1, cfg, "TTY", CFG_STR, tty_def, sizeof(tty_def));
    read1CfgEntry(1, cfg, "PORT", CFG_INT, &port, 0);
}

/* read the config file and set up any serial entries.
 * must be done after ttyfd and listenfd are set so CFD<->NA macros work.
 */
static void initPty(void)
{
    int addr;

    if (verbose)
        daemonLog("Scanning %s for SERn entries\n", cfg);

    /* read the optional SERn entries */
    for (addr = 0; addr <= MAXNA; addr++)
    {
        char name[32], value[32];
        sprintf(name, "SER%d", addr);
        if (!read1CfgEntry(0, cfg, name, CFG_STR, value, sizeof(value)))
        {
            char pty[64];
            int baud;

            if (sscanf(value, "%s %d", pty, &baud) != 2)
            {
                daemonLog("%s: Bad entry %s=%s", cfg, name, value);
                exit(1);
            }

            openPty(pty, addr, baud);
        }
    }
}

/* open pty[], assign to serial port on toaddr @ baud, add the fd as a new
 * "client" for which we listen.
 * exit(1) if trouble.
 */
static void openPty(char pty[], int toaddr, int baud)
{
    CInfo *cip;
    int cfd = -1;
    int i;

    /* convert ttyMN to ptyMN and open */
    pty[strlen(pty) - 5] = 'p';

    /* try open several times.. other side might be slow to shut down if
     * we have been rebooted
     */
    for (i = 0; i < 5; i++)
    {
        cfd = open(pty, O_RDWR);
        if (cfd < 0)
            sleep(1);
        else
            break;
    }
    if (cfd < 0)
    {
        daemonLog("%s: %s\n", pty, strerror(errno));
        exit(1);
    }

    /* tried tcget/setattr to turn off stuff like echo.
     * setattr with c_cflag = 0 flat fails; c_i/l/oflag don't fail but
     * don't help the echo problem either.
     */

    /* add cfd as a "client" */
    cip = newCInfo();
    if (!cip)
    {
        daemonLog("Out of host addresses for pty.. sorry\n");
        exit(1);
    }

    FD_SET(cfd, &clset);
    if (cfd > maxclset)
        maxclset = cfd;
    cip->inuse = 1;
    cip->cfdset = 1;
    cip->cfd = cfd;
    cip->toaddr = toaddr;
    cip->why = FOR_SERIAL;
    rseq[toaddr][CFD2HA(cfd)] = -1;
    livenodes[toaddr] = 1; /* insure it gets a token */
    if (sendBaud(cfd, baud) < 0)
        exit(1);
    daemonLog("Connected host %d node %d FOR_SERIAL to %s @ %d baud\n", CFD2HA(cfd), toaddr, pty, baud);
}

/* pty on cfd was closed by client.. reopen and reuse cfd.
 * exit (1) if trouble.
 */
static void reopenPty(int cfd)
{
    int toaddr = CFD2CIP(cfd)->toaddr;
    char name[32], value[32];
    char pty[64];
    int baud;
    int ptyfd;

    if (verbose)
        daemonLog("Scanning %s for SERn entries\n", cfg);

    /* find SER<toaddr> */
    sprintf(name, "SER%d", toaddr);
    if (read1CfgEntry(0, cfg, name, CFG_STR, value, sizeof(value)) < 0)
    {
        daemonLog("%s: %d disappeared!\n", cfg, name);
        exit(1);
    }

    /* dig out pty and baud */
    if (sscanf(value, "%s %d", pty, &baud) != 2)
    {
        daemonLog("%s: Bad entry %s=%s", cfg, name, value);
        exit(1);
    }

    /* convert ttyMN to ptyMN and open */
    pty[strlen(pty) - 5] = 'p';
    close(cfd); /* close but keep cfd so we can dup to it */
    ptyfd = open(pty, O_RDWR);
    if (ptyfd < 0)
    {
        daemonLog("%s: %s\n", pty, strerror(errno));
        exit(1);
    }

    /* use same cfd */
    if (ptyfd != cfd)
    {
        if (dup2(ptyfd, cfd) < 0)
        {
            daemonLog("reopenPty dup2(%d, %d): %s", ptyfd, cfd, strerror(errno));
            exit(1);
        }
        close(ptyfd);
    }

    /* ok, should be able to continue with same CInfo and cfd client */
}

/* send baud rate value to given connection.
 * return 0 if ok else log problem and return -1.
 */
static int sendBaud(int cfd, int baud)
{
    int to = CFD2CIP(cfd)->toaddr;
    int fr = CFD2HA(cfd);

    xpkt[PB_SYNC] = PSYNC;
    xpkt[PB_TO] = to;
    xpkt[PB_FR] = fr;
    xpkt[PB_INFO] = PT_SERSETUP | XSEQ(to);
    xpkt[PB_COUNT] = 2;
    xpkt[PB_HCHK] = chkSum(xpkt, PB_NHCHK);
    xpkt[PB_DATA + 0] = baud >> 8; /* big endian word */
    xpkt[PB_DATA + 1] = baud;      /* big endian word */
    xpkt[PB_DCHK] = chkSum(&xpkt[PB_DATA], xpkt[PB_COUNT]);

    if (sendXpkt() < 0)
    {
        /* already closed and logged otherwise */
        daemonLog("Failed setting Baud to %d on %d\n", baud, to);
        return (-1);
    }
    return (0);
}

static void initCInfo(void)
{
    CInfo *cip;

    for (cip = cinfo; cip < &cinfo[NHOSTS]; cip++)
        cip->inuse = 0;
}

/* just like select(2) but retries if interrupted */
static int selectI(int n, fd_set *rp, fd_set *wp, fd_set *xp, struct timeval *tp)
{
    int s;

    while ((s = select(n, rp, wp, xp, tp)) < 0 && errno == EINTR)
        continue;
    return (s);
}

/* just like read(2) but retries if interrupted */
static size_t readI(int fd, void *buf, size_t n)
{
    size_t s;

    while ((s = read(fd, buf, n)) < 0 && errno == EINTR)
        continue;
    return (s);
}

/* just like write(2) but retries if interrupted */
static size_t writeI(int fd, const void *buf, size_t n)
{
    size_t s;

    while ((s = write(fd, buf, n)) < 0 && errno == EINTR)
        continue;
    return (s);
}

/* open tty and set ttyfd, else bail. */
static void openTTY()
{
    struct termios tio;

    ttyfd = open(tty, O_RDWR | O_NONBLOCK);
    if (ttyfd < 0)
    {
        daemonLog("open(%s): %s\n", tty, strerror(errno));
        exit(1);
    }
    fcntl(ttyfd, F_SETFL, 0); /* nonblock back off */

    memset(&tio, 0, sizeof(tio));
    tio.c_cflag = CS8 | CREAD | CLOCAL;
    tio.c_iflag = IGNPAR | IGNBRK;
    tio.c_cc[VMIN] = 0;            /* start timer when call read() */
    tio.c_cc[VTIME] = ACKWT / 100; /* wait up to n 1/10ths seconds */
    cfsetospeed(&tio, SPEED);
    cfsetispeed(&tio, SPEED);
    if (tcsetattr(ttyfd, TCSANOW, &tio) < 0)
    {
        daemonLog("tcsetattr(%s): %s\n", tty, strerror(errno));
        exit(1);
    }

    daemonLog("CSIMC network %s on fd %d\n", tty, ttyfd);
}

/* create listenfd, on this host at the given port */
static void announce()
{
    int i;

    /* if we were killed, socket can remain busy so keep trying */
    for (i = 0; i < SOPWAIT; i++)
    {
        if ((listenfd = csimcd_slisten(port)) < 0 && errno == EADDRINUSE)
            sleep(1);
        else
            break;
    }

    if (listenfd < 0)
    {
        daemonLog("listen(%d): %s\n", port, strerror(errno));
        exit(1);
    }

    daemonLog("Listening for CSIMC clients on port %d with fd %d\n", port, listenfd);
}

/* one of an infinite loop handling connections and traffic.
 * main loop:
 *   increment token
 *   if we have the token
 *     for each new client wanting to connect
 *        check that target has indeed been booted
 *        send PING; wait for ACK; repeat as required; handle
 *     for each existing client wanting to send
 *        read one packet's worth
 *        send packet; wait for ACK; repeat as required; handle
 *   else
 *     send token to new owner
 *     do
 *        listen for packets, dispatch to clients, ack
 *     until we get token back or time out
 */
static void mainLoop()
{
    advanceToken();
    if (isOurToken())
    {
        if (verbose > 3)
            daemonLog("Token is ours\n");
        checkClients();

        // HACK: give the network 10ms to propagate sent
        // data downthe chain.  If we poll again too
        // quickly then we never receive a response.
        usleep(10000);
    }
    else
    {
        sendCurToken();
        wait4TokenBack();
    }
}

/* set curtoken to next active address in ring,
 * or addr2tok(NNODES) if we're next.
 */
static void advanceToken(void)
{
    int a = tok2addr(curtoken);

    do
    {
        a = (a + 1) % (NNODES + 1); /* yes .. NNODES means us */
    } while (a != NNODES && !livenodes[a]);

    curtoken = addr2tok(a);
}

/* for each new client wanting to connect
 *   check that target has indeed been booted
 *   send PING; wait for ACK; repeat as required; handle
 * for each existing client wanting to send
 *   read one packet's worth
 *   send packet; wait for ACK; repeat as required; handle
 */
static void checkClients(void)
{
    struct timeval tv;
    fd_set fs;
    int maxfs;
    int fd;
    int n;

    /* make copy so we can add listenfd */
    fs = clset;
    maxfs = maxclset;
    FD_SET(listenfd, &fs);
    if (listenfd > maxclset)
        maxfs = listenfd;

    /* poll .. must get back to passing token unless no clients now */
    tv.tv_sec = 0;
    tv.tv_usec = maxclset < 0 ? 10000 : 0;

    /* the truth is out there */
    n = selectI(maxfs + 1, &fs, NULL, NULL, &tv);
    if (n < 0)
    {
        daemonLog("select(%d): %s\n", maxfs, strerror(errno));
        return;
    }

    /* handle the actions -- some may change clset */
    for (fd = 0; n > 0 && fd <= maxfs; fd++)
    {
        if (FD_ISSET(fd, &fs))
        {
            if (fd == listenfd)
                newClient();
            else if (FD_ISSET(fd, &clset))
                clientMsg(fd);
            --n;
        }
    }
}

/* do
 *   listen for packets from LAN, dispatch to clients, ack
 * until we get token back or time out
 */
static void wait4TokenBack(void)
{
    while (!readLANpacket("BROKTOK back", TOKWT / ACKWT, tok2addr(curtoken)))
        rpktDispatch();
}

/* a new client just arrived on listenfd.
 * byte 1 should be the node address they want.
 * byte 2 should be one of OpenWhy.
 * byte 3 can be any extra info.
 * unless FOR_REBOOT, ping the node to confirm before committing.
 */
static void newClient()
{
    Byte preamble[3];
    CInfo *cip;
    OpenWhy why;
    int newcfd;
    Byte to;
    int n;

    /* accept the new connection */
    newcfd = csimcd_saccept(listenfd);
    if (newcfd < 0)
    {
        daemonLog("accept(): %s\n", strerror(errno));
        exit(1);
    }

    /* read preamble for address, why and extra */
    n = readI(newcfd, preamble, sizeof(preamble));
    switch (n)
    {
    case -1:
        daemonLog("1st read from client: %s\n", strerror(errno));
        (void)close(newcfd);
        return;
    case 0:
        daemonLog("Yes, I'm here.\n");
        (void)close(newcfd);
        return;
    case 1: /* FALLTHRU */
    case 2:
        daemonLog("Preamble is short from client connecting to %d\n", preamble[0]);
        (void)close(newcfd);
        return;
    default:
        break;
    }

    /* first byte of preamble is target address */
    to = preamble[0];
    if (to > MAXNA && to != BRDCA)
    {
        daemonLog("Bogus client toaddr: %d\n", to);
        (void)close(newcfd);
        return;
    }

    /* second is reason */
    why = (OpenWhy)preamble[1];

    /* assign to new CInfo.
     * N.B. implicitly assigns host address
     */
    cip = newCInfo();
    if (!cip)
    {
        daemonLog("Out of host addresses.. sorry\n");
        (void)close(newcfd);
        return;
    }
    cip->inuse = 1;
    cip->cfdset = 0;
    cip->cfd = newcfd;
    cip->toaddr = to;
    cip->why = why;

    switch (why)
    {
    case FOR_SHELL:
        newShell(cip);
        break;
    case FOR_REBOOT:
        newReboot(cip);
        break;
    case FOR_BOOT:
        newBoot(cip);
        break;
    case FOR_SERIAL:
        newSerial(cip, 300 * preamble[2]); /* 3rd is baud/300 */
        break;
    default:
        daemonLog("Unknown preamble 'Why' to %d: %d\n", to, why);
        closecfd(newcfd);
        break;
    }
}

/* create a new Shell connection */
static void newShell(CInfo *cip)
{
    int newcfd = cip->cfd;
    int ha = CIP2HA(cip);
    int to = cip->toaddr;

    if (verbose)
        daemonLog("New Shell client request: fd %d host %d node %d\n", newcfd, ha, to);

    if (sendConfirmPing(cip) < 0)
        return; /* already closed + logged */
    if (verbose)
        daemonLog("New Shell client accepted: fd %d host %d node %d\n", newcfd, ha, to);
}

/* send a PT_REBOOT to all nodes */
static void newReboot(CInfo *cip)
{
    int fd = cip->cfd;
    int ha = CIP2HA(cip);
    char hachar;

    if (verbose)
        daemonLog("New ReBoot client request: fd %d host %d\n", fd, ha);

    buildCtrlPkt(ha, BRDCA, PT_REBOOT);

    hachar = ha;
    if (writeI(fd, &hachar, 1) < 0)
    {
        daemonLog("Host %d on fd %d asking for REBOOT disappeared! %s\n", ha, fd, strerror(errno));
        (void)closecfd(fd);
        return;
    }

    /* send */
    daemonLog("Broadcasting REBOOT\n");
    sendPkt(xpkt, 0);      /* get no ACKs from BRDCA */
    sendPkt(xpkt, 0);      /* repeat for good measure */
    breakAllConnections(); /* close all client connections */
    initPty();             /* rescan for new SER entries, if any */
}

/* create a new BOOT connection */
static void newBoot(CInfo *cip)
{
    int newcfd = cip->cfd;
    int ha = CIP2HA(cip);
    int to = cip->toaddr;
    CInfo *tcip;

    if (verbose)
        daemonLog("New Boot client request: fd %d host %d node %d\n", newcfd, ha, to);

    /* lock out if in use */
    for (tcip = cinfo; tcip < &cinfo[NHOSTS]; tcip++)
    {
        if (tcip != cip && tcip->inuse && tcip->toaddr == to)
        {
            if (verbose)
                daemonLog("Node %d is in use.. new boot locked out\n", to);
            closecfd(newcfd);
            return;
        }
    }

    /* if confirm alive we are go */
    if (sendConfirmPing(cip) < 0)
        return; /* already closed + logged */
    if (verbose)
        daemonLog("New Boot client accepted: fd %d host %d node %d\n", newcfd, ha, to);
}

/* create a new serial connection */
static void newSerial(CInfo *cip, int baud)
{
    int newcfd = cip->cfd;
    int ha = CIP2HA(cip);
    int to = cip->toaddr;
    CInfo *lcip;

    if (verbose)
        daemonLog("New Serial client request: fd %d host %d node %d\n", newcfd, ha, to);

    /* sanity check for >1 using serial port at one time */
    for (lcip = cinfo; lcip < &cinfo[NHOSTS]; lcip++)
    {
        if (lcip != cip && lcip->inuse && lcip->toaddr == to && lcip->why == FOR_SERIAL)
        {
            daemonLog("Serial port on Node %d already in use by Host %d\n", to, CIP2HA(lcip));
            closecfd(newcfd);
            return;
        }
    }

    /* set baud */
    if (sendBaud(newcfd, baud) < 0)
        return; /* already closed + logged */

    /* confirm real and tell client */
    if (sendConfirmPing(cip) < 0)
        return; /* already closed + logged */

    if (verbose)
        daemonLog("New Serial client accepted: fd %d host %d node %d\n", newcfd, ha, to);
}

/* send a PING to cip's to from ha.
 * if ok add to clset, tell client and add to livenodes[].
 * return 0 if ok, else -1.
 */
static int sendConfirmPing(CInfo *cip)
{
    int newcfd = cip->cfd;
    int ha = CIP2HA(cip);
    int to = cip->toaddr;
    char hachar;

    rseq[to][ha] = -1;
    buildCtrlPkt(ha, to, PT_PING);
    if (sendXpkt() < 0)
        return (-1); /* already closed + logged */

    /* PINGed ok .. finish init */

    FD_SET(newcfd, &clset);
    if (newcfd > maxclset)
        maxclset = newcfd;
    cip->cfdset = 1;

    hachar = ha;
    if (writeI(newcfd, &hachar, 1) < 0)
    {
        daemonLog("New host %d on fd %d for node %d disappeared! %s\n", ha, newcfd, to, strerror(errno));
        (void)closecfd(newcfd);
        return (-1);
    }

    livenodes[to] = 1;

    return (0);
}

/* return 1 if buf[0..1] is a token packet, else 0 */
static int isTokPkt(Byte buf[2])
{
    return (buf[0] == PSYNC && (buf[1] == BROKTOK || ISNTOK(buf[1])));
}

/* read the next char from the lan.
 * return 0 if ok else -1 if time out after nto tries
 */
static int readLANchar(int nto, Byte *bp)
{
    static Byte inbuf[100];
    static int lastsent, ninbuf;

    /* pull from inbuf first */
    if (lastsent < ninbuf)
    {
        *bp = inbuf[lastsent++];
        return (0);
    }

    /* then try to read more, up to nto times */
    while (nto-- > 0)
    {
        int n = readI(ttyfd, inbuf, sizeof(inbuf));
        if (n < 0)
        {
            daemonLog("Read(%s): %s\n", tty, strerror(errno));
            exit(1);
        }
        if (n > 0)
        {
            /* suppres token traffic at level 2 */
            if (verbose > 3 || (verbose > 2 && !isTokPkt(inbuf)))
            {
                daemonLog("Read %d from %s.. rpktlen now %d\n", n, tty, rpktlen);
                dump(inbuf, n);
            }
            ninbuf = n;
            lastsent = 0;
            return (readLANchar(nto, bp));
        }
    }

    /* nothing */
    return (-1);
}

/* read from ttyfd into rpkt until we have a packet or see BROKTOK or timeout.
 * nto is number of ACKWT periods to wait before considering it a timeout.
 * "what" is a string of what we are hoping to read for printing and fr is
 *    the node address from which we anticipate a packet, for verbose.
 * return 0 if read normal packet, else -1 if anything else.
 */
static int readLANpacket(char *what, int nto, int fr)
{
    Byte d;
    int n;

    /* new packet */
    rpktlen = 0;

    /* repeat until know what is going on */
    while (1)
    {
        if (readLANchar(nto, &d) < 0)
        {
            daemonLog("Time out waiting for %s from %d\n", what, fr);
            return (-1);
        }

        if (d == PSYNC)
        {
            /* saw SYNC -- always start over */
            rpkt[PB_SYNC] = PSYNC;
            rpktlen = 1;
        }
        else
        {
            /* handle bytes subsequent to known-good SYNC */
            switch (rpktlen)
            {
            case 0: /* just garbage */
                break;
            case 1: /* To or token */
                if (d == BROKTOK)
                {
                    if (verbose > 3)
                        daemonLog("Received BROKTOK back from %d\n", fr);
                    return (-1);
                }
                if (ISNTOK(d))
                {
                    rpktlen = 0; /* start over */
                }
                else
                {
                    rpkt[PB_TO] = d; /* continue with normal pkt */
                    rpktlen = 2;
                }
                break;
            case 2: /* From */
                rpkt[PB_FR] = d;
                rpktlen = 3;
                break;
            case 3: /* Info */
                rpkt[PB_INFO] = d;
                rpktlen = 4;
                break;
            case 4: /* Data Count */
                if (d > PMXDAT)
                {
                    daemonLog("Preposterous data count: %d\n", d);
                    dump(rpkt, 5);
                    rpktlen = 0; /* must be illegal */
                }
                else
                {
                    rpkt[PB_COUNT] = d;
                    rpktlen = 5;
                }
                break;
            case 5:                /* Header Checksum */
                rpkt[PB_HCHK] = d; /* proposed header checksum */
                rpktlen = 6;
                n = chkSum(rpkt, PB_NHCHK);
                if (n == d)
                {                            /* if good checksum */
                    if (rpkt[PB_COUNT] == 0) /*   if control packet */
                        return (0);          /*     good to go */
                }
                else
                {
                    daemonLog("Bad header chksum: 0x%02x vs 0x%02x\n", n, rpkt[PB_HCHK]);
                    dump(rpkt, rpktlen);
                    rpktlen = 0; /* start over */
                }
                break;
            case 6:                /* Data Checksum */
                rpkt[PB_DCHK] = d; /* claimed data checksum */
                rpktlen = 7;       /* now collect data */
                break;
            default:                 /* gathering data */
                rpkt[rpktlen++] = d; /* another byte of data */
                if (rpktlen >= rpkt[PB_COUNT] + PB_DATA)
                { /* if have all */
                    n = chkSum(&rpkt[PB_DATA], rpkt[PB_COUNT]);
                    if (n == rpkt[PB_DCHK]) /* if good data */
                        return (0);         /*   good to go */
                    else
                    {
                        daemonLog("Bad data chksum from %d: 0x%02x vs 0x%02x\n", rpkt[PB_FR], n, rpkt[PB_DCHK]);
                        dump(rpkt, rpktlen);
                        rpktlen = 0; /* start over */
                    }
                }
                break;
            }
        }
    }
}

/* rpkt from tty checksums ok .. dispatch to client.
 * N.B. this is *not* for cracking an ACK.
 */
static void rpktDispatch()
{
    int haddr = rpkt[PB_TO];
    int netaddr = rpkt[PB_FR];
    int seq = rpkt[PB_INFO] & PSQ_MASK;
    int t = rpkt[PB_INFO] & PT_MASK;
    int opencfd;
    int cfd;

    /* log */
    if (verbose)
    {
        daemonLog("Saw %s packet: %d data from %d to %d seq 0x%x\n", p2tstr((Pkt *)rpkt), rpkt[PB_COUNT], netaddr,
                  haddr, rpkt[PB_INFO] >> PSQ_SHIFT);
        if (verbose > 1)
            dump(rpkt, pktSize(rpkt));
    }

    /* check whether for a host at all */
    if (haddr <= MAXNA)
        return;

    /* now that we know haddr is reasonable, convert to fd */
    cfd = HA2CFD(haddr);
    opencfd = FD_ISSET(cfd, &clset);

    /* looks good enough to dispatch, now things depend on type */
    switch (t)
    {
    case PT_SHELL:
        if (haddr == LOGADR)
        {
            /* special logging host addr */
            logAddr(netaddr);
            sendAck();
        }
        else if (!opencfd)
        {
            daemonLog("Received %s packet from %d to inactive %d\n", p2tstr((Pkt *)rpkt), netaddr, haddr);
            dump(rpkt, pktSize(rpkt));
            /* "response" is to deprive sender of ACK */
        }
        else if (seq == rseq[netaddr][haddr])
        {
            /* dup */
            daemonLog("Dup %s from %d to %d seq 0x%x\n", p2tstr((Pkt *)rpkt), netaddr, haddr, seq >> PSQ_SHIFT);
            dump(rpkt, pktSize(rpkt));
            sendAck();
        }
        else
        {
            /* new packet. woopie! */
            if (verbose > 2)
            {
                daemonLog("Writing %d Shell bytes to host %d on fd %d\n", rpkt[PB_COUNT], haddr, cfd);
                if (verbose > 4)
                    dump(rpkt + PB_DATA, rpkt[PB_COUNT]);
            }
            if (writeI(cfd, rpkt + PB_DATA, rpkt[PB_COUNT]) < 0)
            {
                daemonLog("Host %d socket write error: %s\n", haddr, strerror(errno));
                closecfd(cfd);
                /* "response" is to deprive sender of ACK */
            }
            else
            {
                sendAck();
            }
        }
        break;

    case PT_SERDATA:
        /* received data from remote serial link */
        if (!opencfd)
        {
            daemonLog("Stray PT_SERDATA from node %d to host %d\n", netaddr, haddr);
            dump(rpkt + PB_DATA, rpkt[PB_COUNT]);
            /* "response" is to deprive sender of ACK */
        }
        else if (write(cfd, &rpkt[PB_DATA], rpkt[PB_COUNT]) < 0)
        {
            /* TODO: explode escapes */
            daemonLog("Host %d pty socket write error: %s\n", haddr, strerror(errno));
            closecfd(cfd);
            /* "response" is to deprive sender of ACK */
        }
        else
        {
            sendAck();
        }
        break;

    default:
        /* just log and close client, any any */
        daemonLog("%s is unsupported from node %d to host %d\n", p2tstr((Pkt *)rpkt), netaddr, haddr);
        if (opencfd)
            closecfd(cfd);
        break;
    }
}

/* client, connected on cfd, wants to send something.
 * build xpkt from cfd and send it, wait for ACK.
 */
static void clientMsg(int cfd)
{
    switch (CFD2CIP(cfd)->why)
    {
    case FOR_BOOT:
        if (buildBootXPkt(cfd) < 0)
            return;
        break;
    case FOR_SHELL:
        if (buildShellXPkt(cfd) < 0)
            return;
        break;
    case FOR_SERIAL:
        if (buildSerialXPkt(cfd) < 0)
            return;
        break;
    default:
        daemonLog("Bogus why field %d from %d\n", CFD2CIP(cfd)->why, CFD2HA(cfd));
        return;
    }

    (void)sendXpkt(); /* closes if trouble and logs */
}

/* read client cfd with shell chat and create xpkt.
 * return 0 if ok, else -1.
 */
static int buildShellXPkt(int cfd)
{
    int haddr = CFD2HA(cfd);
    int toaddr = CFD2CIP(cfd)->toaddr;
    int newseq = XSEQ(toaddr);
    char *dp = (char *)&xpkt[PB_DATA];
    int n;

    /* this much of new xpkt is always the same */
    xpkt[PB_SYNC] = PSYNC;
    xpkt[PB_TO] = toaddr;
    xpkt[PB_FR] = haddr;

    /* read client's data.
     * N.B. we can't support binary data because of a few flags chars.
     */
    n = readI(cfd, dp, PMXDAT);
    if (n <= 0)
    {
        if (n < 0)
            daemonLog("Host %d socket %d read error: %s. KILLing node %d\n", haddr, cfd, strerror(errno), toaddr);
        else if (verbose)
            daemonLog("EOF from host %d.. sending KILL to node %d\n", haddr, toaddr);
        closecfd(cfd);
        xpkt[PB_INFO] = PT_KILL | newseq;
        xpkt[PB_COUNT] = 0;
    }
    else if (memchr(dp, CSIMCD_INTR, n))
    {
        /* client knows we and CSIMC are discarding all prior chars
         * and will wait until we send it 1 char for sync.
         */
        if (verbose)
            daemonLog("INTR from host %d.. sending INTR to node %d\n", haddr, toaddr);
        xpkt[PB_INFO] = PT_INTR | newseq;
        xpkt[PB_COUNT] = 0;
    }
    else
    {
        /* routine data */
        if (verbose > 2)
        {
            daemonLog("Read %d bytes from %d to %d\n", n, haddr, toaddr);
            if (verbose > 4)
                dump(dp, n);
        }
        xpkt[PB_INFO] = PT_SHELL | newseq;
        xpkt[PB_COUNT] = n;
        xpkt[PB_DCHK] = chkSum(dp, n);
    }
    xpkt[PB_HCHK] = chkSum(xpkt, PB_NHCHK);

    return (0);
}

/* read client cfd with raw boot code and create xpkt.
 * return 0 if ok to send xpkt, else -1 when finished.
 */
static int buildBootXPkt(int cfd)
{
    int haddr = CFD2HA(cfd);
    int toaddr = CFD2CIP(cfd)->toaddr;
    int newseq = XSEQ(toaddr);
    char *dp = (char *)&xpkt[PB_DATA];
    int n;

    /* this much of a boot xpkt is always the same */
    xpkt[PB_SYNC] = PSYNC;
    xpkt[PB_TO] = toaddr;
    xpkt[PB_FR] = haddr;
    xpkt[PB_INFO] = PT_BOOTREC | newseq;

    /* read one boot packet.
     * client won't send another until we tell ok.
     */
    if ((n = readI(cfd, dp, PMXDAT)) <= 0)
    {
        if (n < 0)
            daemonLog("Host %d booting %d: socket read error: %s\n", haddr, toaddr, strerror(errno));
        else
        {
            if (verbose)
                daemonLog("EOF from host %d booting %d\n", haddr, toaddr);
        }

        /* finished. reset sequence and close */
        closecfd(cfd);
        return (-1);
    }
    if (n > PMXDAT)
    {
        daemonLog("%d-byte record from host %d booting %d (max is %d)\n", n, haddr, toaddr, PMXDAT);
        closecfd(cfd);
        return (-1);
    }
    if (verbose > 2)
    {
        daemonLog("Read %d BOOTREC bytes from host %d\n", n, haddr);
        if (verbose > 4)
            dump(dp, n);
    }

    /* routine boot records */
    xpkt[PB_COUNT] = n;
    xpkt[PB_DCHK] = chkSum(dp, n);
    xpkt[PB_HCHK] = chkSum(xpkt, PB_NHCHK);
    return (0);
}

/* read client cfd with serial data and create xpkt.
 * return 0 if ok, else -1.
 * TODO: escape??
 */
static int buildSerialXPkt(int cfd)
{
    int haddr = CFD2HA(cfd);
    int toaddr = CFD2CIP(cfd)->toaddr;
    int newseq = XSEQ(toaddr);
    char *dp = (char *)&xpkt[PB_DATA];
    int n;

    /* this much of new xpkt is always the same */
    xpkt[PB_SYNC] = PSYNC;
    xpkt[PB_TO] = toaddr;
    xpkt[PB_FR] = haddr;

    /* read client's data.
     */
    n = readI(cfd, dp, PMXDAT);
    if (n < 0)
    {
        if (errno == EIO)
        {
            /* client closed.. we just reopen */
            if (verbose)
                daemonLog("EOF from pty on node %d.. reopening", toaddr);
            reopenPty(cfd);
            return (-1);
        }
        daemonLog("Host %d pty error: %s\n", haddr, strerror(errno));
        xpkt[PB_INFO] = PT_KILL | newseq;
        n = 0;
        closecfd(cfd);
    }
    else if (n == 0)
    {
        if (verbose)
            daemonLog("EOF from host %d pty\n", haddr, toaddr);
        xpkt[PB_INFO] = PT_KILL | newseq;
        closecfd(cfd);
    }
    else
        xpkt[PB_INFO] = PT_SERDATA | newseq;

    if (verbose > 2)
    {
        daemonLog("Read %d serial bytes from %d to %d\n", n, haddr, toaddr);
        if (verbose > 4)
            dump(dp, n);
    }
    xpkt[PB_COUNT] = n;
    xpkt[PB_HCHK] = chkSum(xpkt, PB_NHCHK);
    xpkt[PB_DCHK] = chkSum(dp, n);

    return (0);
}

/* compute check sum on the given array */
int chkSum(Byte p[], int n)
{
    Word sum;

    for (sum = 0; n > 0; --n)
        sum += *p++;
    while (sum > 255)
        sum = (sum & 0xff) + (sum >> 8);
    if (sum == PSYNC)
        sum = 1;
    return (sum);
}

/* send an ACK packet for what is in rpkt.
 * record sequence in rseq[] and start timer.
 * we don't expect _this_ to be acked.
 */
static void sendAck()
{
    Byte apkt[PB_HSZ];
    Byte seq = (rpkt[PB_INFO] & PSQ_MASK);
    Byte fr = rpkt[PB_FR];
    Byte to = rpkt[PB_TO];

    apkt[PB_SYNC] = PSYNC;
    apkt[PB_TO] = fr;
    apkt[PB_FR] = to;
    apkt[PB_INFO] = PT_ACK | seq;
    apkt[PB_COUNT] = 0;
    apkt[PB_HCHK] = chkSum(apkt, PB_NHCHK);

    rseq[fr][to] = seq;

    sendPkt(apkt, 0);
}

/* fill xpkt with one of the basic control packets that have no data */
static void buildCtrlPkt(int from, int to, PktType t)
{
    xpkt[PB_SYNC] = PSYNC;
    xpkt[PB_TO] = to;
    xpkt[PB_FR] = from;
    xpkt[PB_INFO] = t | (to == BRDCA ? 0 : XSEQ(to));
    xpkt[PB_COUNT] = 0;
    xpkt[PB_HCHK] = chkSum(xpkt, PB_NHCHK);
}

/* return 0 if rpkt is an ACK for xpkt, else -1 */
static int ack4xpkt(void)
{
    int netaddr = rpkt[PB_FR];
    int haddr = rpkt[PB_TO];
    int seq = rpkt[PB_INFO] & PSQ_MASK;
    int t = rpkt[PB_INFO] & PT_MASK;

    if (t != PT_ACK)
    {
        daemonLog("Unexpected %s from %d to %d\n", p2tstr((Pkt *)rpkt), netaddr, haddr);
    }
    else if (netaddr == xpkt[PB_TO] && seq == (xpkt[PB_INFO] & PSQ_MASK))
    {
        /* yes, rpkt is ACK for xpkt.
         * a few ACKs inform their clients.
         */

        if (verbose)
            daemonLog("Saw ACK packet: from %d to %d seq 0x%x\n", netaddr, haddr, seq >> PSQ_SHIFT);

        /* if ack for BOOTREC from boot client, inform size as progress.
         * N.B. first ack of FOR_BOOT is just the Ping confirm.
         */
        if (HA2CIP(haddr)->why == FOR_BOOT && (xpkt[PB_INFO] & PT_MASK) == PT_BOOTREC)
        {
            int cfd = HA2CFD(haddr);
            if (verbose)
                daemonLog("Telling host %d that its %d BOOTREC bytes were ACKed\n", haddr, xpkt[PB_COUNT]);
            if (writeI(cfd, &xpkt[PB_COUNT], 1) < 0)
            {
                daemonLog("Boot client %d for %d disappeared! %s\n", haddr, netaddr, strerror(errno));
                closecfd(cfd);
            }
        }

        /* if ack for INTR from SHELL client, send byte to sync. */
        if (HA2CIP(haddr)->why == FOR_SHELL && (xpkt[PB_INFO] & PT_MASK) == PT_INTR)
        {
            int cfd = HA2CFD(haddr);
            char zero = 0;
            if (verbose)
                daemonLog("Telling host %d that its PT_INTR was ACKed\n", haddr);
            if (writeI(cfd, &zero, 1) < 0)
            {
                daemonLog("Shell client %d for %d disappeared! %s\n", haddr, netaddr, strerror(errno));
                closecfd(cfd);
            }
        }

        /* rpkt is indeed an ack for xpkt */
        return (0);
    }

    /* rpkt is not an ack for xpkt */
    return (-1);
}

/* wait for packet from LAN.
 * return 0 if ack for xpkt, else -1.
 */
static int wait4ACK(void)
{
    return (!readLANpacket("ACK", 1, xpkt[PB_TO]) && !ack4xpkt() ? 0 : -1);
}

/* send xpkt and wait for ACK, retrying as necessary.
 * if time out, break all its client connections and no longer live.
 * return 0 if ok else -1.
 */
static int sendXpkt(void)
{
    int to = xpkt[PB_TO];
    int i;

    /* send and retry as necessary */
    for (i = 0; i <= MAXRTY; i++)
    {
        sendPkt(xpkt, i);
        if (wait4ACK() == 0)
            return (0);
    }

    /* sorry */
    daemonLog("Restarting node %d after %d tries.\n", to, MAXRTY + 1);
    breakConnections(to);
    livenodes[to] = 0;
    buildCtrlPkt(MAXNA + 1, to, PT_REBOOT);
    sendPkt(xpkt, 0);
    sendPkt(xpkt, 0); /* no ACK so repeat for good measure */
    return (-1);
}

/* return total Bytes in the given packet */
int pktSize(Byte pkt[])
{
    /* 1 for PB_DCHK too when finite COUNT */
    return (pkt[PB_COUNT] ? PB_HSZ + 1 + pkt[PB_COUNT] : PB_HSZ);
}

/* send na bytes to tty.
 * exit if fail.
 */
static void sendTTY(Byte a[], int na)
{
    int s;

    /* send array */
    s = writeI(ttyfd, a, na);
    if (s != na)
    {
        if (s < 0)
            daemonLog("Write(%s): %s\n", tty, strerror(errno));
        else
            daemonLog("Write(%s): short write: %d %d\n", tty, na, s);
        _exit(1);
    }
    else if (verbose > 3 || (verbose > 2 && !isTokPkt(a)))
    {
        daemonLog("Wrote %d to %s:\n", na, tty);
        if (verbose > 4)
            dump(a, na);
    }
}

/* send the given packet to the csimc network.
 */
void sendPkt(Byte pkt[], int try)
{
    int npkt = pktSize(pkt);

    if (verbose)
    {
        daemonLog("Sending %s packet: %d data from %d to %d seq 0x%x retry %d\n", p2tstr((Pkt *)pkt), pkt[PB_COUNT],
                  pkt[PB_FR], pkt[PB_TO], pkt[PB_INFO] >> PSQ_SHIFT, try);

        if (verbose > 1)
            dump(pkt, npkt);
    }

    sendTTY(pkt, npkt);
}

/* send curtoken onto the LAN.
 * this does not get ACKed
 */
static void sendCurToken()
{
    Byte tpkt[2];

    tpkt[0] = PSYNC;
    tpkt[1] = curtoken;

    if (verbose > 3)
        daemonLog("Sending token to node %d\n", tok2addr(curtoken));

    sendTTY(tpkt, 2);
}

/* close the client file descriptor cfd and associated bookkeeping */
static void closecfd(int cfd)
{
    CInfo *cip = CFD2CIP(cfd);

    /* sanity-check cfd */
    if (cip->cfd != cfd)
    {
        daemonLog("FD cross check failed: %d vs %d\n", cip->cfd, cfd);
        return;
    }

    if (verbose)
        daemonLog("Closing %s connection from %d to %d on fd %d\n", why2str(cip->why), CFD2HA(cfd), cip->toaddr, cfd);

    /* close real fd */
    (void)close(cfd);
    cip->inuse = 0;

    /* remove from clset, if claims to be in */
    if (cip->cfdset)
    {
        if (!FD_ISSET(cfd, &clset))
        {
            daemonLog("Request to Close bogus fd %d\n", cfd);
            return;
        }
        FD_CLR(cfd, &clset);
        if (maxclset == cfd)
        {
            while (--maxclset >= 0 && !FD_ISSET(maxclset, &clset))
                continue;
        }
    }
}

/* handle a packet sent to the universal logging address.
 */
static void logAddr(int fr)
{
    daemonLog("Log: Node %d: %.*s", fr, rpkt[PB_COUNT], rpkt + PB_DATA);
}

/* break ALL connections */
static void breakAllConnections(void)
{
    CInfo *cip;
    int i;

    for (cip = cinfo; cip < &cinfo[NHOSTS]; cip++)
        if (cip->inuse)
            closecfd(cip->cfd);

    /* nothing is alive now */
    for (i = 0; i < NNODES; i++)
        livenodes[i] = 0;
}

/* break all connections to the given node.
 */
static void breakConnections(int to)
{
    CInfo *cip;

    for (cip = cinfo; cip < &cinfo[NHOSTS]; cip++)
        if (cip->inuse && cip->toaddr == to)
            closecfd(cip->cfd);
}

/* dump np bytes to log starting at p */
void dump(Byte *p, int np)
{
#define BPR 8
    int ntot = 0;
    int i, n;

    do
    {
        char buf[10 * BPR], *bp = buf;
        n = np > BPR ? BPR : np;
        bp += sprintf(bp, "%3d..%3d: ", ntot, ntot + n - 1);
        for (i = 0; i < BPR; i++)
            if (i < n)
                bp += sprintf(bp, "%02x ", p[i]);
            else
                bp += sprintf(bp, "   ");
        bp += sprintf(bp, "   ");
        for (i = 0; i < BPR; i++)
        {
            *bp++ = (i < n && isprint(p[i])) ? p[i] : ' ';
            *bp++ = ' ';
        }
        *bp = 0;
        daemonLog("%s\n", buf);
        p += n;
        ntot += n;
    } while (np -= n);
}

/* given a packet, return a string describing its type */
char *p2tstr(Pkt *pkp)
{
    switch (pkp->info & PT_MASK)
    {
    case PT_SHELL:
        /* a few special cases */
        if (pkp->to == LOGADR)
            return ("LOGGING");
        return ("SHELL");
    case PT_BOOTREC:
        return ("BOOTREC");
    case PT_INTR:
        return ("INTR");
    case PT_KILL:
        return ("KILL");
    case PT_ACK:
        return ("ACK");
    case PT_REBOOT:
        return ("REBOOT");
    case PT_PING:
        return ("PING");
    case PT_GETVAR:
        return ("GETVAR");
    case PT_SETVAR:
        return ("SETVAR");
    case PT_SERDATA:
        return ("SERDATA");
    case PT_SERSETUP:
        return ("SERSETUP");
    default:
        return ("???");
    }
}

/* given an OpenWhy return a descriptive string */
static char *why2str(OpenWhy why)
{
    switch (why)
    {
    case FOR_SHELL:
        return ("FOR_SHELL");
    case FOR_BOOT:
        return ("FOR_BOOT");
    case FOR_REBOOT:
        return ("FOR_REBOOT");
    case FOR_SERIAL:
        return ("FOR_SERIAL");
    default:
        return ("FOR_???");
    }
}

/* increment verbose, modulo MAXV+1 */
static void onVerboseSig(int dummy)
{
    signal(SIGHUP, onVerboseSig);
    verbose = (verbose + 1) % (MAXV + 1);
    daemonLog("Verbose set to %d\n", verbose);
}

static void onExit(void)
{
    onBye(-1);
}

/* we die so the nodes do too since there is no way to resync with them. */
static void onBye(int signo)
{
    if (signo < 0)
        daemonLog("Exit: first rebooting all nodes\n");
    else
        daemonLog("Signal %d: first rebooting all nodes", signo);

    buildCtrlPkt(MAXNA + 1, BRDCA, PT_REBOOT);
    sendPkt(xpkt, 0); /* get no ACKs from BRDCA */
    sendPkt(xpkt, 0); /* repeat for good measure */

    if (signo < 0)
        daemonLog("Exit: Ok fine, we're outta here\n");
    else
        daemonLog("Signal %d: we're outta here\n", signo);
    _exit(0);
}

/* return pointer to the first unused cinfo[] entry, or NULL if full.
 * always return smallest possible, UNIX fashion.
 */
static CInfo *newCInfo()
{
    CInfo *cip;

    for (cip = cinfo; cip < &cinfo[NHOSTS]; cip++)
        if (!cip->inuse)
            return (cip);

    return (NULL);
}

/* scan cinfo[] for cfd fd.
 * exit if none.
 */
static CInfo *CFD2CIP(int fd)
{
    CInfo *cip;

    for (cip = cinfo; cip < &cinfo[NHOSTS]; cip++)
        if (cip->inuse && cip->cfd == fd)
            return (cip);

    daemonLog("fd %d disappeared!\n", fd);
    exit(1);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid,
                         "@(#) $RCSfile: csimcd.c,v $ $Date: 2002/10/24 01:31:27 $ $Revision: 1.2 $ $Name:  $"};
