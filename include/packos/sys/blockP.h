#ifndef _PACKOS_KERNEL_BLOCKP_H_
#define _PACKOS_KERNEL_BLOCKP_H_

#include <packos/errors.h>
#include <packos/types.h>

typedef uint32_t PackosInterruptState;

PackosInterruptState PackosKernelContextBlock(PackosError* error);
int PackosKernelContextRestore(PackosInterruptState state,
                               PackosError* error);

#endif /*_PACKOS_KERNEL_BLOCKP_H_*/
