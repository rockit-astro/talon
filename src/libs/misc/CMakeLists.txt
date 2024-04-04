cmake_minimum_required (VERSION 2.6)
project (misc)

set(MISC_SRC misc.c strops.c telfifo.c cliserv.c csimc.c running.c telaxes.c configfile.c telenv.c tts.c)

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/fits")


add_library(misc SHARED ${MISC_SRC})

install (TARGETS misc DESTINATION lib)
