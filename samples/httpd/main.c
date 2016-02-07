#include <assert.h>
#include <util/stream.h>
#include <util/stream-string.h>
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

#define USE_FILES

typedef enum {
  httpMethodGET=1,
  httpMethodPUT,
  httpMethodPOST,
  httpMethodUnknown
} HttpMethod;

typedef enum {
  httpStatusOK=200,
  httpStatusMethodUnimplemented=501,
  httpStatusFileNotFound=404,
  httpStatusForbidden=403,
  httpStatusUnknownError=500
} HttpStatus;

typedef struct {
  HttpMethod method;
  HttpStatus status;
  char path[1024];
} HttpRequest;

#ifndef USE_FILES
static struct {
  const char* name;
  const char* text;
} files[]={{"index.html",
            "<html>\n"
            "<head>\n"
            "<title>PackOS Sample HTTP Server</title>\n"
            "</head>\n"
            "\n"
            "<body>\n"
            "<h1>PackOS Sample HTTP Server</h1>\n"
            "\n"
            "<p>Whee!</p>\n"
            "\n"
            "<ul>\n"
            "<li><a href=\"thesis-idea.html\">Thesis idea</a></li>\n"
            "</ul>\n"
            "\n"
            "</body>\n"
            "</html>\n"
},
           {"thesis-idea.html",
            "<html>\n"
            "<head>\n"
            "<title>PackOS: Thesis Idea</title>\n"
            "</head>\n"
            "\n"
            "<body>\n"
            "<h1>PackOS: Thesis Idea</h1>\n"
            "\n"
            "<p>My thesis idea is to create a microkernel. When I read Joachim\n"
            "Liedtke's papers on L4, the ideas made sense to me, except that the\n"
            "IPC system struck me as excessively complicated. I started sketching\n"
            "out ideas for a microkernel that would be more minimal than L4, with\n"
            "IPC based on IPv6. In 91.516, I've started implementing this\n"
            "microkernel, albeit emulated in a Linux process. I call it PackOS.\n"
            "</p>\n"
            "\n"
            "<p>PackOS is highly minimal. The only kernel services are context\n"
            "switching, IPv6 packet delivery, and interrupt dispatch. Along with\n"
            "lock-free programming techniques, the result should be a system that\n"
            "spends very little time in the kernel, which makes for some\n"
            "interesting simplifications. There are no kernel threads; everything\n"
            "done in the kernel is O(1), and runs to completion before returning to\n"
            "user space. Also, it is my hope that minimizing time spent in the\n"
            "kernel will allow me to use a monolithic kernel lock, even on an SMP\n"
            "machine. The result is that the trusted kernel code can be very small\n"
            "and very simple. All drivers will live in user space (similar to L4,\n"
            "Minix 3.0, and some other modern systems); interrupts will be\n"
            "delivered to the drivers as packets.\n"
            "</p>\n"
            "\n"
            "<p>As of this writing, my user-space PackOS has basic IPv6, UDP, and\n"
            "the beginnings of TCP. It has an interface to the outside network, via\n"
            "shared memory and the Linux TUN driver, and a primitive filesystem. My\n"
            "goal for my 91.516 project is to complete the TCP implementation and\n"
            "add an HTTP server.\n"
            "</p>\n"
            "\n"
            "<p>For my thesis, I would port PackOS to native hardware (probably x86\n"
            "and PowerPC), and develop it enough to be able to understand where it\n"
            "lies in the flexibility/performance tradeoff. (A nice goal would be to\n"
            "implement a POSIX layer, but I'm not sure that would teach me enough\n"
            "to be worth the extensive effort.) I would, of course, have to do some\n"
            "research on existing microkernels, such as L4, Minix, and Coyotos.\n"
            "</p>\n"
            "\n"
            "</body>\n"
            "</html>\n"
           },
           {0,0}
};
#endif

static const char* httpStatusString(HttpStatus status)
{
  switch (status)
    {
    case httpStatusOK:
      return "OK";

    case httpStatusMethodUnimplemented:
      return "Method Not Implemented";

    case httpStatusFileNotFound:
      return "Not Found";

    case httpStatusForbidden:
      return "Forbidden";

    case httpStatusUnknownError:
      return "Unknown Error";

    default:
      return "Unknown Status Code";
    }
}

