#include <packos/checksums.h>
#include <packos/arch.h>

static uint32_t sumRange(void* base,
                         uint32_t count,
                         bool swab)
{
  uint16_t* p=(uint16_t*)base;
  uint32_t res=0;
  int numWords=count/2;
  while (numWords>0)
    {
      uint16_t x=p[--numWords];
      if (swab) x=ntohs(x);
      res+=x;
    }
  if (count&1)
    {
      uint16_t x=(((byte*)base)[count-1]);
      res+=(x<<8);
    }
  return res;
}

int UdpChecksum(PackosPacket* packet,
                IpHeader* udp,
                PackosError* error)
{
  uint32_t res;

  if (!(packet
        && udp
        && (udp->kind==ipHeaderTypeUDP)
        && (udp->u.udp->length==packet->ipv6.payloadLength)
        && (udp->u.udp->length>=sizeof(IpHeaderUDP))
        )
      )
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  res=sumRange(&(packet->ipv6.src),32,true);
  res+=udp->u.udp->length;
  res+=ipHeaderTypeUDP;

  res+=udp->u.udp->sourcePort;
  res+=udp->u.udp->destPort;
  res+=udp->u.udp->length;

  res+=sumRange(((byte*)(udp->u.udp))+sizeof(IpHeaderUDP),
                udp->u.udp->length-sizeof(IpHeaderUDP),
                true);
  while (res>>16)
    res=(res&0xffff)+(res>>16);

  *error=packosErrorNone;
  return (~res)&0xffff;
}

uint16_t IcmpChecksum(PackosPacket* packet,
                      IpHeader* icmp,
                      PackosError* error)
{
  uint32_t sum;
  uint32_t len=packet->ipv6.payloadLength;

  *error=packosErrorNone;

  if (!(packet && icmp && (icmp->kind==ipHeaderTypeICMP)))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  sum=sumRange(&(packet->ipv6.src),32,true);
  sum+=len;
  sum+=58;
  sum+=((uint32_t)(icmp->u.icmp->type)<<8);
  sum+=icmp->u.icmp->code;

  switch (icmp->u.icmp->type)
    {
    case icmpTypeDestinationUnreachable:
      break;

    case icmpTypePacketTooBig:
    case icmpTypeTimeExceeded:
    case icmpTypeParameterProblem:
      *error=packosErrorNotImplemented;
      return -1;

    case icmpTypeEchoReply:
      sum+=icmp->u.icmp->u.echoReply.identifier;
      sum+=icmp->u.icmp->u.echoReply.sequenceNumber;
      break;

    case icmpTypeEchoRequest:
      sum+=icmp->u.icmp->u.echoRequest.identifier;
      sum+=icmp->u.icmp->u.echoRequest.sequenceNumber;
      break;
    }

  sum+=sumRange(((byte*)icmp->u.icmp)+8,len-8,true);

  while (sum>>16)
    sum=(sum&0xffff)+(sum>>16);

  *error=packosErrorNone;
  return (~sum)&0xffff;
}

int TcpChecksum(PackosPacket* packet,
                IpHeader* tcp,
                PackosError* error)
{
  uint32_t res;

  if (!(packet && tcp && (tcp->kind==ipHeaderTypeTCP)))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  res=sumRange(&(packet->ipv6.src),32,true);
  res+=packet->ipv6.payloadLength;
  res+=ipHeaderTypeTCP;

  res+=tcp->u.tcp->sourcePort;
  res+=tcp->u.tcp->destPort;
  res+=(tcp->u.tcp->sequenceNumber>>16);
  res+=(tcp->u.tcp->sequenceNumber&0xffff);
  res+=(tcp->u.tcp->ackNumber>>16);
  res+=(tcp->u.tcp->ackNumber&0xffff);
  res+=tcp->u.tcp->dataOffsetAndFlags;
  res+=tcp->u.tcp->window;
  res+=tcp->u.tcp->checksum;
  res+=tcp->u.tcp->urgent;

  res+=sumRange(((byte*)(tcp->u.tcp))+sizeof(IpHeaderTCP),
                packet->ipv6.payloadLength-sizeof(IpHeaderTCP),
                true);
  res-=tcp->u.tcp->checksum;

  while (res>>16)
    res=(res&0xffff)+(res>>16);

  *error=packosErrorNone;
  return (~res)&0xffff;
}
