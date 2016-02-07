#include <assert.h>
#include <util/stream.h>
#include <util/alloc.h>

#include <packos/sys/contextP.h>
#include <packos/packet.h>
#include <timer.h>
#include <iface-native.h>
#include <router.h>

#include <schedulers/basic.h>
#include <file.h>
#include <file-system-dumbfs.h>

static void fileProcess(void)
{
  PackosError error;
  UdpSocket socket;

  if (UtilArenaInit(&error)<0)
    return;

  {
    extern PackosAddress routerAddr;
    IpIface iface=IpIfaceNativeNew(&error);
    if (!iface)
      {
        UtilPrintfStream(errStream,&error,
                "fileProcess(): IpIfaceNativeNew: %s\n",
                PackosErrorToString(error));
        return;
      }

    if (IpIfaceRegister(iface,&error)<0)
      {
        UtilPrintfStream(errStream,&error,
                "fileProcess(): IpIfaceRegister: %s\n",
                PackosErrorToString(error));
        return;
      }

    if (IpSetDefaultRoute(routerAddr,iface,&error)<0)
      {
        UtilPrintfStream(errStream,&error,
                "fileProcess(): IpSetDefaultRoute(): %s\n",
                PackosErrorToString(error));
        return;
      }

  }

  socket=UdpSocketNew(&error);
  if (!socket)
    {
      UtilPrintfStream(errStream,&error,"fileProcess(): UdpSocketNew(): %s\n",
              PackosErrorToString(error));
      return;
    }

  if (UdpSocketBind(socket,PackosAddrGetZero(),0,&error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&error,"fileProcess(): UdpSocketBind(): %s\n",
              PackosErrorToString(error));
      UdpSocketClose(socket,&tmp);
      return;
    }

  {
    File f;
    FileSystem fs;

    PackosAddress addr=PackosAddrFromString("7e8f::02",&error);
    if (error!=packosErrorNone)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&error,"fileProcess(): PackosAddrFromString(): %s\n",
                PackosErrorToString(error));
        UdpSocketClose(socket,&tmp);
        return;
      }

    fs=FileSystemDumbFSNew(addr,0,&error);
    if (!fs)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&error,"fileProcess(): FileSystemDumbFSNew(): %s\n",
                PackosErrorToString(error));
        UdpSocketClose(socket,&tmp);
        return;
      }

    f=FileOpen(fs,"foo.txt",fileOpenFlagRead,&error);
    if (!f)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&error,"fileProcess(): FileOpen(): %s\n",
                PackosErrorToString(error));
        UdpSocketClose(socket,&tmp);
        return;
      }

    while (true)
      {
        char buff[512];
        int actual=FileRead(f,buff,sizeof(buff),&error);
        if (actual<0)
          {
            PackosError tmp;
            if (error==packosErrorEndOfFile)
              {
                UtilPrintfStream(errStream,&error,"fileProcess(): end of file\n");
                break;
              }
            UtilPrintfStream(errStream,&error,"fileProcess(): FileRead(): %s\n",
                    PackosErrorToString(error));
            FileClose(f,&tmp);
            UdpSocketClose(socket,&tmp);
            return;
          }
        UtilPrintfStream(errStream,&error,"fileProcess(): FileRead(): read %d bytes:\n",actual);
        StreamWrite(errStream,buff,actual,&error);
      }

    if (FileClose(f,&error)<0)
      UtilPrintfStream(errStream,&error,"fileProcess(): FileClose(): %s\n",
              PackosErrorToString(error));
    if (UdpSocketClose(socket,&error)<0)
      UtilPrintfStream(errStream,&error,"fileProcess(): UdpSocketClose(): %s\n",
              PackosErrorToString(error));
  }
}

PackosAddress routerAddr;

int SchedulerCallbackCreateProcesses(PackosContextQueue contexts,
                                     PackosError* error)
{
  PackosContext file;
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

  file=PackosContextNew(fileProcess,"file",error);
  if (!file)
    {
      UtilPrintfStream(errStream,error,"PackosContextNew(file): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  PackosContextQueueAppend(contexts,file,error);

  return 0;
}

int testMain(int argc, const char* argv[])
{
  PackosError error;

  PackosKernelLoop(PackosContextNew(SchedulerBasic,"scheduler",&error),&error);
  UtilPrintfStream(errStream,&error,"Loop terminated\n");
  return 0;
}
