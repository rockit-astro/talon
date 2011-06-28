# Makefile for CSIMC Rev C

# how to run bdm, the pod interface program for debugging
BDM12 = PodexICE -t /dev/podexice

all: it

# default target just lists some useful targets.
help:
	@echo Type 'make it' to load a new board 
	@echo Edit Makefile for bdm12 port and delay setting changes

# build all necessary files but don't touch the board
files:	mainpg00.s19 flex_prog.s19 boot.s19 saloader.s19 main.cmf

# load a brand new board with everything, then start
it: loadee eraseflash loadflex loadmain loadsaloader
	@echo 'q' | $(BDM12) > /dev/null

# load the EEPROM boot code, then start
loadee: 
	@echo 'l init.bdm; b' | $(BDM12) > /dev/null
	@echo ""; echo ""; echo "============= loading boot code into EEPROM ============="
	@echo 'Pv boot.s19' | $(BDM12)
	@echo 'q' | $(BDM12) > /dev/null

# erase **all** of FLASH
eraseflash: 
	@echo 'l init.bdm; b' | $(BDM12) > /dev/null
	@echo ""; echo ""; echo "============= erasing FLASH ============="
	@echo 'w ppage 3; A! 8000' | $(BDM12) > /dev/null

# load the FLEX program into FLASH
# N.B. this does not pre-erase
loadflex: 
	@echo 'l init.bdm; b' | $(BDM12) > /dev/null
	@echo ""; echo ""; echo "============= loading FLEX code into FLASH PAGE 3 ============="
	@echo 'w PPAGE  3; Qv flex_prog.s19' | $(BDM12)

# load the main application program info FLASH
# N.B. this does not pre-erase
loadmain: 
	@echo 'l init.bdm' | $(BDM12) > /dev/null
	@echo ""; echo ""; echo "============= loading main code into FLASH PAGE 0 ============="
	@echo 'w PPAGE  0; Qv mainpg00.s19' | $(BDM12)
	@echo ""; echo ""; echo "============= loading main code into FLASH PAGE 1 ============="
	@echo 'w PPAGE  1; Qv mainpg01.s19' | $(BDM12)
	@echo ""; echo ""; echo "============= loading main code into FLASH PAGE 2 ============="
	@echo 'w PPAGE  2; Qv mainpg02.s19' | $(BDM12)
	@echo ""; echo ""; echo "============= loading main code into FLASH PAGE 1f ============="
	@echo 'w PPAGE 1f; Qv mainpg1f.s19' | $(BDM12)


# load the stand-alone loader program into FLASH PPAGE 4
# N.B. this does not pre-erase
loadsaloader: 
	@echo 'l init.bdm; b' | $(BDM12) > /dev/null
	@echo ""; echo ""; echo "============= loading saloader code into FLASH PAGE 4 ============="
	@echo 'w PPAGE  4; Qv saloader.s19' | $(BDM12)

