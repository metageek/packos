#include <ip.h>
#include <iface.h>

#include <util/stream.h>
#include <util/string.h>
#include <packos/arch.h>
#include <packos/context.h>

int IpHeaderTypeOrder(IpHeaderType type, PackosError* error)
{
  switch (type) {
  case ipHeaderTypeHopByHop: return 1;
  case ipHeaderTypeDestination: return 2;
  case ipHeaderTypeRouting: return 3;
  case ipHeaderTypeFragment: return 4;
  case ipHeaderTypeAuthentication: return 5;
  case ipHeaderTypeESP: return 6;
  case ipHeaderTypeTCP: return 7;
  case ipHeaderTypeUDP: return 7;
  case ipHeaderTypeICMP: return 7;

  case ipHeaderTypeNone: return 255;
  case ipHeaderTypeError:
  default:
    *error=packosErrorInvalidArg;
    return -1;
  }
}

static int sendICMP(IpIface iface,
                    PackosPacket* packet,
                    byte icmpCode,
                    PackosError* error)
{
  UtilPrintfStream(errStream,error,"sendICMP(%u): not implemented\n",
          (unsigned int)icmpCode);
  *error=packosErrorNotImplemented;
  return -1;
}

int IpPacketFromNetworkOrder(PackosPacket* packet,
                             PackosError* error)
{
  IpHeaderIterator it;
  IpHeader* cur;

  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  packet->ipv6.versionAndTrafficClassAndFlowLabel
    =ntohl(packet->ipv6.versionAndTrafficClassAndFlowLabel);
  packet->ipv6.payloadLength
    =ntohs(packet->ipv6.payloadLength);

  it=IpHeaderIteratorNew(packet,error);
  if (!it)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "IpPacketFromNetworkOrder(): IpHeaderIteratorNew(): %s\n",
                       PackosErrorToString(*error)
                       );
      return -1;
    }

  while ((cur=IpHeaderIteratorNext(it,error))!=0)
    {
      switch (cur->kind)
        {
        case ipHeaderTypeHopByHop:
        case ipHeaderTypeDestination:
        case ipHeaderTypeRouting:
          break;

        case ipHeaderTypeFragment:
          cur->u.fragment->fragmentOffsetAndM
            =ntohs(cur->u.fragment->fragmentOffsetAndM);
          cur->u.fragment->id
            =ntohl(cur->u.fragment->id);
          break;

        case ipHeaderTypeUDP:
          cur->u.udp->sourcePort=ntohs(cur->u.udp->sourcePort);
          cur->u.udp->destPort=ntohs(cur->u.udp->destPort);
          cur->u.udp->length=ntohs(cur->u.udp->length);
          cur->u.udp->checksum=ntohs(cur->u.udp->checksum);
          break;

        case ipHeaderTypeICMP:
          cur->u.icmp->checksum=ntohs(cur->u.icmp->checksum);
          switch (cur->u.icmp->type)
            {
            case icmpTypeDestinationUnreachable:
              cur->u.icmp->u.destinationUnreachable.unused=0;
              break;

            case icmpTypePacketTooBig:
              cur->u.icmp->u.packetTooBig.mtu
                =ntohl(cur->u.icmp->u.packetTooBig.mtu);
              break;

            case icmpTypeTimeExceeded:
              cur->u.icmp->u.timeExceeded.unused=0;
              break;

            case icmpTypeParameterProblem:
              cur->u.icmp->u.parameterProblem.pointer
                =ntohl(cur->u.icmp->u.parameterProblem.pointer);
              break;

            case icmpTypeEchoRequest:
              cur->u.icmp->u.echoRequest.identifier
                =ntohs(cur->u.icmp->u.echoRequest.identifier);
              cur->u.icmp->u.echoRequest.sequenceNumber
                =ntohs(cur->u.icmp->u.echoRequest.sequenceNumber);
              break;

            case icmpTypeEchoReply:
              cur->u.icmp->u.echoReply.identifier
                =ntohs(cur->u.icmp->u.echoReply.identifier);
              cur->u.icmp->u.echoReply.sequenceNumber
                =ntohs(cur->u.icmp->u.echoReply.sequenceNumber);
              break;

            default:
              *error=packosErrorNotImplemented;
              return -1;
            }
          break;

        case ipHeaderTypeTCP:
          cur->u.tcp->sourcePort=ntohs(cur->u.tcp->sourcePort);
          cur->u.tcp->destPort=ntohs(cur->u.tcp->destPort);
          cur->u.tcp->sequenceNumber=ntohl(cur->u.tcp->sequenceNumber);
          cur->u.tcp->ackNumber=ntohl(cur->u.tcp->ackNumber);
          cur->u.tcp->dataOffsetAndFlags=ntohs(cur->u.tcp->dataOffsetAndFlags);
          cur->u.tcp->window=ntohs(cur->u.tcp->window);
          cur->u.tcp->checksum=ntohs(cur->u.tcp->checksum);
          cur->u.tcp->urgent=ntohs(cur->u.tcp->urgent);
          break;

        default:
          *error=packosErrorNotImplemented;
          return -1;
        }
    }

  IpHeaderIteratorDelete(it,error);

  return 0;
}

