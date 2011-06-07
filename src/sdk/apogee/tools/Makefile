CLDFLAGS=
CFLAGS= $(CLDFLAGS) -O2 -ffast-math -Wall -I../../../libmisc
LDFLAGS= $(CLDFLAGS) -L$(TELHOME)/lib
LIBS = -lmisc -lastro -lfits -lm

OBJS= insapogee.o

insapogee: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

pass5: install

install: insapogee
	rm -f $(TELHOME)/bin/insapogee
	cp insapogee $(TELHOME)/bin/insapogee

clobber:
	touch x.o
	rm -fr *.o insapogee

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $
