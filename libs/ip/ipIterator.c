#include <ip.h>
#include <util/alloc.h>

struct IpHeaderIterator {
  PackosPacket* packet;
  uint32_t curOffset;
  byte curType;
  IpHeader curHeader;
};

IpHeaderIterator IpHeaderIteratorNew(PackosPacket* packet,
                                     PackosError* error)
{
  IpHeaderIterator res;
  if (!packet)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  res=(IpHeaderIterator)(malloc(sizeof(struct IpHeaderIterator)));
  if (!res)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  res->packet=packet;

  if (IpHeaderIteratorRewind(res,error)<0)
    {
      free(res);
      return 0;
    }

  return res;
}

int IpHeaderIteratorDelete(IpHeaderIterator it,
                           PackosError* error)
{
  if (!it)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  free(it);
  return 0;
}

IpHeader* IpHeaderIteratorNext(IpHeaderIterator iterator,
                               PackosError* error)
{
  void* h;

  if (!iterator)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  if (iterator->curType==ipHeaderTypeNone)
    {
      *error=packosErrorDoesNotExist;
      return 0;
    }

  h=((iterator->packet->ipv6.dataAndHeaders)+(iterator->curOffset));

  switch (iterator->curHeader.kind=iterator->curType)
    {
    case ipHeaderTypeHopByHop:
      iterator->curHeader.u.hopByHop=(IpHeaderHopByHopOptions*)h;
      iterator->curType=iterator->curHeader.u.hopByHop->nextHeader;
      iterator->curOffset+=8*(iterator->curHeader.u.hopByHop->len+1);
      break;

    case ipHeaderTypeDestination:
      iterator->curHeader.u.dest=(IpHeaderDestOptions*)h;
      iterator->curType=iterator->curHeader.u.dest->nextHeader;
      iterator->curOffset+=8*(iterator->curHeader.u.dest->len+1);
      break;

    case ipHeaderTypeRouting:
      iterator->curHeader.u.routing0=(IpHeaderRouting0*)h;
      iterator->curType=iterator->curHeader.u.routing0->nextHeader;
      iterator->curOffset+=8*(iterator->curHeader.u.routing0->len+1);
      break;

    case ipHeaderTypeFragment:
      iterator->curHeader.u.fragment=(IpHeaderFragment*)h;
      iterator->curType=iterator->curHeader.u.fragment->nextHeader;
      iterator->curOffset+=sizeof(IpHeaderFragment);
      break;

    case ipHeaderTypeUDP:
      iterator->curHeader.u.udp=(IpHeaderUDP*)h;
      iterator->curType=ipHeaderTypeNone;
      iterator->curOffset+=sizeof(IpHeaderUDP);
      break;

    case ipHeaderTypeTCP:
      iterator->curHeader.u.tcp=(IpHeaderTCP*)h;
      iterator->curType=ipHeaderTypeNone;
      iterator->curOffset
        +=((iterator->curHeader.u.tcp->dataOffsetAndFlags)>>12)*4;
      break;

    case ipHeaderTypeICMP:
      iterator->curHeader.u.icmp=(IpHeaderICMP*)h;
      iterator->curType=ipHeaderTypeNone;
      iterator->curOffset+=sizeof(IpHeaderICMP);
      break;

    default:
      iterator->curType=ipHeaderTypeNone;
      *error=packosErrorNotImplemented;
      return 0;
    }

  return &(iterator->curHeader);
}

bool IpHeaderIteratorHasNext(IpHeaderIterator iterator,
                             PackosError* error)
{
  if (!iterator)
    {
      *error=packosErrorInvalidArg;
      return false;
    }

  *error=packosErrorNone;
  switch (iterator->curType)
    {
    case ipHeaderTypeHopByHop:
    case ipHeaderTypeDestination:
    case ipHeaderTypeRouting:
    case ipHeaderTypeFragment:
    case ipHeaderTypeUDP:
    case ipHeaderTypeICMP:
    case ipHeaderTypeTCP:
      return true;

    case ipHeaderTypeNone:
      return false;

    default:
      *error=packosErrorNotImplemented;
      return false;
    }
}

int IpHeaderIteratorRewind(IpHeaderIterator iterator,
                           PackosError* error)
{
  if (!iterator)
    {
      *error=packosErrorInvalidArg;
      return false;
    }

  iterator->curOffset=0;
  iterator->curType=iterator->packet->ipv6.nextHeader;
  iterator->curHeader.u.udp=0;
  return 0;
}
