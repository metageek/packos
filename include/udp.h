#ifndef _UDP_H_
#define _UDP_H_

#include <ip.h>

typedef struct UdpSocket* UdpSocket;

UdpSocket UdpSocketNew(PackosError* error);
int UdpSocketClose(UdpSocket socket,
                   PackosError* error);

int UdpSocketBind(UdpSocket socket,
                  PackosAddress addr,
                  uint16_t port,
                  PackosError* error);

/* Returns a 16-bit port number, or <0 on error. */
int UdpSocketGetLocalPort(UdpSocket socket,
                          PackosError* error);

/* Convenience method.  Returns packet created with PackosPacketAlloc(). */
PackosPacket* UdpPacketNew(UdpSocket socket,
                           uint16_t datalen,
                           IpHeaderUDP** udpHeader,
                           PackosError* error);

IpHeaderUDP* UdpPacketSeekHeader(PackosPacket* packet,
                                 PackosError* error);

int UdpSocketSend(UdpSocket socket,
                  PackosPacket* packet,
                  PackosError* error);

PackosPacket* UdpSocketReceive(UdpSocket socket,
                               IpHeaderRouting0** routingHeader,
                               bool stopWhenReceiveOtherPacket,
                               PackosError* error);

bool UdpSocketReceivePending(UdpSocket socket,
                             PackosError* error);

#endif /*_UDP_H_*/
