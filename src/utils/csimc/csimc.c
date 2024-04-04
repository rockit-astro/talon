/* command line interface to the CSIMC network.
 * can also boot nodes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include "strops.h"
#include "telenv.h"
#include "csimc.h"
#include "configfile.h"

#include "el.h"
#include "mc.h"

#define	CMCHAR		'!'		/* char to denote command mode */
#define DWT             40       	/* secs to wait for daemon to start */
#define	DEFBAUD		1200		/* default serial port baud rate */

int verbose;				/* verbose flag */

static void usage (void);
static void readCfg (char *cfn);
static void daemonCheck (void);
static void mainLoop(void);
static void doCmd (char buf[]);
static void doStdin(void);
static void doNodeIn(int fd);
static void sendBuf (char buf[], int n);
static char *skipWhite (char *bp);
static void closeFD(int fd);
static void kickPrompt(void);
static void onInt (int signo);
static void onCont (int signo);
static void noConn(void);
static void cmdHelp(void);
static void cmdConnect (int addr);
static void cmdClose (void);
static void cmdFirmware (char cmd[]);
static void cmdInterrupt (void);
static void cmdReboot (void);
static void cmdTrace (char cmd[]);
static void cmdLoad (char cmd[]);
static void cmdHistory (char cmd[]);
static void cmdSerial (char cmd[]);

static char *me;			/* our name, for usage */
static int nflag;			/* initial connection to addr */
static int addr;			/* if nflag address to connect */
static int tflag;			/* initial connection to tty */
static int lflag;			/* preload scripts on all nodes */
static int rflag;			/* reboot all nodes on network */
static int el;				/* set if using edit line */
static char targs[32];			/* string to collect tflag args */

static char cfg_def[] = "archive/config/csimc.cfg"; /* default config file */
static char *cfg_fn = cfg_def;		/* actual config file */
static char ipme[] = "127.0.0.1";	/* local host IP */
char *host = ipme;			/* actual server host */
int port = CSIMCPORT;			/* server port */
static char dname[] = "csimcd";		/* name of network daemon */

static fd_set fdset;			/* set of all conn fds, + stdin */
static int maxfdset;			/* max fd in fdset */
static int cfd;				/* current fd, 0 when none */
static FILE *tracefp;			/* tracing file, unless 0 */
static char tracefn[256];		/* tracing file name, if tracefp */

int
main (int ac, char *av[])
{
	me = basenm(av[0]);

	while ((--ac > 0) && ((*++av)[0] == '-')) {
	    char *s;
	    for (s = av[0]+1; *s != '\0'; s++)
		switch (*s) {
		case 'c':
		    if (ac < 2)
			usage();
		    cfg_fn = *++av;
		    --ac;
		    break;
		case 'i':
		    if (ac < 3)
			usage();
		    host = *++av;
		    port = atoi(*++av);
		    ac -= 2;
		    break;
		case 'l':
		    lflag++;
		    break;
		case 'n':
		    if (ac < 2)
			usage();
		    addr = strtol (*++av, NULL, 0);
		    --ac;
		    nflag++;
		    break;
		case 'r':
		    rflag++;
		    break;
		case 't':
		    if (ac < 3)
			usage();
		    sprintf (targs, "serial %s %s", av[1], av[2]);
		    av += 2;
		    ac -= 2;
		    tflag++;
		    break;
		case 'v':
		    verbose++;
		    break;
		default:
		    usage();
		}
	}

	/* ac remaining args starting at av[0] */
	if (ac > 0)
	    usage();

	/* various setup */
	readCfg(cfg_fn);
	signal (SIGPIPE, SIG_IGN);
	signal (SIGINT, onInt);
	signal (SIGCONT, onCont);
	setbuf (stdout, NULL);
	daemonCheck();
	if (elSetup() == 0) {
	    atexit (elReset);
	    verbose++;
	    el++;
	}

	/* start fdset with stdin */
	FD_ZERO (&fdset);
	FD_SET (0, &fdset);
	maxfdset = 0;

	/* handle some options */
	if (rflag) {
	    int fd = csi_rebootAll(host, port);
	    if (fd < 0) {
		printf ("Reboot failed to Host %s Port %d\n", host, port);
		exit(2);
	    }
	    csi_close(fd);
	}
	if (lflag)
	    loadAllCfg(cfg_fn);
	if (nflag)
	    cmdConnect(addr);
	if (tflag)
	    cmdSerial (targs);

	/* go */
	if (verbose)
	    printf ("Welcome, CSIMC user.\nType \"!\" for command summary.\n");
	kickPrompt();
	while (1)
	    mainLoop();

	return (1);
}

