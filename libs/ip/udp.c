#include <util/alloc.h>
#include <util/stream.h>

#include <contextQueue.h>
#include <udp.h>
#include <iface.h>
#include <packos/sys/contextP.h>
#include <packos/arch.h>
#include <packos/checksums.h>
#include "util.h"
#include <icmp.h>

struct UdpSocket {
  IpIface iface;
  PackosContext owner;
  PackosAddress addr;
  uint16_t port;

  PackosPacketQueue queue;

  UdpSocket next;
  UdpSocket prev;
};

static struct {
  UdpSocket first;
  UdpSocket last;
} sockets={0,0};

UdpSocket UdpSocketNew(PackosError* error)
{
  UdpSocket res=(UdpSocket)(malloc(sizeof(struct UdpSocket)));
  if (!res)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  res->queue=PackosPacketQueueNew(16,error);
  if (!(res->queue))
    {
      free(res);
      return 0;
    }

  res->owner=PackosKernelContextCurrent(error);
  if (!(res->owner))
    {
      free(res);
      return 0;
    }

  res->iface=0;
  res->addr=PackosAddrGetZero();
  res->port=0;

  if ((res->prev=sockets.last)!=0)
    res->prev->next=res;
  else
    sockets.first=res;
  res->next=0;

  sockets.last=res;
  return res;
}

int UdpSocketClose(UdpSocket socket,
                   PackosError* error)
{
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (PackosPacketQueueDelete(socket->queue,error)<0) return -1;

  if (socket->next)
    socket->next->prev=socket->prev;
  else
    sockets.last=socket->prev;

  if (socket->prev)
    socket->prev->next=socket->next;
  else
    sockets.first=socket->next;

  free(socket);
  return 0;
}

static UdpSocket seek(PackosAddress addr,
		      uint16_t port,
		      PackosError* error)
{
  UdpSocket cur;
  PackosContext self;

  if (!port)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  self=PackosKernelContextCurrent(error);
  if (!self) return 0;

  for (cur=sockets.first; cur; cur=cur->next)
    {
      if (cur->owner!=self) continue;
      if (cur->port!=port) continue;
      if (PackosAddrIsZero(cur->addr)
          || PackosAddrIsZero(addr)
          || PackosAddrEq(cur->addr,addr)
          )
        return cur;
    }

  *error=packosErrorDoesNotExist;
  return 0;
}

static uint16_t seekAnonPort(IpIface iface,
                             PackosAddress addr,
                             PackosError* error)
{
  uint16_t port;
  for (port=iface->anonPortNext.udp; port<=anonPortMax; port++)
    {
      PackosError tmp;
      if (!seek(addr,port,&tmp)) return port;
    }

  for (port=anonPortMax; port<iface->anonPortNext.udp; port++)
    {
      PackosError tmp;
      if (!seek(addr,port,&tmp)) return port;
    }

  *error=packosErrorAllAnonPortsBound;
  return 0;
}

