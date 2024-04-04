/* handle loading scripts and cracking the config file */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#include "strops.h"
#include "telenv.h"
#include "csimc.h"
#include "configfile.h"

#include "mc.h"

static FILE *openACFile (char *fn);
static int download (char *fn, FILE *fp, int fd);
static void escData (Byte b, Byte *bp, int *ip);
static int chkVersion (int vn, int addr);

/* open script fn and copy to node connected to fd.
 * return 0 if ok, else -1.
 */
int
loadOneCfg (int fd, char *fn)
{
	char buf[512];
	FILE *fp;
	int n;

	/* open */
	fp = openACFile (fn);
	if (!fp) {
	    printf ("%s: %s\n", fn, strerror(errno));
	    return (-1);
	}

	/* send */
	while ((n = fread (buf, sizeof(char), sizeof(buf), fp)) > 0) {
	    if (writeI (fd, buf, n) < 0) {
		printf ("Node %d: %s\n", csi_f2n(fd),strerror(errno));
		fclose (fp);
		return(-11);
	    }
	    pollBack(fd);
	}

	/* close script */
	fclose (fp);

	/* ok */
	printf ("Node %d loaded with %s\n", csi_f2n(fd), fn);
	return (0);
}

/* open the config file cfn and load all scripts to all nodes.
 * exit(3) if trouble.
 */
void
loadAllCfg (char *cfn)
{
	int addr;

	/* scan through cfn for INITn entries */
	for (addr = 0; addr <= MAXNA; addr++) {
	    char name[32], value[256];
	    sprintf (name, "INIT%d", addr);
	    if (!read1CfgEntry (0, cfn, name, CFG_STR, value, sizeof(value))) {
		/* scan for and load each .cmc file */
		char *fn, *vp;
		char buf[32];
		int fd;

		/* open node */
		fd = csi_open (host, port, addr);
		if (fd < 0) {
		    printf ("Can not open Host %s Port %d address %d\n",
							host, port, addr);
		    exit(3);
		}

		/* load each script */
		vp = value;
		while ((fn = strtok (vp, " \t")) != NULL) {
		    if (loadOneCfg (fd, fn) < 0)
			exit (3);
		    vp = NULL;
		}

		/* sync */
		if (csi_wr (fd, buf, sizeof(buf), "=version;") < 0) {
		    printf ("Addr %d error: %s\n", csi_f2n(fd),strerror(errno));
		    exit(3);
		}

		/* done */
		csi_close (fd);
	    }
	}
}

/* open the given file.
 * check first the path given, then in $TELHOME/archive/config.
 * return FILE * else NULL.
 */
static FILE *
openACFile (char *fn)
{
	FILE *fp;

	fp = fopen (fn, "r");
	if (!fp) {
	    char buf[1024];
	    sprintf (buf, "archive/config/%s", fn);
	    fp = telfopen (buf, "r");
	}
	if (!fp)
	    printf ("%s: %s\n", fn, strerror(errno));
	return (fp);
}

static void
guardFirmware (int signo)
{
	printf ("\nNever interrupt a firmware download while in progress.\n");
}

/* attempt to download firmware in fn to the node with addr.
 * return 0 if ok else -1.
 */
int
loadFirmware (int addr, char *fn)
{
	void (*oldint)();
	FILE *ffp;
	char buf[128];
	int bfd;
	int vn;

	/* open firmware file */
	ffp = openACFile (fn);
	if (!ffp)
	    return (-1);		/* already explained */

	/* open a boot connection */
	bfd = csi_bopen (host, port, addr);
	if (bfd < 0) {
	    printf ("Boot open(%d): %s\n", addr, strerror (errno));
	    fclose (ffp);
	    return (-1);
	}

	/* get version from first line */
	if (fgets (buf, sizeof(buf), ffp) == NULL) {
	    printf ("%s: %s\n", fn, strerror(errno));
	    fclose (ffp);
	    csi_close (bfd);
	    return (-1);
	}
	vn = atoi (buf);

	/* do not allow casual interruptions */
	oldint = signal (SIGINT, guardFirmware);

	/* load firmware */
	if (verbose)
	    printf ("Loading Node %d with version %d from %s.. ", addr, vn, fn);
	if (download (fn, ffp, bfd) < 0) {
	    fclose (ffp);
	    csi_close (bfd);
	    signal (SIGINT, oldint);
	    return(-1);
	}
	printf ("done.                  \n");

	/* finished with firmware file and boot connection */
	fclose (ffp);
	csi_close (bfd);

	/* confirm version */
	if (chkVersion (vn, addr) < 0) {
	    signal (SIGINT, oldint);
	    return (-1);
	}

	/* reinstate old handler */
	signal (SIGINT, oldint);

	/* ok! */
	if (verbose)
	    printf ("\a");	/* beep! */
	return (0);
}

/* load firmware in boot file fp named fn to socket fd.
 * return 0 if all ok, else -1.
 * N.B. we assume fp starts positioned at start of boot records.
 * N.B. if a BT_EXEC BootIm is found, it must be placed at the end of a packet
 *   because it will be immediately jumped to, anything after will be lost.
 */
