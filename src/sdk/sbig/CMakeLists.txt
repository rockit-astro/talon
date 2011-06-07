cmake_minimum_required (VERSION 2.6)
project (sbig)

set(SBIG_SRC csbigcam.cpp csbigimg.cpp testmain.cpp)

#include_directories ("${CORE_LIBS_DIR}/astro")
#include_directories ("${CORE_LIBS_DIR}/fits")
#include_directories ("${CORE_LIBS_DIR}/misc")

add_library(sbig SHARED ${SBIG_SRC})

install (TARGETS sbig DESTINATION lib)
