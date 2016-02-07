#include "kprintfK.h"
#include "kputcK.h"
#include "pagingK.h"
#include "swapContextK.h"

#include <packos/checksums.h>
#include <packos/arch.h>

#include <packos/sys/memoryP.h>
#include <packos/sys/packetP.h>
#include <packos/sys/interruptsP.h>
#include <packos/sys/contextP.h>
#include <packos/sys/blockP.h>
#include <packos/sys/asm.h>

#include <ip.h>

#include <util/string.h>

static PackosContext scheduler=0;
static PackosContext idleContext=0;
PackosContext currentContext=0;

#define PRINT_ESP(msg)\
  {const uint32_t* esp; asm("movl %%esp, %0": "=r"(esp));\
    kprintf("%s: %p: %x %x %x %x %x %x\n",msg,esp,\
            esp[0],esp[1],esp[2],esp[3],esp[4],esp[5]);}

//#define DEBUG

#ifdef DEBUG
static void dumpTSS(const PackosContext386TSS* tss);
#endif

int PackosKernelSetIdleContext(PackosContext idleContext_,
                               PackosError* error)
{
  PackosInterruptState state;

  if (!error) return -2;
  if (!idleContext_)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return -1;

  if (currentContext!=scheduler)
    {
      *error=packosErrorKernelPermissionDenied;
      return -1;
    }

  idleContext=idleContext_;
  PackosKernelContextRestore(state,0);

  *error=packosErrorNone;
  return 0;
}

