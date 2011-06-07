#ifndef _PS_H
#define _PS_H

/* public include file for X postscript interface, ps.c.
 * requires stdio.h, Xlib.h and P_.h.
 */

#include "camera.h"

extern void XPSAsk P_((char *title, void (go)(void)));
extern void XPSRegisterFont P_((Font fid, char *fname));
extern void XPSXBegin P_((Window win, int Xx0, int Xy0, int Xw, int Xh, int Px0,
    int Py0, int Pw, int Ph));
extern void XPSXEnd P_((void));
extern void XPSDirect P_((char *s));
extern char *XPSCleanStr P_((char *s, int l));
extern void XPSClose P_((void));
extern int XPSInColor P_((void));
extern int XPSDrawing P_((void));

extern void PSFillRectangle P_((Drawable win, double gray, int x, int y,
    unsigned w, unsigned h));

extern void XPSDrawEllipse P_((Display *dsp, Drawable win, GC gc, int x, int y,
    int a0, unsigned w, unsigned h, int a1, int a2));
extern void XPSDrawArc P_((Display *dsp, Drawable win, GC gc, int x, int y,
    unsigned w, unsigned  h, int a1, int a2));
extern void XPSDrawArcs P_((Display *dsp, Drawable win, GC gc, XArc xa[], int
    nxa));
extern void XPSDrawLine P_((Display *dsp, Drawable win, GC gc, int x1, int x2,
    int y1, int y2));
extern void XPSDrawLines P_((Display *dsp, Drawable win, GC gc, XPoint xp[],
    int nxp, int mode));
extern void XPSDrawPoint P_((Display *dsp, Drawable win, GC gc, int x, int y));
extern void XPSDrawPoints P_((Display *dsp, Drawable win, GC gc, XPoint xp[],
    int nxp, int mode));
extern void XPSDrawRectangle P_((Display *dsp, Drawable win, GC gc,
    int x, int y, unsigned w, unsigned h));
extern void XPSDrawRectangles P_((Display *dsp, Drawable win, GC gc,
    XRectangle xra[], int nxr));
extern void XPSDrawSegments P_((Display *dsp, Drawable win, GC gc,
    XSegment xs[], int nxs));
extern void XPSDrawString P_((Display *dsp, Drawable win, GC gc, int x, int y,
    char *s, int l));
extern void XPSFillArc P_((Display *dsp, Drawable win, GC gc, int x, int y,
    unsigned w, unsigned h, int a1, int a2));
extern void XPSFillArcs P_((Display *dsp, Drawable win, GC gc, XArc xa[], int
    nxa));
extern void XPSFillPolygon P_((Display *dsp, Drawable win, GC gc, XPoint xp[],
    int nxp, int shape, int mode));
extern void XPSFillRectangle P_((Display *dsp, Drawable win, GC gc,
    int x, int y, unsigned w, unsigned h));
extern void XPSFillRectangles P_((Display *dsp, Drawable win, GC gc,
    XRectangle xra[], int nxr));
extern void XPSImage P_((Drawable win, unsigned short pix[], char cm[],
    int maxcm, int inv, AOI *aoip, double scale, int w, int h));

/* annotation y coord from row */
#define	ANNOT_PTSZ	10	/* font size for annotation, PS's points.
				 * if change, must redesign all annotations.
				 */
#define	AROWY(r)	(72 + (ANNOT_PTSZ+1)*(r))

#endif // _PS_H

/* For RCS Only -- Do Not Edit
 * @(#) $RCSfile: ps.h,v $ $Date: 2001/05/23 15:34:58 $ $Revision: 1.2 $ $Name:  $
 */
