#include <util/stream.h>
#include <util/string.h>

#include <iface.h>
#include <ip-filter.h>
#include <packos/arch.h>
#include <packos/checksums.h>
#include <icmp.h>

#include "util.h"

static IpFilterAction IcmpFilterMethod(IpIface iface,
                                       void* context_,
                                       PackosPacket* packet,
                                       PackosError* error)
{
  IpHeaderICMP* header;

#if 0
  UtilPrintfStream(errStream,error,"IcmpFilterMethod(%p): nextHeader %u\n",
          packet,(unsigned int)packet->ipv6.nextHeader);
  if (packet->ipv6.nextHeader==ipHeaderTypeUDP)
    {
      IpHeaderUDP* udp=(IpHeaderUDP*)(packet->ipv6.dataAndHeaders);
      UtilPrintfStream(errStream,error,"\tsrcPort: %hu\n\tdestPort: %hu\n\tlen: %hu\n",
              udp->sourcePort,udp->destPort,
              udp->length);
    }
#endif

  if (!error)
    {
      UtilPrintfStream(errStream,error,"IcmpFilterMethod(): !error\n");
      return -2;
    }

  if (!(iface && packet))
    {
      UtilPrintfStream(errStream,error,"IcmpFilterMethod(): !(iface && packet)\n");
      *error=packosErrorInvalidArg;
      return ipFilterActionError;
    }

  {
    PackosAddress addr=IpIfaceGetAddress(iface,error);
    if (!(PackosAddrEq(packet->ipv6.dest,addr)))
      {
        char destBuff[80],ifaceBuff[80];
        const char* destStr=destBuff;
        const char* ifaceStr=ifaceBuff;
        PackosError tmp;

        if (PackosAddrToString(packet->ipv6.dest,
                               destBuff,sizeof(destBuff),
                               &tmp)
            <0)
          destStr=PackosErrorToString(tmp);

        if (PackosAddrToString(addr,
                               ifaceBuff,sizeof(ifaceBuff),
                               &tmp)
            <0)
          ifaceStr=PackosErrorToString(tmp);

#if 0
        UtilPrintfStream(errStream,error,"IcmpFilterMethod(): %s != %s\n",
                destStr,ifaceStr);
#endif
        return ipFilterActionPass;
      }
  }

  {
    IpHeader* h=IpHeaderSeek(packet,ipHeaderTypeICMP,error);
    if (!h)
      {
        if ((*error)==packosErrorDoesNotExist)
          {
#if 0
            UtilPrintfStream(errStream,error,"IcmpFilterMethod(): not an ICMP packet\n");
#endif
            *error=packosErrorNone;
            return ipFilterActionPass;
          }
        UtilPrintfStream(errStream,error,"IcmpFilterMethod(): IpHeaderSeek(): %s\n",
                PackosErrorToString(*error));
        return ipFilterActionError;
      }
    header=h->u.icmp;
  }

  switch (header->type)
    {
    case icmpTypeDestinationUnreachable:
    case icmpTypePacketTooBig:
    case icmpTypeTimeExceeded:
    case icmpTypeParameterProblem:
    case icmpTypeEchoReply:
      UtilPrintfStream(errStream,error,"IcmpFilterMethod(): unimplemented type %d\n",
              (int)(header->type));
      *error=packosErrorNotImplemented;
      return ipFilterActionError;

    case icmpTypeEchoRequest:
      {
        IpHeader replyHeader;
        IpHeaderICMP replyICMP;
        unsigned int size;

        PackosPacket* reply=PackosPacketAlloc(&size,error);
        if (!reply)
          {
            UtilPrintfStream(errStream,error,"IcmpFilterMethod(): PackosPacketAlloc(): %s\n",
                    PackosErrorToString(*error));
            return ipFilterActionError;
          }

#if 0
        UtilPrintfStream(errStream,error,"IcmpFilterMethod(): echo request\n");
#endif

        reply->packos.src=packet->packos.dest;
        reply->packos.dest=packet->packos.src;
        reply->ipv6.src=packet->ipv6.dest;
        reply->ipv6.dest=packet->ipv6.src;

        
        replyHeader.kind=ipHeaderTypeICMP;
        replyHeader.u.icmp=&replyICMP;
        replyICMP.type=icmpTypeEchoReply;
        replyICMP.code=0;
        replyICMP.u.echoReply.identifier
          =header->u.echoRequest.identifier;
        replyICMP.u.echoReply.sequenceNumber
          =header->u.echoRequest.sequenceNumber;

        if (IpHeaderAppend(reply,&replyHeader,error)<0)
          {
            PackosError tmp;
            UtilPrintfStream(errStream,error,"IcmpFilterMethod(): IpHeaderAppend(): %s\n",
                    PackosErrorToString(*error));
            PackosPacketFree(reply,&tmp);
            return ipFilterActionError;
          }

        {
          int headerLen=IpHeaderGetSize(&replyHeader,error);
          int sizeToCopy,packetDataLen;
          byte* packetData;
          byte* replyData;
          if (headerLen<0)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,error,"IcmpFilterMethod(): IpHeaderGetSize(): %s\n",
                      PackosErrorToString(*error));
              PackosPacketFree(reply,&tmp);
              return ipFilterActionError;
            }
          sizeToCopy=size-72-headerLen;

          packetDataLen=IpPacketGetDataLen(packet,error);
          if (packetDataLen<0)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,error,
                      "IcmpFilterMethod(): IpPacketGetDataLen(packet): %s\n",
                      PackosErrorToString(*error));
              PackosPacketFree(reply,&tmp);
              return ipFilterActionError;
            }

          if (sizeToCopy>packetDataLen)
            sizeToCopy=packetDataLen;

          packetData=IpPacketData(packet,error);
          if (!packetData)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,error,
                      "IcmpFilterMethod(): IpPacketData(packet): %s\n",
                      PackosErrorToString(*error));
              PackosPacketFree(reply,&tmp);
              return ipFilterActionError;
            }

          replyData=IpPacketData(reply,error);
          if (!replyData)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,error,
                      "IcmpFilterMethod(): IpPacketData(reply): %s\n",
                      PackosErrorToString(*error));
              PackosPacketFree(reply,&tmp);
              return ipFilterActionError;
            }

          if (IpPacketSetDataLen(reply,sizeToCopy,error)<0)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,error,
                      "IcmpFilterMethod(): IpPacketSetDataLen(reply): %s\n",
                      PackosErrorToString(*error));
              PackosPacketFree(reply,&tmp);
              return ipFilterActionError;
            }

