/* include file for using the config file tools */

/* describes each parameter to find */
typedef enum {
    CFG_INT, CFG_DBL, CFG_STR
} CfgType;
typedef struct {
    char *name;		/* name of parameter */
    CfgType type;	/* type */
    void *valp;		/* pointer to value */
    int slen;		/* if CFG_STR, length of array at valp */
    int found;		/* set if found */
}  CfgEntry;

typedef	void (*CfgPrFp)();

extern int readCfgFile (int trace, char *fn, CfgEntry cea[], int ncea);
extern int read1CfgEntry (int trace, char *fn, char *name, CfgType t,
    void *vp, int slen);
extern void cfgFileError (char *fn,int rv, CfgPrFp fp, CfgEntry ca[], int nca);
extern int cfgFound (char *name, CfgEntry cea[], int ncea);
extern int writeCfgFile (char *fn, char *name, char *value, char *cmt);

extern int nextPair (FILE *fp, char fn[], char name[], int maxname,
    char value[], int maxvalue);
extern int readFilenames (char *fn, char ***fnames);
extern void decomposeFN (char *fn, char dir[], char base[], char ext[]);

/* support for swallowing the array from the filter.cfg */
typedef struct {
    char name[32];	/* filter name */
    double flatdur;	/* duration of flat, seconds */
    int flatlights;	/* light setting, 1..3 */
    int dflt0 : 1;	/* set if *0 are from defaults, not actual entry */
    int dflt1 : 1;	/* set if *1 are from defaults, not actual entry */
    double f0;		/* focus position 0: position from home, microns */
    double t0;		/* focus position 0: tempeature, C */
    double f1;		/* focus position 1: position from home, microns */
    double t1;		/* focus position 1: tempeature, C */
} FilterInfo;

extern int readFilterCfg (int trace, char *fn, FilterInfo **fipp, int *defaultp,
    char errmsg[]);
extern int writeFilterCfg (char *fn, FilterInfo *fip, int nfip, int filtn,
    char errmsg[]);

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: configfile.h,v $ $Date: 2001/04/19 21:12:14 $ $Revision: 1.1.1.1 $ $Name:  $
 */
