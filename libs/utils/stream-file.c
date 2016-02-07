#include <util/stream-file.h>
#include <util/streamP.h>

static int CloseMethod(Stream stream,
                       PackosError* error)
{
  if (!error) return -2;
  if (!(stream && (stream->context)))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (FileClose((File)(stream->context),error)<0) return -1;

  stream->context=0;
  return 0;
}

static int ReadMethod(Stream stream,
                      void* buffer,
                      uint32_t count,
                      PackosError* error)
{
  if (!error) return -2;
  if (!(stream && (stream->context) && buffer))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  return FileRead((File)(stream->context),buffer,count,error);
}

static int WriteMethod(Stream stream,
                       const void* buffer,
                       uint32_t count,
                       PackosError* error)
{
  if (!error) return -2;
  if (!(stream && (stream->context) && buffer))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  return FileWrite((File)(stream->context),buffer,count,error);
}

Stream StreamFileOpen(File file,
                      PackosError* error)
{
  Stream res;

  if (!error) return 0;
  if (!file)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  res=StreamPConstruct(error);
  if (!res) return 0;

  res->methods.close=CloseMethod;
  res->methods.read=ReadMethod;
  res->methods.write=WriteMethod;
  res->context=file;

  return res;
}