#if 0
          UtilPrintfStream(errStream,error,
                  "reply: %p, replyData: %p, packetDataLen: %d, sizeToCopy: %d\n",
                  reply,replyData,packetDataLen,sizeToCopy);
#endif

          if (IpPacketToNetworkOrder(packet,error)<0)
            {
              UtilPrintfStream(errStream,error,
                      "IcmpFilterMethod(): IpPacketToNetworkOrder(): %s\n",
                      PackosErrorToString(*error));
              return ipFilterActionError;
            }

          UtilMemcpy(replyData,packetData,sizeToCopy);

          {
            IpHeader* h=IpHeaderSeek(reply,ipHeaderTypeICMP,error);
            if (!h)
              {
                UtilPrintfStream(errStream,error,"IcmpFilterMethod(): IpHeaderSeek(): %s\n",
                        PackosErrorToString(*error));
                return ipFilterActionError;
              }
            h->u.icmp->checksum=IcmpChecksum(reply,h,error);
            if ((*error)!=packosErrorNone)
              {
                UtilPrintfStream(errStream,error,"IcmpFilterMethod(): IcmpChecksum(): %s\n",
                        PackosErrorToString(*error));
                return ipFilterActionError;
              }
          }

          if (IpSendOn(iface,reply,error)<0)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,error,
                      "IcmpFilterMethod(): IpSendOn(): %s\n",
                      PackosErrorToString(*error));
              PackosPacketFree(reply,&tmp);
              return ipFilterActionError;
            }
#if 0
          UtilPrintfStream(errStream,error,"IcmpFilterMethod(): sent echo reply\n");
#endif
        }
        return ipFilterActionReplied;
      }

    default:
      UtilPrintfStream(errStream,error,"IcmpFilterMethod(): unknown type %d\n",
              (int)(header->type));
      *error=packosErrorNotImplemented;
      return ipFilterActionError;
    }
}

int IcmpInit(IpIface iface,
             PackosError* error)
{
  IpFilter filter;

  if (!error) return -2;
  if (!iface)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  filter=IpFilterInstall(iface,IcmpFilterMethod,0,error);
  if (!filter)
    {
      UtilPrintfStream(errStream,error,"IcmpInit(): IpFilterInstall(): %s\n",
              PackosErrorToString(*error));
      return -1;
    }
  return 0;
}

