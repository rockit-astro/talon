# Makefile for wxd

CLDFLAGS = $(ICEFLAGS) -g
CFLAGS := $(CLDFLAGS) -Wall -O2 -Idigitemp -I../../libmisc -I../../libastro $(CFLAGS)
LDFLAGS = $(CLDFLAGS) -L../../build/libs
LIBS =  -lmisc -lfits -lastro -lm -lapogee -lsbigudrv

DOBJS = digitemp/ds1820.o digitemp/onewire.o

OBJS = wxd.o davis.o page.o petebros.o rainwise.o digitemp.o url.o $(DOBJS)

wxd: $(OBJS) digitemp
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)


.PHONY: digitemp
digitemp:
	$(MAKE) -C $@

install: wxd
	rm -f $(TELHOME)/bin/wxd
	cp wxd $(TELHOME)/bin
	$(MAKE) -C digitemp install

clean:
	touch x.o
	rm -f *.o wxd
	$(MAKE) -C digitemp clean

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $
