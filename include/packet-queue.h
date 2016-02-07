#ifndef _PACKET_QUEUE_H_
#define _PACKET_QUEUE_H_

#include <packos/packet.h>

typedef struct PackosPacketQueue {
  PackosPacket** packets;
  uint16_t capacity,start,count;
}* PackosPacketQueue;

PackosPacketQueue PackosPacketQueueNew(int capacity,
                                       PackosError* error);
int PackosPacketQueueDelete(PackosPacketQueue queue,
                            PackosError* error);
int PackosPacketQueueEnqueue(PackosPacketQueue queue,
                             PackosPacket* packet,
                             PackosError* error);
PackosPacket* PackosPacketQueueDequeue(PackosPacketQueue queue,
                                       byte protocolExpected,
                                       PackosError* error);
bool PackosPacketQueueNonEmpty(PackosPacketQueue queue,
                               PackosError* error);

#endif /*_PACKET_QUEUE_H_*/
