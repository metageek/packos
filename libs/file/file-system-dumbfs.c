#include <util/string.h>
#include <util/stream.h>
#include <util/alloc.h>

#include <file-system-dumbfs.h>
#include <file-system-dumbfs-protocol.h>
#include <file-systemP.h>
#include <fileP.h>
#include <udp.h>
#include <timer.h>

extern PackosAddress routerAddr;

static int OpenMethod(FileSystem fs,
                      File file,
                      const char* path,
                      uint32_t flags,
                      PackosError* error);
static int CloseMethod(FileSystem fs,
                       File file,
                       PackosError* error);
static int ReadMethod(FileSystem fs,
                      File file,
                      void* buffer,
                      uint32_t count,
                      PackosError* error);
static int WriteMethod(FileSystem fs,
                       File file,
                       const void* buffer,
                       uint32_t count,
                       PackosError* error);
static int DeleteMethod(FileSystem fs,
                        const char* path,
                        PackosError* error);
static int RenameMethod(FileSystem fs,
                        const char* pathFrom,
                        const char* pathTo,
                        PackosError* error);
static int Destructor(FileSystem fs,
                      PackosError* error);

typedef struct {
  UdpSocket requestSocket;
  UdpSocket timerSocket;
  Timer timer;
  PackosAddress addr;
  uint32_t port;
  uint16_t nextRequestId;
} DumbFSContext;

typedef struct {
  uint32_t fileId;
} DumbFileContext;

FileSystem FileSystemDumbFSNew(PackosAddress addr,
                               uint32_t port,
                               PackosError* error)
{
  DumbFSContext* context;
  FileSystem fs;

  if (!error) return 0;

  context=(DumbFSContext*)(malloc(sizeof(DumbFSContext)));
  if (!context)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  if (!port) port=DUMBFS_DEFAULT_PORT;

  fs=FileSystemPNew(error);
  if (!fs)
    {
      UtilPrintfStream(errStream,error,"FileSystemDumbFSNew(): FileSystemPNew(): %s\n",
              PackosErrorToString(*error));
      free(context);
      return 0;
    }

  context->requestSocket=UdpSocketNew(error);
  if (!(context->requestSocket))
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,"FileSystemDumbFSNew(): UdpSocketNew(): %s\n",
              PackosErrorToString(*error));
      FileSystemPDelete(fs,&tmp);
      free(context);
      return 0;
    }

  if (UdpSocketBind(context->requestSocket,PackosAddrGetZero(),0,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,"FileSystemDumbFSNew(): UdpSocketBind(): %s\n",
              PackosErrorToString(*error));
      UdpSocketClose(context->requestSocket,&tmp);
      FileSystemPDelete(fs,&tmp);
      free(context);
      return 0;
    }

  context->timerSocket=UdpSocketNew(error);
  if (!(context->timerSocket))
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,"FileSystemDumbFSNew(): UdpSocketNew(): %s\n",
              PackosErrorToString(*error));
      UdpSocketClose(context->requestSocket,&tmp);
      FileSystemPDelete(fs,&tmp);
      free(context);
      return 0;
    }

  if (UdpSocketBind(context->timerSocket,PackosAddrGetZero(),0,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,"FileSystemDumbFSNew(): UdpSocketBind(): %s\n",
              PackosErrorToString(*error));
      UdpSocketClose(context->timerSocket,&tmp);
      UdpSocketClose(context->requestSocket,&tmp);
      FileSystemPDelete(fs,&tmp);
      free(context);
      return 0;
    }

  context->timer=TimerNew(context->timerSocket,1,0,1,true,error);
  if (!(context->timer))
    {
      PackosError tmp;
      UtilPrintfStream(errStream,error,"FileSystemDumbFSNew(): TimerNew(): %s\n",
              PackosErrorToString(*error));
      UdpSocketClose(context->timerSocket,&tmp);
      UdpSocketClose(context->requestSocket,&tmp);
      FileSystemPDelete(fs,&tmp);
      free(context);
      return 0;
    }

  context->addr=addr;
  context->port=port;
  context->nextRequestId=1;

  fs->methods.open=OpenMethod;
  fs->methods.close=CloseMethod;
  fs->methods.read=ReadMethod;
  fs->methods.write=WriteMethod;
  fs->methods.rename=RenameMethod;
  fs->methods.delete=DeleteMethod;
  fs->methods.destructor=Destructor;

  fs->context=context;
  return fs;
}

