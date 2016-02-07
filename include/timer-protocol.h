#ifndef _TIMER_PROTOCOL_H_
#define _TIMER_PROTOCOL_H_

#define TIMER_FIXED_UDP_PORT 4000
#define TIMER_PROTOCOL_VERSION 1

typedef enum {
  timerRequestCmdInvalid=0,
  timerRequestCmdOpen=1,
  timerRequestCmdClose=2,
  timerRequestCmdTick=3
} TimerRequestCmd;

typedef struct {
  uint32_t id;
  uint16_t version,cmd,requestId,reserved;
  union {
    struct {
      uint32_t sec,usec;
      bool repeat;
    } open;
  } args;
} TimerRequest;

typedef struct {
  uint32_t id;
  uint16_t cmd,requestId;
  union {
    struct {
      uint32_t errorAsInt;
    } open,close;
  } u;
} TimerReply;

#endif /*_TIMER_PROTOCOL_H_*/
