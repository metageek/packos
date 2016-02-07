#ifndef _LIBS_SCHEDULER_CONTROL_SRVR_H_
#define _LIBS_SCHEDULER_CONTROL_SRVR_H_

typedef struct SchedulerControlClient* SchedulerControlClient;
typedef struct SchedulerControlServer* SchedulerControlServer;

SchedulerControlServer SchedulerControlServerInit(PackosContextQueue contexts,
                                                  PackosError* error);
int SchedulerControlServerClose(SchedulerControlServer server,
                                PackosError* error);
int SchedulerControlServerPoll(SchedulerControlServer server,
                               PackosError* error);

#endif /*_LIBS_SCHEDULER_CONTROL_SRVR_H_*/
