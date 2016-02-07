#include <packos/arch.h>
#include <packos/sys/contextP.h>
#include <packos/sys/blockP.h>
#include <packos/sys/packetP.h>
#include <packos/sys/memoryP.h>
#include "kprintfK.h"

typedef struct {
  PackosAddress addr;
  PackosContext context;
} Pair;

#define DEBUG

#define NUM_PAIRS PACKOS_KERNEL_PACKET_MAXADDRS*10
static Pair pairs[NUM_PAIRS];
static bool pairsInited=false;

static void initPairs(void)
{
  int i;

  if (pairsInited) return;

  for (i=0; i<NUM_PAIRS; i++)
    pairs[i].context=0;

  pairsInited=true;
}

PackosAddress PackosAddressGenerate(PackosError* error)
{
  static uint32_t nextId=1;
  PackosAddress res;
  res.octs[0]=res.octs[1]=0;
  res.bytes[0]=0x7e;
  res.bytes[1]=0x8e;
  res.quads[3]=htonl(nextId++);
  nextId%=NUM_PAIRS;
  *error=packosErrorNone;
  return res;
}

int PackosPacketRegisterContext(PackosContext context,
                                PackosError* error)
{
  int i=ntohl(context->os.config->addresses.self.quads[3]);
  initPairs();

  if ((i<0) || (i>=NUM_PAIRS))
    {
      *error=packosErrorAddrUnregistered;
      return -1;
    }

  if (pairs[i].context)
    {
      *error=packosErrorAddrInUse;
      return -1;
    }

  pairs[i].context=context;
  pairs[i].addr=context->os.config->addresses.self;
  return 0;
}

int PackosPacketUnregisterContext(PackosContext context,
                                  PackosError* error)
{
  int i=ntohl(context->os.config->addresses.self.quads[3]);
  initPairs();

  if ((i<0) || (i>=NUM_PAIRS))
    {
      *error=packosErrorAddrUnregistered;
      return -1;
    }

  if ((pairs[i].context==context)
      && (PackosAddrEq(pairs[i].addr,context->os.config->addresses.self))
      )
    {
      pairs[i].context=0;
      return 0;
    }

  *error=packosErrorAddrUnregistered;
  return -1;
}

PackosContext PackosKernelPacketFindContextByAddr(PackosAddress addr,
                                               PackosError* error)
{
  int i=ntohl(addr.quads[3]);

  if ((i<0) || (i>=NUM_PAIRS))
    {
      *error=packosErrorAddrUnregistered;
      return 0;
    }

  initPairs();

  if (!(pairs[i].context))
    {
      *error=packosErrorAddrUnregistered;
      return 0;
    }

  if (!(PackosAddrEq(pairs[i].addr,addr)))
    {
      *error=packosErrorAddrUnregistered;
      return 0;
    }

  *error=packosErrorNone;
  return pairs[i].context;
}

int PackosPacketSend(PackosPacket* packet,
                     bool yieldToRecipient,
                     PackosError* error)
{
  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  packet->packos.src=PackosContextGetAddr(PackosKernelContextCurrent(error),
                                          error);
  return PackosPacketSendFromKernel(packet,yieldToRecipient,error);
}

int PackosPacketSendFromKernel(PackosPacket* packet,
                               bool yieldToRecipient,
                               PackosError* error)
{
  PackosContext recipient;
  PackosInterruptState state;

  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return -1;

  recipient=PackosKernelPacketFindContextByAddr(packet->packos.dest,error);
  if (!recipient)
    {
#ifdef DEBUG
      char buff[80];
      const char* addrStr=buff;
      {
	PackosError tmp;
	if (PackosAddrToString(packet->packos.dest,buff,sizeof(buff),&tmp)<0)
	  addrStr=PackosErrorToString(tmp);
      }
      kprintf("PackosPacketSendFromKernel(): PackosKernelPacketFindContextByAddr(%s): %s\n",
 	      addrStr,
	      PackosErrorToString(*error));
#endif
      PackosKernelContextRestore(state,0);
      if ((*error)==packosErrorAddrUnregistered)
        *error=packosErrorAddressUnreachable;
      return -1;
    }

#ifdef PACKETS_ARE_PAGES
  {
    PackosPageEntry entry=PackosEntryOfPage(packet,error);
    if (!entry)
      {
        PackosKernelContextRestore(state,0);
        return -1;
      }

    if (PackosPageEntryTransfer(entry,PackosKernelContextCurrent(error),
                                recipient,error)<0)
      {
        PackosKernelContextRestore(state,0);
        return -1;
      }
  }
#endif

  if (PackosKernelContextInsertPacket(recipient,packet,error)<0)
    {
      /*kprintf("PackosPacketSendFromKernel(): PackosKernelContextInsertPacket(): %s\n",
        PackosErrorToString(*error));*/
      PackosKernelContextRestore(state,0);
      return -1;
    }

  if (PackosKernelContextBlocking(recipient,error))
    {
      PackosKernelContextSetBlocking(recipient,false,error);
      if (yieldToRecipient)
        PackosKernelContextYieldTo(recipient,error);
    }

  PackosKernelContextRestore(state,0);
  return 0;
}

