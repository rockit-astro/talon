# Makefile for rund

CLDFLAGS = $(ICEFLAGS) -O2 -ffast-math -Wall
CFLAGS := $(CLDFLAGS) -I../../libmisc $(CFLAGS)
LDFLAGS = $(CLDFLAGS) -L../../build/libs
LIBS= -lmisc -lastro -lfits -lm -lapogee -lsbigudrv

OBJS =	rund.o

rund: $(OBJS)
	rm -f $@
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

install: rund
	rm -f $(TELHOME)/bin/rund
	cp rund $(TELHOME)/bin

clean:
	touch x.o
	rm -f *.o rund

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $
