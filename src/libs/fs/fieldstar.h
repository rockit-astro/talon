/* fieldstar.h: used both by gsc and usno libs.
 */

/* One Field star */
typedef struct {
    char name[14];	/* "GSC NNNN-NNNN" or "SA1.0 NNNNNN" */
    char isstar;	/* 1 if a star, 0 if something else */
    float ra, dec;	/* J2000, rads */
    float mag;		/* magnitude */
} FieldStar;

extern int GSCSetup (char *cdpath, char *cachepath, char msg[]);
extern int GSCFetch (double ra0, double dec0, double fov, double fmag,
    FieldStar **spp, int nspp, char msg[]);

extern int USNOSetup (char *cdpath, int wantgsc, char *msg);
extern int USNOFetch (double ra0, double dec0, double fov, double fmag,
    FieldStar **spp, char msg[]);

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: fieldstar.h,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $
 */
