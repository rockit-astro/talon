# Makefile for gpsd

CLDFLAGS = -m32
CFLAGS := $(CLDFLAGS) -Wall -O2 -I../../libmisc -I../../libastro $(CFLAGS)
LDFLAGS = $(CLDFLAGS) -L../../build/libs
LIBS =  -lastro -lmisc -lfits -lm -lapogee -lsbigudrv

gpsd: gpsd.o
	$(CC) $(LDFLAGS) -o $@ $? $(LIBS)

install: gpsd
	cp gpsd $(TELHOME)/bin/gpsd
	cp fakegps $(TELHOME)/bin/fakegps
	chown root:root $(TELHOME)/bin/gpsd
	chmod a+s $(TELHOME)/bin/gpsd

clean:
	touch x.o
	rm -f *.o gpsd

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.3 $ $Name:  $
