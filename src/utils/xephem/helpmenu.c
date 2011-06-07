/* this file contains the code to put up help messages.
 * the messages come from a file or, if no file is found or there is no
 * help entry for the requested subject, a small default text is provided.
 *
 * help file format:
 *    @<tag>
 *	help for section labeled <tag> is from here to the next @
 *    +<tag>
 *	interpolate section for <tag> here then continue
 * the tags are referenced in the call to hlp_dialog().
 */

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#if defined(__STDC__)
#include <stdlib.h>
#endif
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include <Xm/Text.h>

#include "P_.h"


extern Widget toplevel_w;
#define	XtD XtDisplay(toplevel_w)
extern Colormap xe_cm;

char helpcategory[] = "Help";			/* Save category */

extern FILE *fopenh P_((char *name, char *how));
extern char *getShareDir P_((void));
extern void prompt_map_cb P_((Widget w, XtPointer client, XtPointer call));
extern void set_something P_((Widget w, char *resource, XtArgVal value));
extern void xe_msg P_((char *msg, int app_modal));

static Widget hlp_create_dialog P_((char *tag));
static void hlp_ok_cb P_((Widget w, XtPointer client, XtPointer call));
static void hlp_popdown_cb P_((Widget w, XtPointer client, XtPointer call));
static FILE *hlp_openfile P_((char *tag));
static int hlp_fillfromfile P_((char *tag, Widget txt_w, int l));
static void hlp_fillfromstrings P_((char *msg[], int nmsg, Widget txt_w));


#define	MAXLINE		128	/* longest allowable help file line */
#define	HLP_TAG		'@'	/* help file tag marker */
#define	HLP_NEST	'+'	/* help file nested tag marker */

/* put up a help dialog. it contains a scrolled text area and an Ok button.
 * make a new dialog each time so we can have several up at once.
 * this means we need an explicit callback on the Ok button to destroy it
 * again (rather than being able to use the autoUnmanage feature).
 * if can't find any help, say so.
 */
void
hlp_dialog (tag, deflt, ndeflt)
char *tag;	/* tag to look for in help file - also dialog title */
char *deflt[];	/* help text to use if tag not found */
int ndeflt;	/* number of strings in deflt[] */
{
	Widget txt_w, sh_w;

	txt_w = hlp_create_dialog (tag);

	sh_w = txt_w;
	for (sh_w = txt_w; !XtIsShell(sh_w); sh_w = XtParent(sh_w))
	    continue;

	if (hlp_fillfromfile(tag, txt_w, 0) < 0) {
	    if (!deflt || ndeflt == 0) {
		char buf[MAXLINE];
		(void) sprintf (buf, "No help for %s", tag);
		xe_msg (buf, 1);
		XtDestroyWidget (sh_w);
		return;
	    } else
		hlp_fillfromstrings(deflt, ndeflt, txt_w);
	}

	XmTextShowPosition (txt_w, (XmTextPosition)0);
	XtPopup (sh_w, XtGrabNone);

	/* everything gets destroyed if shell is popped down */
}

/* create the help window with a scrolled text area and an Ok button.
 * return the text area widget; when ready to view, popup its 3rd parent:
 *   toplevel -> form -> scrolled_window -> text.
 * pass the toplevel shell as the client parameter of the ok activate callback
 *   so it can pop it down; popdown callback then destroys the whole thing.
 */
static Widget
hlp_create_dialog (tag)
char *tag;
{
	Widget f_w;
	Widget sh_w;
	Widget t_w, cb_w;
	Arg args[20];
	char title[MAXLINE];
	int n;

	/* make the help shell and form */

	n = 0;
	XtSetArg (args[n], XmNallowShellResize, True); n++;
	(void) sprintf (title, "xephem Help on %s", tag);
	XtSetArg (args[n], XmNtitle, title); n++;
	XtSetArg (args[n], XmNcolormap, xe_cm); n++;
	XtSetArg (args[n], XmNiconName, "Help"); n++;
	XtSetArg (args[n], XmNdeleteResponse, XmUNMAP); n++;
	sh_w = XtCreatePopupShell ("HelpWindow", topLevelShellWidgetClass,
							toplevel_w, args, n);
	set_something (sh_w, XmNcolormap, (XtArgVal)xe_cm);
	XtAddCallback (sh_w, XmNpopdownCallback, hlp_popdown_cb, 0);
	/* Save handles us special because we create/destroy many */

	n = 0;
	XtSetArg (args[n], XmNmarginHeight, 10); n++;
	XtSetArg (args[n], XmNmarginWidth, 10); n++;
	XtSetArg (args[n], XmNverticalSpacing, 10); n++;
	XtSetArg (args[n], XmNfractionBase, 9); n++;
	f_w = XmCreateForm (sh_w, "HelpF", args, n);
	XtManageChild (f_w);

	/* make the Ok button */

	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 3); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 6); n++;
	cb_w = XmCreatePushButton (f_w, "Ok", args, n);
	XtAddCallback (cb_w, XmNactivateCallback, hlp_ok_cb, (XtPointer)sh_w);
	XtManageChild (cb_w);

	/* make the scrolled text area to help the help text */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, cb_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	XtSetArg (args[n], XmNeditable, False); n++;
	XtSetArg (args[n], XmNcursorPositionVisible, False); n++;
	XtSetArg (args[n], XmNmarginHeight, 5); n++;
	XtSetArg (args[n], XmNmarginWidth, 5); n++;
	t_w = XmCreateScrolledText (f_w, "HelpText", args, n);
	XtManageChild (t_w);

	return (t_w);
}

