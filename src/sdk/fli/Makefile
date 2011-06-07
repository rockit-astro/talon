LIBDIST = libfli-RBI-prerelease
#LIBDIST = libfli-release-1.71

libfli:
	ln -s $(LIBDIST) libfli
	
all:	libfli
	$(MAKE) -C libfli
	$(MAKE) -C fliusb

install: all
	cp libfli/libfli.a ../../build/libs
 
clean:	libfli
	$(MAKE) -C libfli clean
	$(MAKE) -C fliusb clean
	rm -f libfli/unix/linux/linux-usb.h
	rm -f fliusb/Module.symvers
	rm -rf libfli

