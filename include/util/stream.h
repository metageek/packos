#ifndef _UTIL_STREAM_H_
#define _UTIL_STREAM_H_

#include <packos/errors.h>
#include <packos/types.h>

typedef struct Stream* Stream;

#include <util/printf.h>

extern Stream errStream;
extern Stream outStream;
extern Stream inStream;

int StreamClose(Stream stream,
                PackosError* error);

int StreamRead(Stream stream,
               void* buffer,
               uint32_t count,
               PackosError* error);
int StreamWrite(Stream stream,
                const void* buffer,
                uint32_t count,
                PackosError* error);

int StreamInit(PackosError* error);

#endif /*_UTIL_STREAM_H_*/
