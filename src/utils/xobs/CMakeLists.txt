cmake_minimum_required (VERSION 2.6)
project (xobs)

set(XOBS_SRC 
        xobs.c 
        config.c 
        control.c 
        fifo_cb.c 
        gui.c 
        paddle.c 
        query.c 
        scope.c 
        skymap.c 
        update.c 
) 

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")
include_directories ("${CORE_LIBS_DIR}/xmisc")

add_executable(xobs ${XOBS_SRC})

target_link_libraries (xobs astro m misc xmisc Xm Xt X11)

install (TARGETS xobs DESTINATION bin)
