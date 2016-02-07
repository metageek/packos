#ifndef _UTIL_PRINTF_H_
#define _UTIL_PRINTF_H_

#include <util/stream.h>
#include <stdarg.h>

int UtilVPrintfStream(Stream stream,
                      PackosError* error,
                      const char* format,
                      va_list ap);
int UtilPrintfStream(Stream stream,
                     PackosError* error,
                     const char* format,
                     ...);

#endif /*_UTIL_PRINTF_H_*/
