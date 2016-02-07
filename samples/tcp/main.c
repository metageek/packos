#include <assert.h>
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
#include <file-system-dumbfs.h>

static const uint16_t serverPort=5100;
PackosAddress routerAddr;

static void serverProcess(void)
{
  PackosError error;
  TcpSocket clientSocket=0;
  PackosAddress myAddr;

  if (UtilArenaInit(&error)<0)
    return;

  myAddr=PackosMyAddress(&error);
  if (error!=packosErrorNone)
    {
      UtilPrintfStream(errStream,&error,"serverProcess(): PackosMyAddress(): %s\n",
              PackosErrorToString(error));
      return;
    }

  {
    IpIface iface=IpIfaceNativeNew(&error);
    if (!iface)
      {
        UtilPrintfStream(errStream,&error,
                "serverProcess(): IpIfaceNativeNew: %s\n",
                PackosErrorToString(error));
        return;
      }

    if (IpIfaceRegister(iface,&error)<0)
      {
        UtilPrintfStream(errStream,&error,
                "serverProcess(): IpIfaceRegister: %s\n",
                PackosErrorToString(error));
        return;
      }

    if (IpSetDefaultRoute(routerAddr,iface,&error)<0)
      {
        UtilPrintfStream(errStream,&error,
                "serverProcess(): IpSetDefaultRoute(): %s\n",
                PackosErrorToString(error));
        return;
      }
  }

  TcpSocket socket=TcpSocketNew(&error);
  if (!socket)
    {
      UtilPrintfStream(errStream,&error,"serverProcess(): TcpSocketNew(): %s\n",
              PackosErrorToString(error));
      return;
    }

  if (TcpSocketBind(socket,myAddr,serverPort,&error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&error,"serverProcess(): TcpSocketBind(): %s\n",
              PackosErrorToString(error));
      TcpSocketClose(socket,&tmp);
      return;
    }

  if (TcpSocketListen(socket,&error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&error,"serverProcess(): TcpSocketListen(): %s\n",
              PackosErrorToString(error));
      TcpSocketClose(socket,&tmp);
      return;
    }

  while (!clientSocket)
    {
      clientSocket=TcpSocketAccept(socket,&error);
      if (!clientSocket)
        {
          UtilPrintfStream(errStream,&error,"serverProcess(): TcpSocketAccept(): %s\n",
                  PackosErrorToString(error));
          if (error==packosErrorConnectionResetByPeer)
            continue;

          TcpSocketClose(socket,&error);
          return;
        }
    }

  UtilPrintfStream(errStream,&error,"serverProcess(): accepted\n");
  while (true)
    {
      const char* msg="Greetings!";
      const int msgLen=UtilStrlen(msg);

      {
        int actual=TcpSocketSend(clientSocket,msg,msgLen,&error);
        if (actual<0)
          {
            UtilPrintfStream(errStream,&error,"serverProcess(): TcpSocketSend(): %s\n",
                    PackosErrorToString(error));
            break;
          }
        else
          {
            if (actual<msgLen)
              UtilPrintfStream(errStream,&error,
                      "serverProcess(): TcpSocketSend(): sent %d of %d bytes\n",
                      actual,msgLen);
          }
      }

      {
        char buff[2048];
        const int nbytes=sizeof(buff);
        int actual=TcpSocketReceive(clientSocket,buff,nbytes-1,&error);
        if (actual<0)
          {
            UtilPrintfStream(errStream,&error,"serverProcess(): TcpSocketReceive(): %s\n",
                    PackosErrorToString(error));
            break;
          }
        else
          {
            buff[actual]=0;
            UtilPrintfStream(errStream,&error,"serverProcess(): received %d bytes:\n\t%s\n",
                    actual,buff);
          }
      }

      TcpPoll(&error);
    }
}

int SchedulerCallbackCreateProcesses(PackosContextQueue contexts,
                                     PackosError* error)
{
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

  PackosContext server=PackosContextNew(serverProcess,"server",error);
  if (!server)
    {
      UtilPrintfStream(errStream,error,"PackosContextNew(server): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  PackosContextQueueAppend(contexts,server,error);

  return 0;
}

int main(int argc, const char* argv[])
{
  PackosError error;
  assert(sizeof(PackosAddress)==16);

  PackosKernelLoop(PackosContextNew(SchedulerBasic,"scheduler",&error),&error);
  UtilPrintfStream(errStream,&error,"Loop terminated\n");
  return 0;
}
