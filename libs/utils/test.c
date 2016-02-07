#include <util/stream-stdio.h>
#include <util/alloc.h>
#include <schedulers/basic.h>
#include <packos/sys/contextP.h>

static void testProcess(void)
{
  PackosError error;

  if (UtilArenaInit(&error)<0)
    return;

  if (StreamStdioInitStreams(&error)<0) return;

  if (UtilPrintfStream(errStream,&error,"Now is the [%s] [%d] all good men [%02d] come\n","time",4,2)<0)
    {
      UtilPrintfStream(errStream,&error,"UtilPrintfStream(): %s\n",PackosErrorToString(error));
      return;
    }

  if (UtilPrintfStream(errStream,&error,"%lld\n",0x1234567812345678ULL)<0)
    {
      UtilPrintfStream(errStream,&error,"UtilPrintfStream(): %s\n",PackosErrorToString(error));
      return;
    }
}

int SchedulerCallbackCreateProcesses(PackosContextQueue contexts,
                                     PackosError* error)
{
  PackosContext test=PackosContextNew(testProcess,"test",error);
  if (!test)
    {
      UtilPrintfStream(errStream,error,"PackosContextNew(test): %s\n",
              PackosErrorToString(*error));
      return -1;
    }

  PackosContextQueueAppend(contexts,test,error);

  return 0;
}

int main(int argc, const char* argv[])
{
  PackosError error;

  PackosSimContextLoop(PackosContextNew(SchedulerBasic,"scheduler",&error),
                       &error);
  UtilPrintfStream(errStream,&error,"Loop terminated\n");
  return 0;
  return 0;
}
