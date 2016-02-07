#include <util/alloc.h>
#include <util/stream.h>
#include <packos/sys/contextP.h>
#include <iface.h>

typedef struct {
  IpIface first;
  IpIface last;

  struct {
    PackosAddress addr;
    IpIface iface;
  } defaultRoute;
} Registry;

static Registry* GetRegistry(PackosError* error)
{
  Registry* res;
  PackosContext cur=PackosKernelContextCurrent(error);
  if (!cur) return 0;

  res=(Registry*)(PackosKernelContextGetIfaceRegistry(cur,error));
  if (res) return res;

  if ((*error)!=packosErrorNone) return 0;

  res=(Registry*)(malloc(sizeof(Registry)));
  if (!res)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  res->first=res->last=0;
  res->defaultRoute.iface=0;
  if (PackosKernelContextSetIfaceRegistry(cur,res,0,error)<0)
    {
      free(res);
      return 0;
    }

  return res;
}

int IpIfaceRegister(IpIface iface, PackosError* error)
{
  Registry* registry;
  if (!iface)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  registry=GetRegistry(error);
  if (!registry) return -1;

  if ((iface->next) || (iface->prev) || (iface==registry->first))
    {
      *error=packosErrorIfaceAlreadyRegistered;
      return -1;
    }

  if ((iface->prev=registry->last)!=0)
    registry->last->next=iface;
  else
    registry->first=iface;

  registry->last=iface;

  return 0;
}

int IpIfaceUnregister(IpIface iface, PackosError* error)
{
  Registry* registry;

  if (!iface)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  registry=GetRegistry(error);
  if (!registry) return -1;

  if (!((iface->next) || (iface->prev) || (iface==registry->first)))
    {
      *error=packosErrorIfaceNotRegistered;
      return -1;
    }

  if (iface->prev)
    iface->prev->next=iface->next;
  else
    registry->first=iface->next;

  if (iface->next)
    iface->next->prev=iface->prev;
  else
    registry->last=iface->prev;

  return 0;
}

IpIface IpIfaceGetFirst(PackosError* error)
{
  Registry* registry=GetRegistry(error);
  if (!registry) return 0;

  if (registry->first) return registry->first;

  *error=packosErrorDoesNotExist;
  return 0;
}

IpIface IpIfaceLookupSend(PackosAddress addr,
                          PackosError* error)
{
  IpIface cur;

  Registry* registry=GetRegistry(error);
  if (!registry) return 0;

  for (cur=registry->first; cur; cur=cur->next)
    {
      if (PackosAddrMatch(cur->mask,addr))
        return cur;
    }

  *error=packosErrorDoesNotExist;
  return 0;
}

IpIface IpIfaceLookupReceive(PackosAddress addr, PackosError* error)
{
  IpIface cur;

  Registry* registry=GetRegistry(error);
  if (!registry) return 0;

  for (cur=registry->first; cur; cur=cur->next)
    {
      if (PackosAddrEq(cur->addr,addr))
        return cur;
    }

  *error=packosErrorDoesNotExist;
  return 0;
}

int IpSetDefaultRoute(PackosAddress addr,
                      IpIface iface,
                      PackosError* error)
{
  Registry* registry;
  if (!error) return -2;
  if (!iface)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  registry=GetRegistry(error);
  if (!registry)
    {
      UtilPrintfStream(errStream,error,"IpSetDefaultRoute(): GetRegistry(): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  registry->defaultRoute.addr=addr;
  registry->defaultRoute.iface=iface;
  return 0;
}

int IpGetDefaultRoute(PackosAddress* addr,
                      IpIface* iface,
                      PackosError* error)
{
  Registry* registry;
  if (!error) return -2;
  if (!(addr && iface))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  registry=GetRegistry(error);
  if (!registry)
    {
      UtilPrintfStream(errStream,error,"IpGetDefaultRoute(): GetRegistry(): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  if (!(registry->defaultRoute.iface))
    {
      *error=packosErrorDoesNotExist;
      return -1;
    }

  *addr=registry->defaultRoute.addr;
  *iface=registry->defaultRoute.iface;
  return 0;
}
