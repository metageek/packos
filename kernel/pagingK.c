#include "pagingK.h"
#include "kprintfK.h"

static Packos386SegmentDescriptor descriptorTable[8192];

Packos386SegmentDescriptor*
Packos386GetDescriptorTable(uint16_t* numDescriptors)
{
  if (numDescriptors)
    *numDescriptors=8191;
  return descriptorTable;
}

const Packos386SegmentDescriptor* Packos386GetGDT(uint16_t* size)
{
  byte gdtAndSize[6];
  asm("sgdt (%0)\n": :"p"(gdtAndSize));

  {
    uint16_t* size_=(uint16_t*)(gdtAndSize);
    *size=((*(size_))+1)/8;
  }
  {
    Packos386SegmentDescriptor** resP
      =(Packos386SegmentDescriptor**)(gdtAndSize+2);
    return *resP;
  }
}

const Packos386SegmentDescriptor*
Packos386GetGDTDescriptor(uint16_t i,
                          PackosError* error)
{
  const Packos386SegmentDescriptor* descriptors;
  uint16_t numDescriptors;

  if (i<1)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  if (!error) return 0;

  descriptors=Packos386GetGDT(&numDescriptors);
  if (i>numDescriptors)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  return descriptors+i;
}

int Packos386SetGDT(Packos386SegmentDescriptor* descriptors,
                    uint16_t numDescriptors,
                    PackosError* error)
{
  byte gdtAndSize[6];
  if (!error) return -2;
  if (!(descriptors
        && (numDescriptors>0)
        && (numDescriptors<8192)
        )
      )
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  {
    uint16_t* size_=(uint16_t*)(gdtAndSize);
    *size_=numDescriptors*8;
  }

  {
    Packos386SegmentDescriptor** resP
      =(Packos386SegmentDescriptor**)(gdtAndSize+2);
    *resP=descriptors;
  }

  asm("lgdt (%0)\n": :"p"(gdtAndSize));
  return 0;
}

uint16_t Packos386GetLDT(void)
{
  uint16_t res=17;
  asm("sldt (%0)\n": :"p"(&res));
  return res;
}

int Packos386InterpretSegmentSelector(uint16_t segment,
                                      Packos386SegmentSelectorParsed* parsed,
                                      PackosError* error)
{
  if (!error) return -2;
  if (!parsed)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  parsed->index=(segment>>3);
  parsed->isLocal=((segment&4)!=0);
  parsed->privilegeLevel=(segment&3);
  return 0;
}

int Packos386InterpretSegmentDescriptor(const Packos386SegmentDescriptor* desc,
                                        Packos386SegmentDescriptorSane* sane,
                                        PackosError* error)
{
  if (!error) return -2;
  if (!(desc && sane))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  {
    uint32_t base=(desc->lowBase) | (desc->highBase << 24);
    sane->base=(void*)base;
  }

  sane->size=(desc->lowLimit) | (desc->highLimit << 16);
  sane->inPages=desc->limitsArePages;

  bool system=!(desc->system);
  if (system)
    {
      switch (desc->type)
        {
        case 0:
        case 8:
        case 10:
        case 13:
          sane->type=packos386DescriptorTypeInvalid; /* reserved */
          break;

        case 1:
        case 3:
          sane->type=packos386DescriptorTypeTSS16;
          sane->u.tss.busy=(((desc->type)&2)!=0);
          break;

        case 2:
          sane->type=packos386DescriptorTypeLDT;
          break;

        case 4:
          sane->type=packos386DescriptorTypeCallGate16;
          break;

        case 5:
          sane->type=packos386DescriptorTypeTaskGate;
          break;

        case 6:
          sane->type=packos386DescriptorTypeInterruptGate16;
          break;

        case 7:
          sane->type=packos386DescriptorTypeTrapGate16;
          break;

        case 9:
        case 11:
          sane->type=packos386DescriptorTypeTSS32;
          sane->u.tss.busy=(((desc->type)&2)!=0);
          break;

        case 12:
          sane->type=packos386DescriptorTypeCallGate32;
          sane->present=desc->present;
          sane->privilegeLevel=desc->privilegeLevel;
          sane->size=0;
          sane->inPages=true;
          sane->accessed=false;

          sane->u.callGate32.segmentSelector=(desc->lowBase)&0xffff;
          sane->u.callGate32.paramCount=((desc->lowBase)>>16)&0x1f;
          sane->base=(void*)(((uint32_t)(desc->lowLimit))
                             | (((uint32_t)(desc->highLimit))<<16)
                             | (((uint32_t)(desc->availableForOS))<<20)
                             | (((uint32_t)(desc->zero))<<21)
                             | (((uint32_t)(desc->is32Bit))<<22)
                             | (((uint32_t)(desc->limitsArePages))<<23)
                             | (((uint32_t)(desc->highBase))<<24)
                             );
          break;

        case 14:
          sane->type=packos386DescriptorTypeInterruptGate32;
          break;

        case 15:
          sane->type=packos386DescriptorTypeTrapGate32;
          break;
        }
    }
  else
    {
      bool isCode=((desc->type) & 8)!=0;
      sane->accessed=((desc->type) & 1)!=0;
      if (isCode)
        {
          sane->type=packos386DescriptorTypeCode;
          sane->u.code.is32Bit=desc->is32Bit;
          sane->u.code.isReadableAsData=((desc->type) & 2)!=0;
          sane->u.code.conforming=((desc->type) & 4)!=0;
        }
      else
        {
          sane->type=packos386DescriptorTypeData;
          sane->u.data.isWritable=((desc->type) & 2)!=0;
          if (sane->u.data.isWritable)
            {
              sane->u.data.stack.big=desc->is32Bit;
              sane->u.data.stack.expandsDown=((desc->type) & 4)!=0;
            }
        }
    }

  sane->availableForOS=desc->availableForOS;
  sane->present=desc->present;
  sane->privilegeLevel=desc->privilegeLevel;

  return 0;
}

