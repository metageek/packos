#ifndef _IFACE_H_
#define _IFACE_H_

#include <ip.h>
#include <tcp.h>
#include <ip-filter.h>
#include <packet-queue.h>

typedef int (*IpIfaceSendMethod)
     (IpIface iface, PackosPacket* packet, PackosError* error);
typedef PackosPacket* (*IpIfaceReceiveMethod)
     (IpIface iface, PackosError* error);
typedef int (*IpIfaceCloseMethod)
     (IpIface iface, PackosError* error);

typedef struct TcpIfaceContext* TcpIfaceContext;

struct IpIface {
  IpIfaceSendMethod send;
  IpIfaceReceiveMethod receive;
  IpIfaceCloseMethod close;
  void* context;

  PackosAddress addr;
  PackosAddressMask mask;

  PackosPacketQueue queue;

  struct {
    IpFilter first;
    IpFilter last;
  } filters;

  struct {
    uint16_t udp,tcp;
  } anonPortNext;

  TcpIfaceContext tcpContext;

  IpIface next;
  IpIface prev;
};

int IpIfaceInit(IpIface iface,
                PackosError* error);
int IpIfaceClose(IpIface iface,
                 PackosError* error);
int IpIfaceEnqueue(IpIface iface,
                   PackosPacket* packet,
                   PackosError* error);
PackosPacket* IpIfaceDequeue(IpIface iface,
                             byte protocolExpected,
                             PackosError* error);
PackosPacket* IpIfaceDequeueFromAny(byte protocolExpected,
                                    IpIface* ifaceReceivedOn,
                                    PackosError* error);

int IpIfaceRegister(IpIface iface, PackosError* error);
int IpIfaceUnregister(IpIface iface, PackosError* error);
IpIface IpIfaceGetFirst(PackosError* error);
PackosAddress IpIfaceGetAddress(IpIface iface, PackosError* error);
PackosAddressMask IpIfaceGetMask(IpIface iface, PackosError* error);
IpIface IpIfaceLookupSend(PackosAddress v6Dest,
                          PackosError* error);
IpIface IpIfaceLookupReceive(PackosAddress addr, PackosError* error);

#endif /*_IFACE_H_*/
