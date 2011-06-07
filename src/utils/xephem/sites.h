/* interface to e_read_sites(). */

/* this is to form a list of sites */
typedef struct {
    float si_lat;	/* lat (+N), rads */
    float si_lng;	/* long (+E), rads */
    float si_elev;	/* elevation above sea level, meters (-1 means ?) */
    char si_tzdefn[32];	/* timezone info.. same format as UNIX tzset(3) */
    char si_name[40];	/* name */
} Site;

extern int sites_get_list P_((Site **sipp));
extern int sites_search P_((char *str));
extern void sites_query P_((void));
extern void sites_abbrev P_((char *full, char ab[], int maxn));

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: sites.h,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $
 */
