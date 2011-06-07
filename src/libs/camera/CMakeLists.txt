cmake_minimum_required (VERSION 2.6)
project (camera)

set(CAMERA_SRC ccd_server.c ccdshared.c ccdcamera.c ccd_template.c ccd_auxcam.c ccd_ocaas.c ccd_andor.c ccd_fli.c)
#ccd_apogee.c ccd_sbig.c

#include_directories ("${CORE_LIBS_DIR}/astro")
#include_directories ("${CORE_LIBS_DIR}/fits")
include_directories ("${CORE_LIBS_DIR}/misc")
include_directories ("${CORE_SDK_DIR}/fli/libfli-RBI-prerelease")

add_library(camera SHARED ${CAMERA_SRC})

install (TARGETS camera DESTINATION lib)
