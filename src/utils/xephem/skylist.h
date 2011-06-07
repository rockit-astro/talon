/* include file to hook skyviewmenu.c and skylist.c together.
 */

/* Obj.flags or TSky flags values */
#define OBJF_ONSCREEN	FUSER0	/* bit set if obj is on screen */
#define OBJF_RLABEL	FUSER1	/* set if right-label is to be on */
#define OBJF_LLABEL	FUSER4	/* set if left-label is to be on */
#define OBJF_PERSLB	(OBJF_RLABEL|OBJF_LLABEL) /* either means persistent */
#define OBJF_NLABEL	FUSER5	/* set if name-label is to be on */
#define OBJF_MLABEL	FUSER6	/* set if name-label is to be on */


/* skyviewmenu.c */
extern void sv_all P_((Now *np));
extern void sv_getcenter P_((int *aamode, double *fov,
    double *altp, double *azp, double *rap, double *decp));
extern void sv_getfldstars P_((ObjF **fsp, int *nfsp));
extern Widget svshell_w;

/* skylist.c */
extern void sl_manage P_((void));
extern void sl_unmanage P_((void));


/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: skylist.h,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $
 */
