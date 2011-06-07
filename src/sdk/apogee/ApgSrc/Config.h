#ifndef _config
#define _config

#ifdef __cplusplus
extern "C" {
#endif

#define CFG_OK          0
#define CFG_NOFILE      1
#define CFG_NOPATH      2
#define CFG_HAVEMORE    3
#define CFG_HAVEPARM    4 
#define CFG_NOSECT      5
#define CFG_NOPARM      6
#define CFG_BADNAME     7
#define CFG_DOSERR      8

short   CfgGet (char  *ininame,
                char  *inisect,
                char  *iniparm,
                char  *retbuff,
                short bufflen,
                short *parmlen);


short   CfgAdd (char  *ininame,
                char  *inisect,
                char  *iniparm,
                char  *inival);

#ifdef __cplusplus
}
#endif

#endif