/* called whenever a help shell is popped down.
 */
/* ARGSUSED */
static void
hlp_popdown_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtDestroyWidget (w);
}

/* called on OK.
 * client is shell to pop down.
 */
/* ARGSUSED */
static void
hlp_ok_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	Widget sh_w = (Widget) client;

	XtPopdown (sh_w);
	/* popdown callback will destroy */
}

/* open the help file and position at first line after we see "@tag\n".
 * if successfull return a FILE *, else return 0.
 */
static FILE *
hlp_openfile (tag)
char *tag;
{
	static char fn[256];
	char buf[MAXLINE];
	char tagline[MAXLINE];
	FILE *fp;

	if (fn[0] == '\0')
	    (void) sprintf (fn, "%s/auxil/xephem.hlp",  getShareDir());

	fp = fopenh (fn, "r");
	if (!fp)
	    return ((FILE *)0);

	(void) sprintf (tagline, "%c%s\n", HLP_TAG, tag);
	while (fgets (buf, sizeof(buf), fp))
	    if (strcmp (buf, tagline) == 0)
		return (fp);

	(void) fclose (fp);
	return ((FILE *)0);
}

/* search help file for tag entry, then copy that entry into txt_w.
 * l is the number of chars already in txt_w.
 * also recursively follow any NESTed entries found.
 * return new length of txt_w, else -1 if error.
 */
static int
hlp_fillfromfile(tag, txt_w, l)
char *tag;
Widget txt_w;
int l;
{
	FILE *fp;
	char buf[MAXLINE];
	
	fp = hlp_openfile (tag);
	if (!fp)
	    return (-1);

	while (fgets (buf, sizeof(buf), fp)) {
	    if (buf[0] == HLP_TAG)
		break;
	    else if (buf[0] == HLP_NEST) {
		int newl;
		buf[strlen(buf)-1] = '\0';	/* remove trailing \n */
		newl = hlp_fillfromfile (buf+1, txt_w, l);
		if (newl > l)
		    l = newl;
	    } else {
		char tabbuf[8*MAXLINE];
		int in, out;

		for (in = out = 0; buf[in] != '\0'; in++)
		    if (buf[in] == '\t')
			do
			    tabbuf[out++] = ' ';
			while (out%8);
		    else
			tabbuf[out++] = buf[in];
		tabbuf[out] = '\0';

		/* tabbuf will already include a trailing '\n' */
		XmTextReplace (txt_w, l, l, tabbuf);
		l += out;
	    }
	}

	(void) fclose (fp);
	return (l);
}

static void
hlp_fillfromstrings(msg, nmsg, txt_w)
char *msg[];
int nmsg;
Widget txt_w;
{
	static char nohelpwarn[] = 
	    "No HELP file found. Set XEphem.HELPFILE to point at xephem.hlp.\n\nMinimal Help only:\n\n";
	int i, l;

	l = 0;

	XmTextReplace (txt_w, l, l, nohelpwarn);
	l += strlen (nohelpwarn);

	for (i = 0; i < nmsg; i++) {
	    XmTextReplace (txt_w, l, l, msg[i]);
	    l += strlen (msg[i]);
	    XmTextReplace (txt_w, l, l, "\n");
	    l += 1;
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: helpmenu.c,v $ $Date: 2001/04/19 21:12:01 $ $Revision: 1.1.1.1 $ $Name:  $"};
