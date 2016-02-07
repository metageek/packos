#include <iface-shm-protocol.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, const char* argv[])
{
  void* block;
  int shmid=shmget(IfaceShmKey,sizeof(IfaceShmBlock),0600);
  if (shmid<0)
    {
      perror("shmget");
      return 1;
    }

  block=(IfaceShmBlock*)(shmat(shmid,0,0));
  if (!block)
    {
      perror("shmat");
      return 2;
    }

  if (write(1,block,sizeof(IfaceShmBlock))<0)
    {
      perror("write");
      return 3;
    }

  return 0;
}
