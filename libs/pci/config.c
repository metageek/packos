#include <util/alloc.h>
#include <util/printf.h>
#include <util/string.h>
#include <pci/config-protocol.h>
#include <tcp.h>

struct PCIConnection {
  TcpSocket socket;
  uint32_t nextRequestId,outstandingRequestId;
  int numDevices;
};

PCIConnection PCIConfigNew(PackosAddress addr, PackosError* error)
{
  PCIConnection res;
  if (!error) return 0;

  res=(PCIConnection)(malloc(sizeof(struct PCIConnection)));
  if (!res)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  res->socket=TcpSocketNew(error);
  if (!(res->socket))
    {
      free(res);
      return 0;
    }

  PackosAddress myAddr=PackosMyAddress(error);
  if ((*error)!=packosErrorNone)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "PCIConfigNew(): PackosMyAddress(): %s\n",
                       PackosErrorToString(*error));
      TcpSocketClose(res->socket,&tmp);
      free(res);
      return 0;
    }

  if (TcpSocketBind(res->socket,myAddr,0,error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "PCIConfigNew(): TcpSocketBind(): %s\n",
                       PackosErrorToString(*error));
      TcpSocketClose(res->socket,&tmp);
      free(res);
      return 0;
    }

  if (TcpSocketConnect(res->socket,addr,PCIConfigPort,error)<0)
    {
      PackosError tmp;
      TcpSocketClose(res->socket,&tmp);
      free(res);
      return 0;
    }

  res->nextRequestId=1;
  res->outstandingRequestId=0;
  res->numDevices=-1;
  return res;
}

int PCIConfigClose(PCIConnection pci,
                   PackosError* error)
{
  if (!error) return -2;
  if (!pci)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (pci->socket)
    {
      if (TcpSocketClose(pci->socket,error)<0)
        {
          PackosError tmp;
          UtilPrintfStream(errStream,&tmp,
                           "PCIConfigClose(): TcpSocketClose: %s\n",
                           PackosErrorToString(*error));
          return -1;
        }
      pci->socket=0;
    }

  free(pci);
  return 0;
}

static int send(PCIConnection pci,
                const PCIConfigProtocolRequest* request,
                PackosError* error)
{
  if (!error) return -2;
  if (!(pci && (pci->socket) && request))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (TcpSocketSend(pci->socket,
                    request,sizeof(PCIConfigProtocolRequest),
                    error)
      <0
      )
    return -1;

  return 0;
}

static int receive(PCIConnection pci,
                   PCIConfigProtocolReply* reply,
                   PackosError* error)
{
  if (!error) return -2;
  if (!(pci && (pci->socket) && reply))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  {
    char* buff=(char*)reply;
    uint32_t nbytes=sizeof(PCIConfigProtocolReply);
    while (nbytes>0)
      {
        int actual=TcpSocketReceive(pci->socket,buff,nbytes,error);
        if (actual<0)
          return -1;
        nbytes-=actual;
        buff+=actual;
      }
  }

  return 0;
}

static int query(PCIConnection pci,
                 const PCIConfigProtocolRequest* request,
                 PCIConfigProtocolReply* reply,
                 PackosError* error)
{
  if (send(pci,request,error)<0) return -1;
  if (receive(pci,reply,error)<0) return -1;
  if (((reply->op)!=(request->op))
      || ((reply->requestId)!=(request->requestId))
      )
    {
      *error=packosErrorProtocolError;
      return -1;
    }

  return 0;
}
                 
int PCIConfigSetFilter(PCIConnection pci,
                       int32_t vendorId,
                       int32_t deviceId,
                       int32_t baseClass,
                       int32_t subClass,
                       int32_t programmingInterface,
                       PackosError* error)
{
  PCIConfigProtocolRequest request;
  PCIConfigProtocolReply reply;

  request.op=pciConfigProtocolRequestOpSetFilter;
  request.u.setFilter.vendorId=vendorId;
  request.u.setFilter.deviceId=deviceId;
  request.u.setFilter.baseClass=baseClass;
  request.u.setFilter.subClass=subClass;
  request.u.setFilter.programmingInterface=programmingInterface;

  if (query(pci,&request,&reply,error)<0) return -1;

  *error=reply.error;
  if ((*error)!=packosErrorNone)
    return -1;
  else
    return 0;
}

int PCIConfigGetNumDevices(PCIConnection pci, PackosError* error)
{
  PCIConfigProtocolRequest request;
  PCIConfigProtocolReply reply;

  request.op=pciConfigProtocolRequestOpGetNumDevices;

  if (query(pci,&request,&reply,error)<0) return -1;

  *error=reply.error;
  if ((*error)!=packosErrorNone)
    return -1;
  else
    return pci->numDevices=reply.u.getNumDevices.numDevices;
}

int PCIConfigGetDevice(PCIConnection pci,
                       uint32_t deviceNumber,
                       PCIDevice* device,
                       PackosError* error)
{
  PCIConfigProtocolRequest request;
  PCIConfigProtocolReply reply;

  request.op=pciConfigProtocolRequestOpGetDevice;
  request.u.getDevice.deviceNumber=deviceNumber;

  if (query(pci,&request,&reply,error)<0) return -1;

  *error=reply.error;
  if ((*error)!=packosErrorNone)
    return -1;
  else
    {
      UtilMemcpy(device,
                 &(reply.u.getDevice.device),
                 sizeof(reply.u.getDevice.device)
                 );
      return 0;
    }
}

int PCIConfigReserveDevice(PCIConnection pci,
                           uint32_t deviceNumber,
                           PackosError* error)
{
  PCIConfigProtocolRequest request;
  PCIConfigProtocolReply reply;

  request.op=pciConfigProtocolRequestOpReserveDevice;
  request.u.reserveDevice.deviceNumber=deviceNumber;

  if (query(pci,&request,&reply,error)<0) return -1;

  *error=reply.error;
  if (((*error)!=packosErrorNone) || (!(reply.u.reserveDevice.granted)))
    return -1;
  return 0;
}

int PCIConfigReleaseDevice(PCIConnection pci,
                           uint32_t deviceNumber,
                           PackosError* error)
{
  PCIConfigProtocolRequest request;
  PCIConfigProtocolReply reply;

  request.op=pciConfigProtocolRequestOpReleaseDevice;
  request.u.releaseDevice.deviceNumber=deviceNumber;

  if (query(pci,&request,&reply,error)<0) return -1;

  *error=reply.error;
  if (((*error)!=packosErrorNone) || (!(reply.u.releaseDevice.released)))
    return -1;
  return 0;
}
