cmake_minimum_required (VERSION 2.6)
project (wcs)

set(WCS_SRC align2wcs.c checkwcsfits.c delwcsfits.c findreg.c findregd.c resetwcs.c setwcsfits.c worldpos.c xyradec.c)
 
include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/fits")
include_directories ("${CORE_LIBS_DIR}/misc")
include_directories ("${CORE_LIBS_DIR}/fs")


add_library(wcs SHARED ${WCS_SRC})

install (TARGETS wcs DESTINATION lib)
