#ifndef _SCHEDULERS_COMMON_H_
#define _SCHEDULERS_COMMON_H_

#include <contextQueue.h>

/* Must be defined by the app.  Called from any Scheduler process,
 *  to create the processes to run.  Each context created must
 *  be added to the queue.
 */
int SchedulerCallbackCreateProcesses(PackosContextQueue contexts,
                                     PackosError* error);

/* A standard idle process; does nothing but loop. */
void SchedulerIdle(void);

#endif /*_SCHEDULERS_COMMON_H_*/
