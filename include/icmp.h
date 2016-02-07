#ifndef _PACKOS_LIBS_IP_ICMP_H_
#define _PACKOS_LIBS_IP_ICMP_H_

#include <ip.h>
#include <icmp-types.h>

int IcmpSendError(IpIface iface,
		  byte type,
		  byte code,
		  PackosPacket* inReplyTo,
		  PackosError* error);

int IcmpInit(IpIface iface,
             PackosError* error);

int IcmpSendDestinationUnreachable(IpIface iface,
				   IcmpDestinationUnreachableCode code,
				   PackosPacket* inReplyTo,
				   PackosError* error);

#endif /*_PACKOS_LIBS_IP_ICMP_H_*/

