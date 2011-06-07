cmake_minimum_required (VERSION 2.6)
project (fits)

set(FITS_SRC align2fits.c filters.c fits.c fitscorr.c fitsip.c)
 
include_directories ("${CORE_LIBS_DIR}/astro")
include_directories ("${CORE_LIBS_DIR}/misc")
include_directories ("${CORE_LIBS_DIR}/wcs")


add_library(fits SHARED ${FITS_SRC})

install (TARGETS fits DESTINATION lib)
