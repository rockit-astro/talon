# Makefile for csimcd

CLDFLAGS = $(ICEFLAGS) -O2 -ffast-math -Wall -g
CFLAGS := $(CLDFLAGS) -I../../libmisc $(CFLAGS)
LDFLAGS = $(CLDFLAGS) -L../../build/libs
LIBS= -lastro -lmisc -lfits -lm -lapogee -lsbigudrv

OBJS =	csimcd.o

csimcd: $(OBJS)
	rm -f $@
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

install: csimcd
	rm -f $(TELHOME)/bin/csimcd
	cp csimcd $(TELHOME)/bin

clean:
	touch x.o
	rm -f *.o csimcd

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $
