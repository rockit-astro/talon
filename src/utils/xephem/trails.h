/* include file for users of the trail module.
 */

/* codes to indicate tickmark label rate */
typedef enum {
    TRLR_1, 		/* every label */
    TRLR_2, 		/* every 2nd label */
    TRLR_5, 		/* every 5th label */
    TRLR_10, 		/* every 10th label */
    TRLR_FL, 		/* first and last */
    TRLR_ML, 		/* middle and last */
    TRLR_FML,		/* first middle and last */
    TRLR_NONE,		/* none */
    TRLR_N
} TrLR;

/*  codes to indicate tickmark intervals */
typedef enum {
    TRI_5MIN, 		/* 5 minutes */
    TRI_HOUR, 		/* one hour */
    TRI_DAY, 		/* one day */
    TRI_WEEK, 		/* one week */
    TRI_MONTH, 		/* one month */
    TRI_YEAR, 		/* one year */
    TRI_CUSTOM,		/* custom -- enter any h:m you want */
    TRI_N
} TrInt;

/* code to indicate format of label */
typedef enum {
    TRF_TIME,		/* stamp with hh:mm */
    TRF_DATE,		/* stamp with date (in current preference format) */
    TRF_N
} TrFormat;

/* code to indicate initial time-step rounding */
typedef enum {
    TRR_MIN,		/* round up to next whole minute */
    TRR_DAY,		/* round up to next whole day */
    TRR_INTER,		/* round up to next whole interval */
    TRR_NONE,		/* no rounding -- use current time */
    TRR_N
} TrRound;

/* code to indicate label orientation */
typedef enum {
    TRO_UP,		/* upwards */
    TRO_DOWN,		/* downwards */
    TRO_LEFT,		/* leftwards */
    TRO_RIGHT,		/* rightwards */
    TRO_ABOVE,		/* centered above */
    TRO_BELOW,		/* centered below */
    TRO_UPR,		/* upwards and to the right */
    TRO_DOWNR,		/* downwards and to the right */
    TRO_PATHL,		/* leftwards of current path direction */
    TRO_PATHR,		/* rightwards of current path direction */
    TRO_N
} TrOrient;

/* code to indicate the desired label size.
 * see *_SIZE #defines in trails.c for actual pixel sizes.
 */
typedef enum {
    TRS_SMALL,		/* small label */
    TRS_MEDIUM,		/* medium label */
    TRS_LARGE,		/* large label */
    TRS_HUGE,		/* huge label */
    TRS_N
} TrSize;

/* state of trail options */
typedef struct {
    TrLR l;
    TrInt i;
    TrFormat f;
    TrRound r;
    TrOrient o;
    TrSize s;
    int nticks;
    double customi;	/* custom interval, days (use only if i==TRI_CUSTOM) */
} TrState;

/* one time stamp and whether it should be labeled */
typedef struct {
    double t;		/* mjd of time stamp */
    int lbl;		/* whether this mark should be labeled */
} TrTS;

/* user's function to call when Ok/Apply is pressed */
typedef int (*TrCB)(
#if NeedFunctionPrototypes
    TrTS ts[],		/* time stamps -- count is in state->nticks */
    TrState *state,	/* all trail options */
    XtPointer client	/* saved by tr_setup() and returned to callback */
#endif
);

/* trails.c */
extern void tr_setup P_((char *title, char *hdr, TrState *initial_sp,
    TrCB cb, XtPointer client));
extern void tr_draw P_((Display *dsp, Drawable win, GC gc, int ticklen,
    TrTS *tp, TrTS *ltp, TrState *sp, int lx, int ly, int x, int y));

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: trails.h,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $
 */
