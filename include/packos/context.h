#ifndef _PACKOS_CONTEXT_H_
#define _PACKOS_CONTEXT_H_

#include <packos/types.h>
#include <packos/errors.h>
#include <packos/packet.h>

typedef void (*PackosContextFunc)(void);

PackosContext PackosContextNew(PackosContextFunc func,
                               const char* name,
                               PackosError* error);
PackosContext PackosGetCurrentContext(PackosError* error);
const char* PackosContextGetName(PackosContext context,
                                 PackosError* error);
const char* PackosContextGetOwnName(PackosError* error);
int PackosSetIdleContext(PackosContext idleContext_,
                         PackosError* error);

void PackosContextExit(void);
void PackosContextYield(void);
/* Only for the scheduler. */
void PackosContextYieldTo(PackosContext context,
                          PackosError* error);
bool PackosContextBlocking(PackosContext context,
                           PackosError* error);
void PackosContextSetBlocking(PackosContext context,
                              bool blocking,
                              PackosError* error);
bool PackosContextFinished(PackosContext context,
                           PackosError* error);
PackosAddress PackosContextGetAddr(PackosContext context,
                                   PackosError* error);

char* PackosContextGetArena(PackosContext context,
                            SizeType* size,
                            PackosError* error);
void* PackosContextGetIfaceRegistry(PackosContext context,
                                    PackosError* error);
int PackosContextSetIfaceRegistry(PackosContext context,
                                  void* ifaceRegistry,
                                  void* oldIfaceRegistry,
                                  PackosError* error);

/* Used by the scheduler to annotate contexts with data the kernel doesn't
 *  need to standardize.
 */
int PackosContextSetMetadata(PackosContext context,
                             void* metadata,
                             PackosError* error);
/* If the return code is NULL, check *error; if it's packosErrorNone,
 *  it means the context's metadata really is set to NULL.
 */
void* PackosContextGetMetadata(PackosContext context,
                               PackosError* error);

#endif /*_PACKOS_CONTEXT_H_*/
