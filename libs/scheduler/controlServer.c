#include <schedulers/control-protocol.h>
#include <schedulers/basic.h>
#include <tcp.h>
#include <contextQueue.h>
#include <util/alloc.h>
#include <util/printf.h>
#include <packos/arch.h>

#include "controlServer.h"

struct SchedulerControlClient {
  SchedulerControlClient next;
  SchedulerControlClient prev;

  PackosContext context;
  SchedulerBasicContextMetadata* metadata;

  TcpSocket socket;
  struct {
    uint32_t offset;
    ControlRequest request;
  } incoming;
};

struct SchedulerControlServer {
  PackosContextQueue contexts;
  TcpSocket socket;
  SchedulerControlClient first;
  SchedulerControlClient last;
};

static int closeClient(SchedulerControlServer server,
                       SchedulerControlClient client,
                       PackosError* error)
{
  if (!error) return -2;
  if (!(server && client))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (TcpSocketClose(client->socket,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "closeClient(): TcpSocketClose(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  if (client->next)
    client->next->prev=client->prev;
  else
    server->last=client->prev;

  if (client->prev)
    client->prev->next=client->next;
  else
    server->first=client->next;

  free(client);
  return 0;
}

SchedulerControlServer SchedulerControlServerInit(PackosContextQueue contexts,
                                                  PackosError* error)
{
  SchedulerControlServer res;

  if (!error) return 0;
  if (!contexts)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  res=(SchedulerControlServer)(malloc(sizeof(struct SchedulerControlServer)));
  if (!res)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  res->socket=TcpSocketNew(error);
  if (!(res->socket))
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "SchedulerControlServerInit(): TcpSocketNew(): %s\n",
                       PackosErrorToString(*error));
      free(res);
      return 0;
    }

  if (TcpSocketBind(res->socket,PackosMyAddress(error),
                    SCHEDULER_CONTROL_FIXED_TCP_PORT,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "SchedulerControlServerInit(): TcpSocketBind(): %s\n",
                       PackosErrorToString(*error));
      TcpSocketClose(res->socket,&tmp);
      free(res);
      return 0;
    }

  if (TcpSocketListen(res->socket,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "SchedulerControlServerInit(): TcpSocketListen(): %s\n",
                       PackosErrorToString(*error));
      TcpSocketClose(res->socket,&tmp);
      free(res);
      return 0;
    }

  res->first=res->last=0;
  res->contexts=contexts;
  return res;
}

