#include <util/stream.h>
#include <util/stream-stdio.h>
#include <util/alloc.h>
#include <packos/packet.h>
#include <packos/sys/contextP.h>
#include <packos/interrupts.h>
#include <contextQueue.h>
#include <packos/idle.h>

#include <udp.h>
#include <iface-native.h>

#include "processes.h"

void scheduler(void)
{
  PackosError error;
  PackosContextQueue contexts=PackosContextQueueNew(&error);
  UdpSocket tickSocket;
  UdpSocket childSocket;
  IpIface iface;
  PackosAddress myAddr;
  PackosContext pinger;

  const uint16_t clockPort=7000;
  PackosInterruptId clockId
    =PackosInterruptAliasLookup(PACKOS_INTERRUPT_ALIAS_CLOCK,&error);

  if (StreamStdioInitStreams(&error)<0) return;

  if (PackosInterruptRegisterFor(clockId,clockPort,&error)<0)
    {
      return;
    }

  iface=IpIfaceNativeNew(&error);
  if (!iface)
    {
      UtilPrintfStream(errStream,&error,"IpIfaceNativeNew: %s\n",PackosErrorToString(error));
      return;
    }

  if (IpIfaceRegister(iface,&error)<0)
    {
      UtilPrintfStream(errStream,&error,"IpIfaceRegister: %s\n",PackosErrorToString(error));
      return;
    }

  tickSocket=UdpSocketNew(&error);
  if (!tickSocket)
    {
      UtilPrintfStream(errStream,&error,"UdpSocketNew: %s\n",PackosErrorToString(error));
      return;
    }

  myAddr=PackosMyAddress(&error);
  if (error!=packosErrorNone)
    {
      UtilPrintfStream(errStream,&error,"PackosMyAddress(): %s\n",PackosErrorToString(error));
      return;
    }

  if (UdpSocketBind(tickSocket,myAddr,7000,&error)<0)
    {
      UtilPrintfStream(errStream,&error,"UdpSocketBind(7000): %s\n",PackosErrorToString(error));
      return;
    }

  childSocket=UdpSocketNew(&error);
  if (!childSocket)
    {
      UtilPrintfStream(errStream,&error,"UdpSocketNew: %s\n",PackosErrorToString(error));
      return;
    }

  if (UdpSocketBind(childSocket,myAddr,8000,&error)<0)
    {
      UtilPrintfStream(errStream,&error,"UdpSocketBind(8000): %s\n",PackosErrorToString(error));
      return;
    }

  pinger=PackosContextNew(pingerProcess,"pinger",&error);
  if (!pinger)
    {
      UtilPrintfStream(errStream,&error,"PackosContextNew\n");
      return;
    }

  PackosContextQueueAppend(contexts,pinger,&error);

  while (!(PackosContextQueueEmpty(contexts,&error)))
    {
      PackosPacket* packet;
      PackosContext cur=PackosContextQueueHead(contexts,&error);
      if (PackosContextBlocking(cur,&error))
        {
          UtilPrintfStream(errStream,&error,"scheduler: %s is blocking\n",
                  PackosContextGetName(cur,&error));
        }

      while (PackosContextBlocking(cur,&error))
        {
          PackosContext next;
          PackosContextQueueRemove(contexts,cur,&error);
          PackosContextQueueAppend(contexts,cur,&error);
          next=PackosContextQueueHead(contexts,&error);
          if (next==cur)
            {
              UtilPrintfStream(errStream,&error,"scheduler: all contexts are blocking\n");
              return;
            }
          cur=next;
        }

      if (PackosContextFinished(cur,&error))
        {
          UtilPrintfStream(errStream,&error,"scheduler: there's a finished process in the queue\n");
          return;
        }

      UtilPrintfStream(errStream,&error,"scheduler: give %p/%s a slot\n",
              cur,PackosContextGetName(cur,&error));

      IpIfaceNativeSetNextYieldTo(iface,cur,&error);
      packet=UdpSocketReceive(tickSocket,0,true,&error);
      if (!packet)
        {
          switch (error)
            {
            case packosErrorStoppedForOtherSocket:
            case packosErrorContextFinished:
            case packosErrorYieldedToBlocked:
              break;

            default:
              UtilPrintfStream(errStream,&error,"UdpSocketReceive(tickSocket): %s\n",
                      PackosErrorToString(error));
              return;
            }
        }

      if (packet)
        {
          byte* payload=packet->ipv6.dataAndHeaders+sizeof(IpHeaderUDP);
          PackosInterruptId* idP=(PackosInterruptId*)payload;
          if ( ((*idP)!=clockId)
               || (!(PackosAddrEq(packet->packos.src,
                                  PackosPacketGetKernelAddr(&error)
                                  )
                     )
                   )
               )
            UtilPrintfStream(errStream,&error,"Got an interrupt packet that wasn't a tick\n");
          else
            UtilPrintfStream(errStream,&error,"Got a tick\n");

          PackosPacketFree(packet,&error);
        }

      PackosContextQueueRemove(contexts,cur,&error);
      if (!PackosContextFinished(cur,&error))
        PackosContextQueueAppend(contexts,cur,&error);
    }

  PackosSimContextLoopTerminate(&error);
}