int Packos386BuildSegmentDescriptor(const Packos386SegmentDescriptorSane* sane,
                                    Packos386SegmentDescriptor* desc,
                                    PackosError* error)
{
  if (!error) return -2;
  if (!(desc && sane))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  bool system=true;
  switch (sane->type)
    {
    case packos386DescriptorTypeInvalid:
      desc->lowLimit=0;
      desc->lowBase=0;
      desc->type=0;
      desc->system=0;
      desc->privilegeLevel=0;
      desc->present=0;
      desc->highLimit=0;
      desc->availableForOS=0;
      desc->zero=0;
      desc->is32Bit=0;
      desc->limitsArePages=0;
      desc->highBase=0;
      return 0;

    case packos386DescriptorTypeCode:
      system=false;
      desc->type=(8
                  | (sane->u.code.conforming ? 4 : 0)
                  | (sane->u.code.isReadableAsData ? 2 : 0)
                  | (sane->accessed ? 1 : 0)
                  );
      desc->is32Bit=sane->u.code.is32Bit;
      break;

    case packos386DescriptorTypeData:
      system=false;
      desc->type=(  (sane->u.data.stack.expandsDown ? 4 : 0)
                  | (sane->u.data.isWritable ? 2 : 0)
                  | (sane->accessed ? 1 : 0)
                  );
      desc->is32Bit=sane->u.code.is32Bit;
      break;

    case packos386DescriptorTypeTSS16:
      desc->type=(sane->u.tss.busy ? 3 : 1);
      break;

    case packos386DescriptorTypeLDT:
      desc->type=2;
      desc->is32Bit=0;
      break;

    case packos386DescriptorTypeCallGate16:
      desc->type=4;
      break;

    case packos386DescriptorTypeTaskGate:
      desc->type=5;
      break;

    case packos386DescriptorTypeInterruptGate16:
      desc->type=6;
      break;

    case packos386DescriptorTypeTrapGate16:
      desc->type=7;
      break;

    case packos386DescriptorTypeTSS32:
      desc->type=(sane->u.tss.busy ? 11 : 9);
      break;

    case packos386DescriptorTypeCallGate32:
      desc->type=12;
      desc->is32Bit=1;
      desc->lowLimit=(((uint32_t)(sane->base))&0xffff);
      desc->lowBase=((uint32_t)(sane->u.callGate32.segmentSelector)
                     | (((uint32_t)(sane->u.callGate32.paramCount))<<16)
                     );
      desc->system=0;
      desc->present=sane->present;
      desc->privilegeLevel=sane->privilegeLevel;
      desc->highLimit=(((uint32_t)(sane->base))>>16)&0xf;
      desc->availableForOS=(((uint32_t)(sane->base))>>20)&1;
      desc->zero=(((uint32_t)(sane->base))>>21)&1;
      desc->is32Bit=(((uint32_t)(sane->base))>>22)&1;
      desc->limitsArePages=(((uint32_t)(sane->base))>>23)&1;
      desc->highBase=((uint32_t)(sane->base))>>24;
      return 0;

    case packos386DescriptorTypeInterruptGate32:
      desc->type=14;
      break;

    case packos386DescriptorTypeTrapGate32:
      desc->type=15;
      break;
    }

  desc->system=!system;

  {
    uint32_t base=(uint32_t)(sane->base);
    desc->lowBase=(base & 0xffffff);
    desc->highBase=(base >> 24);
  }

  desc->lowLimit=((sane->size) & 0xffff);
  desc->highLimit=((sane->size) >> 16);
  desc->limitsArePages=sane->inPages;
  desc->availableForOS=sane->availableForOS;
  desc->present=sane->present;
  desc->privilegeLevel=sane->privilegeLevel;
  desc->zero=0;

  return 0;
}

