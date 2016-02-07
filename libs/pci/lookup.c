#include <pci/lookup.h>

typedef struct {
  uint16_t vendorId;
  const char* name;
} PCIKnownVendor;

typedef struct {
  uint16_t vendorId,deviceId;
  const char* name;
  bool isController;
} PCIKnownDevice;

typedef struct {
  byte baseClass;
  const char* name;
} PCIBaseClass;

typedef struct {
  byte baseClass,subClass;
  int infClass; /* if -1, it's a wildcard */
  const char* name;
} PCISubClass;

#include "pciIds.c"

const char* pciGetVendorName(uint16_t vendorId,
                             PackosError* error)
{
  int i;
  for (i=0; knownVendors[i].name; i++)
    if (knownVendors[i].vendorId==vendorId)
      return knownVendors[i].name;

  *error=packosErrorNoSuchDevice;
  return 0;
}

static const PCIKnownDevice* seekDevice(uint16_t vendorId, uint16_t deviceId,
                                        PackosError* error)
{
  int i;
  for (i=0; knownDevices[i].name; i++)
    if ((knownDevices[i].vendorId==vendorId)
        && (knownDevices[i].deviceId==deviceId)
        )
      return knownDevices+i;

  *error=packosErrorNoSuchDevice;
  return 0;
}

const char* pciGetDeviceName(uint16_t vendorId, uint16_t deviceId,
                             PackosError* error)
{
  const PCIKnownDevice* dev=seekDevice(vendorId,deviceId,error);
  if (dev)
    return dev->name;

  *error=packosErrorNoSuchDevice;
  return 0;
}

const char* pciGetControllerName(uint16_t vendorId, uint16_t deviceId,
                                 PackosError* error)
{
  const PCIKnownDevice* dev=seekDevice(vendorId,deviceId,error);
  if (!dev) return 0;

  if (dev->isController)
    return dev->name;

  *error=packosErrorDeviceNotController;
  return 0;
}

const char* pciGetSubClassName(byte baseClass,
                               byte subClass,
                               byte infClass,
                               PackosError* error)
{
  int i;
  for (i=0; knownSubClasses[i].name; i++)
    if ((knownSubClasses[i].baseClass==baseClass)
        && (knownSubClasses[i].subClass==subClass)
        && ((knownSubClasses[i].infClass==infClass)
            || (knownSubClasses[i].infClass==-1)
            )
        )
      return knownSubClasses[i].name;

  *error=packosErrorNoSuchDevice;
  return 0;
}

const char* pciGetBaseClassName(byte baseClass,
                                PackosError* error)
{
  int i;
  for (i=0; knownBaseClasses[i].name; i++)
    if (knownBaseClasses[i].baseClass==baseClass)
      return knownSubClasses[i].name;

  *error=packosErrorNoSuchDevice;
  return 0;
}

const char* pciGetClassName(byte baseClass,
                            byte subClass,
                            byte infClass,
                            PackosError* error)
{
  const char* res=pciGetSubClassName(baseClass,subClass,infClass,error);
  if (res) return res;
  return pciGetBaseClassName(baseClass,error);
}
