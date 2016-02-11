/* This is a cheat; it will not work once we have memory protection.
 */

#include "stream-kputs.h"

#include <util/streamP.h>

static int CloseMethod(Stream stream,
                       PackosError* error)
{
  return 0;
}

static inline void outb(uint16_t port, byte v)
{
  asm volatile ("outb %0, %1" : : "a"(v), "Nd"(port) );
}

static int WriteMethod(Stream stream,
                       const void* buffer_,
                       uint32_t count,
                       PackosError* error)
{
  extern void kputc(char);
  const char* buffer=(const char*)buffer_;
  int i;
  for (i=0; i<count; i++) {
    kputc(buffer[i]);
    outb(0xe9, buffer[i]);
  }

  return count;
}

Stream StreamKputsOpen(PackosError* error)
{
  Stream res=StreamPConstruct(error);
  if (!res) return 0;

  res->methods.close=CloseMethod;
  res->methods.read=0;
  res->methods.write=WriteMethod;
  res->context=0;
  return res;
}
