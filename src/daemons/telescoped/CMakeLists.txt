cmake_minimum_required (VERSION 2.6)
project (telescoped)

set(TELESCOPED_SRC axes.c csimc.c fifoio.c tel.c focus.c mountcor.c telescoped.c)

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")

add_executable(telescoped ${TELESCOPED_SRC})

target_link_libraries (telescoped astro m misc)

install (TARGETS telescoped DESTINATION bin)

