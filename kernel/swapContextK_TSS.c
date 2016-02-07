#include "swapContextK.h"
#include <packos/sys/contextP.h>
#include "kprintfK.h"
#include "pagingK.h"

//#define DEBUG

#ifdef SWAP_VIA_TSS
extern PackosContext currentContext;

void loadContext(PackosContext dummy,
                 PackosContext context)
{
#ifdef DEBUG
  {
    void* espActual;
    asm("movl %%esp, %0": "=m"(espActual));
    kprintf("TSS/loadContext(_,%p:\"%s\"): ESP %p\n",
            context,context->os.name,
            espActual);
  }
#endif

  PackosError error;
  if (context->os.tssIndex<0)
    {
      context->os.tssIndex=Packos386AllocateDescriptor(&error);
      if (context->os.tssIndex<0)
        {
          kprintf("loadContext(): Packos386AllocateDescriptor(): %s\n",
                  PackosErrorToString(error));
          return;
        }

      Packos386SegmentDescriptorSane sane;
      sane.base=&(context->tss);
      sane.size=sizeof(*context);
      sane.inPages=false;
      sane.availableForOS=false;
      sane.present=true;
      sane.type=packos386DescriptorTypeTSS32;
      sane.privilegeLevel=3;
      sane.u.tss.busy=0;

      uint16_t numDescriptors;
      Packos386SegmentDescriptor* descriptors
        =Packos386GetDescriptorTable(&numDescriptors);
      Packos386SegmentDescriptor* descriptor
        =descriptors+context->os.tssIndex;
      
      if (Packos386BuildSegmentDescriptor(&sane,descriptor,&error)<0)
        {
          kprintf("loadContext(): Packos386BuildSegmentDescriptor(): %s\n",
                  PackosErrorToString(error));
          context->os.tssIndex=-1;
          return;
        }
      //Packos386DumpGDT(false,&error);

      Packos386SetGDT(descriptors,numDescriptors,&error);
#ifdef DEBUG
      Packos386DumpDescriptorByIndex(context->os.tssIndex,&error);
#endif
    }

#ifdef DEBUG
  {
    void* esp;
    asm("movl %%esp, %0": "=m"(esp));
    kprintf("loadContext(%s => %s): cur ESP %p; func %p; ESP %p, EIP %p\n",
            currentContext->os.name,
            context->os.name,
            esp,
            context->os.func,
            context->tss.esp,context->tss.eip);
  }
#endif

  if (!(context->tss.eip))
    {
      kprintf("dest EIP is null; halting\n");
      asm("cli; hlt;");
    }

#ifdef DEBUG
  PackosContext from=currentContext;
#endif
  currentContext=context;

 {
   static struct {
     unsigned int offset:32;
     unsigned int seg:16;
   } dest;

   dest.seg=(uint16_t)((context->os.tssIndex<<3)|3);
   dest.offset=0;

#ifdef DEBUG
   kprintf("loadContext(%s): leaving %s\n",
           context->os.name,from->os.name);
#endif
   asm("ljmp *%0": : "m"(dest));
#ifdef DEBUG
 {
   void* espFromTSS=(void*)(currentContext->tss.esp);
   void* espActual;
    asm("movl %%esp, %0": "=m"(espActual));
   kprintf("loadContext(%p): we're back to %s; esp %p (TSS says %p):\n",
           context,currentContext->os.name,
           espActual,espFromTSS);
   /*dumpTSSByIndex(currentContext->os.tssIndex);*/
   /*dumpStack(esp);*/
 }
#endif
 }
}

void swapContext(PackosContext from,
                 PackosContext to)
{
#ifdef DEBUG
  {
    void* espActual;
    asm("movl %%esp, %0": "=m"(espActual));
    kprintf("TSS/swapContext(%p:%s,%p:%s): ESP %p\n",
            from,from->os.name,to,to->os.name,
            espActual);
  }
#endif

  uint16_t numDescriptors;
  Packos386SegmentDescriptor* descriptors
    =Packos386GetDescriptorTable(&numDescriptors);

  if (from->os.tssIndex<0)
    {
      PackosError error;
      from->os.tssIndex=Packos386AllocateDescriptor(&error);
      if (from->os.tssIndex<0)
        {
          kprintf("swapContext(): Packos386AllocateDescriptor(): %s\n",
                  PackosErrorToString(error));
          return;
        }

      Packos386SegmentDescriptorSane sane;
      sane.base=&(from->tss);
      sane.size=sizeof(*from);
      sane.inPages=false;
      sane.availableForOS=false;
      sane.present=true;
      sane.type=packos386DescriptorTypeTSS32;
      sane.privilegeLevel=0;
      sane.u.tss.busy=0;

      Packos386SegmentDescriptor* descriptor=descriptors+from->os.tssIndex;
      
      if (Packos386BuildSegmentDescriptor(&sane,descriptor,&error)<0)
        {
          kprintf("swapContext(): Packos386BuildSegmentDescriptor(): %s\n",
                  PackosErrorToString(error));
          from->os.tssIndex=-1;
          return;
        }
      Packos386SetGDT(descriptors,numDescriptors,&error);
    }

  loadContext(0,to);
}
#endif
