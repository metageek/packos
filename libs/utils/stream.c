#include <util/streamP.h>
#include <util/alloc.h>

#include "stream-kputs.h"

Stream errStream=0;
Stream outStream=0;
Stream inStream=0;

int StreamClose(Stream stream,
                PackosError* error)
{
  if (!error) return -2;
  if (!stream)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (((stream->methods.close)(stream,error))<0)
    return -1;

  free(stream);
  return 0;
}

int StreamRead(Stream stream,
               void* buffer,
               uint32_t count,
               PackosError* error)
{
  if (!error) return -2;
  if (!(stream && buffer))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (!(stream->methods.read))
    {
      *error=packosErrorNotOpenForRead;
      return -1;
    }

  if (!count) return 0;
  
  return ((stream->methods.read)(stream,buffer,count,error));
}

int StreamWrite(Stream stream,
                const void* buffer,
                uint32_t count,
                PackosError* error)
{
  if (!error) return -2;
  if (!(stream && buffer))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (!count) return 0;

  return ((stream->methods.write)(stream,buffer,count,error));
}

Stream StreamPConstruct(PackosError* error)
{
  Stream res;
  if (!error) return 0;

  res=(Stream)(malloc(sizeof(struct Stream)));
  if (!res)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  res->methods.close=0;
  res->methods.read=0;
  res->methods.write=0;
  res->context=0;
  return res;
}

int StreamInit(PackosError* error)
{
  if (!error) return -2;
  if (!outStream)
    {
      outStream=StreamKputsOpen(error);
      if (!outStream) return -1;
    }

  errStream=outStream;
  return 0;
}
