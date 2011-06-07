#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>

#include "ccdcamera.h"
#include "ccdshared.h"
#include "telenv.h"
#include "strops.h"

/*
 * Support a camera that can be controlled via an external program
 * (an "auxcam" program)
 *
 * The auxcam program must support the following commands:
 *   -g x:y:w:h:bx:by:open	# test if given exp params are ok
 *   -x dur:x:y:w:h:bx:by:open	# perform specified exposure, pixels on stdout
 *   -k 			# kill current exp, if any
 *   -t temp			# set temp to given degrees C
 *   -T				# current temp on stdout in C
 *   -s open			# immediate shutter open or close
 *   -i				# 1-line camera id string on stdout
 *   -f				# max "w h bx by" on stdout
 *
*/

static int aux_fd = -1;		/* stdout/err of auxcam process */
static int aux_pid = -1;	/* auxcam pid */
static char aux_cmd[1024];		/* path to auxcam script */

static CCDExpoParams expsav;	/* copy of exposure params to use */

extern int TIME_SYNC_DELAY; /* in ccdcamera.c */

/************ Support functions ***********/

/* exec aux_cmd with the given args and capture its stdout/err.
 * if pipe/fork/exec fails return -1 and fill msg with strerror.
 * if <sync> then wait and return -(exit status) and any output in msg,
 * else set aux_fd to a file des to aux_cmd and set aux_pid and return 0.
 */
static int
auxExecCmd (char *argv[], int sync, char *msg)
{
	int pid, fd;
	int p[2];
	int i;

#ifdef CMDTRACE
	char *retbuf = msg;
	for (i = 0; argv[i]; i++)
	    fprintf (stderr, "auxExecCmd argv[%d] = '%s'\n", i, argv[i]);
#endif

	if (pipe(p) < 0) {
	    strcpy (msg, strerror(errno));
	    return (-1);
	}

	pid = fork();
	if (pid < 0) {
	    /* failed */
	    close (p[0]);
	    close (p[1]);
	    strcpy (msg, strerror(errno));
	    return (-1);
	}

	if (pid == 0) {
	    /* child */
	    close (p[0]);
	    close (0);
	    dup2 (p[1], 1);
	    close (p[1]);
	    for (i = 2; i < 100; i++)
		close (i);
	    execvp (aux_cmd, argv);
	    exit (errno);
	}

	/* parent continues.. */
	close (p[1]);
	fd = p[0];
	if (sync)  {
	    /* read response, gather exit status */
	    while ((i = read (fd, msg, 1024)) > 0)
		msg += i;
	    *msg = '\0';
	    close (fd);
	    wait (&i);
	    if (WIFEXITED(i))
		i = -WEXITSTATUS(i);
	    else
		i = -(32 + WTERMSIG(i));
	    if (i != 0 && strlen(msg) == 0)
		sprintf (msg, "Error from %s: %s", aux_cmd, strerror(-i));
	} else {
	    /* let process run, store info for later use, presumably readPix */
	    aux_fd = fd;
	    aux_pid = pid;
	    i = 0;
	}

#ifdef CMDTRACE
	    fprintf (stderr, "auxExecCmd returns %d\n", i);
	    if(sync) fprintf(stderr,"  return buffer: %s\n",retbuf);
#endif
	return (i);
}

static void
auxKill ()
{
	if (aux_pid > 0) {
	    kill (aux_pid, SIGKILL);
	    aux_pid = -1;
	}
	wait (NULL);
	if (aux_fd >= 0) {
	    close (aux_fd);
	    aux_fd = -1;
	}
}

/***************** Begin public Talon CCD interface functions *************/

/* Detect and initialize a camera. Optionally use information passed in 
 * via the *path string. For example, this may be a path to a kernel
 * device node, a path to an auxcam script, an IP address for a networked
 * camera, etc.
 *
 * Return 1 if camera was detected and successfully initialized.
 * Return 0 if camera was not detected.
 * Return -1 and put error string in *errmsg if camera was detected but
 * could not be initialized.
 */
int
aux_findCCD (char *path, char *errmsg)
{
    char *argv[10];
    char buf[10][32];
    int n;

    /* If path is of the form "auxcam:/path/to/script", then we assume
     * we're dealing with an auxcam here */
    if (strncmp(path, "auxcam:", 7) != 0) {
        return 0; // Auxcam not specified
    }

    /* Copy actual path name into scriptpath and fix it up */
    strcpy(aux_cmd, path+7);
    telfixpath(aux_cmd, aux_cmd);

    n = 0;
    sprintf (argv[n] = buf[n], "%s", basenm(aux_cmd)); n++;
    sprintf (argv[n] = buf[n], "-f"); n++;
    argv[n] = NULL;

    if (auxExecCmd (argv, 1, errmsg) != 0) {
        return -1; // Auxcam command failed
    }

    return 1; // Auxcam seems to be initialized and working
}


