cmake_minimum_required (VERSION 2.6)
project (PodexICE)

set(PODEXICE_SRC PodexICE.c)


add_executable(PodexICE ${PODEXICE_SRC})

install (TARGETS PodexICE DESTINATION bin)