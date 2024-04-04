/* functions to lock, test and unlock whether a given program is running.
 *
 * N.B. we just use kill(pid,0). Yes, this is not atomic but it allows us to
 * include perl scripts in the scheme.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include "strops.h"
#include "telenv.h"
#include "running.h"

static char lock_fmt[] = "comm/%s.pid";	/* format for file names */

static void build_fn (char *name, char fn[]);
static int read_lockpid (char *fn);

/* set a lock for program `name', which is really this process.
 * if successful, the lock file also stores our pid.
 * return 0 if succeed; -1 if already locked or can't tell.
 */
int
lock_running(char *name)
{
	char fn[1024];
	char buf[32];
	int fd;

	/* bale if already locked */
	if (testlock_running (name) == 0)
	    return (-1);

	/* put our pid in new lock file */
	build_fn (name, fn);
	fd = open (fn, O_RDWR|O_CREAT, 0666);
	if (fd < 0) {
	    fprintf (stderr, "%s: %s\n", fn, strerror(errno));
	    return (-1);
	}
	fchmod (fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
	ftruncate (fd, 0);
	sprintf (buf, "%d\n", (int)getpid());
	write (fd, buf, strlen(buf));
	close (fd);

	return (0);
}

/* logically unlock the given program instance if it's us.
 * if killtoo then send it a SIGTERM too.
 */
void
unlock_running(char *name, int killtoo)
{
	char fn[1024];
	int pid;
	int us;

	build_fn (name, fn);
	pid = read_lockpid (fn);
	if (pid < 0)
	    return;
	us = (pid == getpid());
	if (us)
	    (void) unlink (fn);
	if (killtoo) {
	    (void) kill (pid, SIGTERM);
	    (void) waitpid (pid, NULL, 0);
	}
}

/* test the state of the lock for the given program instance.
 * return 0 if it looks likely the process is running, else -1.
 */
int
testlock_running(char *name)
{
	char fn[1024];
	int pid;

	build_fn (name, fn);
	pid = read_lockpid (fn);
	if (pid > 1 && (kill (pid, 0) == 0 || errno == EPERM))
	    return (0);
	return (-1);
}

/* build the lock/pid filename in fn */
static void
build_fn (char *name, char fn[])
{
	(void) sprintf (fn, lock_fmt, basenm(name));
	telfixpath (fn, fn);
}

/* read the given lock file and return the pid it contains, else -1.
 * N.B. we require pid to be > 1.
 */
static int
read_lockpid (char *fn)
{
	char buf[32];
	int fd, nr, pid;

	fd = open (fn, O_RDONLY);
	if (fd < 0)
	    return (-1);
	nr = read (fd, buf, sizeof(buf)-1);
	(void) close (fd);
	if (nr <= 0)
	    return (-1);
	buf[nr] = '\0';
	pid = atoi(buf);
	return (pid > 1 ? pid : -1);
}
