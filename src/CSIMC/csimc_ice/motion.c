/* code to handle motion.
 * there are basically two entry points:
 *  onMotion, called periodically by the scheduler
 *  the various motion commands (mtpos, etc), called as user programs execute.
 * internal calculations are computed in motor steps.
 * "target" is the goal, "gun" is reality.
 */

#include "sa.h"

int n_st; /* stat of longest time between servicing */

#define CTPMU	32              /* min clock ticks per motion update.
				 * N.B. must be ^2
				 */
#define SPMU    (CTPMU*SPCT)    /* secs per motion update */
#define	ENCDT	500		/* encoder velocity sample period, ticks */

static long tnow; /* clock now */
static long lastnow; /* clock at last update */
static long timeout; /* clock tick when current effort expires */
static Byte countmask; /* implements onMotion run modulus */

static float Vg, Ag, limAg; /* max "gun" vel and acc, max limit acc */
static float Kv; /* velocity feedback coeffient */
static float xt, vt; /* target pos and vel now, counts */
static float lastxt; /* xt @ lastnow */
static float xg, vg; /* "gun" pos and vel now, counts */
static float goal; /* motor goal pos or vel */
static float goalerr; /* done when goal is within this */
static int ontrack; /* whether logically ontrack this time */
static int lastontrack; /* whether physically ontrack last time */

static float espms; /* signed enc steps per motor step */
static float vascale; /* handy CV_VASCALE already as a float */

static void (*updateFp)(void); /* function called on each update if WORKING */
static int stopping; /* set when stopping suddenly */

/* params for tracking */
static float *tr_tbl; /* malloced tr_ntbl positions */
static float *tr_deriv; /* malloced 2nd derivatives */
static int tr_ntbl; /* entries in tr_tbl[] and tr_deriv[] */
static long tr_start; /* clock tick of start of tr_tbl */
static long tr_end; /* clock tick of end of tr_tbl */
static float tr_ipct; /* position intervals per clock tick */
static float tr_scale; /* pos -> mot steps */

/* fiducials for maintaining encoder vel */
static long ep[3], et[3]; /* history of encoder pos and time */

static void refreshEv(void);
static int hitLimit(void);
static void getEv(float *xp);
static void tr_getDerivatives(void);
static void tr_EDSpline(long dt, float *xp);
static void updateStopping(void);

typedef enum
{
	W_TO = -1, W_IDLE, W_RUN
} Working;

/* get motor vel now */
static void getMv(float *vp)
{
	Long L;

	getMotVel(&L);
	*vp = getMotDir() * L.l / MOTCONRATE;
}

/* called regularly to update commanded velocity and monitor encoder motion.
 * N.B. not reentrant and must not use pti
 */
void onMotion(void)
{
	int clockdt;
	Byte b0;

	/* always check for hitting limits after every instruction.
	 * otherwise only run every CPTMU ticks, ie, when CPTMU bit changes.
	 */
	if (hitLimit() && !stopping)
		updateStopping();
	b0 = clocktick.B.b0 & CTPMU;
	if (b0 == countmask)
		return;
	countmask = b0;

	/* refresh */
	checkStack(3);
	lastnow = tnow;
	tnow = getClock();
	refreshEv();

	/* collect max interval for stats() */
	clockdt = tnow - lastnow;
	if (clockdt > n_st)
		n_st = clockdt;

	/* if see limit: stop quick and abandon current work
	 * else if working: continue or time out and note when done.
	 */
	if (stopping)
		updateStopping();
	else if (cvgw(CV_WORKING) == W_RUN)
	{
		if (tnow > timeout)
		{
			cvsw(CV_WORKING, W_TO);
			cvsw(CV_ONTRACK, 0);
			updateStopping();
		}
		else
			(*updateFp)();
	}
}

/* called to get motor pos */
void gmpos(VType *vp)
{
	Long L;

	getMotPos(&L);
	*vp = L.l;
}

/* called to get motor vel */
void gmvel(VType *vp)
{
	float mv;

	getMv(&mv);
	*vp = f2l(mv * vascale);
}

/* called to get motor direction */
void gmdir(VType *vp)
{
	*vp = getMotDir();
}

/* called to get encoder pos */
void gepos(VType *vp)
{
	Long L;

	getEncPos(&L);
	*vp = L.l;
}

/* called to get encoder vel */
void gevel(VType *vp)
{
	float ev;

	getEv(&ev);
	*vp = f2l(ev * espms * vascale);
}

