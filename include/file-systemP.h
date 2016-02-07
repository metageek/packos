#ifndef _FILE_SYSTEMP_H_
#define _FILE_SYSTEMP_H_

#include <file-system.h>

typedef int (*FileSystemOpenMethod)(FileSystem fs,
                                    File file,
                                    const char* path,
                                    uint32_t flags,
                                    PackosError* error);
typedef int (*FileSystemCloseMethod)(FileSystem fs,
                                     File file,
                                     PackosError* error);
typedef int (*FileSystemReadMethod)(FileSystem fs,
                                    File file,
                                    void* buffer,
                                    uint32_t count,
                                    PackosError* error);
typedef int (*FileSystemWriteMethod)(FileSystem fs,
                                     File file,
                                     const void* buffer,
                                     uint32_t count,
                                     PackosError* error);
typedef int (*FileSystemDeleteMethod)(FileSystem fs,
                                      const char* path,
                                      PackosError* error);
typedef int (*FileSystemRenameMethod)(FileSystem fs,
                                      const char* pathFrom,
                                      const char* pathTo,
                                      PackosError* error);
typedef int (*FileSystemDestructor)(FileSystem fs,
                                    PackosError* error);

struct FileSystem {
  struct {
    FileSystemOpenMethod open;
    FileSystemCloseMethod close;
    FileSystemReadMethod read;
    FileSystemWriteMethod write;
    FileSystemDeleteMethod delete;
    FileSystemRenameMethod rename;
    FileSystemDestructor destructor;
  } methods;
  void* context;
};

FileSystem FileSystemPNew(PackosError* error);
int FileSystemPDelete(FileSystem fs, PackosError* error);

#endif /*_FILE_SYSTEMP_H_*/