int IpPacketToNetworkOrder(PackosPacket* packet,
                           PackosError* error)
{
  IpHeaderIterator it;
  IpHeader* cur;

  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  packet->ipv6.versionAndTrafficClassAndFlowLabel
    =htonl(packet->ipv6.versionAndTrafficClassAndFlowLabel);
  packet->ipv6.payloadLength
    =htons(packet->ipv6.payloadLength);

  it=IpHeaderIteratorNew(packet,error);
  if (!it) return -1;

  while ((cur=IpHeaderIteratorNext(it,error))!=0)
    {
      switch (cur->kind)
        {
        case ipHeaderTypeHopByHop:
        case ipHeaderTypeDestination:
        case ipHeaderTypeRouting:
          break;

        case ipHeaderTypeFragment:
          cur->u.fragment->fragmentOffsetAndM
            =htons(cur->u.fragment->fragmentOffsetAndM);
          cur->u.fragment->id
            =htonl(cur->u.fragment->id);
          break;

        case ipHeaderTypeUDP:
          cur->u.udp->sourcePort=htons(cur->u.udp->sourcePort);
          cur->u.udp->destPort=htons(cur->u.udp->destPort);
          cur->u.udp->length=htons(cur->u.udp->length);
          cur->u.udp->checksum=htons(cur->u.udp->checksum);
          break;


        case ipHeaderTypeICMP:
          cur->u.icmp->checksum=htons(cur->u.icmp->checksum);
          switch (cur->u.icmp->type)
            {
            case icmpTypeDestinationUnreachable:
              cur->u.icmp->u.destinationUnreachable.unused=0;
              break;

            case icmpTypePacketTooBig:
              cur->u.icmp->u.packetTooBig.mtu
                =htonl(cur->u.icmp->u.packetTooBig.mtu);
              break;

            case icmpTypeTimeExceeded:
              cur->u.icmp->u.timeExceeded.unused=0;
              break;

            case icmpTypeParameterProblem:
              cur->u.icmp->u.parameterProblem.pointer
                =htonl(cur->u.icmp->u.parameterProblem.pointer);
              break;

            case icmpTypeEchoRequest:
              cur->u.icmp->u.echoRequest.identifier
                =htons(cur->u.icmp->u.echoRequest.identifier);
              cur->u.icmp->u.echoRequest.sequenceNumber
                =htons(cur->u.icmp->u.echoRequest.sequenceNumber);
              break;

            case icmpTypeEchoReply:
              cur->u.icmp->u.echoReply.identifier
                =htons(cur->u.icmp->u.echoReply.identifier);
              cur->u.icmp->u.echoReply.sequenceNumber
                =htons(cur->u.icmp->u.echoReply.sequenceNumber);
              break;

            default:
              *error=packosErrorNotImplemented;
              return -1;
            }
          break;

        case ipHeaderTypeTCP:
          cur->u.tcp->sourcePort=htons(cur->u.tcp->sourcePort);
          cur->u.tcp->destPort=htons(cur->u.tcp->destPort);
          cur->u.tcp->sequenceNumber=htonl(cur->u.tcp->sequenceNumber);
          cur->u.tcp->ackNumber=htonl(cur->u.tcp->ackNumber);
          cur->u.tcp->dataOffsetAndFlags=htons(cur->u.tcp->dataOffsetAndFlags);
          cur->u.tcp->window=htons(cur->u.tcp->window);
          cur->u.tcp->checksum=htons(cur->u.tcp->checksum);
          cur->u.tcp->urgent=htons(cur->u.tcp->urgent);
          break;

        default:
          *error=packosErrorNotImplemented;
          return -1;
        }
    }

  return 0;
}

