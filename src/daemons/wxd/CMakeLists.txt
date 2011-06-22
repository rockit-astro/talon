cmake_minimum_required (VERSION 2.6)
project (wxd)

set(WXD_SRC wxd.c davis.c page.c petebros.c rainwise.c digitemp.c url.c digitemp/ds1820.c digitemp/onewire.c)

include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")
include_directories ("${CORE_LIBS_DIR}/fits")
include_directories ("digitemp")

add_executable(wxd ${WXD_SRC})

target_link_libraries (wxd astro fits m misc)

install (TARGETS wxd DESTINATION bin)

