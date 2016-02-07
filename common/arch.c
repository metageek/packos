#include <packos/arch.h>

uint16_t packosSwab16(uint16_t i)
{
  return (i>>8)|((i&0xff)<<8);
}

uint32_t packosSwab32(uint32_t i)
{
  return ((i>>24)
          |(((i>>16)&0xff)<<8)
          |(((i>>8)&0xff)<<16)
          |((i&0xff)<<24)
          );
}
