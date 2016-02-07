#include <pci/pci.h>
#include <drivers/rtl8139.h>
#include <tcp.h>
#include <udp.h>
#include <util/printf.h>
#include <util/alloc.h>
#include <util/string.h>
#include <iface-native.h>
#include <packos/context.h>
#include <packos/interrupts.h>

static PCIDevice devices[8];
static const int maxDevices=sizeof(devices)/sizeof(devices[0]);
static int numDevices=0;

static byte receiveBuffer[64*1024*1024+16];

void rtl8139DriverProcess(void)
{
  PackosError error;
  extern PackosAddress pciDriverAddr;
  UdpSocket interruptSocket;

  if (UtilArenaInit(&error)<0)
    return;

  IpIface iface=IpIfaceNativeNew(&error);
  if (!iface)
    {
      UtilPrintfStream(errStream,&error,
                       "rtl8139DriverProcess(): IpIfaceNativeNew: %s\n",
                       PackosErrorToString(error));
      return;
    }

  if (IpIfaceRegister(iface,&error)<0)
    {
      UtilPrintfStream(errStream,&error,
                       "rtl8139DriverProcess(): IpIfaceRegister: %s\n",
                       PackosErrorToString(error));
      return;
    }

  interruptSocket=UdpSocketNew(&error);
  if (!interruptSocket)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "rtl8139DriverProcess(): UdpSocketNew(): %s\n",
                       PackosErrorToString(error));
      return;
    }

  if (UdpSocketBind(interruptSocket,
                    PackosMyAddress(&error),
                    0,
                    &error)
      <0
      )
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "rtl8139DriverProcess(): UdpSocketBind(): %s\n",
                       PackosErrorToString(error));
      return;
    }

  PCIConnection pci=PCIConfigNew(pciDriverAddr,&error);
  if (!pci)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "rtl8139DriverProcess(): PCIConfigNew(): %s\n",
                       PackosErrorToString(error));
      return;
    }

  if (PCIConfigSetFilter(pci,-1,0x8139,2,-1,-1,&error)<0)
    {
      UtilPrintfStream(errStream,&error,
                       "rtl8139DriverProcess(): PCIConfigSetFilter: %s\n",
                       PackosErrorToString(error));
      return;
    }

  int N=PCIConfigGetNumDevices(pci,&error);
  if (N<0)
    {
      PackosError tmp;
      UtilPrintfStream(errStream,&tmp,
                       "rtl8139DriverProcess(): PCIConfigGetNumDevices: %s\n",
                       PackosErrorToString(error));
      return;
    }

  UtilPrintfStream(errStream,&error,
                   "There are %d RTL8139 cards in the system",
                   N);

  if (N>maxDevices)
    {
      UtilPrintfStream(errStream,&error," (max %d)\n",maxDevices);
      N=maxDevices;
    }
  else
    UtilPrintfStream(errStream,&error,"\n");
                       
  {
    int i;
    for (i=0; i<N; i++)
      {
        PCIDevice device;
        if (PCIConfigGetDevice(pci,i,&device,&error)<0)
          {
            PackosError tmp;
            UtilPrintfStream(errStream,&tmp,
                             "rtl8139DriverProcess(): PCIConfigGetDevice(%d): %s\n",
                             i,
                             PackosErrorToString(error));
            break;
          }

        PCIConfigData* config=&(device.config);
        if (!((config->baseAddrs[1].valid) && !(config->baseAddrs[1].asIO)))
          {
            UtilPrintfStream(errStream,&error,
                             "rtl8139DriverProcess(): device #%d is not memory-mapped\n",
                             i);
            continue;
          }

        {
          void* memBase=config->baseAddrs[1].u.mem.physAddr;
          uint32_t macHigh=*((uint32_t*)(memBase+4));
          uint32_t macLow=*((uint32_t*)memBase);
          UtilPrintfStream(errStream,&error,
                           "Device %d: %02x:%02x:%02x:%02x:%02x:%02x IRQ %d\n",
                           i,
                           macLow&0xff,
                           (macLow>>8)&0xff,
                           (macLow>>16)&0xff,
                           (macLow>>24),
                           (macHigh&0xff),
                           (macHigh>>8),
                           (int)(config->irq)
                           );
        }

        if (PCIConfigReserveDevice(pci,i,&error)<0)
          {
            PackosError tmp;
            UtilPrintfStream(errStream,&tmp,
                             "rtl8139DriverProcess(): PCIConfigReserveDevice(%d): %s\n",
                             i,
                             PackosErrorToString(error));
            continue;
          }

        UtilMemcpy(&(devices[numDevices++]),&device,sizeof(device));
      }
  }

  if (PCIConfigClose(pci,&error)<0)
    {
      UtilPrintfStream(errStream,&error,
                       "rtl8139DriverProcess(): PCIConfigClose: %s\n",
                       PackosErrorToString(error));
      return;
    }

  int numRegistered;

  {
    int interruptPort=UdpSocketGetLocalPort(interruptSocket,&error);
    if (interruptPort<0)
      {
        PackosError tmp;
        UtilPrintfStream(errStream,&tmp,
                         "rtl8139DriverProcess(): UdpSocketGetLocalPort: %s\n",
                         PackosErrorToString(error));
      }

    int i;
    for (i=numRegistered=0; i<numDevices; i++)
      {
        if (!(devices[i].config.irq))
          {
            UtilPrintfStream(errStream,&error,
                             "\tcard #%d: can't register for IRQ 0\n",
                             i);
            continue;
          }

        if (PackosInterruptRegisterFor(devices[i].config.irq,
                                       interruptPort,
                                       &error)
            <0)
          {
            PackosError tmp;
            UtilPrintfStream(errStream,&tmp,
                             "rtl8139DriverProcess(): <%d>: PackosInterruptRegisterFor(%d): %s\n",
                             i,(int)(devices[i].config.irq),
                             PackosErrorToString(error));
            continue;
          }

        {
          void* base=(devices[i].config.baseAddrs[1].u.mem.physAddr);
          uint16_t* interruptMaskPtr=(uint16_t*)(((char*)base)+0x3c);
          UtilPrintfStream(errStream,&error,
                           "rtl8139 #%d: registered for IRQ %d; mask %x\n",
                           i,devices[i].config.irq,(int)(*interruptMaskPtr));
          numRegistered++;

#if 0
          int i,N;
          for (i=0, N=0xf0; (i*4)<N; i++)
            {
              uint32_t* u=((uint32_t*)base)+i;
              if ((i%4)==0)
                UtilPrintfStream(errStream,&error,"%x: ",i*4);
              else
                UtilPrintfStream(errStream,&error," ");

              UtilPrintfStream(errStream,&error,"%x",(*u));

              if ((i%4)==3)
                UtilPrintfStream(errStream,&error,"\n");
            }
          if (((i-1)%4)!=3)
            UtilPrintfStream(errStream,&error,"\n");
#endif

          byte* cmdRegister=((byte*)base)+0x37;
          UtilPrintfStream(errStream,&error,"resetting...");
          (*cmdRegister)|=0x10;
          while ((*cmdRegister) & 0x10)
            UtilPrintfStream(errStream,&error,".");
          UtilPrintfStream(errStream,&error,"done\n");

          *interruptMaskPtr|=((1<<1)|(1<<0));

          uint32_t* receiveConfig=(uint32_t*)(((byte*)base)+0x44);
          *receiveConfig=(3<<11)|(1<<4)|(1<<3)|(1<<2)|(1<<1)|(1<<0);

          uint32_t* receiveBufferAddr=(uint32_t*)(((byte*)base)+0x30);
          *receiveBufferAddr=(uint32_t)(receiveBuffer);
          *cmdRegister|=(1<<3);
        }
      }
  }

  if (numRegistered<=0)
    {
      UtilPrintfStream(errStream,&error,"rtl8139: no cards found\n");
      return;
    }

  const int limit=4;
  int numIrqs=0;
  while (numIrqs<limit)
    {
      {
        PackosPacket* packet=UdpSocketReceive(interruptSocket,0,false,&error);
        if (!packet)
          {
            PackosError tmp;
            switch (error)
              {
              case packosErrorStoppedForOtherSocket:
              case packosErrorPacketFilteredOut:
                UtilPrintfStream(errStream,&tmp,
                                 "rtl8139: UdpSocketReceive(): OK: %s\n",
                                 PackosErrorToString(error));
                continue;

              default:
                UtilPrintfStream(errStream,&tmp,
                                 "rtl8139: UdpSocketReceive(): %s\n",
                                 PackosErrorToString(error));
                return;
              }
          }

        PackosPacketFree(packet,&error);
        if (error!=packosErrorNone)
          {
            PackosError tmp;
            UtilPrintfStream(errStream,&tmp,
                             "rtl8139: PackosPacketFree(): %s\n",
                             PackosErrorToString(error));
          }
      }

      {
        void* base=(devices[0].config.baseAddrs[1].u.mem.physAddr);
        uint16_t* interruptMaskPtr=(uint16_t*)(((char*)base)+0x3c);

        uint16_t* interruptStateRegPtr
          =(uint16_t*)(((char*)base)+0x3e);
        uint16_t interruptStateReg=*interruptStateRegPtr;

        uint32_t* functionPresentStateRegPtr
          =(uint32_t*)(((char*)base)+0xf8);
        uint32_t functionPresentStateReg=*functionPresentStateRegPtr;

        UtilPrintfStream(errStream,&error,
                         "rtl8139: interrupt: isr %x[%s %s %s %s %s %s %s %s %s %s] fpsr %x[%s %s]\n",
                         interruptStateReg,
                         ((interruptStateReg & (1U<<15)) ? "SERR" : ""),
                         ((interruptStateReg & (1U<<14)) ? "TimeOut" : ""),
                         ((interruptStateReg & (1U<<13)) ? "LenChg" : ""),
                         ((interruptStateReg & (1U<<6)) ? "FOVW" : ""),
                         ((interruptStateReg & (1U<<5)) ? "PUN" : ""),
                         ((interruptStateReg & (1U<<4)) ? "RXOVW" : ""),
                         ((interruptStateReg & (1U<<3)) ? "TER" : ""),
                         ((interruptStateReg & (1U<<2)) ? "TOK" : ""),
                         ((interruptStateReg & (1U<<1)) ? "RER" : ""),
                         ((interruptStateReg & (1U<<0)) ? "ROK" : ""),
                         functionPresentStateReg,
                         ((functionPresentStateReg & (1U<<15)) ? "INTR" : ""),
                         ((functionPresentStateReg & (1U<<4)) ? "GWAKE" : "")
                         );
        *interruptStateRegPtr=0;
      }
      //numIrqs++;
    }
}