/* poll for and print any info back from fd */
void
pollBack (int fd)
{
	struct timeval tv;
	fd_set line;

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO (&line);
	FD_SET (fd, &line);
	while (select (fd+1, &line, NULL, NULL, &tv) == 1) {
	    char buf[1024];
	    int n = read (fd, buf, sizeof(buf));
	    if (n > 0)
		(void) write (1, buf, n);
	    else
		break;
	}
}

static void
usage()
{
	fprintf(stderr, "%s: [options]\n", me);
	fprintf(stderr, "Purpose: command line interface to CSIMC network\n");
	fprintf(stderr, "$Revision: 1.1.1.1 $\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, " -c f    set alternate config <f>; default is %s\n",
								    cfg_def);
	fprintf(stderr, " -i h p  connect to host <h> with port <p>;\n");
	fprintf(stderr, "         default is %s port %d\n", ipme, CSIMCPORT);
	fprintf(stderr, " -l      load all nodes as per config file\n");
	fprintf(stderr, " -n a    make initial connection to node <a>\n");
	fprintf(stderr, " -r      reboot all nodes on network\n");
	fprintf(stderr, " -t n b  make initial connection to serial port on node <n> at baud rate <b>.\n");
	fprintf(stderr, " -v      verbose\n");

	exit (4);
}

/* read config file for some defaults.
 * none are required.
 */
static void
readCfg (char *cfn)
{
	char newhost[128];

	if (!read1CfgEntry (0, cfn, "HOST", CFG_STR, newhost, sizeof(newhost)))
	    host = strcpy (malloc (strlen(newhost)+1), newhost);
	(void) read1CfgEntry (0, cfn, "PORT", CFG_INT, &port, 0);
}

static void
onInt (int signo)
{
	elClear();
	cmdInterrupt();
}

static void
onCont (int signo)
{
	/* restore cbreak mode */
	if (el)
	    elSetup();
}

/* check we can contact the csimcd daemon.
 * if not and it should be on the local host, try to start it.
 * exit if fail.
 */
static void
daemonCheck()
{
	int fd = csimcd_clconn(host, port);

	if (fd<0 && (!host || !strcmp(host,"localhost") || !strcmp(host,ipme))){
	    char buf[256];
	    int i;

	    sprintf (buf, "rund %s -i %d", dname, port);
	    if (system (buf) != 0) {
		fprintf (stderr, "Can not start %s\n", dname);
		exit (1);
	    }

	    for (i = 0; i < DWT; i++) {
		sleep (1);
		fd = csimcd_clconn(host, port);
		if (fd >= 0)
		    break;
	    }
	}

	if (fd < 0) {
	    fprintf (stderr, "%s: %s\n", dname, strerror(errno));
	    exit (1);
	}

	if (verbose)
	    printf ("%s on %s/%d is working ok.\n", dname, host,  port);
	(void) close (fd);
}

static void
mainLoop()
{
	fd_set rset;
	int fd, n;

	rset = fdset;
	n = selectI (maxfdset+1, &rset, NULL, NULL, NULL);
	if (n < 0) {
	    fprintf (stderr, "select(): %s\n", strerror(errno));
	    exit (5);
	}

	for (fd = 0; n > 0 && fd <= maxfdset; fd++) {
	    if (FD_ISSET (fd, &rset)) {
		if (fd == 0)
		    doStdin();
		else
		    doNodeIn(fd);
		--n;
	    }
	}
}

/* read and handle stdin */
static void
doStdin()
{
	char buf[1024];
	int n;

	if (el)
	    n = elNext(buf);
	else {
	    /* handle stdin here */
	    if (fgets (buf, sizeof(buf), stdin))
		n = strlen(buf);
	    else
		n = feof(stdin) ? -2 : -1;
	}

	switch (n) {
	case -2:	/* EOF */
	    exit (0);
	    break;
	case -1:	/* error */
	    fprintf (stderr, "stdin: %s\n", strerror(errno));
	    exit (5);
	    break;
	case 0:		/* keep working */
	    return;
	    break;
	default:	/* yes! */
	    break;
	}

	if (tracefp)
	    fprintf (tracefp, "%.*s", n, buf);

	doCmd (buf);
}

