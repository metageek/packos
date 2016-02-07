#ifndef _PACKOS_KERNEL_PACKETP_H_
#define _PACKOS_KERNEL_PACKETP_H_

#include <packos/packet.h>

#define PACKOS_KERNEL_PACKET_MAXADDRS 32
#if 0
#define PACKETS_ARE_PAGES
#endif

int PackosKernelPacketRegisterContext(PackosContext context,
                                      PackosError* error);
int PackosKernelPacketUnregisterContext(PackosContext context,
                                        PackosError* error);
PackosContext PackosKernelPacketFindContextByAddr(PackosAddress addr,
                                                  PackosError* error);

int PackosPacketSendFromKernel(PackosPacket* packet,
                               bool yieldToRecipient,
                               PackosError* error);

int PackosKernelPacketSend(PackosPacket* packet,
                           bool yieldToRecipient,
                           PackosError* error);
PackosPacket* PackosKernelPacketReceive(PackosError* error);
PackosPacket* PackosKernelPacketReceiveOrYieldTo(PackosContext context,
                                                 PackosError* error);
PackosPacket* PackosKernelPacketAlloc(PackosError* error);
void PackosKernelPacketFree(PackosPacket* packet,
                            PackosError* error);

#endif /*_PACKOS_KERNEL_PACKETP_H_*/