int IpSendOn(IpIface iface, PackosPacket* packet, PackosError* error)
{
  if (!(iface && packet && (iface->send)))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  packet->ipv6.hopLimit--;

  if (IpPacketToNetworkOrder(packet,error)<0) return -1;

  return (iface->send)(iface,packet,error);
}

static PackosPacket*
IpReceiveOnUnfiltered(IpIface iface,
                      byte* protocolP,
                      IpHeaderRouting0** routingHeader,
                      PackosError* error)
{
  IpHeaderIterator it;
  IpHeader* cur;
  PackosPacket* packet;
  byte protocol;

  if (!(iface && (iface->receive)))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  packet=IpIfaceDequeue(iface,0,error);
  if (packet)
    {
      if (IpPacketFromNetworkOrder(packet,error)<0)
        {
          PackosError tmp;
          PackosPacketFree(packet,&tmp);
          UtilPrintfStream(errStream,&tmp,
                  "IpReceiveOnUnfiltered(): IpPacketFromNetworkOrder(): %s\n",
                  PackosErrorToString(*error)
                  );
          return 0;
        }
      return packet;
    }

  switch (*error)
    {
    case packosErrorNone:
    case packosErrorHopLimitExceeded:
    case packosErrorQueueEmpty:
      break;

    default:
      return 0;
    }

  packet=(iface->receive)(iface,error);
  if (!packet)
    {
      if ((*error)!=packosErrorContextYieldedBack)
        {
          PackosError tmp;
          UtilPrintfStream(errStream,&tmp,
                           "IpReceiveOnUnfiltered(): iface->receive(): %s\n",
                           PackosErrorToString(*error)
                           );
        }
      if ((*error)==packosErrorHopLimitExceeded)
	*error=packosErrorNone;
      return 0;
    }

  if (IpPacketFromNetworkOrder(packet,error)<0)
    {
      PackosError tmp;
      PackosPacketFree(packet,&tmp);
      UtilPrintfStream(errStream,&tmp,
              "IpReceiveOnUnfiltered(): IpPacketFromNetworkOrder(): %s\n",
              PackosErrorToString(*error)
              );
      return 0;
    }

  protocol=packet->ipv6.nextHeader;

  it=IpHeaderIteratorNew(packet,error);
  if (!it)
    {
      PackosError tmp;
      PackosPacketFree(packet,&tmp);
      return 0;
    }

  while ((cur=IpHeaderIteratorNext(it,error))!=0)
    {
      protocol=cur->kind;
      switch (cur->kind)
        {
        case ipHeaderTypeHopByHop:
        case ipHeaderTypeDestination:
          {
            byte* optionBase
              =(cur->kind==ipHeaderTypeDestination
                ? cur->u.dest->data
                : cur->u.hopByHop->data
                );

            byte* nextPos=(byte*)(IpHeaderGetNextPos(cur,error));
            if (!nextPos)
              {
                PackosError tmp;
                PackosPacketFree(packet,&tmp);
                return 0;
              }

            while (optionBase<nextPos)
              {
                byte optionType=optionBase[0];
                if (optionType==ipHeaderOptionTypePad1)
                  optionBase++;
                else
                  {
                    /* Unimplemented options, including PadN */
                    byte optionLen=optionBase[1];
                    byte optionCategory=optionType>>6;
                    UtilPrintfStream(errStream,error,
                            "IpReceiveOnUnfiltered(): %s header: unimplemented optionType %u\n",
                            (cur->kind==ipHeaderTypeDestination
                             ? "Destination"
                             : "Hop-By-Hop"
                             ),
                            (unsigned int)optionType);

                    switch (optionCategory)
                      {
                      case 0: break; /* ignore the option */
                      case 2:
                      case 3:
                        if ((optionCategory==2)
                            || !(PackosAddrIsMcast(packet->ipv6.dest))
                            )
                          sendICMP(iface,packet,2,error);
                        /* Fall through */
                      case 1: /* discard the packet */
                        {
                          PackosPacketFree(packet,error);
                          IpHeaderIteratorDelete(it,error);
                          *error=packosErrorOptionNotSupported;
                          return 0;
                        }
                      }

                    optionBase+=optionLen;
                  }
              }
          }
          break;

        case ipHeaderTypeRouting:
          if (routingHeader)
            *routingHeader=cur->u.routing0;
          else
            {
              PackosPacketFree(packet,error);
              IpHeaderIteratorDelete(it,error);
              *error=packosErrorRoutingNotSupported;
              return 0;
            }
          break;

        case ipHeaderTypeFragment:
          PackosPacketFree(packet,error);
          IpHeaderIteratorDelete(it,error);
          *error=packosErrorFragmentsNotSupported;
          return 0;
          break;

        case ipHeaderTypeUDP:
        case ipHeaderTypeTCP:
        case ipHeaderTypeICMP:
          break;

        default:
          PackosPacketFree(packet,error);
          IpHeaderIteratorDelete(it,error);
          *error=packosErrorNotImplemented;
          return 0;
        }
    }

  IpHeaderIteratorDelete(it,error);

  *error=packosErrorNone;
  *protocolP=protocol;
  return packet;
}

