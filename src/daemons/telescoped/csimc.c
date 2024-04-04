/* utils to handle csimc stuff */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "configfile.h"
#include "strops.h"
#include "telstatshm.h"
#include "running.h"
#include "misc.h"
#include "telenv.h"
#include "cliserv.h"
#include "csimc.h"

#include "teled.h"

/* info about each CSIMC connected.
 * index with MotorInfo->axis.
 */
CSIMCInfo csii[NNODES];

static char ipme[] = "127.0.0.1";
static char *host;
static int port = CSIMCPORT;
static char *cfg = "csimc.cfg";

/* insure csimcd is running and loaded with config scripts.
 * N.B. call this before other funcs.
 */
void
csiInit()
{
    char buf[256];
    int fd;

    /* gather optional host and port config info */
    if (!read1CfgEntry (0, cfg, "PORT", CFG_INT, &port, 0))
        daemonLog ("%15s = %d\n", "PORT", port);
    if (!read1CfgEntry(0, cfg, "HOST", CFG_STR, buf, sizeof(buf))) {
        daemonLog ("%15s = %s\n", "HOST", buf);
        host = strcpy (malloc(strlen(buf)+1), buf);
    }

    /* start daemon, reboot and load config scripts */
    sprintf (buf, "csimc -i %s %d -rl < /dev/null", host?host:ipme, port);
    if (system (buf)) {
        tdlog ("Can not load csimc scripts\n");
        exit(1);
    }

    /* test the connection */
    fd = csimcd_clconn (host, port);
    if (fd < 0) {
        tdlog ("Can not contact csimcd: %s\n", strerror(errno));
        exit (1);
    }
    (void) close (fd);
}

/* open addr using host and port.
 * return fd else -1.
 */
int
csiOpen (int addr)
{
    return (csi_open (host, port, addr));
}

/* close csi fd.
 * return 0 if ok, else -1
 */
int
csiClose (int fd)
{
    return (csi_close (fd));
}

/* use csi_open() to open cfd and sfd for mip->axis using host and port from
 * config file.
 * exit if real trouble.
 */
void
csiiOpen (MotorInfo *mip)
{
    int addr = mip->axis;
    int fd;

    fd = csiOpen (addr);
    if (fd < 0) {
        tdlog ("CSIMC cntrl open addr %d: %s\n", addr, strerror(errno));
        exit(1);
    }
    MIPCFD(mip) = fd;

    fd = csiOpen (addr);
    if (fd < 0) {
        tdlog ("CSIMC status open addr %d: %s\n", addr, strerror(errno));
        exit(1);
    }
    MIPSFD(mip) = fd;
}

/* close both channels in csii[] for mip.
 * N.B. assumes indeed open.
 */
void
csiiClose (MotorInfo *mip)
{
    csiClose (MIPCFD(mip));
    MIPCFD(mip) = 0;

    csiClose (MIPSFD(mip));
    MIPSFD(mip) = 0;
}

/* return 1 if given csimcd fd can be read, else 0 */
int
csiIsReady (int fd)
{
    struct timeval tv;
    fd_set r;
    int s;

    FD_ZERO (&r);
    FD_SET (fd, &r);
    tv.tv_sec = tv.tv_usec = 0;
    s = select(fd+1,&r,NULL,NULL,&tv);
    if (s < 0) {
        tdlog ("Select(%d): %s\n", fd, strerror(errno));
        exit(1);
    }
    return (s);
}

/* drain and discard any pending info from csimc fd */
void
csiDrain (int fd)
{
    char buf[128];

    while (csiIsReady(fd) && read (fd, buf, sizeof(buf)) > 0)
        continue;
}

/* issue stop(); or mtvel=0; to the given motor */
void
csiStop (MotorInfo *mip, int fast)
{
    int cfd = MIPCFD(mip);

    csi_intr (cfd);
    csiDrain(cfd);
    if (fast)
        csi_w (cfd, "stop();");
    else
        csi_w (cfd, "mtvel=0;");
}

/* set up the board with the given basic parameters.
 * N.B. we assume mip->have is set.
 */
void
csiSetup (MotorInfo *mip)
{
    struct timeval tv;

    int cfd = MIPCFD(mip);
    double scale = mip->step/(2*PI);
    csiDrain(cfd);
    csiDrain(MIPSFD(mip));

    tv.tv_sec = 0;
    tv.tv_usec = 250000;  // 250 ms wait
    select(0, NULL,NULL,NULL,&tv);

    if (mip->homelow)
        csi_w (cfd, "ipolar |= homebit;\n");
    else
        csi_w (cfd, "ipolar &= ~homebit;\n");

    csi_w(cfd, "esign = %d; maxvel=%.0f; maxacc=%.0f; limacc=%.0f; esteps=%d; msteps=%d;\n",
          mip->sign*mip->esign, mip->maxvel*scale,mip->maxacc*scale,mip->slimacc*scale,mip->haveenc ? mip->estep : 0, mip->step);

	csi_w(cfd, "mtvel=0;");

    // wait again before continuing
    tv.tv_sec = 0;
    tv.tv_usec = 250000;  // 250 ms wait
    select(0, NULL,NULL,NULL,&tv);
}
