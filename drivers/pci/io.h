#ifndef _DRIVERS_PCI_IO_H_
#define _DRIVERS_PCI_IO_H_

#include <packos/types.h>

static const struct {
  uint16_t regSelectorPort,regValuePort;
  struct {
    uint16_t vendorIdReg,deviceIdReg,commandReg;
    struct {
      uint16_t io,memory,master,special,invalidate,vgaPaletteSnoop;
      uint16_t parity,addrDataStepping,serr,fastBackToBack,intxDisable;
    } commandFlags;
    uint16_t statusReg;
    struct {
      uint32_t signaledSystemError,receivedMasterAbort,receivedTargetAbort;
    } statusFlags;
    uint16_t revisionIdReg,programmingInterfaceReg,subClassReg,baseClassReg;
    uint16_t cacheLineSizeReg,latencyTimeReg,headerTypeReg;
    byte headerTypeMultiFlag;
    uint16_t builtInSelfTestReg;
    uint16_t baseAddrReg[4];
    uint16_t secondaryStatusReg,cardbusCISPtrReg;
    struct {
      uint16_t vendorIdReg,deviceIdReg;
    } subsystem;
    uint16_t bootRomReg,capabilityPtrReg,interruptLineReg,interruptPinReg;
    uint16_t minimumGrantTimerReg,maximumLatencyTimerReg;
  } conf;
  uint32_t regBaseValue;
  uint32_t busValueMask;
  int busValueShift;
  uint32_t deviceValueMask;
  int deviceValueShift;
  uint32_t functionValueMask;
  int functionValueShift;
  uint32_t registerValueMask;
  int registerValueShift;
} PCIConstants={
  regSelectorPort:0xCF8,
  regValuePort:0xCFC,
  conf:{
    vendorIdReg:0x00,
    deviceIdReg:0x02,
    commandReg:0x04,
    commandFlags:{
      io:0x1,
      memory:0x2,
      master:0x4,
      special:0x8,
      invalidate:0x10,
      vgaPaletteSnoop:0x20,
      parity:0x40,
      addrDataStepping:0x80,
      serr:0x100,
      fastBackToBack:0x200,
      intxDisable:0x400
    },
    statusReg:0x06,
    statusFlags:{
      signaledSystemError:0x4000,
      receivedMasterAbort:0x2000,
      receivedTargetAbort:0x1000
    },
    revisionIdReg:0x08,
    programmingInterfaceReg:0x09,
    subClassReg:0x0a,
    baseClassReg:0x0b,
    cacheLineSizeReg:0x0c,
    latencyTimeReg:0x0d,
    headerTypeReg:0x0e,
    headerTypeMultiFlag:0x80,
    builtInSelfTestReg:0x0f,
    baseAddrReg:{0x10,0x14,0x18,0x1c},
    secondaryStatusReg:0x1e, /* ??? overlap with baseAddrReg[3] */
    cardbusCISPtrReg:0x28,
    subsystem:{
      vendorIdReg:0x2c,
      deviceIdReg:0x2e
    },
    bootRomReg:0x30,
    capabilityPtrReg:0x34,
    interruptLineReg:0x3c,
    interruptPinReg:0x3d,
    minimumGrantTimerReg:0x3e,
    maximumLatencyTimerReg:0x3f
  },
  regBaseValue:0x80000000,
  busValueMask:0xff0000,
  busValueShift:16,
  deviceValueMask:0xf800,
  deviceValueShift:11,
  functionValueMask:0x700,
  functionValueShift:8,
  registerValueMask:0xfc,
  registerValueShift:2
};

inline byte inb(uint16_t port)
{
  byte res=0;
  asm("movw %[port], %%dx; inb %%dx, %[res]": [res] "=r"(res) : [port] "r"(port) );
  return res;
}

inline void outl(uint16_t port, uint32_t v)
{
  asm volatile ("outl %0, %1" : : "a"(v), "Nd"(port) );
}

inline void outb(uint16_t port, byte v)
{
  asm volatile ("outb %0, %1" : : "a"(v), "Nd"(port) );
}

inline uint32_t pciValueToSelectReg(int bus, int device, int function, int reg)
{
  return (PCIConstants.regBaseValue
          | ((bus << PCIConstants.busValueShift)
             & PCIConstants.busValueMask
             )
          | ((device << PCIConstants.deviceValueShift)
             & PCIConstants.deviceValueMask
             )
          | ((function << PCIConstants.functionValueShift)
             & PCIConstants.functionValueMask
             )
          | (((reg/4) << PCIConstants.registerValueShift)
             & PCIConstants.registerValueMask
             )
          );
}

inline byte pciConfRead8(int bus, int device, int function, int reg)
{
  outl(PCIConstants.regSelectorPort,
       pciValueToSelectReg(bus,device,function,reg)
       );
  return inb(PCIConstants.regValuePort+(reg&3));
}

inline uint16_t pciConfRead16(int bus, int device, int function, int reg)
{
  uint16_t low=pciConfRead8(bus,device,function,reg);
  uint16_t high=pciConfRead8(bus,device,function,reg+1);
  return low | (high <<8);
}

inline uint32_t pciConfRead32(int bus, int device, int function, int reg)
{
  uint32_t low=pciConfRead16(bus,device,function,reg);
  uint32_t high=pciConfRead16(bus,device,function,reg+2);
  return low | (high <<16);
}

inline void pciConfWrite8(int bus, int device, int function, int reg,
                             byte value)
{
  outl(PCIConstants.regSelectorPort,pciValueToSelectReg(bus,device,function,reg));
  outb(PCIConstants.regValuePort+(reg&3),value);
}

inline void pciConfWrite16(int bus, int device, int function, int reg,
                              uint16_t value)
{
  pciConfWrite8(bus,device,function,reg,(value&0xff));
  pciConfWrite8(bus,device,function,reg+1,(value>>8));
}

inline void pciConfWrite32(int bus, int device, int function, int reg,
                              uint32_t value)
{
  pciConfWrite16(bus,device,function,reg,(value&0xffff));
  pciConfWrite16(bus,device,function,reg+2,(value>>16));
}

#endif /*_DRIVERS_PCI_IO_H_*/
