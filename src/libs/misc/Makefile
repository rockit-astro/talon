
CFLAGS := $(ICEFLAGS) -O2 -ffast-math -fPIC -Wall -I../libastro -I../libfits -I ../include $(CFLAGS)

LIBPATH = ../build/libs

# For installations requiring FLI camera support
FLI_LIB= -lfli -L../drivers/fli/libfli
FLI_INCLUDE= -I../drivers/fli/libfli

# For installations which don't need FLI support
#FLI_LIB=
#FLI_INCLUDE=

# For installations requiring SBIG camera support
#SBIG_LIB= $(CURDIR)/../build/libs/libsbigudrv.so
SBIG_LIB=-lsbigudrv -L../drivers/libsbig
SBIG_INCLUDE= -I../drivers/libsbig

# For installations which don't need SBIG support
#SBIG_LIB=
#SBIG_INCLUDE=

APOGEE_LIB=-lapogee -L../drivers/alta
APOGEE_INCLUDE=-I../drivers/alta

CAMERA_LIBS=$(APOGEE_LIB) $(SBIG_LIB) $(FLI_LIB)

OBJS =	catalogs.o 	\
	ccdcamera.o 	\
	ccdshared.o 	\
	ccd_apogee.o \
	ccd_auxcam.o \
	ccd_server.o \
	ccd_fli.o \
	ccd_sbig.o \
	ccd_ocaas.o \
	cliserv.o 	\
	configfile.o 	\
	crackini.o	\
	csimc.o 	\
	focustemp.o     \
	funcmax.o	\
	gaussfit.o 	\
	lstsqr.o 	\
	misc.o 		\
	newton.o	\
	photstd.o 	\
	rot.o 		\
	running.o 	\
	scan.o 		\
	strops.o 	\
	telaxes.o	\
	telenv.o	\
	telfits.o	\
	telfifo.o   \
	tts.o

libmisc.so:	$(OBJS)
	$(CC) $(ICEFLAGS) -shared -o $@ $(OBJS) -L$(LIBPATH) $(CAMERA_LIBS)
	cp $@ ../build/libs/

libmisc.a:	$(OBJS)
	ar r $@ $(OBJS)
	cp $@ ../build/libs/

#ccdcamera.o: ccdcamera.c ../build/libs/libfli.a ../build/libs/libsbigudrv.so
#	$(CC) $(CFLAGS) $(FLI_INCLUDE) $(SBIG_INCLUDE) -c $<

ccd_fli.o: ccd_fli.c ../build/libs/libfli.a
	$(CC) $(CFLAGS) $(FLI_INCLUDE) -c ccd_fli.c

ccd_sbig.o: ccd_sbig.c ../build/libs/libsbigudrv.so
	$(CC) $(CFLAGS) $(SBIG_INCLUDE) -c ccd_sbig.c

ccd_apogee.o: ccd_apogee.c ../build/libs/libapogee.so
	$(CC) $(CFLAGS) $(APOGEE_INCLUDE) -c ccd_apogee.c

# Rules to copy updated camera libraries into build area
../build/libs/libfli.a: ../drivers/fli/libfli/libfli.a
	cp $^ $@

../build/libs/libsbigudrv.so: ../drivers/libsbig/libsbigudrv.so
	cp $^ $@

../build/libs/libapogee.so: ../drivers/alta/libapogee.so
	cp $^ $@

# Rules to build camera libraries
../drivers/fli/libfli:
	$(MAKE) -C ../drivers/fli

../drivers/fli/libfli/libfli.a: ../drivers/fli/libfli
	$(MAKE) -C ../drivers/fli/libfli

../drivers/libsbig/libsbigudrv.so:
	$(MAKE) -C ../drivers/libsbig

../drivers/alta/libapogee.so:
	$(MAKE) -C ../drivers/alta

install: libmisc.so
	cp libmisc.so $(TELHOME)/lib/libmisc.so
	cp ../build/libs/libsbigudrv.so $(TELHOME)/lib/
	cp ../build/libs/libapogee.so $(TELHOME)/lib/

clean:
	rm -f *.o libmisc.so
	rm -f ../build/libs/libmisc.so ../build/libs/libmisc.a ../build/libs/libfli.a ../build/libs/libsbigudrv.so

# build test program version of telaxes
# CFLAGS= -O2 -ffast-math -Wall -I../libastro -I../libfits $(MOTIFI) -DTEST_IT
# LDFLAGS = -L. -L../libastro $(MOTIFL)
# LIBS = -lmisc -lastro -lm $(MOTIFLIBS)
# telaxes: telaxes.o
# 	$(CC) $(LDFLAGS) -o $@ telaxes.o $(LIBS)

# test program for catbrowse.c
LDFLAGS = -L. -L../libastro $(MOTIFL)
LIBS = -lmisc -lastro -lm $(MOTIFLIBS)

catbrowsemain: catbrowsemain.o
	$(CC) $(LDFLAGS) -o $@ catbrowsemain.o $(LIBS)
	./catbrowsemain

catbrowsemain.o: catbrowse.c
	$(CC) $(CFLAGS) -DTESTIT -o catbrowsemain.o -c catbrowse.c

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2007/03/11 06:00:04 $ $Revision: 1.11 $ $Name:  $
