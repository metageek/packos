#include <packos/packet.h>
#include <iface-shm-protocol.h>

#include <stdio.h>
#include <stdlib.h>
#include <util/alloc.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/ioctl.h>

static IfaceShmBlock* block=0;
static bool canSendToPackos=true;
static int tunFd=-1;

static void incomingPacket(PackosPacket* packet);

static void packetFromPackos(void)
{
  if (!(block->send.buff.valid))
    return;

  incomingPacket((PackosPacket*)(block->send.buff.data));

  block->send.buff.valid=false;
  kill(block->packosPid,
       block->send.signumToPackos);
}

static void packetToPackos(void)
{
  canSendToPackos=true;
}

static void signalHandler(int signum)
{
  fprintf(stderr,"signalHandler(%d)\n",signum);

  if (signum==block->send.signumToOutside)
    packetFromPackos();
  else
    {
      if (signum==block->receive.signumToOutside)
        packetToPackos();
      else
        fprintf(stderr,"\tunknown signal\n");
    }
}

static void incomingPacket(PackosPacket* packet)
{
  int len=ntohs(packet->ipv6.payloadLength)+40;
  void* pad=((char*)&(packet->ipv6))-4;
  fprintf(stderr,"received %d-byte packet\n",len);

  {
    struct tun_pi* pi=(struct tun_pi*)pad;
    pi->flags=0;
    pi->proto=ntohs(0x86dd);
  }

  write(tunFd,pad,len+4);
}

static void sendPacket(PackosPacket* packet)
{
  if (!canSendToPackos) return;

  memcpy(block->receive.buff.data,packet,PACKOS_MTU);
  block->receive.buff.valid=true;
  canSendToPackos=false;
  kill(block->packosPid,
       block->receive.signumToPackos);
  fprintf(stderr,"kill(%d,%d)\n",block->packosPid,
	  block->receive.signumToPackos);
}

static int tun_alloc(char* device,
                     int buflen)
{
  struct ifreq ifr;
  int fd;

  if( (fd = open("/dev/net/tun", O_RDWR)) < 0 )
    return -1;

  memset(&ifr, 0, sizeof(ifr));

  /* Flags: IFF_TUN   - TUN device (no Ethernet headers) 
   *        IFF_TAP   - TAP device  
   *
   *        IFF_NO_PI - Do not provide packet information  
   */ 
  ifr.ifr_flags = IFF_TUN;
  if (*device)
    strncpy(ifr.ifr_name, device, IFNAMSIZ);

  if (ioctl(fd, TUNSETIFF, (void *) &ifr)<0) {
    int tmp=errno;
    close(fd);
    errno=tmp;
    return -1;
  }
  if (strlen(ifr.ifr_name)+1>buflen) {
    close(fd);
    errno=EOVERFLOW;
    return -1;
  }

  strcpy(device, ifr.ifr_name);
  return fd;
}

int main(int argc, const char* argv[])
{
  int shmid=shmget(IfaceShmKey,sizeof(IfaceShmBlock),0);
  char deviceName[1024]="";
  if (shmid<0)
    {
      perror("shmget");
      return 1;
    }

  tunFd=tun_alloc(deviceName,sizeof(deviceName));
  if (tunFd<0)
    {
      perror("tun_alloc");
      return 3;
    }

  fprintf(stderr,"tun device \"%s\"\n",deviceName);

  {
    const char* cmdPatterns[]={
      "/sbin/ifconfig %s 192.168.15.1 pointopoint 192.168.15.2",
      "/sbin/ifconfig %s add 7e8f::2/16",
      "/sbin/ifconfig %s",
      "/sbin/route -A inet6 add 7e8e::/24 gw 7e8f::1"
    };
    int i;
    const int N=sizeof(cmdPatterns)/sizeof(cmdPatterns[0]);
    for (i=0; i<N; i++)
      {
        char cmd[1024];
        sprintf(cmd,cmdPatterns[i],deviceName);
        system(cmd);
      }
  }

  if (setuid(500)<0)
    {
      perror("setuid");
      return 7;
    }

  block=(IfaceShmBlock*)(shmat(shmid,0,0));
  if (!block)
    {
      perror("shmat");
      close(tunFd);
      return 2;
    }

  block->send.signumToOutside=SIGUSR1;
  block->receive.signumToOutside=SIGUSR2;

  if (block->send.buff.valid)
    packetFromPackos();

  {
    struct sigaction sigdat;
    struct sigaction cur;
    sigdat.sa_handler=signalHandler;
    sigemptyset(&sigdat.sa_mask);
    sigaddset(&sigdat.sa_mask,block->send.signumToOutside);
    sigaddset(&sigdat.sa_mask,block->receive.signumToOutside);
    sigdat.sa_flags=SA_RESTART;

    if (sigaction(block->send.signumToOutside,&sigdat,&cur)<0)
      {
	perror("sigaction(block->send.signumToOutside)");
	return 5;
      }

    if (sigaction(block->receive.signumToOutside,&sigdat,&cur)<0)
      {
	perror("sigaction(block->receive.signumToOutside)");
	return 6;
      }
  }

  block->outsidePid=getpid();

  kill(block->packosPid,block->send.signumToPackos);

  while (1)
    {
      char buff[PACKOS_MTU];
      char* base=buff+32-sizeof(struct tun_pi);
      int actual=read(tunFd,
                      base,
                      sizeof(buff)-32+sizeof(struct tun_pi)
                      );
      if (actual<0)
        {
          perror("read");
          return 8;
        }

      {
        struct tun_pi* pi=(struct tun_pi*)base;
        if (pi->proto!=ntohs(0x86dd))
          {
            fprintf(stderr,"That's not an IPv6 packet.\n");
            continue;
          }
      }

      memset(buff,0,32);

      sendPacket((PackosPacket*)buff);
      fprintf(stderr,"sent %d-byte packet\n",
			  (int)(actual-sizeof(struct tun_pi))
		  );
    }

  return 0;
}
