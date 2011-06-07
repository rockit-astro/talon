/* include file for fitslib.c
 */

#define	FITS_HROWS	36
#define	FITS_HCOLS	80
typedef char		FITSRow[FITS_HCOLS];

typedef struct {
    /* following fields are cracked from the header for easy reference */
    int bitpix;		/* handy BITPIX */
    int sw, sh;		/* handy NAXIS1 and NAXIS2 */

    FITSRow *var;	/* malloced array of all unrecognized header lines */
    int nvar;		/* number of var[] */

    char *image;	/* malloced image array of sw*sh*(bitpix/8) bytes */
    int nbytes;		/* bytes read so far .. used for incremental loading */
    int totbytes;	/* total bytes to be read .. aka, size of image[] */

    /* cache of WCS info from recent FImage */
    int wcsset;		/* set when following are ok */
    double xref;	/* x reference coordinate value (deg) */
    double yref;	/* y reference coordinate value (deg) */
    double xrefpix;	/* x reference pixel */
    double yrefpix;	/* y reference pixel */
    double xinc;	/* x coordinate increment (deg) */
    double yinc;	/* y coordinate increment (deg) */
    double rot;		/* rotation (deg)  (from N through E) */
    char type[5];	/* projection code, see worldpos() */
} FImage;

extern int writeFITS P_((int fd, FImage *fip, char *errmsg, int restore));
extern int readFITS P_((int fd, FImage *fip, char *errmsg));
extern int readIncFITS P_((int fd, FImage *fip, char *errmsg));
extern int readFITSHeader P_((int fd, FImage *fip, char *errmsg));
extern int writeSimpleFITS P_((int fd, char *pix, int w, int h, int restore));
extern int getNAXIS P_((FImage *fip, int *n1p, int *n2p, char errmsg[]));
extern void initFImage P_((FImage *fip));
extern void resetFImage P_((FImage *fip));
extern void setSimpleFITSHeader P_((FImage *fip));
extern void setLogicalFITS P_((FImage *fip, char *name, int v, char *comment));
extern void setIntFITS P_((FImage *fip, char *name, int v, char *comment));
extern void setRealFITS P_((FImage *fip, char *name, double v, int sigdig,
    char *comment));
extern void setCommentFITS P_((FImage *fip, char *name, char *comment));
extern void setStringFITS P_((FImage *fip, char *name, char *string, char *comment));
extern int getLogicalFITS P_((FImage *fip, char *name, int *vp));
extern int getIntFITS P_((FImage *fip, char *name, int *vp));
extern int getRealFITS P_((FImage *fip, char *name, double *vp));
extern int getCommentFITS P_((FImage *fip, char *name, char *buf));
extern int getStringFITS P_((FImage *fip, char *name, char *string));
extern void addFImageVar P_((FImage *fip, FITSRow row));
extern int delFImageVar P_((FImage *fip, char *name));
extern int cpyFImageVar P_((FImage *dstfip, FImage *srcfip, char *name));

typedef unsigned short CamPixel;		/* type of pixel */
#define	NCAMPIX	(1<<(int)(8*sizeof(CamPixel)))	/* number of unique CamPixels */
#define	MAXCAMPIX	(NCAMPIX-1)		/* largest value in a CamPixel*/

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: fits.h,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $
 */
