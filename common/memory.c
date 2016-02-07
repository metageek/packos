#include <packos/sys/memoryP.h>
#include <packos/sys/contextP.h>
#include "../kernel/kprintfK.h"
#include "../kernel/kputcK.h"

#define PHYSICAL_RANGE_POOL_SIZE 1024
#define LOGICAL_RANGE_POOL_SIZE (PHYSICAL_RANGE_POOL_SIZE*4)

static struct {
  struct PackosMemoryPhysicalRange rangeStructs[PHYSICAL_RANGE_POOL_SIZE];
  PackosMemoryPhysicalRange first;
  PackosMemoryPhysicalRange last;
} physicalRangePool;

static struct {
  struct PackosMemoryLogicalRange rangeStructs[LOGICAL_RANGE_POOL_SIZE];
  PackosMemoryLogicalRange first;
  PackosMemoryLogicalRange last;
} logicalRangePool;

static struct {
  struct {
    PackosMemoryPhysicalRange first;
    PackosMemoryPhysicalRange last;
  } inUse,avail;
} physicalMemory={{0,0},{0,0}};

PackosMemoryPhysicalRange
PackosMemoryPhysicalRangeGetFirstInUse(PackosError* error)
{
  if (!error) return 0;
  *error=packosErrorNone;
  return physicalMemory.inUse.first;
}

PackosMemoryPhysicalRange
PackosMemoryPhysicalRangeGetFirstAvail(PackosError* error)
{
  if (!error) return 0;
  *error=packosErrorNone;
  return physicalMemory.avail.first;
}

PackosMemoryPhysicalRange
PackosMemoryPhysicalRangeGetNext(PackosMemoryPhysicalRange physical,
                                 PackosError* error)
{
  if (!error) return 0;
  if (!physical)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  return physical->next;
}

PackosMemoryPhysicalRange
PackosMemoryPhysicalRangeGetPrev(PackosMemoryPhysicalRange physical,
                                 PackosError* error)
{
  if (!error) return 0;
  if (!physical)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  return physical->prev;
}

static PackosMemoryPhysicalRange seekAddrFrom(PackosMemoryPhysicalRange first,
                                              void* physicalAddr,
                                              PackosError* error)
{
  uint32_t target=(uint32_t)physicalAddr;
  PackosMemoryPhysicalRange cur;
  for (cur=first; cur; cur=cur->next)
    {
      uint32_t base=(uint32_t)(cur->physicalAddr);
      if (target<base) continue;
      if (target<(base+(cur->len))) return cur;
    }

  *error=packosErrorDoesNotExist;
  return 0;
}

PackosMemoryPhysicalRange
PackosMemoryPhysicalRangeSeekAddr(void* physicalAddr,
                                  PackosError* error)
{
  PackosMemoryPhysicalRange res;
  if (!error) return 0;
  res=seekAddrFrom(physicalMemory.inUse.first,physicalAddr,error);
  if (res) return res;
  if ((*error)!=packosErrorNone) return 0;

  return seekAddrFrom(physicalMemory.avail.first,physicalAddr,error);
}

static PackosMemoryPhysicalRange physicalRangePoolAlloc(PackosError* error)
{
  PackosMemoryPhysicalRange res;

  if (!error) return 0;
  if (!(physicalRangePool.first))
    {
      kprintf("physicalRangePoolAlloc(): !first\n");
      *error=packosErrorDoesNotExist;
      return 0;
    }

  res=physicalRangePool.first;
  physicalRangePool.first=res->next;
  if (physicalRangePool.first)
    physicalRangePool.first->prev=physicalRangePool.first;
  else
    physicalRangePool.last=0;

  res->next=res->prev=0;
  res->firstUsedBy=res->lastUsedBy=0;
  res->len=0;
  res->physicalAddr=0;
  res->type=packosMemoryPhysicalTypeInvalid;
  res->inUse=false;
  return res;
}