/* refresh the encoder velocity fiducials */
static void refreshEv(void)
{
	Long L;
	long dt;

	/* update latest */
	getEncPos(&L);
	ep[0] = L.l;
	et[0] = tnow;

	/* replace oldest if ENCDT elapsed or clock was reset */
	dt = tnow - et[1];
	if (dt > (ENCDT / 2) || dt <= 0)
	{
		ep[2] = ep[1];
		et[2] = et[1];
		ep[1] = ep[0];
		et[1] = et[0];
	}
}

/* return 1 if moving and limits are a problem, else 0 */
static int hitLimit(void)
{
	Long L;

	getMotVel(&L);
	if (L.H.u || L.H.l)
		return (atLimit(getMotDir()));
	return (0);
}

/* call before starting a new command with desired motor direction.
 * checks for current and desired limits.
 * return 1 if ok to proceed, else 0
 */
static int newCmdOk(int newdir)
{
	/* check limits reality, and wait if need to stop */
	while (stopping)
		scheduler(0);

	/* check desired direction */
	if (atLimit(newdir))
		return (0);

	/* ok, we can go our way and presume the other way is ok too */
	resetLimit(-newdir);
	cvsw (CV_WORKING, W_RUN);
	cvsw(CV_ONTRACK, 0);
	timeout = tnow + f2l(cvg(CV_TIMEOUT) / MSPCT);
	return (1);
}

/* collect user values */
static void getUser(void)
{
	/* get encoder sign and scaling */
	espms = (float) cvg(CV_ESIGN) * cvg(CV_ESTEPS) / cvg(CV_MSTEPS);

	/* acc and vel */
	vascale = (float) cvg(CV_VASCALE);
	Ag = cvg(CV_MAXACC) / vascale;
	Vg = cvg(CV_MAXVEL) / vascale;
	Kv = cvg(CV_KV) / 1000.0;
	limAg = cvg(CV_LIMACC) / vascale;
}

/* get motor pos now */
static void getMx(float *xp)
{
	Long L;

	getMotPos(&L);
	*xp = L.l;
}

/* set motor vel */
static void setMv(float v)
{
	int sign = fsign(v);
	Long L;

	setMotDir(sign);
	L.l = sign * f2l(v * MOTCONRATE); /* abs value :) */
	setMotVel(&L);
}

/* get encoder pos now in motor steps from 0 */
static void getEx(float *xp)
{
	Long L;

	getEncPos(&L);
	*xp = L.l / espms;
}

/* get encoder vel now in motor steps/sec */
static void getEv(float *vp)
{
	*vp = (ep[0] - ep[2]) / (espms * SPCT * (et[0] - et[2]));
}

/* fake out a 0-speed encoder */
static void forceEv0(void)
{
	ep[2] = ep[1] = ep[0];
}

/* return 1 if now is within goalerr of goal, else 0 */
static int atGoal(float now)
{
	return (fabs(now - goal) <= goalerr);
}

/* enforce -lim <= *valp <= lim
 * return 1 if had to change, else 0.
 */
static int clamp(float *valp, float lim)
{
	if (*valp > lim)
	{
		*valp = lim;
		return (1);
	};
	if (*valp < -lim)
	{
		*valp = -lim;
		return (1);
	};
	return (0);
}

/* compute next gun velocity, vg for case of having a target position.
 *   in: lastnow, lastxt, xg, xt
 *  out: vt, lastxt <- xt, vg
 */
static void computeVG()
{
	float lastvg;
	float acc;

	/* biggish stack user */
	checkStack(5);

	/* compute target vel, counts per sec */
	vt = (xt - lastxt) / ((tnow - lastnow) * SPCT);
	lastxt = xt;

	/* new gun velocity to hit target */
	lastvg = vg;
	vg = vt - Kv * Ag / Vg * (xg - xt);

	/* constrain vg to Vg and Ag */
	(void) clamp(&vg, Vg);
	acc = (vg - lastvg) / SPMU;
	if (clamp(&acc, Ag))
		vg = lastvg + acc * SPMU;
}

/* shared mtpos and etpos update service. clear CV_WORKING when finished */
static void updateTPOS(void)
{
	if (atGoal(xg))
	{
		/* TODO: check for low speed too? */
		setMv(0.0);
		cvsw(CV_WORKING, W_IDLE);
		cvsw(CV_ONTRACK, 0);
		return;
	}

	computeVG();
	setMv(vg);
}

/* mtpos update service. clear CV_WORKING when finished */
static void updateMTPOS(void)
{
	/* done when on target and stopped */
	getMx(&xg);
	updateTPOS();
}

/* etpos update service. clear CV_WORKING when finished */
static void updateETPOS(void)
{
	/* done when on target and stopped */
	getEx(&xg);
	updateTPOS();
}

/* special dedicated version of stopping that does not mess up other
 * state stuff.. it just bears down hard to 0 vel.
 * to abandon current work, we clear CV_WORKING.
 * sets stopping until finished.
 */
