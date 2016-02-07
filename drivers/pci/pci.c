/* Includes code taken from Linux 2.6.16.20's arch/i386/pci/irq.c.
 *  Therefore, must be under the GPL.
 */

#include <pci/pci.h>
#include <pci/config-protocol.h>
#include <schedulers/control.h>
#include "io.h"
#include <util/printf.h>
#include <util/alloc.h>
#include <util/string.h>
#include <tcp.h>
#include <iface-native.h>
#include <packos/context.h>

#define MAX_BUSSES 8
static int numBusses=0;

typedef struct {
  struct {
    int device,type;
  } isaBridge;
  int deviceIndex,bus;
} PCIBus;

struct irq_info {
  uint8_t bus, devfn;			/* Bus, device and function */
  struct {
    uint8_t link;		/* IRQ line ID, chipset dependent, 0=not routed */
    uint16_t bitmap;		/* Available IRQs */
  } __attribute__((packed)) irq[4];
  uint8_t slot;			/* Slot number, 0=onboard */
  uint8_t rfu;
} __attribute__((packed));

struct irq_routing_table {
  uint32_t signature;			/* PIRQ_SIGNATURE should be here */
  uint16_t version;			/* PIRQ_VERSION */
  uint16_t size;			/* Table size in bytes */
  uint8_t rtr_bus, rtr_devfn;		/* Where the interrupt router lies */
  uint16_t exclusive_irqs;		/* IRQs devoted exclusively to PCI usage */
  uint16_t rtr_vendor, rtr_device;	/* Vendor and device ID of interrupt router */
  uint32_t miniport_data;		/* Crap */
  uint8_t rfu[11];
  uint8_t checksum;			/* Modulo 256 checksum must give zero */
  struct irq_info slots[0];
} __attribute__((packed));

#define PIRQ_SIGNATURE	(('$' << 0) + ('P' << 8) + ('I' << 16) + ('R' << 24))
#define PIRQ_VERSION 0x0100

static struct irq_routing_table* routingTable=0;
static PCIDevice* routerDevice=0;

static PCIBus busses[MAX_BUSSES];

#define MAX_DEVICES 24
static unsigned int numDevices=0;

static PCIDevice devices[MAX_DEVICES];

static void pciWriteStatus(int bus, int device, int function, uint16_t value)
{
  pciConfWrite16(bus,device,function,PCIConstants.conf.statusReg,value);
  outl(PCIConstants.regSelectorPort,0);
}

static uint16_t pciReadStatus(int bus, int device, int function)
{
  return pciConfRead16(bus,device,function,
                       PCIConstants.conf.secondaryStatusReg);
}

static int probe_bus(int bus, PackosError* error)
{
  uint32_t dev;
  uint16_t vendorId, deviceId, status;
  int devind;
  uint32_t nextIOAddr=0x8000,nextMemAddr=0xf0000000;

  if (numDevices>=MAX_DEVICES)
    {
      *error=packosErrorDeviceTableFull;
      return -1;
    }

  devind=numDevices;

  for (dev=0; dev<32; dev++)
    {
      int function;
      for (function=0; function<8; function++)
        {
          byte headerType,baseClass,subClass,infClass;
          const char* deviceName;
          const char* vendorName;
          const char* className;

          pciWriteStatus(bus,dev,function,
                         (PCIConstants.conf.statusFlags.signaledSystemError
                          |PCIConstants.conf.statusFlags.receivedMasterAbort
                          |PCIConstants.conf.statusFlags.receivedTargetAbort
                          )
                         );
          vendorId=pciConfRead16(bus,dev,function,
                                 PCIConstants.conf.vendorIdReg);
          deviceId=pciConfRead16(bus,dev,function,
                                 PCIConstants.conf.deviceIdReg);
          headerType=pciConfRead8(bus,dev,function,
                                  PCIConstants.conf.headerTypeReg);
          status=pciReadStatus(bus,dev,function);

          if (vendorId==0xffff)
            break;	/* Nothing here */

          if (status &
              (PCIConstants.conf.statusFlags.signaledSystemError
               |PCIConstants.conf.statusFlags.receivedMasterAbort
               |PCIConstants.conf.statusFlags.receivedTargetAbort
               )
              )
            break;

          deviceName=pciGetDeviceName(vendorId,deviceId,error);
          vendorName=pciGetVendorName(vendorId,error);

          baseClass=pciConfRead8(bus,dev,function,
                                 PCIConstants.conf.baseClassReg);
          subClass=pciConfRead8(bus,dev,function,
                                PCIConstants.conf.subClassReg);
          infClass=pciConfRead8(bus,dev,function,
                                PCIConstants.conf.programmingInterfaceReg);
          className=pciGetClassName(baseClass, subClass, infClass,error);
          if (!className) className="(unknown class)";

          devind=numDevices;
          numDevices++;

          devices[devind].owner=PackosAddrGetZero();
          {
            PCIDevice* device=&(devices[devind]);
            PCIConfigData* config=&(device->config);

            device->addr.bus=bus;
            device->addr.device=dev;
            device->addr.function=function;

            config->vendorId=vendorId;
            config->deviceId=deviceId;
            config->status=status;
            config->programmingInterface=infClass;
            config->subClass=subClass;
            config->baseClass=baseClass;
            config->headerType=headerType;

            config->revisionId
              =pciConfRead8(bus,dev,function,
                            PCIConstants.conf.revisionIdReg);
            config->cacheLineSize
              =pciConfRead8(bus,dev,function,
                            PCIConstants.conf.cacheLineSizeReg);
            config->latencyTime
              =pciConfRead8(bus,dev,function,PCIConstants.conf.latencyTimeReg);
            config->builtInSelfTest
              =pciConfRead8(bus,dev,function,
                            PCIConstants.conf.builtInSelfTestReg);
            config->subsystem.vendorId
              =pciConfRead8(bus,dev,function,
                            PCIConstants.conf.subsystem.vendorIdReg);
            config->subsystem.deviceId
              =pciConfRead8(bus,dev,function,
                            PCIConstants.conf.subsystem.deviceIdReg);
            {
              uint32_t bootRomData
                =pciConfRead32(bus,dev,function,
                               PCIConstants.conf.bootRomReg);
              config->bootRom.enabled=(bootRomData&1);
              config->bootRom.size=(bootRomData>>11)&8;
              config->bootRom.physAddr=(void*)((bootRomData>>18)<<18);
            }
            config->interruptLine
              =pciConfRead8(bus,dev,function,
                            PCIConstants.conf.interruptLineReg);
            config->irq=0;
            config->minimumGrantTimer
              =pciConfRead8(bus,dev,function,
                            PCIConstants.conf.minimumGrantTimerReg);
            config->maximumLatencyTimer
              =pciConfRead8(bus,dev,function,
                            PCIConstants.conf.maximumLatencyTimerReg);

            if ((headerType==0) && (baseClass!=6))
              {
                uint16_t oldCmd=pciConfRead16(bus,dev,function,
                                              PCIConstants.conf.commandReg);
                bool already=(oldCmd & (PCIConstants.conf.commandFlags.io
                                        | PCIConstants.conf.commandFlags.memory
                                        )
                              );
                if (already)
                  UtilPrintfStream(errStream,error,
                                   "\t\talready configured\n");

                {
                  uint16_t cmd=oldCmd;
                  int i;
                  for (i=0; i<4; i++)
                    {
                      uint32_t baseAddrSpec;
                      if (!already)
                        pciConfWrite32(bus,dev,function,
                                       PCIConstants.conf.baseAddrReg[i],
                                       0xffffffff);
                      baseAddrSpec
                        =pciConfRead32(bus,dev,function,
                                       PCIConstants.conf.baseAddrReg[i]);
                      if ((baseAddrSpec==0xffffffff) || (baseAddrSpec==0))
                        {
                          config->baseAddrs[i].valid=false;
                          continue;
                        }

                      config->baseAddrs[i].valid=true;
                      config->baseAddrs[i].asIO=(baseAddrSpec&1);
                      if (config->baseAddrs[i].asIO)
                        {
                          uint32_t ioAddr,ioSize;
                          if (already)
                            {
                              ioSize=0;
                              ioAddr=baseAddrSpec&(~0xffUL);
                            }
                          else
                            {
                              ioSize=1+(~(baseAddrSpec&(~3)));
                              ioAddr=nextIOAddr;
                              if (ioAddr%ioSize)
                                ioAddr+=ioSize-(ioAddr%ioSize);
                              nextIOAddr=ioAddr+ioSize;

                              pciConfWrite32(bus,dev,function,
                                             PCIConstants.conf.baseAddrReg[i],
                                             ioAddr|1);
                              UtilPrintfStream
                                (errStream,error,
                                 "\t\t\tbaseAddrSpec[%d]: %x => %x+%x\n",
                                 i,baseAddrSpec,ioAddr,ioSize);
                            }

                          config->baseAddrs[i].u.io.ioAddr=ioAddr;
                          cmd|=PCIConstants.conf.commandFlags.io;
                        }
                      else
                        {
                          uint32_t memSize,memAddr;
                          byte memType;
                          if (already)
                            {
                              memSize=0;
                              memAddr=(baseAddrSpec>>8)<<8;
                              memType=0;
                            }
                          else
                            {
                              memSize=1+(baseAddrSpec&(~15));
                              memAddr=nextMemAddr;

                              if (memAddr%memSize)
                                memAddr+=memSize-(memAddr%memSize);

                              /* I don't know what to do with this. */
                              memType=(baseAddrSpec>>1)&3;

                              pciConfWrite32(bus,dev,function,
                                             PCIConstants.conf.baseAddrReg[i],
                                             memAddr|(baseAddrSpec&15));

                              nextMemAddr=memAddr+memSize;

                              UtilPrintfStream
                                (errStream,error,
                                 "\t\t\tbaseAddrSpec[%d]: %x => %x+%x\n",
                                 i,baseAddrSpec,memAddr,memSize);
                            }

                          bool memPrefetchable=(baseAddrSpec>>3)&1;

                          config->baseAddrs[i].u.mem.physAddr=(void*)memAddr;
                          config->baseAddrs[i].u.mem.type=memType;
                          config->baseAddrs[i].u.mem.prefetchable
                            =memPrefetchable;

                          cmd|=PCIConstants.conf.commandFlags.memory;
                        }
                    }

                  if ((!already) && (cmd!=oldCmd))
                    pciConfWrite16(bus,dev,function,
                                   PCIConstants.conf.commandReg,
                                   cmd);
                }

                {
                  int i;
                  bool isFirst=true;
                  for (i=0; i<4; i++)
                    {
                      if (!(config->baseAddrs[i].valid))
                        continue;

                      UtilPrintfStream(errStream,error,
                                       (isFirst ? "\t\t" : " "));
                      isFirst=false;

                      if (config->baseAddrs[i].asIO)
                        UtilPrintfStream(errStream,error,"{io %08x}",
                                         config->baseAddrs[i].u.io.ioAddr
                                         );
                      else
                        UtilPrintfStream(errStream,error,"{mem %08x%s}",
                                         config->baseAddrs[i].u.mem.physAddr,
                                         (
                                          config->baseAddrs[i].u.mem.prefetchable
                                          ? " prefetchable"
                                          : ""
                                          )
                                         );
                    }
                  if (!isFirst)
                    UtilPrintfStream(errStream,error,"\n");

                  if ((vendorId==0x10EC)
                      && (deviceId==0x8139)
                      && (config->baseAddrs[1].valid)
                      )
                    {
                      void* memBase=config->baseAddrs[1].u.mem.physAddr;
                      uint32_t macHigh=*((uint32_t*)(memBase+4));
                      uint32_t macLow=*((uint32_t*)memBase);
                      UtilPrintfStream(errStream,error,
                                       "\t\t%02x:%02x:%02x:%02x:%02x:%02x\n",
                                       (macHigh>>8),(macHigh&0xff),
                                       (macLow>>24),
                                       (macLow>>16)&0xff,
                                       (macLow>>8)&0xff,
                                       macLow&0xff);
                    }
                }
              }
            if ((!routerDevice)
                && routingTable
                && (routingTable->rtr_vendor==config->vendorId)
                && (routingTable->rtr_device==config->deviceId)
                )
              routerDevice=&(devices[devind]);

            UtilPrintfStream(errStream,error,"\t(%02x,%02x,%02x): %s (%x/%x/%x): [%x %x] %s: %s IRQ line %d%s\n",
                             bus,dev,function,
                             className,
                             baseClass,subClass,infClass,
                             vendorId,deviceId,
                             (vendorName ? vendorName : "<unknown>"),
                             (deviceName ? deviceName : "<unknown>"),
                             (int)(config->interruptLine),
                             ((device==routerDevice)
                              ? " router"
                              : ""
                              )
                             );
          }

          if (numDevices >=MAX_DEVICES)
            {
              *error=packosErrorDeviceTableFull;
              return -1;
            }

          if (function==0 && !(headerType
                               & PCIConstants.conf.headerTypeMultiFlag
                               )
              )
            break;
        }
    }

  return 0;
}

