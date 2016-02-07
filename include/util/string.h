#ifndef _UTIL_STRING_H_
#define _UTIL_STRING_H_

#include <packos/types.h>

int UtilStrlen(const char* s);
char* UtilStrcpy(char* dest, const char* src);
char* UtilStrdup(const char* src);
char* UtilStrchr(const char* s, char target);
char* UtilStrrchr(const char* s, char target);
int UtilStrcmp(const char* s, const char* t);
int UtilStrncmp(const char* s, const char* t, SizeType n);
int UtilStrcasecmp(const char* s, const char* t);

void* UtilMemcpy(void* dest, const void* src, SizeType n);
void* UtilMemset(void* s, int c, SizeType n);

#endif /*_UTIL_STRING_H_*/