static int OpenMethod(FileSystem fs,
                      File file,
                      const char* path,
                      uint32_t flags,
                      PackosError* error)
{
  PackosPacket* packet;
  IpHeaderUDP* udpHeader;
  DumbFSContext* context;
  DumbFileContext* fileContext;
  DumbFSRequest* request;
  uint16_t requestId;

  if (!error)
    {
      UtilPrintfStream(errStream,error,"FileSystemDumbFS::OpenMethod(): !error\n");
      return -2;
    }

  if (!(fs && fs->context && file && path && flags))
    {
      UtilPrintfStream(errStream,error,"FileSystemDumbFS::OpenMethod(): invalid arg\n");
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (UtilStrlen(path)>DUMBFS_MAX_PATHLEN)
    {
      UtilPrintfStream(errStream,error,"FileSystemDumbFS::OpenMethod(): path too long\n");
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (flags & ~(fileOpenFlagRead | fileOpenFlagWrite))
    {
      UtilPrintfStream(errStream,error,"file-system-dumbfs: OpenMethod(): unknown flags %u\n",
              flags);
      *error=packosErrorNotImplemented;
      return -1;
    }

  fileContext=(DumbFileContext*)(malloc(sizeof(DumbFileContext)));
  if (!fileContext)
    {
      *error=packosErrorOutOfMemory;
      UtilPrintfStream(errStream,error,"FileSystemDumbFS::OpenMethod(): out of memory\n");
      return -1;
    }

  context=(DumbFSContext*)(fs->context);

  while(true)
    {
      int numTimeouts=0;

      packet=UdpPacketNew(context->requestSocket,sizeof(DumbFSRequest),&udpHeader,error);
      if (!packet)
        {
          free(fileContext);
          UtilPrintfStream(errStream,error,"FileSystemDumbFS::OpenMethod(): UdpPacketNew(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }

      udpHeader->destPort=context->port;
      packet->ipv6.src=PackosMyAddress(error);
      packet->ipv6.dest=context->addr;
      packet->packos.dest=routerAddr;

      request=(DumbFSRequest*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));
      request->cmd=dumbFSRequestCmdOpen;
      request->requestId=requestId=context->nextRequestId++;
      UtilStrcpy(request->args.open.path,path);
      request->args.open.flags=flags;

      if (UdpSocketSend(context->requestSocket,packet,error)<0)
        {
          PackosError tmp;
          PackosPacketFree(packet,&tmp);
          free(fileContext);
          UtilPrintfStream(errStream,error,"FileSystemDumbFS::OpenMethod(): UdpSocketSend(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }

      while (numTimeouts<2)
        {
          packet=UdpSocketReceive(context->requestSocket,0,true,error);
          if (!packet)
            {
              if (((*error)==packosErrorNone)
                  || ((*error)==packosErrorStoppedForOtherSocket)
                  || ((*error)==packosErrorPacketFilteredOut)
                  )
                {
                  if (UdpSocketReceivePending(context->timerSocket,error))
                    {
                      UdpSocketReceive(context->timerSocket,0,false,error);
                      numTimeouts++;
                    }
                  continue;
                }
              else
                {
                  free(fileContext);
                  UtilPrintfStream(errStream,error,"FileSystemDumbFS::OpenMethod(): UdpSocketReceive(): %s\n",
                          PackosErrorToString(*error));
                  return -1;
                }
            }

          udpHeader=UdpPacketSeekHeader(packet,error);
          if (!udpHeader)
            {
              PackosError tmp;
              free(fileContext);
              UtilPrintfStream(errStream,error,"FileSystemDumbFS::OpenMethod(): UdpPacketSeekHeader(): %s (can't happen)\n",
                      PackosErrorToString(*error));
              PackosPacketFree(packet,&tmp);
              return -1;
            }

          {
            DumbFSReply* reply
              =(DumbFSReply*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));
            if (!((reply->cmd==dumbFSRequestCmdOpen)
                  && (reply->requestId==requestId)
                  )
                )
              {
                PackosPacketFree(packet,error);
                continue;
              }

            if (reply->errorAsInt)
              {
                free(fileContext);
                *error=(PackosError)(reply->errorAsInt);
                UtilPrintfStream(errStream,error,"FileSystemDumbFS::OpenMethod(): received %s\n",
                        PackosErrorToString(*error));
                return -1;
              }

            fileContext->fileId=reply->args.open.fileId;
            file->context=fileContext;
            return 0;
          }      
        }
    }

  return -1;
}

static int CloseMethod(FileSystem fs,
                       File file,
                       PackosError* error)
{
  PackosPacket* packet;
  IpHeaderUDP* udpHeader;
  DumbFSContext* context;
  DumbFileContext* fileContext;
  DumbFSRequest* request;
  uint16_t requestId;

  if (!error) return -2;
  if (!(fs && fs->context && file && file->context))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  fileContext=(DumbFileContext*)(file->context);
  if (!fileContext)
    {
      *error=packosErrorOutOfMemory;
      return -1;
    }

  context=(DumbFSContext*)(fs->context);

  UtilPrintfStream(errStream,error,"CloseMethod(%p: %d)\n",file,fileContext->fileId);

  while(true)
    {
      int numTimeouts=0;

      packet=UdpPacketNew(context->requestSocket,sizeof(DumbFSRequest),&udpHeader,error);
      if (!packet)
        {
          UtilPrintfStream(errStream,error,"FileSystemDumbFS::CloseMethod(): UdpPacketNew(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }

      packet=UdpPacketNew(context->requestSocket,sizeof(DumbFSRequest),&udpHeader,error);
      if (!packet)
        {
          UtilPrintfStream(errStream,error,"FileSystemDumbFS::CloseMethod(): UdpPacketNew(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }

      udpHeader->destPort=context->port;
      packet->ipv6.src=PackosMyAddress(error);
      packet->ipv6.dest=context->addr;
      packet->packos.dest=routerAddr;

      request=(DumbFSRequest*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));
      request->cmd=dumbFSRequestCmdClose;
      request->requestId=requestId=context->nextRequestId++;
      request->args.close.fileId=fileContext->fileId;

      if (UdpSocketSend(context->requestSocket,packet,error)<0)
        {
          PackosError tmp;
          PackosPacketFree(packet,&tmp);
          UtilPrintfStream(errStream,error,"FileSystemDumbFS::CloseMethod(): UdpSocketSend(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }

      while (numTimeouts<2)
        {
          packet=UdpSocketReceive(context->requestSocket,0,true,error);
          if (!packet)
            {
              if (((*error)==packosErrorNone)
                  || ((*error)==packosErrorStoppedForOtherSocket)
                  || ((*error)==packosErrorPacketFilteredOut)
                  )
                {
                  if (UdpSocketReceivePending(context->timerSocket,error))
                    {
                      UdpSocketReceive(context->timerSocket,0,false,error);
                      numTimeouts++;
                    }
                  continue;
                }
              else
                {
                  UtilPrintfStream(errStream,error,"FileSystemDumbFS::CloseMethod(): UdpSocketReceive(): %s\n",
                          PackosErrorToString(*error));
                  return -1;
                }
            }

          udpHeader=UdpPacketSeekHeader(packet,error);
          if (!udpHeader)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,error,"FileSystemDumbFS::CloseMethod(): UdpPacketSeekHeader(): %s (can't happen)\n",
                      PackosErrorToString(*error));
              PackosPacketFree(packet,&tmp);
              return -1;
            }

          {
            DumbFSReply* reply
              =(DumbFSReply*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));
            if (!((reply->cmd==dumbFSRequestCmdClose)
                  && (reply->requestId==requestId)
                  )
                )
              {
                PackosPacketFree(packet,error);
                continue;
              }

            if (reply->errorAsInt)
              {
                *error=(PackosError)(reply->errorAsInt);
                UtilPrintfStream(errStream,error,"FileSystemDumbFS::CloseMethod(): received %s\n",
                        PackosErrorToString(*error));
                return -1;
              }

            return 0;
          }      
        }
    }

  return 0;
}

static int ReadMethod(FileSystem fs,
                      File file,
                      void* buffer,
                      uint32_t count,
                      PackosError* error)
{
  PackosPacket* packet;
  IpHeaderUDP* udpHeader;
  DumbFSContext* context;
  DumbFileContext* fileContext;
  DumbFSRequest* request;
  uint16_t requestId;

  if (!error) 
    {
      UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): !error\n");
      return -2;
    }

  if (!(fs && fs->context && file && file->context && buffer))
    {
      *error=packosErrorInvalidArg;
      if (!fs)
        UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): !fs\n");
      if (!(fs->context))
        UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): !(fs->context)\n");
      if (!file)
        UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): !file\n");
      if (!(file->context))
        UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): !(file->context)\n");
      if (!buffer)
        UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): !buffer\n");
      return -1;
    }

  if (!count) return 0;

  if (count>DUMBFS_MAX_BUFFER) count=DUMBFS_MAX_BUFFER;

  fileContext=(DumbFileContext*)(file->context);
  if (!fileContext)
    {
      *error=packosErrorOutOfMemory;
      UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): out of memory\n");
      return -1;
    }

  context=(DumbFSContext*)(fs->context);

  while (true)
    {
      int numTimeouts=0;
      packet=UdpPacketNew(context->requestSocket,sizeof(DumbFSRequest),&udpHeader,error);
      if (!packet)
        {
          UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): UdpPacketNew(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }

      udpHeader->destPort=context->port;
      packet->ipv6.src=PackosMyAddress(error);
      packet->ipv6.dest=context->addr;
      packet->packos.dest=routerAddr;

      request=(DumbFSRequest*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));
      request->cmd=dumbFSRequestCmdRead;
      request->requestId=requestId=context->nextRequestId++;
      request->args.read.fileId=fileContext->fileId;
      request->args.read.pos=file->pos;
      request->args.read.count=count;

      if (UdpSocketSend(context->requestSocket,packet,error)<0)
        {
          PackosError tmp;
          PackosPacketFree(packet,&tmp);
          UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): UdpSocketSend(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }

      while (numTimeouts<2)
        {
          packet=UdpSocketReceive(context->requestSocket,0,true,error);
          if (!packet)
            {
              if (((*error)==packosErrorNone)
                  || ((*error)==packosErrorStoppedForOtherSocket)
                  || ((*error)==packosErrorPacketFilteredOut)
                  )
                {
                  if (UdpSocketReceivePending(context->timerSocket,error))
                    {
                      packet=UdpSocketReceive(context->timerSocket,0,
                                              false,error);
                      if (packet)
                        PackosPacketFree(packet,error);
                      numTimeouts++;
                    }
                  continue;
                }
              else
                {
                  UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): UdpSocketReceive(): %s\n",
                          PackosErrorToString(*error));
                  return -1;
                }
            }

          udpHeader=UdpPacketSeekHeader(packet,error);
          if (!udpHeader)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): UdpPacketSeekHeader(): %s (can't happen)\n",
                      PackosErrorToString(*error));
              PackosPacketFree(packet,&tmp);
              return -1;
            }

          {
            DumbFSReply* reply
              =(DumbFSReply*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));
            if (!((reply->cmd==dumbFSRequestCmdRead)
                  && (reply->requestId==requestId)
                  )
                )
              {
                PackosPacketFree(packet,error);
                continue;
              }

            if (reply->errorAsInt)
              {
                PackosPacketFree(packet,error);
                *error=(PackosError)(reply->errorAsInt);
                UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): received %s\n",
                        PackosErrorToString(*error));
                return -1;
              }

            UtilMemcpy(buffer,reply->args.read.buff,reply->args.read.actual);
            file->pos+=reply->args.read.actual;
            UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): success\n");
            PackosPacketFree(packet,error);
            return reply->args.read.actual;
          }      
        }
    }

  UtilPrintfStream(errStream,error,"FileSystemDumbFS::ReadMethod(): loop ended; can't happen\n");
  return 0;
}

