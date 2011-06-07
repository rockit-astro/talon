
// build specfic settings
#include "buildcfg.h"

/* ids for fifos.
 * N.B. see fifos[] in fifoio.c
 */
typedef enum {
    Tel_Id, Filter_Id, Focus_Id, Dome_Id, Cover_Id, Lights_Id, Cam_Id
} FifoId;

/* scan program function */
typedef int (*PrFunc)(int first);

/* telrun.c */
#define	cscan	(&telstatshmp->scan)	/* handy access to current scan */
extern char ccfn[];
extern TelStatShm *telstatshmp;
extern double SETUP_TO;
extern double CAMDIG_MAX;
extern char scanfile[];
extern void die(void);
extern void all_stop(int mark);
extern void tlog (Scan *sp, char *fmt, ...);
extern int addProgram (PrFunc p, int bkg);

/* fifoio.c */
extern void init_fifos(void);
extern int chk_pending(void);
extern int chk_fifos(void);
extern void setTrigger (time_t t);
extern void stop_all_devices(void);
extern void fifoWrite (int f, char *fmt, ...);

/* fileio.c */
extern int newSLS (char scanfn[]);
extern int findNew (char scanfn[], Scan *sp);
extern void markScan (char fn[], Scan *sp, int code);

/* program files */
extern int pr_regscan(int first);
extern void pr_regSetup(void);
extern int pr_bias(int first);
extern int pr_thermal(int first);
extern int pr_flat(int first);
extern void pr_flatSetup(void);
extern int pr_new_bias(int first);
extern int pr_new_thermal(int first);
extern int pr_new_flat(int first);

static double STOWAZ;
static double STOWALT;

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: telrun.h,v $ $Date: 2002/12/04 08:48:01 $ $Revision: 1.2 $ $Name:  $
 */
