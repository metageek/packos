#include <util/alloc.h>
#include <util/stream.h>

#include <timer.h>
#include <timer-protocol.h>

struct Timer {
  UdpSocket socket;
  uint32_t id;
  bool repeat;
};

Timer TimerNew(UdpSocket socket,
               uint32_t sec,
               uint32_t usec,
               uint32_t id, /* included in the tick packets */
               bool repeat,
               PackosError* error)
{
  PackosPacket* packet;
  IpHeaderUDP* udpHeader;
  TimerRequest* request;
  Timer timer;
  PackosAddress myAddr;

  if (!error) return 0;
  if (!socket)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  myAddr=PackosMyAddress(error);
  if ((*error)!=packosErrorNone)
    {
      UtilPrintfStream(errStream,error,"TimerNew(): PackosMyAddress(): %s\n",
              PackosErrorToString(*error));
      return 0;
    }

  timer=(Timer)(malloc(sizeof(struct Timer)));
  if (!timer)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  packet=UdpPacketNew(socket,sizeof(TimerRequest),&udpHeader,error);
  if (!packet)
    {
      UtilPrintfStream(errStream,error,"TimerNew(): UdpPacketNew(): %s\n",
              PackosErrorToString(*error));
      free(timer);
      return 0;
    }

  packet->ipv6.dest=PackosSchedulerAddress(error);
  if ((*error)!=packosErrorNone)
    {
      PackosError tmp;
      PackosPacketFree(packet,&tmp);
      free(timer);

      UtilPrintfStream(errStream,error,"TimerNew(): PackosSchedulerAddress(): %s\n",
              PackosErrorToString(*error));
      return 0;
    }

  {
    PackosError tmp;
    char buff[80];
    const char* msg=buff;
    if (PackosAddrToString(packet->ipv6.dest,buff,sizeof(buff),&tmp)<0)
      msg=PackosErrorToString(tmp);
  }

  packet->packos.dest=packet->ipv6.dest;
  packet->ipv6.src=myAddr;

  udpHeader->destPort=TIMER_FIXED_UDP_PORT;

  request=(TimerRequest*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));
  request->id=id;
  request->version=TIMER_PROTOCOL_VERSION;
  request->cmd=timerRequestCmdOpen;
  request->requestId=1;
  request->reserved=0;
  request->args.open.sec=sec;
  request->args.open.usec=usec;
  request->args.open.repeat=repeat;

  if (UdpSocketSend(socket,packet,error)<0)
    {
      PackosError tmp;
      PackosPacketFree(packet,&tmp);
      free(timer);

      UtilPrintfStream(errStream,error,"TimerNew(): UdpSocketSend(): %s\n",
              PackosErrorToString(*error));
      return 0;
    }

  timer->socket=socket;
  timer->id=id;
  timer->repeat=repeat;

  return timer;
}

int TimerClose(Timer timer,
               PackosError* error)
{
  *error=packosErrorNotImplemented;
  return -1;
}