static int pciInit(uint32_t bus,
                   PackosError* error)
{
  uint16_t vendorId, deviceId;
  int busIndex;

  vendorId=pciConfRead16(bus, 0, 0, PCIConstants.conf.vendorIdReg);
  deviceId=pciConfRead16(bus, 0, 0, PCIConstants.conf.deviceIdReg);
  outl(PCIConstants.regSelectorPort,0);

  if (vendorId==0xffff && deviceId==0xffff)
    {
      *error=packosErrorNoSuchDevice;
      return -1;	/* Nothing here */
    }

  {
    const char* controllerName=pciGetControllerName(vendorId,deviceId,error);
    UtilPrintfStream(errStream,error,
                     "pciInit(%u): vendor %04x, device %04x: %s\n",
                     (int)bus,vendorId, deviceId,
                     (controllerName
                      ? controllerName
                      : "<none>"
                      )
                     );

    if (!controllerName)
      {
        *error=packosErrorNoSuchDevice;
        return -1;	/* Nothing here */
      }
  }

  if (numBusses>=MAX_BUSSES)
    {
      *error=packosErrorBufferFull;
      return -1;
    }

  busIndex=numBusses++;
  busses[busIndex].isaBridge.device=-1;
  busses[busIndex].isaBridge.type=0;
  busses[busIndex].deviceIndex=-1;
  busses[busIndex].bus=0;

  probe_bus(busIndex,error);
  return 0;
}

