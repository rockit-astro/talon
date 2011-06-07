# Makefile for csimcd

CLDFLAGS = -O2 -ffast-math -Wall -g
CFLAGS = $(CLDFLAGS) -I../../../libmisc
LDFLAGS = $(CLDFLAGS) -L$(TELHOME)/lib
LIBS= -lastro -lmisc -lfits -lm

read485: read485.o
	rm -f $@
	$(CC) $(LDFLAGS) -o $@ read485.o $(LIBS)

clobber:
	touch x.o
	rm -f *.o read485

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2001/04/19 21:11:57 $ $Revision: 1.1.1.1 $ $Name:  $
