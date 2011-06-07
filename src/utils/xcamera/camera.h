#ifndef _CAM_H
#define _CAM_H

// Bring in build specific settings 
#include "buildcfg.h"

/* data structs and global declarations for camera program.
 * we require Xm.h and fits.h.
 */

#include "luts.h"

/* all coords are x/y, upper left 0/0 */

/* An area of interest */
typedef struct {
    int x, y;			/* upper left, image coords */
    int w, h;			/* size, pixels; must be >= 0 */
} AOI;


#define	MAGDENOM	64
#define	NXCOLS		75	/* number of gray-scale colors */


typedef struct {
    FImage	fimage;		/* FITS image header and its 16 bit data */
    char	fname[1024];	/* filename */

    XImage	*ximagep;	/* X Image -- manipulate with XGet/PutPixel */
    int 	depth;		/* depth of image == depth of window, bits */
    int 	bpp;		/* bits per pixel in array: 8, 16 or 32 */

    Widget	imageSW;	/* scrolled window containing the DrawingArea */
    Widget	imageDA;	/* DrawingArea containing ximagep */
    GC		daGC;		/* GC to use with imageDA */

    AOIStats	stats;		/* stats about the image, as per flags */
    AOIStats	aoiStats;	/* stats in the current AOI */
    AOI		aoi;		/* area of interest, image coords */
    Pixel	lut[NCAMPIX];	/* idx with CamPixel, get back its pixel */
    Pixel	xpixels[NXCOLS];/* X pixels ramp black through white */
    LutType	luttype;	/* which lut to use */
    int		mag;		/* numerator over MAGDENOM */
    CamPixel	lo, hi;		/* current window */

    FieldStar	*fs_a;		/* malloced array of field stars stars */
    int		fs_n;		/* number of items in fs_a array */ 

    /* flags */
    int		crop:1;		/* whether to crop scene to AOI */
    int		aoistats:1;	/* whether to compute stats on just AOI */
    int		aoinot:1;	/* if aoistats, whether to use all but AOI */
    int		inverse:1;	/* whether we want an inverse video lut */
    int		resetaoi:1;	/* whether to reset AOI on each open */
    int		showgsc:1;	/* whether to display GSC star overlay */
    int		lrflip:1;	/* whether to flip left-right */
    int		tbflip:1;	/* whether to flip top-bottom */
    int		roam:1;		/* whether to show cursor coords all the time */
} State;

/* KMI 10/19/03 - moved MTB from basic.c to here */
/* info about mag settings */
typedef struct {
    char *name;     /* TB name */
    char *label;    /* TB label */
    int numerator;  /* assumes denominator of MAGDENOM */
    Widget w;       /* TB */
} MTB; 

extern State state;

typedef enum {
    START_PLOT, RUN_PLOT, END_PLOT
} PlotState;

/* basic.c */
extern void manageBasic(void);
extern void createBasic(void);
extern void doAOI (PlotState ps, int x, int y);
extern void drawAOI(Bool restore, AOI *aoip);
extern void updateAOI(void);
extern void resetAOI(void);
extern void resetCrop(void);

/* arith.c */
extern void arithCB(Widget w, XtPointer client, XtPointer call);

/* blink.c */
extern void blinkCB(Widget w, XtPointer client, XtPointer call);
extern void blinkRefCB(Widget w, XtPointer client, XtPointer call);

/* camera.c */
extern int camDevFileRW(void);
extern void camManage(void);
extern void camCreate(void);
extern void camTake1(void);
extern void camLoop(void);
extern void camCancel(void);
extern void startBias (char *bfn, int n);
extern void startThermal (char *tfn, char *bfn, int n, double d);
extern void startFlat (char *ffn, char *tfn, char *bfn, int filter, int n,
    double d);

/* corr.c */
extern void manageCorr(void);
extern void createCorr(void);
extern void applyCorr(void);
extern void getCorrFilter (char *buf);
extern int fixBadColumns(FImage *fip);
extern Bool getAutoCorrectState(void);
extern void setAutoCorrectState(Bool state);

/* del.c */
extern void manageDel(void);

/* drift.c */
extern int haveDS(void);
extern void startCDS (int us);
extern void dsNewInterval (int us);
extern void dsStop (void);

