/* get data from a URL */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>

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

#define	TOUT	180		/* max secs to wait for socket data.
				 * default http timeout is 3 minutes.
				 */

static int tout P_((int maxt, int fd, int w));
static char *herr P_((char *errmsg));
static int connect_to P_((int sockfd, struct sockaddr *serv_addr, int addrlen));
static int httpGET P_((char *host, char *GETcmd, char msg[]));
static int mkconnection P_((char *host, int port, char msg[]));
static int sendbytes P_((int fd, unsigned char buf[], int n));
static int recvlineb P_((int fd, char buf[], int n));                           

/* buffer for recvlineb() */
static char rb_linebuf[512];	/* [next .. bad-1] are good */
static int rb_next;		/* index of next good char */
static int rb_unk;		/* index of first unknown char */

/* get data from the given URL containing _TPL templates
 * exit if bad url format, else return -1 if trouble else 0.
 */
int
URLUpdate (char *url, WxStats *wp, double *tp, double *pp)
{
	static char fmt[] = "/%s HTTP/1.0\r\nUser-Agent: wxd\r\n\r\n";
	static char http[] = "http://";
	static char cmd[1024];
	static char *host = 0;
	char *file;
	char buf[1024];
	char *url0 = url;
	int nfound;
	char *sp;
	int fd;

	/* dig out the host and file name first time */
	if (!host) {
	    if (strncmp (url, http, sizeof(http)-1) == 0)
		url += sizeof(http)-1;
	    host = url;
	    file = strchr (url, '/');
	    if (file) {
		*file++ = '\0';	/* terminate EOS and move past '/' */
	    } else {
		daemonLog ("Bad url: %s\n", url0);
		exit(1);
	    }

	    /* create a well-formed http request */
	    sprintf (cmd, fmt, file);
	}

	/* connect */
	if ((fd = httpGET (host, cmd, buf)) < 0) {
	    daemonLog ("%s: %s\n", url0, buf);
	    return (-1);
	}

	/* pull, looking for bare keywords */
	nfound = 0;
	while (recvlineb (fd, buf, sizeof(buf)) > 0) {
	    if ((sp = strstr (buf, WS_TPL)) != NULL && 
             sp[strlen(WS_TPL)] == '=' &&
             sp[-1] != '%') {
		wp->wspeed = atoi (sp+strlen(WS_TPL)+1);
		nfound++;
	    }
	    if ((sp = strstr (buf, WD_TPL)) != NULL &&
             sp[strlen(WD_TPL)] == '=' &&
             sp[-1] != '%') {
		wp->wdir = atoi (sp+strlen(WD_TPL)+1);
		nfound++;
	    }
	    if ((sp = strstr (buf, HM_TPL)) != NULL &&
             sp[strlen(HM_TPL)] == '=' &&
             sp[-1] != '%') {
		wp->humidity = atoi (sp+strlen(HM_TPL)+1);
		nfound++;
	    }
	    if ((sp = strstr (buf, RN_TPL)) != NULL &&
             sp[strlen(RN_TPL)] == '=' &&
             sp[-1] != '%') {
		wp->rain = 10*atof(sp+strlen(RN_TPL)+1);
		nfound++;
	    }
	    if ((sp = strstr (buf, PR_TPL)) != NULL &&
             sp[strlen(PR_TPL)] == '=' &&
             sp[-1] != '%') {
		*pp = 33.86*atof(sp+strlen(PR_TPL)+1);
		nfound++;
	    }
	    if ((sp = strstr (buf, TM_TPL)) != NULL &&
             sp[strlen(TM_TPL)] == '=' &&
             sp[-1] != '%') {
		*tp = (5./9.)*(atof(sp+strlen(TM_TPL)+1)-32);
		nfound++;
	    }
	}

	/* finished with socket */
	close (fd);

	/* check for results */
	if (nfound != 6) {
	    daemonLog ("%s: did not find all keywords. Only found %d\n", 
                    url, nfound);
	    return (-1);
	}
	return (0);
}

/* open the host, do the given GET cmd, and return a socket fd for the result.
 * return -1 and with excuse in msg[], else 0 if ok.
 */
static int
httpGET (host, GETcmd, msg)
char *host;
char *GETcmd;
char msg[];
{
	char buf[1024];
	int fd;
	int n;

	/* connect */
	fd = mkconnection (host, 80, msg);
	if (fd < 0)
	    return (-1);
	(void) sprintf (buf, "GET %s", GETcmd);

	/* send it */
	n = strlen (buf);
	if (sendbytes(fd, (unsigned char *)buf, n) < 0) {
	    (void) sprintf (msg, "%s: send error: %s", host, strerror(errno));
	    (void) close (fd);
	    return (-1);
	}

	/* caller can read response */
	return (fd);
}

/* establish a TCP connection to the named host on the given port.
 * if ok return file descriptor, else -1 with excuse in msg[].
 * reset recvlineb() readahead in case old stuff left behind.
 */
