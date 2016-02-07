#ifndef _PACKOS_SYS_MEMORYP_H_
#define _PACKOS_SYS_MEMORYP_H_

#include <packos/memory.h>

struct PackosMemoryPhysicalRange {
  PackosMemoryPhysicalRange next;
  PackosMemoryPhysicalRange prev;

  PackosMemoryLogicalRange firstUsedBy;
  PackosMemoryLogicalRange lastUsedBy;

  uint32_t len;
  void* physicalAddr;
  PackosMemoryPhysicalType type;
  bool inUse;
};

struct PackosMemoryLogicalRange {
  PackosMemoryPhysicalRange physical;
  struct {
    PackosMemoryLogicalRange next;
    PackosMemoryLogicalRange prev;
  } usingPhysical,inOwner;

  void* logicalAddr;
  uint32_t flags,offset,len;
  uint16_t segmentId;
};

#endif /*_PACKOS_SYS_MEMORYP_H_*/