typedef struct Client* Client;
struct Client {
  Client next;
  Client prev;

  TcpSocket socket;
  struct {
    PCIConfigProtocolRequest request;
    int offset;
  } incoming;

  struct {
    int32_t vendorId,deviceId,baseClass,subClass,programmingInterface;
  } filter;
};

static void closeClient(Client client,
                        Client* first,
                        Client* last)
{
  if (client->prev)
    client->prev->next=client->next;
  else
    (*first)=client->next;

  if (client->next)
    client->next->prev=client->prev;
  else
    (*last)=client->prev;

  {
    PackosError error;
    TcpSocketClose(client->socket,&error);
  }
  free(client);
}

static int getNumDevices(Client client,
                         PackosError* error)
{
  if ((client->filter.vendorId==-1)
      && (client->filter.deviceId==-1)
      && (client->filter.baseClass==-1)
      && (client->filter.subClass==-1)
      && (client->filter.programmingInterface==-1)
      )
    return numDevices;

  {
    int i,res;
    for (i=res=0; i<numDevices; i++)
      {
        if (((client->filter.vendorId==-1)
             || (client->filter.vendorId==devices[i].config.vendorId)
             )
            &&
            ((client->filter.deviceId==-1)
             || (client->filter.deviceId==devices[i].config.deviceId)
             )
            &&
            ((client->filter.baseClass==-1)
             || (client->filter.baseClass==devices[i].config.baseClass)
             )
            &&
            ((client->filter.subClass==-1)
             || (client->filter.subClass==devices[i].config.subClass)
             )
            &&
            ((client->filter.programmingInterface==-1)
             || (client->filter.programmingInterface
                 ==devices[i].config.programmingInterface
                 )
             )
            )
          res++;
      }

    return res;
  }
}