const char* Packos386DescriptorTypeToString(Packos386DescriptorType type,
                                            PackosError* error)
{
  if (!error) return 0;
  switch (type)
    {
    case packos386DescriptorTypeInvalid: return "Invalid";
    case packos386DescriptorTypeCode: return "Code";
    case packos386DescriptorTypeData: return "Data";
    case packos386DescriptorTypeTSS16: return "TSS16";
    case packos386DescriptorTypeLDT: return "LDT";
    case packos386DescriptorTypeCallGate16: return "CallGate16";
    case packos386DescriptorTypeTaskGate: return "TaskGate";
    case packos386DescriptorTypeInterruptGate16: return "InterruptGate16";
    case packos386DescriptorTypeTrapGate16: return "TrapGate16";
    case packos386DescriptorTypeTSS32: return "TSS32";
    case packos386DescriptorTypeCallGate32: return "CallGate32";
    case packos386DescriptorTypeInterruptGate32: return "InterruptGate32";
    case packos386DescriptorTypeTrapGate32: return "TrapGate32";
    }

  *error=packosErrorInvalidArg;
  return 0;
}

uint32_t Packos386GetCR0(void)
{
  uint32_t res;
  asm("pushl %eax");
  asm("movl %%cr0, %%eax; movl %%eax,%0\n": "=o"(res));
  asm("popl %eax");
  return res;
}

uint32_t Packos386GetCR1(void)
{
  uint32_t res;
  asm("movl %%cr1, %%eax; movl %%eax,%0\n": "=o"(res));
  return res;
}

uint32_t Packos386GetCR2(void)
{
  uint32_t res;
  asm("movl %%cr2, %%eax; movl %%eax,%0\n": "=o"(res));
  return res;
}

uint32_t Packos386GetCR3(void)
{
  uint32_t res=0xabcd;
  asm("pushl %eax");
  asm("movl %%cr3, %%eax; movl %%eax,%0\n": "=o"(res));
  asm("popl %eax");
  return res;
}

void Packos386SetCR0(uint32_t cr0)
{
  kprintf("Packos386SetCR0(%x): before; cr0==%x, cr3==%x\n",
          cr0,Packos386GetCR0(),Packos386GetCR3());
  asm("pushl %eax");
  asm("movl %0,%%eax; movl %%eax, %%cr0\n": :"o"(cr0));
  asm("Packos386SetCR0After:");
  asm("popl %eax");
  kprintf("Packos386SetCR0(): after\n");
}

void Packos386SetCR3(uint32_t cr3)
{
  //kprintf("Packos386SetCR3(%x): before; cr0==%x, cr3==%x\n",
  //      cr3,Packos386GetCR0(),Packos386GetCR3());
  asm("pushl %eax");
  asm("movl %0,%%eax; mov %%eax, %%cr3\n": :"o"(cr3));
  asm("popl %eax");
  //kprintf("Packos386SetCR3(%x): after: cr3==%x\n",cr3,Packos386GetCR3());
}

int Packos386DumpDescriptors(const Packos386SegmentDescriptor* descriptors,
                             uint16_t count,
                             bool onlyIfDebugging,
                             PackosError* error)
{
  int i;
  {
    bool skip=false;

#ifdef DEBUG
    skip=false;
#else
    skip=onlyIfDebugging;
#endif
    if (skip) return 0;
  }

  for (i=1; i<count; i++)
    {
      if (Packos386DumpDescriptor(descriptors+i,i,true,error)<0)
        {
          if (0 && ((*error)!=packosErrorInvalidArg))
            return -1;
        }
    }
  return 0;
}

int Packos386DumpGDT(bool onlyIfDebugging, PackosError* error)
{
  if (!error) return -2;
  uint16_t numDescriptors;
  const Packos386SegmentDescriptor* gdt=Packos386GetGDT(&numDescriptors);
  if (!gdt)
    {
      *error=packosErrorUnknownError;
      return -1;
    }

  return Packos386DumpDescriptors(gdt,numDescriptors,onlyIfDebugging,error);
}