/* check and save the given exposure parameters.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
aux_setExpCCD (CCDExpoParams *expP, char *errmsg)
{
    char *argv[10];
    char bufs[10][128];
    int n;

    /* Save a copy for startExpCCD */
    expsav = *expP;

    n = 0;
    sprintf (argv[n] = bufs[n], "%s", basenm(aux_cmd)); n++;
    sprintf (argv[n] = bufs[n], "-g"); n++;
    sprintf (argv[n] = bufs[n], "%d:%d:%d:%d:%d:%d:%d",
            expP->sx, expP->sy, expP->sw, expP->sh,
            expP->bx, expP->by, expP->shutter);
    n++;
    argv[n] = NULL;

    return (auxExecCmd (argv, 1, errmsg));

}

/* start an exposure, as previously defined via setExpCCD().
 * we return immediately but leave things open so ccd_fd or aux_fd can be used.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
aux_startExpCCD (char *errmsg)
{
    char *argv[10];
    char bufs[10][128];
    int n;

    n = 0;
    sprintf (argv[n] = bufs[n], "%s", basenm(aux_cmd)); n++;
    sprintf (argv[n] = bufs[n], "-x"); n++;
    sprintf (argv[n] = bufs[n], "%d:%d:%d:%d:%d:%d:%d:%d",
            expsav.duration, expsav.sx, expsav.sy, expsav.sw, expsav.sh,
            expsav.bx, expsav.by, expsav.shutter); n++;
    if(TIME_SYNC_DELAY) {		
        sprintf (argv[n] = bufs[n], "%ld",time(NULL)+TIME_SYNC_DELAY); n++;
    }
    argv[n] = NULL;

    n = auxExecCmd (argv, 0, errmsg);
    if(TIME_SYNC_DELAY) {
        sleep(TIME_SYNC_DELAY);
    }
    return n;
}

/* set up for a new drift scan.
 * return 0 if ok else -1 with errmsg.
 * N.B. leave camera open of ok.
 */
int 
aux_setupDriftScan (CCDDriftScan *dsip, char *errmsg)
{
    strcpy (errmsg, "Drift scanning not supported");
    return (-1);
}

/* Performs RBI Annihilation process, if supported.
 * RBI Annihilation floods the CCD array with charge for
 * an "exposure" and then discards and drains.
 * This prevents exposure to bright objects from creating
 * Residual Buffer Images in subsequent frames.
 * Return 0 if ok, else set errmsg[] and return -1.
 */
int 
aux_performRBIFlushCCD (int flushMS, int numDrains, char* errmsg)
{
    sprintf (errmsg, "RBI Annihilation not supported");
    return (-1);
}

/* abort a current exposure, if any.
 */
void 
aux_abortExpCCD (void)
{
    char *argv[10];
    char buf[10][32];
    char msg[1024];
    int n;

    n = 0;
    sprintf (argv[n] = buf[n], "%s", basenm(aux_cmd)); n++;
    sprintf (argv[n] = buf[n], "-k"); n++;
    argv[n] = NULL;

    (void) auxExecCmd (argv, 1, msg);
    auxKill();
}

/* after calling startExpCCD() or setupDriftScan(), call this to obtain a
 * handle which is suitable for use with select(2) to wait for pixels to be
 * ready. return file descriptor if ok, else fill errmsg[] and return -1.
 */
int 
aux_selectHandleCCD (char *errmsg)
{
    if (aux_fd < 0) {
        strcpy (errmsg, "selectHandleCCD() before startExpCCD()");
        return (-1);
    } else 
        return (aux_fd);
}

/* set up the cooler according to tp.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
aux_setTempCCD (CCDTempInfo *tp, char *errmsg)
{
    char *argv[10];
    char buf[10][32];
    int n;

    n = 0;
    sprintf (argv[n] = buf[n], "%s", basenm(aux_cmd)); n++;
    sprintf (argv[n] = buf[n], "-t"); n++;
    // STO 10-22-02: Support "OFF" as well
    if(tp->s == CCDTS_OFF) {
        sprintf(argv[n] = buf[n],"%s","OFF"); n++;
    } else {
        sprintf (argv[n] = buf[n], "%d", tp->t); n++;
    }
    argv[n] = NULL;

    return (auxExecCmd (argv, 1, errmsg));
}

/* fetch the camera temp, in C.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
aux_getTempCCD (CCDTempInfo *tp, char *errmsg)
{
    char *argv[10];
    char buf[10][32];
    int n;

    n = 0;
    sprintf (argv[n] = buf[n], "%s", basenm(aux_cmd)); n++;
    sprintf (argv[n] = buf[n], "-T"); n++;
    argv[n] = NULL;

    if (auxExecCmd (argv, 1, errmsg) < 0)
        return (-1);

    parseTemperatureString(errmsg, tp);
    return 0;
}

/* immediate shutter control: open or close.
 * return 0 if ok, else set errmsg[] and return -1.
 */
