cmake_minimum_required (VERSION 2.6)
project (icc)

add_subdirectory (bin)
add_subdirectory (include)
add_subdirectory (lib)
add_subdirectory (libc5)

set(ICC_FILES README)
install (FILES ${ICC_FILES} DESTINATION ${CORE_CSIMC_DIR}/icc)