static PCIDevice* seekDevice(Client client,
                             uint32_t deviceNumber,
                             PackosError* error)
{
  if ((client->filter.vendorId==-1)
      && (client->filter.deviceId==-1)
      && (client->filter.baseClass==-1)
      && (client->filter.subClass==-1)
      && (client->filter.programmingInterface==-1)
      )
    {
      if (deviceNumber>=numDevices)
        {
          *error=packosErrorNoSuchDevice;
          return 0;
        }
    }

  {
    int i;
    for (i=0; i<numDevices; i++)
      {
        if (((client->filter.vendorId==-1)
             || (client->filter.vendorId==devices[i].config.vendorId)
             )
            &&
            ((client->filter.deviceId==-1)
             || (client->filter.deviceId==devices[i].config.deviceId)
             )
            &&
            ((client->filter.baseClass==-1)
             || (client->filter.baseClass==devices[i].config.baseClass)
             )
            &&
            ((client->filter.subClass==-1)
             || (client->filter.subClass==devices[i].config.subClass)
             )
            &&
            ((client->filter.programmingInterface==-1)
             || (client->filter.programmingInterface
                 ==devices[i].config.programmingInterface
                 )
             )
            )
          {
            if (deviceNumber==0)
              return devices+i;
            deviceNumber--;
          }
      }
  }

  *error=packosErrorNoSuchDevice;
  return 0;
}

static int getDevice(Client client,
                     uint32_t deviceNumber,
                     PCIDevice* device,
                     PackosError* error)
{
  const PCIDevice* found=seekDevice(client,deviceNumber,error);
  if (!found) return -1;

  UtilMemcpy(device,found,sizeof(PCIDevice));
  return 0;
}

static bool reserveDevice(Client client,
                          uint32_t deviceNumber,
                          PackosError* error)
{
  PCIDevice* found=seekDevice(client,deviceNumber,error);
  if (!found) return false;


  if (!(PackosAddrIsZero(found->owner)))
    {
      char buff[80];
      const char* addrStr=buff;
      {
        PackosError tmp;
        if (PackosAddrToString(found->owner,
                               buff,sizeof(buff),
                               &tmp)<0)
          addrStr=PackosErrorToString(tmp);
      }
      UtilPrintfStream(errStream,error,
                       "reserveDevice(): already in use by %s\n",
                       addrStr);
      *error=packosErrorResourceInUse;
      return false;
    }

  found->owner=TcpSocketGetPeerAddress(client->socket,error);
  if (PackosAddrIsZero(found->owner))
    return false;

  return true;
}

static bool releaseDevice(Client client,
                          uint32_t deviceNumber,
                          PackosError* error)
{
  PCIDevice* found=seekDevice(client,deviceNumber,error);
  if (!found) return false;

  if (PackosAddrIsZero(found->owner))
    {
      *error=packosErrorResourceNotInUse;
      return false;
    }

  PackosAddress remote=TcpSocketGetPeerAddress(client->socket,error);
  if (PackosAddrIsZero(remote))
    return false;

  if (!(PackosAddrEq(found->owner,remote)))
    {
      *error=packosErrorResourceNotInUse;
      return false;
    }

  found->owner=PackosAddrGetZero();

  return true;
}

