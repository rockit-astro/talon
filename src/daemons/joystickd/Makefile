LIBS := -lastro -lfits -lmisc -lm -lapogee -lsbigudrv
LDPATH := ../../build/libs
INCPATH := ../../libmisc
CFLAGS := $(CFLAGS)

.c.o:
	gcc -c $^ -I$(INCPATH) $(CFLAGS)

joystickd: joystickd.o
	gcc $(ICEFLAGS) -o $@ $^ -L$(LDPATH) $(LIBS)

fifotalk: fifotalk.o
	gcc $(ICEFLAGS) -o $@ $^ -L$(LDPATH) $(LIBS)


.PHONY: clean
clean:
	touch x.o
	rm -f *.o joystickd

.PHONY: install
install: joystickd
	cp joystickd $(TELHOME)/bin
