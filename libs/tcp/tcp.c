#include <util/alloc.h>
#include <util/stream.h>
#include <util/string.h>

#include <tcp.h>
#include <udp.h>
#include <icmp.h>
#include <ip-filter.h>
#include <iface.h>
#include <timer.h>
#include <packos/context.h>
#include <packos/checksums.h>
#include <packos/arch.h>

/*#define TCP_DEBUG*/

struct TcpIfaceContext {
  Timer timer;
  UdpSocket timerSocket;

  TcpSocket first;
  TcpSocket last;

  IpFilter filter;
};

#define QUEUE_SIZE 16384

typedef struct {
  byte data[QUEUE_SIZE];
  uint32_t start,len,latestSequenceNumber,latestAckNumber;
} ByteQueue;

struct TcpSocket {
  TcpSocketState state;
  PackosError errorThatClosed;

  IpIface iface;
  uint16_t localPort;

  PackosAddress remoteAddr;
  uint16_t remotePort;

  ByteQueue in,out;
  uint32_t remoteWindow;

  TcpSocket waitingToAccept;
  TcpSocket acceptedFrom;

  TcpSocket next;
  TcpSocket prev;

  struct {
    int ticksLeft,initTicks;
  } timing;
};

static IpFilterAction TcpFilterMethod(IpIface iface,
                                      void* context,
                                      PackosPacket* packet,
                                      PackosError* error);

static int ByteQueueEnqueue(ByteQueue* queue,
                            const void* data,
                            uint32_t nbytes,
                            PackosError* error);
static int ByteQueueDequeue(ByteQueue* queue,
                            void* data,
                            uint32_t nbytes,
                            PackosError* error);
static int ByteQueueRead(ByteQueue* queue,
                         void* data,
                         uint32_t nbytes,
                         PackosError* error);
static int ByteQueueDrop(ByteQueue* queue,
                         uint32_t nbytes,
                         PackosError* error);

static int checkPackets(IpIface iface,
                        PackosError* error);
static PackosPacket* newPacket(TcpSocket socket,
                               uint32_t datalen,
                               uint32_t* actualDatalen,
                               IpHeaderTCP** tcpHeader,
                               PackosError* error);
static int fire(TcpSocket socket,
                PackosError* error);

static bool modLt(uint32_t a, uint32_t b);
#if 0
static bool modLe(uint32_t a, uint32_t b);
static bool modGt(uint32_t a, uint32_t b);
#endif
static bool modGe(uint32_t a, uint32_t b);

static int TcpInitIface(IpIface iface,
                        PackosError* error)
{
  if (!error) return -2;
  if (!iface)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (iface->tcpContext) return 1;

#ifdef TCP_DEBUG
  UtilPrintfStream(errStream,error,"%s: TCP init\n",
                   PackosContextGetOwnName(error)
                   );
#endif

  iface->tcpContext=(TcpIfaceContext)(malloc(sizeof(struct TcpIfaceContext)));
  if (!(iface->tcpContext))
    {
      UtilPrintfStream(errStream,error,"TcpInitIface(): out of memory\n");
      *error=packosErrorOutOfMemory;
      return -1;
    }

  if (!(PackosAddrEq(IpIfaceGetAddress(iface,error),
                     PackosSchedulerAddress(error)
                     )
        )
      )
    {
      iface->tcpContext->timerSocket=UdpSocketNew(error);
      if (!(iface->tcpContext->timerSocket))
        {
          UtilPrintfStream(errStream,error,"TcpInitIface(): UdpSocketNew(): %s\n",
                           PackosErrorToString(*error));
          free(iface->tcpContext);
          return -1;
        }

      if (UdpSocketBind(iface->tcpContext->timerSocket,
                        PackosAddrGetZero(),0,
                        error)<0)
        {
          PackosError tmp;
          UtilPrintfStream(errStream,error,"TcpInitIface(): UdpSocketBind(): %s\n",
                           PackosErrorToString(*error));
          UdpSocketClose(iface->tcpContext->timerSocket,&tmp);
          free(iface->tcpContext);
          return -1;
        }

#if 0
      UtilPrintfStream(errStream,error,
                       "Bound tcp ticker socket to %d\n",
                       UdpSocketGetLocalPort(iface->tcpContext->timerSocket,
                                             error
                                             )
                       );
#endif

      iface->tcpContext->timer=TimerNew(iface->tcpContext->timerSocket,
                                        0,100000,
                                        (uint32_t)iface,
                                        true,
                                        error);
      if (!iface->tcpContext->timer)
        {
          PackosError tmp;
          UtilPrintfStream(errStream,error,"TcpInitIface(): TimerNew(): %s\n",
                           PackosErrorToString(*error));
          UdpSocketClose(iface->tcpContext->timerSocket,&tmp);
          free(iface->tcpContext);
          return -1;
        }
    }
  else
    {
      iface->tcpContext->timerSocket=0;
      iface->tcpContext->timer=0;
    }

  iface->tcpContext->first=iface->tcpContext->last=0;
  iface->tcpContext->filter=IpFilterInstall(iface,TcpFilterMethod,0,error);
  if (!(iface->tcpContext->filter))
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,"TcpInitIface(): IpFilterInstall(): %s\n",
              PackosErrorToString(*error));
      TimerClose(iface->tcpContext->timer,&tmp);
      UdpSocketClose(iface->tcpContext->timerSocket,&tmp);
      free(iface->tcpContext);
      return -1;
    }

  return 0;
}

TcpSocket TcpSocketNew(PackosError* error)
{
  TcpSocket res;

  if (!error) return 0;

  res=(TcpSocket)(malloc(sizeof(struct TcpSocket)));
  if (!res)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  res->iface=0;
  res->localPort=0;
  res->remoteAddr=PackosAddrGetZero();
  res->remotePort=0;
  res->remoteWindow=0;
  res->waitingToAccept=0;
  res->acceptedFrom=0;

  res->in.start=res->in.len=0;
  res->in.latestSequenceNumber=res->in.latestAckNumber=0;
  res->out.start=res->out.len=0;
  res->out.latestSequenceNumber=res->out.latestAckNumber=0;

  res->next=res->prev=0;
  res->state=tcpSocketStateClosed;
  res->errorThatClosed=packosErrorNone;
  res->timing.initTicks=3;
  res->timing.ticksLeft=res->timing.initTicks;
  return res;
}

static int TcpSocketDelete(TcpSocket socket,
                           PackosError* error)
{
  if (!error) return -2;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

#ifdef TCP_DEBUG
  UtilPrintfStream(errStream,error,"TcpSocketDelete(%hu:%hu)\n",
          socket->localPort,socket->remotePort);
#endif

  if (socket->iface)
    {
      if (socket->next)
        socket->next->prev=socket->prev;
      else
        socket->iface->tcpContext->last=socket->prev;

      if (socket->prev)
        socket->prev->next=socket->next;
      else
        socket->iface->tcpContext->first=socket->next;
      socket->next=socket->prev=0;
    }

  free(socket);
  return 0;
}

