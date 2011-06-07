/* functions which connect to many other functions */

#include <stdio.h>
#include <math.h>

#if defined(__STDC__)
#include <stdlib.h>
#include <string.h>
#endif

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"

extern Widget	toplevel_w;
#define	XtD	XtDisplay(toplevel_w)

extern Now *mm_get_now P_((void));
extern int any_ison P_((void));
extern int listing_ison P_((void));
extern int plot_ison P_((void));
extern int srch_ison P_((void));
extern void av_cursor P_((Cursor c));
extern void c_cursor P_((Cursor c));
extern void c_update P_((Now *np, int force));
extern void db_cursor P_((Cursor c));
extern void db_newdb P_((int appended));
extern void db_newobj P_((int appended));
extern void db_update P_((Obj *op));
extern void dm_cursor P_((Cursor c));
extern void dm_newobj P_((int dbidx));
extern void dm_selection_mode P_((int whether));
extern void dm_update P_((Now *np, int how_much));
extern void e_cursor P_((Cursor c));
extern void e_newobj P_((int dbidx));
extern void e_selection_mode P_((int whether));
extern void e_update P_((Now *np, int force));
extern void fs_cursor P_((Cursor c));
extern void get_something P_((Widget w, char *resource, XtArgVal value));
extern void hzn_cursor P_((Cursor c));
extern void jm_cursor P_((Cursor c));
extern void jm_newdb P_((int appended));
extern void jm_selection_mode P_((int whether));
extern void jm_update P_((Now *np, int how_much));
extern void lst_cursor P_((Cursor c));
extern void lst_log P_((char *name, char *str));
extern void lst_selection P_((char *name));
extern void m_cursor P_((Cursor c));
extern void m_newdb P_((int appended));
extern void m_selection_mode P_((int whether));
extern void m_update P_((Now *np, int how_much));
extern void main_cursor P_((Cursor c));
extern void mars_cursor P_((Cursor c));
extern void mars_selection_mode P_((int whether));
extern void mars_update P_((Now *np, int force));
extern void mm_selection_mode P_((int whether));
extern void marsm_cursor P_((Cursor c));
extern void marsm_newdb P_((int appended));
extern void marsm_selection_mode P_((int whether));
extern void marsm_update P_((Now *np, int how_much));
extern void msg_cursor P_((Cursor c));
extern void ng_cursor P_((Cursor c));
extern void ng_update P_((Now *np, int force));
extern void ng_newobj P_((int appended));
extern void obj_cursor P_((Cursor c));
extern void obj_newdb P_((int appended));
extern void obj_update P_((Now *np, int howmuch));
extern void plt_cursor P_((Cursor c));
extern void plt_log P_((char *name, double value));
extern void plt_selection P_((char *name));
extern void pm_cursor P_((Cursor c));
extern void XPS_cursor P_((Cursor c));
extern void sah_cursor P_((Cursor c));
extern void sf_cursor P_((Cursor c));
extern void sl_cursor P_((Cursor c));
extern void sm_cursor P_((Cursor c));
extern void sm_newdb P_((int appended));
extern void sm_selection_mode P_((int whether));
extern void sm_update P_((Now *np, int how_much));
extern void sr_cursor P_((Cursor c));
extern void srch_cursor P_((Cursor c));
extern void srch_log P_((char *name, double value));
extern void srch_selection P_((char *name));
extern void srch_selection_mode P_((int whether));
extern void ss_cursor P_((Cursor c));
extern void ss_newdb P_((int appended));
extern void ss_newobj P_((int dbidx));
extern void ss_update P_((Now *np, int how_much));
extern void sv_cursor P_((Cursor c));
extern void sv_newdb P_((int appended));
extern void sv_newobj P_((int dbidx));
extern void sv_update P_((Now *np, int how_much));
extern void svh_cursor P_((Cursor c));
extern void svf_cursor P_((Cursor c));
extern void um_cursor P_((Cursor c));
extern void um_newdb P_((int appended));
extern void um_selection_mode P_((int whether));
extern void um_update P_((Now *np, int how_much));
extern void v_cursor P_((Cursor c));
extern void wt_cursor P_((Cursor c));

/* called to set or unset the watch cursor on all menus.
 * allow for nested requests.
 */