static void updateStopping(void)
{
	float dt, acc;

	/* get user settings first time */
	if (!stopping)
	{
		stopping = 1;
		getUser();
		getMv(&vg);
	}

	/* think of this as like updateTVEL with goal and goalerr = 0 */
	dt = SPCT * (tnow - lastnow);
	acc = -vg / dt;
	vg = clamp(&acc, limAg) ? vg + acc * dt : 0;
	setMv(vg);

	/* satisfied with nothing less than perfect stop */
	if (vg == 0)
	{
		stopping = 0;
		forceEv0(); /* since it is timed it won't keep up */
		if (cvgw(CV_WORKING) == W_RUN)
		{ /* avoid hammering W_TO */
			cvsw (CV_WORKING, W_IDLE);
			cvsw(CV_ONTRACK, 0);
		}
	}
}

/* shared mtvel and etvel update service. clear CV_WORKING when finished.
 * getFp is function to call to get current vg.
 */
static void updateTVEL(void(*getFp)())
{
	float vnew;
	float dt, acc;

	dt = SPCT * (tnow - lastnow);
	vnew = goal;
	acc = (vnew - vg) / dt;
	if (clamp(&acc, Ag))
		vnew = vg + acc * dt;
	setMv(vnew);
	(*getFp)(&vg);

	if (atGoal(vnew))
	{
		/* if close, then just nail it */
		setMv(goal);
		cvsw (CV_WORKING, W_IDLE);
		cvsw(CV_ONTRACK, 0);
	}
}

/* mtvel update service. clear CV_WORKING when finished. */
static void updateMTVEL(void)
{
	updateTVEL(getMv);
}

/* etvel update service. clear CV_WORKING when finished. */
static void updateETVEL(void)
{
	updateTVEL(getEv);
}

/* stuff in common when setting up a target position goal */
static int setupTPOS(VType p)
{
	if (!newCmdOk(fsign(goal - xg)))
		return (-1);
	lastxt = xt = goal;
	return (0);
}

/* called when mtpos is set. target motor step is in *pp */
int mtpos(VType *pp)
{
	/* set goals and initial conditions */
	getUser();
	goalerr = 0.499;
	goal = *pp;
	getMx(&xg);
	getMv(&vg);
	if (setupTPOS(*pp) < 0)
		return (-1);
	updateFp = updateMTPOS;

	/* ok.. our action begins at next loop */
	return (0);
}

/* called when etpos is set. target encoder count is in *pp */
int etpos(VType *pp)
{
	/* set goals and initial conditions, all in motor steps.
	 * can't do better than motor though.
	 */
	getUser();
	goalerr = fabs(espms) > 1.0 ? 0.499 : fabs(0.499 / espms);
	goal = *pp / espms;
	getEx(&xg);
	getEv(&vg);
	if (setupTPOS(*pp) < 0)
		return (-1);
	updateFp = updateETPOS;

	/* ok.. our action begins at next loop */
	return (0);
}

/* stuff in common when setting up a target velocity goal */
static int setupTVEL(VType v)
{
	if (fabs(goal) > Vg || !newCmdOk(fsign(goal)))
		return (-1);
	return (0);
}

/* called when mtvel is set. target motor step rate @ VASCALE is in *vp */
int mtvel(VType *vp)
{
	/* set goals and initial conditions */
	getUser();
	goalerr = 1. / vascale;
	goal = *vp * goalerr;
	if (setupTVEL(*vp) < 0)
		return (-1);
	getMv(&vg);
	updateFp = updateMTVEL;

	/* ok.. our action begins at next loop */
	return (0);
}

/* called when etvel is set. target encoder step rate @ VASCALE is in *vp */
int etvel(VType *vp)
{
	/* set goals and initial conditions */
	getUser();
	goalerr = 1. / (espms * vascale);
	goal = *vp * goalerr;
	goalerr = fabs(goalerr);
	if (setupTVEL(*vp) < 0)
		return (-1);
	getEv(&vg);
	updateFp = updateETVEL;

	/* ok.. our action begins at next loop */
	return (0);
}

/* shared mtrack and etrack update service.
 * updatePfp is used until tnow reaches tr_start.
 * getXfp is function to get current gun position.
 * clear CV_WORKING when finished.
 */
