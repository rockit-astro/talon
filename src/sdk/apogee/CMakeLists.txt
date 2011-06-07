cmake_minimum_required (VERSION 2.6)
project (apogee)

set(APOGEE_SRC main.c apdscan.c aplinux.c apapi.c apdata.c aplow.c)

#include_directories ("${CORE_LIBS_DIR}/astro")
#include_directories ("${CORE_LIBS_DIR}/fits")
#include_directories ("${CORE_LIBS_DIR}/misc")
#Set( KERNEL_DIR "/lib/modules/${CMAKE_SYSTEM_VERSION}/build/include" )
#include_directories ("${KERNEL_DIR}")


add_library(apogee MODULE ${APOGEE_SRC})

install (TARGETS apogee DESTINATION lib)
