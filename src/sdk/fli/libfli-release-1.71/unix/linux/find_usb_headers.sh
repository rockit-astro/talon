#!/bin/sh

if [ -e /usr/include/linux/usb.h ]; then
    echo '#include <linux/usb.h>'
fi

if [ -e /usr/include/linux/usb_ch9.h ]; then
    echo '#include <linux/usb_ch9.h>'
    HAVE_CH9=1
fi

if [ -e /usr/include/linux/usb/ch9.h ]; then
    echo '#include <linux/usb/ch9.h>'
    HAVE_CH9=1
fi

if [ -e /usr/include/linux/usb_chn9.h ]; then
    echo '#include <linux/usb_chn9.h>'
    HAVE_CH9=1
fi

# Sometimes /usr/include/usb.h conflicts with definitions in (e.g.) usb_ch9.h
# Only include it as a last resort
if [ "$HAVE_CH9" != "1" -a -e /usr/include/usb.h ]; then
    echo '#include <usb.h>'
fi

