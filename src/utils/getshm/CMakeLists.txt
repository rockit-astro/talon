cmake_minimum_required (VERSION 2.6)
project (getshm)

set(GETSHM_SRC getshm.c)

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")

add_executable(getshm ${GETSHM_SRC})

target_link_libraries (getshm astro misc m)

install (TARGETS getshm DESTINATION bin)

