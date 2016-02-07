#include <util/alloc.h>

int UtilArenaInitStatic(PackosError* error)
{
  return UtilArenaInit(arena,sizeof(arena),0,error);
}
