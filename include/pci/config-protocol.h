#ifndef _PCI_CONFIG_PROTOCOL_H_
#define _PCI_CONFIG_PROTOCOL_H_

#include <pci/config.h>

static const int PCIConfigPort=2000;

typedef enum {
  pciConfigProtocolRequestOpInvalid=0,
  pciConfigProtocolRequestOpSetFilter,
  pciConfigProtocolRequestOpGetNumDevices,
  pciConfigProtocolRequestOpGetDevice,
  pciConfigProtocolRequestOpReserveDevice,
  pciConfigProtocolRequestOpReleaseDevice
} PCIConfigProtocolRequestOp;

typedef struct {
  PCIConfigProtocolRequestOp op;
  uint32_t requestId;
  union {
    struct {
      int32_t vendorId,deviceId,baseClass,subClass,programmingInterface;
    } setFilter;
    struct {
      uint32_t deviceNumber;
    } getDevice,reserveDevice,releaseDevice;
  } u;
} PCIConfigProtocolRequest;

typedef struct {
  PCIConfigProtocolRequestOp op;
  uint32_t requestId;
  PackosError error;
  union {
    struct {
      uint32_t numDevices;
    } getNumDevices;
    struct {
      PCIDevice device;
    } getDevice;
    struct {
      bool granted;
    } reserveDevice;
    struct {
      bool released;
    } releaseDevice;
  } u;
} PCIConfigProtocolReply;

#endif /*_PCI_CONFIG_PROTOCOL_H_*/
