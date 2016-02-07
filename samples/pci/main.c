#include <util/stream.h>
#include <util/string.h>
#include <util/alloc.h>

#include <packos/sys/contextP.h>
#include <packos/packet.h>
#include <timer.h>
#include <iface-native.h>
#include <router.h>

#include <schedulers/basic.h>
#include <file.h>
#include <tcp.h>
#include <pci/pci.h>
#include <drivers/rtl8139.h>

static const uint16_t serverPort=5100;
PackosAddress pciDriverAddr;

static SchedulerBasicContextMetadata pciDriverMetadata,rtl8139Metadata;

int SchedulerCallbackCreateProcesses(PackosContextQueue contexts,
                                     PackosError* error)
{
  PackosContext pciDriver
    =PackosContextNew(pciDriverProcess,"_pciDriver",error);
  if (!pciDriver)
    {
      UtilPrintfStream(errStream,error,"PackosContextNew: %s\n",PackosErrorToString(*error));
      return -1;
    }

  pciDriverMetadata.isDaemon=true;
  pciDriverMetadata.isRunning=true;
  pciDriverMetadata.isInited=false;
  pciDriverMetadata.dependsOn=0;

  if (PackosContextSetMetadata(pciDriver,&pciDriverMetadata,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "PackosContextSetMetadata(pciDriver): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  PackosContextQueueAppend(contexts,pciDriver,error);

  pciDriverAddr=PackosContextGetAddr(pciDriver,error);
  if (*error!=packosErrorNone)
    {
      UtilPrintfStream(errStream,error,"PackosContextGetAddr(pciDriver): %s\n",
	      PackosErrorToString(*error));
      return -1;
    }

  PackosContext rtl8139=PackosContextNew(rtl8139DriverProcess,"rtl8139",error);
  if (!rtl8139)
    {
      UtilPrintfStream(errStream,error,"PackosContextNew(rtl8139): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  rtl8139Metadata.isDaemon=false;
  rtl8139Metadata.isRunning=false;
  rtl8139Metadata.isInited=true;
  rtl8139Metadata.dependsOn=pciDriver;

  if (PackosContextSetMetadata(rtl8139,&rtl8139Metadata,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "PackosContextSetMetadata(rtl8139): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  PackosContextQueueAppend(contexts,rtl8139,error);
  return 0;
}

void testMain(void)
{
  PackosError error;

  PackosKernelLoop(PackosContextNew(SchedulerBasic,"scheduler",&error),&error);
  UtilPrintfStream(errStream,&error,"Loop terminated\n");
}