void
watch_cursor(want)
int want;
{
	static Cursor wc;
	static int nreqs;
	Cursor c;

	if (!wc)
	    wc = XCreateFontCursor (XtD, XC_watch);

	if (want) {
	    if (nreqs++ > 0)
		return;
	    c = wc;
	} else {
	    if (--nreqs > 0)
		return;
	    c = (Cursor)0;
	}

	XPS_cursor(c);
	av_cursor(c);
	c_cursor(c);
	db_cursor(c);
	dm_cursor(c);
	e_cursor(c);
	fs_cursor(c);
	hzn_cursor(c);
	jm_cursor(c);
	lst_cursor(c);
	m_cursor(c);
	main_cursor(c);
	mars_cursor(c);
	marsm_cursor(c);
	msg_cursor(c);
	ng_cursor(c);
	obj_cursor(c);
	plt_cursor(c);
	pm_cursor(c);
	sah_cursor(c);
	sf_cursor(c);
	sl_cursor(c);
	sm_cursor(c);
	sr_cursor(c);
	srch_cursor(c);
	ss_cursor(c);
	sv_cursor(c);
	svf_cursor(c);
	svh_cursor(c);
	um_cursor(c);
	v_cursor(c);
	wt_cursor(c);
 
	XFlush (XtD);
	XmUpdateDisplay (toplevel_w);
}

/* update stuff on all major views */
void
all_update(np, how_much)
Now *np;
int how_much;
{
	dm_update (np, how_much);
	mars_update (np, how_much);
	marsm_update (np, how_much);
	ng_update (np, how_much);
	e_update (np, how_much);
	jm_update (np, how_much);
	sm_update (np, how_much);
	um_update (np, how_much);
	ss_update (np, how_much);
	sv_update (np, how_much);
	m_update  (np, how_much);
	obj_update (np, how_much);
	c_update (np, how_much);
}

/* tell everyone who might care that a user-defined object has changed
 * then recompute and redisplay new values.
 */
void
all_newobj(dbidx)
int dbidx;
{
	watch_cursor (1);

	db_newobj(dbidx);
	dm_newobj(dbidx);
	e_newobj (dbidx);
	ng_newobj (dbidx);
	ss_newobj(dbidx);
	sv_newobj(dbidx);
	all_update (mm_get_now(), 1);

	watch_cursor (0);
}

/* tell everyone who might care that the db (beyond NOBJ) has changed.
 * appended is true if it grew; else it was deleted.
 */
void
all_newdb(appended)
int appended;
{
	watch_cursor (1);

	obj_newdb(appended);
	sm_newdb(appended);
	jm_newdb(appended);
	ss_newdb(appended);
	um_newdb(appended);
	marsm_newdb(appended);
	sv_newdb(appended);
	db_newdb (appended);
	m_newdb (appended);

	watch_cursor (0);
}

/* inform all menus that have something selectable for plotting/listing/srching
 * wether we are now in a mode that they should report when those fields are
 * selected.
 */
void
all_selection_mode(whether)
int whether;
{
	dm_selection_mode(whether);
	mm_selection_mode(whether);
	jm_selection_mode(whether);
	mars_selection_mode(whether);
	e_selection_mode(whether);
	sm_selection_mode(whether);
	um_selection_mode(whether);
	marsm_selection_mode(whether);
	srch_selection_mode(whether);
	m_selection_mode(whether);
}

/* inform all potentially interested parties of the name of a field that
 * it might want to use for latter.
 * this is just to collect in one place all the modules that gather care.
 */
void
register_selection (name)
char *name;
{
	plt_selection (name);
	lst_selection (name);
	srch_selection (name);
}

/* if we are plotting/listing/searching, send the current field info to them.
 * N.B. only send `value' to plot and search if logv is not 0.
 */
void
field_log (w, value, logv, str)
Widget w;
double value;
int logv;
char *str;
{
	char *name;

	if (!any_ison())
	    return;

	get_something (w, XmNuserData, (XtArgVal)&name);
	if (name) {
	    if (logv) {
		plt_log (name, value);
		srch_log (name, value);
	    }
	    lst_log (name, str);
	}
}

/* return !0 if any of the button/data capture tools are active, else 0.
 */
int
any_ison()
{
	return (srch_ison() || plot_ison() || listing_ison());
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: broadcast.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
