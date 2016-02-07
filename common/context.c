#include <packos/context.h>
#include <packos/sys/contextP.h>
#include <packos/sys/blockP.h>
#include <packos/sys/packetP.h>
#include <packos/sys/memoryP.h>
#include <packos/sys/interruptsP.h>
#include <packos/sys/asm.h>
#include <packos/sys/entry-points.h>

#include <util/string.h>
#include <util/printf.h>
#include "../kernel/kprintfK.h"
#include "../kernel/kputcK.h"

void* UtilMemset(void* s_, int c, SizeType n)
{
  char* s=(char*)s_;
  while (n)
    s[--n]=c;
  return s_;
}

typedef struct {
  struct PackosContext context;
  bool inUse;
  int next,prev;
} ContextInPool;

ContextInPool pool[PACKOS_KERNEL_PACKET_MAXADDRS];
static bool poolInited=false;
static int poolFirst=-1;

static PackosContext poolAlloc(PackosError* error)
{
  PackosInterruptState state=PackosKernelContextBlock(error);

  asm("poolAlloc0:");
  if (!poolInited)
    {
  asm("poolAlloc1:");
      int i,N;
      for (i=0, N=sizeof(pool)/sizeof(pool[0]); i<N; i++)
        {
          pool[i].inUse=false;
          pool[i].next=i+1;
          pool[i].prev=i-1;
        }
      pool[N-1].next=-1;
      poolFirst=0;
      poolInited=true;
    }

  if (poolFirst<0)
    {
      if (false) kprintf("poolAlloc(): poolFirst<0\n");
      PackosKernelContextRestore(state,0);
      *error=packosErrorOutOfContexts;
      return 0;
    }

  {
    int resIndex=poolFirst;
    poolFirst=pool[resIndex].next;
    pool[resIndex].prev=-1;
    pool[resIndex].next=-1;
    pool[resIndex].inUse=true;
    pool[poolFirst].prev=-1;
    PackosKernelContextRestore(state,0);
    return &(pool[resIndex].context);
  }
}

static int poolFree(PackosContext context,
                    PackosError* error)
{
  PackosInterruptState state=PackosKernelContextBlock(error);

  ContextInPool* cip=(ContextInPool*)context;
  int i=cip-pool;
  if ((!(cip->inUse)) || (i<0) || (i>=PACKOS_KERNEL_PACKET_MAXADDRS))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  cip->inUse=false;
  cip->next=poolFirst;
  cip->prev=-1;
  poolFirst=i;

  PackosKernelContextRestore(state,0);
  return 0;
}

static void wrapper(void)
{
  PackosContextFunc func;

#if 0
  {
    PackosError error;
    func=PackosGetCurrentContext(&error)->os.func;
  }
#else
  asm("movl %%eax, %0": "=m"(func));
#endif

  asm("wrapperCallFunc:");
  func();
  asm("wrapperAfterFunc:");

  PackosContextExit();
}

