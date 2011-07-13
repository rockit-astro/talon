cmake_minimum_required (VERSION 2.6)
project (xobs)

set(XOBS_SRC 
        xobs.c 
        autofocus.c 
        batch.c 
        calaxes.c 
        config.c 
        control.c 
        dome.c 
        cover.c 
        fifo_cb.c 
        gui.c 
        paddle.c 
        query.c 
        scope.c 
        skymap.c 
        sound.c 
        telrun.c 
        tips.c 
        update.c 
        xephem.c
) 

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


add_executable(xobs ${XOBS_SRC})

target_link_libraries (xobs astro camera ${ANDOR_LIBS} fli fits fs m misc xmisc wcs Xm Xt Xpm X11)

install (TARGETS xobs DESTINATION bin)
