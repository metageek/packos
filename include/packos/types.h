#ifndef _PACKOS_TYPES_H_
#define _PACKOS_TYPES_H_

#ifdef __x86_64__
#define PACKOS_64BIT
#else
#define PACKOS_32BIT
#endif

#define __int8_t_defined

typedef short int16_t;
typedef int int32_t;

typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

typedef unsigned char byte;
typedef byte uint8_t;
typedef int bool;

#ifdef PACKOS_64BIT
typedef long int int64_t;
typedef unsigned long int uint64_t;
typedef uint64_t SizeType;
typedef int64_t PtrDiffType;
#else
typedef long long int64_t;
typedef unsigned long long uint64_t;
typedef uint32_t SizeType;
typedef int32_t PtrDiffType;
#endif

#define true 1
#define false 0

#define PACKOS_PACKET_QUEUE_LEN 16

typedef struct PackosContext* PackosContext;
typedef struct PackosMemoryPhysicalRange* PackosMemoryPhysicalRange;
typedef struct PackosMemoryLogicalRange* PackosMemoryLogicalRange;

#define PACKOS_CONTEXT_MAX_NAMELEN 255

#endif /*_PACKOS_TYPES_H_*/
