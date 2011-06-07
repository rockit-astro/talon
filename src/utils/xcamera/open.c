/* code to ask file name and open a fresh image file */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <Xm/Xm.h>
#include <Xm/FileSB.h>

#include "xtools.h"
#include "fits.h"
#include "strops.h"
#include "fieldstar.h"
#include "telenv.h"
#include "camera.h"

static void openCB(Widget w, XtPointer client, XtPointer call);
static int prepOpen (char fn[], char errmsg[]);

static Widget open_w;		/* the main open dialog widget */

static void filefifoCB (XtPointer client, int *fdp, XtInputId *idp);
static int filefifoFD = -1;
static XtInputId listenID;

void
manageOpen()
{
	if (!open_w)
	    createOpen();

	if (XtIsManaged(open_w))
	    raiseShell (open_w);
	else
	    XtManageChild (open_w);
}

/* create the FSB */
void
createOpen()
{
	Arg args[20];
	int n;

	n = 0;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	open_w = XmCreateFileSelectionDialog (toplevel_w, "OpenFSB", args, n);
	set_something (XtParent(open_w), XmNtitle, "Camera File Selection");
	XtAddCallback (open_w, XmNokCallback, openCB, NULL);
	XtAddCallback (open_w, XmNcancelCallback,
					(XtCallbackProc)XtUnmanageChild, NULL);
	set_xmstring (open_w, XmNokLabelString, "Apply");
	set_xmstring (open_w, XmNcancelLabelString, "Close");
}

/* --- STO 7/22/01: Minor change here to allow for fifo name to be externally set */
static char *filefifo;

void
setFilenameFifo(char *name)
{
	char commname[256];
	char buf[1024];
	sprintf(commname,"comm/%s",name);
	telfixpath(buf,commname);
	filefifo = XtNewString(buf);
}

char * getFilenameFifo(void)
{
	return filefifo;
}

/* if on, create fresh fifo and connect, if off disconnect and unlink.
 * return 0 if ok, else -1.
 */
int
listenFifoSet (int on)
{
	/* get full path */
	if (!filefifo) {
	    char buf[1024];
	    telfixpath (buf, getXRes (toplevel_w, "FifoName",
	    						"comm/CameraFilename"));
	    filefifo = XtNewString (buf);
	}

	if (on) {
	    /* make sure it's not already open for some reason */
	    if (filefifoFD >= 0) {
		(void) close (filefifoFD);
		filefifoFD = -1;
	    }

	    /* only one camera can use it at a time */
	    filefifoFD = open (filefifo, O_WRONLY|O_NONBLOCK);
	    if (filefifoFD >= 0) {
		msg ("Fifo is in use by another Camera.");
		(void) close (filefifoFD);
		filefifoFD = -1;
		return (-1);
	    }

	    /* make a new one in case it is sitting there but not a fifo */
	    (void) unlink (filefifo);
	    if (mknod (filefifo, S_IFIFO|0662, 0) < 0 && errno != EEXIST) {
		msg ("%s: %s", filefifo, strerror(errno));
		return (-1);
	    }
	    filefifoFD = open (filefifo, O_RDWR);
	    if (filefifoFD < 0) {
		msg ("%s: %s", filefifo, strerror(errno));
		return (-1);
	    }

	    /* always cooperate with user and group */
	    fchmod (filefifoFD, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);

	    /* connect up for callback when names arrive */
	    if (listenID)
		XtRemoveInput (listenID);
	    listenID = XtAppAddInput (app, filefifoFD,
		    (XtPointer)XtInputReadMask, filefifoCB,(XtPointer)filefifo);
	    msg ("Auto listen is now on.");
	} else {
	    (void) unlink (filefifo);
	    if(listenID) {
		XtRemoveInput (listenID);
		listenID = (XtInputId)0;
	    }
	    (void) close (filefifoFD);
	    filefifoFD = -1;
	    msg ("Auto listen is now off.");
	}

	return (0);
}

/* do everything we need to once we have created a new state.fimage.
 * this is used when a new file has been opened, or one has been read from
 * the camera.
 */
void
presentNewImage()
{
	FImage *fip = &state.fimage;

	msg("");

	/* the extra "== 0" tests are for the first time we are called and
	 * ResetAOI resource is False. the others are when the image is a
	 * new size.
	 */
	if (state.resetaoi || state.aoi.w == 0 || state.aoi.h == 0
			   || state.aoi.x + state.aoi.w > fip->sw
			   || state.aoi.y + state.aoi.h > fip->sh) {
	    resetAOI();
	    resetCrop();
	} else
	    updateAOI();

	/* inact flipping, if set. no need to flip aoi: if it's reset, it's
	 * symmetric; otherwise it's as desired already.
	 */
	if (state.lrflip)
	    flipImgCols ((CamPixel *)fip->image, fip->sw, fip->sh);
	if (state.tbflip)
	    flipImgRows ((CamPixel *)fip->image, fip->sw, fip->sh);

	applyCorr();

	computeStats();
	updateFITS();
	gscSetDialog();
	setWindow();
	updateWin();
	newXImage();
	showHeader();
	readAbsPhotom();

	/*
	printState();
	*/
}

