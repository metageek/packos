#ifndef _PACKOS_MEMORY_H_
#define _PACKOS_MEMORY_H_

#include <packos/types.h>
#include <packos/context.h>
#include <packos/errors.h>

#define PACKOS_PAGE_SIZE 4096

/** Note that (f & CopyOnWrite) && !(f & Writable) is nonsensical.
 */
typedef enum {
  packosMemoryFlagWritable=1,
  packosMemoryFlagGrowableUp=2,
  packosMemoryFlagGrowableDown=4,
  packosMemoryFlagCopyOnWrite=8,
  packosMemoryFlagGlobal=16
} PackosMemoryFlag;

typedef enum {
  packosMemoryPhysicalTypeInvalid=0,
  packosMemoryPhysicalTypeRAM,
  packosMemoryPhysicalTypeROM,
  packosMemoryPhysicalTypeIO
} PackosMemoryPhysicalType;

PackosMemoryPhysicalRange
PackosMemoryPhysicalRangeGetFirstInUse(PackosError* error);

PackosMemoryPhysicalRange
PackosMemoryPhysicalRangeGetFirstAvail(PackosError* error);

PackosMemoryPhysicalRange
PackosMemoryPhysicalRangeGetNext(PackosMemoryPhysicalRange physical,
                                 PackosError* error);

PackosMemoryPhysicalRange
PackosMemoryPhysicalRangeGetPrev(PackosMemoryPhysicalRange physical,
                                 PackosError* error);

PackosMemoryPhysicalRange
PackosMemoryPhysicalRangeSeekAddr(void* physicalAddr,
                                  PackosError* error);

int PackosMemoryInit(PackosError* error);

int
PackosMemoryPhysicalRangeDefine(void* physicalAddr,
                                PackosMemoryPhysicalType type,
                                uint32_t len,
                                PackosError* error);

PackosMemoryPhysicalRange
PackosMemoryPhysicalRangeAlloc(PackosMemoryPhysicalType type,
                               uint32_t len,
                               uint32_t alignmentMultiple,
                               PackosError* error);

int
PackosMemoryPhysicalRangeFree(PackosMemoryPhysicalRange physical,
                              PackosError* error);

/** If ((flags & packosMemoryFlagCopyOnWrite)
 *       && !(flags & packosMemoryFlagWritable)
 *      ), will report
 *   packosErrorInvalidArg.
 */
PackosMemoryLogicalRange
PackosMemoryLogicalRangeNew(PackosMemoryPhysicalRange physical,
                            PackosContext context,
                            uint32_t flags,
                            void* logicalAddr,
                            uint32_t offset,
                            uint32_t len,
                            PackosError* error);

PackosMemoryLogicalRange
PackosMemoryLogicalRangeNewK(PackosMemoryPhysicalRange physical,
                             uint32_t flags,
                             void* logicalAddr,
                             uint32_t offset,
                             uint32_t len,
                             PackosError* error);

int
PackosMemoryLogicalRangeFree(PackosMemoryLogicalRange logical,
                             PackosContext context,
                             PackosError* error);

PackosMemoryLogicalRange
PackosMemoryPhysicalRangeGetFirstUser(PackosMemoryPhysicalRange physical,
                                      PackosError* error);
PackosMemoryLogicalRange
PackosMemoryPhysicalRangeGetNextUser(PackosMemoryLogicalRange logical,
                                     PackosError* error);
void*
PackosMemoryPhysicalRangeGetAddr(PackosMemoryPhysicalRange physical,
                                 PackosError* error);
uint32_t
PackosMemoryPhysicalRangeGetLen(PackosMemoryPhysicalRange physical,
                                PackosError* error);
PackosMemoryPhysicalType
PackosMemoryPhysicalRangeGetType(PackosMemoryPhysicalRange physical,
                                 PackosError* error);
bool
PackosMemoryPhysicalRangeGetInUse(PackosMemoryPhysicalRange physical,
                                  PackosError* error);

PackosMemoryPhysicalRange
PackosMemoryLogicalRangeGetPhysical(PackosMemoryLogicalRange logical,
                                    PackosError* error);

PackosMemoryLogicalRange
PackosMemoryLogicalRangeGetNext(PackosMemoryLogicalRange logical,
                                PackosError* error);
PackosMemoryLogicalRange
PackosMemoryLogicalRangeGetPrev(PackosMemoryLogicalRange logical,
                                PackosError* error);
void*
PackosMemoryLogicalRangeGetAddr(PackosMemoryLogicalRange logical,
                                PackosError* error);
uint32_t
PackosMemoryLogicalRangeGetLen(PackosMemoryLogicalRange logical,
                               PackosError* error);
uint32_t
PackosMemoryLogicalRangeGetFlags(PackosMemoryLogicalRange logical,
                                 PackosError* error);

#endif /*_PACKOS_MEMORY_H_*/
