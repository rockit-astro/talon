/* code to manage connections between clients and the csimcd.
 * follows APUE by W. Richard Stevens, section 15.5.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "csimc.h"

/*** low-level server connections, not for applications ***********************/

/* create the public csimcd server endpoint on this host with the given port.
 * return fd on which to accept() for new connections, else -1.
 */
int
csimcd_slisten (int port)
{
	struct sockaddr_in serv_socket;
	int serv_fd;
	int reuse = 1;

	/* make socket endpoint */
	if ((serv_fd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
		return (-1);
	
	/* bind to given port for any IP address */
	memset (&serv_socket, 0, sizeof(serv_socket));
	serv_socket.sin_family = AF_INET;
	serv_socket.sin_addr.s_addr = htonl (INADDR_ANY);
	serv_socket.sin_port = htons (port);
	if (setsockopt (serv_fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse))<0)
	    return (-1);
	if (bind(serv_fd,(struct sockaddr *)&serv_socket,sizeof(serv_socket))<0)
	    return (-1);

	/* willing to accept connections with a backlog of 5 pending */
	if (listen (serv_fd, 5) < 0)
		return (-1);

	/* serv_fd is ready */
	return (serv_fd);
}

/* server waits for a client connection to arrive.
 * serv_fd came from csimcd_slisten().
 * return private 2-way fd, else -1.
 */
int
csimcd_saccept (int serv_fd)
{
	struct sockaddr_in cli_socket;
	int cli_len, cli_fd;

	/* get a private connection to new client */
	cli_len = sizeof(cli_socket);
	if ((cli_fd = accept(serv_fd, (struct sockaddr *)&cli_socket, &cli_len))<0)
	    return (-1);

	/* ok */
	return (cli_fd);
}

/* connect to the csimcd server running on the given host and port.
 * if !host assume this host, !port CSIMCPORT.
 * returns a private 2-way fd, else -1.
 */
int
csimcd_clconn (char *host, int port)
{
	struct sockaddr_in cli_socket;
	struct hostent *hp;
	int cli_fd, len;

	/* assume local if not explicit */
	if (!host)
	    host = "127.0.0.1";
	if (!port)
	    port = CSIMCPORT;

	/* get host name running server */
	if (!(hp = gethostbyname(host)))
	    return (-1);

	/* create a socket */
	if ((cli_fd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
	    return (-1);

	/* connect to the server */
	len = sizeof(cli_socket);
	memset (&cli_socket, 0, len);
	cli_socket.sin_family = AF_INET;
	cli_socket.sin_addr.s_addr =
			    ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
	cli_socket.sin_port = htons(port);
	if (connect (cli_fd, (struct sockaddr *)&cli_socket, len) < 0) {
	    close (cli_fd);
	    return (-1);
	}
	
	/* ready */
	return (cli_fd);
}

/*** hi-level API connections *********************************************/

/* table to look up host and network addresses from file descriptor.
 * malloced/grown as needed.
 */
typedef struct {
    int inuse;
    int fd;
    int haddr;
    int naddr;
    OpenWhy why;
} FDInfo;
static FDInfo *fdinfo;
static int nfdinfo;

static void
fdiAdd (int fd, int haddr, int naddr, int why)
{
	FDInfo *fp, *lfp;

	if (!fdinfo)
	    fdinfo = (FDInfo *) malloc (sizeof(FDInfo));

	for (fp = fdinfo, lfp = fp + nfdinfo; fp < lfp; fp++)
	    if (!fp->inuse)
		break;
	if (fp == lfp) {
	    fdinfo = (FDInfo *) realloc (fdinfo, (nfdinfo+1)*sizeof(FDInfo));
	    fp = &fdinfo[nfdinfo++];
	}

	if (!fdinfo) {
	    fprintf (stderr, "No memory for more connections\n");
	    exit(1);
	}

	fp->inuse = 1;
	fp->fd = fd;
	fp->haddr = haddr;
	fp->naddr = naddr;
	fp->why = why;
}

static FDInfo *
fdiFind (int fd)
{
	FDInfo *fp, *lfp;

	for (fp = fdinfo, lfp = fp + nfdinfo; fp < lfp; fp++)
	    if (fp->inuse && fp->fd == fd)
		return (fp);
	return (NULL);
}

static int
fdisShell (int fd)
{
	FDInfo *fp = fdiFind(fd);
	return (fp && fp->why == FOR_SHELL);
}

static int
common_close (int fd)
{
	FDInfo *fp = fdiFind(fd);

	if (fp) {
	    (void) close (fp->fd);
	    fp->inuse = 0;
	    return (0);
	}
	return (-1);
}

static int
common_open (char *host, int port, int addr, OpenWhy why, int client)
{
	Byte preamble[3];
	int fd;

	/* contact server */
	fd = csimcd_clconn(host, port);
	if (fd < 0)
	    return (-1);

	/* tell address and why, and any extra */
	preamble[0] = addr;
	preamble[1] = why;
	preamble[2] = client;
	if (write (fd, preamble, sizeof(preamble)) < 0) {
	    (void) close (fd);
	    return (-1);
	}

	/* read back assigned host addr byte and to confirm connection */
	if (read (fd, preamble, 1) <= 0) {
	    (void) close (fd);
	    return (-1);
	}

	/* new */
	fdiAdd (fd, preamble[0], addr, why);
	return (fd);
}

/* build a shell connection to csimcd for the given TCP/IP host and port.
 * return fd or -1.
 */
int
csi_open (char *host, int port, int addr)
{
	return (common_open (host, port, addr, FOR_SHELL, 0));
}

/* build a serial connection to csimcd for the given TCP/IP host and port.
 * return fd or -1.
 */
int
csi_sopen (char *host, int port, int addr, int baud)
{
	return (common_open (host, port, addr, FOR_SERIAL, baud/300));
}

/* build a connection to csimcd for the given host/port for booting.
 * return fd or -1.
 */
int
csi_bopen (char *host, int port, int addr)
{
	return (common_open (host, port, addr, FOR_BOOT, 0));
}

/* convert a file descriptor from csi_[b]open() to its assigned host addr */
int
csi_f2h (int fd)
{
	FDInfo *fp = fdiFind (fd);
	if (fp)
	    return (fp->haddr);
	return (-1);
}

/* convert a file descriptor from csi_[b]open() to its assigned network addr */
int
csi_f2n (int fd)
{
	FDInfo *fp = fdiFind (fd);
	if (fp)
	    return (fp->naddr);
	return (-1);
}

/* inform node on connection fd to interrupt our shell.
 * wait for an arbitrary byte back for sync.
 */
int
csi_intr (int fd)
{
	Byte a = CSIMCD_INTR;

	if (!fdisShell(fd))
	    return (-1);
	if (write (fd, &a, 1) < 0)
	    return (-1);
	if (read (fd, &a, 1) < 0)
	    return (-1);
	return (0);
}

/* reboot all nodes .. doing so will make all our fd's be closed on the other
 * end. our caller should find them out with their select() or read() then call
 * csi_close().
 */
int
csi_rebootAll (char *host, int port)
{
	return (common_open (host, port, BRDCA, FOR_REBOOT, 0));
}

/* inform node on connection fd to kill our shell, then close fd */
int
csi_close (int fd)
{
	/* closing the fd results in sending PT_KILL. 
	 * this gets us that behavior automatically if a process dies as well.
	 */
	return (common_close (fd));
}

/* handy routine to send a command to the given node which requires no response.
 * return length of final message if ok, else -1.
 */
int
csi_w (int fd, char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	int l;

	va_start (ap, fmt);
	l = vsprintf (buf, fmt, ap);
	va_end (ap);

	if (write (fd, buf, l) < 0)
	    return (-1);

	return (l);
}

/* wait for and read up through the next newline or buflen-1 chars, whichever
 * comes first, into buf[]. '\0' is added to the end. Returns count, 0 if EOF,
 * or -1 if error.
 */
int
csi_r (int fd, char buf[], int buflen)
{
	int s, n;

	for (n = 0; n < buflen-1; ) {
	    if ((s = read (fd, &buf[n], 1)) <= 0)
		return (s);
	    if (buf[n++] == '\n')
		break;
	}

	buf[n] = '\0';
	return (n);
}

/* like csi_w() followed by csi_r() all in one.
 * N.B. this should only be used when we indeed expect a response. if none
 *   is expected, use csi_w().
 */
int
csi_wr (int fd, char rbuf[], int rbuflen, char *fmt, ...)
{
	va_list ap;
	char wbuf[1024];
	int l;

	va_start (ap, fmt);
	l = vsprintf (wbuf, fmt, ap);
	va_end (ap);

	if (write (fd, wbuf, l) < 0)
	    return (-1);

	return (csi_r (fd, rbuf, rbuflen));
}

/* create an expression which (hopefully!) returns an integer then
 * crack and return the value.
 * since we do not return an error code we exit if fail.
 */
int
csi_rix (int fd, char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	int l;

	va_start (ap, fmt);
	l = vsprintf (buf, fmt, ap);
	va_end (ap);

	if (write (fd, buf, l) < 0) {
	    fprintf (stderr, "csi_rix(%d, %s): %s\n", fd, buf, strerror(errno));
	    exit(1);
	}

	if (csi_r (fd, buf, sizeof(buf)) < 0)
	    return (-1);

	return (strtol (buf, NULL, 0));
}