static int WriteMethod(FileSystem fs,
                       File file,
                       const void* buffer,
                       uint32_t count,
                       PackosError* error)
{
  PackosPacket* packet;
  IpHeaderUDP* udpHeader;
  DumbFSContext* context;
  DumbFileContext* fileContext;
  DumbFSRequest* request;
  uint16_t requestId;

  if (!error) return -2;
  if (!(fs && fs->context && file && file->context && buffer))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (!count) return 0;

  if (count>DUMBFS_MAX_BUFFER) count=DUMBFS_MAX_BUFFER;

  fileContext=(DumbFileContext*)(file->context);
  if (!fileContext)
    {
      *error=packosErrorOutOfMemory;
      return -1;
    }

  context=(DumbFSContext*)(fs->context);

  while (true)
    {
      int numTimeouts=0;
      packet=UdpPacketNew(context->requestSocket,sizeof(DumbFSRequest),&udpHeader,error);
      if (!packet)
        {
          UtilPrintfStream(errStream,error,"FileSystemDumbFS::WriteMethod(): UdpPacketNew(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }

      udpHeader->destPort=context->port;
      packet->ipv6.src=PackosMyAddress(error);
      packet->ipv6.dest=context->addr;
      packet->packos.dest=routerAddr;

      request=(DumbFSRequest*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));
      request->cmd=dumbFSRequestCmdWrite;
      request->requestId=requestId=context->nextRequestId++;
      request->args.write.fileId=fileContext->fileId;
      request->args.write.pos=file->pos;
      request->args.write.count=count;
      UtilMemcpy(request->args.write.buff,buffer,count);

      if (UdpSocketSend(context->requestSocket,packet,error)<0)
        {
          PackosError tmp;
          PackosPacketFree(packet,&tmp);
          UtilPrintfStream(errStream,error,"FileSystemDumbFS::WriteMethod(): UdpSocketSend(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }

      while (numTimeouts<2)
        {
          packet=UdpSocketReceive(context->requestSocket,0,true,error);
          if (!packet)
            {
              if (((*error)==packosErrorNone)
                  || ((*error)==packosErrorStoppedForOtherSocket)
                  || ((*error)==packosErrorPacketFilteredOut)
                  )
                {
                  if (UdpSocketReceivePending(context->timerSocket,error))
                    {
                      UdpSocketReceive(context->timerSocket,0,false,error);
                      numTimeouts++;
                    }
                  continue;
                }
               else
                {
                  UtilPrintfStream(errStream,error,"FileSystemDumbFS::WriteMethod(): UdpSocketReceive(): %s\n",
                          PackosErrorToString(*error));
                  return -1;
                }
            }

          udpHeader=UdpPacketSeekHeader(packet,error);
          if (!udpHeader)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,error,"FileSystemDumbFS::WriteMethod(): UdpPacketSeekHeader(): %s (can't happen)\n",
                      PackosErrorToString(*error));
              PackosPacketFree(packet,&tmp);
              return -1;
            }

          {
            DumbFSReply* reply
              =(DumbFSReply*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));
            if (!((reply->cmd==dumbFSRequestCmdWrite)
                  && (reply->requestId==requestId)
                  )
                )
              {
                PackosPacketFree(packet,error);
                continue;
              }

            if (reply->errorAsInt)
              {
                *error=(PackosError)(reply->errorAsInt);
                UtilPrintfStream(errStream,error,"FileSystemDumbFS::WriteMethod(): received %s\n",
                        PackosErrorToString(*error));
                return -1;
              }

            file->pos+=reply->args.write.actual;
            return reply->args.write.actual;
          }      
        }
    }

  return 0;
}

