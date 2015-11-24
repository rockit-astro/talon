cmake_minimum_required (VERSION 2.6)
project (install_scripts)

#install(FILES preinstall postinstall
#DESTINATION install_scripts
#PERMISSIONS 
#OWNER_READ OWNER_WRITE OWNER_EXECUTE
#GROUP_READ GROUP_EXECUTE
#WORLD_READ WORLD_EXECUTE)

#add_custom_target (preinstall
#	COMMAND su -c "${install_scripts_SOURCE_DIR}/preinstall"
#	COMMENT "Preinstall"
#)

#add_custom_target (postinstall
#	COMMAND su -c "${install_scripts_SOURCE_DIR}/postinstall"
#	COMMENT "Postinstall"
#)

#add_custom_target (uninstall
#	COMMAND su -c "${install_scripts_SOURCE_DIR}/uninstall"
#	COMMENT "Uninstall"
#)

set(UDEV_DIR /usr/lib/udev/rules.d)
install(FILES udev/talon.rules DESTINATION ${UDEV_DIR})
set(PROFILE_DIR /etc/profile.d)
install(FILES profile.d/talon.sh DESTINATION ${PROFILE_DIR})