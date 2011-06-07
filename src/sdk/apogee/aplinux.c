/* glue between the linux driver and the (almost) generic Apogee API functions.
 * (C) Copyright 1997 Elwood Charles Downey. All right reserved.
 * 6/25/97
 */

#include "apdata.h"
#include "apglob.h"
#include "apccd.h"
#include "aplow.h"

#include "aplinux.h"

static void apPrError(int minor);

/* probe for a camera at the given address.
 * return 1 if the device appears to be present, else return 0
 */
int
apProbe(int base)
{
	return (loopback(base) == CCD_OK ? 1 : 0);
}

/* set the camdata[] entry for the given camera from the module params.
 * since we can't actually read the camera type from the hardware, these
 * could be entirely bogus and we'd never know.
 * we set up the ROI for full chip (minus a 2-bit border), 1:1 binning.
 * return 0 if ok, else -1
 */
int
apInit (int minor, int params[])
{
	int hccd = MIN2HCCD(minor);
	CAMDATA *cdp = &camdata[minor];
	SHORT rows = (SHORT) params[APP_ROWS];
	SHORT cols = (SHORT) params[APP_COLS];
	SHORT topb = (SHORT) params[APP_TOPB];
	SHORT botb = (SHORT) params[APP_BOTB];
	SHORT leftb = (SHORT) params[APP_LEFTB];
	SHORT rightb = (SHORT) params[APP_RIGHTB];

	if (ap_trace) {
	    printk ("Ap @ 0x%x config params:\n", params[APP_IOBASE]);
	    printk ("  %5d %s\n", params[APP_ROWS],      "APP_ROWS");
	    printk ("  %5d %s\n", params[APP_COLS],      "APP_COLS");
	    printk ("  %5d %s\n", params[APP_TOPB],      "APP_TOPB");
	    printk ("  %5d %s\n", params[APP_BOTB],      "APP_BOTB");
	    printk ("  %5d %s\n", params[APP_LEFTB],     "APP_LEFTB");
	    printk ("  %5d %s\n", params[APP_RIGHTB],    "APP_RIGHTB");
	    printk ("  %5d %s\n", params[APP_TEMPCAL],   "APP_TEMPCAL");
	    printk ("  %5d %s\n", params[APP_TEMPSCALE], "APP_TEMPSCALE");
	    printk ("  %5d %s\n", params[APP_TIMESCALE], "APP_TIMESCALE");
	    printk ("  %5d %s\n", params[APP_CACHE],     "APP_CACHE");
	    printk ("  %5d %s\n", params[APP_CABLE],     "APP_CABLE");
	    printk ("  %5d %s\n", params[APP_MODE],      "APP_MODE");
	    printk ("  %5d %s\n", params[APP_TEST],      "APP_TEST");
	    printk ("  %5d %s\n", params[APP_GAIN],      "APP_GAIN");
	    printk ("  %5d %s\n", params[APP_OPT1],      "APP_OPT1");
	    printk ("  %5d %s\n", params[APP_OPT2],      "APP_OPT2");
	}

	/* borders must be at least two for clocking reasons.
	 * well ok, only need 1 if there's no cache.. so sue me.
	 */
	if (topb < 1 || botb < 1 || leftb < 1 || rightb < 1 ||
			    rows < topb + botb || cols < leftb + rightb) {
	    printk ("Ap @ 0x%x: Bad border params\n", params[APP_IOBASE]);
	    return (-1);
	}

	cdp->base = params[APP_IOBASE];
	cdp->handle = hccd;
	cdp->mode = params[APP_MODE];
	cdp->test = params[APP_TEST];
	cdp->rows = rows;
	cdp->cols = cols;
	cdp->topb = topb;
	cdp->botb = botb;
	cdp->leftb = leftb;
	cdp->rightb = rightb;
	cdp->imgrows = rows - (topb + botb);
	cdp->imgcols = cols - (leftb + rightb);
	cdp->rowcnt = cdp->imgrows;	/* initial only */
	cdp->colcnt = cdp->imgcols;	/* initial only */
	cdp->bic = leftb;		/* initial only */
	cdp->aic = rightb;		/* initial only */
	cdp->hbin = 1;			/* initial only */
	cdp->vbin = 1;			/* initial only */
	cdp->bir = topb;			/* initial only */
	cdp->air = botb;		/* initial only */
	cdp->shutter_en = 1;		/* initial only */
	cdp->trigger_mode = 0;
	cdp->caching = params[APP_CACHE];
	cdp->timer = 10; 			/* multiples of .01 secs */
	cdp->tscale = params[APP_TIMESCALE];	/* * FP_SCALE */
	cdp->cable = params[APP_CABLE];
	cdp->temp_cal = params[APP_TEMPCAL];
	cdp->temp_scale = params[APP_TEMPSCALE];/* * FP_SCALE */
	cdp->camreg = 0;
        if (params[APP_GAIN])
            cdp->camreg |= 0x08;
        if (params[APP_OPT1])
            cdp->camreg |= 0x04;
        if (params[APP_OPT2])
            cdp->camreg |= 0x02;
	cdp->error = 0;

	/* load the mode and test bits */
	if (load_mode_bits (hccd, cdp->mode) != CCD_OK) {
	    printk ("Failed to load MODE bits %d\n", cdp->mode);
	    apPrError (minor);
	    return (-1);
	}
	if (load_test_bits (hccd, cdp->test) != CCD_OK) {
	    printk ("Failed to load TEST bits %d\n", cdp->test);
	    apPrError (minor);
	    return (-1);
	}
	if (ap_trace) {
	    printk ("Loaded MODE bits: %d\n", cdp->mode);
	    printk ("Loaded TEST bits: %d\n", cdp->test);
	}

	/* load params and start camera flushing */
	if (load_ccd_data (hccd, FLUSH) != CCD_OK) {
	    apPrError (minor);
	    return (-1);
	}

	return (0);
}

