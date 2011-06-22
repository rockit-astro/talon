#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include "csimc.h"

#include "mc.h"

/* just like select(2) but retries if interrupted */
int
selectI (int n, fd_set *rp, fd_set *wp, fd_set *xp, struct timeval *tp)
{
	int s; 

	while ((s = select (n, rp, wp, xp, tp)) < 0 && errno == EINTR)
	    continue;
	return (s);
}

/* just like read(2) but retries if interrupted */
size_t
readI (int fd, void *buf, size_t n)
{
	size_t s;

	while ((s = read (fd, buf, n)) < 0 && errno == EINTR)
	    continue;
	return (s);
}

/* just like write(2) but retries if interrupted */
size_t
writeI (int fd, const void *buf, size_t n)
{
	size_t s;

	while ((s = write (fd, buf, n)) < 0 && errno == EINTR)
	    continue;
	return (s);
}

