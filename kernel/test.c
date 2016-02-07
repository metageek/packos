#include <packos/interrupts.h>
#include <packos/memory.h>
#include <packos/sys/contextP.h>
#include "kprintfK.h"

#define N (10)

void lidt(void* base, unsigned int limit) {
   unsigned int i[2];

   i[0] = limit << 16;
   i[1] = (unsigned int) base;
   asm ("lidt (%0)": :"p" (((char *) i)+2));
}

void* sidt(uint32_t* numInts) {
    uint16_t ptr[3];
    asm ("sidt (%0)": :"p" (((char *) ptr)));
    if (numInts)
      (*numInts)=ptr[0];

    {
      uint32_t* resIntP=(uint32_t*)(ptr+1);
      uint32_t resInt=*resIntP;
      return (void*)resInt;
    }
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
  extern void noop_handler(void);
  int i;

  for (i=0; i<256; i++)
    setIDTEntry(i,noop_handler);
}

static void f(void)
{
  kprintf("f()\n");

  PackosError error;
  const char* name=PackosContextGetOwnName(&error);
  int id=name[0]-'0';

  int i;
  for (i=0; i<N; i++)
    {
      kprintf("[c%d: %d]",id,i);

      asm("f2:");

      {
        int i;
        for (i=0; i<1000000; i++)
          {
            asm("f2a:");
          }
      }
      asm("f3:");
    }
}

static void idle(void)
{
  while (1);
}

static void scheduler(void)
{
  PackosContext c[N];

  kprintf("scheduler()\n");

  PackosError error;
  int i,numFinished=0;
  bool finished[N];
  const uint16_t clockPort=7000;

  asm("schedulerMakeCs:");

  kprintf("scheduler creating contexts:");

  {
    int i;
    for (i=0; i<N; i++)
      {
        char name[2]={'0'+i,0};
        asm("schedulerBeforePackosContextNew:");
        {
          PackosContext cur=PackosContextNew(f,name,&error);
          c[i]=cur;
        }
        asm("schedulerAfterPackosContextNew:");
        if (!(c[i]))
          {
            kprintf("PackosContextNew(c[%d]): %s\n",
                    i,PackosErrorToString(error));
            return;
          }

        kprintf(" %d",i);
      }
  }
  kprintf("\n");

  asm("schedulerFillFinished:");

  for (i=0; i<N; i++)
    finished[i]=false;

  asm("schedulerGetClockId:");
  PackosInterruptId clockId
    =PackosInterruptAliasLookup(PACKOS_INTERRUPT_ALIAS_CLOCK,&error);
  if (PackosInterruptRegisterFor(clockId,clockPort,&error)<0)
    {
      kprintf("PackosInterruptRegisterFor: %s\n",
              PackosErrorToString(error));
      return;
    }

  asm("schedulerStartLoop:");
  for (i=0; numFinished<N; i=(i+1)%N)
    {
      asm("schedulerCheckFinished:");
      if (finished[i]) continue;

      asm("schedulerCallFinished:");
      if (PackosContextFinished(c[i],&error))
        {
          asm("schedulerYesFinished:");
          numFinished++;
          finished[i]=true;
          continue;
        }

      {
        PackosPacket* packet;
        asm("schedulerCallReceive:");
        packet=PackosPacketReceiveOrYieldTo(c[i],&error);
        if (packet)
          {
            asm("schedulerFreePacket:");
            PackosPacketFree(packet,&error);
            kprintf("got a tick\n");
          }
        else
          {
            if (error==packosErrorContextYieldedBack)
              {
                if (PackosContextFinished(c[i],&error))
                  {
                    numFinished++;
                    finished[i]=true;
                    continue;
                  }
              }

            kprintf("PackosPacketReceive(tickSocket): %s\n",
                    PackosErrorToString(error));
            break;
          }
      }
    }

  kprintf("scheduler exiting; numFinished==%d\n",numFinished);
}

void testMain(void)
{
  PackosContext schedulerContext;
  PackosError error;

  extern void init_8259s(void);
  extern void init_clock(void);
  extern void tick(void);

  kprintf("kernel testMain()\n");

  {
    PackosContext idleContext=PackosContextNew(idle,"idle",&error);
    if (!idleContext)
      {
        kprintf("PackosContextNew(idle): %s\n",
                PackosErrorToString(error));
        return;
      }

    if (PackosKernelSetIdleContext(idleContext,&error)<0)
      {
        kprintf("PackosSetIdleContext(): %s\n",
                PackosErrorToString(error));
        return;
      }
  }

  schedulerContext=PackosContextNew(scheduler,"scheduler",&error);
  if (!schedulerContext)
    {
      kprintf("PackosContextNew(scheduler): %s\n",
              PackosErrorToString(error));
      return;
    }

  PackosKernelLoop(schedulerContext,&error);
  kprintf("scheduler exited.\n");
}
