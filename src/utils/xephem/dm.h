/* used by funcs who want to print like datamenu */

/* identifiers for each entry in a Data Table column.
 * N.B. these must match the order in the col[] array.
 */
typedef enum {
    CONSTEL_ID, RA_ID, HA_ID, DEC_ID, AZ_ID, ALT_ID, ZEN_ID, Z_ID, SIZE_ID,
    VMAG_ID, PHS_ID, HLAT_ID, HLONG_ID, GLAT_ID, GLONG_ID, ECLAT_ID, ECLONG_ID,
    EDST_ID, ELGHT_ID, SDST_ID, SLGHT_ID, ELONG_ID,
    RSTIME_ID, RSAZ_ID, TRTIME_ID, TRALT_ID, SETTIME_ID, SETAZ_ID, HRSUP_ID,
    SEP_SUN_ID, SEP_MOON_ID, SEP_MERCURY_ID, SEP_VENUS_ID, SEP_MARS_ID,
    SEP_JUPITER_ID, SEP_SATURN_ID, SEP_URANUS_ID, SEP_NEPTUNE_ID, SEP_PLUTO_ID,
    SEP_OBJX_ID, SEP_OBJY_ID, SEP_OBJZ_ID,
    NDMCol
} DMCol;

extern int dm_colHeader P_((DMCol c, char str[]));
extern int dm_colFormat P_((Now *np, Obj *op, RiseSet *rp, DMCol c, char *str));

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: dm.h,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $
 */
