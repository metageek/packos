#include <packos/arch.h>

#include <util/stream.h>
#include <util/alloc.h>

#include <udp.h>
#include <timer-protocol.h>
#include "timerServer.h"

struct TimerServer {
  struct {
    TimerClient first;
    TimerClient last;
  } clients;

  UdpSocket sock;
  PackosAddress myAddr;
};

struct TimerClient {
  TimerClient next;
  TimerClient prev;

  struct {
    PackosAddress addr;
    uint16_t port;
  } remote;

  uint32_t id;
  uint16_t requestId;
  bool repeat;

  struct {
    uint32_t sec,usec;
  } initial;

  uint32_t deltaTicks;
};

static uint32_t timeToTicks(uint32_t sec,
                            uint32_t usec)
{
  return sec*10+((usec+49999)/100000);
}

static int insertClient(TimerServer server,
                        TimerClient client,
                        PackosError* error)
{
  TimerClient after;

  if (!error) return -2;
  if (!(server && client))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  for (after=server->clients.first;
       after && (after->deltaTicks<=client->deltaTicks);
       after=after->next
       )
    client->deltaTicks-=after->deltaTicks;

  if (after)
    after=after->prev;
  else
    after=server->clients.last;

  if (after)
    {
      client->next=after->next;
      if (client->next)
        after->next->prev=client;
      else
        server->clients.last=client;

      client->prev=after;
      after->next=client;
    }
  else
    {
      client->next=server->clients.first;
      if (client->next)
        client->next->prev=client;
      else
        server->clients.last=client;
      server->clients.first=client;
    }

  return 0;
}

static int removeClient(TimerServer server,
                        TimerClient client,
                        PackosError* error)
{
  if (!error) return -2;
  if (!(server && client))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (client->prev)
    client->prev->next=client->next;
  else
    server->clients.first=client->next;

  if (client->next)
    client->next->prev=client->prev;
  else
    server->clients.last=client->prev;

  return 0;
}

static int moveClient(TimerServer server,
                      TimerClient client,
                      PackosError* error)
{
  if (removeClient(server,client,error)<0) return -1;
  if (insertClient(server,client,error)<0) return -1;
  return 0;
}

static TimerClient newClient(TimerServer server,
                             PackosAddress addr,
                             uint16_t port,
                             uint32_t sec,
                             uint32_t usec,
                             uint32_t id, /* included in the tick packets */
                             bool repeat,
                             PackosError* error)
{
  TimerClient res;
  uint32_t numTicks;

  if (!error) return 0;
  if (!server)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  numTicks=timeToTicks(sec,usec);
  if (!numTicks)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

#if 0
  {
    PackosError tmp;
    char buff[80];
    const char* msg=buff;
    if (PackosAddrToString(addr,buff,sizeof(buff),&tmp)<0)
      msg=PackosErrorToString(tmp);
    UtilPrintfStream(errStream,error,"newClient(%s:%hu,%u,%u,%u)\n",
            msg,port,sec,usec,id);
  }
#endif

  res=(TimerClient)(malloc(sizeof(struct TimerClient)));
  if (!res)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  res->remote.addr=addr;
  res->remote.port=port;
  res->id=id;
  res->repeat=repeat;
  res->initial.sec=sec;
  res->initial.usec=usec;
  res->deltaTicks=numTicks;

  res->next=res->prev=0;
  insertClient(server,res,error);
  return res;
}

static int freeClient(TimerServer server,
                      TimerClient client,
                      PackosError* error)
{
  if (!error) return -2;
  if (!(server && client))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (removeClient(server,client,error)<0) return -1;
  free(client);
  return 0;
}

TimerServer TimerServerInit(PackosError* error)
{
  TimerServer res;
  PackosAddress myAddr;
  if (!error) return 0;

  myAddr=PackosMyAddress(error);
  if ((*error)!=packosErrorNone)
    {
      UtilPrintfStream(errStream,error,"TimerServerInit(): PackosMyAddress(): %s\n",
              PackosErrorToString(*error));
      return 0;
    }

  res=(TimerServer)(malloc(sizeof(struct TimerServer)));
  if (!res)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  res->myAddr=myAddr;

  res->sock=UdpSocketNew(error);
  if (!(res->sock))
    {
      UtilPrintfStream(errStream,error,"TimerServerInit(): UdpSocketNew(): %s\n",
              PackosErrorToString(*error));
      free(res);
      return 0;
    }

  if (UdpSocketBind(res->sock,
                    PackosAddrGetZero(),
                    TIMER_FIXED_UDP_PORT,
                    error)
      <0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,"TimerServerInit(): UdpSocketBind(): %s\n",
              PackosErrorToString(*error));
      UdpSocketClose(res->sock,&tmp);
      free(res);
      return 0;
    }

  UtilPrintfStream(errStream,error,
                   "Bound timer socket to %d\n",
                   UdpSocketGetLocalPort(res->sock,error));

  res->clients.first=res->clients.last=0;
  return res;
}

