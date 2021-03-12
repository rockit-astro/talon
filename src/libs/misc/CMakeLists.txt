cmake_minimum_required (VERSION 2.6)
project (misc)

set(MISC_SRC catalogs.c  crackini.c funcmax.c misc.c rot.c strops.c telfifo.c cliserv.c csimc.c gaussfit.c newton.c running.c telaxes.c configfile.c lstsqr.c photstd.c telenv.c tts.c)

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/fits")


add_library(misc SHARED ${MISC_SRC})

install (TARGETS misc DESTINATION lib)