static int handleRequest(Client client,
                         PCIConfigProtocolRequest* request,
                         PackosError* error)
{
  PCIConfigProtocolReply reply;
  reply.op=request->op;
  reply.requestId=request->requestId;

  *error=packosErrorNone;
  switch (request->op)
    {
    case pciConfigProtocolRequestOpInvalid:
      *error=packosErrorBadProtocolCmd;
      break;

    case pciConfigProtocolRequestOpSetFilter:
      client->filter.vendorId=request->u.setFilter.vendorId;
      client->filter.deviceId=request->u.setFilter.deviceId;
      client->filter.baseClass=request->u.setFilter.baseClass;
      client->filter.subClass=request->u.setFilter.subClass;
      client->filter.programmingInterface
        =request->u.setFilter.programmingInterface;
      break;

    case pciConfigProtocolRequestOpGetNumDevices:
      {
        int32_t N=getNumDevices(client,error);
        if (N>=0)
          reply.u.getNumDevices.numDevices=(uint32_t)N;
        else
          reply.u.getNumDevices.numDevices=0;
      }
      break;

    case pciConfigProtocolRequestOpGetDevice:
      getDevice(client,request->u.getDevice.deviceNumber,
                &(reply.u.getDevice.device),
                error);
      break;

    case pciConfigProtocolRequestOpReserveDevice:
      reply.u.reserveDevice.granted
        =reserveDevice(client,request->u.reserveDevice.deviceNumber,error);
      break;

    case pciConfigProtocolRequestOpReleaseDevice:
      reply.u.releaseDevice.released
        =releaseDevice(client,request->u.releaseDevice.deviceNumber,error);
      break;
    }

  reply.error=*error;

  if (TcpSocketSend(client->socket,&reply,sizeof(reply),error)<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "handleRequest(): TcpSocketSend(): %s\n",
                       PackosErrorToString(*error));
      return -1;
    }

  *error=reply.error;
  if (((*error)!=packosErrorNone)
      && !(((*error)==packosErrorNoSuchDevice)
           && (request->op==pciConfigProtocolRequestOpGetDevice)
           )
      )
    return -1;
  else
    return 0;
}

static inline struct irq_routing_table * pirq_check_routing_table(byte *addr)
{
  struct irq_routing_table *rt;
  int i;
  byte sum;

  rt = (struct irq_routing_table *) addr;
  if (rt->signature != PIRQ_SIGNATURE ||
      rt->version != PIRQ_VERSION ||
      rt->size % 16 ||
      rt->size < sizeof(struct irq_routing_table))
    return 0;
  sum = 0;
  for (i=0; i < rt->size; i++)
    sum += addr[i];
  if (!sum) {
    return rt;
  }
  return 0;
}

static struct irq_routing_table* findRoutingTable(void)
{
  byte* addr;
  for(addr = (byte*)0xf0000; addr < (byte *)0x100000; addr += 16) {
    struct irq_routing_table* rt = pirq_check_routing_table(addr);
    if (rt)
      return rt;
  }

  return 0;
}

static struct irq_info* pirq_get_info(PCIDevice* device)
{
  struct irq_routing_table* rt = routingTable;
  int entries =
    ((rt->size - sizeof(struct irq_routing_table))
     / sizeof(struct irq_info)
     );
  struct irq_info *info;

  for (info = rt->slots; entries--; info++)
    {
      if ((info->bus == device->addr.bus)
          && (device->addr.device) == ((info->devfn >> 3) & 0x1f)
          )
        return info;
    }
  return 0;
}

