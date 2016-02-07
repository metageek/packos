#ifndef _UTIL_STREAMP_H_
#define _UTIL_STREAMP_H_

#include <util/stream.h>

typedef int (*StreamCloseMethod)(Stream stream,
                                 PackosError* error);
typedef int (*StreamReadMethod)(Stream stream,
                                void* buffer,
                                uint32_t count,
                                PackosError* error);
typedef int (*StreamWriteMethod)(Stream stream,
                                 const void* buffer,
                                 uint32_t count,
                                 PackosError* error);

struct Stream {
  struct {
    StreamCloseMethod close;
    StreamReadMethod read;
    StreamWriteMethod write;
  } methods;
  void* context;
};

Stream StreamPConstruct(PackosError* error);

#endif /*_UTIL_STREAMP_H_*/
