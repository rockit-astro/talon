/* glue for skyview tool bar */

#define	SVTB_NHIST	4	/* number of History references */

extern void svtb_create P_((Widget trc_w, Widget lrc_w));
extern void svtb_newpm P_((Widget rc_w));

extern void svtb_brighter_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_constel_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_dimmer_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_grid_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_automag_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_hzn_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_fstars_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_names_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_planes_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_orient_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_proj_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_flip_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_print_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_zoomin_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_unzoom_cb P_((Widget w, XtPointer client, XtPointer call));
extern void svtb_zoomok P_((int whether));
extern void svtb_unzoomok P_((int whether));
extern int svtb_iszoomok P_((void));
extern void svtb_updateCTTT P_((char fclass_table[NCLASSES],
    char type_table[NOBJTYPES]));
extern void svtb_updateHorizon P_((int on));
extern void svtb_updateFStars P_((int on));
extern void svtb_updateGrid P_((int on));
extern void svtb_updateLRFlip P_((int on));
extern void svtb_updateTBFlip P_((int on));
extern void svtb_updateAutoMag P_((int on));
extern void svtb_updateCyl P_((int on));
extern void svtb_updateCns P_((int on));
extern void svtb_updateNames P_((int on));
extern void svtb_updatePlanes P_((int on));


/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: skytoolbar.h,v $ $Date: 2001/04/19 21:12:02 $ $Revision: 1.1.1.1 $ $Name:  $
 */
