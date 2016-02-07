#include "main.h"
#include "kprintfK.h"
#include "kputcK.h"
#include "pagingK.h"

#include <packos/sys/memoryP.h>
#include <packos/sys/contextP.h>

extern void testMain(void);

extern void noop_handler(void);

uint16_t PackosExceptionSS=0;
void* PackosExceptionESP=0;

#define IA32_SYSENTER_CS 0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176

static byte PackosKernelExceptionStack[1<<16];

/** Is i in the half-open interval [low,high)? */
static inline bool inRange(uint32_t i, uint32_t low, uint32_t high)
{
  return (low<=i) && (i<high);
}

static inline void wrmsr(uint32_t msrId, uint32_t high, uint32_t low)
{
  asm("movl %0, %%edx": :"o"(high));
  asm("movl %0, %%ecx": :"o"(msrId));
  asm("movl %0, %%eax": :"o"(low));
  asm("wrmsr");
}

static void setIDTEntry(uint8_t id,
                        void (*handler)(void))
{
  byte* base=(byte*)(8*id);
  uint32_t cs;
  byte cs_high,cs_low;
  uint32_t handler_int=(uint32_t)(handler);
  byte handler_bytes[4];

  handler_bytes[0]=handler_int&0xff;
  handler_bytes[1]=(handler_int>>8)&0xff;
  handler_bytes[2]=(handler_int>>16)&0xff;
  handler_bytes[3]=(handler_int>>24)&0xff;

  asm("movl %cs, %eax\n");
  asm("movl %%eax, %0\n":"=r"(cs));

  cs_high=cs>>8;
  cs_low=cs&0xff;

  base[0]=handler_bytes[0];
  base[1]=handler_bytes[1];
  base[6]=handler_bytes[2];
  base[7]=handler_bytes[3];
  base[2]=cs_low;
  base[3]=cs_high;
  base[4]=0;
  base[5]=0x8e;
}

static void clearIDT(void)
{
  int i;

  for (i=0; i<256; i++)
    setIDTEntry(i,noop_handler);
}

static void gpf(uint32_t error, uint16_t cs, void* addr)
{
  Packos386SegmentSelectorParsed parsed;
  PackosError tmp;
  Packos386InterpretSegmentSelector(error,&parsed,&tmp);
  kprintf("gpf: error %x (%d:%s:%d) at %x:%x\n",
          error,
          parsed.index,(parsed.isLocal ? "L" : "G"),parsed.privilegeLevel,
          (int)cs,addr);

#ifdef SWAP_VIA_TSS
  Packos386DumpGDT(false,&tmp);
#endif

  if (error)
    {
      uint16_t index=error>>3;
      if (Packos386DumpDescriptorByIndex(index,&tmp)<0)
        {
          kprintf("Packos386DumpDescriptorByIndex(%d): %s\n",
                  index,
                  PackosErrorToString(tmp));
#ifdef SWAP_VIA_TSS
          //Packos386DumpGDT(false,&tmp);
#endif
        }
      else
        {
          if (Packos386DumpDescriptorByIndex(cs>>3,&tmp)<0)
            {
              kprintf("Packos386DumpDescriptorByIndex(%d): %s\n",
                      cs>>3,
                      PackosErrorToString(tmp));
#ifdef SWAP_VIA_TSS
              //Packos386DumpGDT(false,&tmp);
#endif
            }
        }

#ifdef SWAP_VIA_TSS
      {
        const Packos386SegmentDescriptor* desc
          =Packos386GetGDTDescriptor(index,&tmp);
        if (!desc)
          {
            kprintf("Packos386GetGDTDescriptor(%d): %s\n",
                    index,
                    PackosErrorToString(tmp));
          }
        else
          {
            Packos386SegmentDescriptorSane sane;
            if (Packos386InterpretSegmentDescriptor(desc,&sane,&tmp)<0)
              kprintf("Packos386InterpretSegmentDescriptor(): %s\n",
                      PackosErrorToString(tmp));
            else
              {
                if ((sane.type==packos386DescriptorTypeTSS32)
                    && (sane.u.tss.busy)
                    )
                  kprintf("Looks like I tried to switch to a busy task.\n");
              }
          }
      }
#endif
    }

  asm("hlt");
}

static void invalidOpCode(uint16_t cs, void* addr)
{
  kprintf("invalid op code: at %x:%x\n",(int)cs,addr);
  asm("hlt");
}

static void parport2(void)
{
  kprintf("parport2: oops\n");
  asm("hlt");
}

static void pageFault(uint32_t error,
                      uint32_t cs,
                      void* eip,
                      void* addr)
{
  kprintf("page fault on 0x%x at IP 0x%x:0x%x:",addr,cs,eip);
  if (error & 1) {
    kprintf(" protection violation");
  } else {
    kprintf(" page not present");
  }

  kprintf(", on ");
  
  if (error & 16) {
    kprintf("instruction");
  } else {
    kprintf("data");
  }

  if (error & 2) {
    kprintf(" write");
  } else {
    kprintf(" read");
  }

  if (error & 4) {
    kprintf(", in user mode");
  } else {
    kprintf(", in kernel mode");
  }

  if (error & 8) {
    kprintf(", reserved bits set");
  }

  asm("cli; hlt");
}

void packosKernelMiscHandlerWithError(uint32_t irq,
                                      uint32_t eflags, uint32_t error,
                                      uint32_t cs, void* addr)
{
  bool eflags_if=(eflags & (1<<9))!=0;
  kprintf("IRQ #%d: eflags %x, eflags[IF] %d, error %x\n",
          irq,eflags,(int)eflags_if,error);
  switch (irq)
    {
    case 13:
      if (true || eflags_if)
        gpf(error,cs,addr);
      else
        parport2();
      break;

    case 6:
      invalidOpCode(cs,addr);
      break;

    case 14:
      pageFault(error,cs,addr,(void*)(Packos386GetCR2()));
      break;

    default:
      kprintf("Unknown IRQ\n");
      asm("hlt");
    }
}

