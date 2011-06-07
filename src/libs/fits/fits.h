/* include file for fits.c and fitsip.c
 */

#define	FITS_HROWS	36
#define	FITS_HCOLS	80
#define MAXSTREAKS      100  // for streak finder // 

typedef char		FITSRow[FITS_HCOLS];

typedef struct {
    /* following fields are cracked from the header for easy reference */
    int bitpix;		/* BITPIX -- MUST BE 16 FOR NOW */
    int sw, sh;		/* width/height, net pixels.  NAXIS1 and NAXIS2 */
    int sx, sy;		/* starting X and Y, raw pixels. OFFSET1 and OFFSET2 */
    int bx, by;		/* binning. XFACTOR and YFACTOR */
    int dur;		/* exposure time, ms. EXPTIME is %.3f seconds */

    FITSRow *var;	/* malloced array of all unrecognized header lines */
    int nvar;		/* number of var[] */

    char *image;	/* malloced image data array of sw*sh*2 bytes */
} FImage;

// data recorded by streak finder
typedef struct
{
	int startX;			// starting x (peak pixel)
	int startY; 		// starting y (peak pixel)
	int endX;			// ending x (peak pixel)
	int endY;			// ending y (peak pixel)
	int walkStartX;		// starting x of gaussian fringe (walk start point)
	int walkStartY;		// starting y of gaussian fringe (walk start point)
	int walkEndX;		// ending x of gaussian fringe (walk stop point)
	int walkEndY;		// ending y of gaussian fringe (walk stop point)
	int length;			// peak-to-peak length of STRTYP_FOUNDLENGTH streak, or full "walk length" for STRTYP_FWHMRATIO
	double slope;		// slope of line. dy/dx. Y is always downward (or 0, or HUGE_VAL).
						// sign is direction of x. Computed according to type of streak between peak/peak or full length
	double fwhmRatio;	// the ratio of computed fwhm y/x measured at peak pixel start.
	int	flags;			// one or more bit flags as defined below.  Defines acceptance or rejection state.
	
} StreakData;

// number of segments for analyzing streak / bits per flag group
#define NSEG 4	// NOTE: 4 seems to work best

// Low-order flag interpretations: Summary bits
#define STREAK_MASK			((NSEG<<1)-1)
#define STREAK_YES			 1
#define STREAK_NO			 0
#define STREAK_MAYBE		 2

// Smear support
// Must include wcs.h first before we can use smear features
/* Record smear segments */
typedef struct {
	int	startx;	// left side of smear
	int starty;	// approximate midpoint of smear
	int length;	// horizontal length
	int bright; // brightness value
	
} SmearData;
extern int MAXSMEARWIDTH;

// Define a pointer to the setWCSFITS function... this will allow a reference to the WCS library
// to exist in libFits (or other libs) without necessarily incurring a prerequisite link
typedef int (*SETWCSFUNC)(FImage *fip, int wantusno, double hunt, int (*bfp)(), int verbose, char *msg);
extern void SetWCSFunction(SETWCSFUNC inWcs);
extern int findSmears(FImage *fip,SmearData **pSmearData, int *pNumSmears,
					   int findAnomolies, int tusno, double hunt, int (*bail_out)(), char *str);

extern int writeFITS (int fd, FImage *fip, char *errmsg, int restore);
extern int writeFITSHeader (FImage *fip, int fd, char *errmsg);
extern int readFITS (int fd, FImage *fip, char *errmsg);
extern int readFITSHeader (int fd, FImage *fip, char *errmsg);
extern int copyFITS (FImage *to, FImage *from);
extern int copyFITSHeader (FImage *to, FImage *from);
extern int writeSimpleFITS (int fd, char *pix, int w, int h, int x, int y,
    int dur, int restore);
extern int cropFITS (FImage *to, FImage *fr, int x, int y, int w, int h,
    char errmsg[]);
extern void timeStampFITS (FImage *fip, time_t t, char *comment);
extern int getNAXIS (FImage *fip, int *n1p, int *n2p, char errmsg[]);
extern void enFITSPixels (char *image, int npix);
extern void unFITSPixels (char *image, int npix);
extern void initFImage (FImage *fip);
extern void resetFImage (FImage *fip);
extern void setSimpleFITSHeader (FImage *fip);
extern void setLogicalFITS (FImage *fip, char *name, int v, char *comment);
extern void setIntFITS (FImage *fip, char *name, int v, char *comment);
extern void setRealFITS (FImage *fip, char *name, double v, int sigdig,
    char *comment);
extern void setCommentFITS (FImage *fip, char *name, char *comment);
extern void setStringFITS(FImage *fip, char *name, char *string, char *comment);
extern int getLogicalFITS (FImage *fip, char *name, int *vp);
extern int getIntFITS (FImage *fip, char *name, int *vp);
extern int getRealFITS (FImage *fip, char *name, double *vp);
extern int getCommentFITS (FImage *fip, char *name, char *buf);
extern int getStringFITS (FImage *fip, char *name, char *string);
extern int delFImageVar (FImage *fip, char *name);

typedef unsigned short CamPixel;		/* type of pixel */
#define	NCAMPIX	(1<<(int)(8*sizeof(CamPixel)))	/* number of unique CamPixels */
#define	MAXCAMPIX	(NCAMPIX-1)		/* largest value in a CamPixel*/

