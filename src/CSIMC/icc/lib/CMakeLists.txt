
set(ICC_LIBS libc12.a libfp12.a liblp12.a)

install (FILES ${ICC_LIBS} DESTINATION ${CORE_CSIMC_ICC_DIR}/lib)
