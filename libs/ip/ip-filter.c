#include <ip-filter.h>
#include <iface.h>

#include <util/alloc.h>
#include <util/stream.h>

IpFilter IpFilterInstall(IpIface iface,
                         IpFilterMethod filterMethod,
                         void* context,
                         PackosError* error)
{
  IpFilter res;
  if (!(iface && filterMethod))
    {
      *error=packosErrorInvalidArg;
      return 0;
    }

  res=(IpFilter)(malloc(sizeof(struct IpFilter)));
  if (!res)
    {
      *error=packosErrorOutOfMemory;
      return 0;
    }

  res->iface=iface;
  res->filterMethod=filterMethod;
  res->context=context;

  res->prev=iface->filters.last;
  if (res->prev)
    res->prev->next=res;
  else
    iface->filters.first=res;
  iface->filters.last=res;
  res->next=0;

  return res;
}

int IpFilterUninstall(IpFilter filter,
                      PackosError* error)
{
  if (!(filter && (filter->iface)))
    {
      *error=packosErrorInvalidArg;
      return -1;
    }

  if (filter->prev)
    filter->prev->next=filter->next;
  else
    filter->iface->filters.first=filter->next;
        

  if (filter->next)
    filter->next->prev=filter->prev;
  else
    filter->iface->filters.last=filter->prev;

  free(filter);
  return 0;
}

PackosPacket* IpFilterApply(IpIface iface,
                            PackosPacket* packet,
                            PackosError* error)
{
  IpFilterAction action=ipFilterActionPass;
  IpFilter cur;

  if (!(iface && packet))
    {
      *error=packosErrorInvalidArg;
      return ipFilterActionError;
    }

  for (cur=iface->filters.first;
       cur && (action==ipFilterActionPass);
       cur=cur->next
       )
    {
      *error=packosErrorNone;
      action=(cur->filterMethod)(iface,cur->context,packet,error);
      if ((action==ipFilterActionError)
          && ((*error)!=packosErrorNone)
          )
	{
	  UtilPrintfStream(errStream,error,"IpFilterApply(): method: %s\n",
		  PackosErrorToString(*error));
	}
    }

  switch (action)
    {
    case ipFilterActionPass:
      /*UtilPrintfStream(errStream,error,"IpFilterApply(): action==ipFilterActionPass\n");*/
      return packet;

    case ipFilterActionDrop:
      /*UtilPrintfStream(errStream,error,"IpFilterApply(): action==ipFilterActionDrop\n");*/
      *error=packosErrorPacketFilteredOut;
    case ipFilterActionError:
      /*UtilPrintfStream(errStream,error,"IpFilterApply(): action==ipFilterActionError: %s\n",
        PackosErrorToString(*error));*/
      *error=packosErrorNone;
      {
	PackosError tmp;
	PackosPacketFree(packet,&tmp);
      }
      return 0;

    case ipFilterActionErrorICMPed:
      /*UtilPrintfStream(errStream,error,"IpFilterApply(): action==ipFilterActionErrorICMPed\n");*/
      *error=packosErrorNone;
      return 0;

    case ipFilterActionForwarded:
      /*UtilPrintfStream(errStream,error,"IpFilterApply(): action==ipFilterActionForwarded\n");*/
      *error=packosErrorNone;
      return 0;

    case ipFilterActionReplied:
      /*UtilPrintfStream(errStream,error,"IpFilterApply(): action==ipFilterActionReplied\n");*/
      *error=packosErrorNone;
      return 0;

    default:
      UtilPrintfStream(errStream,error,"IpFilterApply(): action==?\n");
      *error=packosErrorInvalidArg;
      PackosPacketFree(packet,error);
      return 0;
    }
}
