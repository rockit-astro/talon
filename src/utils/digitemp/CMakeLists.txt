cmake_minimum_required (VERSION 2.6)
project (digitemp)

set(DIGITEMP_SRC digitemp.c ds1820.c onewire.c)

#include_directories ("${CORE_LIBS_DIR}/astro")
#include_directories ("${CORE_LIBS_DIR}/misc")
#include_directories ("${CORE_LIBS_DIR}/fits")
#include_directories ("digitemp")

add_executable(digitemp ${DIGITEMP_SRC})

target_link_libraries (digitemp m)

install (TARGETS digitemp DESTINATION bin)