static int
mkconnection (host, port, msg)
char *host;	/* name of server */
int port;	/* TCP port */
char msg[];	/* return diagnostic message here, if returning -1 */
{

	struct sockaddr_in serv_addr;
	struct hostent  *hp;
	int sockfd;

	/* don't want signal if loose connection to server */
	(void) signal (SIGPIPE, SIG_IGN);

	/* lookup host address.
	 * TODO: time out but even SIGALRM doesn't awaken this if it's stuck.
	 *   I bet that's why netscape forks a separate dnshelper process!
	 */
	hp = gethostbyname (host);
	if (!hp) {
	    (void) sprintf (msg, "Can not find IP of %s.\n%s", host, 
					    herr ("Try entering IP directly"));
	    return (-1);
	}

	/* create a socket to the host's server */
	(void) memset ((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr =
			((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
	serv_addr.sin_port = htons((short)port);
	if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
	    (void) sprintf (msg, "%s/%d: %s", host, port, strerror(errno));
	    return (-1);
	}
	if (connect_to (sockfd, (struct sockaddr *)&serv_addr,
						sizeof(serv_addr)) < 0) {
	    (void) sprintf (msg, "%s: %s", host, strerror(errno));
	    (void) close(sockfd);
	    return (-1);
	}

	/* reset readahead in case user uses recvlineb() */
	rb_next = rb_unk = 0;

	/* ok */
	return (sockfd);
}

/* send n bytes from buf to socket fd.
 * return 0 if ok else -1
 */
static int
sendbytes (fd, buf, n)
int fd;
unsigned char buf[];
int n;
{
	int ns, tot;

	for (tot = 0; tot < n; tot += ns) {
	    if (tout (TOUT, fd, 1) < 0)
		return (-1);
	    ns = write (fd, (void *)(buf+tot), n-tot);
	    if (ns <= 0)
		return (-1);
	}
	return (0);
}

/* rather like recvline but reads ahead in big chunk for efficiency.
 * return length if read a line ok, 0 if hit eof, -1 if error.
 * N.B. we silently swallow all '\r'.
 * N.B. we read ahead and can hide bytes after each call.
 */
static int
recvlineb (sock, buf, size)
int sock;
char *buf;
int size;
{
	char *origbuf = buf;		/* save to prevent overfilling buf */
	char c = '\0';
	int nr = 0;

	/* always leave room for trailing \n */
	size -= 1;

	/* read and copy linebuf[next] to buf until buf fills or copied a \n */
	do {

	    if (rb_next >= rb_unk) {
		/* linebuf is empty -- refill */
		if (tout (TOUT, sock, 0) < 0) {
		    nr = -1;
		    break;
		}
		nr = read (sock, rb_linebuf, sizeof(rb_linebuf));
		if (nr <= 0)
		    break;
		rb_next = 0;
		rb_unk = nr;
	    }

	    if ((c = rb_linebuf[rb_next++]) != '\r')
		*buf++ = c;

	} while (buf-origbuf < size && c != '\n');

	/* always give back a real line regardless, else status */
	if (c == '\n') {
	    *buf = '\0';
	    nr = buf - origbuf;
	}

	return (nr);
}

/* wait at most maxt secs for the ability to read/write using fd and allow X
 *   processing in the mean time.
 * w is 0 is for reading, 1 for writing, 2 for either.
 * return 0 if ok to proceed, else -1 if trouble or timeout.
 */
static int
tout (maxt, fd, w)
int maxt;
int fd;
int w;
{
	int i;
	    
	for (i = 0; i < maxt; i++) {
	    fd_set rset, wset;
	    struct timeval tv;
	    int ret;

	    FD_ZERO (&rset);
	    FD_ZERO (&wset);
	    switch (w) {
	    case 0:
	    	FD_SET (fd, &rset);
		break;
	    case 1:
	    	FD_SET (fd, &wset);
		break;
	    case 2:
	    	FD_SET (fd, &rset);
	    	FD_SET (fd, &wset);
		break;
	    default:
		printf ("Bug: tout() called with %d\n", w);
		exit(1);
	    }

	    tv.tv_sec = 1;
	    tv.tv_usec = 0;

	    ret = select (fd+1, &rset, &wset, NULL, &tv);
	    if (ret > 0)
		return (0);
	    if (ret < 0)
		return (-1);
	}

	errno = i == maxt ? ETIMEDOUT : EINTR;
	return (-1);
}

/* a networking error has occured. if we can dig out more details about why
 * using h_errno, return its message, otherwise just return errmsg unchanged.
 * we do this because we don't know how portable is h_errno?
 */
static char *
herr (errmsg)
char *errmsg;
{
#if defined(HOST_NOT_FOUND) && defined(TRY_AGAIN)
	switch (h_errno) {
	case HOST_NOT_FOUND:
	    errmsg = "Host Not Found";
	    break;
	case TRY_AGAIN:
	    errmsg = "Might be a temporary condition -- try again later";
	    break;
	}
#endif
	return (errmsg);
}

/* just like connect(2) but tries to time out after TOUT yet let X continue.
 * return 0 if ok, else -1.
 */
static int
connect_to (sockfd, serv_addr, addrlen)
int sockfd;
struct sockaddr *serv_addr;
int addrlen;
{
#ifdef O_NONBLOCK               /* _POSIX_SOURCE */
#define NOBLOCK O_NONBLOCK
#else
#define NOBLOCK O_NDELAY
#endif
	unsigned int len;
	int err;
	int flags;
	int ret;

	/* set socket non-blocking */
	flags = fcntl (sockfd, F_GETFL, 0);
	(void) fcntl (sockfd, F_SETFL, flags | NOBLOCK);

	/* start the connect */
	ret = connect (sockfd, serv_addr, addrlen);
	if (ret < 0 && errno != EINPROGRESS)
	    return (-1);

	/* wait for sockfd to become useable */
	ret = tout (TOUT, sockfd, 2);
	if (ret < 0)
	    return (-1);

	/* verify connection really completed */
	len = sizeof(err);
	err = 0;
	ret = getsockopt (sockfd, SOL_SOCKET, SO_ERROR, (char *) &err, &len);
	if (ret < 0)
	    return (-1);
	if (err != 0) {
	    errno = err;
	    return (-1);
	}

	/* looks good - restore blocking */
	(void) fcntl (sockfd, F_SETFL, flags);
	return (0);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: url.c,v $ $Date: 2001/04/19 21:12:11 $ $Revision: 1.1.1.1 $ $Name:  $"};
