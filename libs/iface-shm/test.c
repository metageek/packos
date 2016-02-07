#include <assert.h>
#include <util/stream.h>
#include <util/alloc.h>

#include <packos/sys/contextP.h>
#include <packos/packet.h>
#include <router.h>

#include "processes.h"

int main(int argc, const char* argv[])
{
  PackosError error;
  assert(sizeof(PackosAddress)==16);
  if (UtilArenaInitStatic(&error)<0) return 1;

  PackosSimContextLoop(PackosContextNew(scheduler,"scheduler",&error),
                       &error);
  UtilPrintfStream(errStream,&error,"Loop terminated\n");
  return 0;
}