static void updateTK(void(*updateFp)(), void(*getXfp)())
{
	if (tnow < tr_start)
	{
		/* act like m/etpos until we are ready to start */
		(*updateFp)();
		cvsw(CV_WORKING, W_RUN); /* reinstate in case it succeeded */
		cvsw (CV_ONTRACK, 0);
	}
	else if (tnow >= tr_end)
	{
		/* let mtvel bring us to a nice stop */
		VType z = 0;
		(void) mtvel(&z);
		cvsw (CV_ONTRACK, 0);
	}
	else
	{
		/* in the active period.. interpolate and proceed */
		int ot;

		/* follow spline */
		tr_EDSpline(tnow - tr_start, &xt);
		(*getXfp)(&xg);
		computeVG();
		setMv(vg);

		/* claim on-track if 2 in a row on, off-track if 2 in a row off */
		ot = (fabs(xt - xg) <= tr_scale);
		if (ot == lastontrack)
			ontrack = ot;
		cvsw (CV_ONTRACK, ontrack);
		lastontrack = ot;
	}

	/* update target variables -- no reason, just for monitoring */
	cvs (CV_MTPOS, f2l(xt));
	cvs (CV_MTVEL, f2l(vt*vascale));
	cvs (CV_ETPOS, f2l(espms*xt));
	cvs (CV_ETVEL, f2l(espms*vt*vascale));
}

/* mtrack update service. clear CV_WORKING when finished. */
static void updateMTK(void)
{
	updateTK(updateMTPOS, getMx);
}

/* etrack update service. clear CV_WORKING when finished. */
static void updateETK(void)
{
	updateTK(updateETPOS, getEx);
}

/* given a set of steps, each at dt ms intervals starting at t0 (in ms), follow.
 * positions are WRT encoder counts if enc is != 0, else WRT motor steps.
 * return 0 if looks reasonable, else -1.
 * N.B. pos[] are in reverse time order, eg, pos[0] is oldest entry in table.
 * N.B. we assume npos >= 2 and if enc then esteps > 0.
 */
int startTrack(int enc, VType t0, VType dtms, VType pos[], int npos)
{
	static char nomem[] = "No memory\n";
	int i;

	/* set up */
	getUser();
	tr_scale = (enc ? 1. / espms : 1.);
	cvsw (CV_ONTRACK, 0);

	/* always add 1 point on each end so spline works over entire pos[].
	 * (EDSpline can not work in first or last segment)
	 * this also incidently lets us support npos down to 2.
	 */
	tr_ntbl = npos + 2;

	/* set up timing interval in clock ticks */
	tr_ipct = MSPCT / dtms;
	tr_start = f2l(t0 / MSPCT);
	tr_end = tr_start + f2l((npos - 1) / tr_ipct);
	if (getClock() >= tr_end)
	{
		printf("Too late\n");
		return (-1);
	}
	timeout = tr_start + f2l(cvg(CV_TIMEOUT) / MSPCT);

	/* get mem for table and derivatives */
	if (tr_tbl)
		free((void *) tr_tbl);
	tr_tbl = (float *) malloc(tr_ntbl * sizeof(float));
	if (!tr_tbl)
	{
		printf(nomem);
		return (-1);
	}
	if (tr_deriv)
		free((void *) tr_deriv);
	tr_deriv = (float *) malloc(tr_ntbl * sizeof(float));
	if (!tr_deriv)
	{
		free((void *) tr_tbl);
		tr_tbl = 0;
		printf(nomem);
		return (-1);
	}

	/* copy positions into table in units of motor steps,
	 * in reverse order to get increasing time,
	 * and leave room for new 1st and last entries.
	 */
	for (i = 0; i < npos; i++)
		tr_tbl[i + 1] = tr_scale * pos[npos - 1 - i];

	/* fake 1st and last from respective end slopes */
	tr_tbl[0] = 2 * tr_tbl[1] - tr_tbl[2];
	tr_tbl[tr_ntbl - 1] = 2 * tr_tbl[tr_ntbl - 2] - tr_tbl[tr_ntbl - 3];

	/* build table of derivatives */
	tr_getDerivatives();

#ifdef DUMP_INTERP
#define	NDINTERP	5
	for (i=0; i<=(npos-1)*NDINTERP; i++)
	{
		long tt = i*((npos-1)/tr_ipct)/((npos-1)*NDINTERP);
		float xx;
		tr_EDSpline (tt, &xx);
		printf ("%10ld %10ld\n", f2l(tt*MSPCT), f2l(xx/tr_scale));
	}
#endif /* DUMP_INTERP */

	/* if early, get in place by setting up like mtpos() */
	goalerr = fabs(tr_scale);
	if (tnow < tr_start)
	{
		VType startpos = pos[npos - 1] + cvg(CV_TOFFSET);
		if ((enc ? etpos(&startpos) : mtpos(&startpos)) < 0)
			return (-1);
	}

	/* set up for repeated attention.
	 * init vg once, then computeVG() maintains.
	 */
	if (enc)
	{
		updateFp = updateETK;
		getEv(&vg);
	}
	else
	{
		updateFp = updateMTK;
		getMv(&vg);
	}
	cvsw (CV_WORKING, W_RUN);
	lastontrack = ontrack = 0;

	/* ok.. our action begins at next loop */
	return (0);
}