/* open and display the given file.
 * if name ends with .fth, first apply fdecompress to a copy.
 * return 0 if ok, else -1.
 */
int
openFile (fn)
char *fn;
{
	FImage fimage;		/* temp area in case of failure */
	char errmsg[1024];
	int fd;

	fd = prepOpen (fn, errmsg);
	if (fd < 0) {
	    msg ("%s: %s", basenm(fn), errmsg);
	    return (-1);
	}

	if (readFITS (fd, &fimage, errmsg) < 0) {
	    msg ("%s: %s", basenm(fn), errmsg);
	    (void) close (fd);
	    return (-1);
	}

	(void) close (fd);

	/* commit to new image */

	/* reclaim GSC star memory, if any */
	resetGSC();

	resetFImage (&state.fimage);
	memcpy ((char *)&state.fimage, (char *)&fimage, sizeof(fimage));
	setFName (fn);
	addHistory (fn);
	presentNewImage();

	return (0);
}

/* called when the Ok Apply button on the file open box is pressed.
 */
/* ARGSUSED */
static void
openCB (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
        XmFileSelectionBoxCallbackStruct *s
				= (XmFileSelectionBoxCallbackStruct  *)call;
	char *filename;

	watch_cursor (1);

	XmStringGetLtoR (s->value, XmSTRING_DEFAULT_CHARSET, &filename);

	(void) openFile (filename);

	XtFree (filename);

	watch_cursor (0);
}

/* called when there is input from the CamereFilename fifo.
 */
static void
filefifoCB (client, fdp, idp)
XtPointer client;       /* file name */
int *fdp;               /* pointer to file descriptor */
XtInputId *idp;         /* pointer to input id */
{
	char *fn = (char *)client;
	int fd = *fdp;
	char buf[4097], *new;
	int nr;

	/* read the filename off the fifo */
	nr = read (fd, buf, sizeof(buf)-1); /* leave room for final \0 */
	if (nr <= 0) {
	    if (nr < 0)
		msg ("%s: %s.", basenm(fn), strerror(errno));
	    else
		msg ("Unexpected EOF from %s -- closing connection.",
								basenm(fn));
	    XtRemoveInput (*idp);
	    close (fd);
	    return;
	}

	/* trim any trailing whitespace and be sure name is \0 terminated */
	while (nr > 0 && isspace(buf[nr-1]))
	    nr--;
	buf[nr] = '\0';

	/* use only the most recent entry */
	for (new = buf+nr-1; new > buf && new[-1] && !isspace(new[-1]); new--)
	    continue;

	if (new < buf) {
	    msg ("Spurious CameraFilename");
	    return;
	}

	/* announce arrival of new image */
	msg ("Receiving %s ...", basenm(new));

	/* now just open the file */
	if (openFile (new) == 0)
	    msg (" ");
}

/* open fn for reading and return the fd.
 * if fn ends in .fth we copy to a tmp location and apply fdecompress.
 * if all is well, return fd, else fill errmsg[] and return -1.
 */
static int
prepOpen (fn, errmsg)
char fn[];
char errmsg[];
{
	char cmd[2048];
	char tmp[128];
	int fd;
	int l;

	l = strlen (fn);
	if (l < 4 || strcmp(fn+l-4, ".fth")) {
	    /* just open directly */
	    fd = open (fn, O_RDONLY);
	    if (fd < 0)
		strcpy (errmsg, strerror(errno));
	} else {
	    /* ends with .fth so need to run through fdecompress
	     * TODO: this is a really lazy way to do it --
	     */
	    int s;
	    msg ("Decompressing %s ...", basenm(fn));
	    sprintf (tmp, "/usr/tmp/camXXXXXX");
	    //mktemp (tmp);
	    mkstemp(tmp);
	    strcat (tmp, ".fth");
	    sprintf (cmd, "cp %s %s; fdecompress -r %s", fn, tmp, tmp);
	    s = system (cmd);
	    (void) unlink (tmp);	/* remove the .fth copy */
	    if (s != 0) {
		sprintf (errmsg, "Can not execute `%s' ", cmd);
		if (s < 0)
		    strcat (errmsg, strerror (errno));
		fd = -1;
	    } else {
		tmp[strlen(tmp)-1] = 's';
		fd = open (tmp, O_RDONLY);
		(void) unlink (tmp);	/* remove the .fts copy */
		if (fd < 0)
		    sprintf (errmsg, "Can not decompress %s: %s", tmp,
							    strerror(errno));
	    }
	}

	return (fd);
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: open.c,v $ $Date: 2001/07/23 19:27:48 $ $Revision: 1.3 $ $Name:  $"};
