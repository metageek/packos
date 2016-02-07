#ifndef _PACKOS_KERNEL_INTERRUPTSP_H_
#define _PACKOS_KERNEL_INTERRUPTSP_H_

#include <packos/interrupts.h>
#include <packos/packet.h>

#define PACKOS_INTERRUPT_KERNEL_ID_CLOCK 1
#define PACKOS_INTERRUPT_NUM_INTERRUPTS 0xff

int PackosKernelInterruptInit(bool requiresUsr1,
			   bool requiresUsr2,
			   PackosError* error);

PackosAddress PackosInterruptFor(PackosInterruptId id,
                                 uint16_t* udpPort,
                                 PackosError* error);

PackosInterruptId PackosKernelInterruptAllocate(int* signum,
                                                PackosError* error);
int PackosKernelInterruptDeallocate(PackosInterruptId id,
                                    PackosError* error);

int PackosKernelInterruptRegisterFor(PackosInterruptId id,
                                     uint16_t udpPort,
                                     PackosError* error);
int PackosKernelInterruptUnregisterFor(PackosInterruptId id,
                                       PackosError* error);

#endif /*_PACKOS_KERNEL_INTERRUPTSP_H_*/
