# Makefile for xobs.

IS =	-I../../libastro \
	-I../../libmisc \
	-I../../libfits \
	-I../../libfs \
	-I../../libXmisc \
	-I../../libwcs

CLDFLAGS = $(ICEFLAGS)
CFLAGS := $(CLDFLAGS) -Wall -ffast-math $(IS) $(MOTIFI) $(CFLAGS)
LDFLAGS = -L../../build/libs $(CLDFLAGS) $(MOTIFL) -Wl,-rpath,$(TELHOME)/lib
LIBS = -lastro -lfits -lmisc -lwcs -lfs -lXmisc -lm $(MOTIFLIBS) -lapogee -lsbigudrv

OBJS =	xobs.o \
	autofocus.o \
	batch.o \
	calaxes.o \
	config.o \
	control.o \
	dome.o \
	cover.o \
	fifo_cb.o \
	gui.o \
	paddle.o \
	query.o \
	scope.o \
	skymap.o \
	sound.o \
	telrun.o \
	tips.o \
	update.o \
	xephem.o

xobs: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

# TODO - passX system no longer used; adapt this to new scheme
pass3:
	rm -f buildcfg.h
	touch buildcfg.h

pass3jsf-halfm:
	echo "#define WINDSCREEN 0" >> buildcfg.h

pass3jsf-onem:
	echo "#define COVER 1" >> buildcfg.h

pass6:	install
# END TODO

install: xobs
	rm -f $(TELHOME)/bin/xobs
	cp xobs $(TELHOME)/bin/xobs

clean:
	touch x.o
	rm -f *.o xobs

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.4 $ $Name:  $
