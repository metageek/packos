#ifndef _PACKOS_KERNEL_CONTEXTP_H_
#define _PACKOS_KERNEL_CONTEXTP_H_

#define PACKOS_CONTEXT_STACK_SIZE 65536

#ifdef ASM
#define PACKOS_CONTEXT_TSS_OFFSET_CR3 28
#define PACKOS_CONTEXT_TSS_OFFSET_EIP 32
#define PACKOS_CONTEXT_TSS_OFFSET_EFLAGS 36
#define PACKOS_CONTEXT_TSS_OFFSET_EAX 40
#define PACKOS_CONTEXT_TSS_OFFSET_ECX 44
#define PACKOS_CONTEXT_TSS_OFFSET_EDX 48
#define PACKOS_CONTEXT_TSS_OFFSET_EBX 52
#define PACKOS_CONTEXT_TSS_OFFSET_ESP 56
#define PACKOS_CONTEXT_TSS_OFFSET_EBP 60
#define PACKOS_CONTEXT_TSS_OFFSET_ESI 64
#define PACKOS_CONTEXT_TSS_OFFSET_EDI 68
#define PACKOS_CONTEXT_TSS_OFFSET_ES 72
#define PACKOS_CONTEXT_TSS_OFFSET_CS 76
#define PACKOS_CONTEXT_TSS_OFFSET_SS 80
#define PACKOS_CONTEXT_TSS_OFFSET_DS 84
#define PACKOS_CONTEXT_TSS_OFFSET_FS 88
#define PACKOS_CONTEXT_TSS_OFFSET_GS 92
#define PACKOS_CONTEXT_TSS_OFFSET_LDT 96
#else
#include <packos/context.h>
#include <packos/config.h>
#include <packos/packet.h>

typedef struct {
  /* This list must be sorted by logicalAddr. */
  PackosMemoryLogicalRange first;
  PackosMemoryLogicalRange last;

  struct {
    uint16_t min,max;
  } segmentIds;
} ContextPageTable;

typedef struct {
  unsigned int oldTSS:16;
  unsigned int zero0:16;
  unsigned int esp0:32;
  unsigned int ss0:16;
  unsigned int zeroS0:16;
  unsigned int esp1:32;
  unsigned int ss1:16;
  unsigned int zeroS1:16;
  unsigned int esp2:32;
  unsigned int ss2:16;
  unsigned int zeroS2:16;
  unsigned int cr3:32;
  unsigned int eip:32;
  unsigned int eflags:32;
  unsigned int eax:32;
  unsigned int ecx:32;
  unsigned int edx:32;
  unsigned int ebx:32;
  unsigned int esp:32;
  unsigned int ebp:32;
  unsigned int esi:32;
  unsigned int edi:32;
  unsigned int es:16;
  unsigned int esZero:16;
  unsigned int cs:16;
  unsigned int csZero:16;
  unsigned int ss:16;
  unsigned int ssZero:16;
  unsigned int ds:16;
  unsigned int dsZero:16;
  unsigned int fs:16;
  unsigned int fsZero:16;
  unsigned int gs:16;
  unsigned int gsZero:16;
  unsigned int ldt:16;
  unsigned int ldtZero:16;
  unsigned int debugTrap:1;
  unsigned int debugTrapZero:15;
  unsigned int ioMapOffset:16; /* Relative to base of TSS. */
} PackosContext386TSS;

struct PackosContext {
  PackosContext386TSS tss;

  struct {
    int tssIndex; /* <0 if not yet assigned */
    PackosContextFunc func;

    PackosMemoryLogicalRange stackLogical;

    ContextPageTable pageTable;

    struct {
      PackosPacket* packets[PACKOS_PACKET_QUEUE_LEN];
      unsigned int offset,numPackets;
    } queue;

    bool blocking,finished;

    PackosMemoryPhysicalRange configPhysical;
    PackosMemoryLogicalRange configLogical;
    PackosContextConfig* config;

    void* metadata;
  } os;

  struct {
    byte bits[8192];
  } ioBitmap;
};

PackosContext PackosKernelContextNew(PackosContextFunc func,
                                     const char* name,
                                     PackosError* error);
char* PackosKernelContextGetArena(PackosContext context,
                                  SizeType* size,
                                  PackosError* error);

ContextPageTable* PackosKernelPageTable(void);

void* PackosKernelContextGetIfaceRegistry(PackosContext context,
                                       PackosError* error);
int PackosKernelContextSetIfaceRegistry(PackosContext context,
                                        void* ifaceRegistry,
                                        void* oldIfaceRegistry,
                                        PackosError* error);

void PackosKernelLoop(PackosContext scheduler_,
                      PackosError* error);
void PackosKernelHalt(PackosError* error);
void PackosKernelContextExit(void);
void PackosKernelContextYield(void);
void PackosKernelContextYieldTo(PackosContext to,
                                PackosError* error);
PackosContext PackosKernelContextCurrent(PackosError* error);

int PackosKernelSetTick(PackosError* error);

int PackosKernelSetIdleContext(PackosContext idleContext_,
                               PackosError* error);

/* Returns 0 on success, <0 on failure (say, if the receiving context's
 *  queue is full).
 */
int PackosKernelContextInsertPacket(PackosContext context,
                                    PackosPacket* packet,
                                    PackosError* error);
PackosPacket* PackosKernelContextExtractPacket(PackosContext context,
                                               PackosError* error);
bool PackosKernelContextHasPacket(PackosContext context,
                                  PackosError* error);

bool PackosKernelContextBlocking(PackosContext context,
                                 PackosError* error);
void PackosKernelContextSetBlocking(PackosContext context,
                                    bool blocking,
                                    PackosError* error);
bool PackosKernelContextFinished(PackosContext context,
                                 PackosError* error);

#endif /* ASM */

#endif /*_PACKOS_KERNEL_CONTEXTP_H_*/
