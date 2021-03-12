project (install_scripts)

set(PROFILE_DIR /etc/profile.d)
install(FILES profile.d/talon.sh DESTINATION ${PROFILE_DIR})

