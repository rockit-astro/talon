/* scangraphic.h - added by KMI, 11/11/02 */

#include "telsched.h"

#define	SG_W	800	/* table width */
#define	SG_H	200	/* table height */

static void sg_create_form (void);
static void sg_da_exp_cb (Widget w, XtPointer client, XtPointer call);
static void sg_close_cb (Widget w, XtPointer client, XtPointer call);
static void sg_all (void);
static void sg_stats (Obs *op, int nop);
static void make_gcs (Display *dsp, Window win);

static Widget sgform_w;	/* overall form dialog */
static Widget sgda_w;	/* drawing area */
static Widget stats_w;	/* label to hold the stats */

static char clsn_c[] = "#ee4040";	/* color for collisions */
static char offn_c[] = "#808080";	/* color for items marks off */
static GC c_gc, o_gc,w_gc,fg_gc,bg_gc;	/* GCs */
