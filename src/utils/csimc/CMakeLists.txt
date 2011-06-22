cmake_minimum_required (VERSION 2.6)
project (csimc)

set(CSIMC_SRC csimc.c boot.c eintrio.c el.c)
 
#include_directories ("${CORE_LIBS_DIR}/astro")
#include_directories ("${CORE_LIBS_DIR}/fits")
include_directories ("${CORE_LIBS_DIR}/misc")


add_executable(csimc ${CSIMC_SRC})

target_link_libraries (csimc astro fits m misc)

install (TARGETS csimc DESTINATION bin)