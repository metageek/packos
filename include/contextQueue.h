#ifndef _PACKOS_CONTEXTQUEUE_H_
#define _PACKOS_CONTEXTQUEUE_H_

#include <packos/context.h>

typedef struct PackosContextQueue* PackosContextQueue;

typedef int (*PackosContextOp)(PackosContext context,
                               void* arg,
                               PackosError* error);
typedef bool (*PackosContextFilter)(PackosContext context,
                                    void* arg,
                                    PackosError* error);

PackosContextQueue PackosContextQueueNew(PackosError* error);
void PackosContextQueueDelete(PackosContextQueue queue,
                              PackosError* error);
int PackosContextQueueInsert(PackosContextQueue queue,
                             PackosContext context,
                             PackosError* error);
int PackosContextQueueAppend(PackosContextQueue queue,
                             PackosContext context,
                             PackosError* error);
int PackosContextQueueRemove(PackosContextQueue queue,
                             PackosContext context,
                             PackosError* error);
PackosContext PackosContextQueueHead(PackosContextQueue queue,
                                     PackosError* error);
int PackosContextQueueForEach(PackosContextQueue queue,
                              PackosContextOp op,
                              void* arg,
                              PackosError* error);
PackosContext PackosContextQueueFind(PackosContextQueue queue,
                                     PackosContextFilter filter,
                                     void* arg,
                                     PackosError* error);
int PackosContextQueueLen(PackosContextQueue queue,
                          PackosError* error);
bool PackosContextQueueEmpty(PackosContextQueue queue,
                             PackosError* error);

#endif /*_PACKOS_CONTEXTQUEUE_H_*/
