/*

  Copyright (c) 2002 Finger Lakes Instrumentation (FLI), L.L.C.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

        Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

        Redistributions in binary form must reproduce the above
        copyright notice, this list of conditions and the following
        disclaimer in the documentation and/or other materials
        provided with the distribution.

        Neither the name of Finger Lakes Instrumentation (FLI), LLC
        nor the names of its contributors may be used to endorse or
        promote products derived from this software without specific
        prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  ======================================================================

  Finger Lakes Instrumentation, L.L.C. (FLI)
  web: http://www.fli-cam.com
  email: support@fli-cam.com

*/

#include "stdafx.h"
#include "xgetopt.h"

#define USEFITS

#ifdef USEFITS
#include "fitsio.h"
#endif /* USEFITS */

#include "../libfli.h"

#define TRYFUNC(f, a)				\
  do {						\
    if ((r = f a))			\
      printf(#f "() failed\n");		\
  } while (0)

#define LIBVERSIZ 1024

typedef struct _cam_t {
  flidomain_t domain;
  char *dname;
  char *name;
} cam_t;

int numcams = 0;

void findcams(flidomain_t domain, cam_t **cam);
int writeraw(char *filename, int width, int height, void *data);

#ifdef USEPNG
int writepng(char *filename, int width, int height, void *data);
#endif /* USEPNG */

#ifdef USEFITS
int writefits(char *filename, int width, int height, void *data);
#endif /* USEFITS */

void usage(char *fmt, ...)
{
  extern const char *__progname;
  va_list ap;

  va_start(ap, fmt);

  printf("\n");
  if (fmt != NULL)
  {
    vprintf(fmt, ap);
    printf("\n\n");
  }
  printf("  Usage: wintakepic "
	 "[-n <num pics>] [-h <hbin>] [-v <vbin>] [-f <flushes>] \\\n\t"
	 "[-x <exposure time>] [-t <tdi rate>] <outfile basename>\n\n");

  va_end(ap);

  exit(0);
}

int myatoi(int *num, const char *str)
{
  long tmp;
  char *endptr;

  tmp = strtol(str, &endptr, 0);
  if (*str == '\0' || *endptr != '\0' || tmp < INT_MIN || tmp > INT_MAX)
    return -1;
  *num = (int)tmp;
  return 0;
}

int main(int argc, char *argv[])
{
  int i, pics = 1, hbin = 1, vbin = 1, exptime = 500, flushes = 1;
	int tdi_rate = 0;
  long r;
  char *outfile, libver[LIBVERSIZ];
  cam_t *cam = NULL;
	int ch;

  /* Parse command line argument list */
  while (1)
  {
    int opt;

		if ((opt = getopt(argc, argv, "f:h:n:v:x:t:")) == -1)
      break;

    switch (opt)
    {
    case 'f':
      if (myatoi(&flushes, optarg) || flushes < 0)
				usage("Invalid flushes: %s", optarg);
      break;

    case 'h':
      if (myatoi(&hbin, optarg) || hbin <= 0)
				usage("Invalid hbin: %s", optarg);
      break;

    case 'n':
      if (myatoi(&pics, optarg) || pics <= 0)
				usage("Invalid number of pics: %s", optarg);
      break;

    case 'v':
      if (myatoi(&vbin, optarg) || vbin <= 0)
				usage("Invalid vbin: %s", optarg);
      break;

	  case 't':
      if (myatoi(&tdi_rate, optarg) || tdi_rate <= 0)
				usage("Invalid tdi rate: %s", optarg);
      break;

    case 'x':
      if (myatoi(&exptime, optarg) || exptime <= 0)
				usage("Invalid exposure time: %s", optarg);
      break;

    default:
      usage(NULL);
    }
  }

  argc -= optind;
  argv += optind;

  if (argc == 0)
    usage("No output file basename given");

  outfile = argv[0];
  argc--;
  argv++;

  for (i = 0; i < argc; i++)
    printf("Ignoring command line argument '%s'", argv[i]);

  TRYFUNC(FLIGetLibVersion,(libver, LIBVERSIZ));
  printf("Library version '%s'\n", libver);

  /* Parallel port */
  //findcams(FLIDOMAIN_PARALLEL_PORT, &cam);
  /* USB */
  findcams(FLIDOMAIN_USB, &cam);
  /* Serial */
  //findcams(FLIDOMAIN_SERIAL, &cam);
  /* Inet */
  //findcams(FLIDOMAIN_INET, &cam);

  for (i = 0; i < numcams; i++)
  {
    long tmp1, tmp2, tmp3, tmp4, row, img_rows, row_width;
    double d1, d2;
    flidev_t dev;
#define BUFF_SIZ 4096
    char buff[BUFF_SIZ];
    unsigned short *img;
    int img_size, j;

    printf("Trying camera '%s' from %s domain\n", cam[i].name, cam[i].dname);

    TRYFUNC(FLIOpen, (&dev, cam[i].name, FLIDEVICE_CAMERA | cam[i].domain));
    if (r)
      continue;

    TRYFUNC(FLIGetModel, (dev, buff, BUFF_SIZ));
    printf("Model:        %s\n", buff);

    TRYFUNC(FLIGetHWRevision, (dev, &tmp1));
    printf("Hardware Rev: %ld\n", tmp1);

    TRYFUNC(FLIGetFWRevision, (dev, &tmp1));
    printf("Firmware Rev: %ld\n", tmp1);

    TRYFUNC(FLIGetPixelSize, (dev, &d1, &d2));
    printf("Pixel Size:   %f x %f\n", d1, d2);

    TRYFUNC(FLIGetArrayArea, (dev, &tmp1, &tmp2, &tmp3, &tmp4));
    printf("Array area:   (%ld, %ld)(%ld, %ld)\n", tmp1, tmp2, tmp3, tmp4);

    TRYFUNC(FLIGetVisibleArea, (dev, &tmp1, &tmp2, &tmp3, &tmp4));

		tmp3 += 100;
    printf("Visible area: (%ld, %ld)(%ld, %ld)\n", tmp1, tmp2, tmp3, tmp4);
    row_width = (tmp3 - tmp1) / hbin;
    img_rows = (tmp4 - tmp2) / vbin;

    TRYFUNC(FLISetImageArea, (dev, tmp1, tmp2,
	    tmp1 + (tmp3 - tmp1) / hbin, tmp2 + (tmp4 - tmp2) / vbin));

    TRYFUNC(FLISetNFlushes, (dev, flushes));

    d1 = -10.0;
    TRYFUNC(FLISetTemperature, (dev, d1));

    TRYFUNC(FLIGetTemperature, (dev, &d1));
    printf("Temperature:  %f\n", d1);

    TRYFUNC(FLISetExposureTime, (dev, exptime));

    TRYFUNC(FLISetFrameType, (dev, FLI_FRAME_TYPE_NORMAL));

    TRYFUNC(FLISetHBin, (dev, hbin));

    TRYFUNC(FLISetVBin, (dev, vbin));

		if (tdi_rate > 0)
		{
			printf("Setting TDI rate to: %d\n", tdi_rate);
			TRYFUNC(FLISetTDI, (dev, tdi_rate, 0));
		}

		ch = _getch();

		if (ch == 'q') goto End;

    img_size = img_rows * row_width * sizeof(unsigned short);

    if ((img = (unsigned short *) malloc(img_size)) == NULL)
      printf("malloc() failed\n");

    for (j = 0; j < pics; j ++)
    {
      TRYFUNC(FLIExposeFrame, (dev));

      do {
	TRYFUNC(FLIGetExposureStatus, (dev, &tmp1));
	if (r)
	  break;

	Sleep(tmp1);
      } while (tmp1);

      for (row = 0; row < img_rows; row++)
      {
	TRYFUNC(FLIGrabRow, (dev, &img[row * row_width], row_width));
	if (r)
	  break;
      }

#define WRITEIMG(writefn, ext)						\
  do {									\
    if (_snprintf(buff, BUFF_SIZ, "%s.%d.%d." ext, outfile, i, j) == -1)	\
      printf("output file name too long\n");				\
    TRYFUNC(writefn, (buff, row_width, img_rows, img));			\
    if (r == 0)								\
      printf("image written to '%s'\n", buff);				\
  } while (0)

      WRITEIMG(writeraw, "raw");

#ifdef USEFITS

      WRITEIMG(writefits, "fit");

#endif /* USEFITS */

#undef WRITEIMG

    }


    free(img);

		/* This turns off TDI (including TDI flushing) */
		//if (tdi_rate > 0)
		//{
		//	TRYFUNC(FLISetTDI, (dev, 0, 0));
		//}

		/* This turns off the TDI acquisition */
		if (tdi_rate > 0)
		{
			TRYFUNC(FLICancelExposure, (dev));
		}

End:

    TRYFUNC(FLIClose, (dev));
  }

  for (i = 0; i < numcams; i++)
    free(cam[i].name);

  free(cam);

  exit(0);
}

void findcams(flidomain_t domain, cam_t **cam)
{
  long r;
  char **tmplist;

  TRYFUNC(FLIList, (domain | FLIDEVICE_CAMERA, &tmplist));

  if (tmplist != NULL && tmplist[0] != NULL)
  {
    int i, cams = 0;

    for (i = 0; tmplist[i] != NULL; i++)
      cams++;

    if ((*cam = (cam_t *) realloc(*cam, (numcams + cams) * sizeof(cam_t))) == NULL)
      printf("realloc() failed\n");

    for (i = 0; tmplist[i] != NULL; i++)
    {
      int j;
      cam_t *tmpcam = *cam + i;

      for (j = 0; tmplist[i][j] != '\0'; j++)
	if (tmplist[i][j] == ';')
	{
	  tmplist[i][j] = '\0';
	  break;
	}

      tmpcam->domain = domain;
      switch (domain)
      {
      case FLIDOMAIN_PARALLEL_PORT:
	tmpcam->dname = "parallel port";
	break;

      case FLIDOMAIN_USB:
	tmpcam->dname = "USB";
	break;

      case FLIDOMAIN_SERIAL:
	tmpcam->dname = "serial";
	break;

      case FLIDOMAIN_INET:
	tmpcam->dname = "inet";
	break;

      default:
	tmpcam->dname = "Unknown domain";
	break;
      }
      tmpcam->name = _strdup(tmplist[i]);
    }

    numcams += cams;
  }

  TRYFUNC(FLIFreeList, (tmplist));

  return;
}

int writeraw(char *filename, int width, int height, void *data)
{
  int fd, size, err;

  if ((fd = open(filename, O_WRONLY | O_CREAT | /* O_EXCL */ O_TRUNC,
		 _S_IREAD | _S_IWRITE)) == -1)
  {
    printf("open(%s) failed\n", filename);
    return -errno;
  }

  size = width * height * sizeof(unsigned short);
  if ((err = write(fd, data, size)) != size)
  {
    printf("write() failed\n");
    err = -errno;
  }
  else
    err = 0;

  close(fd);

  return err;
}

#ifdef USEFITS

int writefits(char *filename, int width, int height, void *data)
{
  int status = 0;
  long naxes[2] = {width, height};
  fitsfile *fp;

  fits_create_file(&fp, filename, &status);
  if (status)
  {
		printf("fits_create_file: %d\n", status);
		fits_report_error(stderr, status);
    return -1;
  }

  fits_create_img(fp, SHORT_IMG, 2, naxes, &status);
  if (status)
  {
		printf("fits_create_img: %d\n", status);
    fits_report_error(stderr, status);
    return -1;
  }

	fits_write_img(fp, TSHORT, 1, (width * height), data, &status);
  if (status)
  {
		printf("fits_write_img: %d\n", status);
		fits_report_error(stderr, status);
    return -1;
  }

  fits_close_file(fp, &status);
  if (status)
  {
		printf("fits_close_file: %d\n", status);
    fits_report_error(stderr, status);
    return -1;
  }

  return 0;
}

#endif /* USEFITS */
