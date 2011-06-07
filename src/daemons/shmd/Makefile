CLDFLAGS= $(ICEFLAGS)
CFLAGS := $(CLDFLAGS) -O2 -ffast-math -Wall -I../../libastro -I../../libfits -I../../libmisc $(CFLAGS)
LDFLAGS= $(CLDFLAGS) -L../../build/libs
LIBS = -lastro -lmisc -lfits -lm -lapogee -lsbigudrv

OBJS= shmd.o

shmd: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

install: shmd
	rm -f $(TELHOME)/bin/shmd
	cp shmd $(TELHOME)/bin/shmd

clean:
	touch x.o
	rm -fr *.o shmd

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $
