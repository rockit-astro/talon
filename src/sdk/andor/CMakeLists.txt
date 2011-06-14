cmake_minimum_required (VERSION 2.6)
project (andor)

IF(${CORE_64_BITS} EQUAL 1)
    set(ANDOR_LIBS lib/libandor-stdc++6-x86_64.so.2.88.30003.0)
    set(ATSIFIO_LIBS lib/libatsifio-stdc++6-x86_64.so.2.88.30003.0)
ELSE(${CORE_64_BITS} EQUAL 1)
    set(ANDOR_LIBS lib/libandor-stdc++6-i386.so.2.88.30003.0)
    set(ATSIFIO_LIBS lib/libatsifio-stdc++6-i386.so.2.88.30003.0)
ENDIF(${CORE_64_BITS} EQUAL 1)

install (FILES ${ANDOR_LIBS} DESTINATION lib)
install (FILES ${ANDOR_LIBS} DESTINATION lib RENAME libandor.so.2)

install (FILES ${ATSIFIO_LIBS} DESTINATION lib)
install (FILES ${ANDOR_LIBS} DESTINATION lib RENAME libatsifio.so.2)

install (DIRECTORY andor DESTINATION /usr/local/etc)

