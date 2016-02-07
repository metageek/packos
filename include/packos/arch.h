#ifndef _PACKOS_ARCH_H_
#define _PACKOS_ARCH_H_

#include <packos/types.h>

#ifdef  __powerpc__
#define PACKOS_BIG_ENDIAN
#else
#define PACKOS_LITTLE_ENDIAN
#endif

uint16_t packosSwab16(uint16_t i);
uint32_t packosSwab32(uint32_t i);

#ifdef PACKOS_LITTLE_ENDIAN
#define htons(x) packosSwab16(x)
#define ntohs(x) packosSwab16(x)
#define htonl(x) packosSwab32(x)
#define ntohl(x) packosSwab32(x)
#else
#define htons(x) (x)
#define ntohs(x) (x)
#define htonl(x) (x)
#define ntohl(x) (x)
#endif

#endif /*_PACKOS_ARCH_H_*/
