#include <util/stream.h>
#include <util/alloc.h>
#include <iface.h>

typedef struct {
  bool yieldToRecipient;
  PackosContext nextToYieldTo;
} IpIfaceNativeContext;

static int send(IpIface iface, PackosPacket* packet, PackosError* error)
{
  IpIfaceNativeContext* context=(IpIfaceNativeContext*)(iface->context);
  packet->packos.src=iface->addr;
  return PackosPacketSend(packet,context->yieldToRecipient,error);
}

static PackosPacket* receive(IpIface iface, PackosError* error)
{
  IpIfaceNativeContext* context=(IpIfaceNativeContext*)(iface->context);
  if (context->nextToYieldTo)
    {
      PackosPacket* res
        =PackosPacketReceiveOrYieldTo(context->nextToYieldTo,error);
      context->nextToYieldTo=0;

      if (!res)
        {
          if ((*error)!=packosErrorContextYieldedBack)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,&tmp,
                               "iface-native:receive(): PackosPacketReceiveOrYieldTo(): %s\n",
                               PackosErrorToString(*error)
                               );
            }
          return 0;
        }

      return res;
    }
  else
    {
      PackosPacket* res=PackosPacketReceive(error);
      if (!res)
        {
          PackosError tmp;
          UtilPrintfStream(errStream,&tmp,
                           "iface-native:receive(): PackosPacketReceive(): %s\n",
                           PackosErrorToString(*error)
                           );
          return 0;
        }
      return res;
    }
}

IpIface IpIfaceNativeNew(PackosError* error)
{
  IpIfaceNativeContext* context;
  IpIface res=(IpIface)(malloc(sizeof(struct IpIface)));
  if (!res)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  context=(IpIfaceNativeContext*)(malloc(sizeof(IpIfaceNativeContext)));
  if (!context)
    {
      *error=packosErrorOutOfMemory;
      free(res);
      return 0;
    }

  if (IpIfaceInit(res,error)<0)
    {
      free(res);
      return 0;
    }

  res->addr=PackosMyAddress(error);
  if ((*error)!=packosErrorNone)
    {
      free(res);
      return 0;
    }

  res->mask=PackosSysAddressMask(error);
  if ((*error)!=packosErrorNone)
    {
      free(res);
      return 0;
    }

  context->yieldToRecipient=true;
  context->nextToYieldTo=0;
  res->context=context;

  res->send=send;
  res->receive=receive;
  res->close=0;

  return res;
}

int IpIfaceNativeSetNextYieldTo(IpIface iface,
                                PackosContext yieldTo,
                                PackosError* error)
{
  if (!error) return -2;
  if (!iface)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  ((IpIfaceNativeContext*)(iface->context))->nextToYieldTo=yieldTo;
  return 0;
}

int IpIfaceNativeSetYieldToRecipient(IpIface iface,
                                     bool yieldToRecipient,
                                     PackosError* error)
{
  if (!error) return -2;
  if (!iface)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  ((IpIfaceNativeContext*)(iface->context))->yieldToRecipient=yieldToRecipient;
  return 0;
}
