cmake_minimum_required (VERSION 2.6)
project (telstat)

set(TELSTAT_SRC telstat.c)

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")

add_executable(telstat ${TELSTAT_SRC})

target_link_libraries (telstat astro misc fits m)

install (TARGETS telstat DESTINATION bin)

