# Makefile for DigiTemp v1.1
# Copyright 1996 by Brian C. Lane
#

CC	= gcc
CFLAGS	:= -O2 -Wall $(CFLAGS)
LIBS	= -lm

OBJS	= digitemp.o ds1820.o onewire.o

digitemp: $(OBJS)
	$(CC) $(ICEFLAGS) $(OBJS) -o digitemp $(LIBS)

install: digitemp
	rm -f $(TELHOME)/bin/digitemp
	cp digitemp $(TELHOME)/bin/digitemp

clean:
	touch x.o
	rm -f *.o digitemp
