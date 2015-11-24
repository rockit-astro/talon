cmake_minimum_required (VERSION 2.6)
project (install_scripts)

set(UDEV_DIR /usr/lib/udev/rules.d)
install(FILES udev/talon.rules DESTINATION ${UDEV_DIR})
set(PROFILE_DIR /etc/profile.d)
install(FILES profile.d/talon.sh DESTINATION ${PROFILE_DIR})