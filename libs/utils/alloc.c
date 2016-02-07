#include <util/alloc.h>
#include <util/string.h>
#include <util/stream.h>
#include <contextQueue.h>

#include <packos/sys/contextP.h>

typedef struct FreeBlockHeader {
  SizeType size;
  struct FreeBlockHeader* next;
} FreeBlockHeader;

typedef struct {
  uint32_t inited;
  FreeBlockHeader* blocks;
  char data[1];
} Arena;

static Arena* getCurArena(SizeType* size,
                          PackosError* error)
{
  PackosContext curContext=PackosKernelContextCurrent(error);
  if (!curContext)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "PackosKernelContextCurrent(): %s\n",
                       PackosErrorToString(*error));
      return 0;
    }

  {
    char* arenaBase=PackosKernelContextGetArena(curContext,size,error);
    if (!arenaBase)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&tmp,
                         "PackosKernelContextGetArena(): %s\n",
                         PackosErrorToString(*error));
        return 0;
      }

    return (Arena*)arenaBase;
  }
}

int UtilArenaInit(PackosError* error)
{
  Arena* arena;
  SizeType size;
  if (!error) return -2;

  arena=getCurArena(&size,error);
  if (!arena) return -1;

  arena->blocks=(FreeBlockHeader*)(arena->data);
  arena->blocks->size=size-sizeof(arena->inited)-sizeof(arena->blocks);
  arena->blocks->next=0;
  arena->inited=true;
  return 0;
}

void* calloc(SizeType nmemb, SizeType size)
{
  void* res=malloc(nmemb*size);
  if (!res) return 0;
  UtilMemset(res,0,nmemb*size);
  return res;
}

void* malloc(SizeType size)
{
  PackosError error;
  FreeBlockHeader* cur;
  FreeBlockHeader** prev;
  Arena* arena=getCurArena(0,&error);
  if (!arena)
    {
      UtilPrintfStream(errStream,&error,"!arena\n");
      return 0;
    }

  if (!(arena->inited))
    {
      UtilPrintfStream(errStream,&error,"!(arena->inited)\n");
      return 0;
    }

  cur=arena->blocks;
  prev=&(arena->blocks);
  size+=sizeof(FreeBlockHeader);
  {
    const int64_t chip=size%sizeof(FreeBlockHeader);
    if (chip)
      size+=sizeof(FreeBlockHeader)-chip;
  }
  while (cur)
    {
      const int64_t leftover=((int64_t)cur->size)-((int64_t)size);
      if (leftover<0)
        {
          prev=&(cur->next);
          cur=cur->next;
          continue;
        }

      if (leftover>0)
        {
          FreeBlockHeader* fragment
            =(FreeBlockHeader*)(((char*)cur)+size);
          fragment->size=leftover;
          fragment->next=cur->next;

          *prev=fragment;
        }
      else
        *prev=cur->next;

      cur->size=size;
      cur->next=0;
      return (((char*)cur)+sizeof(FreeBlockHeader));
    }

  UtilPrintfStream(errStream,&error,"no free blocks found\n");
  return 0;
}

void free(void* ptr)
{
  PackosError error;
  FreeBlockHeader* block;
  Arena* arena;
  if (!ptr) return;

  arena=getCurArena(0,&error);
  if (!(arena->inited)) return;

  block=(FreeBlockHeader*)(((char*)ptr)-sizeof(FreeBlockHeader));
  block->next=arena->blocks;
  arena->blocks=block;
}

void* realloc(void* ptr, SizeType size)
{
  return 0;
}
