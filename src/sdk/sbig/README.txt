======================================================
Notes:
======================================================
 * The LPT and USB port kernel mode drivers included
   in this archive were compiled for a non-SMP
   version 2.4 kernel.
 * We offer a non-kernel mode driver for USB that
   works with both version 2.4 and 2.6 kernels.
 * If you have problems or comments please contact
   <matto@sbig.com>.

 Version History
 ================
 08/09/05	libsbigudrv.so - Version 4.35 build 5B
 		added support for the non-kernel based
		usb driver libusb.  The libusb driver
		works under both kernels 2.4 and 2.6
 02/22/05	libsbigudrv.so - Version 4.35 build 5
			sbgifcam.hex - Version 1.09
			sbigucam.hex - Version 1.41
			sbiglcam.hex - Version 1.09
======================================================

======================================================
How to unpack SBIG's kernel driver compressed file.
======================================================
tar xzvf LinuxDevKit

The 'sbig' directory is created and contains all files
from the distribution.

======================================================
Low level LPT and USB Drivers
======================================================
Our cameras require low-level drivers to be installed
in order to talk to the cameras over the LPT or
USB ports.  As is often the case with Linux drivers
the issue is slightly complicated.

LPT Drivers
===========
At this time we only offer kernel mode LPT drivers
for kernel version 2.4.  As we haven't made a Parallel
port camera in the last several years we simply don't
have the resources to port this driver to kernel 2.6.
To install the kernel-based LPT drivers see the section
below.

USB Drivers
===========
We offer two drivers for USB based cameras: one
that is kernel based for kernel version 2.4 and one
the is non-kernel based that works under both kernels
2.4 and 2.6.  We recommend you use the non-kernel
based driver due to the increased compatibility.  Our
libsbigudrv shared library will work with either of
these drivers.  To intall either of these USB drivers
follow the instruction in the next two sections.

======================================================
How to install SBIG's Linux kernel-based drivers.
======================================================
su
cd sbig
./load_lpt  for LPT drivers
./load_usb  for USB drivers

You may see some warnings, but that's normal:

Warning: loading ./modsbiglpt.o will taint the kernel: no license
Warning: loading ./modsbiglpt.o will taint the kernel: forced load

You can check installed driver.
less /proc/devices
cd /dev
ls -l sbiglpt*
You should see 3 devices named sbiglpt0-2,
each dedicated to one LPT port [0-2]

To remove modules from the memory, run scripts:
./unload_lpt
./unload_usb

======================================================
How to install the libusb non-kernel based USB driver.
======================================================
1. Download the current stable version from:
   http://libusb.sourceforge.net/
   The current version as of this note is 0.1.10a
2. Unpack the tar file using:
   tar xzf xxx
   where xxx is the name of the tar file, in this case
   libusb-0.1.10a.tar.gz
3. Go to the unpacked directory, in this case
   cd libusb-0.1.10a
4. Run the configure script:
   CFLAGS=-O2 ./configure
5. Run the make command:
   make
6. Switch to superuser and install the library:
   su
   make install
   In this case, the libusb library will be installed in
   the default place /usr/local/lib

======================================================
How to use install the shared libraries.
======================================================
su
cd sbig
cp *.so /usr/local/lib

Set environment LD_LIBRARY_PATH variables:
export LD_LIBRARY_PATH=/usr/local/lib
======================================================
How to activate the hotplug for SBIG USB cameras.
======================================================
Check if your distribution contains fxload application
which uploads the SBIG's firmware into your USB camera.
The file should be located at:

ls /sbin/fxload

If not, visit the following web pages:

http://linux-hotplug.sourceforge.net

select the USB page and download the latest version
of the fxload file into /sbin directory.

Move all SBIG's firmware files *.hex and *.bin to
the /usr/share/usb directory:
cp *.hex /usr/share/usb
cp *.bin /usr/share/usb

Check your /etc/hotplug/usb.usermap file and
add exactly the four lines which are in our distribution
of hotplug/sbig.usermap.

If your usb.usermap is empty, simply rename our
sbig.usermap file to usb.usermap and move it
to the /etc/hotplug/ directory.

Change mode of 'sbig' script and copy to the location:
chmod +x sbig 
cp sbig /etc/hotplug/usb/

Run /sbin/depmod -a
   
If everything is all right, when you turn ON your
USB camera, the firmware should be automatically
uploaded and after 10-15 seconds you should see and 
hear the camera's fan working. If not check the 
usb.usermap file. All the numbers must be in the 
hexadecimal notation, especially for older distributions.
After downloading the firmware, your camera should
be used by your application.
======================================================
Making the test application (optional) 
======================================================
Included are three cpp files (main.cpp, csbigcam.cpp,
and csbigimg.cpp) and a make file that allow you to 
build a simple test application. Run make then run
the test application:

make -f testapp_makefile
./testapp

There are main.cpp and sbigcam.cpp you should use
for your first playing with the driver.
======================================================   
