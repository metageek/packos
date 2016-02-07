#ifndef _IP_FILTER_H_
#define _IP_FILTER_H_

#include <ip.h>

typedef enum {
  ipFilterActionError,
  ipFilterActionErrorICMPed,
  ipFilterActionDrop,
  ipFilterActionForwarded,
  ipFilterActionPass,
  ipFilterActionReplied
} IpFilterAction;

typedef IpFilterAction (*IpFilterMethod)(IpIface iface,
                                         void* context,
                                         PackosPacket* packet,
                                         PackosError* error);

typedef struct IpFilter* IpFilter;
struct IpFilter {
  IpIface iface;
  IpFilterMethod filterMethod;
  void* context;

  IpFilter next;
  IpFilter prev;
};

IpFilter IpFilterInstall(IpIface iface,
                         IpFilterMethod filterMethod,
                         void* context,
                         PackosError* error);
int IpFilterUninstall(IpFilter filter,
                      PackosError* error);

PackosPacket* IpFilterApply(IpIface iface,
                            PackosPacket* packet,
                            PackosError* error);

#endif /*_IP_FILTER_H_*/
