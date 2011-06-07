cmake_minimum_required (VERSION 2.6)
project (fli)

set(FLI_SRC libfli-camera-parport.c libfli-camera-usb.c libfli-camera.c libfli-filter-focuser.c libfli-mem.c libfli.c)
set(FLI_LINUX_SRC unix/linux/libfli-parport.c unix/linux/libfli-usb-sys.c unix/libfli-debug.c unix/libfli-serial.c unix/libfli-sys.c unix/libfli-usb.c)

include_directories (".")
include_directories ("./unix")
include_directories ("./unix/linux")

add_library(fli SHARED ${FLI_SRC} ${FLI_LINUX_SRC})

install (TARGETS fli DESTINATION lib)