void pciDriverProcess(void)
{
  PackosError error;
  TcpSocket socket;
  Client first=0,last=0;
  if (UtilArenaInit(&error)<0)
    return;

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

  socket=TcpSocketNew(&error);
  if (!socket)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,"pciDriverProcess: TcpSocketNew(): %s\n",
                       PackosErrorToString(error));
      return;
    }

  {
    PackosAddress addr=PackosMyAddress(&error);
    if (error!=packosErrorNone)
      {
        PackosError tmp;
        TcpSocketClose(socket,&tmp);
        UtilPrintfStream(errStream,&tmp,"pciDriverProcess: PackosMyAddress(): %s\n",
                         PackosErrorToString(error));
        return;
      }

    if (TcpSocketBind(socket,addr,PCIConfigPort,&error)<0)
      {
        PackosError tmp;
        TcpSocketClose(socket,&tmp);
        UtilPrintfStream(errStream,&tmp,"pciDriverProcess: TcpSocketBind(): %s\n",
                         PackosErrorToString(error));
        return;
      }
  }

  if (TcpSocketListen(socket,&error)<0)
    {
      PackosError tmp;
      TcpSocketClose(socket,&tmp);
      UtilPrintfStream(errStream,&tmp,"pciDriverProcess: TcpSocketListen(): %s\n",
                       PackosErrorToString(error));
      return;
    }

  routingTable=findRoutingTable();
  if (!routingTable)
    UtilPrintfStream(errStream,&error,
                     "pciDriverProcess(): can't find routing table.  No IRQs will be available.\n");
  else
    UtilPrintfStream(errStream,&error,
                     "pciDriverProcess: found PCI routing table at %p; IRQ router is [%04x,%04x]\n",
                     routingTable,
                     routingTable->rtr_vendor, routingTable->rtr_device
                     );

  {
    int i=0;
    while (pciInit(i,&error)>=0)
      i++;

    if (error!=packosErrorNoSuchDevice)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&tmp,
                         "pciDriverProcess: pciInit(%d): %s\n",
                         i,PackosErrorToString(error)
                         );
        return;
      }
  }

  if (!routerDevice)
    UtilPrintfStream(errStream,&error,
                     "pciDriverProcess(): can't find IRQ router.  No IRQs will be available.\n");
  else
    {
      if (routerDevice->config.vendorId!=0x8086)
        UtilPrintfStream(errStream,&error,
                         "pciDriverProcess(): non-Intel IRQ router not supported.  No IRQs will be available.\n");
      else
        {
          int i;
          for (i=0; i<numDevices; i++)
            {
              byte line=devices[i].config.interruptLine;
              if (line)
                {
                  struct irq_info* info=pirq_get_info(devices+i);
                  if (!info)
                    {
                      UtilPrintfStream(errStream,&error,
                                       "[%04x:%04x]: !info\n",
                                       devices[i].config.vendorId,
                                       devices[i].config.deviceId);
                      continue;
                    }
                  byte pin=pciConfRead8(devices[i].addr.bus,
                                        devices[i].addr.device,
                                        devices[i].addr.function,
                                        PCIConstants.conf.interruptPinReg);
                  if (!pin)
                    {
                      UtilPrintfStream(errStream,&error,
                                       "[%04x:%04x]: !pin\n",
                                       devices[i].config.vendorId,
                                       devices[i].config.deviceId);
                      continue;
                    }

                  byte irq=pciConfRead8(routerDevice->addr.bus,
                                        routerDevice->addr.device,
                                        routerDevice->addr.function,
                                        info->irq[pin-1].link);

                  UtilPrintfStream(errStream,&error,
                                   "[%04x:%04x]: line %d => IRQ %d\n",
                                   (unsigned int)devices[i].config.vendorId,
                                   (unsigned int)devices[i].config.deviceId,
                                   (int)line,(int)irq);
                  devices[i].config.irq=irq;
                }
            }
        }
    }

#ifdef DEBUG
  UtilPrintfStream(errStream,&error,"PCI server: listening\n");
#endif

  {
    SchedulerControl control=SchedulerControlConnect(&error);
    if (!control)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&tmp,
                         "pciDriverProcess: SchedulerControlConnect(): %s\n",
                         PackosErrorToString(error)
                         );
        return;
      }

    if (SchedulerControlReportInited(control,&error)<0)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&tmp,
                         "pciDriverProcess: SchedulerControlReportInited(): %s\n",
                         PackosErrorToString(error)
                         );
        return;
      }

    if (SchedulerControlClose(control,&error)<0)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&tmp,
                         "pciDriverProcess: SchedulerControlClose(): %s\n",
                         PackosErrorToString(error)
                         );
        return;
      }
  }

  while (true)
    {
      if (TcpSocketAcceptPending(socket,&error)
          || (!first)
          )
        {
          TcpSocket clientSocket=TcpSocketAccept(socket,&error);
          if (!clientSocket)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,&tmp,
                               "pciDriverProcess(): TcpSocketAccept(): %s\n",
                               PackosErrorToString(error));
              return;
            }

          Client client=(Client)(malloc(sizeof(struct Client)));
          if (!client)
            {
              PackosError tmp;
              error=packosErrorOutOfMemory;
              UtilPrintfStream(errStream,&tmp,
                               "pciDriverProcess(): can't construct Client: %s\n",
                               PackosErrorToString(error));
              return;
            }

          client->socket=clientSocket;
          client->incoming.offset=0;
          client->filter.vendorId=client->filter.deviceId=
            client->filter.baseClass=client->filter.subClass=
            client->filter.programmingInterface=-1;

          client->prev=last;
          if (client->prev)
            client->prev->next=client;
          else
            first=client;
          last=client;

