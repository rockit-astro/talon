CLDFLAGS= $(ICEFLAGS)
CFLAGS := $(CLDFLAGS) -O2 -ffast-math -Wall -I../../libwcs -I../../libfs -I../../libastro -I../../libfits -I../../libmisc $(CFLAGS)
LDFLAGS= $(CLDFLAGS) -L../../build/libs
LIBS = -lastro -lmisc -lfits -lwcs -lfs -lm -lapogee -lsbigudrv

COBJS= csimc.o boot.o eintrio.o el.o

csimc: $(COBJS)
	$(CC) $(LDFLAGS) -o $@ $(COBJS) $(LIBS)

LOBJS= csimcio.o

csimcio: $(LOBJS)
	$(CC) $(LDFLAGS) -o $@ $(LOBJS) $(LIBS)

install: csimc csimcio
	rm -f $(TELHOME)/bin/csimc
	cp csimc $(TELHOME)/bin/csimc
	rm -f $(TELHOME)/bin/csimcio
	cp csimcio $(TELHOME)/bin/csimcio

eltest: el.c
	$(CC) $(CFLAGS) -DTESTEL -o eltest el.c
	rm -f el.o

clean:
	touch x.o
	rm -fr *.o csimc csimcio eltest

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $
