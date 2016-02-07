#ifndef _IFACE_SHM_PROTOCOL_H_
#define _IFACE_SHM_PROTOCOL_H_

#include <sys/types.h>
#include <packos/packet.h>

typedef struct {
  byte data[PACKOS_MTU];
  bool valid;
} IfaceShmBuff;

typedef struct {
  struct {
    IfaceShmBuff buff;
    int signumToPackos,signumToOutside;
  } send,receive;

  pid_t packosPid,outsidePid;
} IfaceShmBlock;

const key_t IfaceShmKey=0xfeed2460;

#endif /*_IFACE_SHM_PROTOCOL_H_*/
