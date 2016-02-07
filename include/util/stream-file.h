#ifndef _UTIL_STREAM_FILE_H_
#define _UTIL_STREAM_FILE_H_

#include <util/stream.h>
#include <file.h>

/* Ownership of the File is transferred to the new Stream. */
Stream StreamFileOpen(File file,
                      PackosError* error);

#endif /*_UTIL_STREAM_FILE_H_*/
