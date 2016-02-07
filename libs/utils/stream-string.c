#include <util/stream-file.h>
#include <util/streamP.h>
#include <util/alloc.h>

typedef struct {
  char* buff;
  uint32_t bufflen;
  uint32_t actual;
} Context;

static int CloseMethod(Stream stream,
                       PackosError* error)
{
  return 0;
}

static int ReadMethod(Stream stream,
                      void* buffer,
                      uint32_t count,
                      PackosError* error)
{
  if (!error) return -2;
  *error=packosErrorNotOpenForRead;
  return -1;
}

static int WriteMethod(Stream stream,
                       const void* buffer,
                       uint32_t count,
                       PackosError* error)
{
  Context* context;

  if (!error) return -2;
  if (!(stream && (stream->context) && buffer))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  context=(Context*)(stream->context);
  if (((context->actual)+count)>(context->bufflen))
    {
      *error=packosErrorBufferFull;
      return -1;
    }

  {
    int i;
    for (i=0; i<count; i++)
      context->buff[context->actual+i]=((char*)buffer)[i];
  }

  context->actual+=count;
  return count;
}

Stream StreamStringOpen(char* buff,
                        uint32_t bufflen,
                        PackosError* error)
{
  Context* context;
  Stream res;

  if (!error) return 0;
  if (!(buff && bufflen))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  res=StreamPConstruct(error);
  if (!res) return 0;

  context=(Context*)(malloc(sizeof(Context)));
  if (!context)
    {
      free(res);
      *error=packosErrorOutOfMemory;
      return 0;
    }

  context->buff=buff;
  context->bufflen=bufflen;
  context->actual=0;

  res->methods.close=CloseMethod;
  res->methods.read=ReadMethod;
  res->methods.write=WriteMethod;
  res->context=context;

  return res;
}
