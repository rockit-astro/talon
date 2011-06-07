cmake_minimum_required (VERSION 2.6)
project (csimcd)

set(CSIMC_SRC csimcd.c)
 
#include_directories ("${CORE_LIBS_DIR}/astro")
#include_directories ("${CORE_LIBS_DIR}/fits")
include_directories ("${CORE_LIBS_DIR}/misc")


add_executable(csimcd ${CSIMC_SRC})

target_link_libraries (csimcd astro fits m misc)

install (TARGETS csimcd DESTINATION bin)