cmake_minimum_required (VERSION 2.6)
project (usb)

set(USB_SRC descriptors.c error.c linux.c usb.c)

add_library(usb SHARED ${USB_SRC})

install (TARGETS usb DESTINATION lib)
