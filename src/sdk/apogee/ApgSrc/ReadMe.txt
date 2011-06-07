Dear Developers;

Version 4.2 of the Apogee Camera Control Library is completely compatible
with version 4.0 and 4.1. It does define two new INI entries to support
the new Parallel Port Interface cameras and defines four new subroutines
for use under 32 Bit Windows run-time environments. The new INI entries
and the new SDK subroutines are fully documented in the SDK manual.

Documentation to the config_camera routine now includes more explicit
information on defining exposure times. All application developers
should reread the documentation and make sure that their applications
convert exposure times to/from internal timer values correctly. All
applications must use the CAMDATA tscale element to correctly define
exposure times for Apogee camera systems.

All Windows DLLs now contain explicit version information that can be
used by developers as well as users to identify which camera interface
and run-time environment is supported by the active DLLs. Right click
on any DLL and look at the Version tab of the Properties dialog box.
The File version number and Private Build Description string should be
included in any bug/problem reports.

New INI entries...

The [system]c0_repeat value defines an internal counter used to control
the width of some parallel port states. The default value is 1. This
value should only be modified under the direction of customer support
personnel.

The [system]reg_offset value defines the camera register offset value.
This entry is only used by the parallel port interface and allows
multiple cameras to be controlled using a single parallel port. This
value should only be modified under the direction of customer support
personnel.

New subroutines...

The following subroutines are only supported for applications running
under 32 bit versions of Windows;

get_acquire_priority_class 
set_acquire_priority_class 
These routines retrieve and set the priority class used during subsequent
camera acquisitions. Read the manual carefully before using these routines
in your application.

get_acquire_thread_priority
set_acquire_thread_priority
These routines retrieve and set the thread priority used during subsequent
camera acquisitions. Read the manual carefully before using these routines
in your application.

Send any questions or comments about the software development kit to
apogee_support@gkrcc.com

Greg Remington
GKR Computer Consulting