/* stop motion and interrupt thread responsible.
 * if emerg really stop NOW, else stop @ LIMACC
 */
void killMotion(int emerg)
{
	/* savage if real emergency */
	if (emerg)
	{
		/* NOW! */
		setMv(0.0);
		cvsw(CV_WORKING, W_IDLE);
		cvsw(CV_ONTRACK, 0);
		return;
	}

	/* whoa fella */
	updateStopping();
}

/* update our tnow when clockticks reset */
void syncNow(void)
{
	lastnow = tnow = getClock();
}

/* build tr_deriv[] */
static void tr_getDerivatives(void)
{
#define	D_A	0.26794919
#define	D_B	0.53589838
#define	D_C	0.5

	int i;

	checkStack(4);

	for (i = 1; i < tr_ntbl; i++)
		tr_deriv[i] = tr_tbl[i] - tr_tbl[i - 1];

	for (i = 1; i < (tr_ntbl - 1); i++)
		tr_deriv[i - 1] = 3 * (tr_deriv[i + 1] - tr_deriv[i]);

	for (i = 1; i < tr_ntbl - 2; i++)
		tr_deriv[i] -= D_A * tr_deriv[i - 1];

	tr_deriv[tr_ntbl - 3] *= D_B;
	for (i = tr_ntbl - 4; i >= 0; i--)
		tr_deriv[i] = (tr_deriv[i] - D_C * tr_deriv[i + 1]) * D_B;

#undef	D_A
#undef	D_B
#undef	D_C
}

/* heavily compressed version of Spline() for equidistant h=1 case, Namir C.
 * Shammas, page 70.
 * return position in revs at dt clock ticks from tr_start.
 */
static void tr_EDSpline(long dt, float *xp)
{
	float delta1, delta2;
	float di1d2, did1;
	float di1, di;
	float t, x;
	int j, i;

	/* big stack user */
	checkStack(1);

	/* find interval */
	t = dt * tr_ipct;
	i = (int) t;
	delta1 = t - i;
	delta2 = 1.0 - delta1;

	/* move up one due to fake first entry */
	i = i + 1;
	j = i + 1;

	di1 = tr_deriv[i - 1];
	di = tr_deriv[i];
	di1d2 = di1 * delta2;
	did1 = di * delta1;
	x = (di1d2 * delta2 * delta2 + did1 * delta1 * delta1 - did1 - di1d2) / 6.0
			+ tr_tbl[j] * delta1 + tr_tbl[i] * delta2;

	/* return with offset added at one-step precision */
	*xp = (float) f2l(x + cvg(CV_TOFFSET) * tr_scale);
}

/*ICE=============== XDELTA ======================
 typedef enum xdop_enum
 {
 XDELTA_GET = 0,
 XDELTA_SET,
 XDELTA_ADD
 } xdop;

 int xdelta_value = 0;

 int xdelta(xdop xoperation, int value)
 {
 if (xoperation == XDELTA_SET)
 xdelta_value = value;
 else if (xoperation == XDELTA_ADD)
 xdelta_value += value;

 return xdelta_value;
 }
 ICE=============================================*/

/*ICE=============== XTRACK ======================*/

#define XFIFO_SIZE 8

typedef struct xitem_struct
{
	float t;
	float x;
} xitem;

volatile xitem xpos[XFIFO_SIZE];
volatile int xpos_size = 0; /* number of chars in xpos */
volatile int xpos_rp = 0; /* indice number of last read char */
volatile int xpos_wp = 0; /* indice number of last written char */

void xpos_init(void);
int xpos_full(void);
int xpos_empty(void);
void xpos_push(xitem* item);
void xpos_pop(xitem* item);

void xpos_init(void)
{
	xpos_size = 0;
	xpos_rp = 0;
	xpos_wp = 0;
	return;
}

int xpos_get_size(void)
{
	return XFIFO_SIZE - xpos_size;
}

/* adds an tiem */
void xpos_push(xitem* item)
{
	/* increase xpos_wp, check if at end of array */
	if (++xpos_wp >= XFIFO_SIZE)
		xpos_wp = 0;

	xpos[xpos_wp].t = item->t;
	xpos[xpos_wp].x = item->x;
	xpos_size++;

}

