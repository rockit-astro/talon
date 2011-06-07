# create prop and prop2 test programs for the satlib

CLDFLAGS = -g
CFLAGS = $(CLDFLAGS) -I.. -O2 -ffast-math -Wall
LDFLAGS = $(CLDFLAGS)
LIB = -lm

OBJ	= \
	tid.o \
	readtle.o \
	../actan.o \
	../deep.o \
	../sdp4.o \
	../sgp4.o \
	../thetag.o

all:	prop prop2

prop:	prop.o $(OBJ)
	$(CC) $(LDFLAGS) -o prop prop.o $(OBJ) $(LIB)

prop2:	prop2.o $(OBJ) $(LIB)
	$(CC) $(LDFLAGS) -o prop2 prop2.o $(OBJ) $(LIB)

clobber:	
	rm -f tid.o readtle.o prop prop2 prop.o prop2.o
