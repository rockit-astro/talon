/* code to handle drift scanning.
 * (C) Copyright 1999 Elwood Charles Downey. All right reserved.
 */

#ifdef TRY_DRIFTSCAN

#include "apdata.h"
#include "apglob.h"
#include "apccd.h"
#include "aplow.h"
#include "aplinux.h"

#define	DSSZ	sizeof(CCDDriftScan)	/* bytes in CCDDriftScan struct */
#define	USPJ	(1000000/HZ)		/* microsecs per jiffy */

/* info about drift scanning on one camera */
typedef struct {
    /* basic circumstances */
    int nrows, ncols;		/* full frame dimension */

    /* info for waiting between rows */
    long fstart;		/* jiffies when frame started, or 0 when idle */
    int last_rowint;		/* to note if rowint changes and reset start */
    struct timer_list tl;	/* row-delay completion timer list */
    int timeron;		/* set when tl is in use */

    /* the following all refer to a CCDDriftScan in user space */
    USHORT *uarray;
    int *urow;
    int *urowint;
} DSInfo;
static DSInfo dsinfo[MAXCAMS];

static struct wait_queue *ds_wq[MAXCAMS];	/* scan completion wait q */
static int dserrno = -1;			/* set when trouble */

#define offof(s,m)	((int)&((s*)0)->m)	/* offset-of */

static void startTimer (int minor);
static void nextRowTimerComplete (int minor);
static int readRow (int minor, USHORT *outbuf);
static void stopTimer (DSInfo *dsip);
static void wait4DSQ(int minor);
static void wakeupDSQ (unsigned long minor);

#if (LINUX_VERSION_CODE >= 0x020200)
#define	GETU(lval,addr)	(void) get_user (lval, addr)
#else
#define GETU(lval,addr) lval = get_user (addr)
#endif /* (LINUX_VERSION_CODE >= 0x020200) */

/* set up to do a drift scan. arg is user's addr of a CCDDriftScan.
 * return 0 if ok else -errno.
 */
int
setupDriftScan (int minor, unsigned long arg)
{
	DSInfo *dsip = &dsinfo[minor];

	/* get user's CCDDriftScan as individual user-space references */
#if (LINUX_VERSION_CODE >= 0x020200)
	if (!access_ok(VERIFY_WRITE, (const void *)arg, DSSZ))
	    return -EFAULT;
#else
	dserrno = verify_area(VERIFY_WRITE, (const void *)arg, DSSZ);
	if (dserrno)
	    return (dserrno);
#endif /* (LINUX_VERSION_CODE >= 0x020200) */
	dsip->urow = (int *)(arg + offof(CCDDriftScan, row));
	dsip->urowint = (int *)(arg + offof(CCDDriftScan, rowint));

	/* ready */
	return (0);
}

/* perform a drift scan.
 * return 0 if we do not care to do a drift scan, -errno on error.
 * if we do care to try we do not return until signaled or h/w error.
 * N.B. setupDriftScan must have been called prior. if we decide to fail, it
 *   is marked not valid and must be called again.
 */
int
doDriftScan (int minor, char *buf, int count)
{
	CAMDATA *cdp = &camdata[minor];
	DSInfo *dsip = &dsinfo[minor];
	int ccd_handle = MIN2HCCD(minor);
	int index = ccd_handle - 1;
	int nbytes;

	/* check whether even interested */
	if (dserrno)
	    return (0);

	/* confirm the user's buf is valid for full-frame */
	dsip->nrows = cdp->rows - (cdp->topb + cdp->botb);
	dsip->ncols = cdp->cols - (cdp->leftb + cdp->rightb);
	nbytes = dsip->nrows * dsip->ncols * 2;
	if (count < nbytes)
	    return (dserrno = -EFAULT);
#if (LINUX_VERSION_CODE >= 0x020200)
	if (!access_ok(VERIFY_WRITE, buf, nbytes))
	    return -EFAULT;
#else
	dserrno = verify_area(VERIFY_WRITE, buf, nbytes);
	if (dserrno)
	    return (dserrno);
#endif /* (LINUX_VERSION_CODE >= 0x020200) */

	/* looks good, start the action */
	dsip->uarray = (USHORT *)buf;
	dsip->fstart = jiffies;
	GETU (dsip->last_rowint, dsip->urowint);
	put_user (0, dsip->urow);
	if (camdata[index].caching)
	    set_fifo_caching(ccd_handle, TRUE);
	startTimer (minor);

	/* run until interrupted or trouble */
	wait4DSQ(minor);
	if (camdata[index].caching)
	    set_fifo_caching(ccd_handle, FALSE);
	return (dserrno);
}

/* stop doing drift scanning, if any */
void
stopDriftScan (int minor)
{
	DSInfo *dsip = &dsinfo[minor];

	if (dsip->fstart) {
	    dsip->fstart = 0;
	    stopTimer (dsip);
	}
	dserrno = -1;	/* just disable until set up again */
}