static int readLine(TcpSocket httpClient,
                    char* buff,
                    int buffsize,
                    PackosError* error)
{
  int offset;
  for (offset=0; offset<=buffsize; offset++)
    {
      if (TcpSocketReceive(httpClient,buff+offset,1,error)<0)
        {
          UtilPrintfStream(errStream,error,
                  "httpProcess(): readLine(): TcpSocketReceive(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }
      if (buff[offset]=='\n')
        {
          if ((offset>0) && (buff[offset-1]=='\r'))
            offset--;
          buff[offset]=0;
          return offset;
        }
    }

  UtilPrintfStream(errStream,error,"httpProcess(): readLine(): line too long\n");
  *error=packosErrorBadProtocolCmd;
  return -1;
}

static int parseRequest(TcpSocket httpClient,
                        HttpRequest* request,
                        PackosError* error)
{
  char line[1024];
  bool isFirst=true;
  do {
    if (readLine(httpClient,line,sizeof(line),error)<0)
      {
        UtilPrintfStream(errStream,error,"httpProcess(): parseRequest(): readLine(): %s\n",
                PackosErrorToString(*error));
        return -1;
      }
        
    UtilPrintfStream(errStream,error,"parseRequest(): %s\n",line);

    if (isFirst)
      {
        isFirst=false;

        char* space1=UtilStrchr(line,' ');
        if (!space1)
          {
            UtilPrintfStream(errStream,error,"httpProcess(): parseRequest(): request line has no space in it\n");
            *error=packosErrorBadProtocolCmd;
            return -1;
          }

        *space1=0;
        if (!UtilStrcmp(line,"GET"))
          request->method=httpMethodGET;
        else
          {
            if (!UtilStrcmp(line,"PUT"))
              request->method=httpMethodPUT;
            else
              {
                if (!UtilStrcmp(line,"POST"))
                  request->method=httpMethodPOST;
                else
                  request->method=httpMethodUnknown;
              }
          }

        char* space2=0;
        if ((space1-line)<sizeof(line))
          space2=UtilStrchr(space1+1,' ');

        if (!space2)
          {
            UtilPrintfStream(errStream,error,"httpProcess(): parseRequest(): request line has no second space in it\n");
            *error=packosErrorBadProtocolCmd;
            return -1;
          }

        *space2=0;
        UtilStrcpy(request->path,space1+1);

        if ((request->path[0]!='/') || (!(request->path[1])))
          {
            UtilPrintfStream(errStream,error,"httpProcess(): parseRequest(): invalid path %s\n",
                    request->path);
            *error=packosErrorBadProtocolCmd;
            return -1;
          }
      }
  } while (line[0]);

  return 0;
}

static const char* getMimeType(const char* path)
{
  const char* lastSlash=UtilStrrchr(path,'/');
  const char* lastPeriod=UtilStrrchr(path,'.');
  const char* suffix;

  if (!lastPeriod) return 0;
  if (lastSlash && (lastPeriod<lastSlash)) return 0;

  suffix=lastPeriod+1;
  if (!(suffix[0])) return 0;

  {
    static const struct {
      const char* suffix;
      const char* type;
    } config[]={{"txt","text/plain"},
                {"html","text/html"},
                {"htm","text/html"},
                {"xhtml","application/xhtml+xml"},
                {"css","text/css"},
                {"js","application/x-javascript"},
                {"svg","image/svg+xml"},
                {"gif","image/gif"},
                {"png","image/png"},
                {"jpg","image/jpeg"},
                {"jpeg","image/jpeg"}
    };
    const int N=sizeof(config)/sizeof(config[0]);
    int i;
    for (i=0; i<N; i++)
      {
        if (!UtilStrcasecmp(suffix,config[i].suffix))
          return config[i].type;
      }
  }

  return 0;
}

static void httpProcess(void)
{
  PackosError error;
  PackosAddress myAddr;

  if (UtilArenaInit(&error)<0)
    return;

  myAddr=PackosMyAddress(&error);
  if (error!=packosErrorNone)
    {
      UtilPrintfStream(errStream,&error,"httpProcess(): PackosMyAddress(): %s\n",
              PackosErrorToString(error));
      return;
    }

  {
    extern PackosAddress routerAddr;
    IpIface iface=IpIfaceNativeNew(&error);
    if (!iface)
      {
        UtilPrintfStream(errStream,&error,
                "httpProcess(): IpIfaceNativeNew: %s\n",
                PackosErrorToString(error));
        return;
      }

    if (IpIfaceRegister(iface,&error)<0)
      {
        UtilPrintfStream(errStream,&error,
                "httpProcess(): IpIfaceRegister: %s\n",
                PackosErrorToString(error));
        return;
      }

    if (IpSetDefaultRoute(routerAddr,iface,&error)<0)
      {
        UtilPrintfStream(errStream,&error,
                "httpProcess(): IpSetDefaultRoute(): %s\n",
                PackosErrorToString(error));
        return;
      }

  }

  TcpSocket httpSocket=TcpSocketNew(&error);
  if (!httpSocket)
    {
      UtilPrintfStream(errStream,&error,"httpProcess(): TcpSocketNew(): %s\n",
              PackosErrorToString(error));
      return;
    }

  if (TcpSocketBind(httpSocket,myAddr,80,&error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&error,"httpProcess(): TcpSocketBind(): %s\n",
              PackosErrorToString(error));
      TcpSocketClose(httpSocket,&tmp);
      return;
    }

  if (TcpSocketListen(httpSocket,&error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&error,"httpProcess(): TcpSocketListen(): %s\n",
              PackosErrorToString(error));
      TcpSocketClose(httpSocket,&tmp);
      return;
    }

  UdpSocket fileSocket=UdpSocketNew(&error);
  if (!fileSocket)
    {
      UtilPrintfStream(errStream,&error,"httpProcess(): UdpSocketNew(): %s\n",
              PackosErrorToString(error));
      return;
    }

  if (UdpSocketBind(fileSocket,PackosAddrGetZero(),0,&error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&error,"httpProcess(): UdpSocketBind(): %s\n",
              PackosErrorToString(error));
      UdpSocketClose(fileSocket,&tmp);
      return;
    }

    FileSystem fs;

    PackosAddress addr=PackosAddrFromString("7e8f::02",&error);
    if (error!=packosErrorNone)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&error,"httpProcess(): PackosAddrFromString(): %s\n",
                PackosErrorToString(error));
        UdpSocketClose(fileSocket,&tmp);
        return;
      }

    fs=FileSystemDumbFSNew(addr,0,&error);
    if (!fs)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&error,"httpProcess(): FileSystemDumbFSNew(): %s\n",
                PackosErrorToString(error));
        UdpSocketClose(fileSocket,&tmp);
        return;
      }

  while (true)
    {
      HttpRequest request;
      struct {
        int status;
#ifdef USE_FILES
        File f;
#else
        const char* text;
#endif
        const char* contentType;
      } reply={0,0,0};
      TcpSocket httpClient=TcpSocketAccept(httpSocket,&error);
      if (!httpClient)
        {
          UtilPrintfStream(errStream,&error,"httpProcess(): TcpSocketAccept(): %s\n",
                  PackosErrorToString(error));
          break;
        }

      UtilPrintfStream(errStream,&error,"httpProcess(): accepted\n");

      if (parseRequest(httpClient,&request,&error)<0)
        {
          UtilPrintfStream(errStream,&error,"httpProcess(): parseRequest(): %s\n",
                  PackosErrorToString(error));
          TcpSocketClose(httpClient,&error);
          continue;
        }

      if (request.method!=httpMethodGET)
        reply.status=httpStatusMethodUnimplemented;
      else
        {
#ifdef USE_FILES
          reply.f=FileOpen(fs,request.path+1,fileOpenFlagRead,&error);
          if (!reply.f)
            {
              UtilPrintfStream(errStream,&error,"httpProcess(): FileOpen(): %s\n",
                      PackosErrorToString(error));
              switch (error)
                {
                case packosErrorDoesNotExist:
                  reply.status=httpStatusFileNotFound;
                  break;

                case packosErrorAccessDenied:
                  reply.status=httpStatusForbidden;
                  break;

                default:
                  reply.status=httpStatusUnknownError;
                  break;
                }
            }
          else
            {
              reply.contentType=getMimeType(request.path);
              reply.status=httpStatusOK;
            }
#else
          int i;
          reply.text=0;
          for (i=0; files[i].name && files[i].text; i++)
            if (!UtilStrcmp(files[i].name,request.path+1))
              {
                reply.text=files[i].text;
                break;
              }
          if (reply.text)
            {
              reply.contentType=getMimeType(request.path);
              reply.status=httpStatusOK;
            }
          else
            reply.status=httpStatusFileNotFound;
#endif
        }

      {
        char statusLine[80];
        Stream s=StreamStringOpen(statusLine,sizeof(statusLine),&error);
        if (!s)
          {
            UtilPrintfStream(errStream,&error,"httpProcess(): StreamStringOpen(): %s\n",
                    PackosErrorToString(error));
            TcpSocketClose(httpClient,&error);
            continue;
          }
        
        UtilPrintfStream(s,&error,"HTTP/1.0 %d %s\n",reply.status,
                         httpStatusString(reply.status));
        if (TcpSocketSend(httpClient,statusLine,UtilStrlen(statusLine),&error)<0)
          {
            UtilPrintfStream(errStream,&error,"httpProcess(): TcpSocketSend(statusLine): %s\n",
                    PackosErrorToString(error));
            TcpSocketClose(httpClient,&error);
            continue;
          }
      }

      if (reply.contentType)
        {
          char buff[512];
          Stream s=StreamStringOpen(buff,sizeof(buff),&error);
          if (!s)
            {
              UtilPrintfStream(errStream,&error,"httpProcess(): StreamStringOpen(): %s\n",
                               PackosErrorToString(error));
              TcpSocketClose(httpClient,&error);
              continue;
            }

          UtilPrintfStream(s,&error,"Content-type: %s\n",reply.contentType);
          if (TcpSocketSend(httpClient,buff,UtilStrlen(buff),&error)<0)
            {
              UtilPrintfStream(errStream,&error,"httpProcess(): TcpSocketSend(buff): %s\n",
                      PackosErrorToString(error));
              TcpSocketClose(httpClient,&error);
              continue;
            }
        }

      {
        const char* fixedHeaders="Connection: close\nServer: sample-httpd/0.1 (PackOS)\n\n";
        if (TcpSocketSend(httpClient,fixedHeaders,UtilStrlen(fixedHeaders),&error)<0)
          {
            UtilPrintfStream(errStream,&error,"httpProcess(): TcpSocketSend(fixedHeaders): %s\n",
                    PackosErrorToString(error));
            TcpSocketClose(httpClient,&error);
            continue;
          }
      }

#ifdef USE_FILES
      while (reply.f)
        {
          char buff[512];
          int actual=FileRead(reply.f,buff,sizeof(buff),&error);
          if (actual<0)
            {
              PackosError tmp;
              FileClose(reply.f,&tmp);
              reply.f=0;

              UtilPrintfStream(errStream,&error,"httpProcess(): FileRead(): %s\n",
                      PackosErrorToString(error));
              continue;
            }
          UtilPrintfStream(errStream,&error,"httpProcess(): FileRead(): read %d bytes:\n",actual);

          if (TcpSocketSend(httpClient,buff,actual,&error)<0)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,&error,"httpProcess(): TcpSocketSend(buff): %s\n",
                      PackosErrorToString(error));
              TcpSocketClose(httpClient,&error);
              FileClose(reply.f,&tmp);
              reply.f=0;
              continue;
            }
        }
