#ifndef _SWAP_CONTEXTK_H_
#define _SWAP_CONTEXTK_H_

#define SWAP_VIA_TSS

#ifndef ASM
#include <packos/context.h>

void loadContext(PackosContext dummy,
                 PackosContext to);
void swapContext(PackosContext from,
                 PackosContext to);
#endif

#endif /*_SWAP_CONTEXTK_H_*/
