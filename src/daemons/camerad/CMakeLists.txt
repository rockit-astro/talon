cmake_minimum_required (VERSION 2.6)
project (camerad)

set(CAMERAD_SRC camerad.c) 

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")
include_directories ("${CORE_LIBS_DIR}/fits")


add_executable(camerad ${CAMERAD_SRC})

target_link_libraries (camerad astro camera fits fli m misc)

install (TARGETS camerad DESTINATION bin)
