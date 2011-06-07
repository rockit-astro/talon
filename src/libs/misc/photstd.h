/* include file for photmetric calibrator stars support code.
 * it is all based on Landolt fields.
 */

/* observed pixel count and airmass for one star */
typedef struct {
    char fn[32];		/* base filename */
    double Vobs;		/* observed magnitudes computed from pixels.
				 * scaled to exp duration.
				 */
    double Verr;		/* error in measurement */
    double Z;			/* airmass */
} OneStar;

/* one standard photometric star */
typedef struct {
    /* info from photcal.ref file */
    Obj o;			/* star name and location, type FIXED */
    double Bm, Vm, Rm, Im;	/* true apparent magnitude in each color */

    /* info from the images. these get malloced as needed. */
    OneStar *Bp; int nB;
    OneStar *Vp; int nV;
    OneStar *Rp; int nR;
    OneStar *Ip; int nI;
} PStdStar;

/* one set of best-fit values */
typedef struct {
    double BV0, BV0e, Bkp, Bkpe, Bkpp, Bkppe;
    double VV0, VV0e, Vkp, Vkpe, Vkpp, Vkppe;
    double RV0, RV0e, Rkp, Rkpe, Rkpp, Rkppe;
    double IV0, IV0e, Ikp, Ikpe, Ikpp, Ikppe;
} PCalConst;

#define	ABSAP	5		/* aperture used for abs photometry, pixels */

extern int photStdRead (FILE *fp, PStdStar **spp);
extern void photFree (PStdStar *sp, int n);
extern int photReadCalConst (FILE *fp, double jd, double jdwin, PCalConst *cp,
    double *foundjdp, char *msg);
extern int photReadDefCalConst (FILE *fp, PCalConst *cp, double *Bp,
    double *Vp, double *Rp, double *Ip, char *msg);

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: photstd.h,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $
 */
