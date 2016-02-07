#include <util/stream.h>

#include <packos/sys/contextP.h>
#include <packos/packet.h>

#include "processes.h"

void testMain(void)
{
  PackosError error;

  PackosKernelLoop(PackosContextNew(scheduler,"scheduler",&error),
                   &error);
  UtilPrintfStream(errStream,&error,"Loop terminated\n");
  return;
}
