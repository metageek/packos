#include <util/stream.h>
#include <util/alloc.h>

#include <iface.h>
#include <icmp.h>

int IpIfaceInit(IpIface iface,
                PackosError* error)
{
  if (!iface)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  iface->queue=PackosPacketQueueNew(16,error);
  iface->next=iface->prev=0;
  iface->context=0;
  iface->filters.first=iface->filters.last=0;
  iface->anonPortNext.udp=iface->anonPortNext.tcp=anonPortMin;
  iface->tcpContext=0;

  if (IcmpInit(iface,error)<0)
    return -1;

  return 0;
}

int IpIfaceClose(IpIface iface,
                 PackosError* error)
{
  if (!iface)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (IpIfaceUnregister(iface,error)<0)
    {
      UtilPrintfStream(errStream,error,"IpIfaceUnregister: %s\n",PackosErrorToString(*error));
      return -1;
    }

  if (iface->close)
    {
      if (((iface->close)(iface,error))<0)
        return -1;
    }

  if (iface->queue)
    {
      if (PackosPacketQueueDelete(iface->queue,error)<0)
        {
          UtilPrintfStream(errStream,error,
                  "PackosPacketQueueDelete: %s\n",PackosErrorToString(*error));
          return -1;
        }
    }

  free(iface);
  return 0;
}

int IpIfaceEnqueue(IpIface iface,
                   PackosPacket* packet,
                   PackosError* error)
{
  if (!(iface && packet))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }
  return PackosPacketQueueEnqueue(iface->queue,packet,error);
}

PackosPacket* IpIfaceDequeue(IpIface iface,
                             byte protocolExpected,
                             PackosError* error)
{
  if (!iface)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }
  return PackosPacketQueueDequeue(iface->queue,protocolExpected,error);
}

PackosPacket* IpIfaceDequeueFromAny(byte protocolExpected,
                                    IpIface* ifaceReceivedOn,
                                    PackosError* error)
{
  IpIface cur=IpIfaceGetFirst(error);
  if (!cur) return 0;

  while (cur)
    {
      PackosPacket* res=IpIfaceDequeue(cur,protocolExpected,error);
      if (res)
        {
          if (ifaceReceivedOn)
            *ifaceReceivedOn=cur;
          return res;
        }
      if ((*error)!=packosErrorQueueEmpty) return 0;
      cur=cur->next;
    }

  *error=packosErrorQueueEmpty;
  return 0;
}

PackosAddress IpIfaceGetAddress(IpIface iface, PackosError* error)
{
  if (!iface)
    {
      *error=packosErrorInvalidArg;
      return PackosAddrGetZero();
    }

  *error=packosErrorNone;
  return iface->addr;
}

PackosAddressMask IpIfaceGetMask(IpIface iface, PackosError* error)
{
  if (!iface)
    {
      PackosAddressMask mask;
      mask.addr=PackosAddrGetZero();
      mask.len=32;

      *error=packosErrorInvalidArg;
      return mask;
    }

  *error=packosErrorNone;
  return iface->mask;
}
