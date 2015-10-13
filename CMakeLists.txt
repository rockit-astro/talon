cmake_minimum_required (VERSION 2.6)
project (core)

set(CORE_64_BITS 1)

if(PREFIX)
    set(CMAKE_INSTALL_PREFIX ${PREFIX})
else(PREFIX)
    set(CMAKE_INSTALL_PREFIX /usr/local/telescope)
endif(PREFIX)


IF(${CORE_64_BITS} EQUAL 1)
    set(CMAKE_C_FLAGS -m64)
ELSE(${CORE_64_BITS} EQUAL 1)
    set(CMAKE_C_FLAGS -m32)
ENDIF(${CORE_64_BITS} EQUAL 1)





set(CORE_DIR ${CMAKE_SOURCE_DIR})
set(CORE_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
set(CORE_BIN_INSTALL_DIR ${CORE_INSTALL_DIR}/bin)
set(CORE_LIB_INSTALL_DIR ${CORE_INSTALL_DIR}/lib)




set(CORE_SRC_DIR ${CORE_DIR}/src)
set(CORE_LIBS_DIR ${CORE_SRC_DIR}/libs)
set(CORE_DAEMONS_DIR ${CORE_SRC_DIR}/daemons)
set(CORE_SDK_DIR ${CORE_SRC_DIR}/sdk)

set(CORE_LIBS astro fits fs misc wcs)
set(CORE_DAEMONS csimcd telescoped)


set(CORE_CSIMC_DIR ${CORE_INSTALL_DIR}/CSIMC)
set(CORE_CSIMC_ICC_DIR ${CORE_CSIMC_DIR}/icc)

add_subdirectory (src)
add_subdirectory (archive)
add_subdirectory (install_scripts)