static int physicalRangePoolFree(PackosMemoryPhysicalRange physical,
                                 PackosError* error)
{
  if (!error) return -2;
  if (!physical)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  {
    PackosMemoryPhysicalRange* first;
    PackosMemoryPhysicalRange* last;
    if (physical->inUse)
      {
        first=&(physicalMemory.inUse.first);
        last=&(physicalMemory.inUse.last);
      }
    else
      {
        first=&(physicalMemory.avail.first);
        last=&(physicalMemory.avail.last);
      }

    if (physical->next)
      physical->next->prev=physical->prev;
    else
      (*last)=physical->prev;

    if (physical->prev)
      physical->prev->next=physical->next;
    else
      (*first)=physical->next;
  }

  physical->next=physicalRangePool.first;
  if (physical->next)
    physical->next->prev=physical;
  else
    physicalRangePool.last=physical;
  physical->prev=0;

  return 0;
}

int PackosMemoryInit(PackosError* error)
{
  if (!error) return -2;

  physicalRangePool.first
    =&(physicalRangePool.rangeStructs[0]);
  physicalRangePool.last
    =&(physicalRangePool.rangeStructs[PHYSICAL_RANGE_POOL_SIZE-1]);
  physicalRangePool.first->prev=0;
  physicalRangePool.first->next
    =&(physicalRangePool.rangeStructs[1]);
  physicalRangePool.last->prev
    =&(physicalRangePool.rangeStructs[PHYSICAL_RANGE_POOL_SIZE-2]);

  {
    int i;
    for (i=1; i<(PHYSICAL_RANGE_POOL_SIZE-1); i++)
      {
        physicalRangePool.rangeStructs[i].prev
          =&(physicalRangePool.rangeStructs[i-1]);
        physicalRangePool.rangeStructs[i].next
          =&(physicalRangePool.rangeStructs[i+1]);
      }
  }

  logicalRangePool.first
    =&(logicalRangePool.rangeStructs[0]);
  logicalRangePool.last
    =&(logicalRangePool.rangeStructs[LOGICAL_RANGE_POOL_SIZE-1]);
  logicalRangePool.first->inOwner.prev=0;
  logicalRangePool.last->inOwner.next=0;
  logicalRangePool.first->inOwner.next
    =&(logicalRangePool.rangeStructs[1]);
  logicalRangePool.last->inOwner.prev
    =&(logicalRangePool.rangeStructs[LOGICAL_RANGE_POOL_SIZE-2]);

  {
    int i;
    for (i=1; i<(LOGICAL_RANGE_POOL_SIZE-1); i++)
      {
        logicalRangePool.rangeStructs[i].inOwner.prev
          =&(logicalRangePool.rangeStructs[i-1]);
        logicalRangePool.rangeStructs[i].inOwner.next
          =&(logicalRangePool.rangeStructs[i+1]);
      }
  }

  return 0;
}