int IcmpSendError(IpIface iface,
		  byte type,
		  byte code,
		  PackosPacket* packet,
		  PackosError* error)
{
  PackosPacket* reply;
  IpHeader replyHeader;
  IpHeaderICMP replyICMP;
  unsigned int size;

  if (!error) return -2;

  if (!(iface && packet))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  reply=PackosPacketAlloc(&size,error);
  if (!reply)
    {
      UtilPrintfStream(errStream,error,"IcmpSendError(): PackosPacketAlloc(): %s\n",
	      PackosErrorToString(*error));
      return -1;
    }

  reply->packos.src=packet->packos.dest;
  reply->packos.dest=packet->packos.src;
  reply->ipv6.src=packet->ipv6.dest;
  reply->ipv6.dest=packet->ipv6.src;

#if 0
  UtilPrintfStream(errStream,error,"IcmpSendError(): addrs:\n");

  {
    PackosAddress addrs[8]={packet->packos.src,packet->packos.dest,
                            packet->ipv6.src,packet->ipv6.dest,
                            reply->packos.src,reply->packos.dest,
                            reply->ipv6.src,reply->ipv6.dest};
    const int N=sizeof(addrs)/sizeof(addrs[0]);
    int i;
    for (i=0; i<N; i++)
      {
        PackosError tmp;
        char buff[80];
        const char* msg=buff;
        if (PackosAddrToString(addrs[i],buff,sizeof(buff),&tmp)<0)
          msg=PackosErrorToString(tmp);
        UtilPrintfStream(errStream,error,"\t%d: %s\n",i,msg);
      }
  }
#endif

  replyHeader.kind=ipHeaderTypeICMP;
  replyHeader.u.icmp=&replyICMP;
  replyICMP.type=type;
  replyICMP.code=code;
  replyICMP.u.destinationUnreachable.unused=0;

  if (IpHeaderAppend(reply,&replyHeader,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,"IcmpFilterMethod(): IpHeaderAppend(): %s\n",
	      PackosErrorToString(*error));
      PackosPacketFree(reply,&tmp);
      return -1;
    }

  {
    int headerLen=IpHeaderGetSize(&replyHeader,error);
    int sizeToCopy,packetLen;
    byte* replyData;
    if (headerLen<0)
      {
	PackosError tmp;
	UtilPrintfStream(errStream,error,"IcmpFilterMethod(): IpHeaderGetSize(): %s\n",
		PackosErrorToString(*error));
	PackosPacketFree(reply,&tmp);
	return -1;
      }
    sizeToCopy=size-72-headerLen;

    packetLen=packet->ipv6.payloadLength+40;
    if (sizeToCopy>packetLen)
      sizeToCopy=packetLen;

    replyData=IpPacketData(reply,error);
    if (!replyData)
      {
	PackosError tmp;
	UtilPrintfStream(errStream,error,
		"IcmpFilterMethod(): IpPacketData(reply): %s\n",
		PackosErrorToString(*error));
	PackosPacketFree(reply,&tmp);
	return -1;
      }

    if (IpPacketSetDataLen(reply,sizeToCopy,error)<0)
      {
	PackosError tmp;
	UtilPrintfStream(errStream,error,
		"IcmpFilterMethod(): IpPacketSetDataLen(reply): %s\n",
		PackosErrorToString(*error));
	PackosPacketFree(reply,&tmp);
	return -1;
      }

    if (IpPacketToNetworkOrder(packet,error)<0)
      {
	UtilPrintfStream(errStream,error,
		"IcmpFilterMethod(): IpPacketToNetworkOrder(): %s\n",
		PackosErrorToString(*error));
	return -1;
      }

    UtilMemcpy(replyData,&(packet->ipv6),sizeToCopy);

    if (IpPacketFromNetworkOrder(packet,error)<0)
      {
	UtilPrintfStream(errStream,error,
		"IcmpFilterMethod(): IpPacketFromNetworkOrder(): %s\n",
		PackosErrorToString(*error));
	return -1;
      }

    {
      IpHeader* h=IpHeaderSeek(reply,ipHeaderTypeICMP,error);
      if (!h)
	{
	  UtilPrintfStream(errStream,error,"IcmpFilterMethod(): IpHeaderSeek(): %s\n",
		  PackosErrorToString(*error));
	  return -1;
	}
      h->u.icmp->checksum=IcmpChecksum(reply,h,error);
      if ((*error)!=packosErrorNone)
	{
	  UtilPrintfStream(errStream,error,"IcmpFilterMethod(): IcmpChecksum(): %s\n",
		  PackosErrorToString(*error));
	  return -1;
	}
    }

    if (IpSendOn(iface,reply,error)<0)
      {
	PackosError tmp;
	UtilPrintfStream(errStream,error,
		"IcmpFilterMethod(): IpSendOn(): %s\n",
		PackosErrorToString(*error));
	PackosPacketFree(reply,&tmp);
	return -1;
      }
    UtilPrintfStream(errStream,error,"IcmpSendError(): sent error %u, %u\n",
            (unsigned int)type,(unsigned int)code
            );
  }

  return 0;
}

int IcmpSendDestinationUnreachable(IpIface iface,
				   IcmpDestinationUnreachableCode code,
				   PackosPacket* packet,
				   PackosError* error)
{
  return IcmpSendError(iface,
		       icmpTypeDestinationUnreachable,
		       (byte)code,
		       packet,
		       error);
}