static int
download (char *fn, FILE *fp, int fd)
{
	int bimsz = sizeof(BootIm);
	struct timeval tv0;
	int last_was_exec;
	char buf[PMXLEN*2];
	Byte data[PMXLEN*2];
	BootIm bim;
	long fsize, fcount;
	int ndata;
	int totd;

	/* get size for progress.. return to same fp position */
	fcount = ftell (fp);
	fseek (fp, 0L, SEEK_END);
	fsize = ftell (fp);
	fseek (fp, fcount, SEEK_SET);

	/* send, in BOOTREC packets */
	last_was_exec = 0;
	totd = 0;
	ndata = 0;
	gettimeofday (&tv0, NULL);
	while (1) {
	    int more = fread (&bim, bimsz, 1, fp) == 1;
	    int nmore = 0;
	    struct timeval tv1;
	    int ms;

	    /* show progress */
	    gettimeofday (&tv1, NULL);
	    ms = (tv1.tv_sec - tv0.tv_sec)*1000 +
					    (tv1.tv_usec - tv0.tv_usec)/1000;
	    if (ms >= 500) {
		char back[128];
		long fnow = ftell(fp);
		int left = (fsize-fnow)*ms/(1000*totd);
		int min = left/60;
		int sec = left%60;
		int n = printf ("%3ld%% @ %5dB/s %3d:%02d", 100*fnow/fsize,
						1000*totd/ms, min, sec);
		back[n] = 0;
		while (--n >= 0)
		    back[n] = '\b';
		printf ("%s", back);
		tv0 = tv1;
		totd = 0;
	    }

	    if (more) {
		/* read in BootIm */
		escData (bim.type, buf, &nmore);
		escData (bim.len, buf, &nmore);
		escData (bim.addrh, buf, &nmore);
		escData (bim.addrl, buf, &nmore);

		/* then the data, unless 0-length BT_EXEC record */
		if (bim.len > 0) {
		    Byte rbuf[PMXLEN*2];
		    int i;

		    if (fread (rbuf, bim.len, 1, fp) != 1) {
			printf ("%s is short\n", fn);
			return (-1);
		    }

		    for (i = 0; i < bim.len; i++)
			escData (rbuf[i], buf, &nmore);
		}
	    }

	    /* send packet if: eof, more would overflow, or last with BT_EXEC */
	    if ((!more && ndata) || ndata+nmore > PMXDAT || last_was_exec){
		Byte nsync;

		/* send 1 packet */
		if (writeI (fd, data, ndata) < 0) {
		    printf ("Boot write: %s\n", strerror(errno));
		    return (-1);
		}

		/* this lets daemon read our exact write size each time
		 * and let us give synced progress messages.
		 */
		if (readI (fd, &nsync, 1) != 1) {
		    printf ("Boot read: %s\n", strerror(errno));
		    return (-1);
		}
		if (nsync != ndata) {
		    printf ("Bad boot size: wrote %d read %d\n", ndata, nsync);
		    return (-1);
		}

		/* start accumulating another packet's worth */
		totd += ndata;
		ndata = 0;
	    }

	    if (!more)
		break;

	    if (nmore) {
		last_was_exec = (bim.type & BT_EXEC);
		memcpy (data+ndata, buf, nmore);
		ndata += nmore;
	    }
	}

	return (0);
}

/* put b in bp[*ip], accounting for PESC, and update *ip */
static void
escData (Byte b, Byte *bp, int *ip)
{
	switch (b) {
	case PSYNC:
	    bp[(*ip)++] = PESC;
	    bp[(*ip)++] = PESYNC;
	    break;
	case PESC:
	    bp[(*ip)++] = PESC;
	    bp[(*ip)++] = PEESC;
	    break;
	default:
	    bp[(*ip)++] = b;
	    break;
	}
}

/* ask addr for its version. if matches vn return 0 else -1 */
static int
chkVersion (int vn, int addr)
{
	char buf[32];
	int av;
	int fd;

	if (verbose)
	    printf ("Checking version.. ");

	/* open connection to addr */
	fd = csi_open (host, port, addr);
	if (fd < 0) {
	    printf ("Node %d socket: %s\n", addr, strerror(errno));
	    return (-1);
	}

	/* ask node its version */
	av = csi_wr (fd, buf, sizeof(buf), "=version;");
	if (av < 0) {
	    printf ("Node %d comm: %s\n", addr, strerror(errno));
	    csi_close (fd);
	    return (-1);
	}
	av = atoi (buf);

	/* should match */
	if (av != vn) {
	    printf ("Node %d says version %d, should be %d\n", addr, av, vn);
	    csi_close (fd);
	    return (-1);
	}

	/* YES! if get here, node is ready */
	if (verbose)
	    printf ("Node %d confirms version %d is running.\n", addr, av);
	csi_close(fd);
	return (0);
}
