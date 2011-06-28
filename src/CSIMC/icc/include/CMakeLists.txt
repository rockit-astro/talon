
set(ICC_INCLUDE 
912b32.h  avr.h         avrclassic.h  float.h  hc16.h      io2333.h  io4434.h   iom103.h    iotiny15.h  long.h     rtevb16l.h  signal.h  stdio.h   zctype.h
_alloc.h  avr603.h      ctype.h       fperr.h  hc16_icc.h  io2343.h  io8515.h   iom603.h    iotiny22.h  macros.h   rtmitef.h   spi.h     stdlib.h  zstdio.h
_const.h  avr_common.h  eeprom.h      hc11.h   io2313.h    io4414.h  io8535.h   iotiny11.h  limits.h    math.h     rtmitenf.h  stdarg.h  string.h  zstdlib.h
assert.h  avr_old.h     errno.h       hc12.h   io2323.h    io4433.h  ioat94k.h  iotiny12.h  locale.h    rtevb16.h  setjmp.h    stddef.h  time.h
)

install (FILES ${ICC_INCLUDE} DESTINATION ${CORE_CSIMC_ICC_DIR}/include)


