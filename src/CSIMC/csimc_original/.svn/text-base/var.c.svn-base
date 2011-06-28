/* code to manage the user variables known throughout a node. */

#include "sa.h"

/* core variable helper get and set functions.
 * the "get" versions just always get. if just always return current, use NULL.
 * the "set" versions return 0 if the attempt is supported and works, else -1.
 *   if don't want to allow user to change, use rOnly.
 *   if only want to allow positive values, use sOnlyPos.
 */

static void setNetVal (int addr, VRef r, VType v);
static VType getNetVal (int addr, VRef r);

static char using0[] = "Using 0\n";

/* TODO: get rid of this when compiler works better with longs */
static int
iseq0 (VType *vp)
{
    return (!((Long*)vp)->H.u && !((Long*)vp)->H.l);
}

/* TODO: get rid of this when compiler works better with longs */
static int
islt0 (VType *vp)
{
    return (((Long*)vp)->H.u & 0x8000);
}

static int
rOnly (VType *vp)
{
    printf ("Read-only\n");
    return (-1);
}

static int
sOnlyPos (VType *vp)
{
    if (*vp <= 0)
    {
        printf ("Only > 0\n");
        return (-1);
    }
    return (0);
}

static int
smsteps (VType *vp)
{
    return (sOnlyPos (vp));
}

static int
sesteps (VType *vp)
{
    if (islt0(vp))
    {
        printf ("Only >= 0\n");
        return (-1);
    }
    return (0);
}

static int
sesign (VType *vp)
{
    *vp = islt0(vp) ? -1 : 1;   /* go easy */
    return (0);
}

static void
gaddr (VType *vp)
{
    *vp = (VType)getBrdAddr();
}

static void
gpeer (VType *vp)
{
    *vp = (VType)pti.peer;
}

static void
geerr (VType *vp)
{
    int s;

    getEncStatus (&s);
    *vp = s;
}

static int
reseteerr (VType *vp)
{
    resetEncStatus ();
    *vp = 0L;
    return (0);
}

static int
sepos (VType *vp)
{
    if (!iseq0(vp))
        printf (using0);
    *vp = 0L;
    setEncZero();
    return (0);
}

static int
smpos (VType *vp)
{
    if (!iseq0(vp))
        printf (using0);
    *vp = 0L;
    setMotZero();
    return (0);
}

static void
gclock (VType *vp)
{
    /* user wants ms since boot */
    *vp = (VType)f2l(getClock()*MSPCT);
}

static int
sclock (VType *vp)
{
    if (islt0(vp))
    {
        printf ("Only >= 0\n");
        return (-1);
    }

    /* kill any motion since it messes with the time scale */
    killMotion(1);

    /* user uses ms */
    setClock (f2l((*vp)/(1000.*SPCT)));

    /* update motion tnow cache */
    syncNow();
    return (0);
}

static void
gpromp (VType *vp)
{
    *vp = (pti.flags & TF_PROMPT) ? 1 : 0;
}

static int
spromp (VType *vp)
{
    if (*vp)
        pti.flags |= TF_PROMPT;
    else
        pti.flags &= ~TF_PROMPT;
    return (0);
}

static void
gtrace (VType *vp)
{
    *vp = (pti.flags & TF_TRACE) ? 1 : 0;
}

static int
strace (VType *vp)
{
    if (*vp)
        pti.flags |= TF_TRACE;
    else
        pti.flags &= ~TF_TRACE;
    return (0);
}

static int
svascale (VType *vp)
{
    switch (*vp)
    {
        case 1: /* FALLTHRU */
        case 10:    /* FALLTHRU */
        case 100:
            return (0);
    }
    printf ("Only 1, 10 or 100\n");
    return (-1);
}

/* read ATD channel n.
 * N.B. we assume system is in continuous-SCAN mode. see ATDCTL5.
 */
static void
getATD (VType *vp, int n)
{
    unsigned bit = 1<<n;

    while (!(ATDSTAT & bit))
        scheduler(0);
    *vp = (VType)(ABYTE(0x70 + 2*n));
}

/* A/D stuff */

static void gATD0 (VType *vp)
{
    getATD (vp, 0);
}
static void gATD1 (VType *vp)
{
    getATD (vp, 1);
}
static void gATD2 (VType *vp)
{
    getATD (vp, 2);
}
static void gATD3 (VType *vp)
{
    getATD (vp, 3);
}
static void gATD4 (VType *vp)
{
    getATD (vp, 4);
}
static void gATD5 (VType *vp)
{
    getATD (vp, 5);
}
static void gATD6 (VType *vp)
{
    getATD (vp, 6);
}
static void gATD7 (VType *vp)
{
    getATD (vp, 7);
}

/* I/O bit stuff */

static Byte
getilevel(void)
{
    unsigned ipolar = cvgw(CV_IPOLAR);
    return (PORTJ ^ ~ipolar);
}

static void
gilevel (VType *vp)
{
    *vp = getilevel();
}

static void
giedge (VType *vp)
{
    *vp = KWIFJ;
}

