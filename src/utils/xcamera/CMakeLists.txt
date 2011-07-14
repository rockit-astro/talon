cmake_minimum_required (VERSION 2.6)
project (xcamera)

set(XCAMERA_SRC arith.c basic.c blink.c camera.c corr.c del.c epoch.c fits.c glass.c
        drift.c gsclim.c hist.c ip.c luts.c mag.c main.c markers.c measure.c
        open.c patchlevel.c photom.c photomabs.c print.c ps.c query.c save.c
        stars.c wcs.c winhist.c ximage.c) 

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

add_executable(xcamera ${XCAMERA_SRC})

target_link_libraries (xcamera ${ANDOR_LIBS} astro camera fits fli fs m misc xmisc wcs Xm Xt X11)

install (TARGETS xcamera DESTINATION bin)
