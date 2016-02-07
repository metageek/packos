#ifndef _FILE_SYSTEM_DUMBFS_H_
#define _FILE_SYSTEM_DUMBFS_H_

#include <file-system.h>
#include <packos/packet.h>

FileSystem FileSystemDumbFSNew(PackosAddress addr,
                               uint32_t port,
                               PackosError* error);

#endif /*_FILE_SYSTEM_DUMBFS_H_*/
