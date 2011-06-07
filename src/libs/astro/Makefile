# Makefile for the basic astronomy routines.

CFLAGS := $(ICEFLAGS) -g -ggdb -ffast-math -Wall -fPIC $(CFLAGS)

OBJS =	aa_hadec.o \
	aberration.o \
	actan.o \
	airmass.o \
	anomaly.o \
	ap_as.o \
	auxil.o \
	chap95.o \
	chap95_data.o \
	circum.o \
	comet.o \
	dbfmt.o \
	deep.o \
	deltat.o \
	earthsat.o \
	eq_ecl.o \
	eq_gal.o \
	formats.o \
	helio.o \
	libration.o \
	misc.o \
	mjd.o \
	moon.o \
	mooncolong.o \
	nutation.o \
	obliq.o \
	parallax.o \
	plans.o \
	precess.o \
	reduce.o \
	refract.o \
	riset.o \
	riset_cir.o \
	sdp4.o \
	sgp4.o \
	sphcart.o \
	sun.o \
	thetag.o \
	utc_gst.o \
	vsop87.o \
	vsop87_data.o

libastro.so:	$(OBJS)
	gcc $(ICEFLAGS) -shared -o $@ $(OBJS) -g -ggdb
	cp $@ ../build/libs

libastro.a:	$(OBJS)
	ar rv $@ $(OBJS)
	cp $@ ../build/libs

install: libastro.so
	rm -f $(TELHOME)/lib/libastro.so
	cp libastro.so $(TELHOME)/lib/libastro.so

clean:
	touch x.o
	rm -f *.o libastro.a libastro.so
	rm -f ../build/libs/libastro.so ../build/libs/libastro.a

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:19 $ $Revision: 1.2 $ $Name:  $