#ifdef SWAP_VIA_TSS
static int installTSS(PackosContext386TSS* tss,
                      uint32_t size,
                      PackosError* error)
{
  if (!error) return -2;

  uint16_t numDescriptors;
  Packos386SegmentDescriptor* descriptors
    =Packos386GetDescriptorTable(&numDescriptors);

  int index=Packos386AllocateDescriptor(error);
  if (index<0)
    {
      kprintf("installTSS(): Packos386AllocateDescriptor(): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  Packos386SegmentDescriptorSane sane;
  sane.base=tss;
  sane.size=size;
  sane.inPages=false;
  sane.availableForOS=false;
  sane.present=true;
  sane.type=packos386DescriptorTypeTSS32;
  sane.privilegeLevel=0;
  sane.u.tss.busy=0;

  Packos386SegmentDescriptor* descriptor=descriptors+index;
  if (Packos386BuildSegmentDescriptor(&sane,descriptor,error)<0)
    {
      kprintf("swapContext(): Packos386BuildSegmentDescriptor(): %s\n",
              PackosErrorToString(*error));
      index=-1;
      return -1;
    }
  Packos386SetGDT(descriptors,numDescriptors,error);
  return 0;
}
#endif

void PackosKernelLoop(PackosContext scheduler_,
                      PackosError* error)
{
  PackosInterruptState state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return;

  kprintf("PackosKernelLoop()\n");

  if (currentContext)
    {
      kprintf("PackosKernelLoop(): already looping\n");
      *error=packosErrorKernelCantReenterMainLoop;
      PackosKernelContextRestore(state,0);
      return;
    }

  {
    static PackosContext386TSS initialTSS;
    initialTSS.oldTSS=0;
    initialTSS.zero0=0;
    initialTSS.esp0=0;
    initialTSS.ss0=0;
    initialTSS.esp1=0;
    initialTSS.ss1=0;
    initialTSS.esp2=0;
    initialTSS.ss2=0;
    initialTSS.cr3=0;
    initialTSS.eip=0;

    initialTSS.eax=initialTSS.ebx=initialTSS.ecx=initialTSS.edx=0;
    initialTSS.esp=initialTSS.ebp=initialTSS.esi=initialTSS.edi=0;
    initialTSS.esZero=initialTSS.csZero=initialTSS.ssZero=
      initialTSS.dsZero=initialTSS.fsZero=initialTSS.gsZero=0;
    initialTSS.ldt=0;
    initialTSS.ldtZero=0;
    initialTSS.debugTrap=false;
    initialTSS.debugTrapZero=0;
    initialTSS.ioMapOffset=sizeof(initialTSS);

    {
      uint32_t flags;
      asm("pushfl");
      asm("popl %0\n":"=r"(flags));
      initialTSS.eflags=flags;
    }

    {
      uint32_t seg;
      asm("movl %ds, %eax\n");
      asm("movl %%eax, %0\n":"=r"(seg));
      initialTSS.ds=seg;

      asm("movl %cs, %eax\n");
      asm("movl %%eax, %0\n":"=r"(seg));
      initialTSS.cs=seg;
    }

    initialTSS.es=initialTSS.fs=initialTSS.gs=initialTSS.ss=initialTSS.ds;

#ifdef SWAP_VIA_TSS
    int tssId=installTSS(&initialTSS,sizeof(initialTSS),error);
    if (tssId<0)
      {
        kprintf("PackosKernelLoop(): installTSS(): %s\n",
                PackosErrorToString(*error));
        PackosKernelContextRestore(state,0);
        return;
      }

    asm("ltr %0": :"r"((uint16_t)tssId));

    //dumpTSS(&initialTSS);
#endif
  }

  currentContext=scheduler=scheduler_;
  if (!currentContext)
    {
      kprintf("PackosKernelLoop(): no scheduler\n");
      *error=packosErrorKernelNoContexts;
      PackosKernelContextRestore(state,0);
      return;
    }

  if (PackosKernelInterruptInit(true,true,error)<0)
    {
      kprintf("PackosKernelLoop(): PackosKernelInterruptInit(): %s\n",
              PackosErrorToString(*error));
      PackosKernelContextRestore(state,0);
      return;
    }

  if (PackosKernelSetTick(error)<0)
    {
      kprintf("PackosKernelLoop(): PackosKernelSetTick(): %s\n",
              PackosErrorToString(*error));
      PackosKernelContextRestore(state,0);
      return;
    }

  if (PackosKernelContextRestore(state,error)<0)
    {
      kprintf("PackosKernelLoop(): PackosKernelContextRestore(): %d\n",
              (int)(*error));
      return;
    }

  //Packos386DumpGDT(true,error);

  {
    Packos386SegmentSelectorParsed parsed;
    if (Packos386InterpretSegmentSelector(currentContext->tss.cs,&parsed,error)<0)
      kprintf("Packos386InterpretSegmentSelector(cs: %x): %s\n",
              currentContext->tss.cs,
              PackosErrorToString(*error));
    else
      kprintf("cs: %d:%s:%d ",
              (int)(parsed.index),
              (parsed.isLocal ? "L" : "G"),
              (int)(parsed.privilegeLevel));

    if (Packos386InterpretSegmentSelector(currentContext->tss.ds,&parsed,error)<0)
      kprintf("Packos386InterpretSegmentSelector(ds: %x): %s\n",
              currentContext->tss.ds,
              PackosErrorToString(*error));
    else
      kprintf("ds: %d:%s:%d ",
              (int)(parsed.index),
              (parsed.isLocal ? "L" : "G"),
              (int)(parsed.privilegeLevel));

    if (Packos386InterpretSegmentSelector(currentContext->tss.es,&parsed,error)<0)
      kprintf("Packos386InterpretSegmentSelector(es: %x): %s\n",
              currentContext->tss.es,
              PackosErrorToString(*error));
    else
      kprintf("es: %d:%s:%d ",
              (int)(parsed.index),
              (parsed.isLocal ? "L" : "G"),
              (int)(parsed.privilegeLevel));

    if (Packos386InterpretSegmentSelector(currentContext->tss.ss,&parsed,error)<0)
      kprintf("Packos386InterpretSegmentSelector(ss: %x): %s\n",
              currentContext->tss.ss,
              PackosErrorToString(*error));
    else
      kprintf("ss: %d:%s:%d ",
              (int)(parsed.index),
              (parsed.isLocal ? "L" : "G"),
              (int)(parsed.privilegeLevel));
  }
  kprintf("\n");

  loadContext(0,currentContext);

  kprintf("PackosKernelLoop(): impossible return\n");
  *error=packosErrorUnknownError;
}

void PackosKernelContextLoopTerminate(PackosError* error)
{
  asm("cli; hlt");
}

PackosContext PackosKernelContextCurrent(PackosError* error)
{
  if (currentContext)
    return currentContext;
  else
    {
      *error=packosErrorNoCurrentContext;
      return 0;
    }
}

void PackosKernelContextYield(void)
{
  PackosError error;

  PackosKernelContextYieldTo(scheduler,&error);
}

bool PackosKernelContextBlocking(PackosContext context,
                                 PackosError* error)
{
  bool res;
  PackosInterruptState state;
  if (!context)
    {
      *error=packosErrorInvalidArg;
      return false;
    }

  state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return false;

  *error=packosErrorNone;
  res=context->os.blocking;

  PackosKernelContextRestore(state,0);

  return res;  
}

void PackosKernelContextSetBlocking(PackosContext context,
                                    bool blocking,
                                    PackosError* error)
{
  if (!context)
    {
      *error=packosErrorInvalidArg;
      return;
    }

  *error=packosErrorNone;
  context->os.blocking=blocking;
}

bool PackosKernelContextFinished(PackosContext context,
                                 PackosError* error)
{
  if (!context)
    {
      *error=packosErrorInvalidArg;
      return false;
    }

  *error=packosErrorNone;
  return context->os.finished;
}

PackosInterruptState PackosKernelContextBlock(PackosError* error)
{
  PackosInterruptState res=0;
  asm("pushfl\n");
  asm("popl %0\n":"=r"(res));
  res>>=9;
  res&=1;
  asm("cli");

  if (error)
    *error=packosErrorNone;
  return res;
}

int PackosKernelContextRestore(PackosInterruptState state,
                               PackosError* error)
{
  if (state)
    asm("sti");
  else
    asm("cli");

  if (error)
    *error=packosErrorNone;

  return 0;
}

void PackosKernelContextExit(void)
{
  PackosError tmp;
  currentContext->os.finished=true;
  currentContext->os.blocking=false;
  PackosPacketUnregisterContext(currentContext,&tmp);
  PackosKernelContextYield();
}

void PackosKernelContextYieldTo(PackosContext to,
                                PackosError* error)
{
  PackosInterruptState state;
  PackosContext from=PackosKernelContextCurrent(error);

  state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return;

#ifdef DEBUG
#ifdef SWAP_VIA_TSS
  kprintf("PackosKernelContextYieldTo(\"%s\":%d); cur==\"%s\":%d, sch==\"%s\":%d, idle==\"%s\":%d\n",
          (to ? to->os.name : ""),(to ? to->os.tssIndex : -1),
          (currentContext ? currentContext->os.name : ""),
          (currentContext ? currentContext->os.tssIndex : -1),
          (scheduler ? scheduler->os.name : ""),
          (scheduler ? scheduler->os.tssIndex : -1),
          (idleContext ? idleContext->os.name : ""),
          (idleContext ? idleContext->os.tssIndex : -1));
#else
  kprintf("PackosKernelContextYieldTo(\"%s\":%p); cur==\"%s\":%p, sch==\"%s\":%p, idle==\"%s\":%p\n",
          (to ? to->os.name : ""),to,
          (currentContext ? currentContext->os.name : ""),currentContext,
          (scheduler ? scheduler->os.name : ""),scheduler,
          (idleContext ? idleContext->os.name : ""),idleContext);
  PRINT_ESP("at entry");
#endif
#endif

  if ((!scheduler) || (scheduler->os.finished))
    {
      kprintf("PackosKernelContextYieldTo(): scheduler %s; halting.\n",
              (scheduler ? "exited" : "missing")
              );
      asm("hlt");
    }

  if ((!to) || (to->os.finished))
    {
      to=scheduler;
      if ((!to) || (to->os.finished))
        {
          kprintf("context.c: nowhere to yield() to\n");
          asm("hlt");
        }

      kprintf("\tyield instead: %p:%s (%s)\n",to,to->os.config->name,
              ((to->os.finished) ? "finished" : "running")
              );
      asm("hlt");
    }

  if (to==from)
    {
      if (to==scheduler)
        {
          to=idleContext;
          kprintf("\tswitching from scheduler to scheduler? make that idle\n");
        }
      else
        {
          kprintf("\tswitching to self? never mind\n");
          PackosKernelContextRestore(state,0);
          return;
        }
    }

  if (false) {
    Packos386SegmentSelectorParsed parsed;
    if (Packos386InterpretSegmentSelector(to->tss.cs,&parsed,error)<0)
      kprintf("Packos386InterpretSegmentSelector(cs: %x): %s\n",
              to->tss.cs,
              PackosErrorToString(*error));
    else
      kprintf("cs: %d:%s:%d ",
              (int)(parsed.index),
              (parsed.isLocal ? "L" : "G"),
              (int)(parsed.privilegeLevel));

    if (Packos386InterpretSegmentSelector(to->tss.ds,&parsed,error)<0)
      kprintf("Packos386InterpretSegmentSelector(ds: %x): %s\n",
              to->tss.ds,
              PackosErrorToString(*error));
    else
      kprintf("ds: %d:%s:%d ",
              (int)(parsed.index),
              (parsed.isLocal ? "L" : "G"),
              (int)(parsed.privilegeLevel));

    if (Packos386InterpretSegmentSelector(to->tss.es,&parsed,error)<0)
      kprintf("Packos386InterpretSegmentSelector(es: %x): %s\n",
              to->tss.es,
              PackosErrorToString(*error));
    else
      kprintf("es: %d:%s:%d ",
              (int)(parsed.index),
              (parsed.isLocal ? "L" : "G"),
              (int)(parsed.privilegeLevel));

    if (Packos386InterpretSegmentSelector(to->tss.ss,&parsed,error)<0)
      kprintf("Packos386InterpretSegmentSelector(ss: %x): %s\n",
              to->tss.ss,
              PackosErrorToString(*error));
    else
      kprintf("ss: %d:%s:%d ",
              (int)(parsed.index),
              (parsed.isLocal ? "L" : "G"),
              (int)(parsed.privilegeLevel));
    kprintf("\n");
  }

#ifdef DEBUG
  PRINT_ESP("before swapContext()");
#endif

  swapContext(from,to);

#ifdef DEBUG
  PRINT_ESP("after swapContext()");
#endif

  {
    bool finished=currentContext->os.finished;
    PackosKernelContextRestore(state,0);
#ifdef DEBUG
    PRINT_ESP("after restore");
#endif
    if (finished)
      PackosContextYield();
  }
}

int PackosKernelContextInsertPacket(PackosContext context,
                                    PackosPacket* packet,
                                    PackosError* error)
{
  PackosInterruptState state;
  if (!(context && packet))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return -1;

  if (context->os.queue.numPackets>=PACKOS_PACKET_QUEUE_LEN)
    {
      PackosKernelContextRestore(state,0);
      *error=packosErrorPacketQueueFull;
      return -1;
    }

  {
    unsigned int i=context->os.queue.offset+context->os.queue.numPackets;
    i%=PACKOS_PACKET_QUEUE_LEN;
    context->os.queue.numPackets++;
    context->os.queue.packets[i]=packet;
  }

  PackosKernelContextRestore(state,0);
  return 0;
}

PackosPacket* PackosKernelContextExtractPacket(PackosContext context,
                                            PackosError* error)
{
  PackosInterruptState state;

  if (!context)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return 0;

  if (!(context->os.queue.numPackets))
    {
      PackosKernelContextRestore(state,0);
      *error=packosErrorNoPacketAvail;
      return 0;
    }

  {
    PackosPacket* res=context->os.queue.packets[context->os.queue.offset++];
    context->os.queue.offset%=PACKOS_PACKET_QUEUE_LEN;
    context->os.queue.numPackets--;
    PackosKernelContextRestore(state,0);
    *error=packosErrorNone;
    return res;
  }
}

bool PackosKernelContextHasPacket(PackosContext context,
                                  PackosError* error)
{
  if (!context)
    {
      *error=packosErrorInvalidArg;
      return false;
    }

  {
    bool res;
    PackosInterruptState state=PackosKernelContextBlock(error);
    if ((*error)!=packosErrorNone) return false;
    res=(context->os.queue.numPackets>0);
    PackosKernelContextRestore(state,0);
    *error=packosErrorNone;
    return res;
  }
}

PackosAddress PackosMyAddress(PackosError* error)
{
  return PackosContextGetAddr(currentContext,error);
}

ContextPageTable* PackosKernelPageTable(void)
{
  static ContextPageTable pageTable={0,0,{0,0}};
  return &pageTable;
}

char* PackosKernelContextGetArena(PackosContext context,
                                  SizeType* size,
                                  PackosError* error)
{
  PackosInterruptState state;

  if (!error) return 0;
  if (!(context))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return 0;

  if (!(context->os.config->arena.data))
    {
      PackosMemoryPhysicalRange physical
        =PackosMemoryPhysicalRangeAlloc(packosMemoryPhysicalTypeRAM,
                                        1<<20,0,
                                        error);
      if (!physical)
        {
          kprintf("PackosKernelContextGetArena(): PackosMemoryPhysicalRangeAlloc(): %s\n",
                  PackosErrorToString(*error));
          PackosKernelContextRestore(state,0);
          return 0;
        }

      context->os.config->arena.logical
        =PackosMemoryLogicalRangeNew(physical,
                                     context,
                                     packosMemoryFlagWritable,
                                     0,0,0,
                                     error);
      if (!(context->os.config->arena.logical))
        {
          PackosError tmp;
          kprintf("PackosKernelContextGetArena(): PackosMemoryLogicalRangeNew(): %s\n",
                  PackosErrorToString(*error));
          PackosMemoryPhysicalRangeFree(physical,&tmp);
          PackosKernelContextRestore(state,0);
          return 0;
        }

      context->os.config->arena.data
        =PackosMemoryLogicalRangeGetAddr(context->os.config->arena.logical,error);
      if (!(context->os.config->arena.data))
        {
          PackosError tmp;
          kprintf("PackosKernelContextGetArena(): PackosMemoryLogicalRangeGetAddr(): %s\n",
                  PackosErrorToString(*error));
          PackosMemoryLogicalRangeFree(context->os.config->arena.logical,context,&tmp);
          context->os.config->arena.logical=0;
          PackosMemoryPhysicalRangeFree(physical,&tmp);
          PackosKernelContextRestore(state,0);
          return 0;
        }

      context->os.config->arena.size
        =PackosMemoryLogicalRangeGetLen(context->os.config->arena.logical,error);
      if ((context->os.config->arena.size==0) || ((*error)!=packosErrorNone))
        {
          PackosError tmp;
          kprintf("PackosKernelContextGetArena(): PackosMemoryLogicalRangeGetLen(): %s\n",
                  PackosErrorToString(*error));
          PackosMemoryLogicalRangeFree(context->os.config->arena.logical,context,&tmp);
          context->os.config->arena.logical=0;
          context->os.config->arena.data=0;
          PackosMemoryPhysicalRangeFree(physical,&tmp);
          PackosKernelContextRestore(state,0);
          return 0;
        }
    }

  if (size)
    (*size)=context->os.config->arena.size;

  {
    void* res=context->os.config->arena.data;
    PackosKernelContextRestore(state,0);
    return res;
  }
}

void* PackosKernelContextGetIfaceRegistry(PackosContext context,
                                          PackosError* error)
{
  void* res;
  PackosInterruptState state;

  if (!context)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return 0;

  res=context->os.config->state->ifaceRegistry;
  *error=packosErrorNone;
  PackosKernelContextRestore(state,0);
  return res;
}

int PackosKernelContextSetIfaceRegistry(PackosContext context,
                                     void* ifaceRegistry,
                                     void* oldIfaceRegistry,
                                     PackosError* error)
{
  PackosInterruptState state;

  if (!context)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return -1;

  if (context->os.config->state->ifaceRegistry!=oldIfaceRegistry)
    {
      PackosKernelContextRestore(state,0);
      *error=packosErrorValueChanged;
      return -1;
    }

  context->os.config->state->ifaceRegistry=ifaceRegistry;
  *error=packosErrorNone;
  PackosKernelContextRestore(state,0);
  return 0;
}

const char* PackosContextGetOwnName(PackosError* error)
{
  return PackosContextGetName(currentContext,error);
}

#ifdef DEBUG
static void dumpTSS(const PackosContext386TSS* tss)
{
  const char* bytes=(const char*)tss;
  int i=0;
  const int N=sizeof(PackosContext386TSS);
  kprintf("TSS:\n");
  while (i<N)
    {
      int j;
      kprintf("%s%x:",((i<16) ? "0" : ""),i);
      for (j=0; (j<16) && (i<N); j++, i++)
        {
          unsigned int b=((unsigned int)(bytes[i]))&0xff;
          kprintf(" %s%x",((b<16) ? "0" : ""),b);
        }
      kprintf("\n");
    }
}

#ifdef SWAP_VIA_TSS
static void dumpTSSByIndex(uint16_t tssIndex)
{
  PackosError error;
  uint16_t numDescriptors;
  const Packos386SegmentDescriptor* descriptors
    =Packos386GetDescriptorTable(&numDescriptors);
  if (tssIndex>=numDescriptors)
    {
      kprintf("tssIndex>=numDescriptors\n");
      return;
    }

  const Packos386SegmentDescriptor* tssDesc=descriptors+tssIndex;
  Packos386SegmentDescriptorSane sane;
  if (Packos386InterpretSegmentDescriptor(tssDesc,&sane,&error)<0)
    {
      kprintf("Packos386InterpretSegmentDescriptor(): %s\n",
              PackosErrorToString(error));
      return;
    }

  dumpTSS((const PackosContext386TSS*)(sane.base));
}
#endif

static void dumpStack(const void* esp_)
{
  const char* esp=(const char*)esp_;
  int i;
  for (i=0; i<32; i++)
    kprintf(" %x",((unsigned int)(esp[i]))&0xff);
  kprintf("\n");
}
#endif

const PackosContextConfig* PackosContextConfigGet(PackosError* error)
{
  const PackosContextConfig* res;

  PackosInterruptState state=PackosKernelContextBlock(error);
  if ((*error)!=packosErrorNone) return 0;

  res=currentContext->os.config;

  PackosKernelContextRestore(state,0);
  *error=packosErrorNone;
  return res;
}