/* read and show input from the node on fd */
static void
doNodeIn (int fd)
{
	char buf[1024];
	int n;

	n = readI (fd, buf, sizeof(buf));
	if (n < 0) {
	    fprintf (stderr, "read(%d): %s\n", fd, strerror(errno));
	    exit (1);
	}
	if (n == 0) {
	    printf ("Node %d disconnected\n", csi_f2n(fd));
	    closeFD (fd);
	    kickPrompt();
	    return;
	}

	printf ("%.*s", n, buf);
	if (tracefp)
	    fprintf (tracefp, "%.*s", n, buf);
}

/* send buf to the daemon on the current connection */
static void
sendBuf (char buf[], int n)
{
	if (cfd == 0) {
	    noConn();
	    return;
	}
	n = writeI (cfd, buf, n);
	if (n < 0) {
	    fprintf (stderr, "write(fd=%d node=%d): %s\n", cfd, csi_f2n(cfd),
							    strerror(errno));
	    exit (1);
	}
}

/* move bp past any initial spaces or tabs, and return pointing to otherwise */
static char *
skipWhite (char *bp)
{
	while (*bp == ' ' || *bp == '\t')
	    bp++;
	return (bp);
}

/* close the given connection and update fdset */
static void
closeFD(int fd)
{
	int old;
	int i;

	if (fd == cfd)
	    cfd = 0;

	csi_close (fd);
	FD_CLR (fd, &fdset);
	old = maxfdset;
	maxfdset = 0;
	for (i = 0; i <= old; i++)
	    if (FD_ISSET(i, &fdset))
		maxfdset = i;
}

/* provide a fresh prompt if running interactive */
static void
kickPrompt()
{
	if (el) {
	    if (!cfd || csi_w (cfd, "prompt=1;") < 0)
		printf ("xx> ");
		/* don't close if csi_w fails .. let doNode discover it */
	}
}

/* call to fuss about there being no connection */
static void
noConn(void)
{
	printf ("No current connection.\n");
	kickPrompt();
}

/* dispatch the given command */
static void
doCmd (char bp[])
{
	bp = skipWhite (bp);

	if (*bp == CMCHAR) {
	    bp = skipWhite (bp+1);
	    switch (*bp) {
	    case 'F':	/* download new firmware */
		cmdFirmware (bp);
		break;
	    case 'c':	/* close */
		cmdClose ();
		break;
	    case 'l':	/* load script */
		cmdLoad (bp);
		break;
	    case 't':	/* command trace */
		cmdTrace (bp);
		break;
	    case 'i':	/* interrupt */
		cmdInterrupt ();
		break;
	    case 'h':	/* history */
		cmdHistory (bp);
		break;
	    case 'R':	/* reboot */
		cmdReboot ();
		break;
	    case 's':	/* serial */
		cmdSerial (bp);
		break;
	    default:
		if (isdigit(bp[0]))
		    cmdConnect(strtol(bp,NULL,0));
		else
		    cmdHelp();
		break;
	    }
	} else
	    sendBuf (bp, strlen(bp));
}

static void
cmdHelp()
{
    printf ("Commands: (first letter is sufficient; anything unknown prints this)\n");
    printf (" !<n>\t\t\tcreate new or resume existing connection to node <n>\n");
    printf (" !serial <n> [<b>]\tconnect to node <n> serial port @ baud rate <b>\n");
    printf (" !close\t\t\tclose the current connection\n");
    printf (" !load <f>\t\tload script .cmc file <f> to current shell\n");
    printf (" !history [n]\t\tshow command history, or repeat item [n]\n");
    printf (" !trace [<f>]\t\tappend output to file <f>, or turn off if no <f>\n");
    printf (" !interrupt\t\tsame as Ctrl-C\n");
    printf (" !Firm <a> <f>\t\tload firmware in .cmf file <f> onto node <a>\n");
    printf (" !Reboot\t\treboot *all* nodes\n");
    printf (" Ctrl-C\t\t\tstop any loops and go back to reading input\n");
    printf (" Ctrl-D or EOF\t\tclose all connections and exit\n");
    printf ("Line editing:\n");
    printf (" Backspace or Delete erases character to left of cursor\n");
    printf (" Up shows previous history         Down shows next history\n");
    printf (" Left moves left in line           Right moves right in line\n");
    printf (" ^A moves to beginning of line     ^E moves to end of line\n");
    printf (" ^K deletes to end of line         ^U deletes entire line\n");

    if (tracefp)
	printf ("Tracing to %s\n", tracefn);

    if (maxfdset > 0) {
	int i;

	printf ("Connections: host <=> CSIMC (* is current):\n");
	for (i = 1; i <= maxfdset; i++)
	    if (FD_ISSET(i, &fdset))
		printf (" %c %2d <=> %2d\n", i == cfd ? '*' : ' ',
					    csi_f2h (i), csi_f2n(i));
    }

    kickPrompt();
}

