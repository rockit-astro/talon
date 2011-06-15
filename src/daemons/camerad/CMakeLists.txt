cmake_minimum_required (VERSION 2.6)
project (camerad)

set(CAMERAD_SRC camerad.c) 

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")
include_directories ("${CORE_LIBS_DIR}/fits")

IF(${CORE_64_BITS} EQUAL 1)
    find_library(ANDOR_LIBS andor64 ${CORE_SDK_DIR}/andor/lib)
ELSE(${CORE_64_BITS} EQUAL 1)
    find_library(ANDOR_LIBS andor32 ${CORE_SDK_DIR}/andor/lib)
ENDIF(${CORE_64_BITS} EQUAL 1)

add_executable(camerad ${CAMERAD_SRC})

target_link_libraries (camerad ${ANDOR_LIBS} astro camera fits fli m misc)

install (TARGETS camerad DESTINATION bin)
