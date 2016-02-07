#include <util/alloc.h>
#include <util/stream.h>

#include <contextQueue.h>

typedef struct PackosContextQueueNode* PackosContextQueueNode;

struct PackosContextQueueNode {
  PackosContext context;
  PackosContextQueueNode next;
  PackosContextQueueNode prev;
};

struct PackosContextQueue {
  PackosContextQueueNode first;
  PackosContextQueueNode last;
};

#if 0
static void PackosContextQueueDump(PackosContextQueue queue, int max)
{
  PackosError error;
  PackosContextQueueNode cur;
  UtilPrintfStream(errStream,&error,
                   "PackosContextQueueDump(%p): first: %p last: %p\n",
                   queue,queue->first,queue->last);

  for (cur=queue->first; cur && (max>0); cur=cur->next, max--)
    UtilPrintfStream(errStream,&error," %p",cur->context);
  UtilPrintfStream(errStream,&error,"\n");
}
#endif

PackosContextQueue PackosContextQueueNew(PackosError* error)
{
  PackosContextQueue queue
    =(PackosContextQueue)(malloc(sizeof(struct PackosContextQueue)));
  if (!queue)
    {
      UtilPrintfStream(errStream,error,"malloc(PackosContextQueue)\n");
      *error=packosErrorOutOfMemory;
      return 0;
    }

  queue->first=queue->last=0;
  return queue;
}

void PackosContextQueueDelete(PackosContextQueue queue,
                              PackosError* error)
{
  PackosContextQueueNode cur;
  PackosContextQueueNode next;

  if (!queue) return;

  for (cur=queue->first; cur; cur=next)
    {
      next=cur->next;
      free(cur);
    }
}

static PackosContextQueueNode mkNode(PackosContextQueue queue,
                                     PackosContext context,
                                     PackosError* error)
{
  PackosContextQueueNode node;

  if (!(queue && context))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  node=(PackosContextQueueNode)(malloc(sizeof(struct PackosContextQueueNode)));
  if (!node)
    {
      UtilPrintfStream(errStream,error,"malloc(PackosContextQueueNode)\n");
      *error=packosErrorOutOfMemory;
      return 0;
    }

  node->prev=node->next=0;
  node->context=context;
  return node;
}

int PackosContextQueueInsert(PackosContextQueue queue,
                             PackosContext context,
                             PackosError* error)
{
  PackosContextQueueNode node=mkNode(queue,context,error);
  if (!node) return -1;

  node->prev=0;
  node->next=queue->first;
  if (node->next)
    node->next->prev=node;
  else
    queue->last=node;
  queue->first=node;

  return 0;
}

int PackosContextQueueAppend(PackosContextQueue queue,
                             PackosContext context,
                             PackosError* error)
{
  PackosContextQueueNode node=mkNode(queue,context,error);
  if (!node) return -1;

  node->next=0;
  node->prev=queue->last;
  if (node->prev)
    node->prev->next=node;
  else
    queue->first=node;
  queue->last=node;

  return 0;
}

static PackosContextQueueNode
PackosContextQueueSeek(PackosContextQueue queue,
                       PackosContext context,
                       PackosError* error)
{
  PackosContextQueueNode cur;
  for (cur=queue->first; cur; cur=cur->next)
    if (cur->context==context)
      return cur;

  *error=packosErrorDoesNotExist;
  return 0;
}

int PackosContextQueueRemove(PackosContextQueue queue,
                             PackosContext context,
                             PackosError* error)
{
  PackosContextQueueNode node=PackosContextQueueSeek(queue,context,error);
  if (!node) return -1;

  if (node->next)
    node->next->prev=node->prev;
  else
    queue->last=node->prev;

  if (node->prev)
    node->prev->next=node->next;
  else
    queue->first=node->next;

  free(node);
  return 0;
}

PackosContext PackosContextQueueHead(PackosContextQueue queue,
                                     PackosError* error)
{
  if (!queue)
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  if (!(queue->first))
    {
      *error=packosErrorDoesNotExist;
      return 0;
    }

  return queue->first->context;
}

int PackosContextQueueForEach(PackosContextQueue queue,
                              PackosContextOp op,
                              void* arg,
                              PackosError* error)
{
  PackosContextQueueNode cur;
  if (!(queue && op))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  for (cur=queue->first; cur; cur=cur->next)
    {
      if (op(cur->context,arg,error)<0)
        return -1;
    }

  return 0;
}

PackosContext PackosContextQueueFind(PackosContextQueue queue,
                                     PackosContextFilter filter,
                                     void* arg,
                                     PackosError* error)
{
  PackosContextQueueNode cur;
  if (!(queue && filter))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  for (cur=queue->first; cur; cur=cur->next) {
    if ((cur->context) && (filter(cur->context,arg,error))) {
      return cur->context;
    }

    if ((*error)!=packosErrorNone)
      return 0;
  }

  *error=packosErrorDoesNotExist;
  return 0;
}

bool PackosContextQueueEmpty(PackosContextQueue queue,
                             PackosError* error)
{
  return ((!queue) || (!(queue->first)));
}

int PackosContextQueueLen(PackosContextQueue queue,
                          PackosError* error)
{
  PackosContextQueueNode cur;
  int res=0;

  if (!error) return -2;
  if (!queue)
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  for (cur=queue->first; cur; cur=cur->next)
    res++;

  return res;
}
