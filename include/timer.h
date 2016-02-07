#ifndef _TIMER_H_
#define _TIMER_H_

#include <udp.h>

typedef struct Timer* Timer;

Timer TimerNew(UdpSocket socket,
               uint32_t sec,
               uint32_t usec,
               uint32_t id, /* included in the tick packets */
               bool repeat,
               PackosError* error);

int TimerClose(Timer timer,
               PackosError* error);

#endif /*_TIMER_H_*/
