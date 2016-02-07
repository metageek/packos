#include <util/stream.h>
#include <util/alloc.h>
#include <packos/packet.h>
#include <router.h>

#include <iface-native.h>
#include <iface-shm.h>
#include <ip-filter.h>
#include <icmp.h>

#define MAXROUTES 16

typedef struct {
  PackosAddressMask mask;
  IpIface iface;
} RoutingTableEntry;

typedef struct {
  int numRoutes;
  RoutingTableEntry routes[MAXROUTES];
  IpIface native,nonNative;
} Context;

static RoutingTableEntry* seekEntry(Context* context,
                                    PackosPacket* packet,
                                    PackosError* error)
{
  int i,N=context->numRoutes;
  if (N>MAXROUTES) N=MAXROUTES;

  {
    char destAddrBuff[80];
    const char* destAddrStr=destAddrBuff;
    PackosError tmp;

    if (PackosAddrToString(packet->ipv6.dest,
                           destAddrBuff,sizeof(destAddrBuff),
                           &tmp)
        <0)
      destAddrStr=PackosErrorToString(tmp);

    UtilPrintfStream(errStream,error,"seekEntry(%s)\n",destAddrStr);
  }

  for (i=0; i<N; i++)
    {
      RoutingTableEntry* cur=context->routes+i;

      {
        char maskAddrBuff[80],destAddrBuff[80];
        const char* maskAddrStr=maskAddrBuff;
        const char* destAddrStr=destAddrBuff;
        PackosError tmp;

        if (PackosAddrToString(cur->mask.addr,
                               maskAddrBuff,sizeof(maskAddrBuff),
                               &tmp)
            <0)
          maskAddrStr=PackosErrorToString(tmp);

        if (PackosAddrToString(packet->ipv6.dest,
                               destAddrBuff,sizeof(destAddrBuff),
                               &tmp)
            <0)
          destAddrStr=PackosErrorToString(tmp);

        UtilPrintfStream(errStream,error,"Is %s in %s/%d?\n",
                destAddrStr,maskAddrStr,cur->mask.len);
      }

      if (PackosAddrMatch(cur->mask,packet->ipv6.dest))
        return cur;
    }

  *error=packosErrorNoRouteToHost;
  return 0;
}

static IpFilterAction filterMethod(IpIface iface,
                                   void* context_,
                                   PackosPacket* packet,
                                   PackosError* error)
{
  Context* context=(Context*)context_;
  RoutingTableEntry* entry;

  UtilPrintfStream(errStream,error,"filterMethod\n");

  entry=seekEntry(context,packet,error);
  if (!entry)
    {
      UtilPrintfStream(errStream,error,"filterMethod(): seekEntry(): %s\n",
              PackosErrorToString(*error));
      return ipFilterActionError;
    }

  if (entry->iface==iface)
    return ipFilterActionPass;

  packet->packos.dest=packet->ipv6.dest;

  if (IpSendOn(entry->iface,packet,error)<0)
    {
      UtilPrintfStream(errStream,error,"filterMethod(): IpSendOn(): %s\n",
	      PackosErrorToString(*error));
      if ((*error)==packosErrorAddressUnreachable)
        {
          PackosError tmp;
          if (IcmpSendDestinationUnreachable
              (iface,
               icmpDestinationUnreachableCodeAddress,
               packet,
               &tmp)
              <0)
            {
              UtilPrintfStream(errStream,error,
                      "filterMethod(): IcmpSendDestinationUnreachable(): %s\n",
                      PackosErrorToString(tmp)
                      );
            }
          return ipFilterActionErrorICMPed;
        }
      return ipFilterActionError;
    }

  return ipFilterActionForwarded;
}

static bool unacceptableError(PackosError error)
{
  switch (error)
    {
    case packosErrorNone:
    case packosErrorPacketFilteredOut:
    case packosErrorStoppedForOtherSocket:
      return false;

    default:
      return true;
    }
}

void routerProcess(void)
{
  PackosError error;
  struct {
    IpIface iface;
    IpFilter filter;
  } native,shm;

  Context context;
  PackosAddressMask mask;

  UdpSocket irqSendSock,irqReceiveSock;

  if (UtilArenaInit(&error)<0)
    return;

  PackosAddress addr=PackosAddrFromString("7e8f::01",&error);
  if (error!=packosErrorNone)
    {
      UtilPrintfStream(errStream,&error,"router: PackosAddrFromString(addr): %s\n",
	      PackosErrorToString(error));
      return;
    }

  mask.addr=PackosAddrFromString("7e8f::",&error);
  if (error!=packosErrorNone)
    {
      UtilPrintfStream(errStream,&error,"router: PackosAddrFromString(mask): %s\n",
	      PackosErrorToString(error));
      return;
    }

  mask.len=16;

  native.iface=IpIfaceNativeNew(&error);
  if (!native.iface)
    {
      UtilPrintfStream(errStream,&error,"IpIfaceNativeNew: %s\n",PackosErrorToString(error));
      return;
    }

  if (IpIfaceRegister(native.iface,&error)<0)
    {
      UtilPrintfStream(errStream,&error,"IpIfaceRegister(native.iface): %s\n",PackosErrorToString(error));
      return;
    }

  shm.iface=IpIfaceShmNew(addr,mask,&error);
  if (!shm.iface)
    {
      UtilPrintfStream(errStream,&error,"IpIfaceShmNew: %s\n",PackosErrorToString(error));
      return;
    }
  
  if (IpIfaceRegister(shm.iface,&error)<0)
    {
      UtilPrintfStream(errStream,&error,"IpIfaceRegister(shm.iface): %s\n",PackosErrorToString(error));
      return;
    }

  context.native=native.iface;
  context.routes[context.numRoutes].mask=PackosSysAddressMask(&error);
  context.routes[context.numRoutes++].iface=native.iface;
  if (error!=packosErrorNone)
    {
      UtilPrintfStream(errStream,&error,"PackosSysAddressMask(): %s\n",PackosErrorToString(error));
      return;
    }

  context.nonNative=shm.iface;
  context.routes[context.numRoutes].mask=mask;
  context.routes[context.numRoutes++].iface=shm.iface;

  context.routes[context.numRoutes].mask.addr
    =PackosAddrFromString("::",&error);
  if (error!=packosErrorNone)
    {
      UtilPrintfStream(errStream,&error,"PackosAddrFromString(\"::\"): %s\n",PackosErrorToString(error));
      return;
    }
  context.routes[context.numRoutes].mask.len=0;
  context.routes[context.numRoutes++].iface=shm.iface;

  native.filter=IpFilterInstall(native.iface,filterMethod,&context,&error);
  if (!native.filter)
    {
      UtilPrintfStream(errStream,&error,"IpFilterInstall(native.filter): %s\n",PackosErrorToString(error));
      return;
    }

  shm.filter=IpFilterInstall(shm.iface,filterMethod,&context,&error);
  if (!shm.filter)
    {
      UtilPrintfStream(errStream,&error,"IpFilterInstall(shm.filter): %s\n",PackosErrorToString(error));
      return;
    }

  irqSendSock=IpIfaceShmGetSendInterruptSocket(shm.iface,&error);
  if (!irqSendSock)
    {
      UtilPrintfStream(errStream,&error,"IpIfaceShmGetSendInterruptSocket(): %s\n",
	      PackosErrorToString(error));
      return;
    }

  irqReceiveSock=IpIfaceShmGetReceiveInterruptSocket(shm.iface,&error);
  if (!irqReceiveSock)
    {
      UtilPrintfStream(errStream,&error,"IpIfaceShmGetReceiveInterruptSocket(): %s\n",
	      PackosErrorToString(error));
      return;
    }

  while (1)
    {
      PackosError error;
      PackosPacket* packet;

      UtilPrintfStream(errStream,&error,"Router about to receive\n");
      packet=UdpSocketReceive(irqSendSock,0,true,&error);
      if (packet)
	{
	  UtilPrintfStream(errStream,&error,"Router received packet on irqSendSock\n");
	  if (IpIfaceShmOnSendInterrupt(&error)<0)
	    {
	      UtilPrintfStream(errStream,&error,"IpIfaceShmOnSendInterrupt(): %s\n",
		      PackosErrorToString(error));
	      return;
	    }
	}
      else
	{
	  if (unacceptableError(error))
	    {
	      UtilPrintfStream(errStream,&error,"UdpSocketReceive(irqSendSock): %s\n",
		      PackosErrorToString(error));
	      return;
	    }

	  UtilPrintfStream(errStream,&error,"Router received packet, not on irqSendSock\n");

	  if (UdpSocketReceivePending(irqReceiveSock,&error))
	    {
	      UtilPrintfStream(errStream,&error,"Router: packet pending on irqReceiveSock\n");
	      packet=UdpSocketReceive(irqReceiveSock,0,true,&error);
	      if (!packet)
		{
		  UtilPrintfStream(errStream,&error,"UdpSocketReceive(irqReceiveSock): %s\n",
			  PackosErrorToString(error));
		  return;
		}

	      UtilPrintfStream(errStream,&error,"Router received packet on irqReceiveSock\n");
              PackosPacketFree(packet,&error);

	      if (IpIfaceShmOnReceiveInterrupt(&error)<0)
		{
		  UtilPrintfStream(errStream,&error,"IpIfaceShmOnReceiveInterrupt(): %s\n",
			  PackosErrorToString(error));
		  return;
		}

	      packet=IpReceiveOn(shm.iface,0,0,&error);
	      if (packet)
		{
		  UtilPrintfStream(errStream,&error,
			  "Router received packet via shm, not forwarded\n");
                  PackosPacketFree(packet,&error);
		}
	      else
		{
                  if (unacceptableError(error))
                    {
		      UtilPrintfStream(errStream,&error,"IpReceiveOn(shm): %s\n",
			      PackosErrorToString(error));
		      return;
		    }
		}
	    }
	  else
	    {
	      if (unacceptableError(error))
		{
		  UtilPrintfStream(errStream,&error,"UdpSocketReceivePending(): %s\n",
			  PackosErrorToString(error));
		  return;
		}

	      UtilPrintfStream(errStream,&error,"Router seems to have received some other packet\n");
	    }
	}

      if (packet)
        PackosPacketFree(packet,&error);
    }
}
