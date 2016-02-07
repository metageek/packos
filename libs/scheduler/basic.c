#include <util/alloc.h>
#include <util/stream.h>

#include <packos/packet.h>
#include <packos/sys/contextP.h>
#include <packos/interrupts.h>
#include <contextQueue.h>
#include <packos/arch.h>

#include <udp.h>
#include <iface-native.h>
#include <timer.h>

#include <schedulers/basic.h>

#include "timerServer.h"
#include "controlServer.h"

#define CONTROL

static int incrIfNonDaemon(PackosContext context,
                           void* arg,
                           PackosError* error)
{
  SchedulerBasicContextMetadata* metadata
    =PackosContextGetMetadata(context,error);
  if (!metadata)
    {
      PackosError tmp;
      const char* name=PackosContextGetName(context,&tmp);
      if (!name)
        name="<unnamed>";
      if ((*error)==packosErrorNone)
        {
          UtilPrintfStream(errStream,&tmp,
                           "basic:incrIfNonDaemon(): %s has no metadata\n",
                           name);
          *error=packosErrorInvalidArg;
        }
      else
        {
          UtilPrintfStream(errStream,&tmp,
                           "basic:incrIfNonDaemon(): PackosContextGetMetadata(%s): %s\n",
                           name,
                           PackosErrorToString(*error)
                           );
        }
      return -1;
    }

  if (!(metadata->isDaemon))
    {
      int* numNonDaemonsP=(int*)arg;
      (*numNonDaemonsP)++;
    }
  return 0;
}

void SchedulerBasic(void)
{
  SchedulerBasicContextMetadata idleMetadata;
  PackosError error;
  PackosContextQueue contexts;
  int numNonDaemons=0;
  UdpSocket tickSocket;
  PackosInterruptId clockId;
  IpIface iface;
  PackosAddress myAddr;
  TimerServer timerServer;
  SchedulerControlServer controlServer;

  const uint16_t clockPort=7000;

  if (UtilArenaInit(&error)<0)
    return;

  if (StreamInit(&error)<0)
    return;

  contexts=PackosContextQueueNew(&error);

  clockId
    =PackosInterruptAliasLookup(PACKOS_INTERRUPT_ALIAS_CLOCK,&error);

  if (PackosInterruptRegisterFor(clockId,clockPort,&error)<0)
    {
      UtilPrintfStream(errStream,&error,"PackosInterruptRegisterFor: %s\n",
              PackosErrorToString(error));
      return;
    }

  iface=IpIfaceNativeNew(&error);
  if (!iface)
    {
      UtilPrintfStream(errStream,&error,"IpIfaceNativeNew: %s\n",PackosErrorToString(error));
      return;
    }

  if (IpIfaceNativeSetYieldToRecipient(iface,false,&error)<0)
    {
      UtilPrintfStream(errStream,&error,"IpIfaceNativeSetYieldToRecipient(): %s\n",
              PackosErrorToString(error));
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

  UtilPrintfStream(errStream,&error,
                   "Bound tickSocket to %d\n",
                   UdpSocketGetLocalPort(tickSocket,&error));

  timerServer=TimerServerInit(&error);
  if (!timerServer)
    {
      UtilPrintfStream(errStream,&error,"TimerServerInit(): %s\n",PackosErrorToString(error));
      return;
    }

  if (SchedulerCallbackCreateProcesses(contexts,&error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "SchedulerCallbackCreateProcesses(): %s\n",
                       PackosErrorToString(error));
      return;
    }

  {
    PackosContext idle=PackosContextNew(SchedulerIdle,"idle",&error);
    if (!idle)
      {
        UtilPrintfStream(errStream,&error,"PackosContextNew(idle): %s\n",
                PackosErrorToString(error));
        return;
      }

    idleMetadata.isDaemon=true;
    idleMetadata.isRunning=true;
    idleMetadata.dependsOn=0;

    if (PackosContextSetMetadata(idle,&idleMetadata,&error)<0)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&tmp,
                         "PackosContextSetMetadata(idle): %s\n",
                         PackosErrorToString(error));
        return;
      }

    PackosContextQueueAppend(contexts,idle,&error);
    PackosKernelSetIdleContext(idle,&error);
  }

#ifdef CONTROL
  controlServer=SchedulerControlServerInit(contexts,&error);
  if (!controlServer)
    {
      UtilPrintfStream(errStream,&error,
                       "SchedulerControlServerInit(): %s\n",
                       PackosErrorToString(error));
      return;
    }
