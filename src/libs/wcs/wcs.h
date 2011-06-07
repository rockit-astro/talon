/* requires fits.h */

extern int xy2RADec(FImage *fip, double x, double y, double *rap, double *decp);

extern int RADec2xy(FImage *fip, double ra, double dec, double *xp, double *yp);

extern int worldpos(double xpix, double ypix, double xref, double yref,
      double xrefpix, double yrefpix, double xinc, double yinc, double rot,
      char *type, double *xpos, double *ypos);

extern int xypix(double xpos, double ypos, double xref, double yref, 
      double xrefpix, double yrefpix, double xinc, double yinc, double rot,
      char *type, double *xpix, double *ypix);

extern int setWCSFITS (FImage *fip, int tusno, double hunt, int (*bail_out)(),
    int verbose, char msg[]);
extern int checkWCSFITS (FImage *fip, int verbose);
extern int delWCSFITS (FImage *fip, int verbose);
extern int align2WCS (FImage *fip1, FImage *fip2, int *dxp,int *dyp,char msg[]);
extern void resetWCS (FImage *fip0, FImage *fip1, int x, int y, int w, int h);

// Define this as 1 to use David Asher's "distance method" WCS Registration matching code
// and to enable his DSS-like higher order astrometric solution support
// Defining as 0 or leaving undefined reverts to the original Triangle-match WCS method
// implemented by Elwood
// NOTE: Remember to check ip.cfg for needed settings!
#define USE_DISTANCE_METHOD	1