/* returns 1 if xpos is full, 0 if xpos is not full */
int xpos_full(void)
{
	return xpos_rp == xpos_wp && xpos_size == XFIFO_SIZE;
}
/* returns 1 if xpos is empty, 0 if xpos is not empty */
int xpos_empty(void)
{
	return xpos_rp == xpos_wp && xpos_size == 0;
}

/* pop item from queue */
void xpos_pop(xitem* item)
{
	if (++xpos_rp >= XFIFO_SIZE)
		xpos_rp = 0;

	item->t = xpos[xpos_rp].t;
	item->x = xpos[xpos_rp].x;

	/* fill 0 on place of read item so we can see it is removed */
	xpos[xpos_rp].t = 0;
	xpos[xpos_rp].x = 0;
	xpos_size--;

}

void xpos_add(VType time, VType pos)
{
	xitem item;
	item.t = time / SPCT;
	item.x = pos;
	xpos_push(&item);
	return;
}

volatile xitem xdelta[XFIFO_SIZE];
volatile int xdelta_size = 0; /* number of chars in xdelta */
volatile int xdelta_rp = 0; /* indice number of last read char */
volatile int xdelta_wp = 0; /* indice number of last written char */

void xdelta_init(void);
int xdelta_full(void);
int xdelta_empty(void);
void xdelta_push(xitem* item);
void xdelta_pop(xitem* item);

void xdelta_init(void)
{
	xdelta_size = 0;
	xdelta_rp = 0;
	xdelta_wp = 0;
	return;
}
int xdelta_get_size(void)
{
	return XFIFO_SIZE - xdelta_size;
}

/* adds an tiem */
void xdelta_push(xitem* item)
{
	/* increase xdelta_wp, check if at end of array */
	if (++xdelta_wp >= XFIFO_SIZE)
		xdelta_wp = 0;

	xdelta[xdelta_wp].t = item->t;
	xdelta[xdelta_wp].x = item->x;
	xdelta_size++;

}

/* returns 1 if xdelta is full, 0 if xdelta is not full */
int xdelta_full(void)
{
	return xdelta_rp == xdelta_wp && xdelta_size == XFIFO_SIZE;
}
/* returns 1 if xdelta is empty, 0 if xdelta is not empty */
int xdelta_empty(void)
{
	return xdelta_rp == xdelta_wp && xdelta_size == 0;
}

/* pop item from queue */
void xdelta_pop(xitem* item)
{
	if (++xdelta_rp >= XFIFO_SIZE)
		xdelta_rp = 0;

	item->t = xdelta[xdelta_rp].t;
	item->x = xdelta[xdelta_rp].x;

	/* fill 0 on place of read item so we can see it is removed */
	xdelta[xdelta_rp].t = 0;
	xdelta[xdelta_rp].x = 0;
	xdelta_size--;

}

void xdelta_add(VType time, VType delta)
{
	xitem item;
	item.t = time / SPCT;
	item.x = delta;
	xdelta_push(&item);
	return;
}

typedef enum xvar_enum
{
	XTRACK_VMEAN = 0,
	XTRACK_VEL,
	XTRACK_PERR,
	XTRACK_DELTA,
	XTRACK_TIME,
	XTRACK_LAUNCH_STATE,
	XTRACK_POSVEL,
	XTRACK_POS,
	XFILT0,
	XFILT1
} xvar;

static long xenc;
static long xp0;
static float xtick0;
static float xtick1;
static float xvel;
/*static float vg;*/
static float lastvg;
static float vmean;
static float perr;
static float xdp;
static float xdt;
static float xtime;
static float xposvel;
static float xdeltavel;
static float xfilt0;
static float xfilt1;

static xitem xpos0;
static xitem xpos1;
static xitem xdelta0;
static xitem xdelta1;

typedef enum xlaunch_enum
{
	XLAUNCH_GET_INITIAL_POS = 0,
	XLAUNCH_GET_LAUNCH_POS,
	XLAUNCH_SET_LAUNCH_POS,
	XLAUNCH_LAUNCH
};

static int xlaunch_state = XLAUNCH_GET_INITIAL_POS;
static float xlaunch_position;
static float xlaunch_ticks;
static float xtick_start;

static void xsetvar(int par, int val, int dec)
{
	float tmp;
	tmp = (float) val + ((float) dec) / 1000.0;

	if (par == XTRACK_VMEAN)
		vmean = tmp;
	else if (par == XTRACK_VEL)
		vg = tmp;
	else if (par == XTRACK_PERR)
		perr = tmp;
	else if (par == XTRACK_DELTA)
		xdt = tmp;
	else if (par == XTRACK_TIME)
		xtime = tmp;
	else if (par == XTRACK_LAUNCH_STATE)
		xlaunch_state = tmp;
	else if (par == XTRACK_POSVEL)
		xposvel = tmp;
	else if (par == XTRACK_POS)
		xg = tmp;
	else if (par == XFILT0)
		xfilt0 = tmp;
	else if (par == XFILT1)
		xfilt1 = tmp;
	return;
}

