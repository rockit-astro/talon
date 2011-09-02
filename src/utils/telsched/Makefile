# Makefile for telsched and chksch

CLDFLAGS = -m32

IS =	-I../../libmisc \
	-I../../libfits \
	-I../../libastro \
	-I../../libwcs	\
	-I../../libXmisc	\
	$(MOTIFI)
	
CFLAGS := $(CLDFLAGS) -Wall -ffast-math $(IS) $(CFLAGS) -g -ggdb #-O2
LDFLAGS = $(CLDFLAGS) -L../../build/libs $(MOTIFL) -Wl,-rpath,$(TELHOME)/lib
LIBS = -lfits -lmisc -lwcs -lastro -lXmisc -lfs $(MOTIFLIBS) -lm -lapogee -lsbigudrv

TOBJS = camcal.o		\
	cat.o			\
	delsched.o		\
	edit.o			\
	obs.o			\
	obsdsp.o		\
	photstd.o		\
	pointing.o		\
	print.o			\
	readsls.o		\
	scangraphic.o		\
	sortscans.o		\
	telsched.o

telsched: $(TOBJS)
	$(CC) $(LDFLAGS) -o $@ $(TOBJS) $(LIBS)


COBJS =				\
	chksch.o		\
	obs.o

chksch: $(COBJS)
	$(CC) $(LDFLAGS) -o $@ $(COBJS) -lastro -lmisc -lfits -lm

all:	telsched chksch

install: chksch telsched
	rm -f $(TELHOME)/bin/chksch
	cp chksch $(TELHOME)/bin
	rm -f $(TELHOME)/bin/telsched
	cp telsched $(TELHOME)/bin

clean:
	touch x.o
	rm -f *.o telsched chksch

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $
