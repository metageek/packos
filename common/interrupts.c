#include <packos/sys/interruptsP.h>
#include <packos/sys/entry-points.h>

PackosInterruptId PackosInterruptAliasLookup(PackosInterruptId alias,
                                             PackosError* error)
{
  switch (alias)
    {
    case PACKOS_INTERRUPT_ALIAS_CLOCK:
      return PACKOS_INTERRUPT_KERNEL_ID_CLOCK;

    default:
      *error=packosErrorNoSuchInterruptAlias;
      return -1;
    }
}