int SchedulerControlServerClose(SchedulerControlServer server,
                                PackosError* error)
{
  if (!error) return -2;
  if (!server)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  while (server->first)
    {
      if (closeClient(server,server->first,error)<0)
        {
          PackosError tmp;
          UtilPrintfStream(errStream,&tmp,
                           "SchedulerControlServerClose(): closeClient(): %s\n",
                           PackosErrorToString(*error));
          return -1;
        }
    }

  if (TcpSocketClose(server->socket,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "SchedulerControlServerClose(): TcpSocketClose(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  free(server);
  return 0;
}

static int activateDependents(PackosContext context,
                              void* arg,
                              PackosError* error)
{
  PackosContext dependency=(PackosContext)arg;
  SchedulerBasicContextMetadata* metadata
    =(SchedulerBasicContextMetadata*)(PackosContextGetMetadata(context,error));
  if (metadata->dependsOn==dependency)
    metadata->isRunning=true;
  return 0;
}

static int handleRequest(SchedulerControlServer server,
                         SchedulerControlClient client,
                         ControlRequest* request,
                         PackosError* error)
{
  if (!error) return -2;
  if (!(server && client && request))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  switch (ntohs(request->cmd))
    {
    case controlRequestCmdReportInited:
      client->metadata->isInited=true;
      {
        ControlReply reply;
        reply.version=request->version;
        reply.cmd=htons(controlRequestCmdReportInited);
        reply.requestId=request->requestId;
        reply.errorAsInt=htonl(packosErrorNone);
        reply.reserved=0;
        if (TcpSocketSend(client->socket,&reply,sizeof(reply),error)<0)
          {
            PackosError tmp;
            UtilPrintfStream(errStream,&tmp,
                             "controlServer:handleRequest(): TcpSocketSend(): %s\n",
                             PackosErrorToString(*error)
                             );
            return -1;
          }
      }
      UtilPrintfStream(errStream,error,
                       "controlServer:handleRequest(): %s activated\n",
                       PackosContextGetName(client->context,error)
                       );
      PackosContextQueueForEach(server->contexts,
                                activateDependents,
                                client->context,
                                error);
      break;

    case controlRequestCmdInvalid:
    default:
      UtilPrintfStream(errStream,error,
                       "controlServer:handleRequest(): invalid cmd %x\n",
                       (int)(ntohs(request->cmd))
                       );
      *error=packosErrorBadProtocolCmd;
      return -1;
    }

  return 0;
}


static bool matchContextAddr(PackosContext context,
                             void* arg,
                             PackosError* error)
{
  PackosAddress* addr=(PackosAddress*)arg;
  return PackosAddrEq(PackosContextGetAddr(context,error),*addr);
}

int SchedulerControlServerPoll(SchedulerControlServer server,
                               PackosError* error)
{
  SchedulerControlClient cur;
  SchedulerControlClient next;

  if (TcpSocketAcceptPending(server->socket,error))
    {
      TcpSocket socket=TcpSocketAccept(server->socket,error);
      if (!socket)
        {
          PackosError tmp;
          UtilPrintfStream(errStream,&tmp,
                           "SchedulerControlServerPoll(): TcpSocketAccept(): %s\n",
                           PackosErrorToString(*error));
          return -1;
        }

      {
        SchedulerControlClient newClient
          =(SchedulerControlClient)(malloc(sizeof(struct SchedulerControlClient)));
        if (!newClient)
          {
            TcpSocketClose(socket,error);
            *error=packosErrorOutOfMemory;
            return -1;
          }

        newClient->prev=server->last;
        if (newClient->prev)
          newClient->prev->next=newClient;
        else
          server->first=newClient;

        newClient->next=0;
        server->last=newClient;

        {
          PackosAddress addr=TcpSocketGetPeerAddress(socket,error);
          if ((*error)!=packosErrorNone)
            {
              newClient->context=0;
              *error=packosErrorDoesNotExist;
            }
          else
            newClient->context
              =PackosContextQueueFind(server->contexts,
                                      matchContextAddr,
                                      &addr,
                                      error);

          if (!(newClient->context))
            {
              PackosError tmp;
              UtilPrintfStream(errStream,&tmp,
                               "SchedulerControlServerPoll(): PackosContextQueueFind(): %s\n",
                               PackosErrorToString(*error));
              return -1;
            }

          newClient->metadata
            =(SchedulerBasicContextMetadata*)
            (PackosContextGetMetadata(newClient->context,error));

          newClient->incoming.offset=0;
          newClient->socket=socket;
        }
      }
    }
  else
    {
      if ((*error)!=packosErrorNone)
        {
          PackosError tmp;
          UtilPrintfStream(errStream,&tmp,
                           "SchedulerControlServerPoll(): TcpSocketAcceptPending(): %s\n",
                           PackosErrorToString(*error));
          return -1;
        }
    }

  int i=0;
  for (cur=server->first; cur; cur=next)
    {
      i++;
      next=cur->next;
#if 0
      UtilPrintfStream(errStream,error,
                       "SchedulerControlServerPoll(): %d %p {%p,%p,%p} {%p,%p}\n",
                       i,cur,cur->next,cur->prev,cur->socket,
                       server->first,server->last);
#endif
        
      if (!TcpSocketReceivePending(cur->socket,error))
        {
          PackosError tmp;
          if ((*error)==packosErrorNone) continue;
          UtilPrintfStream(errStream,&tmp,
                           "SchedulerControlServerPoll(): TcpSocketReceivePending(): %s\n",
                           PackosErrorToString(*error));
          if (closeClient(server,cur,error)<0)
            {
              UtilPrintfStream(errStream,&tmp,
                               "SchedulerControlServerPoll(): closeClient(): %s\n",
                               PackosErrorToString(*error));
              return -1;
            }
          continue;
        }

      {
        uint32_t nbytes=sizeof(ControlRequest)-cur->incoming.offset;
        int actual=TcpSocketReceive(cur->socket,
                                    &(cur->incoming.request),
                                    nbytes,
                                    error);
        if (actual<0)
          {
            PackosError tmp;
            UtilPrintfStream(errStream,&tmp,
                             "SchedulerControlServerPoll(): TcpSocketReceive(): %s\n",
                             PackosErrorToString(*error));

            if (closeClient(server,cur,error)<0)
              {
                UtilPrintfStream(errStream,&tmp,
                                 "SchedulerControlServerPoll(): closeClient(): %s\n",
                                 PackosErrorToString(*error));
                return -1;
              }
            continue;
          }

        nbytes-=actual;
        if (nbytes==0)
          {
            if (handleRequest(server,cur,&(cur->incoming.request),error)<0)
              {
                PackosError tmp;
                UtilPrintfStream(errStream,&tmp,
                                 "SchedulerControlServerPoll(): handleRequest(): %s\n",
                                 PackosErrorToString(*error));
                if (closeClient(server,cur,error)<0)
                  {
                    UtilPrintfStream(errStream,&tmp,
                                     "SchedulerControlServerPoll(): closeClient(): %s\n",
                                     PackosErrorToString(*error));
                    return -1;
                  }
              }
          }
      }
    }

  return 0;
}
