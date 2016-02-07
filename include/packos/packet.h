#ifndef _PACKOS_PACKETS_H_
#define _PACKOS_PACKETS_H_

#include <packos/types.h>
#include <packos/errors.h>

#define PACKOS_MTU 1500

typedef union {
  byte bytes[16];
  uint16_t words[8];
  uint32_t quads[4];
  uint64_t octs[2];
} PackosAddress;

typedef struct {
  PackosAddress addr;
  unsigned int len;
} PackosAddressMask;

PackosAddress PackosAddressGenerate(PackosError* error);
PackosAddress PackosMyAddress(PackosError* error);
PackosAddressMask PackosSysAddressMask(PackosError* error);
PackosAddress PackosSchedulerAddress(PackosError* error);
PackosAddress PackosPacketGetKernelAddr(PackosError* error);

PackosAddress PackosAddrFromString(const char* str,
                                   PackosError* error);

/* Returns the length of the string, or <0 on error. */
int PackosAddrToString(PackosAddress addr,
                       char* buf,
                       unsigned int buflen,
                       PackosError* error);
PackosAddress PackosAddrGetZero(void);
bool PackosAddrIsZero(PackosAddress a);
bool PackosAddrIsMcast(PackosAddress a);
bool PackosAddrEq(PackosAddress a, PackosAddress b);
bool PackosAddrMatch(PackosAddressMask mask, PackosAddress addr);

/* Order is wrong? */
typedef struct {
	struct {
		PackosAddress src, dest;
	} packos;
	struct {
		uint32_t versionAndTrafficClassAndFlowLabel;
		uint16_t payloadLength;
		byte nextHeader,hopLimit;
		PackosAddress src, dest;
		byte dataAndHeaders[1];
	} ipv6;
} PackosPacket;

/* Allocates a page-sized packet.  Packets *must* be allocated
 *  via this function; otherwise, PackosPacketSend() will fail.
 *  This is because this function actually allocates a *page*,
 *  which can be mapped into the recipient's memory space, to
 *  avoid copying.
 *
 * sizeOut may be NULL; if it is not, then *sizeOut will be filled
 *  out with the actual size of the allocated packet.
 */
PackosPacket* PackosPacketAlloc(SizeType* sizeOut,
                                PackosError* error);

/* Deallocates a packet allocated via PackosPacketAlloc() or received
 *  via PackosPacketReceive().
 */
void PackosPacketFree(PackosPacket* packet,
                      PackosError* error);

/* Send a packet.  If yieldToRecipient is set, and the recipient is
 *  blocking in PackosPacketReceive(), then the caller will yield the
 *  CPU to the recipient.
 */
int PackosPacketSend(PackosPacket* packet,
                     bool yieldToRecipient,
                     PackosError* error);

/* Receive a packet.  If blocking is true, blocks until a packet is
 *  received, then returns it.  The resulting packet is guaranteed to
 *  be a page, as if it had been returned by PackosPacketAlloc(); when
 *  the caller is done with it, it must be freed with PackosPacketFree(),
 *  or else reused and passed to PackosPacketSend().
 */
PackosPacket* PackosPacketReceive(PackosError* error);

/* For use by the scheduler only.  The scheduler calls this to
 *  say "let so-and-so run until you have a message for me".  If
 *  context is, or becomes, blocked, will return with the error
 *  code packosErrorYieldedToBlocked.  If context is NULL,
 *  equivalent to PackosPacketReceive(), above.
 */
PackosPacket* PackosPacketReceiveOrYieldTo(PackosContext context,
                                           PackosError* error);

int PackosPacketRegisterContext(PackosContext context,
                                PackosError* error);
int PackosPacketUnregisterContext(PackosContext context,
                                  PackosError* error);

#endif /*_PACKOS_PACKETS_H_*/
