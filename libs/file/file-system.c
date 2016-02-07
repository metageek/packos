#include <util/stream.h>
#include <util/alloc.h>
#include <file-systemP.h>

FileSystem FileSystemPNew(PackosError* error)
{
  FileSystem fs;
  if (!error) return 0;

  fs=(FileSystem)(malloc(sizeof(struct FileSystem)));
  if (!fs)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  fs->methods.open=0;
  fs->methods.close=0;
  fs->methods.read=0;
  fs->methods.write=0;
  fs->methods.rename=0;
  fs->methods.delete=0;
  fs->methods.destructor=0;
  return fs;
}

int FileSystemPDelete(FileSystem fs, PackosError* error)
{
  if (!error) return -2;
  if (!fs)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  free(fs);

  return 0;
}

int FileSystemClose(FileSystem fs,
                    PackosError* error)
{
  if (!error) return -2;
  if (!fs)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (!(fs->methods.destructor)) return 0;
  return (fs->methods.destructor)(fs,error);
}
