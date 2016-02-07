#include <util/stream-stdio.h>
#include <util/streamP.h>

static int CloseMethod(Stream stream,
                       PackosError* error)
{
  FILE* f=(FILE*)(stream->context);
  if ((f!=stdin) && (f!=stdout) && (f!=stderr))
    fclose(f);
  stream->context=0;
  return 0;
}

static int ReadMethod(Stream stream,
                      void* buffer,
                      uint32_t count,
                      PackosError* error)
{
  if (fread(buffer,1,count,(FILE*)(stream->context))<count)
    {
      *error=packosErrorUnknownError;
      return -1;
    }

  return count;
}

static int WriteMethod(Stream stream,
                       const void* buffer,
                       uint32_t count,
                       PackosError* error)
{
  if (fwrite(buffer,1,count,(FILE*)(stream->context))<count)
    {
      *error=packosErrorUnknownError;
      return -1;
    }

  return count;
}

Stream StreamStdioOpen(FILE* f,
                       PackosError* error)
{
  Stream res=StreamPConstruct(error);
  if (!res) return 0;

  res->methods.close=CloseMethod;
  res->methods.read=ReadMethod;
  res->methods.write=WriteMethod;
  res->context=f;
  return res;
}

int StreamStdioInitStreams(PackosError* error)
{
  if (!error) return -2;

  errStream=StreamStdioOpen(stderr,error);
  if (!errStream) return -1;

  outStream=StreamStdioOpen(stdout,error);
  if (!outStream)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,"StreamStdioOpen(stdout): %s\n",
                       PackosErrorToString(*error));
      StreamClose(errStream,&tmp);
      return 1;
    }

  inStream=StreamStdioOpen(stdin,error);
  if (!inStream)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,"StreamStdioOpen(stdin): %s\n",
                       PackosErrorToString(*error));
      StreamClose(outStream,&tmp);
      StreamClose(errStream,&tmp);
      return 1;
    }
  
  return 0;
}
