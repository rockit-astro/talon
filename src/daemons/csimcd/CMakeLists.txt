cmake_minimum_required (VERSION 2.6)
project (csimcd)

set(CSIMCD_SRC csimcd.c)
 
include_directories ("${CORE_LIBS_DIR}/misc")


add_executable(csimcd ${CSIMCD_SRC})

target_link_libraries (csimcd astro m misc)

install (TARGETS csimcd DESTINATION bin)
