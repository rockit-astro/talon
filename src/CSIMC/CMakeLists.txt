add_subdirectory (PodexICE)
add_subdirectory (icc)
add_subdirectory (byacc-1.9)

install(DIRECTORY csimc_original csimc_ice csimc_firmware DESTINATION ${CORE_CSIMC_DIR})
install(FILES README DESTINATION ${CORE_CSIMC_DIR})
