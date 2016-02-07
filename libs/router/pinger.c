#include <util/stream.h>
#include <util/string.h>
#include <util/alloc.h>
#include <unistd.h>

#include <packos/packet.h>
#include <router.h>

#include <iface-native.h>
#include <udp.h>

void pingerProcess(void)
{
  const char* msg="Hello Outside World!";
  PackosError error;
  extern PackosAddress routerAddr;
  PackosAddress remoteAddr;
  uint16_t remotePort;
  int N=5;

  {
    IpIface iface=IpIfaceNativeNew(&error);
    if (!iface)
      {
        UtilPrintfStream(errStream,&error,
                "pinger: IpIfaceNativeNew: %s\n",PackosErrorToString(error));
        return;
      }

    if (IpIfaceRegister(iface,&error)<0)
      {
        UtilPrintfStream(errStream,&error,
                "pinger: IpIfaceRegister: %s\n",PackosErrorToString(error));
        return;
      }
  }

  {
    UdpSocket socket=UdpSocketNew(&error);
    if (!socket)
      {
        UtilPrintfStream(errStream,&error,
                "pinger: UdpSocketNew: %s\n",PackosErrorToString(error));
        return;
      }

    if (UdpSocketBind(socket,PackosMyAddress(&error),9000,&error)<0)
      {
        UtilPrintfStream(errStream,&error,
                "pinger: UdpSocketBind: %s\n",PackosErrorToString(error));
        return;
      }

    while (N>0)
      {
        {
          IpHeader* udp;
          PackosPacket* packet=UdpSocketReceive(socket,0,false,&error);
          if (!packet)
            {
              UtilPrintfStream(errStream,&error,
                      "pinger: UdpSocketReceive: %s\n",
                      PackosErrorToString(error));
              return;
            }

          udp=IpHeaderSeek(packet,ipHeaderTypeUDP,&error);
          if (!udp)
            {
              UtilPrintfStream(errStream,&error,
                      "pinger: IpHeaderSeek: %s\n",PackosErrorToString(error));
              return;
            }

          remotePort=udp->u.udp->sourcePort;
          remoteAddr=packet->ipv6.src;

          UtilPrintfStream(errStream,&error,
                  "pinger: pinger received message \"%s\"\n",
                  IpPacketData(packet,&error));

          if (IpPacketToNetworkOrder(packet,&error)<0)
            UtilPrintfStream(errStream,&error,
                    "pinger: pinger: IpPacketToNetworkOrder(): %s\n",
                    PackosErrorToString(error));
          else
            {
              if (write(3,packet,PACKOS_MTU)<0)
                UtilPrintfStream(errStream,&error,"write\n");
            }
          PackosPacketFree(packet,&error);
        }

        {
          PackosPacket* packet;
          IpHeaderUDP* udpHeader;

          packet=UdpPacketNew(socket,UtilStrlen(msg)+1,&udpHeader,&error);
          if (!packet)
            {
              UtilPrintfStream(errStream,&error,
                      "pinger: UdpPacketNew: %s\n",PackosErrorToString(error));
              return;
            }

          udpHeader->destPort=remotePort;

          packet->ipv6.src=PackosMyAddress(&error);
          packet->ipv6.dest=remoteAddr;
          packet->packos.dest=routerAddr;

          IpPacketSetDataLen(packet,UtilStrlen(msg)+1,&error);
          UtilStrcpy((char*)(IpPacketData(packet,&error)),msg);
          if (UdpSocketSend(socket,packet,&error)<0)
            {
              UtilPrintfStream(errStream,&error,
                      "pinger: UdpSocketSend: %s\n",
                      PackosErrorToString(error));
              return;
            }
        }

        N--;
      }
  }

  UtilPrintfStream(errStream,&error,"pinger() done: %s\n",msg);
}
