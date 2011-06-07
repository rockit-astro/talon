# Makefile for camera.
SUFFIX = .a

IS =	-I../../libastro \
	-I../../libfits \
	-I../../libfs \
	-I../../libXmisc \
	-I../../libwcs \
	-I../../libmisc

CLDFLAGS = $(ICEFLAGS)
CFLAGS := $(CLDFLAGS) -Wall -ffast-math -O2 $(IS) $(MOTIFI) $(CFLAGS)

LDFLAGS = -s $(CLDFLAGS) -L../../build/libs $(MOTIFL)  #-Wl,-rpath,/usr/src/talon/build/lib
LIBS = -lastro -lfits -lmisc -lwcs -lfs -lXmisc $(MOTIFLIBS) -lm -lapogee -lsbigudrv

OBJS =	arith.o basic.o blink.o camera.o corr.o del.o epoch.o fits.o glass.o \
	drift.o gsclim.o hist.o ip.o luts.o mag.o main.o markers.o measure.o \
	open.o patchlevel.o photom.o photomabs.o print.o ps.o query.o save.o \
	stars.o wcs.o winhist.o ximage.o

camera: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

SLIBS = ../../libastro/libastro.a \
	../../libfits/libfits.a \
	../../libfs/libfs.a \
	../../libXmisc/libXmisc.a \
	../../libwcs/libwcs.a \
	../../libmisc/libmisc.a \
	/usr/X11R6/lib/libXm.a

camera_static: $(OBJS) $(SLIBS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(SLIBS) -lXp -lXpm -lXt -lXext -lXmu -lX11 -lm

%.a:
	cd $(@D); make $(@F)

# TODO: Not using the pass system anymore; find another way to do this
pass3:	
	rm -f buildcfg.h
	touch buildcfg.h

pass3jsf-onem:
	rm -f buildcfg.h
	echo "#define JSF_VERSION 1" > buildcfg.h

pass6:	install
# END TODO

install: camera
	rm -f $(TELHOME)/bin/camera
	cp camera $(TELHOME)/bin/camera

clean:
	touch x.o
	rm -f *.o camera

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.5 $ $Name:  $
