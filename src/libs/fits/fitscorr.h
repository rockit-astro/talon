/* function prototypes for using fitscorr.c */
extern int useMeanBias;
extern int useMeanTherm;
extern int useMeanFlat;

extern void readCorrectionCfg(int trace, char *cfgFile);
extern int correctFITS (FImage *fip, char biasfn[], char thermfn[],
    char flatfn[], char errmsg[]);
extern unsigned short pixRange(double f);
extern int findBiasFN (FImage *matchfip, char caldir[], char fn[],
    char errmsg[]);
extern int findThermFN (FImage *matchfip, char caldir[], char fn[],
    char errmsg[]);
extern int findFlatFN (FImage *matchfip, int filter, char caldir[], char fn[],
    char errmsg[]);
extern int findMapFN (char caldir[], char fn[], char errmsg[]);
extern int findNewBiasFN (char caldir[], char fn[], char errmsg[]);
extern int findNewThermFN (char caldir[], char fn[], char errmsg[]);
extern int findNewFlatFN (int filter, char caldir[], char fn[], char errmsg[]);
extern int findNewMapFN(char caldir[], char fn[], char errmsg[]); /* STO20010405 */
extern void computeMeanFITS (FImage *fip, double *mp);
extern float *nc_makeFPAccum(int npixels);
extern void nc_accumulate (int n, float *acc, CamPixel *im);
extern void nc_accdiv (int n, float *acc, int div);
extern void nc_acc2im (int n, float *acc, CamPixel *im);
extern void nc_subtract (int n, float *acc, CamPixel *im);
extern void nc_fsubtract (int n, float *acc, CamPixel *im, double fact);
extern void nc_biaskw (FImage *fip, int ntot);
extern void nc_thermalkw (FImage *fip, int ntot, char biasfn[]);
extern void nc_flatkw (FImage *fip, int ntot, char biasfn[], char thermfn[],
    int filter);
extern int nc_applyBias(int n, float *acc, char biasfn[], char msg[]);
extern int nc_applyThermal(int n, float *acc, double dur, char thermfn[],
    char msg[]);

extern int flatQual (FImage *fip1, FImage *fip2, char *errmsg);
extern int biasQual (FImage *fip1, FImage *fip2, char *errmsg);
extern int thermQual (FImage *fip1, FImage *fip2, char *errmsg);

/* Bad column definition STO20010405 */
typedef struct{
  int	column;
  int	begin;
  int	end;
  int ladj;
  int radj;
}
BADCOL;

/* Bad Column Fix function STO20010405 */
extern int removeBadColumns(FImage *fip, BADCOL *badColMap, char *mapfileused, char *errmsg);
extern int readMapFile(char * dirname, char *mapfile, BADCOL **bcOut, char *mapnameused, char *errmsg);


/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: fitscorr.h,v $ $Date: 2002/10/23 21:46:53 $ $Revision: 1.5 $ $Name:  $
 */
