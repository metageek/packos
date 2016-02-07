#ifndef _FILE_H_
#define _FILE_H_

#include <packos/types.h>
#include <packos/errors.h>

typedef struct File* File;
typedef struct FileSystem* FileSystem;

typedef enum {
  fileOpenFlagInvalid=0,
  fileOpenFlagRead=1,
  fileOpenFlagWrite=2,
  fileOpenFlagAppend=4
} FileOpenFlag;

File FileOpen(FileSystem fs,
              const char* path,
              uint32_t flags,
              PackosError* error);
int FileClose(File file,
              PackosError* error);

int FileRead(File file,
             void* buffer,
             uint32_t count,
             PackosError* error);
int FileWrite(File file,
              const void* buffer,
              uint32_t count,
              PackosError* error);

int FileDelete(FileSystem fs,
               const char* path,
               PackosError* error);
int FileRename(FileSystem fs,
               const char* pathFrom,
               const char* pathTo,
               PackosError* error);

FileSystem FileGetFileSystem(File file,
                             PackosError* error);

#endif /*_FILE_H_*/
