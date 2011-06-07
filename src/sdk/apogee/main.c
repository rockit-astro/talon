/* Linux driver (module actually) for all Apogee CCD Cameras as of Jan 1998.
 * (C) Copyright 1997-2000 Elwood Charles Downey. All rights reserved.
 *
 * 0.1 23 Jun 97  Begin work
 * 1.0 24 Jun 97  First image!
 * 1.1 25 Jun 97  Add support for MAXCAMS boards
 * 1.2  5 Sep 97  Add AP_MAJ/MINVER. Add ap_impact
 * 1.3 23 Nov 97  Support Mode and Test module parameter fields.
 * 1.4  8 Jan 98  Running ave temp, 2's comp parameter.
 * 1.5  9 Mar 98  Support Running ave with multiple cameras.
 * 1.6 15 Apr 98  Fix support for long cable.
 * 2.2 25 Apr 98  improve installation and stuck-bit messages
 * 2.3 15 May 98  impact now uses schedule(); add /proc/apogee
 * 2.4 28 Jul 98  schedule() in some inner poll loops; always flush while idle
 * 2.5 16 Jul 98  resume flushing if exposure is aborted.
 * 2.6 20 Aug 98  fail more gracefully if camera not plugged in right.
 * 2.7 11 Sep 98  add ap_state to /proc/apogee.
 * 2.8 20 Sep 98  add double-expo shutter state
 * 2.9 10 Nov 98  ap_read: fix return count when sh/by not whole (sw/bx OK)
 * 2.10 28 Jan99  add CAM_SET_SHTR
 * 2.11  9 Feb99  add CCDSO_Multi
 * 2.12 15 Aug99  add CCDDriftScan
 * 2.13 25 Oct99  update for 2.2.* kernels
 * 2.14 17 Nov99  copy to user row at a time
 * 2.15 22 May00  negative exposure duration implies trigger mode
 * 2.16 23 May00  improve subimage read times, and fix abort flushing
 * 2.17  9 Aug00  set MAX_VBIN 8 to hide new subimage alignment error
 *
 * To build and install:
 *   cc -Wall -O -DAP_MJR=120 -DMODULE -D__KERNEL__ -c *.c   # compile each .c
 *   ld -r -o apogee.o *.o			# link into one .o
 *   mknod apogee c 120 0	        	# make a special file
 *   insmod apogee.o apg0=1,2,3.. apg1=4,5,6..	# install module (see below)
 *
 * This code is based directly on Apogee's API source code, by permission.
 *
 * There are no interrupts. The I/O base addresses are set via module params.
 *
 * This driver supports open/close/ioctl/read/select as follows:
 *
 *   open() enforces a single user process using this driver at a time. If
 *     the mode flags are RDWR we actually try connecting to the board. If
 *     the mode flags are just WRONLY, we just read in and save a config file.
 *
 *   close() aborts an exposure in progress, if any, and marks the driver as
 *     not in use.
 *
 *   ioctl()'s do various functions as follows:
 *     CCD_SET_EXPO: set desired exposure duration, subimage, binning, shutter
 *     CCD_SET_TEMP: set cooler temp in degrees C, or turn cooler off if >= 0
 *     CCD_SET_SHTR: force shutter state
 *     CCD_GET_TEMP: read the TEC temperature, in degrees C
 *     CCD_GET_SIZE: get chip size
 *     CCD_GET_ID:   get an ident string for camera
 *     CCD_GET_DUMP: get register dump for debugging
 *     CCD_DRIFT_SCAN: setup for drift scanning, see below.
 *     CCD_TRACE:    turn tracing on or off
 *
 *   read() performs exposures and returns pixels. the exposure parameters are
 *     those supplied by the most recent CCD_SET_EXPO ioctl.
 *   blocking read:
 *     if there is no exposure in progress, a blocking read opens the shutter,
 *       waits for the end of the exposure, closes the shutter, reads the pixels
 *       back to the caller and returns. if the read is signaled while waiting,
 *       the exposure is aborted, the shutter is closed and errno=EINTR.
 *     if there is an exposure in progress, a blocking read will wait for it
 *       to complete, then close the shutter and return the pixels.
 *     if exposure setup time was < 0 then a blocking read waits forever for the
 *       trigger to occur then reads the pixels.
 *   non-blocking read (NONBLOCK):
 *     if there is no exposure in progress, a non-blocking read just
 *       opens the shutter, starts the exposure and returns 0 immediately
 *       without returning any data. the count given in the read call is
 *       immaterial, and the buffer is not used at all, but you should give a
 *       count > 0 just to insure the driver is really called. the shutter will
 *       be closed at the end of the exposure even if no process is listening
 *       at the time but beware that the pixels will continue to accumulate
 *       dark current until they are read out.
 *     if there is an exposure in progress, a non-blocking read returns -1 with
 *       errno=EAGAIN.
 *     if exposure setup time was < 0 then a non-blocking read checks for a
 *       triggered exposure to complete and if so then reads the pixels.
 *   if an exposure has already completed but the pixels have not yet been
 *       retrieved, either form of read will return all the pixels immediately.
 *   N.B.: partial reads are not supported, ie, reads that return data must
 *     provide an array of at least (sw/bx)*(sh/by)*2 bytes or else all pixels
 *     are lost and read returns -1 with errno=EFAULT.
 *
 *   select() will indicate a read will not block when the exposure is complete.
 *
 *   So: using a blocking read is the easiest way to run the camera, if your
 *   process has nothing else to do. But if it does (such as an X client), then
 *   start the exposure with a non-blocking read, use select() to check, then
 *   read all the pixels as soon as select says the read won't block.
 *
 * CCD_DRIFT_SCAN is entirely unique. First call ioctl with address of a
 *   CCDDriftScan; keep this stable since it will be used even after the ioctl
 *   returns. Then call read() with address and count of a buffer large enough
 *   to hold the entire chip. Calling read starts the drift scan and blocks
 *   forever, but driver is performing the scan, updating pixels in the
 *   buffer, updating rows and watching for a new rowint in the user's
 *   CCDDriftScan. The caller is blocked so this seems useless, but the
 *   value comes when the two areas (pixel buf and CCDDriftScan) are in
 *   shared memory for dynamic access. Drift scan is only available if the
 *   driver is compiled with #define TRY_DRIFTSCAN.
 *
 * When installing with insmod, the camera parameters must be set using apg0
 *   for the first camera, apg1 for the seconds, etc, as below. Also,
 *   ap_impact may be set to adjust the degree to which reading pixels
 *   impacts other system activities. 1 is minimal impact, higher numbers
 *   have greater impact but result in better images. If your images
 *   consistently have horizontal streaks in them, you need a higher number.
 *   Go gradually and make it no larger than the number of rows in the image.
 */


