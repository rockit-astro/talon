cmake_minimum_required (VERSION 2.6)
project (fs)


set(FS_SRC gsc.c sadump.c usno.c)
 
include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")


add_library(fs SHARED ${FS_SRC})

install (TARGETS fs DESTINATION lib)
