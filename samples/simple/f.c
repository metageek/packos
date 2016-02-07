#include <util/stream.h>
#include <util/string.h>
#include <util/alloc.h>
#include <packos/packet.h>
#include "processes.h"

#include <iface-native.h>
#include <udp.h>

void f(void)
{
  static const int N1=5;
  static const int N2=1000*1000;
  unsigned int i,j,k;
  char* msg;
  PackosError error;

  if (UtilArenaInit(&error)<0)
    return;

  {
    IpIface iface=IpIfaceNativeNew(&error);
    if (!iface)
      {
        UtilPrintfStream(errStream,&error,
                         "IpIfaceNativeNew: %s\n",PackosErrorToString(error)
                         );
        return;
      }

    if (IpIfaceRegister(iface,&error)<0)
      {
        UtilPrintfStream(errStream,&error,"IpIfaceRegister: %s\n",PackosErrorToString(error));
        return;
      }
  }

  {
    PackosPacket* packet=PackosPacketReceive(&error);
    if (packet)
      {
        msg=UtilStrdup((const char*)(packet->ipv6.dataAndHeaders));
        PackosPacketFree(packet,&error);
      }
    else
      return;
  }

  for (i=0; i<N1; i++)
    {
      for (j=0; j<1; j++)
        {
          UtilPrintfStream(errStream,&error,"f(\"%s\"): %d/%d\n",msg,i,j);
          for (k=0; k<N2; k++);
        }
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

    if (UdpSocketBind(socket,PackosAddrGetZero(),8000,&error)<0)
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

    packet->packos.dest=packet->ipv6.dest=PackosSchedulerAddress(&error);
    {
      char buff[128];
      PackosError tmp;
      if (PackosAddrToString(packet->ipv6.dest,buff,128,&tmp)<0)
        UtilStrcpy(buff,"<error>");
    }
    UtilStrcpy((char*)(IpPacketData(packet,&error)),msg);
    if (UdpSocketSend(socket,packet,&error)<0)
      {
        UtilPrintfStream(errStream,&error,"UdpSocketSend: %s\n",PackosErrorToString(error));
        PackosPacketFree(packet,&error);
      }
  }

  UtilPrintfStream(errStream,&error,"f(\"%s\") done\n",msg);
  free(msg);
}
