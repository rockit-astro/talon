# Makefile for xephemdbd
# Only tested on Linux

CC = gcc

CLDFLAGS =
CFLAGS = $(CLDFLAGS) -Wall -O2 -ffast-math \
	-I../../../../libastro
LDFLAGS = $(CLDFLAGS) \
	-L../../../../libastro
LIBS = -lastro -lm

XED = ../..

OBJS =	xephemdbd.o 		\
	db.o			\
	fsfetch.o		\
	request.o		\
	stubs.o			\
	$(XED)/constel.o	\
	$(XED)/gsc.o		\
	$(XED)/xe2.o

xephemdbd: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

xephemdbd_static: $(OBJS)
	$(CC) -static $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clobber:
	touch x.o
	rm -f *.o xephemdbd


# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2001/04/19 21:12:06 $ $Revision: 1.1.1.1 $ $Name:  $
