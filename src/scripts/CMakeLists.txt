cmake_minimum_required (VERSION 2.6)
project (scripts)

set(SCRIPTS get-catalogs startTel killTel killp)
set(SCRIPTS_PDU
pdu_cover_motors_off    pdu_dome_power_off        pdu_others_off              pdu_telescope_12v_power_off  pdu_telescope_mixed_power_off
pdu_cover_motors_on     pdu_dome_power_on         pdu_others_on               pdu_telescope_12v_power_on   pdu_telescope_mixed_power_on
pdu_dome_controler_off  pdu_nominal_computer_off  pdu_redundant_computer_off  pdu_telescope_80v_power_off  pdu_webcam_power_off
pdu_dome_controler_on   pdu_nominal_computer_on   pdu_redundant_computer_on   pdu_telescope_80v_power_on   pdu_webcam_power_on
pdu_status)


set(SCRIPTS_DIR ${CORE_INSTALL_DIR}/bin)

install(FILES ${SCRIPTS} ${SCRIPTS_PDU} DESTINATION ${SCRIPTS_DIR} PERMISSIONS 
OWNER_READ OWNER_WRITE OWNER_EXECUTE
GROUP_READ GROUP_EXECUTE
WORLD_READ WORLD_EXECUTE)
