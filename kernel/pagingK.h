#ifndef _KERNEL_PAGINGK_H_
#define _KERNEL_PAGINGK_H_

#include <packos/types.h>
#include <packos/errors.h>

typedef struct {
  unsigned int lowLimit:16;
  unsigned int lowBase:24 __attribute__ ((packed));
  unsigned int type:4;
  unsigned int system:1;
  unsigned int privilegeLevel:2;
  unsigned int present:1;
  unsigned int highLimit:4;
  unsigned int availableForOS:1;
  unsigned int zero:1;
  unsigned int is32Bit:1;
  unsigned int limitsArePages:1;
  unsigned int highBase:8;
} Packos386SegmentDescriptor;

typedef enum {
  packos386DescriptorTypeInvalid=0,
  packos386DescriptorTypeCode,
  packos386DescriptorTypeData,
  packos386DescriptorTypeTSS16,
  packos386DescriptorTypeLDT,
  packos386DescriptorTypeCallGate16,
  packos386DescriptorTypeTaskGate,
  packos386DescriptorTypeInterruptGate16,
  packos386DescriptorTypeTrapGate16,
  packos386DescriptorTypeTSS32,
  packos386DescriptorTypeCallGate32,
  packos386DescriptorTypeInterruptGate32,
  packos386DescriptorTypeTrapGate32
} Packos386DescriptorType;

typedef enum {
  packos386DescriptorUsageNone=0,
  packos386DescriptorUsageUserDS,
  packos386DescriptorUsageExceptionStack
} Packos386DescriptorUsage;

typedef struct {
  void* base;
  uint32_t size;
  bool inPages,accessed,availableForOS,present;
  Packos386DescriptorType type;
  unsigned int privilegeLevel:2;
  struct {
    Packos386DescriptorUsage usage;
  } metadata;
  union {
    struct {
      bool isWritable;
      struct {
        bool big,expandsDown;
      } stack;
    } data;
    struct {
      bool is32Bit,isReadableAsData,conforming;
    } code;
    struct {
      bool busy;
    } tss;
    struct {
      uint16_t segmentSelector;
      unsigned int paramCount:5;
    } callGate32;
  } u;
} Packos386SegmentDescriptorSane;

typedef struct {
  uint16_t index;
  bool isLocal;
  byte privilegeLevel;
} Packos386SegmentSelectorParsed;

Packos386SegmentDescriptor*
Packos386GetDescriptorTable(uint16_t* numDescriptors);

int Packos386RegisterMaxDescriptor(int max, PackosError* error);
int Packos386AllocateDescriptor(PackosError* error);

const Packos386SegmentDescriptor* Packos386GetGDT(uint16_t* numDescriptors);

/* Equivalent to Packos386GetGDT(0)+i, but with error checking. */
const Packos386SegmentDescriptor*
Packos386GetGDTDescriptor(uint16_t i,
                          PackosError* error);

int Packos386SetGDT(Packos386SegmentDescriptor* descriptors,
                    uint16_t numDescriptors,
                    PackosError* error);
uint16_t Packos386GetLDT(void);
uint32_t Packos386GetCR0(void);
uint32_t Packos386GetCR1(void);
uint32_t Packos386GetCR2(void);
uint32_t Packos386GetCR3(void);
void Packos386SetCR0(uint32_t cr0);
void Packos386SetCR3(uint32_t cr3);

int Packos386InterpretSegmentSelector(uint16_t segment,
                                      Packos386SegmentSelectorParsed* parsed,
                                      PackosError* error);

int Packos386InterpretSegmentDescriptor(const Packos386SegmentDescriptor* desc,
                                        Packos386SegmentDescriptorSane* sane,
                                        PackosError* error);
int Packos386BuildSegmentDescriptor(const Packos386SegmentDescriptorSane* sane,
                                    Packos386SegmentDescriptor* desc,
                                    PackosError* error);
const char* Packos386DescriptorTypeToString(Packos386DescriptorType type,
                                            PackosError* error);

int Packos386DumpDescriptors(const Packos386SegmentDescriptor* descriptors,
                             uint16_t count,
                             bool onlyIfDebugging,
                             PackosError* error);
int Packos386DumpGDT(bool onlyIfDebugging, PackosError* error);

int Packos386DumpDescriptor(const Packos386SegmentDescriptor* descriptor,
                            int i,
                            bool verbose,
                            PackosError* error);

/* Equivalent to Packos386DumpDescriptor(Packos386GetGDT(0)+i,error).
 *  But with range checking on i, of course.
 */
int Packos386DumpDescriptorByIndex(uint16_t i,
                                   PackosError* error);

#endif /*_KERNEL_PAGINGK_H_*/
