#include "file-system-codefsP.h"
#include <file-systemP.h>
#include <fileP.h>

#include <util/printf.h>
#include <util/string.h>

static int OpenMethod(FileSystem fs,
                      File file,
                      const char* path,
                      uint32_t flags,
                      PackosError* error);
static int CloseMethod(FileSystem fs,
                       File file,
                       PackosError* error);
static int ReadMethod(FileSystem fs,
                      File file,
                      void* buffer,
                      uint32_t count,
                      PackosError* error);
static int WriteMethod(FileSystem fs,
                       File file,
                       const void* buffer,
                       uint32_t count,
                       PackosError* error);
static int DeleteMethod(FileSystem fs,
                        const char* path,
                        PackosError* error);
static int RenameMethod(FileSystem fs,
                        const char* pathFrom,
                        const char* pathTo,
                        PackosError* error);
static int Destructor(FileSystem fs,
                      PackosError* error);

FileSystem FileSystemCodeFSNew(PackosError* error)
{
  FileSystem fs;

  if (!error) return 0;

  fs=FileSystemPNew(error);
  if (!fs)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,"FileSystemCodeFSNew(): FileSystemPNew(): %s\n",
                       PackosErrorToString(*error));
      return 0;
    }

  fs->methods.open=OpenMethod;
  fs->methods.close=CloseMethod;
  fs->methods.read=ReadMethod;
  fs->methods.write=WriteMethod;
  fs->methods.rename=RenameMethod;
  fs->methods.delete=DeleteMethod;
  fs->methods.destructor=Destructor;

  fs->context=0;
  return fs;
}

static const CodeFSEntry* seekEntry(const CodeFSEntry* dir,
                                    const char* path,
                                    PackosError* error)
{
  int i,pathLen,basenameLen;

  if (!error) return 0;
  if (!(dir && path))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  if (!(dir->isDir))
    {
      *error=packosErrorIsNotDirectory;
      return 0;
    }

  pathLen=UtilStrlen(path);

  {
    const char* slash=UtilStrchr(path,'/');
    if (!slash)
      basenameLen=pathLen;
    else
      basenameLen=slash-path;
  }

  for (i=0; i<dir->u.dir.count; i++)
    {
      const CodeFSEntry* cur=dir->u.dir.contents+i;
      if (UtilStrlen(cur->name)!=basenameLen)
        continue;

      if (UtilStrncmp(cur->name,path,basenameLen))
        continue;

      if (basenameLen==pathLen)
        return cur;
      else
        return seekEntry(cur,path+basenameLen+1,error);
    }

  *error=packosErrorDoesNotExist;
  return 0;
}

static int OpenMethod(FileSystem fs,
                      File file,
                      const char* path,
                      uint32_t flags,
                      PackosError* error)
{
  const CodeFSEntry* entry;
  if (!error) return -2;
  if (!(fs && file && path))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (flags & (fileOpenFlagWrite | fileOpenFlagAppend))
    {
      *error=packosErrorReadOnlyFilesystem;
      return -1;
    }

  if (!(flags & fileOpenFlagRead))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  entry=seekEntry(&codeFSRoot,path,error);
  if (!entry)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "file-system-codefs:OpenMethod(%s): seekEntry(): %s\n",
                       path,
                       PackosErrorToString(*error));
      return -1;
    }

  if (entry->isDir)
    {
      *error=packosErrorIsDirectory;
      return -1;
    }

  file->context=(void*)entry; /* cast away const */
  return 0;
}

static int CloseMethod(FileSystem fs,
                       File file,
                       PackosError* error)
{
  if (!error) return -2;
  if (!file)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }
  file->context=0;
  return 0;
}

static int ReadMethod(FileSystem fs,
                      File file,
                      void* buffer,
                      uint32_t count,
                      PackosError* error)
{
  const CodeFSEntry* entry;
  if (!error) return -2;
  if (!(file && buffer && (file->context)))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  entry=(CodeFSEntry*)(file->context);
  if (entry->isDir)
    {
      *error=packosErrorIsDirectory;
      return -1;
    }

  if ((file->pos)>=(entry->u.file.len))
    {
      *error=packosErrorEndOfFile;
      return -1;
    }

  if (((file->pos)+count)>(entry->u.file.len))
    count=(entry->u.file.len)-(file->pos);
  UtilMemcpy(buffer,((char*)entry->u.file.data)+file->pos,count);
  return count;
}

static int WriteMethod(FileSystem fs,
                       File file,
                       const void* buffer,
                       uint32_t count,
                       PackosError* error)
{
  *error=packosErrorReadOnlyFilesystem;
  return -1;
}

static int DeleteMethod(FileSystem fs,
                        const char* path,
                        PackosError* error)
{
  *error=packosErrorReadOnlyFilesystem;
  return -1;
}

static int RenameMethod(FileSystem fs,
                        const char* pathFrom,
                        const char* pathTo,
                        PackosError* error)
{
  *error=packosErrorReadOnlyFilesystem;
  return -1;
}

static int Destructor(FileSystem fs,
                      PackosError* error)
{
  return 0;
}
