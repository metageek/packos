#include <packos/sys/interruptsP.h>
#include <packos/sys/blockP.h>
#include <packos/sys/packetP.h>
#include <packos/arch.h>
#include <packos/checksums.h>
#include <udp.h>

#include "kprintfK.h"

static struct {
  PackosAddress addr;
  uint16_t udpPort;
} registeredById[PACKOS_INTERRUPT_NUM_INTERRUPTS+1];
static bool inited=false;

static void init(void)
{
  int i;
  if (inited) return;

  for (i=0; i<=PACKOS_INTERRUPT_NUM_INTERRUPTS; i++)
    registeredById[i].udpPort=0;

  inited=true;
}

int PackosInterruptRegisterFor(PackosInterruptId id,
                               uint16_t udpPort,
                               PackosError* error)
{
  PackosInterruptState state;

  init();

  kprintf("PackosInterruptRegisterFor(%d,%d)\n",(int)id,(int)udpPort);

  state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return -1;

  if (registeredById[id].udpPort)
    {
      PackosKernelContextRestore(state,error);
      *error=packosErrorResourceInUse;
      return -1;
    }

  registeredById[id].addr=PackosMyAddress(error);
  registeredById[id].udpPort=udpPort;

  PackosKernelContextRestore(state,error);
  return 0;
}

int PackosInterruptUnregisterFor(PackosInterruptId id,
                                 PackosError* error)
{
  PackosInterruptState state;

  init();

  state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return -1;

  if (!(PackosAddrEq(registeredById[id].addr,PackosMyAddress(error))))
    {
      *error=packosErrorInvalidArg;
      PackosKernelContextRestore(state,error);
      return -1;
    }

  registeredById[id].udpPort=0;
  PackosKernelContextRestore(state,error);
  return 0;
}

PackosAddress PackosInterruptFor(PackosInterruptId id,
                                 uint16_t* udpPort,
                                 PackosError* error)
{
  if ((id>0) && (id<=PACKOS_INTERRUPT_NUM_INTERRUPTS))
    {
      PackosAddress res;
      PackosInterruptState state=PackosKernelContextBlock(error);
      if ((*error)!=packosErrorNone) return PackosAddrGetZero();

      res=registeredById[id].addr;
      *udpPort=registeredById[id].udpPort;
      PackosKernelContextRestore(state,0);
      return res;
    }

  *udpPort=0;
  return PackosAddrGetZero();
}

int PackosKernelSetTick(PackosError* error)
{
  return 0;
}

int PackosKernelInterruptInit(bool requiresUsr1,
                              bool requiresUsr2,
                              PackosError* error)
{
  return 0;
}

/* Does not lock the kernel; must be called with interrupts blocked.
 *  Fortunately, the only appropriate place to call it is inside an
 *  interrupt handler, which blocks interrupts.
 */
void sendTickPacket(void)
{
  PackosPacket* packet;
  uint16_t udpPort;
  PackosError error;
  PackosInterruptId id=PACKOS_INTERRUPT_KERNEL_ID_CLOCK;

  packet=PackosPacketAlloc(0,&error);
  if (!packet)
    {
      kprintf("sendTickPacket(): PackosKernelPacketAlloc(): %s\n",
              PackosErrorToString(error));
      return;
    }

  packet->packos.dest=PackosInterruptFor(id,
                                         &udpPort,
                                         &error);
  if (!udpPort)
    {
      /*kprintf("sendTickPacket(id %d): no handler registered\n",(int)id);*/
      /*asm("cli; hlt;");*/
      return;
    }

  packet->packos.src=PackosPacketGetKernelAddr(&error);

  packet->ipv6.src=packet->packos.src;
  packet->ipv6.dest=packet->packos.dest;

  packet->ipv6.nextHeader=17;

  {
    IpHeaderUDP* h=(IpHeaderUDP*)(packet->ipv6.dataAndHeaders);
    IpHeader h2;
    short* payload;

    h2.kind=ipHeaderTypeUDP;
    h2.u.udp=h;
    h->sourcePort=0;
    h->destPort=udpPort;
    h->length=sizeof(PackosInterruptId)+sizeof(IpHeaderUDP);

    payload=(short*)(packet->ipv6.dataAndHeaders+sizeof(IpHeaderUDP));
    *payload=htons(id);

    packet->ipv6.payloadLength=h->length;

    h->checksum=htons(UdpChecksum(packet,&h2,&error));

    packet->ipv6.payloadLength=htons(h->length);
    h->length=htons(h->length);
    h->destPort=htons(h->destPort);
  }
  packet->ipv6.versionAndTrafficClassAndFlowLabel
    =htonl(packet->ipv6.versionAndTrafficClassAndFlowLabel);

  if (PackosPacketSendFromKernel(packet,true,&error)<0)
    {
      kprintf("sendTickPacket(): PackosPacketSendFromKernel: %s\n",
              PackosErrorToString(error));
      PackosPacketFree(packet,&error);
      asm("cli; hlt");
    }
}

void PackosKernelIRQHandler(PackosInterruptId id)
{
  PackosPacket* packet;
  uint16_t udpPort;
  PackosError error;

  kprintf("PackosKernelIRQHandler(%d)\n",(int)id);

  packet=PackosPacketAlloc(0,&error);
  if (!packet)
    {
      kprintf("sendTickPacket(): PackosKernelPacketAlloc(): %s\n",
              PackosErrorToString(error));
      return;
    }

  packet->packos.dest=PackosInterruptFor(id,
                                         &udpPort,
                                         &error);
  if (!udpPort)
    {
      /*kprintf("sendTickPacket(id %d): no handler registered\n",(int)id);*/
      /*asm("cli; hlt;");*/
      return;
    }

  packet->packos.src=PackosPacketGetKernelAddr(&error);

  packet->ipv6.src=packet->packos.src;
  packet->ipv6.dest=packet->packos.dest;

  packet->ipv6.nextHeader=17;

  {
    IpHeaderUDP* h=(IpHeaderUDP*)(packet->ipv6.dataAndHeaders);
    IpHeader h2;
    short* payload;

    h2.kind=ipHeaderTypeUDP;
    h2.u.udp=h;
    h->sourcePort=0;
    h->destPort=udpPort;
    h->length=sizeof(PackosInterruptId)+sizeof(IpHeaderUDP);

    payload=(short*)(packet->ipv6.dataAndHeaders+sizeof(IpHeaderUDP));
    *payload=htons(id);

    packet->ipv6.payloadLength=h->length;

    h->checksum=htons(UdpChecksum(packet,&h2,&error));

    packet->ipv6.payloadLength=htons(h->length);
    h->length=htons(h->length);
    h->destPort=htons(h->destPort);
  }
  packet->ipv6.versionAndTrafficClassAndFlowLabel
    =htonl(packet->ipv6.versionAndTrafficClassAndFlowLabel);

  if (PackosPacketSendFromKernel(packet,true,&error)<0)
    {
      kprintf("sendTickPacket(): PackosPacketSendFromKernel: %s\n",
              PackosErrorToString(error));
      PackosPacketFree(packet,&error);
      asm("cli; hlt");
    }
}