int UdpSocketBind(UdpSocket socket,
                  PackosAddress addr,
                  uint16_t port,
                  PackosError* error)
{
  IpIface iface;

  if ((!socket)
      || (socket->port)
      )
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (PackosAddrIsZero(addr))
    iface=0;
  else
    {
      iface=IpIfaceLookupReceive(addr,error);
      if (!iface) return -1;
    }

  if (!port)
    {
      IpIface i=iface;
      if (!i)
        {
          i=IpIfaceGetFirst(error);
          if (!i)
            {
              UtilPrintfStream(errStream,error,"UdpPacketNew(): IpIfaceGetFirst(): %s\n",
                      PackosErrorToString(*error));
              return -1;
            }
        }

      port=seekAnonPort(i,addr,error);
      if (!port)
        {
          UtilPrintfStream(errStream,error,"UdpPacketNew(): seekAnonPort(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }
    }
  else
    {
      UdpSocket other=seek(addr,port,error);
      if (other)
        {
          *error=packosErrorPortAlreadyBound;
          return -1;
        }
    }

#if 0
  UtilPrintfStream(errStream,error,
                   "<%s>: UdpSocketBind(%d)\n",
                   PackosContextGetOwnName(error),
                   (int)port);
#endif

  socket->iface=iface;

  socket->addr=addr;
  socket->port=port;
  return 0;
}

int UdpSocketGetLocalPort(UdpSocket socket,
                          PackosError* error)
{
  if (!error) return -2;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (!(socket->port))
    {
      *error=packosErrorSocketNotBound;
      return -1;
    }

  return socket->port;
}

/* Convenience method.  Returns packet created with PackosPacketAlloc(). */
PackosPacket* UdpPacketNew(UdpSocket socket,
                           uint16_t datalen,
                           IpHeaderUDP** udpHeader,
                           PackosError* error)
{
  PackosPacket* res;

  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  if (!(socket->port))
    {
      *error=packosErrorSocketNotBound;
      return 0;
    }

  res=PackosPacketAlloc(0,error);
  if (!res)
    {
      UtilPrintfStream(errStream,error,"UdpPacketNew(): PackosPacketAlloc(): %s\n",
              PackosErrorToString(*error));
      return 0;
    }

  if (socket->iface)
    {
      res->ipv6.src=IpIfaceGetAddress(socket->iface,error);
      if ((*error)!=packosErrorNone)
        {
          PackosError tmp;
          PackosPacketFree(res,&tmp);
          UtilPrintfStream(errStream,error,"UdpPacketNew(): IpIfaceGetAddress(): %s\n",
                  PackosErrorToString(*error));
          return 0;
        }
    }

  {
    IpHeader h;
    IpHeaderUDP udp;

    h.kind=ipHeaderTypeUDP;
    h.u.udp=&udp;
    udp.sourcePort=socket->port;
    udp.destPort=0;
    udp.length=datalen+sizeof(IpHeaderUDP);
    udp.checksum=0; /* Will have to compute when sent */

    
    if (IpHeaderAppend(res,&h,error)<0)
      {
        PackosError tmp;
        PackosPacketFree(res,&tmp);
        UtilPrintfStream(errStream,error,"UdpPacketNew(): IpHeaderAppend(): %s\n",
                PackosErrorToString(*error));
        return 0;
      }

    if (udpHeader) *udpHeader=h.u.udp;
  }

  if (IpPacketSetDataLen(res,datalen,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,"UdpPacketNew(): IpPacketSetDataLen(): %s\n",
              PackosErrorToString(*error));
      PackosPacketFree(res,&tmp);
      return 0;
    }

  return res;
}

IpHeaderUDP* UdpPacketSeekHeader(PackosPacket* packet,
                                 PackosError* error)
{
  if (!error) return 0;
  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  {
    IpHeader* h=IpHeaderSeek(packet,ipHeaderTypeUDP,error);
    if (!h)
      {
        UtilPrintfStream(errStream,error,"UdpPacketSeekHeader(): IpHeaderSeek(): %s\n",
                PackosErrorToString(*error));
        return 0;
      }

    return h->u.udp;
  }
  
}

int UdpSocketSend(UdpSocket socket,
                  PackosPacket* packet,
                  PackosError* error)
{
  IpHeader* udp;
  if (!(socket && packet))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (!(socket->port))
    {
      *error=packosErrorSocketNotBound;
      return -1;
    }

  udp=IpHeaderSeek(packet,ipHeaderTypeUDP,error);
  if (!udp)
    {
      if ((*error)==packosErrorDoesNotExist) *error=packosErrorWrongProtocol;
      return -1;
    }

  if (!(udp->u.udp->destPort))
    {
      *error=packosErrorNoDestPort;
      return -1;
    }

  udp->u.udp->sourcePort=socket->port;

  {
    int checksum=UdpChecksum(packet,udp,error);
    if (checksum<0)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&tmp,
                         "UdpSocketSend(): UdpChecksum(): %s\n",
                         PackosErrorToString(*error));
        return -1;
      }
    udp->u.udp->checksum=checksum;
  }

  if (PackosAddrEq(packet->ipv6.src,packet->ipv6.dest))
    {
      UdpSocket otherSocket
        =seek(packet->ipv6.dest,udp->u.udp->destPort,error);
      if (!otherSocket)
        {
          *error=packosErrorNoDestPort;
          return -1;
        }

      if (PackosPacketQueueEnqueue(otherSocket->queue,packet,error)<0)
        {
          PackosError tmp;
          UtilPrintfStream(errStream,&tmp,
                           "UdpSocketSend(): PackosPacketQueueEnqueue(): %s\n",
                           PackosErrorToString(*error));
          return -1;
        }
      return 0;
    }

  if (socket->iface)
    return IpSendOn(socket->iface,packet,error);
  else
    return IpSend(packet,error);
}

