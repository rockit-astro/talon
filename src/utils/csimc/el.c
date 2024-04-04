/* line editing function */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "el.h"

#define	NHB		41		/* number of history buffers */
#define	MLL		75		/* max line length -- no wrap */

static int cc;				/* cursor column -- 0 based */
static int ll;				/* line length */
static char hb[NHB][MLL];		/* history buffer */
static char cl[MLL];			/* current temp line */
static int nh = 1;			/* 1..nh-1 in use, 0 is current */
static int hbase;			/* overflow count */
static int xh;				/* hb[] in use */
static struct termios tio_save;		/* original tty settings */
static int tio_save_set;		/* only save once */

static void nBlanks (int n);
static void nBackspaces (int n);
static void bell(void);
static void showCL (void);

/* call once before using elNext, or after SIGCONT to
 * set up fd 0 for cbreak mode.
 * return 0 if ok else -1.
 */
int
elSetup (void)
{
	struct termios tio;

	if (tcgetattr (0, &tio) < 0)
	    return (-1);
	if (!tio_save_set) {
	    tio_save = tio;
	    tio_save_set = 1;
	}

	tio.c_lflag &= ~(ECHO|ICANON);
	tio.c_oflag = OPOST|ONLCR;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;

	if (tcsetattr (0, TCSAFLUSH, &tio) < 0)
	    return (-1);

	return (0);
}

/* call to turn fd 0 to its state before calling elSetup */
void
elReset (void)
{
	(void) tcsetattr (0, TCSAFLUSH, &tio_save);
}

/* call with new stuff to digest on fd 0. if a whole line is ready fill in
 * buf[] (add \n\0) and return length (sans \0); return 0 if not yet
 * whole; return -1 if error; return -2 if EOF. user output is sent to fd 1.
 * N.B. we assume elSetup has already been called once.
 */
int
elNext (char buf[])
{
	char esccode;
	char c;
	int n;

	n = read (0, &c, 1);
	if (n < 0)
	    return (-1);
	if (n == 0)
	    return (-2);
	
	switch (c) {
	case 033:	/* ESC */
	    /* 2 reads of 1 each since c_cc[VMIN] is 1 */
	    if (read (0, &esccode, 1) != 1 || read (0, &esccode, 1) != 1)
		return (-1);
	    switch (esccode) {
	    case 'D': /* left */
		if (cc > 0) {
		    write (1, "\b", 1);
		    cc--;
		} else
		    bell();
		break;
	    case 'C': /* right */
		if (cc < ll)
		    write (1, &cl[cc++], 1);
		else
		    bell();
		break;
	    case 'A': /* up */
		if (xh < nh-1) {
		    if (xh == 0)
			memcpy (hb[0], cl, MLL);
		    memcpy (cl, hb[++xh], MLL);
		    showCL();
		} else
		    bell();
		break;
	    case 'B': /* down */
		if (xh > 0) {
		    memcpy (cl, hb[--xh], MLL);
		    showCL();
		} else
		    bell();
		break;
	    }
	    break;

	case 1:		/* ^A -- move to beginning of line */
	    nBackspaces (cc);
	    cc = 0;
	    break;

	case 5:		/* ^E -- move to end of line */
	    while (cc < ll)
		write (1, &cl[cc++], 1);
	    break;

	case 4:		/* ^D -- EOF */
	    return (-2);
	    break;

	case 11:	/* ^K -- delete to end of line */
	    n = ll - cc;
	    nBlanks (n);
	    nBackspaces (n);
	    ll -= n;
	    cl[ll] = '\0';
	    break;

	case 21:	/* ^U -- delete entire line */
	    elClear();
	    break;
	    
	case '\b':	/* backspace or delete -- delete char to left */
	case 127:
	    if (cc > 0) {
		nBackspaces (1);
		cc--;
		ll--;
		memmove (&cl[cc], &cl[cc+1], ll-cc);
		write (1, &cl[cc], ll-cc);
		cl[ll] = '\0';
		nBlanks (1);
		nBackspaces (ll-cc+1);
	    } else
		bell();
	    break;

	case '\n':
	    write (1, "\n", 1);
	    cl[ll++] = '\0';
	    sprintf (buf, "%s\n", cl);
	    if (ll > 1 && memcmp (hb[1], cl, MLL)) {
		/* don't put blank or immediate-dup lines into history */
		memcpy (hb[0], cl, MLL);
		for (n = nh; n > 0; --n)
		    if (n < NHB)
			memcpy (hb[n], hb[n-1], MLL);
		if (nh < NHB)
		    nh++;
		else
		    hbase++;
	    }
	    memset (hb[0], 0, MLL);
	    memset (cl, 0, MLL);
	    xh = 0;
	    n = ll;
	    ll = cc = 0;
	    return (n);
	    break;	/* :-| */

	default:
	    if (ll < MLL-1) {
		memmove (&cl[cc+1], &cl[cc], ll-cc);
		cl[cc] = c;
		write (1, &cl[cc], ll-cc+1);
		nBackspaces (ll-cc);
		cc++;
		ll++;
		cl[ll] = '\0';
	    } else
		bell();
	    break;
	}

	return (0);
}

/* return history entry n in buf, where 1 is most recent up through nh.
 * if n <= 0 then just show current history and return 0.
 * return -1 if n is out of range else line-length (just like elNext()).
 */
int
elHistory(int n, char buf[])
{
	int i;

	if (n <= 0) {
	    for (i = 1; i < nh; i++)
		printf ("%2d: %s\n", hbase+i, hb[nh-i]);
	    return (0);
	} else if (n >= hbase+1 && n < hbase+nh) {
	    i = sprintf (buf, "%s\n", hb[nh-(n-hbase)]);
	    memmove (hb[1], hb[nh-n], MLL); /* real command, not hist invoke */
	    return (i);
	} else {
	    printf ("History number must be in range %d .. %d\n", hbase+1,
								    hbase+nh-1);
	    return (-1);
	}
}

/* call to clear any partial input */
void
elClear (void)
{
	nBackspaces (cc);
	nBlanks (ll);
	nBackspaces (ll);
	cc = 0;
	ll = 0;
	cl[ll] = '\0';
}

/* write n blanks */
static void
nBlanks (int n)
{
	while (n-- > 0)
	    write (1, " ", 1);
}

/* write n backspaces */
static void
nBackspaces (int n)
{
	while (n-- > 0)
	    write (1, "\b", 1);
}

static void
bell (void)
{
	write (1, "\a", 1);
}

static void
showCL (void)
{
	int n;

	nBackspaces (cc);
	n = ll;
	cc = ll = strlen(cl);
	write (1, cl, ll);
	n = n-ll;
	if (n > 0) {
	    nBlanks (n);
	    nBackspaces (n);
	}
}
