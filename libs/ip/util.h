#ifndef _PACKOS_LIBS_IP_UTIL_H_
#define _PACKOS_LIBS_IP_UTIL_H_

#include <packos/packet.h>

uint16_t IpUtilAdd1sComplement(uint16_t a, uint16_t b);
uint16_t IpUtilAdd1sComplementAddr(uint16_t a, const PackosAddress* addr);

#endif /*_PACKOS_LIBS_IP_UTIL_H_*/

