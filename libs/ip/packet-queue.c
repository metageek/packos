#include <util/alloc.h>
#include <util/stream.h>
#include <ip.h>
#include <packet-queue.h>

PackosPacketQueue PackosPacketQueueNew(int capacity,
                                       PackosError* error)
{
  PackosPacketQueue res
    =(PackosPacketQueue)(malloc(sizeof(struct PackosPacketQueue)));
  if (!res)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  res->packets=(PackosPacket**)(malloc(capacity*sizeof(PackosPacket*)));
  if (!(res->packets))
    {
      free(res);
      *error=packosErrorOutOfMemory;
      return 0;
    }

  res->capacity=capacity;
  res->start=res->count=0;
  return res;
}

int PackosPacketQueueDelete(PackosPacketQueue queue,
                            PackosError* error)
{
  if (!queue)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  {
    int i;
    int N=queue->capacity;
    for (i=0; i<queue->count; i++)
      {
        PackosPacket* packet=queue->packets[(queue->start+i)%N];
        if (packet)
          PackosPacketFree(packet,error);
      }
  }

  free(queue);
  return 0;
}

int PackosPacketQueueEnqueue(PackosPacketQueue queue,
                             PackosPacket* packet,
                             PackosError* error)
{
  if (!(queue && packet))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if ((queue->count)>=(queue->capacity))
    {
      *error=packosErrorQueueFull;
      return -1;
    }

  {
    int i=((queue->start)+(queue->count))%(queue->capacity);
    queue->packets[i]=packet;
  }
  queue->count++;
  return 0;
}

PackosPacket* PackosPacketQueueDequeue(PackosPacketQueue queue,
                                       byte protocolExpected,
                                       PackosError* error)
{
  if (!queue)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  if (!(queue->count))
    {
      *error=packosErrorQueueEmpty;
      return 0;
    }

  if (protocolExpected)
    {
      PackosPacket* res=0;
      int i;
      for (i=0; i<queue->count; i++)
        {
          PackosPacket* cur=queue->packets[(queue->start+i)%(queue->capacity)];
          int protocol=IpPacketProtocol(cur,error);
          if (protocol==protocolExpected)
            {
              res=cur;
              break;
            }
        }

      while (i<queue->count)
        {
          queue->packets[(queue->start+i)%(queue->capacity)]
            =queue->packets[(queue->start+i+1)%(queue->capacity)];
          i++;
        }

      if (res) queue->count--;

      return res;
    }
  else
    {
      PackosPacket* res=queue->packets[queue->start];
      queue->start++;
      queue->start%=(queue->capacity);
      queue->count--;

      return res;
    }
}

bool PackosPacketQueueNonEmpty(PackosPacketQueue queue,
                               PackosError* error)
{
  if (!queue)
    {
      *error=packosErrorInvalidArg;
      return false;
    }

  *error=packosErrorNone;
  return (queue->count>0);
}
