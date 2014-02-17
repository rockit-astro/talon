cmake_minimum_required (VERSION 2.6)
project (getshm)

set(GETSHM_SRC getshm.c)

set(CORE_LIBS_DIR ../../libs)
include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")
#include_directories ("${CORE_LIBS_DIR}/fits")
#include_directories ("digitemp")

add_executable(getshm ${GETSHM_SRC})

target_link_libraries (getshm)

install (TARGETS getshm DESTINATION bin)