PackosPacket* IpReceiveOn(IpIface iface,
                          byte protocolExpected, /* 0 is wildcard */
                          IpHeaderRouting0** routingHeader,
                          PackosError* error)
{
  byte protocol;
  PackosPacket* res
    =IpReceiveOnUnfiltered(iface,&protocol,routingHeader,error);
  if (!res)
    {
      if ((*error)!=packosErrorContextYieldedBack)
        {
          PackosError tmp;
          UtilPrintfStream(errStream,&tmp,
                           "IpReceiveOn(): IpReceiveOnUnfiltered(): %s\n",
                           PackosErrorToString(*error));
        }
      return 0;
    }

  res=IpFilterApply(iface,res,error);
  if (!res)
    {
      PackosError tmp;
      if ((*error)==packosErrorNone)
        *error=packosErrorPacketFilteredOut;
      else
        UtilPrintfStream(errStream,&tmp,
                         "<%s>: IpReceiveOn(): IpFilterApply(): %s\n",
                         PackosContextGetOwnName(&tmp),
                         PackosErrorToString(*error));
      return 0;
    }

  if (protocolExpected && (protocolExpected!=protocol))
    {
      if (IpIfaceEnqueue(iface,res,error)<0)
        PackosPacketFree(res,error);

      *error=packosErrorNone;
      return 0;
    }

  return res;
}

