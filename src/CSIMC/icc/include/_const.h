#ifndef __CONST_H
#define __CONST_H

/* For AVR, const means program memory object. For the Standard C libray,
 * const often simply means that the function will not modify the argument
 * so these two uses are not compatible.
 * Using this macro in the standard header file makes them compatible
 * with all targets
 */
#ifdef _AVR
#define CONST
#else
#define CONST const
#endif

#endif