/* start timer that calls nextRowTimerComplete() when ready for next row */
static void
startTimer (int minor)
{
	DSInfo *dsip = &dsinfo[minor];
	struct timer_list *tlp = &dsip->tl;
	int rowint, row;
	long next, diff;

	GETU (rowint, dsip->urowint);
	GETU (row, dsip->urow);

	/* stop any existing timer.. not likely, just paranoid. */
	stopTimer (dsip);

	/* if rowint changes, resync fstart to fake whole frame at new rate */
	if (rowint != dsip->last_rowint) {
	    dsip->last_rowint = rowint;
	    dsip->fstart = jiffies - row*rowint/USPJ;
	}

	/* compute next delay */
	next = dsip->fstart + (row+1)*rowint/USPJ;
	diff = next - jiffies;
#if 0
	if (diff < 0) {
	    printk ("apogee[%d]: drift rate is too fast for camera: %ld ms\n",
								minor, diff*10);
	    dserrno = -EINVAL;
	    wakeupDSQ (minor);
	    return;
	}
#endif

	/* start timer */
	init_timer (tlp);
	tlp->expires = next;
	tlp->function = wakeupDSQ;
	tlp->data = (unsigned long)minor;
	dsip->timeron = 1;
	add_timer (tlp);
}

/* timer expired, read next row.
 * then set up timer again for next row.
 */
static void
nextRowTimerComplete (minor)
{
	DSInfo *dsip = (DSInfo *)&dsinfo[minor];
	USHORT *outbuf;
	int row;

	GETU (row, dsip->urow);
	outbuf = dsip->uarray + row*dsip->ncols; /* *2 implied */

	/* read and store a new row .. just stop if trouble */
	if (readRow(minor, outbuf) < 0) {
	    dserrno = -EIO;
	    wakeupDSQ (minor);
	    return;
	}

	/* increment row. wrap when finish chip */
	if (++row == dsip->nrows) {
#ifdef FLUSH_AFTER_EACH
	    int ccd_handle = MIN2HCCD(minor);
	    int index = ccd_handle - 1;

	    set_fifo_caching(ccd_handle,FALSE);
	    set_shutter (ccd_handle, FALSE);
	    set_shutter_open (ccd_handle, 0);
	    load_line_count (ccd_handle, camdata[index].rows);
	    ccd_command(ccd_handle, CCD_CMD_RSTSYS, BLOCK, 5);
	    ccd_command(ccd_handle, CCD_CMD_FLSTRT, BLOCK, 5);
	    poll_frame_done (ccd_handle, 10);
	    set_shutter (ccd_handle, FALSE);
	    set_shutter_open (ccd_handle, 1);
	    ccd_command(ccd_handle, CCD_CMD_FLSTOP, TRUE, 10);
	    set_fifo_caching(ccd_handle,TRUE);
#endif /* FLUSH_AFTER_EACH */

	    row = 0;
	    dsip->fstart = jiffies;
	}

	/* update user's row number */
	put_user (row, dsip->urow);

	/* repeat */
	startTimer (minor);
}

static int
readRow (int minor, USHORT *outbuf)
{
	DSInfo *dsip = (DSInfo *)&dsinfo[minor];
	int ccd_handle = MIN2HCCD(minor);
	int index = ccd_handle - 1;
	CAMDATA *cdp = &camdata[index];
	int base = cdp->base;
	SHORT pixbias = cdp->pixbias;
	int ncols = ap_slbase ? dsip->ncols/2 : dsip->ncols;
	int i;

	if (camdata[index].caching) {
	    ccd_command(ccd_handle, CCD_CMD_LNSTRT, BLOCK, 5);
	    readu_data (base, cdp, outbuf, ncols, ncols);
	    outbuf += ncols;
	    if (ap_slbase) {
		readu_data (ap_slbase, cdp, outbuf, ncols, ncols);
		outbuf += ncols;
	    }
	    assert_done_reading(ccd_handle);
	    if (poll_line_done(ccd_handle,10) != TRUE)
		return (-1);;
	} else {
	    ccd_command(ccd_handle, CCD_CMD_LNSTRT, BLOCK, 5);
	    if (poll_line_done(ccd_handle,10) != TRUE)
		return (-1);;
	    readu_data (base, cdp, outbuf, ncols, ncols);
	    outbuf += ncols;
	    if (ap_slbase) {
		readu_data (ap_slbase, cdp, outbuf, ncols, ncols);
		outbuf += ncols;
	    }
	}

	return (0);
}

/* remove a pending timer */
static void
stopTimer (DSInfo *dsip)
{
	if (dsip->timeron) {
	    del_timer (&dsip->tl);
	    dsip->timeron = 0;
	}
}

/* keep sleeping until something happens to stop drift scan, ie, a signal or
 * some hardware problem, as indicated by something set in dserrno.
 */
static void
wait4DSQ(int minor)
{
	while (1) {
	    interruptible_sleep_on (&ds_wq[minor]);
	    if (dserrno)
		break;
#if (LINUX_VERSION_CODE >= 0x020200)
	    if (signal_pending(current)) {
#else
	    if (current->signal & ~current->blocked) {
#endif
		dserrno = -EINTR;
		break;
	    }
	    nextRowTimerComplete (minor);
	    if (dserrno)
		break;
	}
}

/* wakeup doDriftScan */
static void
wakeupDSQ (unsigned long minor)
{
	wake_up_interruptible (&ds_wq[minor]);
}

#endif /* TRY_DRIFTSCAN */

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: apdscan.c,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $"};
