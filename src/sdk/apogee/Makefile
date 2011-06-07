# Major device number
MJR = 120

# where to place the module and its special file
DIR = $(TELHOME)/dev

MDIR = ../../libmisc
CFLAGS = -O -DAP_MJR=$(MJR) -DMODULE -D__KERNEL__ -Wall -I$(MDIR)
NAME = apogee

OBJS = main.o apdscan.o aplinux.o apapi.o apdata.o aplow.o

pass8:	$(NAME).o

pass8a: install

$(NAME).o: apccd.h apglob.h apdata.h aplinux.h aplow.h $(OBJS)
	ld -r -o $(NAME).o $(OBJS)
	@echo 'Now run "make install" as root'

install: $(NAME).o
	mkdir -p $(DIR)
	rm -f $(DIR)/$(NAME)*
	cp $(NAME).o $(DIR)/$(NAME).o
	chmod 666 $(DIR)/$(NAME).o
	rm -f $(DIR)/ccdcamera
	ln -s $(NAME)0 $(DIR)/ccdcamera
	mknod $(DIR)/$(NAME)0 c $(MJR) 0
	chmod 666 $(DIR)/$(NAME)0
	mknod $(DIR)/$(NAME)1 c $(MJR) 1
	chmod 666 $(DIR)/$(NAME)1

clobber:
	touch x.o
	rm -f *.o

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2001/04/19 21:12:12 $ $Revision: 1.1.1.1 $ $Name:  $