/* set current connection to addr, creating new connection if necessary. */
static void
cmdConnect (int addr)
{
	int fd;

	/* sanity-check proposed address */
	if (addr < 0 || addr > MAXNA) {
	    printf ("%d is an invalid node address\n", addr);
	    kickPrompt();
	    return;
	}

	/* look for existing connection to addr */
	for (fd = 1; fd <= maxfdset; fd++) {
	    if (FD_ISSET(fd, &fdset) && addr == csi_f2n(fd)) {
		if (verbose) {
		    if (fd == cfd)
			printf ("Already connected to node %d\n", addr);
		    else
			printf ("Resuming on node %d from %d\n", addr,
								csi_f2h(fd));
		}
		cfd = fd;
		break;
	    }
	}

	/* if checked all without success, make a new connection */
	if (fd > maxfdset) {
	    if (verbose)
		printf ("Making new connection with node %d\n", addr);
	    fd = csi_open(host, port, addr);
	    if (fd < 0) {
		printf ("Can not contact node %d: %s\n", addr, strerror(errno));
	    } else {
		FD_SET (fd, &fdset);
		if (fd > maxfdset)
		    maxfdset = fd;
		cfd = fd;
	    }
	}

	/* fresh prompt with results */
	kickPrompt();
}

/* firmware command. cmd is the entire original command line, sans CMCHAR. */
static void
cmdFirmware (char cmd[])
{
	char *fn;
	int addr;
	int fd;

	/* get args: <addr> <filename> */
	cmd = strtok (cmd, " \t\n");	/* skip "firmware" */
	cmd = strtok (NULL, " \t\n");	/* this is the address */
	if (!cmd || !isdigit(*cmd)) {
	    printf ("Please specify a node address to download.\n");
	    kickPrompt();
	    return;
	}
	addr = strtol(cmd,NULL,0);
	fn = strtok (NULL, " \t\n");	/* this is the filename */
	if (!fn) {
	    printf ("Please specify a .cmf firmware file to use.\n");
	    kickPrompt();
	    return;
	}

	/* break any connections to addr */
	for (fd = 1; fd <= maxfdset; fd++)
	    if (FD_ISSET(fd, &fdset) && csi_f2n(fd) == addr)
		closeFD (fd);

	/* download to addr */
	if (loadFirmware (addr, fn) < 0) 
	    printf ("Could not download %s to %d\n", fn, addr);
	else if (verbose)
	    printf ("Successfully downloaded %s to %d.\n", fn, addr);

	/* fresh prompt */
	kickPrompt();
}

/* load one .cmc script file to the current node.
 * cmd is the entire original command line, sans CMCHAR.
 */
static void
cmdLoad (char *cmd)
{
	char *fn;

	/* confirm a connection */
	if (!cfd) {
	    noConn();
	    return;
	}

	/* pull out the script file name */
	fn = strtok (cmd, " \t\n");	/* skip "load" */
	fn = strtok (NULL, " \t\n");	/* this is the script file name */
	if (!fn) {
	    printf ("Please specify a script file to load.\n");
	    kickPrompt();
	    return;
	}

	/* load */
	if (!loadOneCfg (cfd, fn) && verbose)
	    printf ("%s: script loaded successfully.\n", fn);
	kickPrompt();
}

/* close current connection */
static void
cmdClose (void)
{
	/* confirm a connection */
	if (!cfd) {
	    noConn();
	    return;
	}

	/* close */
	closeFD (cfd);
	kickPrompt();
}

/* send interrupt for current connection, if any */
static void
cmdInterrupt (void)
{
	/* confirm a connection */
	if (!cfd) {
	    noConn();
	    return;
	}

	/* do it */
	if (csi_intr (cfd) < 0) {
	    printf ("Error sending INTR to %d: %s", csi_f2n(cfd),
							 strerror(errno));
	    closeFD(cfd);
	}

	/* fresh prompt */
	kickPrompt();
}

/* send REBOOT to all nodes via current connection, if any */
static void
cmdReboot (void)
{
	int fd;

	/* do it */
	fd = csi_rebootAll (host, port);
	if (fd < 0)
	    printf ("Error trying to REBOOT: %s\n", strerror(errno));

	/* close our fd and force an extra no-connection prompt */
	closeFD(fd);
	cfd = 0;
	kickPrompt();

	/* all connections are about to close.. EOF handler will mop them up */
}

