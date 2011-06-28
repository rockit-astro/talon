CLDFLAGS =
CFLAGS = $(CLDFLAGS) -Wall -O2
LDFLAGS = $(CLDFLAGS)

all: PodexICE

PodexICE: PodexICE.o
	cc $(LDFLAGS) -o $@ $?

pass5: install

install: PodexICE
	rm -f $(TELHOME)/bin/PodexICE
	mv PodexICE $(TELHOME)/bin/PodexICE

clean: clobber
	
clobber:
	touch x.o
	rm -f *.o PodexICE


# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2003/04/15 20:46:47 $ $Revision: 1.1.1.1 $ $Name:  $