#include "apglob.h"			/* Apogee globals */
#include "aplow.h"			/* CCD_BIT_?? defines */
#include "aplinux.h"			/* linux glue */

#include <linux/module.h>		/* just here */

#define AP_MAJVER	2		/* major version number */
#define AP_MINVER	17		/* minor version number */

/* user-defineable options: */

static char ap_name[] = "apogee";	/* name known to module tools */
static char ap_id[] = "Apogee CCD Camera"; /* id string */

/* The following arrays hold values which are set when the module is loaded
 * via insmod. Many values may be discerned directly from the .ini file
 * supplied by Apogee. Values which are 0 may be skipped. The border values
 * are to allow the driver to hide edge cosmetics from the applications. The
 * only real restriction on them is that they be at least two. For example,
 * with a caching AP7 camera on a long cable the following is typical:
 *
 * insmod apogee.o apg0=0x290,520,520,4,4,4,4,100,210,100,1,1,4,1,1 ap_impact=3
 *
 * additional cameras would do the same but use apg1 .. up to apg3.
 *
 *   .ini name		Description
 *   ---------		-----------
 *   base		base address, in hex with leading 0x
 *   rows		total number of actual rows on chip
 *   columns		total number of actual columns on chip
 *   (top border)	top rows to be hidden, >= 2
 *   (bottom border)	bottom rows to be hidden, >= 2
 *   (left border)	left columns to be hidden, >= 2
 *   (right border)	right columns to be hidden, >= 2
 *   cal		temp calibration, as per .ini file
 *   scale		temp scale, as per .ini file but x100
 *   tscale		time scale, as per .ini file but x100
 *   caching		1 if controller supports caching, else 0
 *   cable		1 if camera is on a long cable, 0 if short
 *   mode		mode bits, as per .ini file
 *   test		test bits, as per .ini file
 *   16bit		1 if camera has 16 bit pixels, 0 if 12 or 14
 *   gain		CCD gain option (unused)
 *   opt1		Factory defined option 1 (unused)
 *   opt2		Factory defined option 2 (unused)
 *
 * 
 */

/* number of arrays should match MAXCAMS.
 * would make it a 2d array if could init it from insmod :-(
 */
static int apg0[APP_N];
static int apg1[APP_N];
static int apg2[APP_N];
static int apg3[APP_N];
static int *apg[MAXCAMS] = {apg0, apg1, apg2, apg3};

/* end of user-defineable options */

#if (LINUX_VERSION_CODE >= 0x020200)
#define MODAPP_N 18	/* must agree with APP_N in aplinux.h */
MODULE_PARM(apg0, "1-" __MODULE_STRING(MODAPP_N) "i");
MODULE_PARM(apg1, "1-" __MODULE_STRING(MODAPP_N) "i");
MODULE_PARM(apg2, "1-" __MODULE_STRING(MODAPP_N) "i");
MODULE_PARM(apg3, "1-" __MODULE_STRING(MODAPP_N) "i");
MODULE_PARM(ap_trace, "i");
MODULE_PARM(ap_impact, "i");
#endif

int ap_trace;				/* set for printk traces; > for more */
int ap_impact;				/* pixel reading impact; see apapi.c */
int ap_slbase;				/* slave controller addr, else 0 */
static int ap_nfound;			/* how many cameras are connected */

#define	AP_IO_EXTENT	16		/* bytes of I/O space */
#define	AP_SELTO	(200*HZ/1000)	/* select() polling rate, jiffies */
#define	AP_TRIGTO	(10*HZ/1000)	/* trigger poll period, jiffies */

/* driver state */
typedef enum {
    AP_ABSENT,				/* controller not known to be present */
    AP_CLOSED,				/* installed, but not now in use */
    AP_OPEN,				/* open for full use, but idle */
    AP_EXPOSING,			/* taking an image */
    AP_EXPDONE				/* image ready to be read */
} APState;

/* all of these are indexed by minor device number */
static CCDExpoParams ap_cep[MAXCAMS];	/* user's exposure details */
static APState ap_state[MAXCAMS];	/* current state of driver */
static struct wait_queue *ap_wq[MAXCAMS];/* expose-completion wait q */
static struct timer_list ap_tl[MAXCAMS];/* expose-completion timer list */
static int ap_timeron[MAXCAMS];		/* set when expose timer is on */
static int ap_multiexp[MAXCAMS];	/* used for multi-expose */
#if (LINUX_VERSION_CODE < 0x020200)
static struct wait_queue *ap_swq[MAXCAMS];/* used for select() */
static struct timer_list ap_stl[MAXCAMS];/* used for select() */
static int ap_stimeron[MAXCAMS];        /* set when select timer is on */
#endif

/* parameters to implement a running average temperature */
#define	RATEMPTO	(30*HZ)		/* max ratemp age, jiffies */
#define	NRATEMP		10		/* max nratemp */
static int ap_ratemp[MAXCAMS][NRATEMP];	/* history */
static int ap_nratemp[MAXCAMS];		/* N readings contributing to ratemp */
static int ap_ratemp_lj[MAXCAMS];	/* jiffies at last temp reading */

static void expTimerComplete (unsigned long minor);
static void expTriggerPollTimerComplete (unsigned long minor);

/* version-dependent means to get-from and send-to user space */
#if (LINUX_VERSION_CODE >= 0x020200)
#define	FROMU(o,a)							\
	if (copy_from_user((void *)(&o), (const void *)a, sizeof(o)))	\
	    return -EFAULT;
#define	TOU(o,a)							\
	if (copy_to_user((void *)a, (void *)(&o), sizeof(o)))		\
	    return -EFAULT;
#else
#define	FROMU(o,a)							\
	s = verify_area(VERIFY_READ, (void *)a, sizeof(o));		\
	if (s)								\
	    return (s);							\
	memcpy_fromfs((void *)(&o), (const void *)a, sizeof(o));
#define	TOU(o,a)							\
	s = verify_area(VERIFY_WRITE, (const void *)a, sizeof(o));	\
	if (s)								\
	    return (s);							\
	memcpy_tofs((void *)a, (void *)(&o), sizeof(o));
#endif /* (LINUX_VERSION_CODE >= 0x020200) */

/* handy way to pause the current process for n jiffies.
 * N.B. do _not_ use this with select().
 */
inline void
ap_pause (int n, USHORT index)
{
#if (LINUX_VERSION_CODE >= 0x020200)
	interruptible_sleep_on_timeout(&ap_wq[index],(long)n);
#else
	current->timeout = jiffies + n;
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	current->timeout = 0;
#endif /* (LINUX_VERSION_CODE >= 0x020200) */
}

static void
manual_open (int minor, int open)
{
	int hccd = MIN2HCCD(minor);

	if (ap_trace)
	    printk ("manual_open(%d)\n", open);
	set_shutter (hccd, FALSE);	/* whthr shutter is open while timing */
	set_shutter_open (hccd, open);	/* set shutter */
}

/* turn off any pending exposure timer, if any. */
static void
stopExpTimer(int minor)
{
	if (ap_timeron[minor]) {
	    del_timer (&ap_tl[minor]);
	    ap_timeron[minor] = 0;
	}
}

/* sleep until the exposure is complete, as indicated by a wakeup on ap_wq. */
static void
waitForExpTimer(int minor)
{
	if (ap_trace)
	    printk ("waitForExpTimer()\n");
	interruptible_sleep_on (&ap_wq[minor]);
}

/* start timer for next step of exposure.
 * return 0 if next step starts, or -1 if no more steps.
 */
static int
startExpTimer(int minor, int nextstep)
{
	int duration = 0;	/* just for lint */

	if (ap_trace)
	    printk ("startExpTimer(%d)\n", nextstep);

	/* figure out what to do next */
	switch (ap_cep[minor].shutter) {
	case CCDSO_Closed: /* FALLTHRU */
	case CCDSO_Open:
	    switch (nextstep) {
	    case 0:	/* start */
		duration = ap_cep[minor].duration;
		break;
	    case 1:	/* finished */
		return (-1);
	    }
	    break;

	case CCDSO_Dbl:
	    switch (nextstep) {
	    case 0:	/* start */
		duration = ap_cep[minor].duration/2;
		break;
	    case 1:	/* now close */
		duration = ap_cep[minor].duration/4;
		manual_open (minor, 0);
		break;
	    case 2:	/* now open */
		duration = ap_cep[minor].duration/4;
		manual_open (minor, 1);
		break;
	    default:	/* finished */
		return (-1);
	    }
	    break;

	case CCDSO_Multi:
	    switch (nextstep) {
	    case 0:	/* start */
		duration = ap_cep[minor].duration/3;
		break;

	    case 1:	/* FALLTHRU */
	    case 3:	/* now close */
		duration = ap_cep[minor].duration/6;
		manual_open (minor, 0);
		break;

	    case 2:	/* FALLTHRU */
	    case 4:	/* now open */
		duration = ap_cep[minor].duration/6;
		manual_open (minor, 1);
		break;

	    default:	/* finished */
		return (-1);
	    }
	    break;

	default:
	    /* unknown */
	    printk ("startExpTimer(): unknown shutter: %d\n", 
							ap_cep[minor].shutter);
	    return (-1);
	}

	/* start next step */
	stopExpTimer(minor);
	ap_multiexp[minor] = nextstep+1;
	init_timer (&ap_tl[minor]);
	ap_tl[minor].expires = jiffies + duration*HZ/1000 + 1;
	ap_tl[minor].function = expTimerComplete;
	ap_tl[minor].data = minor;
	add_timer (&ap_tl[minor]);
	ap_timeron[minor] = 1;
	return (0);
}

/* exposure timer expired -- do next multi-exp state or wake up ap_wq */
static void
expTimerComplete (unsigned long minor)
{
	if (ap_trace)
	    printk ("expTimerComplete(): %d %d\n", ap_cep[minor].shutter,
							ap_multiexp[minor]);

	/* start next step if there is one, else all done */
	if (startExpTimer(minor, ap_multiexp[minor]) < 0) {
	    ap_state[minor] = AP_EXPDONE;
	    ap_timeron[minor] = 0;
	    manual_open (minor, 0);
	    wake_up_interruptible (&ap_wq[minor]);
	}
}

/* start timer to check for trigger
 */
static int
startTriggerPollTimer(int minor)
{
	stopExpTimer(minor);
	init_timer (&ap_tl[minor]);
	ap_tl[minor].expires = jiffies + AP_TRIGTO + 1;
	ap_tl[minor].function = expTriggerPollTimerComplete;
	ap_tl[minor].data = minor;
	add_timer (&ap_tl[minor]);
	ap_timeron[minor] = 1;
	return (0);
}

/* time to poll trigger again.
 * if trigger is done set AP_EXPDONE and wake up ap_wq else repeat.
 */
static void
expTriggerPollTimerComplete (unsigned long minor)
{
	int hccd = MIN2HCCD(minor);

	if (ap_trace)
	    printk ("expTriggerPollTimerComplete()\n");

	if (poll_got_trigger (hccd, NOWAIT) == TRUE) {
	    ap_state[minor] = AP_EXPDONE;
	    ap_timeron[minor] = 0;
	    wake_up_interruptible (&ap_wq[minor]);
	} else {
	    startTriggerPollTimer(minor);
	}
}

#if (LINUX_VERSION_CODE < 0x020200)
/* turn off any pending select() timer, if any. */
static void
stopSelectTimer(int minor)
{
	if (ap_stimeron[minor]) {
	    del_timer (&ap_stl[minor]);
	    ap_stimeron[minor] = 0;
	}
}

/* called to start another wait/poll interval on behalf of select() */
static void
startSelectTimer (int minor)
{
	if (ap_trace > 1)
	    printk ("startSelectTimer()\n");
	stopSelectTimer(minor);
	init_timer (&ap_stl[minor]);
	ap_stl[minor].expires = jiffies + AP_SELTO;
	ap_stl[minor].function = (void(*)(unsigned long))wake_up_interruptible;
	ap_stl[minor].data = (unsigned long) &ap_swq[minor];
	add_timer (&ap_stl[minor]);
	ap_stimeron[minor] = 1;
}
#endif /* (LINUX_VERSION_CODE < 0x020200) */