int IpSend(PackosPacket* packet, PackosError* error)
{
  IpIface iface=IpIfaceLookupSend(packet->ipv6.dest,
                                  error);
  if (iface)
    packet->packos.dest=packet->ipv6.dest;
  else
    {
      if ((*error)==packosErrorDoesNotExist)
        {
          PackosAddress addr;
          PackosError defaultRouteError;
          if (IpGetDefaultRoute(&addr,&iface,&defaultRouteError)<0)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,&tmp,
                               "IpSend(): IpIfaceLookupSend(): %s\n",
                               PackosErrorToString(*error));
              UtilPrintfStream(errStream,&tmp,
                               "IpSend(): IpGetDefaultRoute(): %s\n",
                      PackosErrorToString(defaultRouteError));
              if (defaultRouteError==packosErrorDoesNotExist)
                {
                  PackosError tmp;
                  char buff[80];
                  const char* msg=buff;
                  if (PackosAddrToString(packet->ipv6.dest,
                                         buff,sizeof(buff),
                                         &tmp
                                         )<0)
                    msg=PackosErrorToString(tmp);
                  UtilPrintfStream(errStream,&tmp,"IpSend(): no route to %s\n",
                          msg);
                }
              *error=packosErrorNoRouteToHost;
              return -1;
            }
          else
            packet->packos.dest=addr;
        }
    }

  return IpSendOn(iface,packet,error);
}

PackosPacket* IpReceive(byte protocolExpected, /* 0 is wildcard */
                        IpHeaderRouting0** routingHeader,
                        IpIface* ifaceReceivedOn,
                        bool stopWhenReceiveOtherPacket,
                        PackosError* error)
{
  IpIface iface;
  PackosPacket* res=0;
  bool isFirst=true;

  PackosAddress addr=PackosMyAddress(error);
  if ((*error)!=packosErrorNone) return 0;

  iface=IpIfaceLookupReceive(addr,error);
  if (!iface) return 0;

  *error=packosErrorNone;
  while (!res)
    {
      if (isFirst)
        isFirst=false;
      else
        if (stopWhenReceiveOtherPacket)
          {
            *error=packosErrorStoppedForOtherSocket;
            return 0;
          }

      res=IpReceiveOn(iface,protocolExpected,routingHeader,error);
      if (res)
	return res;

      if ((!res) && ((*error)!=packosErrorNone))
        {
          if (ifaceReceivedOn)
            *ifaceReceivedOn=iface;
          return 0;
        }

      res=IpIfaceDequeueFromAny(protocolExpected,ifaceReceivedOn,error);
      if ((!res) && ((*error)!=packosErrorQueueEmpty)) return 0;
    }

  return res;
}

int IpMcastJoin(IpIface iface, PackosAddress group, PackosError* error)
{
  *error=packosErrorNotImplemented;
  return -1;
}

int IpMcastLeave(IpIface iface, PackosAddress group, PackosError* error)
{
  *error=packosErrorNotImplemented;
  return -1;
}

byte IpGetVersion(const PackosPacket* packet,
                  PackosError* error)
{
  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return (packet->ipv6.versionAndTrafficClassAndFlowLabel)>>28;
}

byte IpGetTrafficClass(const PackosPacket* packet,
                       PackosError* error)
{
  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return ((packet->ipv6.versionAndTrafficClassAndFlowLabel)>>20)&255;
}

uint16_t IpGetFlowLabel(const PackosPacket* packet,
                        PackosError* error)
{
  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return (packet->ipv6.versionAndTrafficClassAndFlowLabel)&65535;
}

int IpSetVersion(PackosPacket* packet,
                 byte version,
                 PackosError* error)
{
  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  {
    uint32_t vtfl=(packet->ipv6.versionAndTrafficClassAndFlowLabel);
    vtfl&=~(15<<28);
    vtfl|=(version<<28);
    packet->ipv6.versionAndTrafficClassAndFlowLabel=vtfl;
  }

  *error=packosErrorNone;
  return 0;
}

int IpSetTrafficClass(PackosPacket* packet,
                      byte trafficClass,
                      PackosError* error)
{
  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  {
    uint32_t vtfl=(packet->ipv6.versionAndTrafficClassAndFlowLabel);
    vtfl&=~(255<<20);
    vtfl|=((trafficClass&255)<<20);
    packet->ipv6.versionAndTrafficClassAndFlowLabel=vtfl;
  }

  *error=packosErrorNone;
  return 0;
}

