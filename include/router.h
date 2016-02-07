#ifndef _ROUTER_H_
#define _ROUTER_H_

#include <ip.h>

typedef struct {
  struct {
    PackosAddressMask mask;
  } native,nonNative;
} RouterConfig;

void routerProcess(void);

#endif /*_ROUTER_H_*/