/* epoch.c */
extern void manageEpoch(void);
extern void createEpoch(void);
extern double pEyear (void);
extern double pEmjd (void);
extern void p2000toE (double *rap, double *decp);

/* fits.c */
extern void manageFITS(void);
extern void updateFITS(void);
extern void setFWHMCB (Widget w, XtPointer client, XtPointer call);

/* glass.c */
extern void manageGlass(void);
extern void createGlass(void);
extern void doGlass (Display *dsp, Window win, PlotState ps, int wx, int wy,
    unsigned wid, unsigned hei);

/* gsclim.c */
extern double gsclimit;
extern double huntrad;
extern int usno_ok;
extern void gscSetDialog(void);
extern void manageGSCLimit(void);
extern void createGSCLimit(void);

/* hist.c */
extern void createHistoryMenu (Widget pdm);
extern void addHistory (char *name);
extern void delHistory (char *base);

/* ip.c */
extern void initXPixels(LutType lt);
extern void setLut(void);
extern void computeStats(void);
extern void image2window (int *xp, int *yp);

/* mag.c */
extern void FtoXImage(void);

/* main.c */
extern void cam1CB (Widget w, XtPointer client, XtPointer call);
extern void camContCB (Widget w, XtPointer client, XtPointer call);
extern void camStopCB (Widget w, XtPointer client, XtPointer call);
extern Widget toplevel_w;
extern Colormap camcm;
extern XtAppContext app;
extern char myclass[];
extern void msg (char *fmt, ...);
extern void showHeader (void);
extern void mkHeader (char *buf);
extern void printState(void);
extern int get_color_resource (Display *dsp, char *myclass, char *cname,
    Pixel *p);

/* markers.c */
extern void markersCB(Widget w, XtPointer client, XtPointer call);
extern void drawMarkers(void);

/* measure.c */
extern void manageMeasure(void);
extern void flipMeasure (int lrflip, int tbflip);
extern void doMeasure (PlotState ps, int x, int y, int radecok, double ra,
    double dec);
extern void doMeasureSetRef (int ix, int iy);

/* open.c */
extern void manageOpen(void);
extern void createOpen(void);
extern void setFilenameFifo(char *name);
extern char * getFilenameFifo(void);
extern int listenFifoSet(int on);
extern void presentNewImage (void);
extern int openFile(char *fn);

/* photom.c */
extern void photomCB(Widget w, XtPointer client, XtPointer call);
extern void doPhotom (int x, int y);
extern void setAp (int newap);

/* photomabs.c */
extern void readAbsPhotom (void);
extern Widget createAbsPhot_w (Widget parent_w, Widget top_w);
extern void manageAbsPhot (int whether);
extern void absPhotWork(StarStats *newStar);

/* print.c */
extern void printCB(Widget w, XtPointer client, XtPointer call);
extern void get_views_font (Display *dsp, XFontStruct **fspp);

/* save.c */
extern void manageSave(void);
extern void updateSave(void);
extern void createSave(void);
extern void writeImage(void);
extern void setFName (char *fn);
extern void setSaveName (void);
extern int mkTemplateName (char *buf);
extern int getSaveName (char *buf);
extern int saveAuto(void);

/* stars.c */
extern void markHdrRADecCB(Widget w, XtPointer client, XtPointer call);
extern void markStarsCB(Widget w, XtPointer client, XtPointer call);
extern void markStreaksCB(Widget w, XtPointer client, XtPointer call);
extern void markSmearsCB(Widget w, XtPointer client, XtPointer call);
extern void markGSCCB(Widget w, XtPointer client, XtPointer call);
extern void markGSC(void);
extern void resetGSC(void);

/* wcs.c */
extern void setWCSCB (Widget w, XtPointer client, XtPointer call);
extern int xy2rd (FImage *fip, double x, double y, double *rap, double *decp);
extern int rd2kxy (FImage *fip, double ra, double dec, double *xp, double *yp);

/* winhist.c */
extern void noAutoWin(void);
extern void manageWin(void);
extern void createWin(void);
extern void newStats(void);
extern void setWindow(void);
extern void updateWin(void);

/* ximage.c */
extern void initDepth(void);
extern void updateXImage(void);
extern void newXImage(void);
extern void refreshScene(int x, int y, int w, int h);
extern void watch_cursor (int want);

#endif // _CAM_H

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: camera.h,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.9 $ $Name:  $
 */
