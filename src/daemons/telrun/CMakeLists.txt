cmake_minimum_required (VERSION 2.6)
project (telrun)

set(TELRUN_SRC fifoio.c fileio.c pr_new_bias.c pr_new_flat.c pr_new_thermal.c pr_regscan.c telrun.c) 

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")

add_executable(telrun ${TELRUN_SRC})

target_link_libraries (telrun astro fits m misc)

install (TARGETS telrun DESTINATION bin)

