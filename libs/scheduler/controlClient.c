#include <schedulers/control-protocol.h>
#include <schedulers/control.h>
#include <tcp.h>
#include <util/alloc.h>
#include <util/printf.h>
#include <packos/arch.h>
#include <packos/context.h>

struct SchedulerControl {
  TcpSocket socket;
  uint32_t nextId;
};

SchedulerControl SchedulerControlConnect(PackosError* error)
{
  SchedulerControl res;
  PackosAddress schedulerAddr,myAddr;

  if (!error) return 0;

  res=(SchedulerControl)(malloc(sizeof(struct SchedulerControl)));
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
                       "SchedulerControlConnect(): TcpSocketNew(): %s\n",
                       PackosErrorToString(*error));
      free(res);
      return 0;
    }

  myAddr=PackosMyAddress(error);
  if ((*error)!=packosErrorNone)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "SchedulerControlConnect(): PackosMyAddress(): %s\n",
                       PackosErrorToString(*error));
      TcpSocketClose(res->socket,&tmp);
      free(res);
      return 0;
    }

  if (TcpSocketBind(res->socket,
                    myAddr,0,
                    error)
      <0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "SchedulerControlConnect(): TcpSocketBind(): %s\n",
                       PackosErrorToString(*error));
      TcpSocketClose(res->socket,&tmp);
      free(res);
      return 0;
    }

  schedulerAddr=PackosSchedulerAddress(error);
  if ((*error)!=packosErrorNone)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "SchedulerControlConnect(): PackosSchedulerAddress(): %s\n",
                       PackosErrorToString(*error));
      TcpSocketClose(res->socket,&tmp);
      free(res);
      return 0;
    }

  if (TcpSocketConnect(res->socket,
                       schedulerAddr,
                       SCHEDULER_CONTROL_FIXED_TCP_PORT,
                       error
                       )<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "SchedulerControlConnect(): TcpSocketConnect(): %s\n",
                       PackosErrorToString(*error));
      TcpSocketClose(res->socket,&tmp);
      free(res);
      return 0;
    }

  res->nextId=1;
  return res;
}

int SchedulerControlClose(SchedulerControl control,
                          PackosError* error)
{
  if (!error) return -2;
  if (!control)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (TcpSocketClose(control->socket,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "SchedulerControlConnect(): TcpSocketClose(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  free(control);

  return 0;
}

static int requestReply(SchedulerControl control,
                        ControlRequest* request,
                        ControlReply* reply,
                        PackosError* error)
{
  const char* myName;

  if (!error) return -2;
  if (!control)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  myName=PackosContextGetOwnName(error);
  if (!myName) return -1; /* can't happen? */

  request->version=htons(SCHEDULER_CONTROL_PROTOCOL_VERSION);
  request->cmd=htons(request->cmd);
  request->requestId=htons(control->nextId++);
  request->reserved=0;

  if (TcpSocketSend(control->socket,request,sizeof(ControlRequest),error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "controlClient:requestReply(): TcpSocketSend(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  if (TcpSocketReceive(control->socket,reply,sizeof(ControlReply),error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "controlClient:requestReply(): TcpSocketReceive(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  if ((reply->requestId)!=(request->requestId))
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "controlClient:requestReply(): request id mismatch\n"
                       );
      *error=packosErrorProtocolError;
      return -1;
    }

  if ((reply->cmd)!=(request->cmd))
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "controlClient:requestReply(): cmd mismatch\n"
                       );
      *error=packosErrorProtocolError;
      return -1;
    }

  if ((reply->version)!=(request->version))
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "controlClient:requestReply(): version mismatch\n"
                       );
      *error=packosErrorProtocolError;
      return -1;
    }

  reply->cmd=ntohs(reply->cmd);
  reply->requestId=ntohs(reply->requestId);
  reply->version=ntohs(reply->version);
  reply->errorAsInt=ntohl(reply->errorAsInt);

  return 0;
}

/* Lets the scheduler know it can start giving time slots to
 *  contexts that depend on this one.
 */
int SchedulerControlReportInited(SchedulerControl control,
                                 PackosError* error)
{
  ControlRequest request;
  ControlReply reply;

  request.cmd=controlRequestCmdReportInited;

  if (requestReply(control,&request,&reply,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "controlClient:requestReply(): TcpSocketReceive(): %s\n",
                       PackosErrorToString(*error));
      SchedulerControlClose(control,&tmp);
      return -1;
    }

  *error=(PackosError)(reply.errorAsInt);
  if ((*error)!=packosErrorNone)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "controlClient:requestReply(): error from server(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  return 0;
}