#else
      if (reply.text)
        {
          int offset=0,nbytes=UtilStrlen(reply.text);
          while (nbytes>0)
            {
              int actual=TcpSocketSend(httpClient,
                                       reply.text+offset,nbytes,
                                       &error);
              if (actual<0)
                {
                  UtilPrintfStream(errStream,&error,"httpProcess(): TcpSocketSend(buff): %s\n",
                          PackosErrorToString(error));
                  TcpSocketClose(httpClient,&error);
                  nbytes=0;
                  continue;
                }
              offset+=actual;
              nbytes-=actual;
            }
        }
#endif
      TcpSocketClose(httpClient,&error);
  }

  if (UdpSocketClose(fileSocket,&error)<0)
    UtilPrintfStream(errStream,&error,"httpProcess(): UdpSocketClose(): %s\n",
            PackosErrorToString(error));
}

PackosAddress routerAddr;

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

  PackosContext httpServer=PackosContextNew(httpProcess,"httpServer",error);
  if (!httpServer)
    {
      UtilPrintfStream(errStream,error,"PackosContextNew(httpServer): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  PackosContextQueueAppend(contexts,httpServer,error);

  return 0;
}

int main(int argc, const char* argv[])
{
  PackosError error;
  PackosContext scheduler;
  assert(sizeof(PackosAddress)==16);

  scheduler=PackosContextNew(SchedulerBasic,"scheduler",&error);
  if (!scheduler)
    {
      UtilPrintfStream(errStream,&error,"PackosContextNew(): %s\n",PackosErrorToString(error));
      return 1;
    }

  PackosSimContextLoop(scheduler,&error);
  UtilPrintfStream(errStream,&error,"Loop terminated\n");
  return 0;
}
