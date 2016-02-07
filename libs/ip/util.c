#include "util.h"

uint16_t IpUtilAdd1sComplement(uint16_t a, uint16_t b)
{
  uint32_t a32=a,b32=b;
  uint32_t res32=a32+b32;
  uint16_t res=((uint16_t)(res32&0xffff))+((uint16_t)(res32>>16)&1);
  /*if (res==0xffff) res=0;*/
  return res;
}

uint16_t IpUtilAdd1sComplementAddr(uint16_t a, const PackosAddress* addr)
{
  uint16_t i;
  for (i=0; i<8; i++)
    a=IpUtilAdd1sComplement(a,addr->words[i]);
  return a;
}
