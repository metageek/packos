#ifndef _IFACE_SHM_H_
#define _IFACE_SHM_H_

#include <iface.h>
#include <udp.h>

IpIface IpIfaceShmNew(PackosAddress addr,
                      PackosAddressMask mask,
                      PackosError* error);

UdpSocket IpIfaceShmGetSendInterruptSocket(IpIface iface,
					   PackosError* error);
UdpSocket IpIfaceShmGetReceiveInterruptSocket(IpIface iface,
					      PackosError* error);

int IpIfaceShmOnSendInterrupt(PackosError* error);
int IpIfaceShmOnReceiveInterrupt(PackosError* error);

#endif /*_IFACE_SHM_H_*/
