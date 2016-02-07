#include <util/alloc.h>
#include <util/stream.h>
#include <util/string.h>
#include <packos/packet.h>
#include <packos/sys/contextP.h>
#include <packos/interrupts.h>
#include <packos/arch.h>
#include <contextQueue.h>

#include <udp.h>
#include <iface-native.h>

#include "processes.h"
#include "../../kernel/kprintfK.h"

void scheduler(void)
{
  PackosError error;
  PackosContextQueue contexts;
  UdpSocket tickSocket;
  UdpSocket childSocket;
  IpIface iface;
  PackosAddress myAddr;

  extern void pciDriverProcess(void);

  const uint16_t clockPort=7000;
  PackosInterruptId clockId
    =PackosInterruptAliasLookup(PACKOS_INTERRUPT_ALIAS_CLOCK,&error);

  if (UtilArenaInit(&error)<0)
    {
      kprintf("UtilArenaInit(): %s\n",PackosErrorToString(error));
      return;
    }

  if (StreamInit(&error)<0)
    {
      kprintf("StreamInit(): %s\n",PackosErrorToString(error));
      return;
    }

  UtilPrintfStream(errStream,&error,"simple scheduler\n");

  contexts=PackosContextQueueNew(&error);
  if (!contexts)
    {
      UtilPrintfStream(errStream,&error,"PackosContextQueueNew: %s\n",
                       PackosErrorToString(error));
      return;
    }

  if (PackosInterruptRegisterFor(clockId,clockPort,&error)<0)
    {
      UtilPrintfStream(errStream,&error,
                       "PackosInterruptRegisterFor(): %s\n",
                       PackosErrorToString(error));
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

#if 0
  {
    char* msgs[]={"foo","bar","baz",0};
    unsigned int i;
    for (i=0; msgs[i]; i++)
      {
        const char* msg=msgs[i];
        PackosContext c=PackosContextNew(f,msg,&error);
        if (!c)
          {
            UtilPrintfStream(errStream,&error,"PackosContextNew\n");
            return;
          }

        if (PackosContextQueueAppend(contexts,c,&error)<0)
          {
            UtilPrintfStream(errStream,&error,
                             "PackosContextQueueAppend(%p/%s): %s\n",
                             c,msg,
                             PackosErrorToString(error)
                             );
            return;
          }

        {
          PackosPacket* packet=PackosPacketAlloc(0,&error);
          packet->ipv6.src=PackosMyAddress(&error);
          packet->packos.dest=packet->ipv6.dest=PackosContextGetAddr(c,&error);
          packet->ipv6.payloadLength=UtilStrlen(msg)+1;
          UtilStrcpy((char*)packet->ipv6.dataAndHeaders,msg);
          if (PackosPacketSend(packet,false,&error)<0)
            {
              UtilPrintfStream(errStream,&error,"PackosPacketSend\n");
              PackosPacketFree(packet,&error);
            }
        }
      }
  }
#else
 {
   PackosContext c=PackosContextNew(pciDriverProcess,"pci",&error);
   if (!c)
     {
       UtilPrintfStream(errStream,&error,"PackosContextNew\n");
       return;
     }

   if (PackosContextQueueAppend(contexts,c,&error)<0)
     {
       UtilPrintfStream(errStream,&error,
                        "PackosContextQueueAppend(%p/%s): %s\n",
                        c,scheduler,
                        PackosErrorToString(error)
                        );
       return;
     }
 }
#endif

  while (!(PackosContextQueueEmpty(contexts,&error)))
    {
      PackosPacket* packet;
      PackosContext cur,first;

      first=cur=PackosContextQueueHead(contexts,&error);
      /*UtilPrintfStream(errStream,&error,"scheduler: cur==%p\n",cur);*/

      while (PackosContextBlocking(cur,&error))
        {
          PackosContext next;
          PackosContextQueueRemove(contexts,cur,&error);
          PackosContextQueueAppend(contexts,cur,&error);
          next=PackosContextQueueHead(contexts,&error);
          if (next==first)
            {
              UtilPrintfStream(errStream,&error,
                               "scheduler: all contexts are blocking\n");
              return;
            }
          /*UtilPrintfStream(errStream,&error,"make it %p\n",next);*/
          cur=next;
        }

      if (PackosContextFinished(cur,&error))
        {
          UtilPrintfStream(errStream,&error,"scheduler: there's a finished process in the queue\n");
          return;
        }

      /*UtilPrintfStream(errStream,&error,"scheduler: give %p a slot\n",cur);*/

      IpIfaceNativeSetNextYieldTo(iface,cur,&error);
      packet=UdpSocketReceive(tickSocket,0,true,&error);
      if (!packet)
        {
          switch (error)
            {
            case packosErrorStoppedForOtherSocket:
            case packosErrorContextFinished:
            case packosErrorContextYieldedBack:
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
          short* payload
            =(short*)(packet->ipv6.dataAndHeaders+sizeof(IpHeaderUDP));
          PackosInterruptId id=(PackosInterruptId)(ntohs(*payload));
          if ( (id!=clockId)
               || (!(PackosAddrEq(packet->packos.src,
                                  PackosPacketGetKernelAddr(&error)
                                  )
                     )
                   )
               )
            UtilPrintfStream(errStream,&error,"Got a non-tick interrupt: id %d\n",
                             (int)id
                             );
          /*else
            UtilPrintfStream(errStream,&error,"Got a tick\n");*/

          PackosPacketFree(packet,&error);
        }

      PackosContextQueueRemove(contexts,cur,&error);
      if (!PackosContextFinished(cur,&error))
        PackosContextQueueAppend(contexts,cur,&error);

      if (UdpSocketReceivePending(childSocket,&error))
        {
          packet=UdpSocketReceive(childSocket,0,true,&error);
          if (!packet)
            {
              UtilPrintfStream(errStream,&error,"UdpSocketReceive(childSocket): %s\n",
                      PackosErrorToString(error));
            }
          else
            {
              UtilPrintfStream(errStream,&error,"received: \"%s\"\n",
                      (char*)(packet->ipv6.dataAndHeaders)
                      +8
                      );
              PackosPacketFree(packet,&error);
            }
        }
      else
        {
          if (error!=packosErrorNone)
            UtilPrintfStream(errStream,&error,"UdpSocketReceivePending: %s\n",
                    PackosErrorToString(error));
        }
    }
}
