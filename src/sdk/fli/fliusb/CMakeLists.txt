cmake_minimum_required (VERSION 2.6)
project (fliusb)

Set( MODULE_OUTPUT_FILES    fliusb.ko )

Set( MODULE_SOURCE_FILES    fliusb.c )

Set( DRIVER_FILE        fliusb.ko )
Set( DRIVER_TARGET_NAME fliusb-module )
Set( DRIVER_INSTALL_DIR ${CORE_INSTALL_DIR}/lib/modules)
Set( DRIVER_BIN_FILE    ${DRIVER_INSTALL_DIR}/${DRIVER_FILE} )

Set( MODULE_SOURCE_DIR  ${PROJECT_SOURCE_DIR})
Set( KERNEL_DIR "/lib/modules/${CMAKE_SYSTEM_VERSION}/build" )
Set( KBUILD_CMD ${CMAKE_MAKE_PROGRAM}
                -C ${KERNEL_DIR}
                M=${MODULE_SOURCE_DIR}
                modules )

Add_Custom_Command( OUTPUT  ${DRIVER_BIN_FILE}
                    COMMAND ${KBUILD_CMD}
#                    COMMAND mkdir -p ${DRIVER_INSTALL_DIR}
#                    COMMAND cp -f ${DRIVER_FILE} ${DRIVER_BIN_FILE}
                    WORKING_DIRECTORY ${MODULE_SOURCE_DIR}
                    DEPENDS ${MODULE_SOURCE_FILES} VERBATIM )

#Add_Custom_Target ( ${DRIVER_TARGET_NAME}
#                    ALL
#                    DEPENDS ${DRIVER_BIN_FILE} )

install(FILES ${DRIVER_FILE} DESTINATION ${DRIVER_INSTALL_DIR} PERMISSIONS 
        OWNER_READ OWNER_WRITE
        GROUP_READ
        WORLD_READ)