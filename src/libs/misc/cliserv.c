/* these functions are used by the daemons and clients to establish 
 * communication through fifos and shared memory.
 * the naming convention for these functions is such that they start with
 * cli_ if they are use by the client, serv_ if by the server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "telstatshm.h"
#include "cliserv.h"
#include "telenv.h"

static void sigalarm_dummy (int dummy);
static void catch_alarm(void);
static int alarm_wentoff;

/* used by a daemon to announce a fifo pair for communications.
 * fd[0] should be used to read commands from clients, fd[1] to write
 * responses. we always make fresh fifos each time, and open writer for
 * non-block so servers never hang talking to dead clients.
 * return 0 if ok, else -1 with errno set and an excuse in msg[].
 */
int
serv_conn (char *name, int fd[2], char msg[])
{
	char ws[1024];

	(void) sprintf (ws, "comm/%s.in", name);
	telfixpath (ws, ws);
	(void) unlink (ws);
	if (mknod (ws, S_IFIFO|0660, 0) < 0) {
	    (void) sprintf (msg, "%s: %s", ws, strerror(errno));
	    return (-1);
	}
	fd[0] = open (ws, O_RDWR);
	if (fd[0] < 0) {
	    (void) sprintf (msg, "%s: %s", ws, strerror(errno));
	    return (-1);
	}

	/* cooperate with teloper group */
	fchmod (fd[0], S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

	/* used so serv_read() won't block forever */
	catch_alarm();

	(void) sprintf (ws, "comm/%s.out", name);
	telfixpath (ws, ws);
	(void) unlink (ws);
	if (mknod (ws, S_IFIFO|0660, 0) < 0) {
	    (void) sprintf (msg, "%s: %s", ws, strerror(errno));
	    close (fd[0]);
	    return (-1);
	}
	fd[1] = open (ws, O_RDWR|O_NONBLOCK);
	if (fd[1] < 0) {
	    (void) sprintf (msg, "%s: %s", ws, strerror(errno));
	    close (fd[0]);
	    return (-1);
	}

	/* cooperate with teloper group */
	fchmod (fd[1], S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

	return (0);
}

/* used by a client to contact a daemon.
 * fd[0] should be used to read responses from daemons, fd[1] to write commands.
 * return 0 if ok, else -1 with errno set and an excuse in msg[].
 */
int
cli_conn (char *name, int fd[2], char msg[])
{
	char ws[1024];
	struct stat ss;

	(void) sprintf (ws, "comm/%s.out", name);
	telfixpath (ws, ws);
	if (stat (ws, &ss) < 0) {
	    (void) sprintf (msg, "%s: %s", ws, strerror(errno));
	    return (-1);
	}
	if (!(ss.st_mode & S_IFIFO)) {
	    (void) sprintf (msg, "%s: not a FIFO", ws);
	    return (-1);
	}
	fd[0] = open (ws, O_RDONLY|O_NONBLOCK);
	if (fd[0] < 0) {
	    (void) sprintf (msg, "%s: %s", ws, strerror(errno));
	    return (-1);
	}
	(void) fcntl (fd[0], F_SETFL, 0);	/* turn off NONBLOCK */

	/* used so cli_read() won't block forever */
	catch_alarm();

	(void) sprintf (ws, "comm/%s.in", name);
	telfixpath (ws, ws);
	if (stat (ws, &ss) < 0) {
	    (void) sprintf (msg, "%s: %s", ws, strerror(errno));
	    return (-1);
	}
	if (!(ss.st_mode & S_IFIFO)) {
	    (void) sprintf (msg, "%s: not a FIFO", ws);
	    (void) close (fd[0]);
	    return (-1);
	}
	fd[1] = open (ws, O_WRONLY|O_NONBLOCK);
	if (fd[1] < 0) {
	    (void) sprintf (msg, "%s: %s", ws, strerror(errno));
	    (void) close (fd[0]);
	    return (-1);
	}
	(void) fcntl (fd[1], F_SETFL, 0);	/* turn off NONBLOCK */

	return (0);
}

/* disconnect from the named connection */
void
dis_conn (char *name, int fd[2])
{
	char ws[1024];

	(void) close (fd[0]);
	(void) close (fd[1]);
	(void) sprintf (ws, "comm/%s.in", name);
	telfixpath (ws, ws);
	(void) unlink (ws);
	(void) sprintf (ws, "comm/%s.out", name);
	telfixpath (ws, ws);
	(void) unlink (ws);
}

/* used by a server to write response to a client.
 * all such responses always begin with an integer code.
 * (we pick the correct fd to use for you :-)
 * return 0 if ok, else fill err[] with excuse and return -1.
 */
int
serv_write (int fd[2], int code, char *msg, char *err)
{
	char buf[1024];

	sprintf (buf, "%d %s", code, msg);
	return (cli_write (fd, buf, err));
}

/* used by a client to write msg to a server.
 * (we pick the correct fd to use for you :-)
 * return 0 if ok, else fill err[] with excuse and return -1.
 */
int
cli_write (int fd[2], char *msg, char *err)
{
	int l = strlen(msg);
	int s;

	/* include \0 if doesn't end with \n */
	if (msg[l-1] != '\n')
	    l++;

	s = write (fd[1], msg, l);
	if (s < 0)
	    sprintf (err, "%s", strerror(errno));
	else if (s == 0)
	    sprintf (err, "Fifo disappeared");
	else if (s < l)
	    sprintf (err, "Fifo write short");
	else
	    return (0);
	return (-1);
}

/* used by a server to read from a client into buf[bufl].
 * all such received messages are assumed to end with \0 or \n.
 * (we pick the correct fd to use for you :-)
 * if ok, return 0 with message in buf.
 * else fill buf[] with excuse and return -1.
 */
int
serv_read (int fd[2], char *buf, int bufl)
{
	int found;
	int n;
	int s;

	/* max time to wait for rest of message */
	alarm_wentoff = 0;
	alarm (5);

	/* read until get EOS, get bufl chars or alarm goes off */
	for (found = n = 0; 1; n++) {
	    if (n >= bufl) {
		sprintf (buf, "Buffer overflow");
		break;
	    }
	    s = read (fd[0], &buf[n], 1);
	    if (s < 0) {
		if (alarm_wentoff)
		    sprintf (buf, "Message timeout");
		else
		    sprintf (buf, "%s", strerror(errno));
		break;
	    }
	    if (s == 0) {
		sprintf (buf, "Fifo disappeared");
		break;
	    }
	    if (buf[n] == '\0' || buf[n] == '\n') {
		buf[n++] = '\0';
		found = 1;
		break;
	    }
	}

	alarm (0);				/* done with alarm */

	return (found ? 0 : -1);
}

/* used by a client to read from a server into buf[bufl].
 * (we pick the correct fd to use for you :-)
 * all such received messages are assumed to begin with an integer, then a
 * space, then a message.
 * if ok, return 0 with *code set to the leading number and remainder of
 *   message in buf (without the leading number).
 * else fill buf[] with excuse and return -1.
 */
int
cli_read (int fd[2], int *code, char *buf, int bufl)
{
	int n, v;
	char *sp;
	int found;
	int s;

	/* max time to wait for rest of message */
	alarm_wentoff = 0;
	alarm (5);

	/* read until get EOS, get bufl chars or alarm goes off */
	for (found = n = 0; 1; n++) {
	    if (n >= bufl) {
		sprintf (buf, "Buffer overflow");
		break;
	    }
	    s = read (fd[0], &buf[n], 1);
	    if (s < 0) {
		if (alarm_wentoff)
		    sprintf (buf, "Message timeout");
		else
		    sprintf (buf, "%s", strerror(errno));
		break;
	    }
	    if (s == 0) {
		sprintf (buf, "Fifo disappeared");
		break;
	    }
	    if (buf[n] == '\0' || buf[n] == '\n') {
		buf[n++] = '\0';
		found = 1;
		break;
	    }
	}

	alarm (0);				/* done with alarm */
	if (!found)				/* if nothing good found */
	    return (-1);			/* bail out */
	v = atoi (buf);				/* leading status number */
	sp = strchr (buf, ' ');			/* skip status code */
	if (sp) {				/* if found space */
	    while (*sp == ' ')			/* while over space */
		sp++;				/* skip to first non-space */
	    memmove (buf, sp, n-(sp-buf));	/* shift back over the number */
	}					/* else return orig buf */
	*code = v;
	return (0);
}

/* called when SIGALRM arrives. just set alarm_wentoff */
static void
sigalarm_dummy (int dummy)
{
	alarm_wentoff = 1;
}

/* arrange for reads to be interrupted.
 * N.B. plain signal() on linux defaults to auto-restart.
 */
static void
catch_alarm()
{
	struct sigaction act;

	act.sa_handler = sigalarm_dummy;
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_INTERRUPT
	act.sa_flags |= SA_INTERRUPT;
#endif
	sigaction (SIGALRM, &act, NULL);
}

/* connect to the telstatshm shared memory segment.
 * same function for cli and serv.
 * return 0 and set *tpp if ok, else -1.
 */
int
open_telshm(TelStatShm **tpp)
{
	int shmid;
	long addr;

	shmid = shmget (TELSTATSHMKEY, sizeof(TelStatShm), 0);
	if (shmid < 0)
	    return (-1);

	addr = (long) shmat (shmid, (void *)0, 0);
	if (addr == -1)
	    return (-1);

	*tpp = (TelStatShm *) addr;
	return (0);
}