PackosPacket* UdpSocketReceive(UdpSocket socket,
                               IpHeaderRouting0** routingHeader,
                               bool stopWhenReceiveOtherPacket,
                               PackosError* error)
{
  PackosPacket* res=0;
  bool isFirst=true;

  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  if (!(socket->port))
    {
      *error=packosErrorSocketNotBound;
      return 0;
    }

  res=PackosPacketQueueDequeue(socket->queue,ipHeaderTypeUDP,error);
  if (res) return res;
  if ((*error)!=packosErrorQueueEmpty)
    {
      UtilPrintfStream(errStream,error,"UdpSocketReceive(): PackosPacketQueueDequeue(): %s\n",
              PackosErrorToString(*error));
      return 0;
    }

  *error=packosErrorNone;
  while ((!res) && ((*error)==packosErrorNone))
    {
      IpIface ifaceReceivedOn;

      if (isFirst)
        isFirst=false;
      else
        {
          if (stopWhenReceiveOtherPacket)
            {
              *error=packosErrorStoppedForOtherSocket;
              return 0;
            }
        }

      if (socket->iface)
	{
	  res=IpReceiveOn(socket->iface,ipHeaderTypeUDP,routingHeader,error);
	  ifaceReceivedOn=socket->iface;
	}
      else
        res=IpReceive(ipHeaderTypeUDP,
                      routingHeader,&ifaceReceivedOn,
                      stopWhenReceiveOtherPacket,
                      error);

      if (!res)
        {
          switch (*error)
            {
            case packosErrorContextFinished:
            case packosErrorYieldedToBlocked:
              return 0;

            case packosErrorContextYieldedBack:
            case packosErrorPacketFilteredOut:
              *error=packosErrorNone;
              continue;

            default:
              UtilPrintfStream(errStream,error,"IpReceive[On]: %s\n",
                      PackosErrorToString(*error));
              continue;
            }
        }

      {
        IpHeader* h=IpHeaderSeek(res,ipHeaderTypeUDP,error);
        if (!(h && (h->kind==ipHeaderTypeUDP)))
          {
            PackosError tmp;
            PackosPacketFree(res,&tmp);
            UtilPrintfStream(errStream,error,
                             "UdpSocketReceive(): IpHeaderSeek(): %s\n",
                             PackosErrorToString(*error));
            *error=packosErrorNone;
            continue;
          }

        {
          PackosError tmp;
          int sum=UdpChecksum(res,h,&tmp);
          if (sum!=h->u.udp->checksum)
            UtilPrintfStream(errStream,error,"Checksum %x; I calculate %x\n",
                             (int)(h->u.udp->checksum),sum);
        }

        if (h->u.udp->destPort!=socket->port)
          {
            UdpSocket otherSocket
              =seek(res->ipv6.dest,h->u.udp->destPort,error);
            if (!otherSocket)
	      {
		if (IcmpSendDestinationUnreachable
		    (ifaceReceivedOn,
		     icmpDestinationUnreachableCodePort,
		     res,
		     error)
		    <0
		    )
		  UtilPrintfStream(errStream,error,
			  "UdpSocketReceive(): IcmpSendDestinationUnreachable(): %s\n",
			  PackosErrorToString(*error)
			  );
                else
                  UtilPrintfStream
                    (errStream,error,
                     "<%s>: UdpSocketReceive(): sent Port Unreachable\n",
                     PackosContextGetOwnName(error)
                     );

                PackosError tmp;
		PackosPacketFree(res,&tmp);
		if (*error==packosErrorDoesNotExist)
		  *error=packosErrorNone;
	      }
            else
              {
                if (PackosPacketQueueEnqueue(otherSocket->queue,res,error)<0)
                  {
                    {
                      PackosError tmp;
                      PackosPacketFree(res,&tmp);
                    }

                    res=0;

                    if ((*error)==packosErrorQueueFull)
                      {
                        *error=packosErrorNone;
                        continue;
                      }

                    UtilPrintfStream(errStream,error,"UdpSocketReceive(): PackosPacketQueueEnqueue(): %s\n",
                            PackosErrorToString(*error));
                    return 0;
                  }
              }
            res=0;
          }
      }
    }

  return res;
}

bool UdpSocketReceivePending(UdpSocket socket,
                             PackosError* error)
{
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return false;
    }

  return PackosPacketQueueNonEmpty(socket->queue,error);
}