/* get cooler state and running-ave temperature, in C.
 * return 0 if ok, else -1
 */
static int
ap_ReadRATempStatus (int minor, CCDTempInfo *tp)
{
	int sum;
	int i;

	/* read the real temp now */
	if (apReadTempStatus (minor, tp) < 0)
	    return (-1);

	/* discard running average if too old */
	if (jiffies > ap_ratemp_lj[minor] + RATEMPTO)
	    ap_nratemp[minor] = 0;
	ap_ratemp_lj[minor] = jiffies;

	/* insert at 0, shifting out first to make room */
	for (i = ap_nratemp[minor]; --i > 0; )
	    ap_ratemp[minor][i] = ap_ratemp[minor][i-1];
	ap_ratemp[minor][0] = tp->t;
	if (ap_nratemp[minor] < NRATEMP)
	    ap_nratemp[minor]++;

	/* compute new average and update */
	for (i = sum = 0; i < ap_nratemp[minor]; i++)
	    sum += ap_ratemp[minor][i];

	/* like floor(sum/ap_nratemp[minor] + 0.5) */
	tp->t = (2*(sum+1000)/ap_nratemp[minor] + 1)/2 - 1000/ap_nratemp[minor];
	return (0);
}

/* open the device and return 0, else a errno.
 */
static int
ap_open (struct inode *inode, struct file *file)
{
	int minor = MINOR (inode->i_rdev);
	int flags = file->f_flags;

	if (ap_trace)
	    printk ("ap_open(%d): ap_state=%d\n", minor, ap_state[minor]);
	if (minor < 0 || minor >= MAXCAMS)
	    return (-ENODEV);
	if (ap_state[minor] == AP_ABSENT)
	    return (-ENODEV);
	if (ap_state[minor] != AP_CLOSED)
	    return (-EBUSY);

	/* there's no write but RDWR captures the spirit of ioctl. */
	if ((flags & O_ACCMODE) != O_RDWR)
	    return (-EACCES);

	ap_state[minor] = AP_OPEN;
	MOD_INC_USE_COUNT;
	return (0);
}    

/* called when driver is closed. clean up and decrement module use count */
#if (LINUX_VERSION_CODE >= 0x020200)
static int
#else
static void
#endif
ap_release (struct inode *inode, struct file *file)
{
	int minor = MINOR (inode->i_rdev);

	if (ap_trace)
	    printk ("ap_release()\n");
#ifdef TRY_DRIFTSCAN
	stopDriftScan (minor);
#endif /* TRY_DRIFTSCAN */
	stopExpTimer(minor);
#if (LINUX_VERSION_CODE >= 0x020200)
        wake_up_interruptible (&ap_wq[minor]);
#else
	stopSelectTimer(minor);
#endif /* (LINUX_VERSION_CODE >= 0x020200) */
	if (ap_state[minor] == AP_EXPOSING)
	    apAbortExposure(minor);
	ap_state[minor] = AP_CLOSED;
	MOD_DEC_USE_COUNT;
#if (LINUX_VERSION_CODE >= 0x020200)
        return 0;
#endif /* (LINUX_VERSION_CODE >= 0x020200) */
}    

/* fake system call for lseek -- just so cat works */
#if (LINUX_VERSION_CODE >= 0x020200)
static loff_t
ap_lseek (struct file *file, loff_t offset, int orig)
#else
static int
ap_lseek (struct inode *inode, struct file *file, off_t offset, int orig)
#endif /* (LINUX_VERSION_CODE >= 0x020200) */
{
	if (ap_trace)
	    printk ("ap_lseek()\n");
	return (file->f_pos = 0);
}

/* read data from the camera, possibly performing an exposure first.
 * pixels are each an unsigned short and the first pixel returned is the upper
 *   left corner.
 *
 * algorithm:
 *   sanity check incoming buffer, if it might be used
 *   if a drift scan has been defined, go do that and never return.
 *   if an exposure has not been started
 *     start an exposure.
 *     if called as non-blocking
 *       that's all we do -- return 0
 *   if an exposure is in progress
 *     if we were called as non-blocking
 *       return EAGAIN
 *     wait for exposure to complete;
 *   data is ready: read the chip, copying data to caller, returning count;
 *
 * N.B. if we are ever interrupted while waiting for an exposure, we abort
 *   the exposure, close the shutter and return EINTR.
 * N.B. the size of the read count must be at least large enough to contain the
 *   entire data set from the chip, given the current binning and subimage size.
 *   if it is not, we return EFAULT.
 */