static int
siedge (VType *vp)
{
    unsigned ipolar = (unsigned) cvgw(CV_IPOLAR);
    unsigned v = *vp;

    KWIEJ &= ~v;            /* guard against stray intrp while chg*/
    KPOLJ = (KPOLJ & ~v)|(ipolar & v);  /* set edge polarities */
    KWIFJ = v;          /* reset latches */
    KWIEJ |= v;         /* reenable edge interrupts */
    return (0);
}

static int
solevel (VType *vp)
{
    PORTH = *vp;
    return (0);
}

/* given a motor direction, return mask containing its limit bit */
static unsigned
dirLimMask (int motdir)
{
    return (motdir > 0 ? cvgw(CV_PLIMBIT) : cvgw(CV_NLIMBIT));
}

/* return 1 if the given motor direction is forbidden with respect to
 * current limit conditions, else return 0.
 */
int
atLimit (int motdir)
{
    return (motdir && ((KWIFJ|getilevel()) & dirLimMask(motdir)));
}

/* given a motor direction, reset/rearm that limit edge */
void
resetLimit (int motdir)
{
    if (motdir)
        KWIFJ = dirLimMask(motdir);
}

/* use to get one of the bitmask variables without suffering sign extension
 * when written in clever ways such as ~0.
 */
static void
gmask (VType *vp)
{
    *vp = (Byte)*vp;
}

/* info about the core variables.
 * N.B. must be in alphabetical order and match CVIds.
 */
CVar cvar[] =
{
    {"ad0", gATD0,      rOnly},
    {"ad1", gATD1,      rOnly},
    {"ad2", gATD2,      rOnly},
    {"ad3", gATD3,      rOnly},
    {"ad4", gATD4,      rOnly},
    {"ad5", gATD5,      rOnly},
    {"ad6", gATD6,      rOnly},
    {"ad7", gATD7,      rOnly},
    {"cloc",    gclock,     sclock},
    {"eerr",    geerr,      reseteerr},
    {"epos",    gepos,      sepos},
    {"esig",    NULL,       sesign,     1L},
    {"este",    NULL,       sesteps},
    {"etpo",    NULL,       etpos},
    {"etri",    NULL,       NULL},
    {"etve",    NULL,       etvel},
    {"evel",    gevel,      rOnly},
    {"home",    gmask,      NULL},
    {"iedg",    giedge,     siedge},
    {"ilev",    gilevel,    rOnly},
    {"ipol",    gmask,      NULL},
    {"kv",  NULL,       sOnlyPos,   1000L},
    {"lima",    NULL,       sOnlyPos,   10000L},
    {"maxa",    NULL,       sOnlyPos,   100L},
    {"maxv",    NULL,       sOnlyPos,   200L},
    {"mdir",    gmdir,      rOnly},
    {"mpos",    gmpos,      smpos},
    {"mste",    NULL,       smsteps,    200L},
    {"mtpo",    NULL,       mtpos},
    {"mtri",    NULL,       NULL},
    {"mtve",    NULL,       mtvel},
    {"mvel",    gmvel,      rOnly},
    {"myad",    gaddr,      rOnly},
    {"nlim",    gmask,      NULL},
    {"olev",    gmask,      solevel},
    {"ontr",    NULL,       rOnly},
    {"peer",    gpeer,      rOnly},
    {"plim",    gmask,      NULL},
    {"prom",    gpromp,     spromp},
    {"time",    NULL,       sOnlyPos,   300000L},
    {"toff",    NULL,       NULL},
    {"trac",    gtrace,     strace},    /* undocumented */
    {"vasc",    NULL,       svascale,   1L},
    {"vers",    NULL,       rOnly,      VERSION},
    {"work",    NULL,       rOnly},
};

int ncvar = (sizeof(cvar)/sizeof(cvar[0]));

/* the global user variables */
static VType uvar[NUSRV];

/* search cvar[] for name.
 * return index, else -1.
 */
static int
findCV (char *name)
{
    int t, b, m, s;

    /* handy place to sanity-check the cvar list size */
    if (CV_N != ncvar)
        errFlash (18);

    /* binary search */
    t = ncvar - 1;
    b = 0;
    while (b <= t)
    {
        m = (t+b)/2;
        s = strncmp (name, cvar[m].name, NCNAME);
        if (s == 0)
            return (m);
        if (s < 0)
            t = m-1;
        else
            b = m+1;
    }

    return (-1);
}

/* get a reference to the named variable. the ref can then be used with
 *    v[gs]v(). references are packed as per sa.h.
 * fpn[] are the function's nfpn formal params (not used for all classes).
 * return BADVREF if no match.
 * N.B. we modify name[] IN PLACE if it is a remote reference.
 *
 * A reference is a packed id for any of the several types of variables
 * supported. A VRef holds a class, a node address and an index. See sa.h
 * for packing details. Index depends on class according to:
 *     CORE: core global variables, with names given by cvar[].name.
 *        0 .. ncvar-1 indices into cvar[], used as cvar[i]
 *     USER: user global variables, with names 1 char 'a' .. 'z'
 *        0 .. NUSRV-1 correspond to 'a' .. 'z', used as gvar[i]
 *     FPAR: func parameters, found defined, all match "$"[a-z][a-z0-9_]*
 *        2+NFUNP-1 .. 2 used as FP[i]
 *     FTMP: func temp (stack) variables, with names as $0 .. ${NTMPV-1}
 *        2 .. 2+NTMPV-1 used as FP[-i]
 */
