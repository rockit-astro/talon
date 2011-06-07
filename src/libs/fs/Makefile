CLDFLAGS= $(ICEFLAGS)
CFLAGS := $(CLDFLAGS) -O2 -ffast-math -fPIC -Wall -I../libmisc -I../libastro $(CFLAGS)
LDFLAGS= $(CLDFLAGS) -L$(TELHOME)/lib
SLIBS = -lfs -lfits -lmisc -lastro -lm

LOBJS = gsc.o \
	usno.o

libfs.so: $(LOBJS)
	gcc $(ICEFLAGS) -shared -o $@ $(LOBJS)
	cp $@ ../build/libs

libfs.a: $(LOBJS)
	ar rv $@ $(LOBJS)
	cp $@ ../build/libs

install: libfs.so
	rm -f $(TELHOME)/lib/libfs.so
	cp libfs.so $(TELHOME)/lib/libfs.so


SOBJS = sadump.o

sadump: $(SOBJS)
	$(CC) $(LDFLAGS) -o $@ $(SOBJS) $(SLIBS)

clean:
	rm -f *.o libfs.a libfs.so sadump
	rm -f ../build/libs/libfs.so ../build/libs/libfs.a

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:19 $ $Revision: 1.2 $ $Name:  $