/* start or stop tracing. cmd[] is the entire original command line.
 * if a filename is given, turn on tracing and set tracefp, else turn off.
 */
static void
cmdTrace (char cmd[])
{
	char *fn;

	fn = strtok (cmd, " \t\n");	/* skip "trace" */
	fn = strtok (NULL, " \t\n");	/* file name, if any */

	if (fn) {
	    FILE *fp;

	    fp = fopen (fn, "a");
	    if (!fp) {
		printf ("%s: %s\n", fn, strerror(errno));
	    } else {
		setbuf (fp, NULL);		/* always immediate output */
		if (verbose)
		    printf ("Logging started to %s\n", fn);
		tracefp = fp;
		strcpy (tracefn, fn);
	    }
	} else {
	    if (tracefp) {
		fclose (tracefp);
		tracefp = NULL;
		if (verbose)
		    printf ("Logging ceased.\n");
	    } else
		printf ("Logging was already off.\n");
	}

	kickPrompt();
}

/* history command.
 * if followed by a number run that entry, else list.
 */
static void
cmdHistory (char cmd[])
{
	char buf[128];
	char *tok;

	tok = strtok (cmd, " \t\n");	/* skip "history" */
	tok = strtok (NULL, " \t\n");	/* this is the history number, if any */

	if (!tok) {
	    (void) elHistory (-1, NULL);
	    kickPrompt();
	} else if (elHistory (atoi(tok), buf) > 0) {
	    printf ("%s", buf);
	    doCmd (buf);
	} else {
	    /* elHistory already explained problem */
	    kickPrompt();
	}
}

/* serial command.
 * args are node, optional baud rate.
 */
static void
cmdSerial (char cmd[])
{
	char buf[128];
	int baud;
	int sfd;
	int addr;
	char *tok;

	/* get node addr and baud rate */
	tok = strtok (cmd, " \t\n");		/* skip "serial" */
	tok = strtok (NULL, " \t\n");		/* check for optional addr */
	if (tok) {
	    addr = atoi (tok);
	    if (addr < 0 || addr > MAXNA) {
		printf ("%d is an invalid node address\n", addr);
		kickPrompt();
		return;
	    }
	    tok = strtok (NULL, " \t\n");	/* now for optional baud */
	    if (tok) {
		baud = atoi(tok);
		if (baud%300) {
		    /* better than nothing */
		    printf ("Baud rate %d is not a standard value\n", baud);
		    kickPrompt();
		    return;
		}
	    } else {
		baud = DEFBAUD;
	    }
	} else {
	    if (cfd) {
		addr = csi_f2n(cfd);
	    } else {
		printf ("Specify a node\n");
		kickPrompt();
		return;
	    }
	    baud = DEFBAUD;
	}

	/* open */
	if (verbose)
	    printf ("Connecting to Node %d serial port @ %d ..\n", addr, baud);
	sfd = csi_sopen (host, port, addr, baud);
	if (sfd < 0) {
	    printf ("Node %d serial port connect attempt failed: %s\n", addr,
							    strerror(errno));
	    kickPrompt();
	    return;
	}

	/* copy here until EOF from either side */
	if (verbose)
	    printf (" .. ready. Quit with Ctrl-D.\n");
	while (1) {
	    fd_set sfdset;
	    int maxsfdsetp1;
	    int ns;

	    /* listen to stdin and node */
	    FD_ZERO (&sfdset);
	    FD_SET (0, &sfdset);
	    FD_SET (sfd, &sfdset);
	    maxsfdsetp1 = sfd+1;
	    ns = selectI (maxsfdsetp1, &sfdset, NULL, NULL, NULL);
	    if (FD_ISSET (0, &sfdset)) {
		int n = read (0, buf, sizeof(buf));	/* allow SIGINT out */
		if (n <= 0 || buf[0] == 4)
		    break;
		if (writeI (sfd, buf, n) < 0) {
		    printf ("Socket write error to Node %d: %s\n", addr,
							    strerror(errno));
		    break;
		}
	    }
	    if (FD_ISSET (sfd, &sfdset)) {
		int n = readI (sfd, buf, sizeof(buf));
		if (n <= 0) {
		    if (n < 0)
			printf ("Socket read error from Node %d: %s\n", addr,
							    strerror(errno));
		    else
			printf ("Socket EOF from Node %d\n", addr);
		    break;
		}
		(void) writeI (1, buf, n);	/* *we* never fail :-) */
	    }
	}

	/* done */
	if (verbose)
	    printf ("Node %d serial port connection closed.\n", addr);
	csi_close (sfd);
	kickPrompt();
}
