#include <stdio.h>
#include <util/alloc.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <packos/types.h>
#include <packos/errors.h>
#include <file.h>
#include <file-system-dumbfs-protocol.h>

typedef struct OpenFile OpenFile;

struct OpenFile {
  OpenFile* next;
  OpenFile* prev;

  struct sockaddr_in6 remote;
  uint16_t requestId;
  int fd;
  bool reading,writing;
  uint32_t fileId;
};

static uint32_t nextFileId=1;

static OpenFile* openFiles=0;

static OpenFile* seekOpenFile(const struct sockaddr_in6* remote,
                              uint16_t requestId,
                              bool reading,
                              bool writing)
{
  OpenFile* cur;
  for (cur=openFiles; cur; cur=cur->next)
    {
      if (requestId!=cur->requestId)
        continue;
      if (reading!=cur->reading)
        continue;
      if (writing!=cur->writing)
        continue;
      if (memcmp(remote,&(cur->remote),sizeof(struct sockaddr_in6)))
        continue;

      return cur;
    }

  return 0;
}

static OpenFile* seekOpenFileById(const struct sockaddr_in6* remote,
                                  uint32_t fileId)
{
  OpenFile* cur;
  for (cur=openFiles; cur; cur=cur->next)
    {
      if (fileId!=cur->fileId)
        continue;
      if (memcmp(remote,&(cur->remote),sizeof(struct sockaddr_in6)))
        continue;

      return cur;
    }

  return 0;
}

static OpenFile* getOpenFile(const struct sockaddr_in6* remote,
                             uint16_t requestId,
                             const char* path,
                             uint32_t flags,
                             PackosError* error)
{
  bool reading=((flags & fileOpenFlagRead)!=0);
  bool writing=((flags & fileOpenFlagWrite)!=0);
  OpenFile* res;

  fprintf(stderr,"getOpenFile(%s)\n",path);

  res=seekOpenFile(remote,requestId,reading,writing);
  if (res) return res;

  res=(OpenFile*)(malloc(sizeof(OpenFile)));
  if (!res)
    {
      perror("malloc");
      *error=packosErrorOutOfMemory;
      return 0;
    }

  res->fd=open(path,
               (reading
                ? (writing ? O_RDWR : O_RDONLY)
                : (writing ? O_WRONLY : 0)
                ) | (writing ? O_CREAT : 0),
               0600
               );
  if (res->fd<0)
    {
      int tmp=errno;
      perror("open");
      switch (tmp)
        {
        case EROFS: *error=packosErrorReadOnlyFilesystem; break;

        case ETXTBSY:
        case EACCES:
          *error=packosErrorAccessDenied;
          break;

        case EMFILE:
        case ENFILE:
          *error=packosErrorOutOfOther;
          break;

        case ENOENT: *error=packosErrorDoesNotExist; break;
        case EISDIR: *error=packosErrorIsDirectory; break;
        case ENOTDIR: *error=packosErrorIsNotDirectory; break;
        case ENAMETOOLONG: *error=packosErrorNameTooLong; break;
        case ENOSPC: *error=packosErrorOutOfDisk; break;
        case ENOMEM: *error=packosErrorOutOfMemory; break;
        default: *error=packosErrorUnknownError; break;
        }
      free(res);
      return 0;
    }

  res->requestId=requestId;
  res->reading=reading;
  res->writing=writing;
  res->fileId=nextFileId++;

  res->prev=0;
  res->next=openFiles;
  if (res->next)
    res->next->prev=res;
  openFiles=res;

  memcpy(&(res->remote),remote,sizeof(res->remote));

  return res;
}

static void closeOpenFile(OpenFile* f)
{
  if (!f) return;

  fprintf(stderr,"closeOpenFile()\n");

  if (f->next)
    f->next->prev=f->prev;
  if (f->prev)
    f->prev->next=f->next;
  else
    openFiles=f->next;

  close(f->fd);

  free(f);
}