#ifdef DEBUG
          UtilPrintfStream(errStream,&error,"PCI server: accepted\n");
#endif
        }
      else
        {
          if (error!=packosErrorNone)
            {
              PackosError tmp;
              UtilPrintfStream(errStream,&tmp,
                               "pciDriverProcess(): TcpSocketAcceptPending(): %s\n",
                               PackosErrorToString(error));
              return;
            }
        }

      Client cur,next;      
      for (cur=first; cur; cur=next)
        {
          bool pending=false;
          next=cur->next;
#ifdef DEBUG
          UtilPrintfStream(errStream,&error,
                           "pciDriverProcess(): anything pending?\n");
#endif
          if (((cur!=first) || (cur!=last))
              && !(pending=TcpSocketReceivePending(cur->socket,&error))
              )
            {
              switch (error)
                {
                case packosErrorNone:
                case packosErrorStoppedForOtherSocket:
#ifdef DEBUG
                  UtilPrintfStream(errStream,&error,
                                   "pciDriverProcess(): no, nothing\n");
#endif
                  break;

                case packosErrorConnectionClosed:
                case packosErrorConnectionResetByPeer:
                  closeClient(cur,&first,&last);
                  {
                    PackosError tmp;
                    UtilPrintfStream(errStream,&tmp,
                                     "pciDriverProcess(): TcpSocketReceivePending(): %s\n",
                                     PackosErrorToString(error));
                  }
                  break;

                default:
                  {
                    PackosError tmp;
                    UtilPrintfStream(errStream,&tmp,
                                     "pciDriverProcess(): TcpSocketReceivePending(): %s\n",
                                     PackosErrorToString(error));
                    return;
                  }
                }
              continue;
            }

          {
            char* buff
              =((char*)(&(cur->incoming.request)))+cur->incoming.offset;
            int nbytes=sizeof(cur->incoming.request)-cur->incoming.offset;

#ifdef DEBUG
            UtilPrintfStream(errStream,&error,
                             "pciDriverProcess(): %s: let's read %d\n",
                             (pending ? "yes" : "no"),
                             nbytes);
#endif

            int actual=TcpSocketReceive(cur->socket,buff,nbytes,&error);
            if (actual<0)
              {
                if (error==packosErrorStoppedForOtherSocket)
                  continue;

                PackosError tmp;
                UtilPrintfStream(errStream,&tmp,
                                 "pciDriverProcess(): TcpSocketReceive(): %s\n",
                                 PackosErrorToString(error));
                return;
              }

#ifdef DEBUG
            {
              PackosError tmp;
              UtilPrintfStream(errStream,&tmp,
                               "pciDriverProcess(): read %d bytes of %d\n",
                               actual,nbytes);
            }
#endif

            nbytes-=actual;
            if (nbytes==0)
              {
                if (handleRequest(cur,&cur->incoming.request,&error)<0)
                  {
                    PackosError tmp;
                    UtilPrintfStream(errStream,&tmp,
                                     "pciDriverProcess(): handleRequest(): %s\n",
                                     PackosErrorToString(error));
                    switch (error)
                      {
                      case packosErrorResourceInUse:
                      case packosErrorResourceNotInUse:
                        break;

                      default:
                        closeClient(cur,&first,&last);
                        return;
                      }
                  }
                cur->incoming.offset=0;
              }
            else
              cur->incoming.offset+=actual;
          }
        }
    }
}
