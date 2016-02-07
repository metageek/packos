#include <util/alloc.h>
#include <util/stream.h>
#include <file-systemP.h>
#include <fileP.h>

File FileOpen(FileSystem fs,
              const char* path,
              uint32_t flags,
              PackosError* error)
{
  File file;

  if (!error) return 0;
  if (!(fs && (fs->methods.open) && path))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  file=(File)(malloc(sizeof(struct File)));
  if (!file)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  file->fs=fs;
  file->context=0;
  file->pos=0;

  if ((fs->methods.open)(fs,file,path,flags,error)<0)
    {
      free(file);
      UtilPrintfStream(errStream,error,"FileOpen(): method: %s\n",PackosErrorToString(*error));
      return 0;
    }

  return file;
}

int FileClose(File file,
              PackosError* error)
{
  if (!error) return 0;
  if (!(file && (file->fs) && (file->fs->methods.close)))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  {
    int res=(file->fs->methods.close)(file->fs,file,error);
    if (res<0)
      UtilPrintfStream(errStream,error,"FileClose(): method: %s\n",
              PackosErrorToString(*error));
    free(file);
    return res;
  }
}

int FileRead(File file,
             void* buffer,
             uint32_t count,
             PackosError* error)
{
  if (!error) return 0;
  if (!(file && (file->fs) && (file->fs->methods.read) && buffer))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  return (file->fs->methods.read)(file->fs,file,buffer,count,error);
}

int FileWrite(File file,
              const void* buffer,
              uint32_t count,
              PackosError* error)
{
  if (!error) return 0;
  if (!(file && (file->fs) && (file->fs->methods.write) && buffer))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  return (file->fs->methods.write)(file->fs,file,buffer,count,error);
}

int FileDelete(FileSystem fs,
               const char* path,
               PackosError* error)
{
  if (!error) return 0;
  if (!(fs && path))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  return (fs->methods.delete)(fs,path,error);
}

int FileRename(FileSystem fs,
               const char* pathFrom,
               const char* pathTo,
               PackosError* error)
{
  if (!error) return 0;
  if (!(fs && pathFrom && pathTo))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  return (fs->methods.rename)(fs,pathFrom,pathTo,error);
}

FileSystem FileGetFileSystem(File file,
                             PackosError* error)
{
  if (!error) return 0;
  if (!file)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  return file->fs;
}
