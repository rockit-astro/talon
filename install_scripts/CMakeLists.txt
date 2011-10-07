

install(FILES preinstall preinstall_as_root postinstall postinstall_as_root
DESTINATION install_scripts
PERMISSIONS 
OWNER_READ OWNER_WRITE OWNER_EXECUTE
GROUP_READ GROUP_EXECUTE
WORLD_READ WORLD_EXECUTE)


 add_custom_command(TARGET postinstall
	POST_BUILD
    ${CMAKE_INSTALL_PREFIX}/install_scripts/postinstall
)
                     

add_custom_target (postinstall
	COMMAND ${CMAKE_INSTALL_PREFIX}/install_scripts/postinstall
)

#add_dependencies(install postinstall)                   