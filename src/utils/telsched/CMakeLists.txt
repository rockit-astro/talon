cmake_minimum_required (VERSION 2.6)
project (telsched)

set(TELSCHED_SRC camcal.c cat.c delsched.c edit.c obs.c obsdsp.c photstd.c pointing.c
		print.c readsls.c scangraphic.c sortscans.c telsched.c)
	
include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/fits")
include_directories ("${CORE_LIBS_DIR}/fs")
include_directories ("${CORE_LIBS_DIR}/misc")
include_directories ("${CORE_LIBS_DIR}/xmisc")
include_directories ("${CORE_LIBS_DIR}/wcs")

IF(${CORE_64_BITS} EQUAL 1)
    find_library(ANDOR_LIBS andor64 ${CORE_SDK_DIR}/andor/lib)
ELSE(${CORE_64_BITS} EQUAL 1)
    find_library(ANDOR_LIBS andor32 ${CORE_SDK_DIR}/andor/lib)
ENDIF(${CORE_64_BITS} EQUAL 1)

add_executable(telsched ${TELSCHED_SRC})

target_link_libraries (telsched ${ANDOR_LIBS} astro camera fits fli fs m misc xmisc wcs Xm Xt Xpm X11)

install (TARGETS telsched DESTINATION bin)