static void xgetvar(int par)
{
	float tmp;
	long val, dec;

	if (par == XTRACK_VMEAN)
		tmp = vmean;
	else if (par == XTRACK_VEL)
		tmp = vg;
	else if (par == XTRACK_PERR)
		tmp = perr;
	else if (par == XTRACK_DELTA)
		tmp = xdt;
	else if (par == XTRACK_TIME)
		tmp = xtime;
	else if (par == XTRACK_LAUNCH_STATE)
		tmp = xlaunch_state;
	else if (par == XTRACK_POSVEL)
		tmp = xposvel;
	else if (par == XTRACK_POS)
		tmp = xg;
	else if (par == XFILT0)
		tmp = xfilt0;
	else if (par == XFILT1)
		tmp = xfilt1;

	val = (long) tmp;
	dec = (long) (1000.0 * (tmp - (float) val));
	if (tmp < 0)

		printf("-%ld.%03ld\n", -val, -dec);
	else
		printf(" %ld.%03ld\n", val, dec);

	return;
}

static float xcalculate_pos(float tick)
{
	if (tick <= xpos0.t)
		return xpos0.x;

	if (tick >= xpos1.t)
	{
		if (!xpos_empty())
		{
			xpos0.t = xpos1.t;
			xpos0.x = xpos1.x;
			xpos_pop(&xpos1);
		}
		if (xpos1.t < xpos0.t)
			xpos1.t = xpos0.t;
		timeout = xpos1.t + f2l(cvg(CV_TIMEOUT) / MSPCT);
	}

	if (tick >= xpos1.t)
		return xpos1.x;
	if (xpos0.t == xpos1.t)
		return xpos0.x;
	return xpos0.x + ((xpos1.x - xpos0.x) * (tick - xpos0.t)) / (xpos1.t
			- xpos0.t);
}

static float xcalculate_delta(float tick)
{
	if (tick <= xdelta0.t)
		return xdelta0.x;

	if (tick >= xdelta1.t)
	{

		if (!xdelta_empty())
		{
			xdelta0.t = xdelta1.t;
			xdelta0.x = xdelta1.x;
			xdelta_pop(&xdelta1);
		}
		if (xdelta1.t < xdelta0.t)
			xdelta1.t = xdelta0.t;
	}

	if (tick >= xdelta1.t)
		return xdelta1.x;
	if (xdelta0.t == xdelta1.t)
		return xdelta0.x;
	return xdelta0.x + ((xdelta1.x - xdelta0.x) * (tick - xdelta0.t))
			/ (xdelta1.t - xdelta0.t);
}

void xsetMv(float vel)
{
	setMv(vel * cvg(CV_MSTEPS) / cvg(CV_ESTEPS)); /*set velocity in motor steps.*/
}

static int updateXPOS(long pos, float targeterror)
{
	float acc;

	if (fabs(xg - pos) <= targeterror)
	{
		perr = xg - pos;
		vg = 0;
		lastvg = vg;
		xsetMv(0.0);
		return 1;
	}
	else
	{
		perr = xg - pos;
		lastvg = vg;
		vg = -Kv * Ag / Vg * perr;

		/* constrain vg to Vg and Ag */
		(void) clamp(&vg, Vg);
		acc = (vg - lastvg) / SPMU;
		if (clamp(&acc, Ag))
			vg = lastvg + acc * SPMU;
		xsetMv(vg);

	}
	return 0;
}

#define XFINE_TUNE_POS 0.5
float xperr;
static float lastxt;
static void updateXVEL(float p0, float t0, float vel)
{
	float xtick;
	float acc;
	float esign = cvg(CV_ESIGN);
	float abserr;

	lastxt = (long) xt;
	lastvg = vg;

	xtick = tnow - t0;
	xtime = xtick * SPCT;
	xdp = xcalculate_pos(xtick);
	xdt = xcalculate_delta(xtick);
	xt = (vel * xtime) + xdp + xdt + (float) cvg(CV_XDELTA);
	xt *= esign;
	xperr = xg - xt;

	perr = (float) ((long int) xperr);

	if (xpos1.t != xpos0.t)
		xposvel = (xpos1.x - xpos0.x) / ((xpos1.t - xpos0.t) * SPCT);
	else
		xposvel = 0;

	if (xdelta1.t != xdelta0.t)
		xdeltavel = (xdelta1.x - xdelta0.x) / ((xdelta1.t - xdelta0.t) * SPCT);
	else
		xdeltavel = 0;

	vg = esign * (vel + xposvel + xdeltavel) - Kv * Ag / Vg * perr;

	/* constrain vg to Vg and Ag */
	(void) clamp(&vg, Vg);
	acc = (vg - lastvg) / SPMU;
	if (clamp(&acc, Ag))
		vg = lastvg + acc * SPMU;
	xsetMv(vg); /*set velocity in motor steps.*/

	vmean = (float) (xg - p0) / xtime;

	return;
}

