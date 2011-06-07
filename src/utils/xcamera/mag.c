/* code to handle the magnification algorithm */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <Xm/Xm.h>

#include "P_.h"
#include "xtools.h"
#include "fits.h"
#include "fieldstar.h"
#include "ps.h"
#include "camera.h"

static void FtoX1(void);
static void FtoX2(void);
static void FtoX4(void);
static void FtoX1_2(void);
static void FtoX1_4(void);
static void FtoX1_8(void);
static void FtoXMagN(int n);
static void FtoXShrinkN(int n);

/* given state.fimage and the aoi, crop, mag and lut info,
 *    fill in state.ximagep with the resulting scene.
 */
void
FtoXImage()
{
	switch (state.mag) {
	case MAGDENOM:		FtoX1();   break;
	case 2*MAGDENOM:	FtoX2();   break;
	case 4*MAGDENOM:	FtoX4();   break;
	case MAGDENOM/2:	FtoX1_2(); break;
	case MAGDENOM/4:	FtoX1_4(); break;
	case MAGDENOM/8:	FtoX1_8(); break;
	default: msg ("Bad mag: %d", state.mag); break;
	}
}

static void FtoX1()
{
	FtoXMagN(1);
}

static void FtoX2()
{
	FtoXMagN(2);
}

static void FtoX4()
{
	FtoXMagN(4);
}

static void FtoX1_2()
{
	FtoXShrinkN(2);
}

static void FtoX1_4()
{
	FtoXShrinkN(4);
}

static void FtoX1_8()
{
	FtoXShrinkN(8);
}

/* magnify by simple pixel replication */
static void FtoXMagN (m)
int m;	/* mag factor */
{
	CamPixel *ip;	/* input */
	Pixel *lut = state.lut;
	int iwrap;	/* pixels from end of one input line to start of next */
	int nx, ny;	/* region pixels wide and high */
	int x, y;	/* input pixel loc */
	int ox, oy;	/* output pixel loc */

	if (state.crop) {
	    ip = (CamPixel *) state.fimage.image;
	    ip = &ip[state.fimage.sw*state.aoi.y + state.aoi.x];
	    nx = state.aoi.w;
	    ny = state.aoi.h;
	    iwrap = state.fimage.sw - state.aoi.w;
	} else {
	    ip = (CamPixel *) state.fimage.image;
	    nx = state.fimage.sw;
	    ny = state.fimage.sh;
	    iwrap = 0;
	}

	ox = oy = 0;
	for (y = 0; y < ny; y++) {
	    for (x = 0; x < nx; x++) {
		Pixel p = lut[*ip++];
		int i, j;

		for (i = 0; i < m; i++)
		    for (j = 0; j < m; j++)
			XPutPixel (state.ximagep, ox+i, oy+j, p);

		ox += m;
	    }

	    ox = 0;
	    oy += m;
	    ip += iwrap;
	}
}

/* simple pixel averaging */
static void FtoXShrinkN (s)
int s;	/* shrink factor */
{
	CamPixel *ip;	/* input */
	Pixel *lut = state.lut;
	int iwrap;	/* pixels from one row to next, allowing for shrink */
	int nx, ny;	/* output pixels wide and high */
	int area;	/* s * s */
	int x, y;	/* output pixel loc */

	if (state.crop) {
	    ip = (CamPixel *) state.fimage.image;
	    ip = &ip[state.fimage.sw*state.aoi.y + state.aoi.x];
	    iwrap = state.fimage.sw * s;
	    nx = state.aoi.w*state.mag/MAGDENOM;
	    ny = state.aoi.h*state.mag/MAGDENOM;
	} else {
	    ip = (CamPixel *) state.fimage.image;
	    iwrap = state.fimage.sw * s;
	    nx = state.fimage.sw*state.mag/MAGDENOM;
	    ny = state.fimage.sh*state.mag/MAGDENOM;
	}

	area = s * s;

	for (y = 0; y < ny; y++) {
	    CamPixel *row = ip;
	    for (x = 0; x < nx; x++) {
		unsigned ix, iy, sum;
		CamPixel *r = row;
		Pixel p;

		/* compute sum of sxs block of input pixels */
		for (sum = iy = 0; iy < s; iy++) {
		    for (ix = 0; ix < s; ix++)
			sum += *r++;
		    r += state.fimage.sw - s;
		}

		p = lut[sum/area];
		XPutPixel (state.ximagep, x, y, p);

		row += s;
	    }
	    ip += iwrap;
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: mag.c,v $ $Date: 2001/04/19 21:11:59 $ $Revision: 1.1.1.1 $ $Name:  $"};
