
/* ids for fifos.
 * N.B. see fifos[] in telescoped.c
 */
typedef enum {
    Tel_Id, Focus_Id, Dome_Id, Cover_Id
} FifoId;

/* CSIMC info */
typedef struct {
    int cfd;		/* command fifo, ok to leave return info pending */
    int sfd;		/* status fifo, always block to capture anything back */
} CSIMCInfo;

#define	MIPCFD(mip)	(csii[(int)((mip)->axis)].cfd)	/* handy mip ==> cfd */
#define	MIPSFD(mip)	(csii[(int)((mip)->axis)].sfd)	/* handy mip ==> sfd */

/* axes.c */
extern int axis_home (MotorInfo *mip, FifoId fid, int first);
extern int axis_limits (MotorInfo *mip, FifoId fid, int first);
extern int axisLimitCheck (MotorInfo *mip, char msgbuf[]);
extern int axisMotionCheck (MotorInfo *mip, char msgbuf[]);
extern int axisHomedCheck(MotorInfo *mip, char msgbuf[]);

/* csimc.c */
extern CSIMCInfo csii[NNODES];
extern void csiInit (void);
extern void csiDrain (int fd);
extern void csiSetup (MotorInfo *mip);
extern void csiStop (MotorInfo *mip, int fast);
extern void csiiOpen (MotorInfo *mip);
extern void csiiClose (MotorInfo *mip);
extern int csiOpen (int addr);
extern int csiClose (int addr);
extern int csiIsReady (int fd);

/* dome.c */
extern void dome_msg (char *msg);

/* csi_dome.c */
extern void csi_dome_msg (char *msg);

/* commhub_dome.c */
extern void commhub_dome_msg (char *msg);

/* fifoio.c */
extern void fifoWrite (FifoId f, int code, char *fmt, ...);
extern void init_fifos(void);
extern void chk_fifos(void);
extern void close_fifos(void);

/* focus.c */
extern void focus_msg (char *msg);

/* mountcor.c */
extern void init_mount_cor(void);
extern void tel_mount_cor (double ha, double dec, double *dhap, double *ddecp);

/* tel.c */
extern void tel_msg (char *msg);

/* telescoped.c */
extern double STOWALT, STOWAZ;
extern TelStatShm *telstatshmp;
extern int virtual_mode;
extern char tscfn[];
extern char tdcfn[];
extern char hcfn[];
extern char ocfn[];
extern char icfn[];
extern char dcfn[];
extern char csidcfn[]; /* CSIDomeCFN */
extern char chdcfn[]; /* CommHubDomeCFN */
extern char ccfn[];
extern void init_cfg(void);
extern void allstop(void);
extern void tdlog (char *fmt, ...);
extern void die(void);

/* cover.c */
extern void cover_msg (char *msg);

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: teled.h,v $ $Date: 2002/12/04 08:50:32 $ $Revision: 1.5 $ $Name:  $
 */