PackosContext PackosContextNew(PackosContextFunc func,
                               const char* name,
                               PackosError* error)
{
  extern PackosContext currentContext;
  if (!error) return 0;

  bool resIsScheduler=(!currentContext);

  PackosContext res=poolAlloc(error);
  if (!res)
    {
      /*kputs("poolAlloc failed\n");*/
      if (false) kprintf("PackosContextNew(): !res: %s\n",PackosErrorToString(*error));
      return 0;
    }

  res->os.configPhysical
    =PackosMemoryPhysicalRangeAlloc(packosMemoryPhysicalTypeRAM,
                                    sizeof(PackosContextConfig),
                                    PACKOS_PAGE_SIZE,
                                    error);
  if (!(res->os.configPhysical))
    {
      PackosError tmp;
      poolFree(res,&tmp);
      return 0;
    }

  res->os.configLogical=PackosMemoryLogicalRangeNew
    (res->os.configPhysical,res,0,0,0,0,error);
  if (!(res->os.configLogical))
    {
      PackosError tmp;
      PackosMemoryPhysicalRangeFree(res->os.configPhysical,&tmp);
      poolFree(res,&tmp);
      return 0;
    }

  res->os.config
    =PackosMemoryLogicalRangeGetAddr(res->os.configLogical,error);
  if (!(res->os.config))
    {
      PackosError tmp;
      PackosMemoryLogicalRangeFree(res->os.configLogical,res,&tmp);
      PackosMemoryPhysicalRangeFree(res->os.configPhysical,&tmp);
      poolFree(res,&tmp);
      return 0;
    }

  res->os.config->arena.data=0;
  res->os.config->arena.logical=0;
  res->os.config->arena.size=0;

  if (name)
    {
      int i;

      for (i=0; name[i] && i<=PACKOS_CONTEXT_MAX_NAMELEN; i++)
        res->os.config->name[i]=name[i];
      if (name[i])
        {
          PackosError tmp;
          /*kputs("PackosContextNew(): name too long\n");*/
          PackosMemoryLogicalRangeFree(res->os.configLogical,res,&tmp);
          PackosMemoryPhysicalRangeFree(res->os.configPhysical,&tmp);
          poolFree(res,&tmp);
          *error=packosErrorContextNameTooLong;
          return 0;
        }
    }
  else
    res->os.config->name[0]=0;

  UtilMemset(&(res->ioBitmap),0,sizeof(res->ioBitmap));
  UtilMemset(&(res->tss),0,sizeof(res->tss));

  res->os.pageTable.first=res->os.pageTable.last=0;
  res->os.pageTable.segmentIds.min=res->os.pageTable.segmentIds.max=0;

  {
    PackosMemoryPhysicalRange physical
      =PackosMemoryPhysicalRangeAlloc(packosMemoryPhysicalTypeRAM,
                                      PACKOS_CONTEXT_STACK_SIZE,4096,
                                      error);
    if (!physical)
      {
        PackosError tmp;
        if (false) kprintf("PackosContextNew(): !physical: %s\n",
                           PackosErrorToString(*error));
        PackosMemoryLogicalRangeFree(res->os.configLogical,res,&tmp);
        PackosMemoryPhysicalRangeFree(res->os.configPhysical,&tmp);
        poolFree(res,&tmp);
        return 0;
      }

    res->os.stackLogical=PackosMemoryLogicalRangeNew
      (physical,res,packosMemoryFlagWritable,0,0,0,error);

    if (!(res->os.stackLogical))
      {
        PackosError tmp;
        if (false) kprintf("PackosContextNew(): !(res->os.stackLogical): %s\n",
                           PackosErrorToString(*error));
        PackosMemoryPhysicalRangeFree(physical,&tmp);
        PackosMemoryLogicalRangeFree(res->os.configLogical,res,&tmp);
        PackosMemoryPhysicalRangeFree(res->os.configPhysical,&tmp);
        poolFree(res,&tmp);
        return 0;
      }
  }

  {
    uint32_t stackLen
      =PackosMemoryLogicalRangeGetLen(res->os.stackLogical,error);
    if (stackLen<PACKOS_CONTEXT_STACK_SIZE)
      {
        PackosError tmp;
        if (false) kprintf("PackosContextNew(): stack too small\n");
        PackosMemoryLogicalRangeFree(res->os.configLogical,res,&tmp);
        PackosMemoryPhysicalRangeFree(res->os.configPhysical,&tmp);
        poolFree(res,error);
        *error=packosErrorUnknownError;
        return 0;
      }
  }

  byte* stackBase
    =(byte*)(PackosMemoryLogicalRangeGetAddr(res->os.stackLogical,error));
  if (!stackBase)
    {
      PackosError tmp;
      if (false) kprintf("PackosContextNew(): !stackBase: %s\n",
                         PackosErrorToString(*error));
      PackosMemoryLogicalRangeFree(res->os.configLogical,res,&tmp);
      PackosMemoryPhysicalRangeFree(res->os.configPhysical,&tmp);
      poolFree(res,&tmp);
      return 0;
    }

  asm("PackosContextNewAfterStackBase:");

  res->tss.esp=(uint32_t)(uint32_t*)(stackBase+PACKOS_CONTEXT_STACK_SIZE);

  res->tss.ss=res->os.stackLogical->segmentId;

  {
    uint32_t* s=(uint32_t*)(res->tss.esp);
    res->os.func=func;

    asm("PackosContextNewPushWrapper:");

    /* Address of wrapper(), to which to jump on loadContext */
    *(--s)=(uint32_t)wrapper;

    asm("PackosContextNewPushFlags:");

    /* flags */
    {
      uint32_t flags;
      asm("pushfl");
      asm("popl %0\n":"=r"(flags));
      res->tss.eflags=flags;
    }

    {
      uint16_t ds;
      asm("movw %%ds, %0": "=m"(ds));
      res->tss.ds=ds;
    }
    {
      uint16_t cs;
      asm("movw %%cs, %0": "=m"(cs));
      res->tss.cs=cs;
    }

    res->tss.es=res->tss.fs=res->tss.gs=res->tss.ds;
    res->tss.ss=res->tss.ds;
    res->tss.esp=(uint32_t)s;
  }

  res->tss.eip=(uint32_t)wrapper;

  res->tss.eax=(uint32_t)func;

  res->tss.ss1=res->tss.ss2=0;
  res->tss.esp0=res->tss.esp1=res->tss.esp2=0;
  {
    extern uint16_t PackosExceptionSS;
    extern void* PackosExceptionESP;
    res->tss.ss0=PackosExceptionSS;
    res->tss.esp0=(uint32_t)PackosExceptionESP;
  }
  res->tss.debugTrap=false;
  res->tss.ldt=0;
  res->tss.ioMapOffset=(((char*)(&(res->ioBitmap)))-((char*)res));

  res->os.tssIndex=-1;
  res->os.blocking=false;
  res->os.finished=false;
  res->os.config->addresses.self=PackosAddressGenerate(error);

  asm("PackosContextNewBeforeSchedulerAddr:");

  /*kprintf("PackosContextNewBeforeSchedulerAddr\n");*/
  {
    PackosContext scheduler=(resIsScheduler ? res : currentContext);
    asm("PackosContextNewInSchedulerAddr1:");
    /*kprintf("PackosContextNewInSchedulerAddr1: scheduler==0x%x[0x%x], res==0x%x[0x%x]\n",
            scheduler,scheduler->os.config,
            res,res->os.config);*/
    res->os.config->addresses.scheduler=scheduler->os.config->addresses.self;
  }

  asm("PackosContextNewAfterSchedulerAddr:");

  res->os.config->state=&(res->os.config->stateConst);
  res->os.config->state->ifaceRegistry=0;
  res->os.queue.offset=res->os.queue.numPackets=0;

  {
    uint16_t cpl;
    asm("movw %%cs, %0": "=m"(cpl));
    cpl&=3;

    if (PackosPacketRegisterContext(res,error)<0)
      {
        PackosError tmp;
        if (false) kprintf("PackosContextNew(): PackosPacketRegisterContext(): %s\n",PackosErrorToString(*error));
        PackosMemoryLogicalRangeFree(res->os.configLogical,res,&tmp);
        PackosMemoryPhysicalRangeFree(res->os.configPhysical,&tmp);
        poolFree(res,&tmp);
        return 0;
      }
  }

  {
    char buff[80];
    const char* addrStr;
    if (PackosAddrToString(res->os.config->addresses.self,buff,sizeof(buff),error)<0)
      addrStr="<?>";
    else
      addrStr=buff;
    if (false) kprintf("PackosContextNew(): %s/%s\n",res->os.config->name,addrStr);
  }

  asm("PackosContextNewAtEnd:");

  return res;
}

