cmake_minimum_required (VERSION 2.6)
project (camera)

set(CAMERA_SRC ccd_server.c ccdshared.c ccdcamera.c ccd_template.c)

include_directories ("${CORE_LIBS_DIR}/misc")

add_library(camera SHARED ${CAMERA_SRC})

install (TARGETS camera DESTINATION lib)
