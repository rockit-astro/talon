#ifdef VMS
#include <types.h>
#include <socket.h>
#include <in.h>
#include <netdb.h>
#include <time.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#if !defined(FD_ZERO) || defined(_AIX)
/* one last place to look for select() stuff */
#include <sys/select.h>
#endif

/* generic support functions */
extern int httpGET P_((char *host, char *GETcmd, char msg[]));
extern int mkconnection P_((char *host, int port, char msg[]));
extern int sendbytes P_((int fd, unsigned char buf[], int n));
extern int recvbytes P_((int fd, unsigned char buf[], int n));
extern int readbytes P_((int fd, unsigned char buf[], int n));
extern int recvline P_((int fd, char buf[], int n));
extern int recvlineb P_((int fd, char buf[], int n));
extern void stopd_up P_((void));
extern void stopd_down P_((void));

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: net.h,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $
 */
