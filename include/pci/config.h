#ifndef _PCI_CONFIG_H_
#define _PCI_CONFIG_H_

#include <packos/types.h>
#include <packos/packet.h>

typedef enum {
  pciBootRomSizeNone=0,
  pciBootRomSize8K=1,
  pciBootRomSize16K=2,
  pciBootRomSize32K=3,
  pciBootRomSize64K=4,
  pciBootRomSize128K=5
  /* Do we have values for 256K and 512K, too? */
} PCIBootRomSize;

typedef struct {
  uint16_t vendorId,deviceId,command,status;
  byte revisionId,programmingInterface,subClass,baseClass,cacheLineSize;
  byte latencyTime,headerType,builtInSelfTest;
  struct {
    bool valid,asIO;
    union {
      struct {
        void* physAddr;
        bool prefetchable;
        byte type;
      } mem;
      struct {
        uint32_t ioAddr;
      } io;
    } u;
  } baseAddrs[4];
  struct {
    uint16_t vendorId,deviceId;
  } subsystem;
  struct {
    bool enabled;
    void* physAddr;
    PCIBootRomSize size;
  } bootRom;
  byte interruptLine,irq,minimumGrantTimer,maximumLatencyTimer;
} PCIConfigData;

typedef struct {
  byte bus, device, function;
} PCIAddr;

typedef struct {
  PCIAddr addr;
  PCIConfigData config;
  PackosAddress owner;
} PCIDevice;

typedef struct PCIConnection* PCIConnection;

PCIConnection PCIConfigNew(PackosAddress pciDriverAddr,
                           PackosError* error);
int PCIConfigClose(PCIConnection pci,
                   PackosError* error);
int PCIConfigSetFilter(PCIConnection pci,
                       int32_t vendorId,
                       int32_t deviceId,
                       int32_t baseClass,
                       int32_t subClass,
                       int32_t programmingInterface,
                       PackosError* error);
int PCIConfigGetNumDevices(PCIConnection pci, PackosError* error);
int PCIConfigGetDevice(PCIConnection pci,
                       uint32_t deviceNumber,
                       PCIDevice* device,
                       PackosError* error);
int PCIConfigReserveDevice(PCIConnection pci,
                           uint32_t deviceNumber,
                           PackosError* error);
int PCIConfigReleaseDevice(PCIConnection pci,
                           uint32_t deviceNumber,
                           PackosError* error);

#endif /*_PCI_CONFIG_H_*/
