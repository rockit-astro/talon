CFLAGS := $(ICEFLAGS) -O2 -ffast-math -fPIC -Wall -I../libmisc -I../libastro -I../libwcs $(CFLAGS)

OBJS =	align2fits.o	\
	fits.o		\
	fitscorr.o	\
	filters.o	\
	fitsip.o

libfits.so:	$(OBJS)
	gcc $(ICEFLAGS) -shared -o $@ $(OBJS)
	cp $@ ../build/libs

libfits.a:	$(OBJS)
	ar rv $@ $(OBJS)

install: libfits.so
	cp libfits.so $(TELHOME)/lib/libfits.so

bzero:
	make -f Makefile.bzero clobber
	make -f Makefile.bzero install	

clean:
	touch x.o
	rm -f *.o libfits.a libfits.so
	rm -f ../build/libs/libfits.so ../build/libs/libfits.a

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:19 $ $Revision: 1.5 $ $Name:  $
