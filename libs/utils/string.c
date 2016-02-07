#include <util/string.h>
#include <util/alloc.h>
#include <util/ctype.h>

int UtilStrlen(const char* s)
{
  int res=0;
  if (!s) return 0;
  while (*s)
    {
      s++;
      res++;
    }
  return res;
}

char* UtilStrcpy(char* dest, const char* src)
{
  char* res=dest;
  if (!(dest && src)) return 0;

  while (*src)
    (*dest++)=(*src++);
  *dest=0;

  return res;
}

char* UtilStrdup(const char* src)
{
  char* res=(char*)(malloc(UtilStrlen(src)+1));
  if (!res) return 0;
  return UtilStrcpy(res,src);
}

char* UtilStrchr(const char* s, char target)
{
  if (!s) return 0;
  while (*s)
    {
      if ((*s)==target) return (char*)s;
      s++;
    }

  return 0;
}

char* UtilStrrchr(const char* s, char target)
{
  const char* cur;
  if (!s) return 0;

  cur=s+UtilStrlen(s)-1;
  while (cur>=s)
    {
      if ((*cur)==target) return (char*)cur;
      cur--;
    }

  return 0;
}

static int UtilStrcmpMaybeN(const char* s, const char* t, SizeType n)
{
  int i=0;
  if ((!s) && (!t)) return 0;
  if (!s) return -1;
  if (!t) return 1;

  while ((*s) && (*t) && ((n==0) || (i<n)))
    {
      int delta=((*s)-(*t));
      if (delta) return delta;

      s++;
      t++;
      i++;
    }

  if (*s) return 1;
  if (*t) return -1;
  return 0;
}

int UtilStrcmp(const char* s, const char* t)
{
  return UtilStrcmpMaybeN(s,t,0);
}

int UtilStrncmp(const char* s, const char* t, SizeType n)
{
  if (!n) return 0;
  return UtilStrcmpMaybeN(s,t,n);
}

int UtilStrcasecmp(const char* s, const char* t)
{
  if ((!s) && (!t)) return 0;
  if (!s) return -1;
  if (!t) return 1;

  while ((*s) && (*t))
    {
      int delta=tolower(*s)-tolower(*t);
      if (delta) return delta;

      s++;
      t++;
    }

  if (*s) return 1;
  if (*t) return -1;
  return 0;
}

void* UtilMemcpy(void* dest, const void* src, SizeType n)
{
  byte* db=(byte*)dest;
  const byte* sb=(const byte*)src;

  int i;
  for (i=0; i<n; i++)
    db[i]=sb[i];

  return dest;
}
