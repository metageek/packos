#ifndef _SCHEDULER_CONTROL_PROTOCOL_H_
#define _SCHEDULER_CONTROL_PROTOCOL_H_

#include <packos/types.h>

#define SCHEDULER_CONTROL_FIXED_TCP_PORT 4000
#define SCHEDULER_CONTROL_PROTOCOL_VERSION 1

typedef enum {
  controlRequestCmdInvalid=0,
  controlRequestCmdReportInited=1
} ControlRequestCmd;

typedef struct {
  uint16_t version,cmd,requestId,reserved;
} ControlRequest;

typedef struct {
  uint16_t version,cmd,requestId,reserved;
  uint32_t errorAsInt;
} ControlReply;

#endif /*_SCHEDULER_CONTROL_PROTOCOL_H_*/