PackosContext PackosGetCurrentContext(PackosError* error)
{
  extern PackosContext currentContext;
  return currentContext;
}

const char* PackosContextGetName(PackosContext context,
                                 PackosError* error)
{
  if (!context)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  if (context->os.config->name[0])
    return context->os.config->name;
  else
    return "<Unnamed>";
}

PackosAddress PackosContextGetAddr(PackosContext context,
                                   PackosError* error)
{
  if (!context)
    {
      *error=packosErrorInvalidArg;
      return PackosAddrGetZero();
    }

  *error=packosErrorNone;
  return context->os.config->addresses.self;
}

int PackosContextSetMetadata(PackosContext context,
                             void* metadata,
                             PackosError* error)
{
  if (!error) return -2;
  if (!context)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  context->os.metadata=metadata;
  return 0;
}

void* PackosContextGetMetadata(PackosContext context,
                               PackosError* error)
{
  if (!error) return 0;
  if (!context)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return context->os.metadata;
}

void PackosContextYield(void)
{
  PackosKernelContextYield();
}

void PackosContextExit(void)
{
  PackosKernelContextExit();
}

int PackosSetIdleContext(PackosContext idleContext,
                         PackosError* error)
{
  return PackosKernelSetIdleContext(idleContext,error);
}

void PackosContextYieldTo(PackosContext context,
                          PackosError* error)
{
  PackosKernelContextYieldTo(context,error);
}

bool PackosContextBlocking(PackosContext context,
                           PackosError* error)
{
  return PackosKernelContextBlocking(context,error);
}

void PackosContextSetBlocking(PackosContext context,
                              bool blocking,
                              PackosError* error)
{
  PackosKernelContextSetBlocking(context,blocking,error);
}

bool PackosContextFinished(PackosContext context,
                           PackosError* error)
{
  return PackosKernelContextFinished(context,error);
}