int IpSetFlowLabel(PackosPacket* packet,
                   uint16_t flowLabel,
                   PackosError* error)
{
  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  {
    uint32_t vtfl=(packet->ipv6.versionAndTrafficClassAndFlowLabel);
    vtfl&=~65535;
    vtfl|=(flowLabel&65535);
    packet->ipv6.versionAndTrafficClassAndFlowLabel=vtfl;
  }

  *error=packosErrorNone;
  return 0;
}

int IpHeaderGetSize(IpHeader* header,
                    PackosError* error)
{
  if (!header)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  switch (header->kind)
    {
    case ipHeaderTypeHopByHop:
      return 8*(header->u.hopByHop->len+1);
    case ipHeaderTypeDestination:
      return 8*(header->u.dest->len+1);
    case ipHeaderTypeRouting:
      return 8*(header->u.routing0->len+1);
    case ipHeaderTypeFragment:
      return sizeof(IpHeaderFragment);
    case ipHeaderTypeUDP:
      return sizeof(IpHeaderUDP);
    case ipHeaderTypeICMP:
      return sizeof(IpHeaderICMP);
    case ipHeaderTypeTCP:
      return ((header->u.tcp->dataOffsetAndFlags)>>12)*4;

    default:
      *error=packosErrorNotImplemented;
      return -1;
    }
}

byte* IpHeaderGetBasePos(IpHeader* header,
                         PackosError* error)
{
  if (!header)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  switch (header->kind)
    {
    case ipHeaderTypeHopByHop:
      return (byte*)(header->u.hopByHop);
    case ipHeaderTypeDestination:
      return (byte*)(header->u.dest);
    case ipHeaderTypeRouting:
      return (byte*)(header->u.routing0);
    case ipHeaderTypeFragment:
      return (byte*)(header->u.fragment);
    case ipHeaderTypeUDP:
      return (byte*)(header->u.udp);
    case ipHeaderTypeICMP:
      return (byte*)(header->u.icmp);
    case ipHeaderTypeTCP:
      return (byte*)(header->u.tcp);

    default:
      *error=packosErrorNotImplemented;
      return 0;
    }
}

byte* IpHeaderGetNextPos(IpHeader* header,
                         PackosError* error)
{
  void* base;
  int len;

  if (!header)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  base=IpHeaderGetBasePos(header,error);
  if (!base) return 0;

  len=IpHeaderGetSize(header,error);
  if (len<0) return 0;

  return ((byte*)base)+len;
}

IpHeader* IpHeaderSeek(PackosPacket* packet,
                       IpHeaderType type,
                       PackosError* error)
{
  IpHeaderIterator it;

  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  /* Memory leak: we never free the iterator...and we can't, easily,
   *  because it contains the IpHeader we return.
   */

  it=IpHeaderIteratorNew(packet,error);
  if (!it) return 0;

  while (IpHeaderIteratorHasNext(it,error))
    {
      IpHeader* cur=IpHeaderIteratorNext(it,error);
      if (!cur) return 0;
      if (cur->kind==type) return cur;
    }

  *error=packosErrorDoesNotExist;
  return 0;
}

int IpPacketProtocol(PackosPacket* packet,
                     PackosError* error)
{
  IpHeaderIterator it;
  int res;

  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  it=IpHeaderIteratorNew(packet,error);
  if (!it) return -1;

  res=packet->ipv6.nextHeader;

  while (IpHeaderIteratorHasNext(it,error))
    {
      IpHeader* cur=IpHeaderIteratorNext(it,error);
      if (!cur) return -1;
      res=cur->kind;
    }

  *error=packosErrorNone;
  return res;
}