int
PackosMemoryPhysicalRangeDefine(void* physicalAddr,
                                PackosMemoryPhysicalType type,
                                uint32_t len,
                                PackosError* error)
{
  PackosMemoryPhysicalRange physical=0;
  if (!error) return -2;
  if (!(len
        && physicalAddr
        && (type>=packosMemoryPhysicalTypeRAM)
        && (type<=packosMemoryPhysicalTypeIO)
        )
      )
    {
      kprintf("PackosMemoryPhysicalRangeDefine(): invalid arg\n");
      *error=packosErrorInvalidArg;
      return -1;
    }

  physical=physicalRangePoolAlloc(error);
  if (!physical)
    {
      kprintf("PackosMemoryPhysicalRangeDefine(): physicalRangePoolAlloc(): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  if (len%PACKOS_PAGE_SIZE)
    len+=(PACKOS_PAGE_SIZE-(len%PACKOS_PAGE_SIZE));

  physical->len=len;
  physical->physicalAddr=physicalAddr;
  physical->type=type;

  physical->next=physicalMemory.avail.first;
  if (physical->next)
    physical->next->prev=physical;
  else
    physicalMemory.avail.last=physical;
  physicalMemory.avail.first=physical;

  return 0;
}

PackosMemoryPhysicalRange
PackosMemoryPhysicalRangeAlloc(PackosMemoryPhysicalType type,
                               uint32_t len,
                               uint32_t alignmentMultiple,
                               PackosError* error)
{
  PackosMemoryPhysicalRange res=0;

  if (!error)
    {
      kprintf("PackosMemoryPhysicalRangeAlloc(): !error\n");
      return 0;
    }

  if (!(len
        && (type>=packosMemoryPhysicalTypeRAM)
        && (type<=packosMemoryPhysicalTypeIO)
        )
      )
    {
      kprintf("PackosMemoryPhysicalRangeAlloc(): invalid arg\n");
      *error=packosErrorInvalidArg;
      return 0;
    }

  {
    PackosMemoryPhysicalRange cur;

    for (cur=physicalMemory.avail.first; cur && (!res); cur=cur->next)
      {
        if (cur->type!=type) continue;
        if ((cur->len)<len) continue;

        uint32_t misalignment;
        if (alignmentMultiple)
          misalignment=(((uint32_t)(cur->physicalAddr))
                        % alignmentMultiple
                        );
        else
          misalignment=0;

        /* Shall we try getting our block from the end of cur? */
        if (misalignment
            && !((((uint32_t)(cur->physicalAddr))+(cur->len))
                 %alignmentMultiple
                 )
            )
          {
            res=physicalRangePoolAlloc(error);
            if (!res)
              {
                kprintf("PackosMemoryPhysicalRangeAlloc(): physicalRangePoolAlloc(): %s\n",PackosErrorToString(*error));
                return 0;
              }
            res->len=len;
            cur->len-=(res->len);
            res->physicalAddr=(((byte*)cur->physicalAddr)+(cur->len));
          }

        misalignment=alignmentMultiple-misalignment;

        if ((cur->len-misalignment)<len) continue;

        if ((cur->len)==len)
          {
            if (cur->next)
              cur->next->prev=cur->prev;
            else
              physicalMemory.avail.last=cur->prev;

            if (cur->prev)
              cur->prev->next=cur->next;
            else
              physicalMemory.avail.first=cur->next;

            res=cur;
          }
        else
          {
            res=physicalRangePoolAlloc(error);
            if (!res)
              {
                kprintf("PackosMemoryPhysicalRangeAlloc(): physicalRangePoolAlloc(): %s\n",PackosErrorToString(*error));
                return 0;
              }
            res->len=(len+misalignment);
            cur->len-=(res->len);
            res->physicalAddr=cur->physicalAddr;
            cur->physicalAddr=(((byte*)cur->physicalAddr)+(res->len));

            if (misalignment)
              {
                PackosMemoryPhysicalRange fragment
                  =physicalRangePoolAlloc(error);
                if (!fragment)
                  {
                    kprintf("PackosMemoryPhysicalRangeAlloc(): physicalRangePoolAlloc(): %s\n",PackosErrorToString(*error));
                    kprintf("Leaking %d bytes\n",misalignment);

                  }
                else
                  {
                    fragment->physicalAddr=res->physicalAddr;
                    fragment->len=misalignment;
                    fragment->next=physicalMemory.avail.first;
                    if (fragment->next)
                      fragment->next->prev=fragment;
                    else
                      physicalMemory.avail.last=fragment;
                  }

                res->len-=misalignment;
                res->physicalAddr
                  =(((char*)(res->physicalAddr))+misalignment);
              }
          }
      }

    if (!res)
      {
        kprintf("PackosMemoryPhysicalRangeAlloc(): passed through loop without finding res\n");
        *error=packosErrorUnknownError;
        return 0;
      }
  }

  res->next=physicalMemory.inUse.first;
  if (res->next)
    res->next->prev=res;
  else
    physicalMemory.inUse.last=res;
  physicalMemory.inUse.first=res;
  res->inUse=true;

  //kprintf("PackosMemoryPhysicalRangeAlloc(): returning %p\n",res);
  return res;
}

int
PackosMemoryPhysicalRangeFree(PackosMemoryPhysicalRange physical,
                              PackosError* error)
{
  PackosMemoryPhysicalRange before=0;
  PackosMemoryPhysicalRange after=0;

  PackosMemoryPhysicalRange cur;
  if (!error) return -2;
  if (!physical)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  for (cur=physicalMemory.inUse.first;
       cur && !(before && after);
       cur=cur->next)
    {
      if ((cur->type)!=(physical->type)) continue;
      if ((!before)
          && ((((char*)(cur->physicalAddr))+(cur->len))
              ==(physical->physicalAddr)
              )
          )
        cur=before;
      else
        {
          if ((!after)
              && ((((char*)(physical->physicalAddr))+(physical->len))
                  ==(cur->physicalAddr)
                  )
              )
            cur=after;
        }
    }

  if (before)
    {
      if (physicalRangePoolFree(physical,error)==0)
        {
          (before->len)+=(physical->len);
          physical=before;
        }
    }

  if (after)
    {
      if (physicalRangePoolFree(after,error)==0)
        (physical->len)+=(after->len);
    }

  return 0;
}

static PackosMemoryLogicalRange logicalRangePoolAlloc(PackosError* error)
{
  PackosMemoryLogicalRange res;

  if (!error) return 0;
  if (!(logicalRangePool.first))
    {
      *error=packosErrorDoesNotExist;
      return 0;
    }

  res=logicalRangePool.first;
  logicalRangePool.first=res->inOwner.next;
  if (logicalRangePool.first)
    logicalRangePool.first->inOwner.prev=logicalRangePool.first;
  else
    logicalRangePool.last=0;

  res->inOwner.next=res->inOwner.prev=0;
  res->usingPhysical.next=res->usingPhysical.prev=0;
  res->flags=0;
  res->logicalAddr=0;
  return res;
}

static int logicalRangePoolFree(PackosMemoryLogicalRange logical,
                                PackosError* error)
{
  if (!error) return -2;
  if (!logical)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  logical->inOwner.next=logicalRangePool.first;
  if (logical->inOwner.next)
    logical->inOwner.next->inOwner.prev=logical;
  else
    logicalRangePool.last=logical;
  logical->inOwner.prev=0;

  return 0;
}

static int newSegmentId(ContextPageTable* pageTable,
                        PackosError* error)
{
  uint16_t res;
  if (!error) return -2;
  if (!pageTable)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (pageTable->segmentIds.max==0)
    {
      res=pageTable->segmentIds.max=pageTable->segmentIds.min=1;
      return res;
    }

  if ((pageTable->segmentIds.max)<0xffff)
    {
      res=(++(pageTable->segmentIds.max));
      return res;
    }

  if ((pageTable->segmentIds.min)>1)
    {
      res=(--(pageTable->segmentIds.min));
      return res;
    }

  {
    byte bitmap[8192];
    PackosMemoryLogicalRange cur;
    for (cur=pageTable->first; cur; cur=cur->inOwner.next)
      {
        int i=(cur->segmentId)/8;
        int j=(cur->segmentId)%8;
        bitmap[i]|=(1<<j);
      }

    for (res=pageTable->segmentIds.min+1; res<pageTable->segmentIds.max; res++)
      {
        int i=res/8;
        int j=res%8;
        if (!(bitmap[i] & (1<<j)))
          return res;
      }
  }

  *error=packosError386NoSegmentAvailable;
  return -1;
}

static PackosMemoryLogicalRange
LogicalRangeNew(PackosMemoryPhysicalRange physical,
                ContextPageTable* pageTable,
                uint32_t flags,
                void* logicalAddr,
                uint32_t offset,
                uint32_t len,
                PackosError* error)
{
  PackosMemoryLogicalRange res;
  PackosMemoryLogicalRange before=0;

  if (!error) return 0;
  if ((!(physical && pageTable))
      || ((flags & packosMemoryFlagCopyOnWrite)
          && !(flags & packosMemoryFlagWritable)
          )
      )
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  if ((physical->type==packosMemoryPhysicalTypeROM)
      && (flags & packosMemoryFlagWritable)
      )
    {
      *error=packosErrorPhysicalMemoryNotWritable;
      return 0;
    }

  if (offset%PACKOS_PAGE_SIZE)
    {
      *error=packosErrorNotPageAligned;
      return 0;
    }

  if (!len)
    len=physical->len-offset;

  if ((offset+len)>(physical->len))
    {
      *error=packosErrorPhysicalMemoryRangeTooSmall;
      return 0;
    }

  if (logicalAddr)
    {
      PackosMemoryLogicalRange cur;
      char* start=(char*)logicalAddr;
      char* end=start+len;
      for (cur=pageTable->first; cur; cur=cur->inOwner.next)
        {
          char* curStart=(char*)(cur->logicalAddr);
          char* curEnd=curStart+cur->len;

          if (curStart>=end)
            {
              before=cur;
              break;
            }

          if (curEnd<=start) continue;
          if (end<=curStart) continue;

          *error=packosErrorLogicalAddressInUse;
          return 0;
        }

      before=0;
    }
  else
    {
      PackosMemoryLogicalRange cur;
      char* prevEnd=0;
      for (cur=pageTable->first; cur; cur=cur->inOwner.next)
        {
          char* curStart=(char*)(cur->logicalAddr);

          if ((curStart-prevEnd)<=len)
            {
              before=cur;
              logicalAddr=prevEnd;
              break;
            }
        }

      if (prevEnd)
        logicalAddr=prevEnd;
      else
        logicalAddr=(void*)0x40000000;
      before=0;
    }

  logicalAddr=((char*)(physical->physicalAddr)+offset);
  {
    static bool already=false;
    if (!already)
      {
        /*kprintf("PackosMemoryLogicalRangeNew(): warning: assuming no mapping going on\n");*/
        already=true;
      }
  }

  res=logicalRangePoolAlloc(error);
  if (!res) return 0;

  {
    int id=newSegmentId(pageTable,error);
    if (id<0)
      {
        PackosError tmp;
        logicalRangePoolFree(res,&tmp);
        return 0;
      }
    res->segmentId=id;
  }

  res->offset=offset;
  res->len=len;

  res->inOwner.next=before;
  if (res->inOwner.next)
    {
      res->inOwner.prev=res->inOwner.next->inOwner.prev;
      res->inOwner.next->inOwner.prev=res;
    }
  else
    {
      res->inOwner.prev=pageTable->last;
      pageTable->last=res;
    }

  if (res->inOwner.prev)
    res->inOwner.prev->inOwner.next=res;
  else
    pageTable->first=res;

  res->physical=physical;
  res->logicalAddr=logicalAddr;
  res->flags=flags;
  return res;
}

PackosMemoryLogicalRange
PackosMemoryLogicalRangeNew(PackosMemoryPhysicalRange physical,
                            PackosContext context,
                            uint32_t flags,
                            void* logicalAddr,
                            uint32_t offset,
                            uint32_t len,
                            PackosError* error)
{
  return LogicalRangeNew(physical,&(context->os.pageTable),
                         flags,logicalAddr,
                         offset,len,
                         error);
}

PackosMemoryLogicalRange
PackosMemoryLogicalRangeNewK(PackosMemoryPhysicalRange physical,
                             uint32_t flags,
                             void* logicalAddr,
                             uint32_t offset,
                             uint32_t len,
                             PackosError* error)
{
  return LogicalRangeNew(physical,PackosKernelPageTable(),
                         flags,logicalAddr,
                         offset,len,
                         error);
}

int
PackosMemoryLogicalRangeFree(PackosMemoryLogicalRange logical,
                             PackosContext context,
                             PackosError* error)
{
  if (!error) return -2;
  if (!(logical && context))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (logical->inOwner.next)
    logical->inOwner.next->inOwner.prev=logical->inOwner.prev;
  else
    context->os.pageTable.last=logical->inOwner.prev;

  if (logical->inOwner.prev)
    logical->inOwner.prev->inOwner.next=logical->inOwner.next;
  else
    context->os.pageTable.first=logical->inOwner.next;

  return logicalRangePoolFree(logical,error);
}

PackosMemoryLogicalRange
PackosMemoryPhysicalRangeGetFirstUser(PackosMemoryPhysicalRange physical,
                                      PackosError* error)
{
  if (!error) return 0;
  if (!physical)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return physical->firstUsedBy;
}

PackosMemoryLogicalRange
PackosMemoryPhysicalRangeGetNextUser(PackosMemoryLogicalRange logical,
                                     PackosError* error)
{
  if (!error) return 0;
  if (!logical)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return logical->usingPhysical.next;
}

void*
PackosMemoryPhysicalRangeGetAddr(PackosMemoryPhysicalRange physical,
                                 PackosError* error)
{
  if (!error) return 0;
  if (!(physical && (physical->physicalAddr)))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return physical->physicalAddr;
}

uint32_t
PackosMemoryPhysicalRangeGetLen(PackosMemoryPhysicalRange physical,
                                PackosError* error)
{
  if (!error) return 0;
  if (!(physical && (physical->len)))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return physical->len;
}

PackosMemoryPhysicalType
PackosMemoryPhysicalRangeGetType(PackosMemoryPhysicalRange physical,
                                 PackosError* error)
{
  if (!error) return 0;
  if (!(physical
        && ((physical->type)>=packosMemoryPhysicalTypeRAM)
        && ((physical->type)<=packosMemoryPhysicalTypeIO)
        )
      )
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return physical->type;
}

bool
PackosMemoryPhysicalRangeGetInUse(PackosMemoryPhysicalRange physical,
                                  PackosError* error)
{
  if (!error) return 0;
  if (!physical)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return physical->inUse;
}

PackosMemoryPhysicalRange
PackosMemoryLogicalRangeGetPhysical(PackosMemoryLogicalRange logical,
                                    PackosError* error)
{
  if (!error) return 0;
  if (!logical)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return logical->physical;
}

PackosMemoryLogicalRange
PackosMemoryLogicalRangeGetNext(PackosMemoryLogicalRange logical,
                                PackosError* error)
{
  if (!error) return 0;
  if (!logical)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return logical->inOwner.next;
}

PackosMemoryLogicalRange
PackosMemoryLogicalRangeGetPrev(PackosMemoryLogicalRange logical,
                                PackosError* error)
{
  if (!error) return 0;
  if (!logical)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return logical->inOwner.prev;
}

void*
PackosMemoryLogicalRangeGetAddr(PackosMemoryLogicalRange logical,
                                PackosError* error)
{
  if (!error) return 0;
  if (!logical)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return logical->logicalAddr;
}

uint32_t
PackosMemoryLogicalRangeGetLen(PackosMemoryLogicalRange logical,
                               PackosError* error)
{
  if (!error) return 0;
  if (!(logical && (logical->physical)))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return logical->physical->len;
}

uint32_t
PackosMemoryLogicalRangeGetFlags(PackosMemoryLogicalRange logical,
                                 PackosError* error)
{
  if (!error) return 0;
  if (!logical)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  *error=packosErrorNone;
  return logical->flags;
}
