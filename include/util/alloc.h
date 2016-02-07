#ifndef _UTIL_ALLOC_H_
#define _UTIL_ALLOC_H_

#include <packos/types.h>
#include <packos/errors.h>

void* calloc(SizeType nmemb, SizeType size);
void* malloc(SizeType size);
void free(void* ptr);
void* realloc(void* ptr, SizeType size);

int UtilArenaInit(PackosError* error);

#endif /*_UTIL_ALLOC_H_*/
