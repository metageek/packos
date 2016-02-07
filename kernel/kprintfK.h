#ifndef _PACKOS_KERNEL_KPRINTF_H_
#define _PACKOS_KERNEL_KPRINTF_H_

#include <packos/types.h>

/* Limited functionality, with only a few formats:
 *  %d, %s, %u, %x, and %p.  It does not handle
 *  length modifiers, field width, or precision.
 */
int kprintf(const char* format, ...);

#endif /*_PACKOS_KERNEL_KPRINTF_H_*/