static IpHeader* getLastHeader(PackosPacket* packet,
                               PackosError* error)
{
  IpHeaderIterator it;
  IpHeader* cur=0;

  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  it=IpHeaderIteratorNew(packet,error);
  if (!it) return 0;

  while (IpHeaderIteratorHasNext(it,error))
    {
      cur=IpHeaderIteratorNext(it,error);
      if (!cur)
        {
          PackosError tmp;
          IpHeaderIteratorDelete(it,&tmp);
          return 0;
        }
    }

  {
    PackosError tmp;
    IpHeaderIteratorDelete(it,&tmp);
  }

  if ((*error)!=packosErrorNone) return 0;

  if (cur) return cur;
  *error=packosErrorDoesNotExist;

  return 0;
}

int IpHeaderAppend(PackosPacket* packet,
                   IpHeader* header,
                   PackosError* error)
{
  IpHeaderIterator it;
  IpHeader* last;
  byte* newBase;
  int len;

  if (!(packet && header))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  len=IpHeaderGetSize(header,error);
  if (len<0) return -1;

  last=getLastHeader(packet,error);
  if ((!last) && ((*error)!=packosErrorDoesNotExist)) return -1;

  if (last)
    {
      {
        int orderHeader, orderLast;
        orderHeader=IpHeaderTypeOrder(header->kind,error);
        if (orderHeader<0) return -1;

        orderLast=IpHeaderTypeOrder(last->kind,error);
        if (orderLast<0) return -1;
        if (orderHeader<orderLast)
          {
            *error=packosErrorHeaderOutOfOrder;
            return -1;
          }
      }

      newBase=IpHeaderGetNextPos(last,error);
      if (!newBase)
        {
          PackosError tmp;
          IpHeaderIteratorDelete(it,&tmp);
          return -1;
        }

      switch (last->kind)
        {
        case ipHeaderTypeHopByHop:
          last->u.hopByHop->nextHeader=header->kind;
          newBase=(((byte*)last->u.hopByHop)
                   +8*(last->u.hopByHop->len+1)
                   );
          break;

        case ipHeaderTypeDestination:
          last->u.dest->nextHeader=header->kind;
          newBase=(((byte*)last->u.dest)
                   +8*(last->u.dest->len+1)
                   );
          break;

        case ipHeaderTypeRouting:
          last->u.routing0->nextHeader=header->kind;
          newBase=(((byte*)last->u.routing0)
                   +8*(last->u.routing0->len+1)
                   );
          break;

        case ipHeaderTypeFragment:
          last->u.fragment->nextHeader=header->kind;
          newBase=(((byte*)last->u.fragment)
                   +sizeof(IpHeaderFragment)
                   );
          break;

        case ipHeaderTypeUDP:
        case ipHeaderTypeTCP:
        case ipHeaderTypeICMP:
          *error=packosErrorHeaderOutOfOrder;
          return -1;

        default:
          *error=packosErrorNotImplemented;
          return -1;
        }
    }
  else
    {
      packet->ipv6.nextHeader=header->kind;
      newBase=packet->ipv6.dataAndHeaders;
    }

  packet->ipv6.payloadLength+=len;
  UtilMemcpy(newBase,header->u.udp,len);

  header->u.udp=(IpHeaderUDP*)newBase;
  return 0;
}

int IpPacketGetDataLen(PackosPacket* packet,
                       PackosError* error)
{
  byte* data=IpPacketData(packet,error);
  if (!data) return -1;

  return packet->ipv6.payloadLength-(data-(packet->ipv6.dataAndHeaders));
}

int IpPacketSetDataLen(PackosPacket* packet,
                       uint16_t datalen,
                       PackosError* error)
{
  byte* data=IpPacketData(packet,error);
  if (!data) return -1;

  packet->ipv6.payloadLength=datalen+(data-(packet->ipv6.dataAndHeaders));
  return 0;
}

byte* IpPacketData(PackosPacket* packet,
                   PackosError* error)
{
  IpHeader* last;
  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  last=getLastHeader(packet,error);
  if (!last)
    {
      if ((*error)!=packosErrorDoesNotExist)
        return 0;

      return packet->ipv6.dataAndHeaders;
    }
  else
    return IpHeaderGetNextPos(last,error);
}
