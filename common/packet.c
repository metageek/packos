#include <packos/sys/packetP.h>
#include <packos/arch.h>
#include <packos/sys/contextP.h>
#include <packos/sys/blockP.h>
#include <packos/sys/entry-points.h>

PackosAddressMask PackosSysAddressMask(PackosError* error)
{
  PackosAddressMask res;
  res.addr.octs[0]=res.addr.octs[1]=0;
  res.addr.bytes[0]=0x7e;
  res.addr.bytes[1]=0x8e;
  res.len=24;
  *error=packosErrorNone;
  return res;
}

static char hexit(unsigned int nibble)
{
  if (nibble<10) return '0'+nibble;
  if (nibble<16) return 'a'+nibble-10;
  return '?';
}

static int hexitToNibble(char hexit)
{
  if ((hexit>='0') && (hexit<='9')) return hexit-'0';
  if ((hexit>='a') && (hexit<='f')) return hexit-'a'+10;
  if ((hexit>='A') && (hexit<='F')) return hexit-'A'+10;
  return -1;
}

PackosAddress PackosAddrFromString(const char* str,
                                   PackosError* error)
{
  PackosAddress res,zero=PackosAddrGetZero();
  int skipStart=-1,numAfterSkip=0;
  const char* s=str;
  int i=0;

  res=zero;

  *error=packosErrorInvalidArg;

  switch (*s)
    {
    case ':':
      s++;
      goto COLON1;
    case 0:
      return zero;
    default:
      goto HEX;
    }

 COLON1:
  if (*(s++)==':')
    {
      skipStart=i;
      goto HEX;
    }
  else
    return zero;

 HEX:
  {
	  uint32_t wordSoFar=0;
	  uint32_t N=0;
	  while (((*s) && ((*s)!=':')) && (N<4))
	  {
		  int nib=hexitToNibble(*(s++));
		  if (nib<0)
			  return zero;
		  wordSoFar<<=4;
		  wordSoFar|=nib;
		  N++;
	  }

	  res.words[i++]=htons(wordSoFar);
	  if (skipStart>=0) numAfterSkip++;
	  if (i==8)
      {
        if (*s)
          return zero;
        else
          goto DONE;
      }
  }
  if ((*s)==0)
	  goto DONE;
  if ((*s)==':')
	  goto COLON2;
  return zero;

 COLON2:
  if ((*s)==0)
    goto DONE;

  if ((*(s++))==':')
    goto NEXT;

  return zero;

 NEXT:
  if ((*s)==':')
    goto SKIP;
  else
    goto HEX;

 SKIP:
  if (skipStart>=0)
    return zero;
  skipStart=i;
  s++;
  goto HEX;

 DONE:
  if (skipStart>=0)
    {
      int j,skipTo=8-numAfterSkip;
      for (j=0; j<numAfterSkip; j++)
        {
          res.words[skipTo+j]=res.words[skipStart+j];
          res.words[skipStart+j]=0;
        }
    }
  else
    {
      if (i<8)
        return zero;
    }
  *error=packosErrorNone;
  return res;
}

int PackosAddrToString(PackosAddress addr,
                       char* buf,
                       unsigned int buflen,
                       PackosError* error)
{
  if ((!buf) || (buflen<40))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  {
    unsigned int pos,i;
    for (i=pos=0; i<8; i++)
      {
        if (pos>0) buf[pos++]=':';

		uint16_t w=ntohs(addr.words[i]);
        buf[pos++]=hexit(w>>12);
        buf[pos++]=hexit((w>>8)&15);
        buf[pos++]=hexit((w>>4)&15);
        buf[pos++]=hexit(w&15);
      }

    buf[pos]=0;

    return pos;
  }
}

bool PackosAddrEq(PackosAddress a, PackosAddress b)
{
  return ((a.octs[0]==b.octs[0]) && (a.octs[1]==b.octs[1]));
}

bool PackosAddrMatch(PackosAddressMask mask, PackosAddress addr)
{
  unsigned int i=0,len=mask.len;
  if (len==0) return true;

  while ((i<4) && (len>32))
    {
      if (mask.addr.quads[i]!=addr.quads[i]) return false;
      i++;
      len-=32;
    }

  if (len<=0) return true;

  {
    unsigned int bitmask=((1<<len)-1)<<(32-len);
    return ((ntohl(mask.addr.quads[i])&bitmask)
            ==
            (ntohl(addr.quads[i])&bitmask)
            );
  }
}

PackosAddress PackosAddrGetZero(void)
{
  PackosAddress res;
  res.octs[0]=res.octs[1]=0;
  return res;
}

bool PackosAddrIsZero(PackosAddress a)
{
  return ((a.octs[0]==0) && (a.octs[1]==0));
}

bool PackosAddrIsMcast(PackosAddress a)
{
  return (a.octs[0]==255);
}

PackosAddress PackosPacketGetKernelAddr(PackosError* error)
{
  return PackosAddrGetZero();
}

PackosAddress PackosSchedulerAddress(PackosError* error)
{
  const PackosContextConfig* config=PackosContextConfigGet(error);
  if (!config) return PackosAddrGetZero();

  return config->addresses.scheduler;
}
