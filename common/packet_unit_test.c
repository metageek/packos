#include <packos/packet.h>
#include <assert.h>
#include <string.h>

int main(int argc, const char* argv[])
{
  PackosError error;

  {
    const char* addrStr="fedc:ba98:7654:3210:fedc:ba98:7654:3210";

    {
      PackosAddress addr=PackosAddrFromString(addrStr,&error);
      assert(error==packosErrorNone);
      assert(addr.bytes[0]==0xfe);
      assert(addr.bytes[1]==0xdc);
      assert(addr.bytes[2]==0xba);
      assert(addr.bytes[3]==0x98);
      assert(addr.bytes[4]==0x76);
      assert(addr.bytes[5]==0x54);
      assert(addr.bytes[6]==0x32);
      assert(addr.bytes[7]==0x10);
      assert(addr.bytes[8]==0xfe);
      assert(addr.bytes[9]==0xdc);
      assert(addr.bytes[10]==0xba);
      assert(addr.bytes[11]==0x98);
      assert(addr.bytes[12]==0x76);
      assert(addr.bytes[13]==0x54);
      assert(addr.bytes[14]==0x32);
      assert(addr.bytes[15]==0x10);
    }

    {
      PackosAddress addr=PackosAddrFromString(addrStr,&error);
      char buff[80];
      assert(error==packosErrorNone);
      assert(PackosAddrToString(addr,buff,sizeof(buff),&error)>=0);
      assert(!strcmp(buff,addrStr));
    }
  }

  {
    const char* addrStr="::fedc:ba98:7654:3210";

    {
      PackosAddress addr=PackosAddrFromString(addrStr,&error);
      assert(error==packosErrorNone);
      assert(addr.bytes[0]==0);
      assert(addr.bytes[1]==0);
      assert(addr.bytes[2]==0);
      assert(addr.bytes[3]==0);
      assert(addr.bytes[4]==0);
      assert(addr.bytes[5]==0);
      assert(addr.bytes[6]==0);
      assert(addr.bytes[7]==0);
      assert(addr.bytes[8]==0xfe);
      assert(addr.bytes[9]==0xdc);
      assert(addr.bytes[10]==0xba);
      assert(addr.bytes[11]==0x98);
      assert(addr.bytes[12]==0x76);
      assert(addr.bytes[13]==0x54);
      assert(addr.bytes[14]==0x32);
      assert(addr.bytes[15]==0x10);
    }

    {
      PackosAddress addr=PackosAddrFromString(addrStr,&error);
      char buff[80];
      assert(error==packosErrorNone);
      assert(PackosAddrToString(addr,buff,sizeof(buff),&error)>=0);
      assert(!strcmp(buff,"0000:0000:0000:0000:fedc:ba98:7654:3210"));
    }
  }

  {
    const char* addrStr="fedc:ba98:7654:3210::";

    {
      PackosAddress addr=PackosAddrFromString(addrStr,&error);
      assert(error==packosErrorNone);
      assert(addr.bytes[0]==0xfe);
      assert(addr.bytes[1]==0xdc);
      assert(addr.bytes[2]==0xba);
      assert(addr.bytes[3]==0x98);
      assert(addr.bytes[4]==0x76);
      assert(addr.bytes[5]==0x54);
      assert(addr.bytes[6]==0x32);
      assert(addr.bytes[7]==0x10);
      assert(addr.bytes[8]==0);
      assert(addr.bytes[9]==0);
      assert(addr.bytes[10]==0);
      assert(addr.bytes[11]==0);
      assert(addr.bytes[12]==0);
      assert(addr.bytes[13]==0);
      assert(addr.bytes[14]==0);
      assert(addr.bytes[15]==0);
    }

    {
      PackosAddress addr=PackosAddrFromString(addrStr,&error);
      char buff[80];
      assert(error==packosErrorNone);
      assert(PackosAddrToString(addr,buff,sizeof(buff),&error)>=0);
      assert(!strcmp(buff,"fedc:ba98:7654:3210:0000:0000:0000:0000"));
    }
  }

  {
    const char* addrStr="::";

    {
      PackosAddress addr=PackosAddrFromString(addrStr,&error);
      assert(error==packosErrorNone);
      assert(addr.bytes[0]==0);
      assert(addr.bytes[1]==0);
      assert(addr.bytes[2]==0);
      assert(addr.bytes[3]==0);
      assert(addr.bytes[4]==0);
      assert(addr.bytes[5]==0);
      assert(addr.bytes[6]==0);
      assert(addr.bytes[7]==0);
      assert(addr.bytes[8]==0);
      assert(addr.bytes[9]==0);
      assert(addr.bytes[10]==0);
      assert(addr.bytes[11]==0);
      assert(addr.bytes[12]==0);
      assert(addr.bytes[13]==0);
      assert(addr.bytes[14]==0);
      assert(addr.bytes[15]==0);
    }

    {
      PackosAddress addr=PackosAddrFromString(addrStr,&error);
      char buff[80];
      assert(error==packosErrorNone);
      assert(PackosAddrToString(addr,buff,sizeof(buff),&error)>=0);
      assert(!strcmp(buff,"0000:0000:0000:0000:0000:0000:0000:0000"));
    }
  }

  return 0;
}
