// Virtual Motion Controller

#ifndef VIRMC_H
#define VIRMC_H

#include <sys/timeb.h>	// ftime

#define mAbs(v)  ((v) < 0 ? -(v) : (v))
#define absclamp(v,m) (v = (mAbs(v) < (m) ? (v) : (v) < 0 ? -(m) : (m)))

#define NVNODES	8		// enough to do stuff with...

#define	HOMEBIT 1
#define PLIMBIT 2
#define NLIMBIT 4

#ifndef PI
#define PI	3.1415
#endif

// Define a structure for a "node"
typedef struct
{
	long	countsPerRev;		// encoder counts per revolution
	long	posLimit;			// location of positive limit
	long	negLimit;			// location of negative limit
	long	homePos;			// location of home position
	
	int		sign;				// 1 or -1
	
	long	currentPos;			// current position
	long	targetPos;			// where we are going
	long	lastPos;			// previous position
	long	lastTime;			// last time stamp
	double	velocity;			// current speed, steps per second, signed
	
	int		maxVel;				// maximum speed, steps per second, unsigned
	
	int		timeout;			// timeout value... not used. Virtual motors never stall...
	
	char	iedge;				// triggered bits, like iedge of csimc
	
	struct timeb timeRef;		// a time reference used for millisecond clock
	double * trackPath;			// allocation for path points if tracking
	int		numTrackPts;		// number of tracking points in path
	int		trackStart;			// ms time this path starts at
	int		trackIval;			// ms interval between each track point
	int		toffset;			// jogged offset from track path
	
	int		targetSet;			// 1 if we are actively pursuing a target
	int		tracking;			// 1 if we are tracking, else 0
	int		clamped;			// 1 if we were clamped during last motion, else 0
	
	long	miscVal[4];			// misc values for passing

} VCNode, *VCNodePtr;


extern void vmcService(int node);
extern void vmcReset(int node);
extern int vmcSetup(int node, double maxvelr, double maxAccr, long steps, int sign);
extern void vmcSetTargetPosition(int node, long position);
extern long vmcGetPosition(int node);
extern int vmcGetVelocity(int node);
extern void vmcResetClock(int node);
extern long vmcGetClock(int node);
extern void vmcSetTimeout(int node, int timeout);
extern void vmcSetTrackingOffset(int node, int offset);
extern void vmcJog(int node, int amt);
extern void vmcStop(int node);
extern int vmcSetTrackPath(int node, int num, int startMs, int ivalMs, double *path);
extern void vmcSetHome(int node);

extern int vmc_r(int node, char *buf, int length);
extern void vmc_w(int node, char *buf);
extern int vmc_isReady(int node);
long vmc_rix(int node, char *string);

#endif // VIRMC_H
