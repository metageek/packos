#ifndef _IFACE_NATIVE_H_
#define _IFACE_NATIVE_H_

#include <iface.h>

IpIface IpIfaceNativeNew(PackosError* error);
IpIface IpIfaceNativeGet(PackosError* error);
int IpIfaceNativeSetNextYieldTo(IpIface iface,
                                PackosContext yieldTo,
                                PackosError* error);
int IpIfaceNativeSetYieldToRecipient(IpIface iface,
                                     bool yieldToRecipient,
                                     PackosError* error);

#endif /*_IFACE_NATIVE_H_*/