#endif

  if (PackosContextQueueForEach(contexts,
                                incrIfNonDaemon,
                                &numNonDaemons,
                                &error)
      <0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "PackosContextQueueForEach(): %s\n",
                       PackosErrorToString(error));
      return;
    }

  UtilPrintfStream(errStream,&error,
                   "There are %d non-daemon processes\n",
                   numNonDaemons);

  while ((PackosContextQueueLen(contexts,&error)>1)
         && (numNonDaemons>0)
         )
    {
      PackosPacket* packet;
      PackosContext cur=PackosContextQueueHead(contexts,&error);
      SchedulerBasicContextMetadata* curMetadata
        =(SchedulerBasicContextMetadata*)
        (PackosContextGetMetadata(cur,&error));

      while ((!(curMetadata->isRunning))
             || (PackosContextBlocking(cur,&error))
             )
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
          curMetadata
            =(SchedulerBasicContextMetadata*)
            (PackosContextGetMetadata(cur,&error));
        }

      if (PackosContextFinished(cur,&error))
        {
          const char* name=PackosContextGetName(cur,&error);
          UtilPrintfStream(errStream,&error,
                           "scheduler: there's a finished process (%p:%s) in the queue\n",
                           cur,name);
          PackosContextQueueRemove(contexts,cur,&error);
          {
            SchedulerBasicContextMetadata* metadata
              =PackosContextGetMetadata(cur,&error);
            if (metadata && !(metadata->isDaemon))
              numNonDaemons--;
          }
          continue;
        }

      /*UtilPrintfStream(errStream,&error,"scheduler: give %p/%s a slot\n",
        cur,PackosContextGetName(cur,&error));*/

      IpIfaceNativeSetNextYieldTo(iface,cur,&error);
      packet=UdpSocketReceive(tickSocket,0,true,&error);
      if (!packet)
        {
          switch (error)
            {
            case packosErrorStoppedForOtherSocket:
              break;

            case packosErrorContextFinished:
            case packosErrorYieldedToBlocked:
            case packosErrorContextYieldedBack:
            case packosErrorPacketFilteredOut:
              break;

            default:
              UtilPrintfStream(errStream,&error,
                               "UdpSocketReceive(tickSocket): %s\n",
                               PackosErrorToString(error));
              return;
            }
        }

      if (packet)
        {
          byte* payload=packet->ipv6.dataAndHeaders+sizeof(IpHeaderUDP);
          PackosInterruptId* idP=(PackosInterruptId*)payload;
          PackosInterruptId id=ntohs(*idP);
          if ( (id!=clockId)
               || (!(PackosAddrEq(packet->packos.src,
                                  PackosPacketGetKernelAddr(&error)
                                  )
                     )
                   )
               )
            {

              /* Found the problem: the TCP stack assumes it's running
               *  in a non-scheduler process, and calls into the timer
               *  code.  This means we're sending UDP from the scheduler
               *  to itself; apparently that isn't working right.
               */

              IpHeaderUDP* udp=(IpHeaderUDP*)packet->ipv6.dataAndHeaders;
              char buffFrom[80],buffTo[80];
              const char* msgFrom;
              const char* msgTo;
              if (PackosAddrToString(packet->packos.src,
                                     buffFrom,sizeof(buffFrom),
                                     &error
                                     )
                  <0
                  )
                msgFrom=PackosErrorToString(error);
              else
                msgFrom=buffFrom;
              if (PackosAddrToString(packet->packos.src,
                                     buffTo,sizeof(buffTo),
                                     &error
                                     )
                  <0
                  )
                msgTo=PackosErrorToString(error);
              else
                msgTo=buffTo;
              UtilPrintfStream(errStream,&error,
                               "Got an interrupt packet that wasn't a tick: id %x from %s:%d to %s:%d\n",
                               (int)id,
                               msgFrom,udp->sourcePort,
                               msgTo,udp->destPort);
            }
          else
            {
              /*UtilPrintfStream(errStream,&error,"Got a tick\n");*/
              if (TimerServerTick(timerServer,&error)<0)
                UtilPrintfStream(errStream,&error,"TimerServerTick(): %s\n",
                        PackosErrorToString(error));
              {
                static int i=0;
                i++;
                i%=10;
                if (i==0)
                  {
                    TcpPoll(&error);
#ifdef CONTROL
                    if (SchedulerControlServerPoll(controlServer,&error)<0)
                      {
                        UtilPrintfStream(errStream,&error,
                                         "SchedulerControlServerPoll(): %s\n",
                                         PackosErrorToString(error));
                        return;
                      }
#endif
                  }
              }
            }

          PackosPacketFree(packet,&error);
        }

      PackosContextQueueRemove(contexts,cur,&error);
      if (!PackosContextFinished(cur,&error))
        PackosContextQueueAppend(contexts,cur,&error);
      else
        {
          const char* name=PackosContextGetName(cur,&error);
          if (!name)
            name=PackosErrorToString(error);
          UtilPrintfStream(errStream,&error,"scheduler: context %s exited\n",
                           name);
          {
            SchedulerBasicContextMetadata* metadata
              =PackosContextGetMetadata(cur,&error);
            if (metadata && !(metadata->isDaemon))
              numNonDaemons--;
          }
        }
    }
}
