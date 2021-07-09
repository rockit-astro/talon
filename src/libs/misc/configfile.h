/* include file for using the config file tools */

/* describes each parameter to find */
typedef enum
{
    CFG_INT,
    CFG_DBL,
    CFG_STR
} CfgType;
typedef struct
{
    char *name;   /* name of parameter */
    CfgType type; /* type */
    void *valp;   /* pointer to value */
    int slen;     /* if CFG_STR, length of array at valp */
    int found;    /* set if found */
} CfgEntry;

typedef void (*CfgPrFp)();

extern int readCfgFile(int trace, char *fn, CfgEntry cea[], int ncea);
extern int read1CfgEntry(int trace, char *fn, char *name, CfgType t, void *vp, int slen);
extern void cfgFileError(char *fn, int rv, CfgPrFp fp, CfgEntry ca[], int nca);
extern int cfgFound(char *name, CfgEntry cea[], int ncea);
extern int writeCfgFile(char *fn, char *name, char *value, char *cmt);

extern int nextPair(FILE *fp, char fn[], char name[], int maxname, char value[], int maxvalue);
extern int readFilenames(char *fn, char ***fnames);
extern void decomposeFN(char *fn, char dir[], char base[], char ext[]);
