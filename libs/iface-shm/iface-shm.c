#include <iface-shm.h>
#include <iface-shm-protocol.h>
#include <udp.h>
#include <packos/sys/interruptsP.h>

#include <util/stream.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <util/alloc.h>
#include <util/string.h>

typedef struct {
  PackosPacketQueue q;
  UdpSocket irqSock;
  PackosInterruptId irqId;
  int signum;
  bool signalled;
  IfaceShmBuff* buff;
} HalfContext;

typedef struct {
  HalfContext send,receive;

  int shmid;
  IfaceShmBlock* block;
} Context;

struct IpIface onlyIface;
static Context context;
static bool inUse=false;

static const int sendIrqPort=65000;
static const int receiveIrqPort=65001;

static int send(IpIface iface, PackosPacket* packet, PackosError* error)
{
  if (context.send.signalled)
    {
      UtilMemcpy(context.send.buff->data+sizeof(packet->packos),
	     ((byte*)packet)+sizeof(packet->packos),
	     PACKOS_MTU-sizeof(packet->packos));
      context.send.buff->valid=true;
      context.send.signalled=false;
      if (context.block->outsidePid)
	kill(context.block->outsidePid,
	     context.block->send.signumToOutside);
      PackosPacketFree(packet,error);
      return 0;
    }

  if (PackosPacketQueueEnqueue(context.send.q,packet,error)<0)
    {
      UtilPrintfStream(errStream,error,"iface-shm: send: PackosPacketQueueEnqueue: %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  return 0;
}

static PackosPacket* receive(IpIface iface, PackosError* error)
{
  PackosPacket* res=PackosPacketQueueDequeue(context.receive.q,0,error);
  if (!res)
    {
      UtilPrintfStream(errStream,error,"PackosPacketQueueDequeue: %s\n",
              PackosErrorToString(*error));
      return 0;
    }
  res->ipv6.hopLimit--;
  if (res->ipv6.hopLimit==0)
    {
      PackosPacketFree(res,error);
      *error=packosErrorHopLimitExceeded;
      return 0;
    }

  return res;
}

UdpSocket IpIfaceShmGetSendInterruptSocket(IpIface iface,
					   PackosError* error)
{
  if (!inUse)
    {
      *error=packosErrorResourceNotInUse;
      return 0;
    }

  if (iface!=&onlyIface)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  return context.send.irqSock;
}

UdpSocket IpIfaceShmGetReceiveInterruptSocket(IpIface iface,
					      PackosError* error)
{
  if (!inUse)
    {
      *error=packosErrorResourceNotInUse;
      return 0;
    }

  if (iface!=&onlyIface)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  return context.receive.irqSock;
}

int IpIfaceShmOnSendInterrupt(PackosError* error)
{
  PackosPacket* packet;

  UtilPrintfStream(errStream,error,"IpIfaceShmOnSendInterrupt\n");

  if (!inUse)
    {
      *error=packosErrorResourceNotInUse;
      return -1;
    }

  packet=PackosPacketQueueDequeue(context.send.q,0,error);
  if (!packet)
    {
      if ((*error)!=packosErrorQueueEmpty)
        return -1;

      context.send.signalled=true;
      return 0;
    }

  UtilMemcpy(context.send.buff->data,packet,PACKOS_MTU);
  context.send.buff->valid=true;
  if (context.block->outsidePid)
    {
      kill(context.block->outsidePid,
           context.block->send.signumToOutside);
      context.send.signalled=true;
    }

  PackosPacketFree(packet,error);
  return 0;
}

int IpIfaceShmOnReceiveInterrupt(PackosError* error)
{
  PackosPacket* packet;

  UtilPrintfStream(errStream,error,"IpIfaceShmOnReceiveInterrupt\n");

  if (!inUse)
    {
      UtilPrintfStream(errStream,error,"IpIfaceShmOnReceiveInterrupt: not in use\n");
      *error=packosErrorResourceNotInUse;
      return -1;
    }

  packet=PackosPacketAlloc(0,error);
  if (!packet)
    {
      UtilPrintfStream(errStream,error,
              "IpIfaceShmOnReceiveInterrupt(): PackosPacketAlloc(): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  UtilMemcpy(packet,context.receive.buff->data,PACKOS_MTU);
  context.receive.buff->valid=false;

  if (PackosPacketQueueEnqueue(context.receive.q,packet,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,
              "IpIfaceShmOnReceiveInterrupt(): PackosPacketQueueEnqueue(): %s\n",
              PackosErrorToString(*error));
      PackosPacketFree(packet,&tmp);
      return -1;
    }

  if (context.block->outsidePid)
    {
      kill(context.block->outsidePid,
           context.block->receive.signumToOutside);
      UtilPrintfStream(errStream,error,"kill(%d,%d)\n",context.block->outsidePid,
              context.block->receive.signumToOutside);
      context.receive.signalled=true;
    }
  else
    UtilPrintfStream(errStream,error,"IpIfaceShmOnReceiveInterrupt(): no outsidePid\n");

  return 0;
}

static int halfOpen(HalfContext* half,
                    PackosAddress myAddr,
                    uint16_t port,
                    PackosError* error)
{
  half->q=PackosPacketQueueNew(16,error);
  if (!(half->q))
    {
      inUse=false;
      return 0;
    }

  half->irqSock=UdpSocketNew(error);
  if (!(half->irqSock))
    {
      PackosError tmp;
      PackosPacketQueueDelete(half->q,&tmp);
      inUse=false;
      return 0;
    }

  if (UdpSocketBind(half->irqSock,
                    myAddr,
                    port,
                    error)<0)
    {
      PackosError tmp;
      UdpSocketClose(half->irqSock,&tmp);
      PackosPacketQueueDelete(half->q,&tmp);
      inUse=false;
      return 0;
    }

  half->irqId=PackosSimInterruptAllocate(&(half->signum),error);
  if (half->irqId<0)
    {
      PackosError tmp;
      UdpSocketClose(half->irqSock,&tmp);
      PackosPacketQueueDelete(half->q,&tmp);
      inUse=false;
      return 0;
    }

  UtilPrintfStream(errStream,error,"halfOpen(): half->irqId==%d, half->signum==%d\n",
	  half->irqId,half->signum);

  if (PackosInterruptRegisterFor(half->irqId,port,error)<0)
    {
      PackosError tmp;
      PackosSimInterruptDeallocate(half->irqId,&tmp);
      UdpSocketClose(half->irqSock,&tmp);
      PackosPacketQueueDelete(half->q,&tmp);
      inUse=false;
      return 0;
    }

  half->signalled=false;

  return 0;
}

static int halfClose(HalfContext* half, PackosError* error)
{
  if (PackosPacketQueueDelete(half->q,error)<0)
    {
      UtilPrintfStream(errStream,error,"PackosPacketQueueDelete: %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  if (UdpSocketClose(half->irqSock,error)<0)
    {
      UtilPrintfStream(errStream,error,"UdpSocketClose: %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  if (PackosInterruptUnregisterFor(half->irqId,error)<0)
    {
      UtilPrintfStream(errStream,error,"PackosInterruptUnregisterFor: %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  if (PackosSimInterruptDeallocate(half->irqId,error)<0)
    {
      UtilPrintfStream(errStream,error,"PackosSimInterruptDeallocate: %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  return 0;
}

static int closeIface(IpIface iface, PackosError* error)
{
  shmdt(context.block);
  shmctl(context.shmid,IPC_RMID,0);

  if (halfClose(&(context.send),error)<0)
    {
      UtilPrintfStream(errStream,error,"halfClose(send): %s\n",PackosErrorToString(*error));
      return -1;
    }

  if (halfClose(&(context.receive),error)<0)
    {
      UtilPrintfStream(errStream,error,"halfClose(receive): %s\n",PackosErrorToString(*error));
      return -1;
    }

  inUse=false;

  return 0;
}

IpIface IpIfaceShmNew(PackosAddress addr,
                      PackosAddressMask mask,
                      PackosError* error)
{
  PackosAddress myAddr;
  if (inUse)
    {
      *error=packosErrorResourceInUse;
      return 0;
    }

  inUse=true;

  myAddr=PackosMyAddress(error);
  if ((*error)!=packosErrorNone) return 0;

  if (halfOpen(&(context.send),myAddr,sendIrqPort,error)<0)
    {
      UtilPrintfStream(errStream,error,"halfOpen(send): %s\n",PackosErrorToString(*error));
      inUse=false;
      return 0;
    }

  if (halfOpen(&(context.receive),myAddr,receiveIrqPort,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,"halfOpen(receive): %s\n",PackosErrorToString(*error));
      halfClose(&(context.send),&tmp);
      inUse=false;
      return 0;
    }

  context.send.signalled=true;

  context.shmid=shmget(IfaceShmKey,sizeof(IfaceShmBlock),IPC_CREAT | 0600);
  if (context.shmid<0)
    {
      PackosError tmp;
      halfClose(&(context.send),&tmp);
      halfClose(&(context.receive),&tmp);
      inUse=false;
      return 0;
    }

  context.block=(IfaceShmBlock*)(shmat(context.shmid,0,0));
  if (!context.block)
    {
      PackosError tmp;
      shmctl(context.shmid,IPC_RMID,0);
      halfClose(&(context.send),&tmp);
      halfClose(&(context.receive),&tmp);
      inUse=false;
      return 0;
    }

  context.block->outsidePid=0;
  context.block->packosPid=getpid();
  context.block->send.signumToPackos=context.send.signum;
  context.block->receive.signumToPackos=context.receive.signum;

  if (IpIfaceInit(&onlyIface,error)<0)
    {
      PackosError tmp;
      shmdt(context.block);
      shmctl(context.shmid,IPC_RMID,0);
      halfClose(&(context.send),&tmp);
      halfClose(&(context.receive),&tmp);
      inUse=false;
      return 0;
    }

  onlyIface.addr=addr;
  onlyIface.mask=mask;
  onlyIface.send=send;
  onlyIface.receive=receive;
  onlyIface.close=closeIface;
  onlyIface.context=&context;

  context.send.buff=&(context.block->send.buff);
  context.receive.buff=&(context.block->receive.buff);

  return &onlyIface;
}
