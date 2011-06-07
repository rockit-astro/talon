cmake_minimum_required (VERSION 2.6)
project (rund)

set(RUND_SRC rund.c) 

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")

add_executable(rund ${RUND_SRC})

target_link_libraries (rund astro fits m misc)

install (TARGETS rund DESTINATION bin)

