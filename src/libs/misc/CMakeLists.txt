cmake_minimum_required (VERSION 2.6)
project (misc)

set(MISC_SRC catalogs.c  crackini.c funcmax.c misc.c rot.c strops.c telfifo.c cliserv.c csimc.c gaussfit.c newton.c running.c telaxes.c telfits.c configfile.c focustemp.c lstsqr.c photstd.c scan.c telenv.c tts.c)
#ccd_server.c ccdshared.c ccdcamera.c ccd_template.c ccd_auxcam.c ccd_ocaas.c ccd_fli.c ccd_apogee.c ccd_sbig.c

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/fits")


add_library(misc SHARED ${MISC_SRC})

install (TARGETS misc DESTINATION lib)