VRef
vgr (char name[], char fpn[][NNAME+1], int nfpn)
{
    int idx, class, addr;
    char *at;
    int i;

    /* establish node address */
    at = strchr (name, '@');
    if (at)
    {
        if (!isdigit(at[1]))
            return (BADVREF);   /* node is required */
        *at = '\0';         /* leave just name */
        addr = atoi (at+1);     /* break out address */
    }
    else
        addr = getBrdAddr();    /* absent implies us */

    /* establish index and class */
    if (name[1] == '\0')
    {
        /* 1-char entries are always the global user variables */
        idx = name[0];
        if (!islower(idx))
            return (BADVREF);
        idx -= 'a';         /* char to index */
        class = VC_USER;
    }
    else if (name[0] == '$')
    {
        /* $* func formal param or 1-digit temp */
        if (at)
            return (BADVREF);   /* local params only */
        if (isdigit(name[1]))
        {
            /* function temp */
            if (name[2])
                return (BADVREF);   /* single-digits only */
            idx = name[1] - '0' + 2;
            class = VC_FTMP;
        }
        else
        {
            /* formal param */
            for (i = 0; i < nfpn; i++)
                if (strncmp (name, fpn[i], NNAME) == 0)
                {
                    idx = nfpn - 1 - i + 2;
                    class = VC_FPAR;
                    break;
                }
            if (i == nfpn)
                return (BADVREF);   /* not found */
        }
    }
    else
    {
        /* better be a core global variable */
        idx = findCV (name);
        if (idx < 0)
            return (BADVREF);   /* not found */
        class = VC_CORE;
    }

#ifdef HOSTTEST
    printf ("%s: %d %d %d\n", name, idx, class, addr);
#endif

    /* ok */
    return((idx<<VR_IDXSHIFT)|(class<<VR_CLASSSHIFT)|(addr<< VR_ADDRSHIFT));
}

/* return the value of a variable given its reference code.
 * include all side effects of reading the variable if any.
 * N.B. this is to be fast.. we do no error checking here.
 */
VType
vgv (VRef r)
{
    int idx, class, addr;
    CVar *cp;

    addr = (r >> VR_ADDRSHIFT) & VR_ADDRMASK;
    if (addr != getBrdAddr())
        return (getNetVal (addr, r));

    class = (r >> VR_CLASSSHIFT) & VR_CLASSMASK;
    idx = (r >> VR_IDXSHIFT) & VR_IDXMASK;
    switch (class)
    {
        case VC_CORE:
            cp = &cvar[idx];
            if (cp->g)
                (*cp->g)(&cp->v);
            return  (cp->v);
        case VC_USER:
            return (uvar[idx]);
        case VC_FPAR:
            return (FP[idx]);
        case VC_FTMP:
            return (FP[-idx]);
        default:
            break;
    }

    return (0);

}

/* set the value of a variable given its reference code.
 * include all side effects of setting the variable if any.
 * return value too for caller's convenience and shadow in case there is no
 *   `get' helper function.
 * N.B. this is to be fast.. we do no error checking here.
 */
VType
vsv (VRef r, VType v)
{
    int idx, class, addr;
    CVar *cp;

    addr = (r >> VR_ADDRSHIFT) & VR_ADDRMASK;
    if (addr != getBrdAddr())
    {
        setNetVal (addr, r, v);
        return (v);
    }

    class = (r >> VR_CLASSSHIFT) & VR_CLASSMASK;
    idx = (r >> VR_IDXSHIFT) & VR_IDXMASK;
    switch (class)
    {
        case VC_CORE:
            cp = &cvar[idx];
            if (!cp->s || !(*cp->s)(&v))
                cp->v = v;
            return  (cp->v);
        case VC_USER:
            return (uvar[idx] = v);
        case VC_FPAR:
            return (FP[idx] = v);
        case VC_FTMP:
            return (FP[-idx] = v);
        default:
            break;
    }

    return (0);

}

#ifdef HOSTTEST

/* given a VRef referencing a value on another node, go get and return it */
static VType
getNetVal (int addr, VRef r)
{
    return (123456);
}

static void
setNetVal (int addr, VRef r, VType v)
{
}

#else

/* given a VRef referencing a value on another node, go get and return it */
static VType
getNetVal (int addr, VRef r)
{
    VType v;

    sendGETVAR(addr, r, &v);    /* dies if fails */
    return (v);
}

static void
setNetVal (int addr, VRef r, VType v)
{
    sendSETVAR (addr, r, v);    /* dies if fails */
}

#endif /* HOSTTEST */

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: var.c,v $ $Date: 2001/04/19 21:11:58 $ $Revision: 1.1.1.1 $ $Name:  $
 */
