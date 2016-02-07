#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>

int main(int argc, const char* argv[])
{
  int N=5;
  int sock=socket(PF_INET6,SOCK_DGRAM,0);
  if (sock<0)
    {
      perror("socket");
      return 1;
    }

  while (N>0)
    {
      {
        const char msg[]="No One Expects the IPv6 Inquisition!";
        const size_t len=sizeof(msg);
        struct sockaddr_in6 dest;
        {
          const char* destStr="7e8e::3";
          if (inet_pton(AF_INET6,destStr,&(dest.sin6_addr))<=0)
            {
              perror("inet_pton");
              return 2;
            }
        }

        dest.sin6_family=AF_INET6;
        dest.sin6_port=htons(9000);
        dest.sin6_flowinfo=0;
        dest.sin6_scope_id=0;

        if (sendto(sock,msg,len,0,(struct sockaddr*)&dest,sizeof(dest))<0)
          {
            perror("sendto");
            return 3;
          }
      }

      {
        struct sockaddr_in6 from;
        socklen_t fromlen=sizeof(from);
        char buff[1501];
        int actual=recvfrom(sock,buff,sizeof(buff)-1,0,
                            (struct sockaddr*)&from,&fromlen
                            );
        if (actual<0)
          {
            perror("recvfrom");
            return 4;
          }

        buff[actual]=0;
        printf("Received: %s\n",buff);
      }

      N--;
    }

  close(sock);
  return 0;
}