PackosPacket* PackosPacketReceive(PackosError* error)
{
  return PackosPacketReceiveOrYieldTo(0,error);
}

PackosPacket* PackosPacketReceiveOrYieldTo(PackosContext context,
                                           PackosError* error)
{
  PackosContext current;
  PackosPacket* res;

  PackosInterruptState state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return 0;

  current=PackosKernelContextCurrent(error);
  res=PackosKernelContextExtractPacket(current,error);
  while (!res)
    {
      if (context && PackosKernelContextFinished(context,error))
        {
          PackosKernelContextRestore(state,0);
          *error=packosErrorContextFinished;
          return 0;
        }

      if (context && PackosKernelContextBlocking(context,error))
        {
          PackosError tmp;
          kprintf("<%s>: PackosKernelPacketReceiveOrYieldTo(): context %s is blocked\n",
                  PackosContextGetOwnName(&tmp),
                  PackosContextGetName(context,&tmp));
          PackosKernelContextRestore(state,0);
          *error=packosErrorYieldedToBlocked;
          return 0;
        }

      PackosKernelContextSetBlocking(current,true,error);
      if (context)
        PackosKernelContextYieldTo(context,error);
      else
        PackosKernelContextYield();

      res=PackosKernelContextExtractPacket(current,error);
      if (!res)
        {
          if (context && ((*error)==packosErrorNoPacketAvail))
            *error=packosErrorContextYieldedBack;
          break;
        }
    }

  PackosKernelContextRestore(state,0);
  return res;
}

typedef struct {
  bool inUse;
  union {
    struct {
      uint16_t next,prev;
    } avail;
    struct {
      PackosMemoryLogicalRange logical;
    } inUse;
  } u;
} PacketInPoolMetadata;

#define POOL_SIZE 256

struct {
  PacketInPoolMetadata metadata[POOL_SIZE];
  struct {
    PackosMemoryPhysicalRange physical;
    PackosMemoryLogicalRange logical;
  } packets;
  struct {
    uint16_t first,last;
  } avail;
} packetPool;

static int initPool(PackosError* error)
{
  static bool inited=false;

  int i;
  PackosInterruptState state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return -1;

  if (inited)
    {
      PackosKernelContextRestore(state,0);
      return 0;
    }

  packetPool.packets.physical
    =PackosMemoryPhysicalRangeAlloc(packosMemoryPhysicalTypeRAM,
                                    POOL_SIZE*PACKOS_PAGE_SIZE,4096,
                                    error);
  if (!(packetPool.packets.physical))
    {
      kprintf("packetK:initPool(): PackosMemoryPhysicalRangeAlloc(): %s\n",
              PackosErrorToString(*error));
      kprintf("Cannot initialize the packet pool; must halt.\n");
      asm("hlt");
    }

  packetPool.packets.logical
    =PackosMemoryLogicalRangeNew(packetPool.packets.physical,
                                 0,packosMemoryFlagGlobal,
                                 0,0,0,
                                 error);
  if (!(packetPool.packets.logical))
    {
      kprintf("packetK:initPool(): PackosMemoryLogicalRangeNew(): %s\n",
              PackosErrorToString(*error));
      kprintf("Cannot initialize the packet pool; must halt.\n");
      asm("hlt");
    }

  for (i=0; i<POOL_SIZE; i++)
    {
      packetPool.metadata[i].inUse=false;
      packetPool.metadata[i].u.avail.next=i+1;
      packetPool.metadata[i].u.avail.prev=i-1;
    }
  packetPool.metadata[0].u.avail.prev=0xffffU;
  packetPool.metadata[POOL_SIZE-1].u.avail.next=0xffffU;

  packetPool.avail.first=0;
  packetPool.avail.last=POOL_SIZE-1;

  inited=true;
  PackosKernelContextRestore(state,0);

  return 0;
}

