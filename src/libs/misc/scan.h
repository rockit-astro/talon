/* info about one scan request.
 */

#ifndef _SCAN_H
#define	_SCAN_H

#include "ccdcamera.h"
#include "circum.h"


typedef enum {
    CT_NONE = 0,	/* take no new cal files */
    CT_BIAS,		/* take fresh bias */
    CT_THERMAL,		/* take fresh bias+therm */
    CT_FLAT,		/* take fresh bias+therm+flat */

/* SO 2/6/02: Forced overloading of this structure to support
   additional features such as focus position, fixed alt/az, etc.
   This is ugly, but the SLS format is somewhat rigid, and this should
   provide some backward compatibility... besides, it's all going to be
   replaced with Claw-derived scheduling anyway */

 /* SO 10/29/09: Funny to read the comments above now...
  * I'm back! Hacking this again to add a more open-ended hook to the ExtendedAction
  * paradigm, but making it more command driven for more future flexibility, if needed.
  * Of course, I'll say what I did almost eight years ago and say this is really just
  * temporary because all of this type of scheduling will soon become obsolete... yeah, right...
  *
  * So, in summary:  CT_ does not prefix Calibration Type.  It really means Convoluted Tampering.
  * But, in fairness, this is the best (simple) way to keep from mucking with the existing .SCH format
  * which is shared between different versions of Talon at different locations in the SSON network,
  * while still allowing customized communications to be passed through the SCH format for specific uses.
  *
  */

   CT_FOCUSPOS,     /* Set focus to an absolute position, in microns.  Position stored in rao. */
   CT_FOCUSOFF,     /* Offset focus position +/- amount of microns in rao from current position */
   CT_AUTOFOCUS,    /* Perform autofocus */
   CT_FIXEDALTAZ,   /* Slew telescope to rao,deco and take exposure w/o tracking */
   CT_EXTCMD        /* The latest in the crazy extended action world (10/29/09); keyword CMDLINE; comment-embedded SCH transfers, etc. */

} CCType;

typedef enum {
    CD_NONE = 0,	/* take no new "data", just possibly cal files */
    CD_RAW,		/* take data but do not calibrate */
    CD_COOKED		/* take data and calibrate */
} CCData;

typedef struct {
    CCType newc;		/* which new cal files to take, if any */
    CCData data;	/* how to process new data, if any */
} CCDCalib;

#define	CAL_NSTR 8	/* number of CCDCalib string representations */
#define	CCDSO_NSTR 4	/* number of CCDShutterOptions string representations */

typedef struct {
    /* file info */
    char schedfn[32];	/* basename of original .sch schedule file */
    char imagefn[32];	/* basename of image to create */
    char imagedn[128];	/* dir of image to create */

    /* object definition */
    char comment[80];	/* COMMENT */
    char title[80];	/* TITLE */
    char observer[80];	/* OBSERVER */
    Obj obj;		/* definition of object, including name */
    double rao, deco;	/* additional ra and dec offsets, rads */
//    double extval1, extval2;  /* keyword extension value fields (added 3/7/02) */
    char extcmd[1024];  /* The open-ended extension hook that changes the ext. stuff more (STO 10/29/09) */

    /* camera settings */
    CCDCalib ccdcalib;	/* how/whether to calibrate, + keyword extensions (added 3/7/02) */
    int compress;	/* 0 for no compression, else a scale factor */
    int sx, sy, sw, sh;	/* SUBIMAGE upper left and size */
    int binx, biny;	/* BINNING in each direction */
    double dur;		/* DURATION, secs */
    CCDShutterOptions shutter;	/* how to operate shutter during exposure */
    char filter;	/* FILTER, first char only */

    /* run details */
    int priority;       /* assign lower values first */
    int running;	/* set when actually in progress */
    time_t starttm;	/* if not 0 when cur run began or next starts */
    int startdt;	/* allowed starttm +/- tolerance, seconds */
    char status;	/* .sls status code: N(ew)/D(one)/F(ail) */
} Scan;

// define a special value of no entry of ALT AZ values
#define NO_ALTAZ_ENTRY -99

/* helper functions */
extern int readNextSLS (FILE *fp, Scan *sp, long *offset);
extern char *ccdCalib2Str (CCDCalib);
extern int ccdStr2Calib (char *s, CCDCalib *cp);
extern int ccdStr2ExtAct(char *s, CCDCalib *cp);
extern char * ccdExtAct2Str(CCDCalib c);
extern char *extActValueStr(Scan *sp);
extern void ccdCalibStrs (char *names[CAL_NSTR]);
extern char *ccdSO2Str (CCDShutterOptions o);
extern int ccdStr2SO (char *s, CCDShutterOptions *op);
extern void ccdSOStrs (char *names[CCDSO_NSTR]);

/* used to copy string s to destination array d and insure the last char of d
 * is untouched.
 */
#define ACPYZ(d,s)      (void)strncpy(d,s,sizeof(d)-1)

#endif /* _SCAN_H */

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: scan.h,v $ $Date: 2006/05/28 01:07:19 $ $Revision: 1.5 $ $Name:  $
 */
