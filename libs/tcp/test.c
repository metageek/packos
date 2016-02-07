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
#include <file-system-dumbfs.h>

static PackosAddress serverAddr;
static const uint16_t serverPort=5100;

static void clientProcess(void)
{
  PackosError error;

  PackosAddress myAddr=PackosMyAddress(&error);
  if (error!=packosErrorNone)
    {
      UtilPrintfStream(errStream,&error,"clientProcess(): PackosMyAddress(): %s\n",
              PackosErrorToString(error));
      return;
    }

  {
    IpIface iface=IpIfaceNativeNew(&error);
    if (!iface)
      {
        UtilPrintfStream(errStream,&error,
                "clientProcess(): IpIfaceNativeNew: %s\n",
                PackosErrorToString(error));
        return;
      }

    if (IpIfaceRegister(iface,&error)<0)
      {
        UtilPrintfStream(errStream,&error,
                "clientProcess(): IpIfaceRegister: %s\n",
                PackosErrorToString(error));
        return;
      }
  }

  TcpSocket socket=TcpSocketNew(&error);
  if (!socket)
    {
      UtilPrintfStream(errStream,&error,"clientProcess(): TcpSocketNew(): %s\n",
              PackosErrorToString(error));
      return;
    }

  if (TcpSocketBind(socket,myAddr,0,&error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&error,"clientProcess(): TcpSocketBind(): %s\n",
              PackosErrorToString(error));
      TcpSocketClose(socket,&tmp);
      return;
    }

  if (TcpSocketConnect(socket,serverAddr,serverPort,&error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&error,"clientProcess(): TcpSocketConnect(): %s\n",
              PackosErrorToString(error));
      TcpSocketClose(socket,&tmp);
      return;
    }

  UtilPrintfStream(errStream,&error,"clientProcess(): connected\n");

  while (true)
    {
      char buff[1024];
      int nbytes=sizeof(buff)-1;
      int actual=TcpSocketReceive(socket,buff,nbytes,&error);
      if (actual<0)
        {
          UtilPrintfStream(errStream,&error,"clientProcess(): TcpSocketReceive(): %s\n",
                  PackosErrorToString(error));
          TcpSocketClose(socket,&error);
          return;
        }
      else
        {
          buff[actual]=0;
          UtilPrintfStream(errStream,&error,"clientProcess(): received %d bytes:\n\t%s\n",
                  actual,buff);
        }

      TcpPoll(&error);
    }
}

static void serverProcess(void)
{
  PackosError error;
  TcpSocket clientSocket;

  PackosAddress myAddr=PackosMyAddress(&error);
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

  clientSocket=TcpSocketAccept(socket,&error);
  if (!clientSocket)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&error,"serverProcess(): TcpSocketAccept(): %s\n",
              PackosErrorToString(error));
      TcpSocketClose(socket,&tmp);
      return;
    }

  UtilPrintfStream(errStream,&error,"serverProcess(): accepted\n");
  while (true)
    {
      const char* msg="Greetings!";
      const int msgLen=UtilStrlen(msg);

      {
        int actual=TcpSocketSend(clientSocket,msg,msgLen,&error);
        if (actual<0)
          UtilPrintfStream(errStream,&error,"serverProcess(): TcpSocketSend(): %s\n",
                  PackosErrorToString(error));
        else
          {
            if (actual<msgLen)
              UtilPrintfStream(errStream,&error,
                      "serverProcess(): TcpSocketSend(): sent %d of %d bytes\n",
                      actual,msgLen);
          }
      }

      TcpPoll(&error);
    }
}

int SchedulerCallbackCreateProcesses(PackosContextQueue contexts,
                                     PackosError* error)
{
  PackosContext server=PackosContextNew(serverProcess,"server",error);
  if (!server)
    {
      UtilPrintfStream(errStream,error,"PackosContextNew(server): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  PackosContextQueueAppend(contexts,server,error);

  serverAddr=PackosContextGetAddr(server,error);
  if (*error!=packosErrorNone)
    {
      UtilPrintfStream(errStream,error,"PackosContextGetAddr(server): %s\n",
	      PackosErrorToString(*error));
      return -1;
    }

  PackosContext client=PackosContextNew(clientProcess,"client",error);
  if (!client)
    {
      UtilPrintfStream(errStream,error,"PackosContextNew(client): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  PackosContextQueueAppend(contexts,client,error);

  return 0;
}

int main(int argc, const char* argv[])
{
  PackosError error;
  assert(sizeof(PackosAddress)==16);
  if (UtilArenaInitStatic(&error)<0) return 1;

  PackosSimContextLoop(PackosContextNew(SchedulerBasic,"scheduler",&error),
                       &error);
  UtilPrintfStream(errStream,&error,"Loop terminated\n");
  return 0;
}