/* save the given settings of CCDExpoParams into camdata[] for the given camera.
 * if ok, load registers, start flushing.
 * enforce the t/b/l/r border limits.
 * return 0 if ok, else -1
 */
int
apSetupExposure (int minor, CCDExpoParams *cep)
{
	CAMDATA *cdp = &camdata[minor];
	SHORT h = cdp->rows - (cdp->topb + cdp->botb);
	SHORT w = cdp->cols - (cdp->leftb + cdp->rightb);
	HCCD hccd = MIN2HCCD(minor);
	SHORT bic, bir, ic, ir, hb, vb;
	LONG t;

	/* sanity check based on real size minus border */
	if (cep->bx < 1 || cep->bx > MAX_HBIN
			|| cep->by < 1
			|| cep->by > MAX_VBIN
			|| cep->sx < 0
			|| cep->sx >= w
			|| cep->sy < 0
			|| cep->sy >= h
			|| cep->sw < 1
			|| cep->sx + cep->sw > w
			|| cep->sh < 1
			|| cep->sy + cep->sh > h)
	    return (-1);

	bir = (SHORT)cep->sy + cdp->topb;
	bic = (SHORT)cep->sx + cdp->leftb;
	ir  = (SHORT)cep->sh;
	ic  = (SHORT)cep->sw;
	hb  = (SHORT)cep->bx;
	vb  = (SHORT)cep->by;
	t   = (LONG)cep->duration/10;	/* in ms, want 100ths */

	/* not included in config_cam.. infer from duration and setup */
	cdp->shutter_en = cep->duration && cep->shutter != CCDSO_Closed;
	cdp->trigger_mode = t < 0 ? CCD_TRIGEXT : CCD_TRIGNORM;

	/* normalize exposure time.
	 * must at least 1 to prime the counter
	 */
	if (t < 0)
	    t = -t;
	if (t < 1)
	    t = 1;

	if (config_camera (hccd,
				IGNORE,		/* chip rows -- use camdata */
				IGNORE,		/* chip cols -- use camdata */
				bir,		/* before image rows: BIR */
				bic,		/* before image cols: BIC */
				ir,		/* chip AOI rows */
				ic,		/* chip AOI cols */
				hb,		/* h bin */
				vb,		/* v bin */
				t,		/* exp time, 100ths of sec */
				IGNORE,		/* cable length -- use camdata*/
				FALSE		/* no config file */
				) != CCD_OK) {
	    apPrError (minor);
	    return (-1);
	}
	    
	return (0);
}

/* get cooler info and current temp from the given camera, in degrees C.
 * return 0 if ok, else -1
 */
int
apReadTempStatus (int minor, CCDTempInfo *tp)
{
	int hccd = MIN2HCCD(minor);
	SHORT status;
	LONG newt;

	if (get_temp (hccd, &status, &newt) != CCD_OK) {
	    apPrError (minor);
	    return (-1);
	}

	switch (status) {
	case CCD_TMP_OK:	tp->s = CCDTS_AT;    break;
	case CCD_TMP_UNDER:	tp->s = CCDTS_UNDER; break;
	case CCD_TMP_OVER:	tp->s = CCDTS_OVER;  break;
	case CCD_TMP_ERR:	tp->s = CCDTS_ERR;   break;
	case CCD_TMP_OFF:	tp->s = CCDTS_OFF;   break;
	case CCD_TMP_RDN:	tp->s = CCDTS_RDN;   break;
	case CCD_TMP_RUP:	tp->s = CCDTS_RUP;   break;
	case CCD_TMP_STUCK:	tp->s = CCDTS_STUCK; break;
	case CCD_TMP_MAX:	tp->s = CCDTS_MAX;   break;
	case CCD_TMP_DONE:	tp->s = CCDTS_AMB;   break;
	default:		tp->s = CCDTS_ERR;   break;
	}

	tp->t = newt;

	return (0);
}

/* set the indicated temperature, in degrees C, for the given camera.
 * return 0 if ok, else -1.
 */
int
apSetTemperature (int minor, int t)
{
	int hccd = MIN2HCCD(minor);

	if (set_temp (hccd, (double)t, CCD_TMP_SET) != CCD_OK) {
	    apPrError (minor);
	    return (-1);
	}
	return (0);
}