int TcpSocketClose(TcpSocket socket,
                   PackosError* error)
{
  if (!error) return -2;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

#ifdef TCP_DEBUG
  UtilPrintfStream(errStream,error,"TcpSocketClose(%hu:%hu)\n",
                   socket->localPort,socket->remotePort);
#endif

  switch (socket->state)
    {
    case tcpSocketStateInvalid:
      *error=packosErrorInvalidArg;
      return -1;

    case tcpSocketStateListen:
      return TcpSocketDelete(socket,error);

    case tcpSocketStateSynSent:
    case tcpSocketStateSynReceived:
    case tcpSocketStateEstablished:
      socket->state=tcpSocketStateFinWait1;
    case tcpSocketStateFinWait1:
    case tcpSocketStateFinWait2:
      return fire(socket,error);

    case tcpSocketStateCloseWait:
      if (fire(socket,error)<0)
        UtilPrintfStream(errStream,error,"TcpSocketClose(): fire(): %s\n",
                PackosErrorToString(*error));
      socket->state=tcpSocketStateLastAck;
      return 0;

    case tcpSocketStateClosing:
    case tcpSocketStateLastAck:
    case tcpSocketStateTimeWait:
      return 0;

    case tcpSocketStateClosed:
      return TcpSocketDelete(socket,error);
    }

  return 0;
}

TcpSocketState TcpSocketGetState(TcpSocket socket,
                                 PackosError* error)
{
  if (!error) return tcpSocketStateInvalid;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return tcpSocketStateInvalid;
    }

  return socket->state;
}

static TcpSocket seek(IpIface iface,
		      uint16_t localPort,
                      int remotePort,
		      PackosError* error)
{
  TcpSocket cur;

  if (!localPort)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  for (cur=iface->tcpContext->first; cur; cur=cur->next)
    {
      if ((cur->localPort==localPort)
          && (   (remotePort==-1)
                 || (cur->remotePort==remotePort)
                 || (cur->state==tcpSocketStateListen)
                 )
          )
        return cur;
    }

  *error=packosErrorDoesNotExist;
  return 0;
}

static uint16_t seekAnonTcpPort(IpIface iface,
                                PackosError* error)
{
  uint16_t port;
  for (port=iface->anonPortNext.tcp; port<=anonPortMax; port++)
    {
      PackosError tmp;
      if (!seek(iface,port,-1,&tmp)) return port;
    }

  for (port=anonPortMax; port<iface->anonPortNext.tcp; port++)
    {
      PackosError tmp;
      if (!seek(iface,port,-1,&tmp)) return port;
    }

  *error=packosErrorAllAnonPortsBound;
  return 0;
}

