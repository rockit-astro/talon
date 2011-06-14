cmake_minimum_required (VERSION 2.6)
project (domehubd)

set(DOMEHUBD_SRC domehubd.c)
 
add_executable(domehubd ${DOMEHUBD_SRC})

install (TARGETS domehubd DESTINATION bin)