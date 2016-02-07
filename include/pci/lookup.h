#ifndef _PCI_LOOKUP_H_
#define _PCI_LOOKUP_H_

#include <packos/types.h>
#include <packos/errors.h>

const char* pciGetVendorName(uint16_t vendorId,
                             PackosError* error);
const char* pciGetDeviceName(uint16_t vendorId, uint16_t deviceId,
                             PackosError* error);
const char* pciGetControllerName(uint16_t vendorId, uint16_t deviceId,
                                 PackosError* error);

const char* pciGetSubClassName(byte baseClass,
                               byte subClass,
                               byte infClass,
                               PackosError* error);
const char* pciGetBaseClassName(byte baseClass,
                                PackosError* error);
const char* pciGetClassName(byte baseClass,
                            byte subClass,
                            byte infClass,
                            PackosError* error);

#endif /*_PCI_LOOKUP_H_*/
