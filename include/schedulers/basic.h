#ifndef _SCHEDULERS_BASIC_H_
#define _SCHEDULERS_BASIC_H_

#include <schedulers/common.h>

typedef struct {
  bool isDaemon,isRunning,isInited;
  PackosContext dependsOn;
} SchedulerBasicContextMetadata;

void SchedulerBasic(void);

#endif /*_SCHEDULERS_BASIC_H_*/