static int DeleteMethod(FileSystem fs,
                        const char* path,
                        PackosError* error)
{
  PackosPacket* packet;
  IpHeaderUDP* udpHeader;
  DumbFSContext* context;
  DumbFSRequest* request;
  uint16_t requestId;

  if (!error) return -2;
  if (!(fs && path && (UtilStrlen(path)<=DUMBFS_MAX_PATHLEN)))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  context=(DumbFSContext*)(fs->context);

  while (true)
    {
      int numTimeouts=0;
      packet=UdpPacketNew(context->requestSocket,
                          sizeof(DumbFSRequest),
                          &udpHeader,
                          error);
      if (!packet)
        {
          UtilPrintfStream(errStream,error,"FileSystemDumbFS::DeleteMethod(): UdpPacketNew(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }

      udpHeader->destPort=context->port;
      packet->ipv6.src=PackosMyAddress(error);
      packet->ipv6.dest=context->addr;
      packet->packos.dest=routerAddr;

      request=(DumbFSRequest*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));
      request->cmd=dumbFSRequestCmdDelete;
      request->requestId=requestId=context->nextRequestId++;
      UtilStrcpy(request->args.delete.path,path);

      if (UdpSocketSend(context->requestSocket,packet,error)<0)
        {
          PackosError tmp;
          PackosPacketFree(packet,&tmp);
          UtilPrintfStream(errStream,error,"FileSystemDumbFS::DeleteMethod(): UdpSocketSend(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }

      while (numTimeouts<2)
        {
          packet=UdpSocketReceive(context->requestSocket,0,true,error);
          if (!packet)
            {
              if (((*error)==packosErrorNone)
                  || ((*error)==packosErrorStoppedForOtherSocket)
                  || ((*error)==packosErrorPacketFilteredOut)
                  )
                {
                  if (UdpSocketReceivePending(context->timerSocket,error))
                    {
                      UdpSocketReceive(context->timerSocket,0,false,error);
                      numTimeouts++;
                    }
                  continue;
                }
               else
                {
                  UtilPrintfStream(errStream,error,"FileSystemDumbFS::DeleteMethod(): UdpSocketReceive(): %s\n",
                          PackosErrorToString(*error));
                  return -1;
                }
            }

          udpHeader=UdpPacketSeekHeader(packet,error);
          if (!udpHeader)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,error,"FileSystemDumbFS::DeleteMethod(): UdpPacketSeekHeader(): %s (can't happen)\n",
                      PackosErrorToString(*error));
              PackosPacketFree(packet,&tmp);
              return -1;
            }

          {
            DumbFSReply* reply
              =(DumbFSReply*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));

            uint32_t cmd=reply->cmd, requestId2=reply->requestId;
            PackosError replyError=(PackosError)(reply->errorAsInt);

            PackosPacketFree(packet,error);

            if (!((cmd==dumbFSRequestCmdDelete)
                  && (requestId2==requestId)
                  )
                )
              continue;

            if (replyError!=packosErrorNone)
              {
                *error=replyError;
                UtilPrintfStream(errStream,error,"FileSystemDumbFS::DeleteMethod(): received %s\n",
                        PackosErrorToString(*error));
                return -1;
              }

            return 0;
          }      
        }
    }
}

