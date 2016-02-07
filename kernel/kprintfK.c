#include <stdarg.h>

#include <packos/sys/blockP.h>
#include "kprintfK.h"
#include "kputcK.h"

#define KPRINTF_BLOCK

static int kputintRecurse(unsigned int i)
{
  if (!i) return 0;

  if (kputintRecurse(i/10)<0) return -1;
  return kputc('0'+(i%10));
}

int kputint(int i)
{
  if (!i) return kputc('0');
  if (i<0)
    {
      if (kputc('-')<0) return -1;
      i*=-1;
    }

  return kputintRecurse(i);
}

int kputintUnsigned(unsigned int i)
{
  if (!i) return kputc('0');

  if (((int)i)<0)
    {
      if (kputc('0'+(i%10))<0) return -1;
      i/=10;
    }

  return kputintRecurse(i);
}

static int kputhexRecurse(uint32_t i, int minWidth)
{
  int minWidth2=(minWidth ? minWidth-1 : 0);
  if ((!i) && (!minWidth)) return 0;

  if (kputhexRecurse(i/16,minWidth2)<0) return -1;
  if (i%16<10)
    return kputc('0'+(i%16));
  else
    return kputc('a'+(i%16)-10);
}

int kputhex(uint32_t i, int minWidth)
{
  if ((!i) && (!minWidth)) return kputc('0');

  return kputhexRecurse(i,minWidth);
}

/* Replace it with a simplified version of what's in
 *  utils/printf.c.  It only needs to handle a few formats:
 *  %d, %s, %x, and %p--oh, and %%.  It does not handle
 *  length modifiers, field width, or precision.
 */
int kprintf(const char* format, ...)
{
  PackosInterruptState state;
  PackosError error;

  va_list ap;
  va_start(ap,format);

#ifdef KPRINTF_BLOCK
  state=PackosKernelContextBlock(&error);
#endif
  if (error!=packosErrorNone) return -1;

  while (*format)
    {
      char ch=*(format++);
      if (ch!='%')
        {
          if (kputc(ch)<0) return -1;
          continue;
        }

      ch=*(format++);
      if (!ch) break;
      switch (ch)
        {
        case '%':
          if (kputc('%')<0) return -1;
          break;

        case 'd':
          if (kputint(va_arg(ap,int))<0) return -1;
          break;

        case 'u':
          if (kputintUnsigned(va_arg(ap,unsigned int))<0) return -1;
          break;

        case 's':
          if (kputs(va_arg(ap,char*))<0) return -1;
          break;

        case 'p':
          if (kputs("0x")<0) return -1;
          {
            void* p=va_arg(ap,void*);
#ifdef PACKOS_64BIT
            if (kputhex(((uint64_t)p)>>32,8)<0) return -1;
            if (kputhex(((uint64_t)p)&(0xffffffff),8)<0) return -1;
#else
            if (kputhex(((uint32_t)p),8)<0) return -1;
#endif
          }
          break;

        case 'x':
          if (kputhex(va_arg(ap,uint32_t),0)<0) return -1;
          break;

        default:
          if (kputc('%')<0) return -1;
          if (kputc(ch)<0) return -1;
          break;
        }
    }

#ifdef KPRINTF_BLOCK
  PackosKernelContextRestore(state,&error);
#endif
  return 0;
}
