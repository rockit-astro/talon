# Makefile for telrun

IS =	-I../../libfits \
	-I../../libmisc \
	-I../../libastro \
	-I../../include

CLDFLAGS = -m32
CFLAGS := $(CLDFLAGS) -O2 -Wall -ffast-math $(IS) $(CFLAGS)
LDFLAGS = $(CLDFLAGS) -L../../build/libs
LIBS = -lmisc -lastro -lfits -lm -lapogee -lsbigudrv

OBJS =  telrun.o		\
	fifoio.o		\
	fileio.o		\
	pr_regscan.o \
	pr_new_bias.o \
	pr_new_thermal.o \
	pr_new_flat.o \
#	pr_bias.o		\
#	pr_thermal.o		\
#	pr_flat.o		\


telrun: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

# TODO - we're not using the passX system anymore... find a way to adapt this
pass3:
	rm -f buildcfg.h
	touch buildcfg.h

jsf-version:
	rm -f buildcfg.h
	echo "#define JSF_VERSION 1" > buildcfg.h

pass3jsf-onem: jsf-version
pass3jsf-halfm: jsf-version
pass3jsf-vir: jsf-version

pass4jsf-halfm:
	cp pipeline $(TELHOME)/bin
	cp pipeline.cfg $(TELHOME)/archive/config

pass4jsf-vir:
pass4jsf-onem:
	cp calimagePV $(TELHOME)/bin

pass4:	install
# END TODO

install: telrun postprocess
	rm -f $(TELHOME)/bin/telrun
	cp telrun $(TELHOME)/bin/telrun
	rm -f $(TELHOME)/bin/postprocess
	cp postprocess $(TELHOME)/bin/postprocess

postprocess: postprocess.csh
	cp postprocess.csh postprocess

clean:
	touch x.o
	rm -f *.o telrun

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:18 $ $Revision: 1.7 $ $Name:  $