/* start a ramp up back to ambient for the given camera.
 * return 0 if ok, else -1.
 */
int
apCoolerOff (int minor)
{
	int hccd = MIN2HCCD(minor);

	if (set_temp (hccd, 0.0, CCD_TMP_AMB) != CCD_OK) {
	    apPrError (minor);
	    return (-1);
	}
	return (0);
}

/* start an exposure for the given camera using its current camdata.
 * return 0 if ok, else -1.
 */
int
apStartExposure(int minor)
{
	HCCD hccd = MIN2HCCD(minor);


	/* get shutter_en and trigger_mode from camdata;
	 * don't block, flush wait.
	 */
	if(start_exposure(hccd, IGNORE, IGNORE, FALSE, FALSE) != CCD_OK){
	    apPrError (minor);
	    return (-1);
	}

	return (0);
}

/* an exposure is complete, or nearly so. read the pixels into the given
 * _user space_ buffer which is presumed to long enough.
 * return 0 if ok, else -1
 */
int
apDigitizeCCD (int minor, unsigned short *buf)
{
	int hccd = MIN2HCCD(minor);

	/* don't bother to wait for after-image-rows to finish flushing */
	if (acquire_image (hccd, 0, buf, TRUE) != CCD_OK) {
	    apPrError (minor);
	    return (-1);
	}
	return (0);
}

/* abort an exposure. amounts to closing the shutter and resuming flushing.
 * return 0 if ok, else -1
 */
int
apAbortExposure (int minor)
{
	int hccd = MIN2HCCD(minor);

	set_shutter (hccd, FALSE);	/* manual shutter ctrl */
	set_shutter_open (hccd, FALSE);	/* close shutter */
	reset_flush (hccd);
        start_flushing (hccd);          /* resume flushing */
	if (ap_trace)
	    printk ("Exposure aborted\n");
	return (0);
}

/* read a row of pixel data and store it in the given user-space ubuf.
 * always perform exactly nreads, but only store up to ncols in ubuf[].
 * Use cdp->tmpbuf to store the row then copy in one chunk.
 * N.B. we assume nreads >= ncols because Apogee h/w rounds up, OCAAS rounds
 *   down.
 */
void
readu_data (int base, CAMDATA *cdp, USHORT *ubuf, int nreads, int ncols)
{
#define R09     8	/* originally in aplow.c */
	SHORT pixbias = cdp->pixbias;
	USHORT *ltb, *tb = cdp->tmpbuf;

	for (ltb = tb+ncols; tb < ltb; )
	    *tb++ = inpw(base + CCD_REG09) + pixbias;
	for (nreads -= ncols; --nreads >= 0; )
	    (void) inpw(base + CCD_REG09);

#if (LINUX_VERSION_CODE >= 0x020200)
	__copy_to_user (ubuf, cdp->tmpbuf, ncols*sizeof(USHORT));
#else
	memcpy_tofs(ubuf, cdp->tmpbuf, ncols*sizeof(USHORT));
#endif /* (LINUX_VERSION_CODE >= 0x020200) */
}

/* decode the error bits for the given device */
static void
apPrError(int minor)
{
	static char me[] = "apogee";
	int error = camdata[minor].error;

	if (error & CCD_ERR_BASE)
	    printk ("%s: invalid base I/O address passed to func\n", me);
	if (error & CCD_ERR_REG)
	    printk ("%s: register access operation error\n", me);
	if (error & CCD_ERR_SIZE)
	    printk ("%s: invalid CCD geometry\n", me);
	if (error & CCD_ERR_HBIN)
	    printk ("%s: invalid horizontal binning factor\n", me);
	if (error & CCD_ERR_VBIN)
	    printk ("%s: invalid vertical binning factor\n", me);
	if (error & CCD_ERR_AIC)
	    printk ("%s: invalid AIC value\n", me);
	if (error & CCD_ERR_BIC)
	    printk ("%s: invalid BIC value\n", me);
	if (error & CCD_ERR_OFF)
	    printk ("%s: invalid line offset value\n", me);
	if (error & CCD_ERR_SETUP)
	    printk ("%s: CCD controller sub-system not initialized\n", me);
	if (error & CCD_ERR_TEMP)
	    printk ("%s: CCD cooler failure\n", me);
	if (error & CCD_ERR_READ)
	    printk ("%s: failure reading image data\n", me);
	if (error & CCD_ERR_BUFF)
	    printk ("%s: invalid buffer pointer specfied\n", me);
	if (error & CCD_ERR_NOFILE)
	    printk ("%s: file not found or not valid\n", me);
	if (error & CCD_ERR_CFG)
	    printk ("%s: config. data invalid\n", me);
	if (error & CCD_ERR_HCCD)
	    printk ("%s: invalid CCD handle passed to function\n", me);
	if (error & CCD_ERR_PARM)
	    printk ("%s: invalid parameter passed to function\n", me);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: aplinux.c,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $"};