void packosKernelMiscHandlerNoError(uint32_t irq,
                                    uint32_t eflags,
                                    uint32_t cs, void* addr)
{
  packosKernelMiscHandlerWithError(irq,eflags,0,cs,addr);
}

asmlinkage void kernelMain(void)
{
  PackosError error;

  kprintf("Welcome to PackOS.\n");

  extern void init_8259s(void);
  extern void init_clock(void);
  extern void tick(void);
  extern void PackosKernelIRQ10HandlerS(void);
  extern void PackosKernelIRQ16HandlerS(void);
  extern void packosKernelGPFHandler(void);
  extern void packosKernelInvalidOpCodeHandler(void);
  extern void packosKernelPageFaultHandler(void);

  uint32_t physicalRangeBase=2*1024*1024;
  uint32_t physicalRangeSize=64*1024*1024;

  if (PackosMemoryInit(&error)<0)
    {
      kprintf("PackosMemoryInit(): %s\n",
              PackosErrorToString(error));
      return;
    }

  if (PackosMemoryPhysicalRangeDefine((void*)physicalRangeBase,
                                      packosMemoryPhysicalTypeRAM,
                                      physicalRangeSize,
                                      &error)<0)
    {
      kprintf("PackosMemoryPhysicalRangeDefine(): %s\n",
              PackosErrorToString(error));
      return;
    }


  {
    static Packos386SegmentDescriptorSane sane[]={
      {base: 0, size: 0xffffff,
       inPages: true, accessed: false, availableForOS: false, present: true,
       type: packos386DescriptorTypeCode, privilegeLevel: 0,
       u: {code: {is32Bit: true, isReadableAsData: true, conforming: true}},
       metadata: {usage: packos386DescriptorUsageNone}
      },
      {base: 0, size: 0xffffff,
       inPages: true, accessed: false, availableForOS: false, present: true,
       type: packos386DescriptorTypeData, privilegeLevel: 0,
       u: {data: {isWritable: true, stack: {big: true, expandsDown: false}}},
       metadata: {usage: packos386DescriptorUsageNone}
      },
      {base: 0, size: 0xffffff,
       inPages: true, accessed: false, availableForOS: false, present: true,
       type: packos386DescriptorTypeData, privilegeLevel: 0,
       u: {data: {isWritable: true, stack: {big: true, expandsDown: false}}},
       metadata: {usage: packos386DescriptorUsageExceptionStack}
      },
    };

    uint16_t gdtSize,descriptorsSize;
    Packos386SegmentDescriptor* descriptors
      =Packos386GetDescriptorTable(&descriptorsSize);
    const Packos386SegmentDescriptor* gdt=Packos386GetGDT(&gdtSize);
    //Packos386DumpGDT(false,&error);
    kputc('\n');

    int i;
    for (i=1; i<gdtSize; i++)
      {
        uint32_t* toQuads=(uint32_t*)(descriptors+i);
        uint32_t* fromQuads=(uint32_t*)(gdt+i);
        toQuads[0]=fromQuads[0];
        toQuads[1]=fromQuads[1];
      }

    for (; i<descriptorsSize; i++)
      {
        uint32_t* quads=(uint32_t*)(descriptors+i);
        quads[1]=quads[0]=0;
      }

    int N=sizeof(sane)/sizeof(sane[0]);
    for (i=0; i<N; i++)
      {
        switch (sane[i].metadata.usage)
          {
          case packos386DescriptorUsageNone:
          case packos386DescriptorUsageUserDS:
            break;

          case packos386DescriptorUsageExceptionStack:
            PackosExceptionSS=(gdtSize+i)<<3;
            PackosExceptionESP
              =PackosKernelExceptionStack+sizeof(PackosKernelExceptionStack)-4;
            break;
          }

        if (Packos386BuildSegmentDescriptor
            (sane+i,descriptors+gdtSize+i,&error)
            <0
            )
          {
            kprintf("Packos386BuildSegmentDescriptor([%d]): %s\n",
                    i,
                    PackosErrorToString(error));
            return;
          }
      }

    if (Packos386SetGDT(descriptors,descriptorsSize,&error)<0)
      {
        kprintf("Packos386SetGDT(): %s\n",
                PackosErrorToString(error));
        return;
      }
    Packos386DumpDescriptors(descriptors,descriptorsSize,false,&error);

    Packos386RegisterMaxDescriptor(N+gdtSize,&error);
  }
  kprintf("exception stack: %x:%x\n",
          PackosExceptionSS,PackosExceptionESP);

  init_8259s();

  clearIDT();

  init_clock();

  kprintf("init_clock() done\n");

  setIDTEntry(0x20,tick);

  {
    extern void enable_irq_10(void);
    setIDTEntry(10,PackosKernelIRQ10HandlerS);
    enable_irq_10();
  }

  /* Not really good enough, because we don't cope with IRQ sharing
   *  properly (if we get a parport2 IRQ, the stack layout is different;
   *  I don't know how to tell the difference).  Assume for the moment
   *  that we're not going to bother with parallel ports.
   */
  setIDTEntry(13,packosKernelGPFHandler);

  setIDTEntry(14,packosKernelPageFaultHandler);

  setIDTEntry(6,packosKernelInvalidOpCodeHandler);

  asm("sti\n");

  testMain();
}

