# Makefile for telescoped.csi

# note: the following code support is normally kept in all builds;
# these devices are detected and used if there and configured in filter.cfg
# define these as 0 only if you do not wish to link to external libraries.

#ICE USE_SBIG = 1 # use SBIG filter wheel code (must link to libsbigudrv)
#ICE USE_FLI = 1 # use FLI filter wheel code (must link to libfli)

ifdef USE_SBIG
	DUSESBIG = -DUSE_SBIG  # Enable SBIG filter wheel code
	SBIGLIB = -lsbigudrv   # sbig library
	SBIG_H = -I../../drivers/libsbig
else
	DUSESBIG = 
	SBIGLIB = 
	SBIG_H =
endif

ifdef USE_FLI
	DUSEFLI = -DUSE_FLI # Enable FLI filter wheel code
	FLILIB = -lfli
	FLI_H = -I../../drivers/fli/libfli
else
	DUSEFLI =
	FLILIB =
	FLI_H =
endif

CLDFLAGS = -m32 -ffast-math -Wall # -O2
CFLAGS := $(CLDFLAGS) -I../../libastro -I../../libmisc $(CFLAGS)
LDFLAGS = $(CLDFLAGS) $(SBIGLIB) $(FLILIB) -L../../build/libs  -Wl,-rpath,$(TELHOME)/lib
LIBS= -lastro -lmisc -lfits -lm -lapogee -lsbigudrv

OBJS =	telescoped.o \
	virmc.o \
	axes.o \
	csimc.o \
	dome.o \
	dome_csi.o \
	dome_commhub.o \
	filter.o \
	focus.o \
	lights.o \
	tel.o \
	mountcor.o \
	powerfail.o \
	fifoio.o \
	cover.o

telescoped.csi: $(OBJS)
	rm -f $@
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

filter.o: filter.c fli_filter.c sbig_filter.c
	$(CC) $(CFLAGS) $(DUSESBIG) $(DUSEFLI) $(SBIG_H) $(FLI_H) -c -o filter.o filter.c  

install: telescoped.csi
	rm -f $(TELHOME)/bin/telescoped.csi
	cp telescoped.csi $(TELHOME)/bin/telescoped.csi

# TODO - we're not using the passX system anymore; find a way to adapt this
pass3:
	rm -f buildcfg.h
	touch buildcfg.h

pass3jsf-onem: 
	rm -f buildcfg.h            
	echo "#define TTY_DOME 1" > buildcfg.h

pass3rigel:
	rm -f buildcfg.h
	echo "#define RIGEL_FILTER 1" > buildcfg.h

pass4:	install
# END TODO

clean:
	touch x.o
	rm -f *.o telescoped.csi

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2007/03/03 23:52:31 $ $Revision: 1.7 $ $Name:  $
