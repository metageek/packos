#ifndef _PACKOS_CHECKSUMS_H_
#define _PACKOS_CHECKSUMS_H_

#include <packos/packet.h>
#include <ip.h>

int UdpChecksum(PackosPacket* packet,
                IpHeader* udp,
                PackosError* error);
uint16_t IcmpChecksum(PackosPacket* packet,
                      IpHeader* icmp,
                      PackosError* error);
int TcpChecksum(PackosPacket* packet,
                IpHeader* tcp,
                PackosError* error);

#endif /*_PACKOS_CHECKSUMS_H_*/
