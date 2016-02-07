#ifndef _SCHEDULERS_CONTROL_H_
#define _SCHEDULERS_CONTROL_H_

#include <packos/errors.h>

typedef struct SchedulerControl* SchedulerControl;

SchedulerControl SchedulerControlConnect(PackosError* error);
int SchedulerControlClose(SchedulerControl control,
                          PackosError* error);

/* Lets the scheduler know it can start giving time slots to
 *  contexts that depend on this one.
 */
int SchedulerControlReportInited(SchedulerControl control,
                                 PackosError* error);

#endif /*_SCHEDULERS_CONTROL_H_*/