int Packos386DumpDescriptor(const Packos386SegmentDescriptor* descriptor,
                            int i, bool verbose,
                            PackosError* error)
{
  if (!error) return -2;
  if (!descriptor)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  Packos386SegmentDescriptorSane sane;
  bool interpreted;
  int intres=Packos386InterpretSegmentDescriptor(descriptor,&sane,error);
  if (intres<0)
    interpreted=false;
  else
    interpreted=true;

  if (interpreted
      && (((sane.type!=packos386DescriptorTypeCallGate32) && (sane.size==0))
          || (sane.type==packos386DescriptorTypeInvalid)
          )
      )
    {
      if (verbose && (descriptor->type!=0))
        {
          if (sane.size==0)
            kprintf("Packos386DumpDescriptor(): interpreted && !sane.size\n");
          else
            kprintf("Packos386DumpDescriptor(): invalid type\n");

          {
            const byte* raw=(const byte*)(descriptor);
            kprintf("#%d: raw: %x %x %x %x %x %x %x %x\n",
                    i,
                    (unsigned)(raw[0]),(unsigned)(raw[1]),(unsigned)(raw[2]),
                    (unsigned)(raw[3]),(unsigned)(raw[4]),(unsigned)(raw[5]),
                    (unsigned)(raw[6]),(unsigned)(raw[7]));
          }
        }

      *error=packosErrorInvalidArg;
      return -1;
    }

  if (i>=0)
    {
      const byte* raw=(const byte*)(descriptor);
      kprintf("#%d: raw: %x %x %x %x %x %x %x %x",
              i,
              (unsigned)(raw[0]),(unsigned)(raw[1]),(unsigned)(raw[2]),
              (unsigned)(raw[3]),(unsigned)(raw[4]),(unsigned)(raw[5]),
              (unsigned)(raw[6]),(unsigned)(raw[7]));
    }

  if (!interpreted)
    kprintf(": %s",PackosErrorToString(*error));
  else
    {
      if (sane.type==packos386DescriptorTypeCallGate32)
        kprintf(": %x:%p %d %s %s %d",
                sane.u.callGate32.segmentSelector,sane.base,
                (int)(sane.privilegeLevel),
                (sane.present ? "P" : "A"),
                Packos386DescriptorTypeToString(sane.type,error),
                sane.u.callGate32.paramCount);
      else
        {
          if (!sane.size)
            {
              if (verbose)
                kprintf("Packos386DumpDescriptor(): !sane.size\n");
              *error=packosErrorInvalidArg;
              return -1;
            }

          kprintf(": %p %x %s %d %s %d %s %s",
                  sane.base,sane.size,
                  (sane.inPages ? "pages" : "bytes"),
                  (int)(sane.privilegeLevel),
                  (sane.accessed ? "A" : "a"),
                  (int)(sane.availableForOS),
                  (sane.present ? "P" : "A"),
                  Packos386DescriptorTypeToString(sane.type,error));
        }

      switch (sane.type)
        {
        case packos386DescriptorTypeCode:
          kprintf(" %d %s %s",
                  (sane.u.code.is32Bit ? 32 : 16),
                  (sane.u.code.isReadableAsData ? "D" : "I"),
                  (sane.u.code.conforming ? "C" : "c")
                  );
          break;

        case packos386DescriptorTypeData:
          kprintf(" %s",(sane.u.data.isWritable ? "RW" : "RO"));
          if (sane.u.data.isWritable)
            {
              kprintf(" %s %s",
                      (sane.u.data.stack.big ? "ESP" : " SP"),
                      (sane.u.data.stack.expandsDown ? "down" : "up")
                      );
            }
          break;

        case packos386DescriptorTypeTSS16:
        case packos386DescriptorTypeTSS32:
          kprintf(" %s",(sane.u.tss.busy ? "busy" : "idle"));
          break;

        default:
          break;
        }
    }

  kprintf("\n");
  return 0;
}

int Packos386DumpDescriptorByIndex(uint16_t i,
                                   PackosError* error)
{
  uint16_t numDescriptors;

  if (i<1)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  const Packos386SegmentDescriptor* descriptors
    =Packos386GetGDT(&numDescriptors);
  if (!descriptors)
    {
      /* can't happen; GRUB boots us up in protected mode. */
      *error=packosErrorUnknownError;
      return -1;
    }

  if (i>numDescriptors)
    {
      kprintf("Packos386DumpDescriptorByIndex(%d): %d>%d\n",
              (int)i,(int)i,(int)numDescriptors);
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (Packos386DumpDescriptor(descriptors+i,i,true,error)<0)
    {
      kprintf("Packos386DumpDescriptorByIndex(%d): Packos386DumpDescriptor(): %s\n",
              i,PackosErrorToString(*error));
      return -1;
    }

  return 0;
}

static int maxDescriptor=-1;

int Packos386RegisterMaxDescriptor(int max, PackosError* error)
{
  kprintf("Packos386RegisterMaxDescriptor(%d)\n",max);
  if (max>maxDescriptor)
    maxDescriptor=max;
  return 0;
}

int Packos386AllocateDescriptor(PackosError* error)
{
  if (!error) return -2;
  if (maxDescriptor>=8192)
    {
      *error=packosErrorOutOfContexts;
      return -1;
    }

  int res=++maxDescriptor;
  kprintf("Packos386AllocateDescriptor(): %d\n",res);
  return res;
}
