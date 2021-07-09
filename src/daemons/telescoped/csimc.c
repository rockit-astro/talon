/* utils to handle csimc stuff */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "cliserv.h"
#include "configfile.h"
#include "csimc.h"
#include "misc.h"
#include "running.h"
#include "strops.h"
#include "telenv.h"
#include "telstatshm.h"

#include "teled.h"

// useful for debugging with a single node for testing scripts, etc.  Normally set to "is_virtual_mode() virtual_mode"
//#define is_virtual_mode() 0
#define is_virtual_mode() virtual_mode

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
void csiInit()
{
    if (!is_virtual_mode())
    {
        char buf[256];
        int fd;

        /* gather optional host and port config info */
        if (!read1CfgEntry(0, cfg, "PORT", CFG_INT, &port, 0))
            daemonLog("%15s = %d\n", "PORT", port);
        if (!read1CfgEntry(0, cfg, "HOST", CFG_STR, buf, sizeof(buf)))
        {
            daemonLog("%15s = %s\n", "HOST", buf);
            host = strcpy(malloc(strlen(buf) + 1), buf);
        }

        /* start daemon, reboot and load config scripts */
        sprintf(buf, "csimc -i %s %d -rl < /dev/null", host ? host : ipme, port);
        if (system(buf))
        {
            tdlog("Can not load csimc scripts\n");
            exit(1);
        }

        /* test the connection */
        fd = csimcd_clconn(host, port);
        if (fd < 0)
        {
            tdlog("Can not contact csimcd: %s\n", strerror(errno));
            exit(1);
        }
        (void)close(fd);
    }
}

/* open addr using host and port.
 * return fd else -1.
 */
int csiOpen(int addr)
{
    if (!is_virtual_mode())
    {
        return (csi_open(host, port, addr));
    }
    else
    {
        return open("/dev/null", O_RDWR);
    }
}

/* close csi fd.
 * return 0 if ok, else -1
 */
int csiClose(int fd)
{
    if (!is_virtual_mode())
    {
        return (csi_close(fd));
    }
    else
    {
        close(fd);
        return 0;
    }
}

/* use csi_open() to open cfd and sfd for mip->axis using host and port from
 * config file.
 * exit if real trouble.
 */
void csiiOpen(MotorInfo *mip)
{
    if (!is_virtual_mode())
    {

        int addr = mip->axis;
        int fd;

        fd = csiOpen(addr);
        if (fd < 0)
        {
            tdlog("CSIMC cntrl open addr %d: %s\n", addr, strerror(errno));
            exit(1);
        }
        MIPCFD(mip) = fd;

        fd = csiOpen(addr);
        if (fd < 0)
        {
            tdlog("CSIMC status open addr %d: %s\n", addr, strerror(errno));
            exit(1);
        }
        MIPSFD(mip) = fd;
    }
}

/* close both channels in csii[] for mip.
 * N.B. assumes indeed open.
 */
void csiiClose(MotorInfo *mip)
{
    if (!is_virtual_mode())
    {

        csiClose(MIPCFD(mip));
        MIPCFD(mip) = 0;

        csiClose(MIPSFD(mip));
        MIPSFD(mip) = 0;
    }
}

/* return 1 if given csimcd fd can be read, else 0 */
int csiIsReady(int fd)
{
    if (is_virtual_mode())
    {
        return 1;
    }
    else
    {

        struct timeval tv;
        fd_set r;
        int s;

        FD_ZERO(&r);
        FD_SET(fd, &r);
        tv.tv_sec = tv.tv_usec = 0;
        s = select(fd + 1, &r, NULL, NULL, &tv);
        if (s < 0)
        {
            tdlog("Select(%d): %s\n", fd, strerror(errno));
            exit(1);
        }
        return (s);
    }
}

/* drain and discard any pending info from csimc fd */
void csiDrain(int fd)
{
    if (!is_virtual_mode())
    {

        char buf[128];

        while (csiIsReady(fd) && read(fd, buf, sizeof(buf)) > 0)
            continue;
    }
}

/* issue stop(); or mtvel=0; to the given motor */
void csiStop(MotorInfo *mip, int fast)
{
    if (!is_virtual_mode())
    {

        int cfd = MIPCFD(mip);

        csi_intr(cfd);
        csiDrain(cfd);
        if (fast)
            csi_w(cfd, "stop();");
        else
            csi_w(cfd, "mtvel=0;");
    }
}

/* set up the board with the given basic parameters.
 * N.B. we assume mip->have is set.
 */
void csiSetup(MotorInfo *mip)
{
    if (!is_virtual_mode())
    {

        struct timeval tv;

        int cfd = MIPCFD(mip);
        double scale = mip->step / (2 * PI);
        /*
            csi_w (cfd, "maxvel=%.0f;", mip->maxvel*scale);
            csi_w (cfd, "maxacc=%.0f;", mip->maxacc*scale);
            csi_w (cfd, "limacc=%.0f;", mip->slimacc*scale);
            csi_w (cfd, "esteps=%d;", mip->haveenc ? mip->estep : 0);
            csi_w (cfd, "msteps=%d;", mip->step);
            csi_w (cfd, "esign=%d;", mip->sign*mip->esign);
        */
        csiDrain(cfd);
        csiDrain(MIPSFD(mip));

        tv.tv_sec = 0;
        tv.tv_usec = 250000; // 250 ms wait
        select(0, NULL, NULL, NULL, &tv);

        if (mip->homelow)
            csi_w(cfd, "ipolar |= homebit;\n");
        else
            csi_w(cfd, "ipolar &= ~homebit;\n");

        csi_w(cfd, "esign = %d; maxvel=%.0f; maxacc=%.0f; limacc=%.0f; esteps=%d; msteps=%d;\n", mip->sign * mip->esign,
              mip->maxvel * scale, mip->maxacc * scale, mip->slimacc * scale, mip->haveenc ? mip->estep : 0, mip->step);

        csi_w(cfd, "mtvel=0;");

        /*
        tdlog("Wrote the following: \n");
        tdlog("    maxvel=%.0f;", mip->maxvel*scale);
        tdlog("    maxacc=%.0f;", mip->maxacc*scale);
        tdlog("    limacc=%.0f;", mip->slimacc*scale);
        tdlog("    esteps=%d;", mip->haveenc ? mip->estep : 0);
        tdlog("    msteps=%d;", mip->step);
        tdlog("    esign=%d;", mip->sign*mip->esign);
        if (mip->homelow)
            tdlog("    ipolar |= homebit; (HOMELOW == 1)");
        else
            tdlog("    ipolar &= ~homebit; (HOMELOW == 0)");
        */

        // wait again before continuing
        tv.tv_sec = 0;
        tv.tv_usec = 250000; // 250 ms wait
        select(0, NULL, NULL, NULL, &tv);
    }
}
