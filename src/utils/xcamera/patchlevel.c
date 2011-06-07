#include "patchlevel.h"

char PATCHLEVEL[] = "3.44";

/* history:
 * 3.44 22 May 06: (STO): Merges 3.42/3.43 KMI/STO changes together
 * 3.43 28 Dec 04: (KMI): Changed default colors
                          Changed histo from -1SD - +2SD to -1SD - +3SD
 * 3.42 19 Oct 03: (KMI): Made "Snap to Max, Show Plots" default options in
 *                        glass screen
 * 3.42  9 Mar 03: (STO): Updated for new smear detection library support 
 * 3.41 25 Nov 02: (STO): Updated for new streak detection library support
 * 3.40  7 Oct 02: (STO): correction image updates: bad columns at end, avg by median, cfg file residual control
 * 3.39 22 Jul 01: (STO): Add optional redefinition of listener FIFO
 * 3.38 29 Jun 01: (JCA): Add find linear streak options
 * 3.37 11 May 01: (STO): Apply bad column fix to correction images (corr.c, camera.c)
 * 3.36 22 Apr 01: (STO): Add bad column map support (calimage, libfits, camera/corr.c)
 * 3.35  9 Jan 01: Mark stars now uses findStatStars()
 * 3.34 15 Dec 00: use camera.cfg (not X) for driver/auxcam (same as camerad)
 * 3.33  7 Dec 00: AOI width was off by 1
 * 3.32 24 Nov 00: tweaks for X drawing GSC and Star
 * 3.31 11 Nov 00: tweaks for auxcam
 * 3.30	27 May 00: add more handy shortcuts to Exposure setup.
 * 3.29	22 May 00: tweaks to Markers and to support triggered exp.
 * 3.28	15 May 00: mods for AUXCAM.
 * 3.27  9 May 00: add support for AUXCAM
 * 3.26 14 Mar 00: each marker: sep color, cam or image coords
 * 3.25 13 Mar 00: add new Markers tool
 * 3.24  8 Feb 00: add stats alone measure line
 * 3.23  4 Jan 00: DATE-OBS format change
 * 3.22  9 Nov 99: better checking of aoi inside new image. add ScreenEdge.
 * 3.21  8 Nov 99: update whole screen when scroll bars in use
 * 3.20 29 Sep 99: support drivers than report whether they support drift scan
 * 3.19 16 Aug 99: add basic continuous drift scan support
 * 3.18  9 Feb 99: allow Mark Header to use new OBJRA/OBJDEC
 *                 add CCDSO_Multi shutter support.
 * 3.17  2 Feb 99: use only most recent if several sent to CameraFilename
 * 3.16 28 Jan 99: add immediate shutter controls
 * 3.15 18 Nov 98: fix printing when cropped or mag != 1
 * 3.14 12 Nov 98: check for a bad Save dir
 * 3.13 10 Nov 98: fix count when sh/by not whole (sw/bx ok)
 * 3.12  8 Nov 98: allow Basic size to grow. maintain AOI in movies.
 * 3.11  6 Nov 98: "just raise" broke photom.
 * 3.10 25 Oct 98: reselecting tools now raises; better blink resize;
 *                 move blink.idx into config; change Blink ref to just load.
 * 3.09 23 Oct 98: Tools/"Basics"; add 1/8mag; AOI Reset sets mag=1x.
 * 3.08 23 Oct 98: add ASCII dump.
 * 3.07 18 Oct 98: fix exploding size when not in virtual window.
 * 3.06 16 Oct 98: no Expose if no ccdcamera
 * 3.05 15 Oct 98: add -prfb
 * 3.04 14 Oct 98: blinking: change ngc.idx name and use strcwcmp()
 * 3.03 29 Sep 98: tweak glass and basic stats display (same math)
 * 3.02 28 Sep 98: update AOI even when not reset; keep new image on screen.
 *                 glass and aoi SD>M now computed with just max star's stats.
 * 3.01 22 Sep 98: add double-expose
 * 3.00 11 Sep 98: save adds .fts if not ext, dir defaults to .
 * 2.99  8 Sep 98: enforce flat dur > 0; better default FILTER if no shm.
 * 2.98 13 Aug 98: more improvements to gnuplot feature.
 * 2.97  4 Aug 98: capture scope info too at end of exp. display fake flat.
 * 2.96 29 Jul 98: active listen fifo detection. no more NOLISTEN.
 * 2.95 27 Jul 98: send AOI to gnuplot
 * 2.94 25 Jul 98: allow editing wcs search seed values; fix wcs expose problem.
 * 2.93 24 Jul 98: right button set new measure ref.
 * 2.92 23 Jul 98: better measure. user-stoppable wcs.
 * 2.91 21 Jul 98: new movie loop (now a tool); recompute aoi stats after arith
 * 2.90 15 Jul 98: can make flake flat; close cam after setting init temp
 * 2.89 13 Jul 98: arrows for exp count
 * 2.88 28 Jun 98: add support for GE mounts
 * 2.87  6 Jun 98: guard using hist before any image
 * 2.86 20 May 98: switch to nc_* fitscorr functions; better FITS look.
 * 2.85 14 Apr 98: dur arrows and 1 exp shortcuts
 * 2.84  9 Mar 98: add Camera.Driver,FifoName resources; open/clse fifo ech time
 * 2.83 23 Feb 98: add support for USNO cdrom
 * 2.82  9 Feb 98: better histogram; SD>M in AOI and Glass;
 * 2.81  7 Feb 98: fix bug in autosave incrementing.
 * 2.80  5 Feb 98: ac/av order;
 * 2.79 23 Jan 98: better exp dialog management; smart auto filenames
 *      25 Jan 98: mag to 1 when uncrop; roam option; AOI Maxat; combine menus
 * 2.78 22 Jan 98: image flipping; user can set exp count; fix "Set from AOI"
 * 2.77  9 Jan 98: fix histogram binning bug;
 * 2.76 19 Dec 97: label RA/Dec in gauss plot; flip gsc colors when invert.
 * 2.75  3 Dec 97: support DEFTEMP in camera.cfg
 * 2.74 28 Nov 97: support TELHOME
 * 2.73 26 Nov 97: explicit Invert choice when printing
 * 2.72 24 Nov 97: improve gaussian fitter for very dim stars; accept file args
 * 2.71 12 Nov 97: invert b/w when printing
 * 2.70 11 Nov 97: port to inferno (no changes!)
 * 2.69 11 Nov 97: label vert rubber band axis;
 * 2.68 10 Nov 97: label GSC magnitudes. start adding printing.
 * 2.67  9 Nov 97: fixed blink registration bug
 * 2.66  7 Nov 97: remove deleted file from hist list. add Arithmetic tool.
 * 2.65  4 Nov 97: add FWHM support in header. only add real files to history.
 * 2.64 24 Oct 97: relink to get new ccdcamera.h defines.
 * 2.63  1 Oct 97: Keep full path in history
 * 2.62  8 Sep 97: add Auto save.
 * 2.61 26 Aug 97: more fields in FITS header.
 * 2.60  8 Aug 97: add support for camera.cfg
 * 2.59 24 Jul 97: fix FWHM report when no CDELT; support telstatshmp
 * 2.58 24 Jun 97: add direct control in expose dialog too.
 * 2.57	 5 Jun 97: use new generic camera .h and driver interfaces
 * 2.56	17 Jan 97: all new photometry algorithm
 *                 add x and y to rubber band tool
 * 2.55	14 Jan 97: relink to get setWCSFITS with fabs(cdelt1);
 *                 gauss report is now noise median + peak above that.
 * 2.54	25 Dec 96: fix mark ra/dec header when mag != 1:1
 * 2.53	22 Dec 96: add file history
 * 2.52 19 Dec 96: clear msg before each new image.
 * 2.51	17 Dec 96: added Mark RA/Dec option
 * 2.50 13 Dec 96: exit when out of mem; allow taking multiple flats too.
 * 2.49	11 Dec 96: add histo graph range option
 * 2.48  3 Dec 96: fewer stars needed for WCS fit; report mag and err to .001.
 * 2.47  5 Nov 96: add NOLISTEN build macro and switch to generic fifo.
 * 2.46 31 Oct 96: add comments to FLATMEAN and CRVAL1,2 FITS fields
 * 2.45 18 Sep 96: guard against cursor outside images; different rubber pixel.
 * 2.44 17 Sep 96: add cross-section plot to measure dialog.
 *		    rearrange menus a little
 * 2.42  16 Sep 96: tighten up AOI checking in blink.c
 */

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: patchlevel.c,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.9 $ $Name:  $"};
