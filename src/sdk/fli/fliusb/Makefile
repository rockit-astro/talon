obj-m		:= fliusb.o

KDIR		?= /lib/modules/$(shell uname -r)/build
PWD		:= $(shell pwd)
MOD_DIR		?= /lib/modules/$(shell uname -r)/kernel/drivers/usb/image
KBUILD_NOPEDANTIC=1

CFLAGS_EXTRA	+= -O2 -Wall

#CFLAGS_EXTRA	+= -DDEBUG	# enable debug messages
#CFLAGS_EXTRA	+= -DASYNCWRITE	# enable asynchronous writes
CFLAGS_EXTRA	+= -DSGREAD	# enable scatter-gather reads

all: module cleanup

module:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

cleanup:
	rm -f *.o .*.cmd *.mod.c; rm -rf .tmp_versions

clean: cleanup
	rm -f *.ko test

install: module cleanup
	cp fliusb.ko $(MOD_DIR)/fliusb.ko
	/sbin/depmod