int 
aux_setShutterNow (int open, char *errmsg)
{
    char *argv[10];
    char buf[10][32];
    int n;

    n = 0;
    sprintf (argv[n] = buf[n], "%s", basenm(aux_cmd)); n++;
    sprintf (argv[n] = buf[n], "-s"); n++;
    sprintf (argv[n] = buf[n], "%d", open); n++;
    argv[n] = NULL;

    return (auxExecCmd (argv, 1, errmsg));
}

/* fetch the camera ID string.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
aux_getIDCCD (char buf[], char *errmsg)
{
    int n;
    char *argv[10];
    char cbuf[10][32];

    n = 0;
    sprintf (argv[n] = cbuf[n], "%s", basenm(aux_cmd)); n++;
    sprintf (argv[n] = cbuf[n], "-i"); n++;
    argv[n] = NULL;

    if (auxExecCmd (argv, 1, errmsg) < 0)
        return (-1);
    n = strlen(errmsg);
    if (errmsg[n-1] == '\n')
        errmsg[n-1] = '\0';
    strcpy (buf, errmsg);
    return (0);
}

/* fetch the max camera settings.
 * return 0 if ok, else set errmsg[] and return -1.
 * leave camera closed when finished if it was when we were called.
 */
int 
aux_getSizeCCD (CCDExpoParams *cep, char *errmsg)
{
    char *argv[10];
    char buf[10][32];
    int n;

    n = 0;
    sprintf (argv[n] = buf[n], "%s", basenm(aux_cmd)); n++;
    sprintf (argv[n] = buf[n], "-f"); n++;
    argv[n] = NULL;

    if (auxExecCmd (argv, 1, errmsg) < 0)
        return (-1);

    return parseCCDSize(errmsg, cep);
}


/* read nbytes worth the pixels into mem[].
 * if `block' we wait even if none are ready now, else only wait if some are
 * ready on the first read.
 */
int
aux_readPix (char *mem, int nbytes, int block, char *errmsg)
{
    int ntot, nread;
    char *memend;

	int fflags = block ? 0 : O_NONBLOCK;

    if (aux_fd < 0) {
        (void) strcpy (errmsg, "camera not open");
        return (-1);
    }

    if (fcntl (aux_fd, F_SETFL, fflags) < 0) {
        (void) sprintf (errmsg, "fcntl(%d): %s",fflags,strerror(errno));
        auxKill();
        return (-1);
    }

    /* first pixel is just "heads up" */
    nread = read (aux_fd, mem, 1);
    if (nread < 0) {
        if (!block && errno == EAGAIN) {
            return (0);
        } else {
            strcpy (errmsg, strerror(errno));
            auxKill();
            return (-1);
        }
    }

    for (ntot = 0; ntot < nbytes; ntot += nread) {
        nread = read (aux_fd, mem+ntot, nbytes-ntot);
        if (nread <= 0) {
            if (nread < 0)
                strcpy (errmsg, strerror(errno));
            else {
                /* if smallish # bytes probably an error report */
                if (ntot < 100)
                    sprintf (errmsg, "%.*s", ntot-1, mem); /* no \n */
                else
                    sprintf (errmsg, "Unexpected EOF after %d bytes",
                            ntot);
            }
            auxKill();
            return (-1);
        }
    }

    /* byte swap FITS to our internal format */
    for (memend = mem+nbytes; mem < memend; ) {
        char tmp = *mem++;
        mem[-1] = *mem;
        *mem++ = tmp;
    }

    /* mop up */
    auxKill();
    return (0);

}

/* Fill in the CCDCallbacks structure with the appropriate function
 * calls for this particular driver. */
void aux_getCallbacksCCD(CCDCallbacks *callbacks)
{
    callbacks->findCCD = aux_findCCD;
    callbacks->setExpCCD = aux_setExpCCD;
    callbacks->startExpCCD = aux_startExpCCD;
    callbacks->setupDriftScan = aux_setupDriftScan;
    callbacks->performRBIFlushCCD = aux_performRBIFlushCCD;
    callbacks->abortExpCCD = aux_abortExpCCD;
    callbacks->selectHandleCCD = aux_selectHandleCCD;
    callbacks->setTempCCD = aux_setTempCCD;
    callbacks->getTempCCD = aux_getTempCCD;
    callbacks->setShutterNow = aux_setShutterNow;
    callbacks->getIDCCD = aux_getIDCCD;
    callbacks->getSizeCCD = aux_getSizeCCD;
    callbacks->readPix = aux_readPix;
}

