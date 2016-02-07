#include <assert.h>
#include <util/stream.h>

#include <packos/sys/contextP.h>
#include <packos/sys/interruptsP.h>
#include <packos/packet.h>
#include <router.h>
#include <util/alloc.h>

#include <schedulers/basic.h>

int main(int argc, const char* argv[])
{
  PackosError error;
  assert(sizeof(PackosAddress)==16);

  if (UtilArenaInitStatic(&error)<0) return 1;

  if (PackosSimInterruptInit(true,true,&error)<0)
    {
      UtilPrintfStream(errStream,&error,"PackosSimInterruptInit(): %s\n",
                       PackosErrorToString(error));
      return 1;
    }

  PackosSimContextLoop(PackosContextNew(SchedulerBasic,"scheduler",&error),
                       &error);
  UtilPrintfStream(errStream,&error,"Loop terminated\n");
  return 0;
}
