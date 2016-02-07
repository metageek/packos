#ifndef _FILEP_H_
#define _FILEP_H_

struct File {
  FileSystem fs;
  void* context;
  uint32_t pos;
};

#endif /*_FILEP_H_*/
