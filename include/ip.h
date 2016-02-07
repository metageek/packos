#ifndef _IP_H_
#define _IP_H_

#include <packos/packet.h>
#include <icmp-types.h>

typedef struct IpIface* IpIface;

#define anonPortMin (1025)
#define anonPortMax (4095)

typedef enum {
  ipHeaderTypeNone=59,
  ipHeaderTypeHopByHop=0,
  ipHeaderTypeDestination=60,
  ipHeaderTypeRouting=43,
  ipHeaderTypeFragment=44,
  ipHeaderTypeAuthentication,
  ipHeaderTypeESP,
  ipHeaderTypeTCP=6,
  ipHeaderTypeUDP=17,
  ipHeaderTypeICMP=58,

  ipHeaderTypeError=-1
} IpHeaderType;

typedef enum {
  ipHeaderOptionTypePad1=0,
  ipHeaderOptionTypePadN=1
} IpHeaderOptionType;

typedef struct {
  byte nextHeader;
  byte len; /* in units of 8 bytes */
  byte data[1];
} IpHeaderHopByHopOptions, IpHeaderDestOptions;

typedef struct {
  byte nextHeader;
  byte len;
  byte routingType0;
  byte segmentsLeft;
  uint32_t reserved;
  PackosAddress addresses[1];
} IpHeaderRouting0;

typedef struct {
  byte nextHeader;
  byte reserved;
  uint16_t fragmentOffsetAndM;
  uint32_t id;
} IpHeaderFragment;

typedef struct {
  uint16_t sourcePort, destPort, length, checksum;
} IpHeaderUDP;

typedef struct {
  uint16_t sourcePort, destPort;
  uint32_t sequenceNumber, ackNumber;
  uint16_t dataOffsetAndFlags, window, checksum, urgent;
} IpHeaderTCP;

typedef struct {
  byte kind;
  union {
    IpHeaderHopByHopOptions* hopByHop;
    IpHeaderDestOptions* dest;
    IpHeaderRouting0* routing0;
    IpHeaderFragment* fragment;
    IpHeaderUDP* udp;
    IpHeaderTCP* tcp;
    IpHeaderICMP* icmp;
  } u;
} IpHeader;

int IpGetDefaultRoute(PackosAddress* addr,
                      IpIface* iface,
                      PackosError* error);
int IpSetDefaultRoute(PackosAddress addr,
                      IpIface iface,
                      PackosError* error);

IpHeaderType IpHeaderGetType(IpHeader* header,
                             PackosError* error);
int IpHeaderGetSize(IpHeader* header,
                    PackosError* error);
byte* IpHeaderGetBasePos(IpHeader* header,
                         PackosError* error);
byte* IpHeaderGetNextPos(IpHeader* header,
                         PackosError* error);
int IpHeaderTypeOrder(IpHeaderType type, PackosError* error);

typedef struct IpHeaderIterator* IpHeaderIterator;

IpHeaderIterator IpHeaderIteratorNew(PackosPacket* packet,
                                     PackosError* error);
int IpHeaderIteratorDelete(IpHeaderIterator it,
                           PackosError* error);

IpHeader* IpHeaderIteratorNext(IpHeaderIterator iterator,
                               PackosError* error);
bool IpHeaderIteratorHasNext(IpHeaderIterator iterator,
                             PackosError* error);
int IpHeaderIteratorRewind(IpHeaderIterator iterator,
                           PackosError* error);

IpHeader* IpHeaderSeek(PackosPacket* packet,
                       IpHeaderType type,
                       PackosError* error);

/* Returns top-level protocol, the last header type in the packet. */
int IpPacketProtocol(PackosPacket* packet,
                     PackosError* error);

int IpPacketGetDataLen(PackosPacket* packet,
                       PackosError* error);
int IpPacketSetDataLen(PackosPacket* packet,
                       uint16_t datalen,
                       PackosError* error);
byte* IpPacketData(PackosPacket* packet,
                   PackosError* error);

int IpHeaderAppend(PackosPacket* packet,
                   IpHeader* header,
                   PackosError* error);

int IpMcastJoin(IpIface iface, PackosAddress group, PackosError* error);
int IpMcastLeave(IpIface iface, PackosAddress group, PackosError* error);

byte IpGetVersion(const PackosPacket* packet,
                  PackosError* error);
byte IpGetTrafficClass(const PackosPacket* packet,
                       PackosError* error);
uint16_t IpGetFlowLabel(const PackosPacket* packet,
                        PackosError* error);
int IpSetVersion(PackosPacket* packet,
                 byte version,
                 PackosError* error);
int IpSetTrafficClass(PackosPacket* packet,
                      byte trafficClass,
                      PackosError* error);
int IpSetFlowLabel(PackosPacket* packet,
                   uint16_t flowLabel,
                   PackosError* error);

int IpSendOn(IpIface iface, PackosPacket* packet, PackosError* error);
PackosPacket* IpReceiveOn(IpIface iface,
                          byte protocolExpected, /* 0 is wildcard */
                          IpHeaderRouting0** routingHeader,
                          PackosError* error);

int IpSend(PackosPacket* packet, PackosError* error);
PackosPacket* IpReceive(byte protocolExpected, /* 0 is wildcard */
                        IpHeaderRouting0** routingHeader,
                        IpIface* ifaceReceivedOn,
                        bool stopWhenReceiveOtherPacket,
                        PackosError* error);

int IpPacketFromNetworkOrder(PackosPacket* packet,
                             PackosError* error);
int IpPacketToNetworkOrder(PackosPacket* packet,
                           PackosError* error);

#endif /*_IP_H_*/
