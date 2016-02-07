#include <util/stream.h>
#include <util/alloc.h>
#include <util/string.h>
#include <packos/packet.h>
#include <packos/sys/contextP.h>
#include <packos/interrupts.h>
#include <contextQueue.h>
#include <schedulers/common.h>

#include <udp.h>
#include <iface-native.h>

#include "processes.h"

PackosAddress routerAddr;

int SchedulerCallbackCreateProcesses(PackosContextQueue contexts,
                                     PackosError* error)
{
  PackosContext pinger;
  PackosContext router=PackosContextNew(routerProcess,"router",error);
  if (!router)
    {
      UtilPrintfStream(errStream,error,"PackosContextNew: %s\n",PackosErrorToString(*error));
      return -1;
    }

  PackosContextQueueAppend(contexts,router,error);

  routerAddr=PackosContextGetAddr(router,error);
  if (*error!=packosErrorNone)
    {
      UtilPrintfStream(errStream,error,"PackosContextGetAddr(router): %s\n",
	      PackosErrorToString(*error));
      return -1;
    }

  pinger=PackosContextNew(pingerProcess,"pinger",error);
  if (!pinger)
    {
      UtilPrintfStream(errStream,error,"PackosContextNew(pinger): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  PackosContextQueueAppend(contexts,pinger,error);

  return 0;
}
