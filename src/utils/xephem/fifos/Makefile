# Make the fifos used by xephem, assuming the default configuration.
#
# on non-POSIX systems, you make fifos using mknod as follows:
#    mknod <file> p

all: xephem_loc_fifo xephem_db_fifo xephem_in_fifo xephem_out_fifo

xephem_loc_fifo:
	mkfifo -m 0666 $@

xephem_db_fifo:
	mkfifo -m 0666 $@

xephem_in_fifo:
	mkfifo -m 0666 $@

xephem_out_fifo:
	mkfifo -m 0666 $@

# For RCS Only -- Do Not Edit
# @(#) $RCSfile: Makefile,v $ $Date: 2001/04/19 21:12:05 $ $Revision: 1.1.1.1 $ $Name:  $