int TcpSocketBind(TcpSocket socket,
                  PackosAddress addr,
                  uint16_t port,
                  PackosError* error)
{
  IpIface iface;

  if (!error) return -1;

  if ((!socket)
      || (socket->localPort)
      )
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (PackosAddrIsZero(addr))
    {
      UtilPrintfStream(errStream,error,"TcpSocketBind(): binding to * not yet implemented\n");
      *error=packosErrorNotImplemented;
      return -1;
    }

#ifdef TCP_DEBUG
  UtilPrintfStream(errStream,error,"%s: TcpSocketBind(port %u)\n",
                   PackosContextGetOwnName(error),
                   (unsigned int)port);
#endif

  iface=IpIfaceLookupReceive(addr,error);
  if (!iface)
    {
      PackosError tmp;
      char addrBuff[80];
      const char* addrStr;
      if (PackosAddrToString(addr,addrBuff,sizeof(addrBuff),error)<0)
        addrStr=PackosErrorToString(*error);
      else
        addrStr=addrBuff;

      UtilPrintfStream(errStream,&tmp,
                       "TcpSocketBind(%s:%d): IpIfaceLookupReceive(): %s\n",
                       addrStr,(int)port,
                       PackosErrorToString(*error));
      return -1;
    }

  if (TcpInitIface(iface,error)<0)
    {
      UtilPrintfStream(errStream,error,
                       "TcpSocketBind(): TcpInitIface(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  if (!port)
    {
      port=seekAnonTcpPort(iface,error);
      if (!port)
        {
          UtilPrintfStream(errStream,error,"TcpPacketNew(): seekAnonTcpPort(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }
    }
  else
    {
      TcpSocket other=seek(iface,port,-1,error);
      if (other)
        {
          if (!((other->state==tcpSocketStateListen)
                && (socket->acceptedFrom==other)
                )
              )
            {
              *error=packosErrorPortAlreadyBound;
              return -1;
            }
        }
    }

  socket->iface=iface;
  socket->localPort=port;

  socket->prev=0;
  socket->next=iface->tcpContext->first;
  if (socket->next)
    socket->next->prev=socket;
  else
    iface->tcpContext->last=socket;

  iface->tcpContext->first=socket;

  return 0;
}

int TcpSocketListen(TcpSocket socket,
                    PackosError* error)
{
  if (!error) return -2;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (socket->state!=tcpSocketStateClosed)
    {
      *error=packosErrorSocketAlreadyInUse;
      return -1;
    }

  socket->state=tcpSocketStateListen;
  return 0;
}

TcpSocket TcpSocketAccept(TcpSocket socket,
                          PackosError* error)
{
  if (!error) return 0;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  if (socket->state!=tcpSocketStateListen)
    {
      *error=packosErrorSocketNotListening;
      return 0;
    }

  while (!(socket->waitingToAccept
           && (socket->waitingToAccept->state==tcpSocketStateEstablished)
           )
         )
    {
      if (checkPackets(socket->iface,error)<0)
        {
          UtilPrintfStream(errStream,error,"TcpSocketAccept(): checkPackets(): %s\n",
                  PackosErrorToString(*error));
          return 0;
        }

      if (socket->errorThatClosed!=packosErrorNone)
        {
          (*error)=socket->errorThatClosed;
          UtilPrintfStream(errStream,error,"TcpSocketAccept(): %s\n",
                           PackosErrorToString(*error));
          return 0;
        }

      PackosContextYield();
    }

  {
    TcpSocket res=socket->waitingToAccept;
    socket->waitingToAccept=0;
    return res;
  }
}

bool TcpSocketAcceptPending(TcpSocket socket,
                            PackosError* error)
{
  if (!error) return false;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return false;
    }

  if (socket->state!=tcpSocketStateListen)
    {
      *error=packosErrorSocketNotListening;
      return false;
    }

  *error=packosErrorNone;
  return (socket->waitingToAccept
          && (socket->waitingToAccept->state==tcpSocketStateEstablished)
          );
}

const char* TcpSocketStateToString(TcpSocketState state,
                                   PackosError* error)
{
  *error=packosErrorNone;
  switch (state)
    {
    case tcpSocketStateInvalid: return "Invalid";
    case tcpSocketStateClosed: return "Closed";
    case tcpSocketStateListen: return "Listen";
    case tcpSocketStateSynSent: return "SynSent";
    case tcpSocketStateSynReceived: return "SynReceived";
    case tcpSocketStateEstablished: return "Established";
    case tcpSocketStateFinWait1: return "FinWait1";
    case tcpSocketStateFinWait2: return "FinWait2";
    case tcpSocketStateCloseWait: return "CloseWait";
    case tcpSocketStateClosing: return "Closing";
    case tcpSocketStateLastAck: return "LastAck";
    case tcpSocketStateTimeWait: return "TimeWait";
    default:
      *error=packosErrorInvalidArg;
      return "??";
    }
}

int TcpSocketSend(TcpSocket socket,
                  const void* buff,
                  uint32_t nbytes,
                  PackosError* error)
{
  int actual;

  if (!error) return -2;
  if (!(socket
        && (socket->iface)
        && buff
        )
      )
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if ((socket->state!=tcpSocketStateEstablished)
      && (socket->state!=tcpSocketStateCloseWait)
      )
    {
      UtilPrintfStream(errStream,error,"TcpSocketSend(): state is %s instead of %s or %s\n",
              TcpSocketStateToString(socket->state,error),
              TcpSocketStateToString(tcpSocketStateEstablished,error),
              TcpSocketStateToString(tcpSocketStateCloseWait,error)
              );
      *error=packosErrorConnectionClosed;
      return -1;
    }

  if (!nbytes) return 0;

  actual=ByteQueueEnqueue(&(socket->out),buff,nbytes,error);
  if (actual<0)
    {
      UtilPrintfStream(errStream,error,"TcpSocketSend(): ByteQueueEnqueue(): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  if ((socket->out.len)>=(PACKOS_MTU-40))
    {
      if (fire(socket,error)<0)
        {
          UtilPrintfStream(errStream,error,"TcpSocketSend(): fire(): %s\n",
                  PackosErrorToString(*error));
          *error=packosErrorNone;
        }
    }

#ifdef TCP_DEBUG
  UtilPrintfStream(errStream,error,
                   "<%s>: TcpSocketSend(): sent %d bytes\n",
                   PackosContextGetOwnName(error),
                   actual);
#endif

  return actual;
}

static int fire(TcpSocket socket,
                PackosError* error)
{
  if (!error) return -2;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

#ifdef TCP_DEBUG
  UtilPrintfStream(errStream,error,
          "tcp<%s>::fire(): {in={lsn=%u, lan=%u, len=%u}, out={lsn=%u, lan=%u, len=%u}, %s}\n",
          PackosContextGetOwnName(error),
          socket->in.latestSequenceNumber,socket->in.latestAckNumber,
          socket->in.len,
          socket->out.latestSequenceNumber,socket->out.latestAckNumber,
          socket->out.len,
          TcpSocketStateToString(socket->state,error)
          );
#endif

  if (socket->state==tcpSocketStateTimeWait)
    {
      TcpSocketDelete(socket,error);
      return 1;
    }

  if ((socket->out.len==0)
      && ((socket->in.latestSequenceNumber)==(socket->in.latestAckNumber))
      )
    return 0;

  {
    uint32_t actual,nbytes=socket->out.len;
    IpHeaderTCP* tcp;
    PackosPacket* packet;

    if (nbytes>socket->remoteWindow)
      nbytes=socket->remoteWindow;

    packet=newPacket(socket,nbytes,&nbytes,&tcp,error);
    if (!packet)
      {
        UtilPrintfStream(errStream,error,"tcp::fire(): newPacket(): %s\n",
                PackosErrorToString(*error));
        return -1;
      }

    if (nbytes>0)
      {
        actual=ByteQueueRead(&(socket->out),
                             ((char*)(tcp))
                             +((tcp->dataOffsetAndFlags)>>12)*4,
                             nbytes,
                             error);

        if (actual<0)
          {
            PackosError tmp;
            PackosPacketFree(packet,&tmp);

            UtilPrintfStream(errStream,error,"tcp::fire(): ByteQueueRead(): %s\n",
                    PackosErrorToString(*error));
            return -1;
          }
      }
    else
      actual=0;

    if (actual>0)
      tcp->dataOffsetAndFlags|=tcpFlagPush;

    tcp->sequenceNumber=socket->out.latestSequenceNumber;

    tcp->dataOffsetAndFlags|=tcpFlagAck;
    tcp->ackNumber=socket->in.latestSequenceNumber;
    socket->in.latestAckNumber=tcp->ackNumber;

    switch (socket->state)
      {
      case tcpSocketStateInvalid:
      case tcpSocketStateListen:
      case tcpSocketStateClosed:
        break;

      case tcpSocketStateSynSent:
      case tcpSocketStateSynReceived:
      case tcpSocketStateEstablished:
      case tcpSocketStateClosing:
      case tcpSocketStateCloseWait:
      case tcpSocketStateLastAck:
      case tcpSocketStateTimeWait:
        break;

      case tcpSocketStateFinWait1:
      case tcpSocketStateFinWait2:
        tcp->dataOffsetAndFlags|=tcpFlagFin;
        break;
      }

    {
      IpHeader h;
      h.kind=ipHeaderTypeTCP;
      h.u.tcp=tcp;
      int checksum=TcpChecksum(packet,&h,error);
      if (checksum<0)
        {
          PackosError tmp;
          PackosPacketFree(packet,&tmp);
          UtilPrintfStream(errStream,error,"tcp<%s>::fire(): TcpChecksum():%s\n",
                  PackosContextGetOwnName(error),
                  PackosErrorToString(*error));
          return -1;
        }

      tcp->checksum=checksum;
    }

#ifdef TCP_DEBUG
    UtilPrintfStream(errStream,error,
            "tcp<%s>::fire(): {%hu => %hu, seq %u, ack %u, ch %hu, len %hu}\n",
            PackosContextGetOwnName(error),
            tcp->sourcePort,tcp->destPort,tcp->sequenceNumber,tcp->ackNumber,
            tcp->checksum,
            nbytes
            );
#endif

    if (IpSend(packet,error)<0)
      {
        PackosError tmp;
        PackosPacketFree(packet,&tmp);
        UtilPrintfStream(errStream,error,"tcp<%s>::fire(): IpSend(): %s\n",
                         PackosContextGetOwnName(&tmp),
                         PackosErrorToString(*error));
        return -1;
      }
#ifdef TCP_DEBUG
    else
      UtilPrintfStream(errStream,error,"tcp<%s>::fire(): IpSend(): success\n",
                       PackosContextGetOwnName(error));
#endif
  }

  return 0;
}

static int tick(IpIface iface,
                PackosError* error)
{
  TcpSocket cur;

  const char* name=PackosContextGetOwnName(error);
  if (!name)
    name=PackosErrorToString(*error);
#ifdef TCP_DEBUG
  UtilPrintfStream(errStream,error,"tcp<%s>::tick()\n",name);
#endif
          
  if (!error) return -2;
  if (!iface)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  cur=iface->tcpContext->first;
  while (cur)
    {
      TcpSocket next=cur->next;

      if (cur->state!=tcpSocketStateListen)
        {
#ifdef TCP_DEBUG
          UtilPrintfStream(errStream,error,"tcp<%s>::tick(): %p, %p\n",name,cur,next);
#endif

          cur->timing.ticksLeft--;
          if (cur->timing.ticksLeft<=0)
            {
              PackosError tmp;
              switch (fire(cur,&tmp))
                {
                case -1:
                  {
                    UtilPrintfStream(errStream,error,"tcp::tick(): fire(): %s\n",
                            PackosErrorToString(tmp));
                    *error=tmp;
                  }
                  break;

                case 0:
                  cur->timing.ticksLeft=cur->timing.initTicks;
                  break;

                default:
                  break;
                }
            }
        }

      cur=next;
    }

#ifdef TCP_DEBUG
  UtilPrintfStream(errStream,error,"tcp<%s>::tick(): done\n",name);
#endif

  if ((*error)!=packosErrorNone)
    return -1;

  return 0;
}

static int checkPackets(IpIface iface,
                        PackosError* error)
{
  if (!error) return -2;
  if (!iface)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (!(iface->tcpContext->timerSocket))
    {
      PackosError tmp;
      if (tick(iface,error)<0)
        {
          UtilPrintfStream(errStream,&tmp,
                           "tcp::checkPackets(): tick(): %s\n",
                           PackosErrorToString(*error));
          return -1;
        }
      return 0;
    }

  {
    int res=0;
    PackosPacket* packet=UdpSocketReceive(iface->tcpContext->timerSocket,
                                          0,true,error);
    if (packet)
      {
        PackosError tmp;
        PackosPacketFree(packet,&tmp);

        if (tick(iface,error)<0)
          {
            UtilPrintfStream(errStream,&tmp,
                             "tcp::checkPackets(): tick(): %s\n",
                             PackosErrorToString(*error));
            return -1;
          }
      }
    else
      {
        switch (*error)
          {
          case packosErrorNone:
          case packosErrorPacketFilteredOut:
          case packosErrorStoppedForOtherSocket:
            return 0;

          default:
            {
              PackosError tmp;
              UtilPrintfStream(errStream,&tmp,
                               "checkPackets(): UdpSocketReceive(): %s\n",
                               PackosErrorToString(*error));
            }
            return -1;
          }
      }

    return res;
  }
}

int TcpSocketReceive(TcpSocket socket,
                     void* buff,
                     uint32_t nbytes,
                     PackosError* error)
{
  if (!error) return -2;
  if (!(socket
        && (socket->iface)
        && buff
        )
      )
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if ((socket->state!=tcpSocketStateEstablished)
      && (socket->state!=tcpSocketStateCloseWait)
      )
    {
      *error=packosErrorConnectionClosed;
      return -1;
    }

  if (!nbytes) return 0;

  while (((socket->state==tcpSocketStateEstablished)
          || (socket->state==tcpSocketStateCloseWait)
          )
         && ((socket->in.len)==0)
         )
    {
      if (checkPackets(socket->iface,error)<0)
        {
          PackosError tmp;
          UtilPrintfStream(errStream,&tmp,
                           "TcpSocketReceive(): checkPackets(): %s\n",
                           PackosErrorToString(*error));
          return -1;
        }

      if (socket->errorThatClosed!=packosErrorNone)
        {
          PackosError tmp;
          (*error)=socket->errorThatClosed;
          UtilPrintfStream(errStream,&tmp,"TcpSocketReceive(): %s\n",
                           PackosErrorToString(*error));
        }
    }

  if (socket->in.len==0)
    {
      *error=packosErrorConnectionClosed;
      return -1;
    }

  {
    int actual=ByteQueueDequeue(&(socket->in),buff,nbytes,error);
    if (actual<0)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&tmp,
                         "TcpSocketReceive(): ByteQueueDequeue(): %s\n",
                         PackosErrorToString(*error));
        return -1;
      }
    return actual;
  }
}

bool TcpSocketReceivePending(TcpSocket socket,
                             PackosError* error)
{
  if (!error) return false;
  if (!(socket
        && (socket->iface)
        && ((socket->state==tcpSocketStateEstablished)
            || (socket->state==tcpSocketStateCloseWait)
            )
        )
      )
    {
#if 0
      if (!socket)
        UtilPrintfStream(errStream,error,"!socket\n");
      else
        {
          if (!(socket->iface))
            UtilPrintfStream(errStream,error,"!(socket->iface)\n");
          else
            UtilPrintfStream(errStream,error,"socket->state %d!=tcpSocketStateEstablished\n",
                             (int)(socket->state)
                             );
        }
#endif

      *error=packosErrorInvalidArg;
      return false;
    }

  *error=packosErrorNone;
  return (socket->in.len)>0;
}

static int ByteQueueEnqueue(ByteQueue* queue,
                            const void* data,
                            uint32_t nbytes,
                            PackosError* error)
{
  int actual;

  if (!error) return -2;
  if (!(queue && data))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (!nbytes) return 0;

  if (queue->len==QUEUE_SIZE)
    {
      *error=packosErrorQueueFull;
      return -1;
    }

  actual=nbytes;
  if (actual>(QUEUE_SIZE-(queue->len)))
    actual=QUEUE_SIZE-(queue->len);

  {
    char* base=(char*)data;
    if ((QUEUE_SIZE-(queue->start+queue->len))<actual)
      {
        int first=QUEUE_SIZE-(queue->start+queue->len);
        int second=actual-first;
        UtilMemcpy(queue->data+queue->start+queue->len,base,first);
        UtilMemcpy(queue->data,base+first,second);
      }
    else
      UtilMemcpy(queue->data+queue->start+queue->len,base,actual);
  }

  queue->len+=actual;
  return actual;
}

static int ByteQueueRead(ByteQueue* queue,
                         void* data,
                         uint32_t nbytes,
                         PackosError* error)
{
  int actual;

  if (!error) return -2;
  if (!(queue && data))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (!nbytes) return 0;

  if (queue->len==0)
    {
      *error=packosErrorQueueEmpty;
      return -1;
    }

  actual=nbytes;
  if (actual>(queue->len))
    actual=queue->len;

  if ((queue->start+actual)>QUEUE_SIZE)
    {
      char* base=(char*)data;
      int first=queue->start+actual-QUEUE_SIZE;
      int second=actual-first;
      UtilMemcpy(base,queue->data+queue->start,first);
      UtilMemcpy(base+first,queue->data,second);
      queue->start=second;
    }
  else
    UtilMemcpy(data,queue->data+queue->start,actual);

  return actual;
}

static int ByteQueueDequeue(ByteQueue* queue,
                            void* data,
                            uint32_t nbytes,
                            PackosError* error)
{
  int actual=ByteQueueRead(queue,data,nbytes,error);
  if (actual>0)
    {
      queue->start+=actual;
      queue->len-=actual;
      if (queue->len==0)
        queue->start=0;
    }

  return actual;
}

static int ByteQueueDrop(ByteQueue* queue,
                         uint32_t nbytes,
                         PackosError* error)
{
  if (!error) return -2;
  if (!queue)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (!nbytes) return 0;

  if (nbytes>=queue->len)
    {
      queue->len=0;
      queue->start=0;
      return 0;
    }

  queue->start+=nbytes;
  queue->start%=QUEUE_SIZE;
  queue->len-=nbytes;
  return 0;
}

static PackosPacket* newPacket(TcpSocket socket,
                               uint32_t datalen,
                               uint32_t* actualDatalen,
                               IpHeaderTCP** tcpHeader,
                               PackosError* error)
{
  PackosPacket* packet;
  uint32_t actualSize;

  if (!error) return 0;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  if (!(socket->iface))
    {
      *error=packosErrorSocketNotBound;
      return 0;
    }

  packet=PackosPacketAlloc(&actualSize,error);
  if (!packet)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "tcp::newPacket(): PackosPacketAlloc(): %s\n",
                       PackosErrorToString(*error));
      return 0;
    }

  packet->ipv6.src=socket->iface->addr;
  packet->ipv6.dest=socket->remoteAddr;

  {
    uint32_t headerSize
      =sizeof(packet->packos)+sizeof(packet->ipv6)+sizeof(IpHeaderTCP);
    if (actualSize<(headerSize+datalen))
      datalen=actualSize-headerSize;
  }

  if (actualDatalen)
    *actualDatalen=datalen;

  {
    IpHeader h;
    IpHeaderTCP tcp;

    h.kind=ipHeaderTypeTCP;
    h.u.tcp=&tcp;
    tcp.sourcePort=socket->localPort;
    tcp.destPort=socket->remotePort;
    tcp.sequenceNumber=0;
    tcp.ackNumber=socket->in.latestSequenceNumber;
    tcp.dataOffsetAndFlags=(sizeof(tcp)/4)<<12;
    tcp.window=QUEUE_SIZE-socket->in.len;
    tcp.urgent=0;
    tcp.checksum=0; /* Will have to compute when sent */

    if (IpHeaderAppend(packet,&h,error)<0)
      {
        PackosError tmp;
        PackosPacketFree(packet,&tmp);
        UtilPrintfStream(errStream,&tmp,
                         "tcp::newPacket(): IpHeaderAppend(): %s\n",
                         PackosErrorToString(*error));
        return 0;
      }

    if (tcpHeader) *tcpHeader=h.u.tcp;
  }

  if (IpPacketSetDataLen(packet,datalen,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,
                       "tcp::newPacket(): IpPacketSetDataLen(): %s\n",
                       PackosErrorToString(*error));
      PackosPacketFree(packet,&tmp);
      return 0;
    }

  return packet;
}

static uint32_t generateInitialSeq(void)
{
  static const uint32_t fixedISN=0xfedcba98;
  PackosError error;

  UtilPrintfStream(errStream,&error,
                   "tcp::generateInitialSeq(): WARNING: insecure: always %u\n",
                   fixedISN);
  return fixedISN;
}

static int sendSimple(TcpSocket socket,
                      uint32_t flags,
                      PackosError* error)
{
  IpHeaderTCP* tcp;
  PackosPacket* packet;
  uint32_t datalen=0,actualDatalen;

  if (!error) return -2;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (flags & tcpFlagSyn)
    datalen=4;

  switch (socket->state)
    {
    case tcpSocketStateInvalid:
    case tcpSocketStateListen:
    case tcpSocketStateSynSent:
    case tcpSocketStateSynReceived:
    case tcpSocketStateEstablished:
    case tcpSocketStateCloseWait:
    case tcpSocketStateClosing:
      break;

    case tcpSocketStateLastAck:
    case tcpSocketStateFinWait1:
    case tcpSocketStateFinWait2:
    case tcpSocketStateClosed:
    case tcpSocketStateTimeWait:
      flags|=tcpFlagFin;
      break;
    }

  packet=newPacket(socket,datalen,&actualDatalen,&tcp,error);
  if (!packet)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "sendSimple(): newPacket(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  if (actualDatalen<datalen)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
              "sendSimple(): packet too small for MSS (%d instead of %d)\n",
              actualDatalen,datalen);
      *error=packosErrorUnknownError;
      return -1;
    }

  if (flags & tcpFlagSyn)
    {
      {
        typedef struct {
          byte kind,length;
          uint16_t mss;
        } MSS;
        MSS* mss=(MSS*)(((char*)tcp)+sizeof(IpHeaderTCP));
        mss->kind=2;
        mss->length=4;
        mss->mss=htons(PACKOS_MTU-sizeof(packet->ipv6));
      }

      tcp->dataOffsetAndFlags
        =((tcp->dataOffsetAndFlags & 0xfff)
          | (((tcp->dataOffsetAndFlags>>12)+1)<<12)
          );
    }

  tcp->sequenceNumber=socket->out.latestSequenceNumber;

  tcp->dataOffsetAndFlags|=flags;

  {
    IpHeader h;
    h.kind=ipHeaderTypeTCP;
    h.u.tcp=tcp;
    int checksum=TcpChecksum(packet,&h,error);
    if (checksum<0)
      {
        PackosError tmp;
        PackosPacketFree(packet,&tmp);
        UtilPrintfStream(errStream,&tmp,
                         "tcp<%s>::sendSimple(): TcpChecksum():%s\n",
                         PackosContextGetOwnName(&tmp),
                         PackosErrorToString(*error));
        return -1;
      }

    tcp->checksum=checksum;
  }

  if (IpSend(packet,error)<0)
    {
      PackosError tmp;
      PackosPacketFree(packet,&tmp);
      UtilPrintfStream(errStream,&tmp,"sendSimple(): IpSend(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  return 0;
}

static int sendSynAndAck(TcpSocket socket,
                         PackosError* error)
{
  return sendSimple(socket,tcpFlagSyn | tcpFlagAck,error);
}

static int sendAck(TcpSocket socket,
                   PackosError* error)
{
  return sendSimple(socket,tcpFlagAck,error);
}

static int sendInitial(TcpSocket socket,
                       uint32_t flags,
                       PackosError* error)
{
  uint32_t seq;

  if (!error) return -2;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  seq=generateInitialSeq();
  socket->out.latestSequenceNumber=seq;
  socket->out.latestAckNumber=seq-1;

  return sendSimple(socket,flags,error);
}

static int sendInitialSynAndAck(TcpSocket socket,
                                PackosError* error)
{
  return sendInitial(socket,tcpFlagSyn | tcpFlagAck,error);
}

static int sendInitialSyn(TcpSocket socket,
                                PackosError* error)
{
  if (sendInitial(socket,tcpFlagSyn,error)<0)
    return -1;
  socket->out.latestSequenceNumber++;

  return 0;
}

static int recordAck(TcpSocket socket,
                     uint32_t ackNumber,
                     PackosError* error)
{
  uint32_t delta;

  if (!error) return -2;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  delta=ackNumber-(socket->out.latestAckNumber);
  if (delta>=0x80000000) return 0;

  if (ByteQueueDrop(&(socket->out),delta,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "recordAck(): ByteQueueDrop(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }
#ifdef TCP_DEBUG
  else
    UtilPrintfStream(errStream,error,
                     "<%s>: recordAck(%u): dropped %u bytes\n",
                     PackosContextGetOwnName(error),
                     ackNumber,delta);
#endif

  socket->out.latestSequenceNumber=socket->out.latestAckNumber=ackNumber;
  return 0;
}

static IpFilterAction TcpFilterMethod(IpIface iface,
                                      void* context,
                                      PackosPacket* packet,
                                      PackosError* error)
{
  TcpSocket socket;
  IpHeader* h;
  IpHeaderTCP* tcp;

#ifdef TCP_DEBUG
  UtilPrintfStream(errStream,error,"%s: TcpFilterMethod()\n",
                   PackosContextGetOwnName(error));
#endif

  if (!error) return ipFilterActionError;

  if (!(iface && packet))
    {
      *error=packosErrorInvalidArg;
      return ipFilterActionError;
    }

  if (!PackosAddrEq(packet->ipv6.dest,iface->addr))
    return ipFilterActionPass;

  h=IpHeaderSeek(packet,ipHeaderTypeTCP,error);
  if (!(h && h->u.tcp))
    return ipFilterActionPass;

  tcp=h->u.tcp;

  socket=seek(iface,tcp->destPort,tcp->sourcePort,error);
  if (socket)
    {
      if ((socket->state!=tcpSocketStateListen)
          && ((!(PackosAddrEq(packet->ipv6.src,socket->remoteAddr)))
              || (socket->state==tcpSocketStateClosed)
              )
          )
        {
          socket=0;
          *error=packosErrorDoesNotExist;
        }
    }

#ifdef TCP_DEBUG
  UtilPrintfStream(errStream,error,"TcpFilterMethod<%s>: {%s%s%s%s%s%s %u => %u, seq %u, ack %u, ch %u, win %u}, {in={lsn=%u, lan=%u, len=%u}, out={lsn=%u, lan=%u, len=%u}}",
          PackosContextGetOwnName(error),
          (tcp->dataOffsetAndFlags & tcpFlagFin ? "F" : ""),
          (tcp->dataOffsetAndFlags & tcpFlagSyn ? "S" : ""),
          (tcp->dataOffsetAndFlags & tcpFlagReset ? "R" : ""),
          (tcp->dataOffsetAndFlags & tcpFlagPush ? "P" : ""),
          (tcp->dataOffsetAndFlags & tcpFlagAck ? "A" : ""),
          (tcp->dataOffsetAndFlags & tcpFlagUrgent ? "U" : ""),
          (unsigned)(tcp->sourcePort),(unsigned)(tcp->destPort),
          (unsigned)(tcp->sequenceNumber),
                   tcp->ackNumber,
                   (unsigned)(tcp->checksum),
                   (unsigned)(tcp->window),
          socket->in.latestSequenceNumber,socket->in.latestAckNumber,
          socket->in.len,
          socket->out.latestSequenceNumber,socket->out.latestAckNumber,
          socket->out.len
          );
#endif

  if (socket->errorThatClosed!=packosErrorNone)
    {
      UtilPrintfStream(errStream,error,"\tSocket already closed: %s\n",
              PackosErrorToString(socket->errorThatClosed));
      *error=socket->errorThatClosed;
      return ipFilterActionReplied;
    }

  if (tcp->dataOffsetAndFlags & tcpFlagReset)
    {
      PackosError tmp;
      PackosPacketFree(packet,&tmp);
      UtilPrintfStream(errStream,error,"\tConnection reset by peer\n");
      socket->errorThatClosed=packosErrorConnectionResetByPeer;
      return ipFilterActionReplied;
    }

#ifdef TCP_DEBUG
  {
    uint32_t size=((tcp->dataOffsetAndFlags)>>12)*4;
    uint32_t offset=sizeof(IpHeaderTCP);
    byte* base=(byte*)tcp;

    while (offset<size)
      {
        byte kind=base[offset];
        byte len=(kind<2 ? 1 : base[offset+1]);
        switch (kind)
          {
          case 0:
            UtilPrintfStream(errStream,error," pad");
            break;

          case 1:
            UtilPrintfStream(errStream,error," nop");
            break;

          case 2:
            {
              uint16_t mss=(base[offset+2]<<8)|(base[offset+3]);
              UtilPrintfStream(errStream,error," mss %hu",mss);
            }
            break;

          default:
            {
              byte i;
              UtilPrintfStream(errStream,error," opt %02x {",(int)kind);
              for (i=2; i<len; i++)
                {
                  if (i>2) UtilPrintfStream(errStream,error," ");
                  UtilPrintfStream(errStream,error,"%02x",(int)(base[offset+i]));
                }
              UtilPrintfStream(errStream,error,"}");
            }
            break;
          }
        offset+=len;
      }
  }

  UtilPrintfStream(errStream,error,"\n");
#endif

  {
    int checksum=TcpChecksum(packet,h,error);
    if (checksum<0)
      UtilPrintfStream(errStream,error,"TcpFilterMethod(): TcpChecksum(): %s\n",
              PackosErrorToString(*error));
    else
      {
        if (checksum!=tcp->checksum)
          UtilPrintfStream(errStream,error,"\tchecksum %04x; expected %04x\n",
                  tcp->checksum,checksum);
      }
  }

  if (!socket)
    {
      UtilPrintfStream(errStream,error,"TcpFilterMethod: !socket: %s\n",
              PackosErrorToString(*error));

      switch (*error)
        {
        case packosErrorDoesNotExist:
          if (IcmpSendDestinationUnreachable
              (iface,
               icmpDestinationUnreachableCodePort,
               packet,
               error)
              <0
              )
            UtilPrintfStream(errStream,error,
                    "TcpFilterMethod(): IcmpSendDestinationUnreachable(): %s\n",
                    PackosErrorToString(*error)
                    );
          else
            UtilPrintfStream(errStream,error,
                             "TcpFilterMethod(): sent port-unreachable\n");

          PackosPacketFree(packet,error);
          *error=packosErrorNone;
          return ipFilterActionErrorICMPed;

        default:
          UtilPrintfStream(errStream,error,"TcpFilterMethod(): seek(%d): %s\n",
                  tcp->destPort,
                  PackosErrorToString(*error));
          return ipFilterActionError;
        }
    }

  socket->remoteWindow=tcp->window;

  switch (socket->state)
    {
    case tcpSocketStateInvalid:
      UtilPrintfStream(errStream,error,"TcpFilterMethod(): invalid state.  Can't happen.\n");
      break;

    case tcpSocketStateClosed:
      UtilPrintfStream(errStream,error,"TcpFilterMethod(): closed again? Can't happen.\n");
      break;

    case tcpSocketStateListen:
      if (!(tcp->dataOffsetAndFlags & tcpFlagSyn))
        UtilPrintfStream(errStream,error,"TcpFilterMethod(): LISTENing, but no SYN\n");
      else
        {
          if (socket->waitingToAccept)
            UtilPrintfStream(errStream,error,"TcpFilterMethod(): LISTENer %hu dropping SYN: already have waitingToAccept %p\n",
                    socket->localPort,
                    socket->waitingToAccept);
          else
            {
              TcpSocket incoming=TcpSocketNew(error);
              if (!incoming)
                UtilPrintfStream(errStream,error,"TcpFilterMethod(): TcpSocketNew(): %s\n",
                        PackosErrorToString(*error));
              else
                {
                  incoming->acceptedFrom=socket;
                  if (TcpSocketBind(incoming,iface->addr,socket->localPort,
                                    error)<0)
                    {
                      if ((*error)==packosErrorPortAlreadyBound)
                        break;
                      else
                        {
                          PackosError tmp;
                          UtilPrintfStream(errStream,error,
                                  "TcpFilterMethod(): TcpSocketBind(): %s\n",
                                  PackosErrorToString(*error));
                          TcpSocketClose(incoming,&tmp);
                          return ipFilterActionError;
                        }
                    }

                  incoming->remoteAddr=packet->ipv6.src;
                  incoming->remotePort=tcp->sourcePort;
                  incoming->state=tcpSocketStateSynReceived;
                  incoming->in.latestSequenceNumber=tcp->sequenceNumber+1;
                  incoming->in.latestAckNumber=tcp->sequenceNumber;

                  if (sendInitialSynAndAck(incoming,error)<0)
                    {
                      PackosError tmp;
                      UtilPrintfStream
                        (errStream,&tmp,
                         "TcpFilterMethod(): sendInitialSynAndAck(): %s\n",
                         PackosErrorToString(*error));
                      
                      TcpSocketClose(incoming,&tmp);
                      return ipFilterActionError;
                    }
                  /*socket->out.latestSequenceNumber++;*/
                  socket->waitingToAccept=incoming;
                }
            }
        }
      break;

    case tcpSocketStateSynSent:
      if (!(tcp->dataOffsetAndFlags & tcpFlagSyn))
        UtilPrintfStream(errStream,error,"TcpFilterMethod(): SYN_SENT, but no SYN\n");
      else
        {
          socket->in.latestSequenceNumber=tcp->sequenceNumber;
          socket->in.latestAckNumber=tcp->sequenceNumber-1;

          if (tcp->dataOffsetAndFlags & tcpFlagAck)
            {
              if (modLt(tcp->ackNumber,socket->out.latestSequenceNumber))
                UtilPrintfStream
                  (errStream,error,
                   "TcpFilterMethod(): SYN_SENT, but wrong ACK # (expected %u)\n",
                   socket->out.latestSequenceNumber);
              else
                {
                  if (sendSynAndAck(socket,error)<0)
                    UtilPrintfStream(errStream,error,
                            "TcpFilterMethod(): sendSynAndAck(): %s\n",
                            PackosErrorToString(*error));
                  else
                    socket->state=tcpSocketStateEstablished;
                }
            }
          else
            {
              if (sendAck(socket,error)<0)
                UtilPrintfStream(errStream,error,
                        "TcpFilterMethod(): sendAck(): %s\n",
                        PackosErrorToString(*error));
              else
                socket->state=tcpSocketStateSynReceived;
            }
        }
      break;

    case tcpSocketStateSynReceived:
      if (!(tcp->dataOffsetAndFlags & tcpFlagAck))
        UtilPrintfStream(errStream,error,
                "TcpFilterMethod(): SYN_RECEIVED, but no ACK\n"
                );
      else
        {
          if (tcp->ackNumber!=(socket->out.latestSequenceNumber))
            UtilPrintfStream(errStream,error,
                    "<%s>: TcpFilterMethod(): SYN_RECEIVED, but bad ACK (expected %u, got %u; delta %d)\n",
                             PackosContextGetOwnName(error),
                    socket->out.latestSequenceNumber,
                    tcp->ackNumber,
                    tcp->ackNumber-socket->out.latestSequenceNumber
                    );
          else
            {
              socket->out.latestAckNumber=tcp->ackNumber;
              socket->state=tcpSocketStateEstablished;
            }
        }
      break;

    case tcpSocketStateCloseWait:
    case tcpSocketStateEstablished:

      if ((tcp->dataOffsetAndFlags) & tcpFlagAck)
        {
          if (recordAck(socket,tcp->ackNumber,error)<0)
            UtilPrintfStream(errStream,error,"TcpFilterMethod(): recordAck(): %s\n",
                    PackosErrorToString(*error));
        }

      {
        int nbytes=IpPacketGetDataLen(packet,error);
        if (nbytes<0)
          UtilPrintfStream(errStream,error,"TcpFilterMethod(): IpPacketGetDataLen(): %s\n",
                  PackosErrorToString(*error));
        else
          {
            void* data=IpPacketData(packet,error);
            if (!data)
              UtilPrintfStream(errStream,error,
                      "TcpFilterMethod(): IpPacketData(): %s\n",
                      PackosErrorToString(*error));
#ifdef TCP_DEBUG
            UtilPrintfStream(errStream,error,
                             "<%s>: TcpFilterMethod(): received %d bytes\n",
                             PackosContextGetOwnName(error),
                             nbytes);
#endif

            if (nbytes>0)
              {
                if (tcp->sequenceNumber>socket->in.latestSequenceNumber)
                  UtilPrintfStream(errStream,error,
                          "TcpFilterMethod(): %d>%d: segment out of order; not supported\n",
                          tcp->sequenceNumber,
                          socket->in.latestSequenceNumber
                          );
                else
                  {
                    int actual=((tcp->sequenceNumber+nbytes)
                                -socket->in.latestSequenceNumber
                                );
                    if (actual>0)
                      {
#ifdef TCP_DEBUG
                        UtilPrintfStream(errStream,error,
                                "TcpFilterMethod(): enqueuing %d bytes\n",
                                actual);
#endif
                        actual=ByteQueueEnqueue(&(socket->in),
                                                ((char*)data)+(nbytes-actual),
                                                actual,error);
                        if (actual<0)
                          UtilPrintfStream(errStream,error,
                                  "TcpFilterMethod(): ByteQueueEnqueue(): %s\n",
                                  PackosErrorToString(*error));
                        else
                          socket->in.latestSequenceNumber+=actual;
                      }
#ifdef TCP_DEBUG
                    else
                      UtilPrintfStream(errStream,error,
                              "TcpFilterMethod(): nothing to enqueue\n");
#endif
                  }

#ifdef TCP_DEBUG
                UtilPrintfStream(errStream,error,"TcpFilterMethod(): incoming queue now has %d bytes\n",socket->in.len);
#endif
              }
          }
      }

      if ((tcp->dataOffsetAndFlags) & tcpFlagFin)
        {
          if (sendAck(socket,error)<0)
            UtilPrintfStream(errStream,error,
                    "TcpFilterMethod(): sendAck(): %s\n",
                    PackosErrorToString(*error));
          else
            socket->state=tcpSocketStateCloseWait;
        }
      break;

    case tcpSocketStateFinWait1:
      if ((tcp->dataOffsetAndFlags & tcpFlagAck)
          && (tcp->ackNumber==(socket->out.latestSequenceNumber))
          )
        {
          socket->out.latestAckNumber=tcp->ackNumber;
          socket->state=tcpSocketStateFinWait2;
        }
      else
        {
          if (tcp->dataOffsetAndFlags & tcpFlagFin)
            {
              if (sendAck(socket,error)<0)
                UtilPrintfStream(errStream,error,"TcpFilterMethod(): sendAck(): %s\n",
                        PackosErrorToString(*error));
              else
                {
                  socket->out.latestSequenceNumber+=2;
                  socket->state=tcpSocketStateClosing;
                }
            }
          else
            UtilPrintfStream(errStream,error,
                    "TcpFilterMethod(): FIN_WAIT_1, but no FIN, no good ACK\n"
                    );
        }
      break;

    case tcpSocketStateFinWait2:
      if (tcp->dataOffsetAndFlags & tcpFlagFin)
        {
          if (sendAck(socket,error)<0)
            UtilPrintfStream(errStream,error,"TcpFilterMethod(): sendAck(): %s\n",
                    PackosErrorToString(*error));
          else
            {
              socket->in.latestSequenceNumber++;
              socket->state=tcpSocketStateTimeWait;
              socket->timing.ticksLeft=60;
            }
        }
      else
        UtilPrintfStream(errStream,error,
                "TcpFilterMethod(): FIN_WAIT_2, but no FIN\n"
                );
      break;

    case tcpSocketStateClosing:
      if ((tcp->dataOffsetAndFlags & tcpFlagAck)
          && modGe(tcp->ackNumber,socket->out.latestSequenceNumber)
          )
        {
          socket->out.latestAckNumber=tcp->ackNumber;
          socket->state=tcpSocketStateTimeWait;
        }
      else
        UtilPrintfStream
          (errStream,error,"TcpFilterMethod(): CLOSING, but no good ACK\n");
      break;

    case tcpSocketStateLastAck:
      if (tcp->dataOffsetAndFlags & tcpFlagAck)
        {
          if (recordAck(socket,tcp->ackNumber,error)<0)
            UtilPrintfStream(errStream,error,
                             "TcpFilterMethod(): recordAck(): %s\n",
                             PackosErrorToString(*error));
          else
            {
              if (sendAck(socket,error)<0)
                UtilPrintfStream(errStream,error,
                                 "TcpFilterMethod(): sendAck(): %s\n",
                                 PackosErrorToString(*error));
              else
                {
                  socket->out.latestAckNumber=tcp->ackNumber;
                  if (socket->out.len==0)
                    {
                      socket->state=tcpSocketStateClosed;
                      TcpSocketClose(socket,error);
                    }
                }
            }
        }
      else
        UtilPrintfStream(errStream,error,
                         "TcpFilterMethod(): LAST_ACK, but no ACK\n");
      break;

    case tcpSocketStateTimeWait:
      UtilPrintfStream(errStream,error,
                       "TcpFilterMethod(): TIME_WAIT; ignoring packet\n");
      break;
    }

  PackosPacketFree(packet,error);
  *error=packosErrorNone;
  return ipFilterActionReplied;
}

int TcpSocketConnect(TcpSocket socket,
                     PackosAddress addr,
                     uint16_t port,
                     PackosError* error)
{
  if (!error) return -2;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (socket->state!=tcpSocketStateClosed)
    {
      *error=packosErrorSocketAlreadyInUse;
      return -1;
    }

#ifdef TCP_DEBUG
  UtilPrintfStream(errStream,error,"%s: TcpSocketConnect(port %u)\n",
                   PackosContextGetOwnName(error),
                   (unsigned int)port);
#endif

  socket->remoteAddr=addr;
  socket->remotePort=port;

  if (sendInitialSyn(socket,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "TcpSocketConnect(): sendInitialSyn(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  socket->state=tcpSocketStateSynSent;

  while ((socket->state==tcpSocketStateSynSent)
         || (socket->state==tcpSocketStateSynReceived)
         )
    {
      if (checkPackets(socket->iface,error)<0)
        {
          PackosError tmp;
          UtilPrintfStream(errStream,&tmp,
                           "TcpSocketConnect(): checkPackets(): %s\n",
                           PackosErrorToString(*error));
          return -1;
        }

      if (socket->errorThatClosed!=packosErrorNone)
        {
          PackosError tmp;
          (*error)=socket->errorThatClosed;
          UtilPrintfStream(errStream,&tmp,
                           "TcpSocketConnect(): %s\n",
                           PackosErrorToString(*error));
        }
    }

  if (socket->state==tcpSocketStateEstablished)
    return 0;
  else
    {
      *error=packosErrorConnectionClosed;
      return -1;
    }
}

int TcpPoll(PackosError* error)
{
  IpIface iface;

  if (!error) return -2;

#ifdef TCP_DEBUG
  UtilPrintfStream(errStream,error,"<%s>::TcpPoll()\n",PackosContextGetOwnName(error));
#endif

  iface=IpIfaceGetFirst(error);
  if (!iface)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "TcpPoll(): IpIfaceGetFirst(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  if (checkPackets(iface,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "TcpPoll(): checkPackets(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

#ifdef TCP_DEBUG
  UtilPrintfStream(errStream,error,"TcpPoll(): done\n");
#endif

  return 0;
}

static bool modLt(uint32_t a, uint32_t b)
{
  return (a-b)>0x80000000U;
}

#if 0
static bool modLe(uint32_t a, uint32_t b)
{
  return (a==b) || modLt(a,b);
}

static bool modGt(uint32_t a, uint32_t b)
{
  return !modLe(a,b);
}
#endif

static bool modGe(uint32_t a, uint32_t b)
{
  return !modLt(a,b);
}

PackosAddress TcpSocketGetPeerAddress(TcpSocket socket,
                                      PackosError* error)
{
  if (!error) return PackosAddrGetZero();
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return PackosAddrGetZero();
    }

  return socket->remoteAddr;
}

int TcpSocketGetLocalPort(TcpSocket socket,
                          PackosError* error)
{
  if (!error) return -2;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  return socket->localPort;
}