typedef struct {
    CamPixel mean, median;	/* mean and median pixel values */
    CamPixel min, max;		/* min and max pixel values */
    int maxx, maxy;		/* location of max pixel */
    double sd;			/* std deviation */
    double sum, sum2;		/* sum of pixels and sum of pixels squared */
    int hist[NCAMPIX];		/* histogram */
} AOIStats;

extern void flipImgCols (CamPixel *img, int w, int h);
extern void flipImgRows (CamPixel *img, int w, int h);
extern void transposeXY(CamPixel *img, int w, int h, int dir);
extern int align2FITS (FImage *fip1, char *image2, int *dxp, int *dyp);
extern void alignAdd (FImage *fip1, char *image2, int dx, int dy);
extern void aoiStatsFITS (char *ip, int w, int x, int y, int nx, int ny,
							    AOIStats *sp);
extern int findStars (char *image, int w, int h, int **xa, int **ya,
    CamPixel **ba);

extern int findStarsAndStreaks(char *im0, int w, int h, int **xa, int **ya,
	CamPixel **ba, StreakData **sa, int *numStreaks);
	
/* how starStats uses its initial x/y */
typedef enum {
    SSHOW_BRIGHTWALK=1, SSHOW_MAXINAREA, SSHOW_HERE
} SSHow;

/* info to define parameters used when we look for a star */
typedef struct {
    int rsrch;		/* max radius to search for brightest pixel */
    int rAp;   		/* aperture radius. if 0, determine automatically */
    SSHow how;		/* how to establish brightest pixel */
} StarDfn;

/* info to describe what we've learned about a "star" from starStats() */
typedef struct {
    int p;		/* value of brightest pixel */
    int bx, by;		/* location of brightest pixel */
    int Src;		/* total counts due just to star in aperature */
    double rmsSrc;	/* rms error of Src */
    int rAp;		/* aperature radius */
    int Sky;		/* median value of noise annulus */
    double rmsSky;	/* rms of Sky */

    /* following are based on the best gaussion fits in each dimension *after*
     * Sky has been subtracted off from all pixel values. Then, the x/ymax
     * have had Sky added back on so they can serve as pixel values.
     */
    double x, y;	/* location of centroid */
    double xfwhm, yfwhm;/* full width at half max. FYI: sigma = fwhm/2.354 */
    double xmax, ymax;	/* gaussian peak (with Sky added back on) */
} StarStats;

extern int starStats (CamPixel *image, int w, int h, StarDfn *sdp,
    int ix, int iy, StarStats *ssp, char errmsg[]);
extern int starMag (StarStats *ref, StarStats *targt, double *mp, double *dmp);
extern int fwhmFITS (char *im, int w, int h, double *hp, double *hsp,
    double *vp, double *vsp, char *msg);
extern int setFWHMFITS (FImage *fip, char whynot[]);
extern int medianFilter (FImage *from, FImage *to, int hsize);
extern int findStatStars (char *im0, int w, int h, StarStats **sspp);
extern int findLinearFeature (char *im0, int w, int h, StarStats **ssp, \
			      double *xfirst, double *yfirst, \
			      double *xlast, double *ylast);
extern void threeSort (int n, double *s, double *v1, double *v2); 
extern void linFit (double *x, double *y, int ndata, double *a, \
		    double *b, double *siga, double *sigb, double *chi2);
extern void getStats (double *x, int ndata, double *mean, double *median,\
	  double *stdev);
extern int removeOutliers (int ndata, double *x, double *y, double *fr);
extern int flatField (FImage *from, FImage *to, int order);


// ip.cfg control
extern void loadIpCfg(void);
extern char * getCurrentIpCfgPath(void);
extern void setIpCfgPath(char *pathname);

// ip.cfg values
// -- STAR FINDER --
extern int FSBORD;
extern int FSNNBOX;
extern int FSNBOXSZ;
extern int FSMINSEP;
extern int FSMINCON;
extern double FSMINSD;
extern int BURNEDOUT;
// -- STAR STATS --
extern double TELGAIN;
extern int DEFSKYRAD;
extern double MINAPRAD;
extern double APGAP;
extern double APSKYX;
extern double MAXSKYPIX;
extern int MINGAUSSR;
// -- FWHM STATS --
extern int NFWHM;
extern double FWHMSD;
extern int FWHMR;
extern double FWHMRF;
extern double FWHMSF;
extern double FWHMRATIO;
// -- STREAK DETECTION --
extern double STRKDEV;
extern int STRKRAD;
extern int MINSTRKLEN;
// -- WCS FITTER
extern int MAXRESID;
extern int MAXISTARS;
extern int MAXCSTARS;
extern int BRCSTAR;
//#if USE_DISTANCE_METHOD	
extern int MINPAIR;
extern int MAXPAIR;
extern int TRYSTARS;
extern double MAXROT;
extern double MATCHDIST;
extern double REJECTDIST;
extern int ORDER;
//#endif




/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: fits.h,v $ $Date: 2003/03/13 00:29:35 $ $Revision: 1.13 $ $Name:  $
 */
