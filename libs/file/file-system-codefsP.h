#ifndef _FILE_SYSTEM_CODEFS_H_
#define _FILE_SYSTEM_CODEFS_H_

#include <file-system-codefs.h>
#include <packos/types.h>

typedef struct CodeFSEntry CodeFSEntry;

struct CodeFSEntry {
  const char* name;
  bool isDir;
  union {
    struct {
      const void* data;
      SizeType len;
    } file;
    struct {
      CodeFSEntry* contents;
      uint32_t count;
    } dir;
  } u;
};

extern const CodeFSEntry codeFSRoot;

#endif /*_FILE_SYSTEM_CODEFS_H_*/
