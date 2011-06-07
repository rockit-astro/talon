cmake_minimum_required (VERSION 2.6)
project (joystickd)

set(JOYSTICKD_SRC joystickd.c) 

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")

add_executable(joystickd ${JOYSTICKD_SRC})

target_link_libraries (joystickd astro fits m misc)

install (TARGETS joystickd DESTINATION bin)