static int RenameMethod(FileSystem fs,
                        const char* pathFrom,
                        const char* pathTo,
                        PackosError* error)
{
  PackosPacket* packet;
  IpHeaderUDP* udpHeader;
  DumbFSContext* context;
  DumbFSRequest* request;
  uint16_t requestId;

  if (!error) return -2;
  if (!(fs
        && pathFrom && (UtilStrlen(pathFrom)<=DUMBFS_MAX_PATHLEN)
        && pathTo && (UtilStrlen(pathTo)<=DUMBFS_MAX_PATHLEN)
        )
      )
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  context=(DumbFSContext*)(fs->context);

  while (true)
    {
      int numTimeouts=0;
      packet=UdpPacketNew(context->requestSocket,
                          sizeof(DumbFSRequest),
                          &udpHeader,
                          error);
      if (!packet)
        {
          UtilPrintfStream(errStream,error,"FileSystemDumbFS::RenameMethod(): UdpPacketNew(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }

      udpHeader->destPort=context->port;
      packet->ipv6.src=PackosMyAddress(error);
      packet->ipv6.dest=context->addr;
      packet->packos.dest=routerAddr;

      request=(DumbFSRequest*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));
      request->cmd=dumbFSRequestCmdRename;
      request->requestId=requestId=context->nextRequestId++;
      UtilStrcpy(request->args.rename.pathFrom,pathFrom);
      UtilStrcpy(request->args.rename.pathTo,pathTo);

      if (UdpSocketSend(context->requestSocket,packet,error)<0)
        {
          PackosError tmp;
          PackosPacketFree(packet,&tmp);
          UtilPrintfStream(errStream,error,"FileSystemDumbFS::RenameMethod(): UdpSocketSend(): %s\n",
                  PackosErrorToString(*error));
          return -1;
        }

      while (numTimeouts<2)
        {
          packet=UdpSocketReceive(context->requestSocket,0,true,error);
          if (!packet)
            {
              if (((*error)==packosErrorNone)
                  || ((*error)==packosErrorStoppedForOtherSocket)
                  || ((*error)==packosErrorPacketFilteredOut)
                  )
                {
                  if (UdpSocketReceivePending(context->timerSocket,error))
                    {
                      UdpSocketReceive(context->timerSocket,0,false,error);
                      numTimeouts++;
                    }
                  continue;
                }
               else
                {
                  UtilPrintfStream(errStream,error,"FileSystemDumbFS::RenameMethod(): UdpSocketReceive(): %s\n",
                          PackosErrorToString(*error));
                  return -1;
                }
            }

          udpHeader=UdpPacketSeekHeader(packet,error);
          if (!udpHeader)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,error,"FileSystemDumbFS::RenameMethod(): UdpPacketSeekHeader(): %s (can't happen)\n",
                      PackosErrorToString(*error));
              PackosPacketFree(packet,&tmp);
              return -1;
            }

          {
            DumbFSReply* reply
              =(DumbFSReply*)(((byte*)udpHeader)+sizeof(IpHeaderUDP));

            uint32_t cmd=reply->cmd, requestId2=reply->requestId;
            PackosError replyError=(PackosError)(reply->errorAsInt);

            PackosPacketFree(packet,error);

            if (!((cmd==dumbFSRequestCmdRename)
                  && (requestId2==requestId)
                  )
                )
              continue;

            if (replyError!=packosErrorNone)
              {
                *error=replyError;
                UtilPrintfStream(errStream,error,"FileSystemDumbFS::RenameMethod(): received %s\n",
                        PackosErrorToString(*error));
                return -1;
              }

            return 0;
          }      
        }
    }
}

static int Destructor(FileSystem fs,
                      PackosError* error)
{
  DumbFSContext* context;

  if (!error) return -2;
  if (!(fs && fs->context))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  context=(DumbFSContext*)(fs->context);

  if (UdpSocketClose(context->requestSocket,error)<0)
    {
      UtilPrintfStream(errStream,error,"FileSystemDumbFS::Destructor(): UdpSocketClose(): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  free(fs->context);
  fs->context=0;
  return 0;
}
