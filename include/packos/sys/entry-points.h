#ifndef _PACKOS_SYS_ENTRY_POINTS_H_
#define _PACKOS_SYS_ENTRY_POINTS_H_

#include <packos/context.h>
#include <packos/errors.h>
#include <packos/interrupts.h>

#define KERNEL_SEES_USER_SPACE

typedef enum {
  packosSysEntryPointIdNull=0,
  packosSysEntryPointIdSetIdleContext,
  packosSysEntryPointIdContextCurrent,
  packosSysEntryPointIdYield,
  packosSysEntryPointIdBlocking,
  packosSysEntryPointIdSetBlocking,
  packosSysEntryPointIdFinished,
  packosSysEntryPointIdExit,
  packosSysEntryPointIdYieldTo,
  packosSysEntryPointIdGetCurrentContext,
  packosSysEntryPointIdContextGetArena,
  packosSysEntryPointIdContextGetIfaceRegistry,
  packosSysEntryPointIdContextSetIfaceRegistry,
  packosSysEntryPointIdInterruptRegisterFor,
  packosSysEntryPointIdInterruptUnregisterFor,
  packosSysEntryPointIdPacketRegisterContext,
  packosSysEntryPointIdPacketUnregisterContext,
  packosSysEntryPointIdPacketSend,
  packosSysEntryPointIdPacketReceive,
  packosSysEntryPointIdPacketReceiveOrYieldTo,
  packosSysEntryPointIdPacketAlloc,
  packosSysEntryPointIdPacketFree
} PackosSysEntryPointId;

typedef union {
  PackosContext context;
#ifdef KERNEL_SEES_USER_SPACE
  SizeType* sizePtr;
  void* voidPtr;
  const char* string;
#endif
  PackosInterruptId interruptId;
  uint16_t uint16;
  int integer;
  bool boolean;
  PackosPacket* packetPtr;
  PackosContextFunc contextFunc;
} PackosSysCallValue;

int PackosSysCall0(PackosSysEntryPointId id,
                   PackosSysCallValue* result,
                   PackosError* error);
int PackosSysCall1(PackosSysEntryPointId id,
                   PackosSysCallValue arg1,
                   PackosSysCallValue* result,
                   PackosError* error);
int PackosSysCall2(PackosSysEntryPointId id,
                   PackosSysCallValue arg1,
                   PackosSysCallValue arg2,
                   PackosSysCallValue* result,
                   PackosError* error);
int PackosSysCall3(PackosSysEntryPointId id,
                   PackosSysCallValue arg1,
                   PackosSysCallValue arg2,
                   PackosSysCallValue arg3,
                   PackosSysCallValue* result,
                   PackosError* error);
int PackosSysCall4(PackosSysEntryPointId id,
                   PackosSysCallValue arg1,
                   PackosSysCallValue arg2,
                   PackosSysCallValue arg3,
                   PackosSysCallValue arg4,
                   PackosSysCallValue* result,
                   PackosError* error);

bool PackosInKernel(void);

#endif /*_PACKOS_SYS_ENTRY_POINTS_H_*/
