#ifndef _LIBS_SCHEDULER_TIMER_SRVR_H_
#define _LIBS_SCHEDULER_TIMER_SRVR_H_

typedef struct TimerClient* TimerClient;
typedef struct TimerServer* TimerServer;

TimerServer TimerServerInit(PackosError* error);
int TimerServerClose(TimerServer server,
                     PackosError* error);
int TimerServerTick(TimerServer server,
                    PackosError* error);

#endif /*_LIBS_SCHEDULER_TIMER_SRVR_H_*/