static int packetToIndex(PackosPacket* packet,
                         PackosError* error)
{
  byte* base
    =(PackosPacket*)
    (PackosMemoryLogicalRangeGetAddr(packetPool.packets.logical,
                                     error)
     );
  int res=(((byte*)packet)-base)/PACKOS_PAGE_SIZE;
  if ((res<0) || (res>=POOL_SIZE))
    {
      *error=packosErrorPacketNotInPool;
      return -1;
    }

  return res;
}

PackosPacket* PackosPacketAlloc(SizeType* sizeOut,
                                PackosError* error)
{
  extern PackosContext currentContext;

  PackosPacket* res;
  PackosInterruptState state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return 0;

  if (initPool(error)<0) return 0;

  {
    uint16_t next;
    int i=packetPool.avail.first;
    if (i==0xffffU)
      {
        *error=packosErrorOutOfMemory;
        PackosKernelContextRestore(state,0);
        return 0;
      }

    next=packetPool.metadata[i].u.avail.next;

    packetPool.metadata[i].u.inUse.logical
      =PackosMemoryLogicalRangeNew(packetPool.packets.physical,
                                   currentContext,
                                   packosMemoryFlagWritable,
                                   0,i*PACKOS_PAGE_SIZE,PACKOS_PAGE_SIZE,
                                   error);
    if (!(packetPool.metadata[i].u.inUse.logical))
      {
        PackosKernelContextRestore(state,0);
        return 0;
      }

    res
      =(PackosPacket*)
      (PackosMemoryLogicalRangeGetAddr(packetPool.metadata[i].u.inUse.logical,
                                       error
                                       )
       )
      ;
    if (!res)
      {
        PackosError tmp;
        PackosMemoryLogicalRangeFree(packetPool.metadata[i].u.inUse.logical,
                                     currentContext,
                                     &tmp);
        PackosKernelContextRestore(state,0);
        return 0;
      }

    packetPool.avail.first=next;
    if (packetPool.avail.first==0xffffU)
      packetPool.avail.last=0xffffU;
    else
      packetPool.metadata[packetPool.avail.first].u.avail.prev=0xffffU;

    packetPool.metadata[i].inUse=true;
  }

  res->ipv6.versionAndTrafficClassAndFlowLabel=0x60000000;
  res->ipv6.payloadLength=0;
  res->ipv6.nextHeader=59;
  res->ipv6.hopLimit=255;

  *error=packosErrorNone;

  PackosKernelContextRestore(state,0);

  if (sizeOut) *sizeOut=PACKOS_PAGE_SIZE;

  return res;
}

void PackosPacketFree(PackosPacket* packet,
                      PackosError* error)
{
  int i;
  PackosInterruptState state;

  if (!error) return;

  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return;
    }

  state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return;

  i=packetToIndex(packet,error);
  if (i<0)
    {
      PackosError tmp;
      kprintf("PackosPacketFree(): packetToIndex(0x%x): %s\n",
              packet,PackosErrorToString(*error));
      PackosKernelContextRestore(state,&tmp);
      asm("cli; hlt");
      return;
    }

  if (!(packetPool.metadata[i].inUse))
    {
      *error=packosErrorAlreadyFreed;
      return;
    }

  packetPool.metadata[i].inUse=false;
  packetPool.metadata[i].u.avail.next=packetPool.avail.first;
  packetPool.metadata[i].u.avail.prev=0xffffU;
  packetPool.avail.first=i;
  if (packetPool.metadata[i].u.avail.next==0xffffU)
    packetPool.avail.last=i;
  else
    packetPool.metadata[packetPool.metadata[i].u.avail.next].u.avail.prev=i;

  {
    PackosError tmp;
    PackosKernelContextRestore(state,&tmp);
  }

  *error=packosErrorNone;
}