int main(int argc, const char* argv[])
{
  int sock=socket(PF_INET6,SOCK_DGRAM,0);
  if (sock<0)
    {
      perror("socket");
      return 1;
    }

  {
    struct sockaddr_in6 addr;
    addr.sin6_family=AF_INET6;
    addr.sin6_port=htons(DUMBFS_DEFAULT_PORT);
    addr.sin6_flowinfo=0;
    addr.sin6_scope_id=0;
    memset(&addr.sin6_addr,0,sizeof(addr.sin6_addr));
    if (bind(sock,(struct sockaddr*)(&addr),sizeof(addr))<0)
      {
        perror("bind");
        return 2;
      }
  }

  while (true)
    {
      struct sockaddr_in6 from;
      socklen_t fromlen=sizeof(from);
      char buff[1501];
      int actual=recvfrom(sock,buff,sizeof(buff)-1,0,
                          (struct sockaddr*)&from,&fromlen
                          );
      if (actual<0)
        {
          perror("recvfrom");
          continue;
        }

      fprintf(stderr,"%d-byte request\n",actual);

      {
        DumbFSReply reply;
        DumbFSRequest* request=(DumbFSRequest*)buff;
        reply.cmd=request->cmd;
        reply.requestId=request->requestId;
        reply.reserved=0;

        switch ((DumbFSRequestCmd)(request->cmd))
          {
          case dumbFSRequestCmdInvalid: continue;

          case dumbFSRequestCmdOpen:
            {
              PackosError error;
              OpenFile* f=getOpenFile(&from,request->requestId,
                                      request->args.open.path,
                                      request->args.open.flags,
                                      &error);
              if (!f)
                {
                  reply.errorAsInt=(uint16_t)error;
                  reply.args.open.fileId=0;
                }
              else
                {
                  reply.errorAsInt=0;
                  reply.args.open.fileId=f->fileId;
                }
            }
            break;

          case dumbFSRequestCmdClose:
            {
              OpenFile* f=seekOpenFileById(&from,request->args.close.fileId);
              if (f)
                closeOpenFile(f);
              else
                fprintf(stderr,"dumbFSRequestCmdClose: can't find OpenFile\n");
                                           
              reply.errorAsInt=0;
            }
            break;

          case dumbFSRequestCmdRead:
            {
              OpenFile* f=seekOpenFileById(&from,request->args.read.fileId);
              if (f)
                {
                  if (f->reading)
                    {
                      if (lseek(f->fd,request->args.read.pos,SEEK_SET)<0)
                        {
                          int tmp=errno;
                          perror("lseek");
                          switch (tmp)
                            {
                            case EBADF:
                              reply.errorAsInt=packosErrorResourceNotInUse;
                              break;

                            default:
                              reply.errorAsInt=packosErrorUnknownError;
                              break;
                            }
                        }
                      else
                        {
                          int actual;
                          bool readIt=false;
                          while (!readIt)
                            {
                              actual=read(f->fd,
                                          reply.args.read.buff,
                                          request->args.read.count);
                              readIt=true;
                              if (actual==0)
                                reply.errorAsInt=packosErrorEndOfFile;
                              else
                                {
                                  if (actual<0)
                                    {
                                      int tmp=errno;
                                      perror("read");
                                      switch (tmp)
                                        {
                                        case EINTR:
                                          readIt=false;
                                          break;

                                        case EBADF:
                                          reply.errorAsInt
                                            =packosErrorResourceNotInUse;
                                          break;

                                        default:
                                          reply.errorAsInt
                                            =packosErrorUnknownError;
                                          break;
                                        }
                                    }
                                }
                            }

                          if (actual>=0)
                            reply.args.read.actual=actual;
                        }
                    }
                  else
                    reply.errorAsInt=packosErrorAccessDenied;
                }
              else
                reply.errorAsInt=packosErrorResourceNotInUse;
            }
            break;

          case dumbFSRequestCmdWrite:
            {
              OpenFile* f=seekOpenFileById(&from,request->args.write.fileId);
              if (f)
                {
                  if (f->writing)
                    {
                      if (lseek(f->fd,request->args.write.pos,SEEK_SET)<0)
                        {
                          int tmp=errno;
                          perror("lseek");
                          switch (tmp)
                            {
                            case EBADF:
                              reply.errorAsInt=packosErrorResourceNotInUse;
                              break;

                            default:
                              reply.errorAsInt=packosErrorUnknownError;
                              break;
                            }
                        }
                      else
                        {
                          int actual;
                          bool writeIt=false;
                          while (!writeIt)
                            {
                              actual=write(f->fd,
                                          request->args.write.buff,
                                          request->args.write.count);
                              writeIt=true;
                              if (actual<0)
                                {
                                  int tmp=errno;
                                  perror("write");
                                  switch (tmp)
                                    {
                                    case EINTR:
                                      writeIt=false;
                                      break;

                                    case EFBIG:
                                      reply.errorAsInt
                                        =packosErrorAccessDenied;
                                      break;

                                    case ENOSPC:
                                      reply.errorAsInt
                                        =packosErrorOutOfDisk;
                                      break;

                                    case EBADF:
                                      reply.errorAsInt
                                        =packosErrorResourceNotInUse;
                                      break;

                                    default:
                                      reply.errorAsInt
                                        =packosErrorUnknownError;
                                      break;
                                    }
                                }
                            }

                          if (actual>=0)
                            reply.args.write.actual=actual;
                        }
                    }
                  else
                    reply.errorAsInt=packosErrorAccessDenied;
                }
              else
                reply.errorAsInt=packosErrorResourceNotInUse;
            }
            break;

          case dumbFSRequestCmdDelete:
            {
              PackosError error;
              if (unlink(request->args.delete.path)<0)
                {
                  int tmp=errno;
                  perror("unlink");
                  switch (tmp)
                    {
                    case EROFS: error=packosErrorReadOnlyFilesystem; break;

                    case EACCES:
                    case EPERM:
                      error=packosErrorAccessDenied;
                      break;

                    case EISDIR: error=packosErrorIsDirectory; break;
                    case EBUSY: error=packosErrorResourceInUse; break;
                    case ENAMETOOLONG: error=packosErrorNameTooLong; break;
                    case ENOENT: error=packosErrorDoesNotExist; break;
                    case ENOTDIR: error=packosErrorIsNotDirectory; break;
                    case ENOMEM: error=packosErrorOutOfMemory; break;
                    case EIO: error=packosErrorUnknownIOError; break;
                    case ELOOP: error=packosErrorTooManyRedirects; break;

                    default: error=packosErrorUnknownError; break;
                    }
                  return 0;
                }
              else
                error=packosErrorNone;

              reply.errorAsInt=(uint16_t)error;
            }
            break;

          case dumbFSRequestCmdRename:
            {
              PackosError error;
              if (rename(request->args.rename.pathFrom,
                         request->args.rename.pathTo)<0)
                {
                  int tmp=errno;
                  perror("rename");
                  switch (tmp)
                    {
                    case EISDIR: error=packosErrorIsDirectory; break;
                    case EXDEV: error=packosErrorCrossFilesystemMove; break;

                    case ENOTEMPTY:
                    case EEXIST:
                      error=packosErrorDirectoryNotEmpty;
                      break;

                    case EBUSY: error=packosErrorResourceInUse; break;
                    case EINVAL: error=packosErrorInvalidArg; break;
                    case EMLINK: error=packosErrorOutOfOther; break;
                    case ENOTDIR: error=packosErrorIsNotDirectory; break;
                    case EROFS: error=packosErrorReadOnlyFilesystem; break;

                    case EACCES:
                    case EPERM:
                      error=packosErrorAccessDenied;
                      break;

                    case ENAMETOOLONG: error=packosErrorNameTooLong; break;
                    case ENOENT: error=packosErrorDoesNotExist; break;
                    case ENOMEM: error=packosErrorOutOfMemory; break;

                    case EIO: error=packosErrorUnknownIOError; break;
                    case ELOOP: error=packosErrorTooManyRedirects; break;
                    case ENOSPC: error=packosErrorOutOfDisk; break;

                    default: error=packosErrorUnknownError; break;
                    }
                  return 0;
                }
              else
                error=packosErrorNone;

              reply.errorAsInt=(uint16_t)error;
            }
            break;
          }

        if (sendto(sock,&reply,sizeof(reply),0,
                   (struct sockaddr*)&from,fromlen
                   )<0)
          perror("sendto");
      }
    }

  return 0;
}
