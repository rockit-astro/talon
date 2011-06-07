CFLAGS := -ffast-math -fPIC -Wall -I../libastro -I../libmisc $(MOTIFI) $(CFLAGS)

OBJS =  catbrowse.o 	\
	xtools.o

libXmisc.so:	$(OBJS)
	gcc $(ICEFLAGS) -shared -o $@ $(OBJS)
	cp $@ ../build/libs

libXmisc.a:	$(OBJS)
	ar rv $@ $(OBJS)
	cp $@ ../build/libs

install: libXmisc.so
	rm -f $(TELHOME)/lib/libXmisc.so
	cp libXmisc.so $(TELHOME)/lib/libXmisc.so

clean:
	rm -f *.o libXmisc.a libXmisc.so
	rm -f ../build/libs/libXmisc.so ../build/libs/libXmisc.a

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2006/05/28 01:07:19 $ $Revision: 1.2 $ $Name:  $