static void updateXLAUNCH(long p0, float t0, float vel)
{
	switch (xlaunch_state)
	{
	case XLAUNCH_GET_INITIAL_POS:
		if (updateXPOS(p0, 0) == 1)
		{
			xtick_start = 0;
			xlaunch_ticks = 0;
			xlaunch_position = p0;
			xlaunch_state = XLAUNCH_GET_LAUNCH_POS;
		}
		break;
	case XLAUNCH_GET_LAUNCH_POS:
		if (xtick_start == 0)
			xtick_start = tnow;
		updateXVEL(xlaunch_position, xtick_start, vel);
		if (fabs(perr) <= 0.5 && fabs(vg - vel) <= 0.5)
		{
			xlaunch_ticks = ((float) (tnow - xtick_start) * 1.5);
			xlaunch_position = p0 - vel * xlaunch_ticks * SPCT;
			xlaunch_state = XLAUNCH_SET_LAUNCH_POS;
		}
		break;
	case XLAUNCH_SET_LAUNCH_POS:
		if (updateXPOS(xlaunch_position, 0) == 1)
		{
			xtick_start = t0 - xlaunch_ticks;
			xlaunch_state = XLAUNCH_LAUNCH;
		}
		break;
	case XLAUNCH_LAUNCH:
		if (tnow >= xtick_start)
		{
			updateXVEL(xlaunch_position, xtick_start, vel);
		}
		break;
	default:
		xlaunch_state = XLAUNCH_GET_INITIAL_POS;
		break;

	}

}

static float getXg()
{
	Long L;
	if (xenc)
	{
		getEncPos(&L);
		tnow = getClock();
		L.l *= cvg(CV_ESIGN);
	}
	else
	{
		getMotPos(&L);
		tnow = getClock();
	}
	return (float) L.l;
}

static void updateXTRACK(void)
{
	xg = getXg();

	if (tnow < xtick0)
	{
		updateXLAUNCH(xp0, xtick0, xvel);
		cvsw(CV_WORKING, W_RUN);
		ontrack = 0;
	}
	else if (tnow >= xtick1 && xtick0 < xtick1)
	{
		/* let mtvel bring us to a nice stop */
		VType z = 0;
		if (xenc)
			etvel(&z);
		else
			mtvel(&z);

		cvsw(CV_WORKING, W_RUN);
		ontrack = 0;

		vmean = 0;
		vg = 0;
		lastvg = vg;
		perr = 0;
		xtime = 0;
	}
	else
	{
		updateXVEL(xp0, xtick0, xvel);

		cvsw(CV_WORKING, W_RUN);
		ontrack = 1;
	}

	cvsw(CV_ONTRACK, ontrack);
	lastontrack = ontrack;

	return;
}

static int xstartTrack(VType enc, VType time, VType t0, VType p0, VType vel,
		VType veldec)
{
	cvsw(CV_ONTRACK, 0);

	getUser();

	xpos_init();
	xdelta_init();

	xenc = enc;
	xp0 = p0;
	xtick0 = (float) t0 / SPCT;
	xtick1 = (float) (t0 + time) / SPCT;
	xvel = (float) vel + ((float) veldec) / 1000.0;
	xpos0.t = xtick0;
	xpos0.x = xp0;
	xpos1.t = xtick0;
	xpos1.x = xp0;

	lastxt = xt + 1; /* make it different of xt */

	xdelta0.t = 0;
	xdelta0.x = 0;
	xdelta1.t = 0;
	xdelta1.x = 0;
	xdt = 0;
	xlaunch_state = XLAUNCH_GET_INITIAL_POS;
	xlaunch_position = xp0;
	xlaunch_ticks = 0;

	vmean = 0;
	vg = 0;
	xg = getXg();
	perr = 0;
	xtime = 0;
	xposvel = 0;
	xdeltavel = 0;

	xfilt0 = 0;
	xfilt1 = 1;

	timeout = tnow + f2l((1000 * t0) / MSPCT) + f2l(cvg(CV_TIMEOUT) / MSPCT);

	updateFp = updateXTRACK;

	cvsw(CV_WORKING, W_RUN);
	lastontrack = ontrack = 0;

	return 0;
}

/*ICE=============================================*/

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: motion.c,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
 */
