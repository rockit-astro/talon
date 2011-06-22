cmake_minimum_required (VERSION 2.6)
project (xtrackd)

set(XTRACKD_SRC xtrackd.c file_notify.c)

#include_directories ("${CORE_LIBS_DIR}/astro")
#include_directories ("${CORE_LIBS_DIR}/misc")

add_executable(xtrackd ${XTRACKD_SRC})

#target_link_libraries (xtrackd astro m)

install (TARGETS xtrackd DESTINATION bin)

