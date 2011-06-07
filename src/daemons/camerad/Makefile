# Makefile for camerad

CLDFLAGS = -m32 -O2 -Wall
CFLAGS := $(CLDFLAGS) -I../../libfits -I../../libastro -I../../libmisc $(CFLAGS)
LDFLAGS = $(CLDFLAGS) -L../../build/libs
LIBS= -lastro -lmisc -lfits -lm -lapogee -lsbigudrv

OBJS = camerad.o

camerad: $(OBJS)
	echo "LD PATH:"
	echo $(LD_LIBRARY_PATH)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

install: camerad
	rm -f $(TELHOME)/bin/camerad
	cp camerad $(TELHOME)/bin/camerad

clean:
	touch x.o
	rm -f *.o camerad

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.2 $ $Name:  $
