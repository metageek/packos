#ifndef _PACKOS_INTERRUPTS_H_
#define _PACKOS_INTERRUPTS_H_

#include <packos/types.h>
#include <packos/errors.h>

typedef int16_t PackosInterruptId;

#define PACKOS_INTERRUPT_ALIAS_CLOCK 1024

typedef struct {
  PackosInterruptId id;
} PackosInterruptMsg;

PackosInterruptId PackosInterruptAliasLookup(PackosInterruptId alias,
                                             PackosError* error);

int PackosInterruptRegisterFor(PackosInterruptId id,
                               uint16_t udpPort,
                               PackosError* error);
int PackosInterruptUnregisterFor(PackosInterruptId id,
                                 PackosError* error);

#endif /*_PACKOS_INTERRUPTS_H_*/
