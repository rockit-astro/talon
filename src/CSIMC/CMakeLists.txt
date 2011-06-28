add_subdirectory (PodexICE)
add_subdirectory (icc)

install(DIRECTORY csimc_original csimc_ice csimc_firmware DESTINATION ${CORE_CSIMC_DIR})
install(FILES README DESTINATION ${CORE_CSIMC_DIR})
