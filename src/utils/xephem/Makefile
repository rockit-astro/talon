# Makefile for OCAAS xephem 3.4.

SUFFIX = .a
SLIBS = XpmRdFToI.o ../../libastro/libastro.a

IS =	-I../../libastro
CC = gcc
CLDFLAGS = -m32
CFLAGS := $(CLDFLAGS) -Wall -ffast-math -O2 $(IS) $(MOTIFI) $(CFLAGS)
LDFLAGS = -s $(CLDFLAGS) -L../../build/libs $(MOTIFL)
LIBS = -lastro $(MOTIFLIBS) -lm

OBJS =			\
	aavso.o		\
	atlas.o		\
	bdl.o		\
	broadcast.o	\
	calmenu.o	\
	closemenu.o	\
	compiler.o	\
	constel.o	\
	datamenu.o	\
	db.o		\
	dbmenu.o	\
	earthmap.o	\
	earthmenu.o	\
	explodegif.o	\
	fallbacks.o	\
	fits.o		\
	formats.o	\
	fsmenu.o	\
	glance.o	\
	gsc.o		\
	gscnet.o	\
	helpmenu.o	\
	homeio.o	\
	hznmenu.o	\
	jupmenu.o	\
	listmenu.o	\
	mainmenu.o	\
	marsmenu.o	\
	marsmmenu.o	\
	moonmenu.o	\
	moonnf.o	\
	msgmenu.o	\
	netmenu.o	\
	objmenu.o	\
	patchlevel.o	\
	plot_aux.o	\
	plotmenu.o	\
	preferences.o	\
	progress.o	\
	ps.o		\
	query.o		\
	rotated.o	\
	satmenu.o	\
	saveres.o	\
	setiathome.o	\
	sites.o		\
	skyeyep.o	\
	skyfiltmenu.o	\
	skyfits.o	\
	skyhist.o	\
	skylist.o	\
	skytoolbar.o	\
	skyviewmenu.o	\
	solsysmenu.o	\
	srchmenu.o	\
	time.o		\
	tips.o		\
	trailmenu.o	\
	uranusmenu.o	\
	usno.o		\
	versionmenu.o	\
	wcs.o		\
	webtlemenu.o	\
	xe2.o		\
	xephem.o	\
	xmisc.o

xephem: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

xephem_static: $(OBJS) $(SLIBS)
	$(CC) -static $(LDFLAGS) -o $@ $(OBJS) $(SLIBS) -lXm -lXpm -lXp -lXt -lXext -lSM -lICE -lX11 -lm

%.a:
	cd $(@D); make $(@F)

# TODO: Pass system isn't used anymore; find something to replace this
pass6:	install
	if [ -f $(HOME)/catalogs.tar.gz ]; then 		\
	    rm -rf $(TELHOME)/archive/catalogs;			\
	    cd $(TELHOME)/archive;				\
	    gunzip < $(HOME)/catalogs.tar.gz | tar xf - ;	\
	fi
	rm -fr $(TELHOME)/xephem
	mkdir -p $(TELHOME)/xephem/auxil
	cp -R auxil/[a-z]* $(TELHOME)/xephem/auxil
	rm -fr $(TELHOME)/xephem/auxil/CVS	
	rm -fr $(TELHOME)/xephem/catalogs
	ln -s ../archive/catalogs $(TELHOME)/xephem/catalogs
	ln -s ../comm $(TELHOME)/xephem/fifos
	rm -f $(TELHOME)/xephem/fifos/xephem_loc_fifo
	mkfifo -m 660 $(TELHOME)/xephem/fifos/xephem_loc_fifo
	rm -f $(TELHOME)/xephem/fifos/xephem_in_fifo
	mkfifo -m 660 $(TELHOME)/xephem/fifos/xephem_in_fifo
# END TODO

install: xephem
	rm -f $(TELHOME)/bin/xephem
	cp xephem $(TELHOME)/bin/xephem

clean:
	touch x.o
	rm -f *.o xephem

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.3 $ $Name:  $
