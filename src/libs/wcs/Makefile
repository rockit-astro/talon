CLDFLAGS= -m32
CFLAGS := $(CLDFLAGS) -O2 -fPIC -Wall -ffast-math -I../libmisc -I../libfits -I../libastro -I../libfs $(CFLAGS)
LDFLAGS= $(CLDFLAGS) 

OBJS =	align2wcs.o \
	delwcsfits.o \
	setwcsfits.o \
	checkwcsfits.o \
	findreg.o \
	findregd.o \
	worldpos.o \
	xyradec.o

libwcs.so: $(OBJS)
	gcc -m32 -shared -o $@ $(OBJS)
	cp $@ ../build/libs

libwcs.a:	$(OBJS)
	ar rv $@ $(OBJS)
	cp $@ ../build/libs

install: libwcs.so
	cp libwcs.so $(TELHOME)/lib/libwcs.so
	cp wcsplot $(TELHOME)/bin/wcsplot

clean:
	rm -f *.o libwcs.a libwcs.so
	rm -f ../build/libs/libwcs.so ../build/libs/libwcs.a

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:19 $ $Revision: 1.5 $ $Name:  $
