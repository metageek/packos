#ifndef _PACKOS_CONFIG_H_
#define _PACKOS_CONFIG_H_

#include <packos/types.h>

typedef struct {
  void* ifaceRegistry;
} PackosContextState;

typedef struct {
  struct {
    PackosAddress self,scheduler;
  } addresses;
  char name[PACKOS_CONTEXT_MAX_NAMELEN+1];

  struct {
    void* data;
    SizeType size;
    PackosMemoryLogicalRange logical;
  } arena;

  PackosContextState stateConst;
  PackosContextState* state;
} PackosContextConfig;

const PackosContextConfig* PackosContextConfigGet(PackosError* error);

#endif /*_PACKOS_CONFIG_H_*/
