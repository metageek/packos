#ifndef _UTIL_STREAM_STRING_H_
#define _UTIL_STREAM_STRING_H_

#include <util/stream.h>

/* Creates a Stream for writing to the given char buffer.  Does not take
 *  ownership of the buffer.
 */
Stream StreamStringOpen(char* buff,
                        int bufflen,
                        PackosError* error);

#endif /*_UTIL_STREAM_STRING_H_*/
