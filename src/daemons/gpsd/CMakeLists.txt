cmake_minimum_required (VERSION 2.6)
project (gpsd)

set(GPSD_SRC gpsd.c) 

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")

add_executable(gpsd ${GPSD_SRC})

target_link_libraries (gpsd astro fits m misc)

install (TARGETS gpsd DESTINATION bin)