int TimerServerClose(TimerServer server,
                     PackosError* error)
{
  if (!error) return -2;
  if (!server)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  while (server->clients.first)
    {
      if (freeClient(server,server->clients.first,error)<0)
        {
          UtilPrintfStream(errStream,error,"TimerServerClose(): freeClient(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }
    }

  if (UdpSocketClose(server->sock,error)<0)
    {
      UtilPrintfStream(errStream,error,"TimerServerClose(): UdpSocketClose(): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  free(server);
  return 0;
}

static void sendError(TimerServer server,
                      PackosAddress addr,
                      uint16_t port,
                      TimerRequest* request,
                      PackosError errorToSend)
{
  IpHeaderUDP* udpHeader;
  PackosError error;
  TimerReply* reply;

  PackosPacket* packet
    =UdpPacketNew(server->sock,sizeof(TimerReply),&udpHeader,&error);
  if (!packet)
    {
      UtilPrintfStream(errStream,&error,"sendError(): UdpPacketNew(): %s\n",
              PackosErrorToString(error));
      return;
    }

  packet->packos.dest=packet->ipv6.dest=addr;
  packet->ipv6.src=server->myAddr;
  udpHeader->destPort=port;
  reply=(TimerReply*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));
  reply->id=request->id;
  reply->requestId=request->requestId;
  reply->cmd=request->cmd;
  reply->u.open.errorAsInt=(uint32_t)errorToSend;

  if (UdpSocketSend(server->sock,packet,&error)<0)
    {
      UtilPrintfStream(errStream,&error,"sendError(): UdpSocketSend(): %s\n",
              PackosErrorToString(error));
      PackosPacketFree(packet,&error);
    }
}

static int processRequest(TimerServer server,
                          PackosAddress addr,
                          uint16_t port,
                          TimerRequest* request,
                          PackosError* error)
{
  if (!error) return -2;
  if (!(server && request))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (request->version!=TIMER_PROTOCOL_VERSION)
    {
      sendError(server,addr,port,request,packosErrorWrongProtocolVersion);
      return -1;
    }

  switch ((TimerRequestCmd)(request->cmd))
    {
    case timerRequestCmdOpen:
      {
        TimerClient client=newClient(server,addr,port,
                                     request->args.open.sec,
                                     request->args.open.usec,
                                     request->id,
                                     request->args.open.repeat,
                                     error);
        if (!client)
          {
            UtilPrintfStream(errStream,error,"processRequest(): newClient(): %s\n",
                    PackosErrorToString(*error));
            sendError(server,addr,port,request,*error);
            return -1;
          }
        return 0;
      }
      break;

    case timerRequestCmdClose:
      break;

    case timerRequestCmdInvalid:
    case timerRequestCmdTick:
      /*default:*/
      sendError(server,addr,port,request,packosErrorBadProtocolCmd);
      return 0;
    }

  return 0;
}

static int sendOneTick(TimerServer server,
                       TimerClient client,
                       PackosError* error)
{
  IpHeaderUDP* udpHeader;
  TimerReply* reply;
  PackosPacket* packet;

  if (false) {
    char buff[80];
    const char* msg=buff;
    PackosError tmp;
    if (PackosAddrToString(client->remote.addr,buff,sizeof(buff),&tmp)<0)
      msg=PackosErrorToString(tmp);
    UtilPrintfStream(errStream,error,"sendOneTick(%s)\n",msg);
  }

  packet=UdpPacketNew(server->sock,sizeof(TimerReply),&udpHeader,error);
  if (!packet)
    {
      UtilPrintfStream(errStream,error,"sendOneTick(): UdpPacketNew(): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  packet->packos.dest=packet->ipv6.dest=client->remote.addr;
  packet->ipv6.src=server->myAddr;
  udpHeader->destPort=client->remote.port;
  reply=(TimerReply*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));
  reply->id=client->id;
  reply->requestId=client->requestId;
  reply->cmd=timerRequestCmdTick;

  if (UdpSocketSend(server->sock,packet,error)<0)
    {
      PackosError tmp;
      PackosPacketFree(packet,&tmp);
      UtilPrintfStream(errStream,&tmp,"sendOneTick(): 2UdpSocketSend(): %s\n",
                       PackosErrorToString(*error));
      PackosPacketFree(packet,&tmp);

      if ((*error)==packosErrorAddressUnreachable)
        {
          PackosError tmp;
          if (freeClient(server,client,&tmp)<0)
            {
              PackosError tmp2;
              UtilPrintfStream(errStream,&tmp2,
                               "sendOneTick(): freeClient(): %s\n",
                               PackosErrorToString(tmp));
            }
        }

      return -1;
    }

  return 0;
}

static int sendTicks(TimerServer server,
                     PackosError* error)
{
  if (!error) return -2;
  if (!server)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  while ((server->clients.first)
         && ((server->clients.first->deltaTicks)==0)
         )
    {
      if (sendOneTick(server,server->clients.first,error)<0)
        {
          PackosError tmp;

          char buff[80];
          const char* addrStr;
          if (PackosAddrToString(server->clients.first->remote.addr,
                                 buff,sizeof(buff),error)<0)
            addrStr=PackosErrorToString(*error);
          else
            addrStr=buff;

          UtilPrintfStream(errStream,&tmp,
                           "sendTicks(): sendOneTick(%s): %s\n",
                           addrStr,
                           PackosErrorToString(*error));

          freeClient(server,server->clients.first,&tmp);
          return -1;
        }

      if (server->clients.first->repeat)
        {
          server->clients.first->deltaTicks
            =timeToTicks(server->clients.first->initial.sec,
                         server->clients.first->initial.usec);

          if (moveClient(server,server->clients.first,error)<0)
            {
              UtilPrintfStream(errStream,error,"sendTicks(): moveClient(): %s\n",
                      PackosErrorToString(*error));
              return -1;
            }
        }
      else
        {
          if (freeClient(server,server->clients.first,error)<0)
            {
              UtilPrintfStream(errStream,error,"sendTicks(): freeClient(): %s\n",
                      PackosErrorToString(*error));
              return -1;
            }
        }
    }

  return 0;
}

int TimerServerTick(TimerServer server,
                    PackosError* error)
{
  PackosPacket* packet;
  if (!error) return -2;
  if (!server)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (server->clients.first)
    {
      if (server->clients.first->deltaTicks)
        server->clients.first->deltaTicks--;

      if (server->clients.first->deltaTicks==0)
        {
          if (sendTicks(server,error)<0)
            {
              /*UtilPrintfStream(errStream,error,"TimerServerTick(): sendTicks(): %s\n",
                PackosErrorToString(*error));*/
              server->clients.first->deltaTicks++;
            }
        }
    }

  if (!(UdpSocketReceivePending(server->sock,error)))
    {
      if ((*error)==packosErrorNone)
        return 0;
      UtilPrintfStream(errStream,error,
              "TimerServerTick(): UdpSocketReceivePending(): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  packet=UdpSocketReceive(server->sock,0,true,error);
  if (!packet)
    {
      if ((*error)==packosErrorStoppedForOtherSocket)
        (*error)=packosErrorNone;

      if ((*error)==packosErrorNone)
        return  0;
    }

  {
    IpHeaderUDP* udp;
    TimerRequest* request=(TimerRequest*)(IpPacketData(packet,error));
    if (!request)
      {
        UtilPrintfStream(errStream,error,"TimerServerTick(): IpPacketData(): %s\n",
                PackosErrorToString(*error));
        return -1;
      }

    udp=UdpPacketSeekHeader(packet,error);
    if (!udp)
      {
        UtilPrintfStream(errStream,error,
                "TimerServerTick(): UdpPacketSeekHeader(): %s\n",
                PackosErrorToString(*error));
        return -1;
      }

    {
      PackosError tmp;
      int res=processRequest(server,
                             packet->ipv6.src,
                             udp->sourcePort,
                             request,
                             error);
      PackosPacketFree(packet,&tmp);
      return res;
    }
  }
}
