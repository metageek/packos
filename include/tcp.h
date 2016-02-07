#ifndef _TCP_H_
#define _TCP_H_

#include <ip.h>

typedef struct TcpSocket* TcpSocket;
typedef enum {
  tcpSocketStateInvalid=0,
  tcpSocketStateClosed,
  tcpSocketStateListen,
  tcpSocketStateSynSent,
  tcpSocketStateSynReceived,
  tcpSocketStateEstablished,
  tcpSocketStateFinWait1,
  tcpSocketStateFinWait2,
  tcpSocketStateCloseWait,
  tcpSocketStateClosing,
  tcpSocketStateLastAck,
  tcpSocketStateTimeWait
} TcpSocketState;

typedef enum {
  tcpFlagFin=1,
  tcpFlagSyn=2,
  tcpFlagReset=4,
  tcpFlagPush=8,
  tcpFlagAck=16,
  tcpFlagUrgent=32
} TcpFlag;

const char* TcpSocketStateToString(TcpSocketState state,
                                   PackosError* error);

TcpSocket TcpSocketNew(PackosError* error);
int TcpSocketClose(TcpSocket socket,
                   PackosError* error);

TcpSocketState TcpSocketGetState(TcpSocket socket,
                                 PackosError* error);

int TcpSocketBind(TcpSocket socket,
                  PackosAddress addr,
                  uint16_t port,
                  PackosError* error);

int TcpSocketConnect(TcpSocket socket,
                     PackosAddress addr,
                     uint16_t port,
                     PackosError* error);
int TcpSocketListen(TcpSocket socket,
                    PackosError* error);
TcpSocket TcpSocketAccept(TcpSocket socket,
                          PackosError* error);
bool TcpSocketAcceptPending(TcpSocket socket,
                            PackosError* error);

PackosAddress TcpSocketGetPeerAddress(TcpSocket socket,
                                      PackosError* error);
int TcpSocketGetLocalPort(TcpSocket socket,
                          PackosError* error);

int TcpSocketSend(TcpSocket socket,
                  const void* buff,
                  uint32_t nbytes,
                  PackosError* error);

int TcpSocketReceive(TcpSocket socket,
                     void* buff,
                     uint32_t nbytes,
                     PackosError* error);

bool TcpSocketReceivePending(TcpSocket socket,
                             PackosError* error);

int TcpPoll(PackosError* error);

#endif /*_TCP_H_*/