#if (LINUX_VERSION_CODE >= 0x020200)
static ssize_t
ap_read (struct file *file, char *buf, size_t count, loff_t *offset)
{
	int minor = MINOR (file->f_dentry->d_inode->i_rdev);
	CCDExpoParams *cep = &ap_cep[minor];
	CAMDATA *cdp = &camdata[minor];
	int nbytes = (cep->sw/cep->bx) * (cep->sh/cep->by) * 2;
	int s;

	if (ap_trace)
	    printk ("ap_read() ap_state=%d\n", ap_state[minor]);

	if (*offset) {
	    printk("ap_read() can't handle offsets!\n");
	    return (-EINVAL);
	}

#ifdef TRY_DRIFTSCAN
	/* if drift scan is enabled this never returns. */
	s = doDriftScan (minor, buf, count);
	if (s)
	    return (s);
#endif /* TRY_DRIFTSCAN */

	/* sanity check buf if we are really going to use it */
	if (ap_state[minor] == AP_EXPDONE || !(file->f_flags & O_NONBLOCK)) {
	    if ((((unsigned)buf) & 1) || count < nbytes)
		return (-EFAULT);
	    if ((count % 2) || !access_ok(VERIFY_WRITE,(const void*)buf,count))
		return (-EFAULT);
	}
#else
static int
ap_read (struct inode *inode, struct file *file, char *buf, int count)
{
	int minor = MINOR (inode->i_rdev);
	CCDExpoParams *cep = &ap_cep[minor];
	CAMDATA *cdp = &camdata[minor];
	int nbytes = (cep->sw/cep->bx) * (cep->sh/cep->by) * 2;
	int s;

	if (ap_trace)
	    printk ("ap_read() ap_state=%d\n", ap_state[minor]);

#ifdef TRY_DRIFTSCAN
	/* if drift scan is enabled this never returns. */
	s = doDriftScan (minor, buf, count);
	if (s)
	    return (s);
#endif /* TRY_DRIFTSCAN */

	/* sanity check buf if we are really going to use it */
	if (ap_state[minor] == AP_EXPDONE || !(file->f_flags & O_NONBLOCK)) {
	    if ((((unsigned)buf) & 1) || count < nbytes)
		return (-EFAULT);
	    s = verify_area(VERIFY_WRITE, (const void *) buf, nbytes);
	    if (s)
		return (s);
	}
#endif /* (LINUX_VERSION_CODE >= 0x020200) */

	if (ap_state[minor] == AP_OPEN) {
	    if (apStartExposure(minor) < 0)
		return (-ENXIO);
	    if (cdp->trigger_mode == CCD_TRIGNORM)
		(void) startExpTimer(minor, 0);
	    else
		(void) startTriggerPollTimer(minor);
	    ap_state[minor] = AP_EXPOSING;
            if (file->f_flags & O_NONBLOCK)
		return (0);
	}

	if (ap_state[minor] == AP_EXPOSING) {
            if (file->f_flags & O_NONBLOCK)
		return (-EAGAIN);
	    waitForExpTimer(minor);
	    if (ap_state[minor] == AP_EXPOSING) {
		/* still exposing means sleep was interrupted -- abort */
		stopExpTimer(minor);
		apAbortExposure (minor);
		ap_state[minor] = AP_OPEN;
		return (-EINTR);
	    }
	    ap_state[minor] = AP_EXPDONE;
	}

	if (ap_state[minor] != AP_EXPDONE) {
	    printk ("%s[%d]: ap_read but state is %d\n", ap_name, minor,
							    ap_state[minor]);
	    ap_state[minor] = AP_OPEN;
	    return (-EXDEV);
	}

	s = apDigitizeCCD (minor, (unsigned short *)buf);
	ap_state[minor] = AP_OPEN;
	return (s < 0 ? -EIO : nbytes);
}

/* system call interface for ioctl.
 * we support setting exposure params, turning the cooler on and off, reading
 * the camera temperature.
 */
static int
ap_ioctl (struct inode *inode, struct file *file, unsigned int cmd,
unsigned long arg)
{
	int minor = MINOR (inode->i_rdev);
	CAMDATA *cdp = &camdata[minor];
	char id[sizeof(ap_id) + 10];
	CCDExpoParams cep;
	CCDTempInfo tinfo;
	int s;

	switch (cmd) {
	case CCD_SET_EXPO: /* save caller's CCDExpoParams, at arg */
	    FROMU (cep, arg);
	    if (ap_trace)
		printk ("ExpoParams: %d/%d %d/%d/%d/%d %d ms\n", cep.bx, cep.by,
				cep.sx, cep.sy, cep.sw, cep.sh, cep.duration);
	    if (apSetupExposure (minor, &cep) < 0)
		return (-ENXIO);
	    ap_cep[minor] = cep;
	    break;

	case CCD_SET_TEMP: /* set temperature */
	    FROMU (tinfo, arg);
	    if (ap_trace)
		printk ("SET_TEMP: t=%d\n", tinfo.t);
	    if (tinfo.s == CCDTS_SET) {
		s = apSetTemperature (minor, tinfo.t);
		ap_nratemp[minor] = 0; /* reset running ave temp history */
	    } else
		s = apCoolerOff(minor);
	    if (s < 0)
		return (-ENXIO);
	    break;

	case CCD_SET_SHTR: /* force shutter state */
	    s = (int)arg;
	    if (ap_trace)
		printk ("SET_SHTR: %d\n", s);
	    switch (s) {
	    case 0:
	    case 1:
		manual_open (minor, s);
		break;
	    default:
		return (-ENOSYS);
	    }
	    break;

	case CCD_GET_TEMP: /* get temperature status */
	    if (ap_ReadRATempStatus(minor, &tinfo) < 0)
		return (-ENXIO);
	    TOU (tinfo, arg);
	    if (ap_trace)
		printk ("GET_TEMP: t=%d\n", tinfo.t);
	    break;

	case CCD_GET_SIZE: /* return max image size supported */
	    /* reduce size to account for fixed border.
	     * see apSetupExposure().
	     */
	    cep.sw = cdp->cols - (cdp->leftb + cdp->rightb);
	    cep.sh = cdp->rows - (cdp->topb + cdp->botb);
	    cep.bx = MAX_HBIN;
	    cep.by = MAX_VBIN;
	    TOU (cep, arg);
	    if (ap_trace)
		printk ("Max image %dx%d\n", cep.sw, cep.sh);
	    break;

	case CCD_GET_ID: /* return an id string to arg */
	    if (ap_nfound > 1)
		(void) sprintf (id, "%s %d", ap_id, minor);
	    else
		(void) sprintf (id, "%s", ap_id);
	    TOU (id, arg);
	    if (ap_trace)
		printk ("ID: %s\n", id);
	    break;

	case CCD_GET_DUMP: /* reg dump */
	    TOU (cdp->reg, arg);
	    break;

	case CCD_DRIFT_SCAN:
	    /* !arg means just asking whether supported */
#ifndef TRY_DRIFTSCAN
	    return (-ENOSYS);	/* no way */
#else
	    if (!arg)
		return (0);	/* yup */
	    s = setupDriftScan (minor, arg);
	    if (s)
		return(s);
	    ap_state[minor] = AP_EXPOSING;
#endif /* TRY_DRIFTSCAN */
	    break;

	case CCD_TRACE: /* set ap_trace from arg. */
	    ap_trace = (int)arg;
	    printk ("%s: tracing is %s\n", ap_name, ap_trace ? "on" : "off");
	    break;

	default: /* huh? */
	    if (ap_trace)
		printk ("ap_ioctl() cmd=%d\n", cmd);
	    return (-EINVAL);
	}

	/* ok */
	return (0);
}

#if (LINUX_VERSION_CODE >= 0x020200)
static unsigned int
ap_select (struct file *file, poll_table *wait)
{
	int minor = MINOR (file->f_dentry->d_inode->i_rdev);
	unsigned int mask=0;

	if (ap_trace > 1) printk ("ap_select(): wait=%d\n", !!wait);

	poll_wait (file, &ap_wq[minor], wait);
	if (ap_state[minor] == AP_EXPDONE) {
	    if (ap_trace > 1) printk ("ap_select(): pixels ready\n");
	    mask |= POLLIN | POLLRDNORM;
	} else if (ap_state[minor] != AP_EXPOSING) return POLLERR;
	return (mask | POLLOUT | POLLWRNORM);
}

#else
/* return 1 if pixels are ready else sleep_wait() and return 0.
 */
static int
ap_select (struct inode *inode, struct file *file, int sel_type,
select_table *wait)
{
	int minor = MINOR (inode->i_rdev);

	if (ap_trace > 1)
	    printk ("ap_select(): sel_type=%d wait=%d\n", sel_type, !!wait);

        switch (sel_type) {
        case SEL_EX:
        case SEL_OUT:
            return (0); /* never any exceptions or anything to write */
        case SEL_IN:
	    if (ap_state[minor] == AP_EXPDONE) {
		if (ap_trace > 1)
		    printk ("ap_select(): pixels ready\n");
		return (1);
	    }
            break;
        }

        /* pixels not ready -- set timer to check again later and wait */
	startSelectTimer (minor);
	select_wait (&ap_swq[minor], wait);

        return (0);
}
#endif /* (LINUX_VERSION_CODE >= 0x020200) */

/* the master hook into the support functions for this driver */
#if (LINUX_VERSION_CODE >= 0x020200)
static struct file_operations ap_fops = {
    llseek:	ap_lseek,
    read:	ap_read,
    poll:	ap_select,
    ioctl:	ap_ioctl,
    open:	ap_open,
    release:	ap_release,
};
#else
static struct file_operations ap_fops = {
    ap_lseek,
    ap_read,
    NULL,	/* write */
    NULL,	/* readdir */
    ap_select,
    ap_ioctl,
    NULL,	/* mmap */
    ap_open,
    ap_release,
    NULL,	/* fsync */
    NULL,	/* fasync */
    NULL,	/* check media change */
    NULL,	/* revalidate */
};
#endif /* (LINUX_VERSION_CODE >= 0x020200) */


/* make a /proc/apogee entry.
 * return the config params and current register shadow set for each attached
 * camera.
 * See Rubin page 75.
 */
static int
ap_read_proc (char *buf, char **stat, off_t offset, int len, int unused)
{
	int u, n;

	/* all-or-nothing room */
	if (len < ap_nfound*(80 + 12*20))
	    return (0);

	n = sprintf (buf, "%s Version %d.%d impact=%d\n", ap_name, AP_MAJVER,
							AP_MINVER, ap_impact);

	for (u = 0; u < ap_nfound; u++) {
	    CAMDATA *cdp = &camdata[u];
	    USHORT d;

	    /* name, unit and state */
	    n += sprintf (buf+n, "%s[%d]: state=", ap_name, u);
	    switch (ap_state[u]) {
	    case AP_ABSENT:   n += sprintf (buf+n, "ABSENT"); break;
	    case AP_CLOSED:   n += sprintf (buf+n, "CLOSED"); break;
	    case AP_OPEN:     n += sprintf (buf+n, "OPEN"); break;
	    case AP_EXPOSING: n += sprintf (buf+n, "EXPOSING"); break;
	    case AP_EXPDONE:  n += sprintf (buf+n, "EXPDONE"); break;
	    }
	    n += sprintf (buf+n, "\n");

	    /* dump config params */
	    n += sprintf(buf+n," IOBASE=0x%x", apg[u][APP_IOBASE]);
	    n += sprintf(buf+n," ROWS=%d", apg[u][APP_ROWS]);
	    n += sprintf(buf+n," COLS=%d", apg[u][APP_COLS]);
	    n += sprintf(buf+n," TOPB=%d", apg[u][APP_TOPB]);
	    n += sprintf(buf+n," BOTB=%d", apg[u][APP_BOTB]);
	    n += sprintf(buf+n," LEFTB=%d", apg[u][APP_LEFTB]);
	    n += sprintf(buf+n," RIGHTB=%d", apg[u][APP_RIGHTB]);
	    n += sprintf(buf+n," TEMPCAL=%d", apg[u][APP_TEMPCAL]);
	    n += sprintf(buf+n,"\n");
	    n += sprintf(buf+n," TEMPSCALE=%d", apg[u][APP_TEMPSCALE]);
	    n += sprintf(buf+n," TIMESCALE=%d", apg[u][APP_TIMESCALE]);
	    n += sprintf(buf+n," CACHE=%d", apg[u][APP_CACHE]);
	    n += sprintf(buf+n," CABLE=%d", apg[u][APP_CABLE]);
	    n += sprintf(buf+n," MODE=%d", apg[u][APP_MODE]);
	    n += sprintf(buf+n," TEST=%d", apg[u][APP_TEST]);
	    n += sprintf(buf+n," 16BIT=%d", apg[u][APP_16BIT]);
	    n += sprintf(buf+n, "\n");

	    /* dump w-o regs */
	    d = cdp->reg[0];
	    n += sprintf (buf+n, " Reg01 xx0 W: 0x%04X Misc Commands\n", d);
		n += sprintf(buf+n," ");
		n += sprintf(buf+n," %sCOOLER", (d&CCD_BIT_COOLER) ? " " : "!");
		n += sprintf(buf+n,"  %sCABLE", (d&CCD_BIT_CABLE) ? " " : "!");
		n += sprintf(buf+n,"   %sFLUSH",(d&CCD_BIT_FLUSH) ? " " : "!");
		n += sprintf(buf+n,"   %sNEXT",   (d&CCD_BIT_NEXT) ? " " : "!");
		n += sprintf(buf+n,"  %sTIMER",  (d&CCD_BIT_TIMER) ? " " : "!");
		n += sprintf(buf+n," %sRDONE",  (d&CCD_BIT_RDONE) ? " " : "!");
		n += sprintf(buf+n,"  %sDOWN",   (d&CCD_BIT_DOWN) ? " " : "!");
		n += sprintf(buf+n,"\n ");
		n += sprintf(buf+n," %sSHUTTER",(d&CCD_BIT_SHUTTER)? " " : "!");
		n += sprintf(buf+n," %sNOFLUSH",(d&CCD_BIT_NOFLUSH)? " " : "!");
		n += sprintf(buf+n," %sTRIGGER",(d&CCD_BIT_TRIGGER)? " " : "!");
		n += sprintf(buf+n," %sCACHE",  (d&CCD_BIT_CACHE) ? " " : "!");
		n += sprintf(buf+n," %sRESET",  (d&CCD_BIT_RESET) ? " " : "!");
		n += sprintf(buf+n," %sOVERRD", (d&CCD_BIT_OVERRD) ? " " : "!");
		n += sprintf(buf+n," %sTMSTRT", (d&CCD_BIT_TMSTRT) ? " " : "!");
		n += sprintf(buf+n, "\n");

	    d = cdp->reg[1];
	    n += sprintf (buf+n, " Reg02 xx2 W: 0x%04X Timer Lower 16\n", d);

	    d = cdp->reg[2];
	    n += sprintf (buf+n, " Reg03 xx4 W: 0x%04X Timer Upper 16", d);
		n += sprintf(buf+n," VBIN=0X%02X=%d", d>>8, d>>8);
		n += sprintf(buf+n," TIMER=0X%X=%d\n", d&0xf, d&0xf);

	    d = cdp->reg[3];
	    n += sprintf (buf+n, " Reg04 xx6 W: 0x%04X AIC           ", d);
		n += sprintf(buf+n," %sGCE", (d&CCD_BIT_GCE) ? " " : "!");
		n += sprintf(buf+n," %sGV1", (d&CCD_BIT_GV1) ? " " : "!");
		n += sprintf(buf+n," %sGV2", (d&CCD_BIT_GV2) ? " " : "!");
		n += sprintf(buf+n," %sAMP", (d&CCD_BIT_AMP) ? " " : "!");
		n += sprintf(buf+n," AIC=0x%03X=%d\n", d&0xfff, d&0xfff);

	    d = cdp->reg[4];
	    n += sprintf (buf+n, " Reg05 xx8 W: 0x%04X Desired Temp  ", d);
		n += sprintf(buf+n," RELAY=0x%02X DAC=0x%02X=%d\n", d>>8,
								d&0xff, d&0xff);

	    d = cdp->reg[5];
	    n += sprintf (buf+n, " Reg06 xxA W: 0x%04X Pixel Counter ", d);
		n += sprintf(buf+n," %sVSYNC", (d&CCD_BIT_VSYNC) ? " " : "!");
		n += sprintf(buf+n," HBIN=0x%X=%d", (d>>12)&7, (d>>12)&7);
		n += sprintf(buf+n," Pixel=%d\n", d&0xfff);

	    d = cdp->reg[6];
	    n += sprintf (buf+n, " Reg07 xxC W: 0x%04X Line Counter  ", d);
		n += sprintf(buf+n," MODE=0x%X=%d", d>>12, d>>12);
		n += sprintf(buf+n," LINE=%d\n", d&0xfff);

	    d = cdp->reg[7];
	    n += sprintf (buf+n, " Reg08 xxE W: 0x%04X BIC           ", d);
		n += sprintf(buf+n," TEST=0x%X=%d", d>>12, d>>12);
		n += sprintf(buf+n," BIC=%d\n", d&0xfff);

	    d = cdp->reg[8];
	    n += sprintf (buf+n, " Reg09 xx0 R: 0x%04X FIFO data\n", d);

	    d = cdp->reg[9];
	    n += sprintf (buf+n, " Reg10 xx2 R: 0x%04X Temp Data\n", d);

	    d = cdp->reg[10];
	    n += sprintf (buf+n, " Reg11 xx6 R: 0x%04X Status\n", d);
		n += sprintf(buf+n, " ");
		n += sprintf(buf+n," %sTST",(d&CCD_BIT_TST) ? " " : "!");
		n += sprintf(buf+n," %sACK",(d&CCD_BIT_ACK) ? " " : "!");
		n += sprintf(buf+n," %sFD",(d&CCD_BIT_FD) ? " " : "!");
		n += sprintf(buf+n," %sTRG",(d&CCD_BIT_TRG) ? " " : "!");
		n += sprintf(buf+n," %sAT",(d&CCD_BIT_AT) ? " " : "!");
		n += sprintf(buf+n," %sSC",(d&CCD_BIT_SC) ? " " : "!");
		n += sprintf(buf+n," %sTMAX",(d&CCD_BIT_TMAX) ? " " : "!");
		n += sprintf(buf+n," %sTMIN",(d&CCD_BIT_TMIN) ? " " : "!");
		n += sprintf(buf+n," %sDATA",(d&CCD_BIT_DATA) ? " " : "!");
		n += sprintf(buf+n," %sCOK",(d&CCD_BIT_COK) ? " " : "!");
		n += sprintf(buf+n," %sLN",(d&CCD_BIT_LN) ? " " : "!");
		n += sprintf(buf+n," %sEXP",(d&CCD_BIT_EXP) ? " " : "!");
		n += sprintf(buf+n, "\n");

	    d = cdp->reg[11];
	    n += sprintf (buf+n, " Reg12 xx8 R: 0x%04X Misc Cmd Feedback\n", d);
		n += sprintf(buf+n, " ");
		n += sprintf(buf+n," %sCOOLER", (d&CCD_BIT_COOLER) ? " " : "!");
		n += sprintf(buf+n,"  %sCABLE",  (d&CCD_BIT_CABLE) ? " " : "!");
		n += sprintf(buf+n,"   %sFLUSHL", (d&CCD_BIT_FLUSHL)?" " : "!");
		n += sprintf(buf+n,"  %sFLUSH",  (d&CCD_BIT_FLUSH) ? " " : "!");
		n += sprintf(buf+n," %sNEXT",   (d&CCD_BIT_NEXT) ? " " : "!");
		n += sprintf(buf+n,"  %sTIMER",  (d&CCD_BIT_TIMER) ? " " : "!");
		n += sprintf(buf+n,"  %sRDONE",  (d&CCD_BIT_RDONE) ? " " : "!");
		n += sprintf(buf+n,"  %sDOWN",   (d&CCD_BIT_DOWN) ? " " : "!");
		n += sprintf(buf+n,"\n ");
		n += sprintf(buf+n," %sSHUTTER",(d&CCD_BIT_SHUTTER)? " " : "!");
		n += sprintf(buf+n," %sNOFLUSH",(d&CCD_BIT_NOFLUSH)? " " : "!");
		n += sprintf(buf+n," %sTRIGGER",(d&CCD_BIT_TRIGGER)? " " : "!");
		n += sprintf(buf+n," %sCACHE",  (d&CCD_BIT_CACHE) ? " " : "!");
		n += sprintf(buf+n," %sRESET",  (d&CCD_BIT_RESET) ? " " : "!");
		n += sprintf(buf+n," %sOVERRD", (d&CCD_BIT_OVERRD) ? " " : "!");
		n += sprintf(buf+n," %sTMSTRT", (d&CCD_BIT_TMSTRT) ? " " : "!");
		n += sprintf(buf+n," %sTMGO",   (d&CCD_BIT_TMGO) ? " " : "!");
		n += sprintf(buf+n, "\n");
	}

	return (n);
}

static struct proc_dir_entry
ap_proc_entry = {
    0,			/* low_ino: the inode -- dynamic */
    6, "apogee",	/* len of name and name */
    S_IFREG|S_IRUGO,	/* mode */
    1, 0, 0,		/* nlinks, owner, group */
    0,			/* size -- unused */
    NULL, 		/* operations -- use default */
    &ap_read_proc,	/* function used to read data */
    /* nothing more */
};

/* called by Linux when we are installed */
int 
init_module (void)
{
	int minor;

	/* scan all possible minor numbers.
	 * N.B. don't commit linux resources until after the loop.
	 */
	for (ap_nfound = minor = 0; minor < MAXCAMS; minor++) {
	    CAMDATA *cdp = &camdata[minor];
	    int base = apg[minor][APP_IOBASE];
	    CCDExpoParams *cep = &ap_cep[minor];
	    int n;

	    /* assume no device until we know otherwise */
	    ap_state[minor] = AP_ABSENT;

	    /* skip if no base address defined */
	    if (!base)
		continue;

	    /* base > 0xfff means top 3 digits are address of slave */
	    if (base > 0xfff) {
		ap_slbase = base >> 12;
		base &= 0xfff;
		apg[minor][APP_IOBASE] = base;
	    }

	    /* see whether these IO ports are already allocated */
	    if (check_region (base, AP_IO_EXTENT)) {
		printk ("%s[%d]: 0x%x already in use\n", ap_name, minor, base);
		continue;
	    }

	    /* probe device */
	    if (!apProbe(base)) {
		printk ("%s[%d] at 0x%x: not present or not ready\n",
							ap_name, minor, base);
		continue;
	    }

	    /* perform any one-time initialization and more sanity checks */
	    if (apInit (minor, apg[minor]) < 0) {
		printk ("%s[%d]: init failed\n", ap_name, minor);
		continue;
	    }
	    cep->sw = cdp->cols - (cdp->leftb + cdp->rightb);
	    cep->sh = cdp->rows - (cdp->topb + cdp->botb);
	    cep->bx = cdp->hbin;
	    cep->by = cdp->vbin;

	    /* set up pixel bias shift */
	    cdp->pixbias = apg[minor][APP_16BIT] ? 32768 : 0;

	    /* set buffer for one row */
	    n = cep->sw*sizeof(USHORT);
	    cdp->tmpbuf = vmalloc (n);
	    if (!cdp->tmpbuf) {
		printk ("%s[%d]: can not get %d for row\n", ap_name, minor, n);
		continue;
	    }

	    ap_state[minor] = AP_CLOSED;
	    ap_nfound++;
	}

	if (ap_nfound == 0) {
	    printk ("No good cameras found or bad module parameters.\n");
	    printk ("  Example: apg0=0x290,520,796,2,2,2,2,100,210,100,0,1\n");
	    return (-EINVAL);
	}

	/* if got this far, check the slave if configured.. can't probe */
	if (ap_slbase && check_region (ap_slbase, AP_IO_EXTENT)) {
	    printk ("%s.slave: 0x%x already in use\n", ap_name, ap_slbase);
	    return (-EINVAL);
	}

	/* install our handler functions -- AP_MJR might be in use */
	if (register_chrdev (AP_MJR, ap_name, &ap_fops) == -EBUSY) {
	    printk ("%s: major %d already in use\n", ap_name, AP_MJR);
	    return (-EBUSY);
	}

	/* sanity check ap_impact */
	if (ap_impact < 1)
	    ap_impact = 1;

	/* ok, register the io regions as in use with linux */
	for (minor = 0; minor < MAXCAMS; minor++) {
	    int base;
	    int i;

	    if (ap_state[minor] == AP_ABSENT)
		continue;

	    base = apg[minor][APP_IOBASE];
	    request_region (base, AP_IO_EXTENT, ap_name);

	    printk ("%s[%d]: 0x%x", ap_name, minor, base);
	    for (i = APP_ROWS; i <= APP_16BIT; i++)
		printk (",%d", apg[minor][i]);
	    printk ("\n");
	    printk ("%s[%d]: Version %d.%d ready. Impact level %d\n",
			ap_name, minor, AP_MAJVER, AP_MINVER, ap_impact);
	}

	/* register the /proc entru */
#if (LINUX_VERSION_CODE >= 0x020200)
        proc_register (&proc_root, &ap_proc_entry);
#else
	proc_register_dynamic (&proc_root, &ap_proc_entry);
#endif

	/* register slave too */
	if (ap_slbase) {
	    request_region (ap_slbase, AP_IO_EXTENT, ap_name);
	    printk ("%s.slave: 0x%x [assumed]\n", ap_name, ap_slbase);
	}

	/* all set to go */
	return (0);
}

/* called by Linux when module is uninstalled */
void
cleanup_module (void)
{
	int minor;

	for (minor = 0; minor < MAXCAMS; minor++) {
	    CAMDATA *cdp = &camdata[minor];
	    int base;

	    if (ap_state[minor] == AP_ABSENT)
		continue;
	    base = apg[minor][APP_IOBASE];
	    stopExpTimer(minor);
#if (LINUX_VERSION_CODE >= 0x020200)
            wake_up_interruptible (&ap_wq[minor]);
#else
	    stopSelectTimer(minor);
#endif
	    if (ap_state[minor] == AP_EXPOSING)
		apAbortExposure(minor);
	    release_region (base, AP_IO_EXTENT);
	    ap_state[minor] = AP_ABSENT;
	    vfree (cdp->tmpbuf);
	}

	if (ap_slbase) {
	    release_region (ap_slbase, AP_IO_EXTENT);
	    ap_slbase = 0;
	}

	unregister_chrdev (AP_MJR, ap_name);
	proc_unregister (&proc_root, ap_proc_entry.low_ino);
	printk ("%s: module removed\n", ap_name);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: main.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.3 $ $Name:  $"};
