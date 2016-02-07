#include <util/string.h>
#include <util/stream.h>
#include <util/stream-stdio.h>
#include <util/alloc.h>
#include <packos/packet.h>
#include <router.h>

#include <iface-native.h>
#include <iface-shm.h>
#include <udp.h>

void pingerProcess(void)
{
  const char* msg="Hello Outside World!";
  PackosError error;
  IpIface ifaceShm;
  PackosAddressMask mask;

  PackosAddress addr=PackosAddrFromString("7e8f::01",&error);
  if (error!=packosErrorNone)
    {
      UtilPrintfStream(errStream,&error,"router: PackosAddrFromString(addr): %s\n",
	      PackosErrorToString(error));
      return;
    }

  mask.addr=PackosAddrFromString("7e8f::",&error);
  if (error!=packosErrorNone)
    {
      UtilPrintfStream(errStream,&error,"router: PackosAddrFromString(mask): %s\n",
	      PackosErrorToString(error));
      return;
    }
  mask.len=16;

  ifaceShm=IpIfaceShmNew(addr,mask,&error);
  if (!ifaceShm)
    {
      UtilPrintfStream(errStream,&error,"IpIfaceShmNew: %s\n",PackosErrorToString(error));
      return;
    }

  if (IpIfaceRegister(ifaceShm,&error)<0)
    {
      UtilPrintfStream(errStream,&error,"IpIfaceRegister: %s\n",PackosErrorToString(error));
      return;
    }

  {
    PackosPacket* packet;
    UdpSocket socket=UdpSocketNew(&error);
    IpHeaderUDP* udpHeader;
    if (!socket)
      {
        UtilPrintfStream(errStream,&error,"UdpSocketNew: %s\n",PackosErrorToString(error));
        return;
      }

    if (UdpSocketBind(socket,addr,9000,&error)<0)
      {
        UtilPrintfStream(errStream,&error,"UdpSocketBind: %s\n",PackosErrorToString(error));
        return;
      }

    packet=UdpPacketNew(socket,UtilStrlen(msg),&udpHeader,&error);
    if (!packet)
      {
        UtilPrintfStream(errStream,&error,"UdpPacketNew: %s\n",PackosErrorToString(error));
        return;
      }

    udpHeader->destPort=8000;

    packet->ipv6.dest=PackosAddrFromString("7e8f::02",&error);
    if (error!=packosErrorNone)
      {
	UtilPrintfStream(errStream,&error,"pinger: PackosAddrFromString(): %s\n",
		PackosErrorToString(error));
	return;
      }
    packet->packos.dest=packet->ipv6.dest;

    IpPacketSetDataLen(packet,UtilStrlen(msg)+1,&error);
    UtilStrcpy((char*)(IpPacketData(packet,&error)),msg);
    if (UdpSocketSend(socket,packet,&error)<0)
      {
        UtilPrintfStream(errStream,&error,"UdpSocketSend: %s\n",PackosErrorToString(error));
        PackosPacketFree(packet,&error);
      }
  }

  UtilPrintfStream(errStream,&error,"pinger() done: %s\n",msg);
}
