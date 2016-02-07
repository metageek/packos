#ifndef _ICMP_TYPES_H_
#define _ICMP_TYPES_H_

typedef enum {
  icmpTypeDestinationUnreachable=1,
  icmpTypePacketTooBig=2,
  icmpTypeTimeExceeded=3,
  icmpTypeParameterProblem=4,
  icmpTypeEchoRequest=128,
  icmpTypeEchoReply=129
} IcmpType;

typedef enum {
  icmpDestinationUnreachableCodeNoRoute=0,
  icmpDestinationUnreachableCodeProhibited=1,
  icmpDestinationUnreachableCodeOutOfScope=2,
  icmpDestinationUnreachableCodeAddress=3,
  icmpDestinationUnreachableCodePort=4,
  icmpDestinationUnreachableCodeFailedGress=5,
  icmpDestinationUnreachableCodeRejectRoute=6
} IcmpDestinationUnreachableCode;

typedef enum {
  icmpTimeExceededCodeHopLimit=0,
  icmpTimeExceededCodeReassemblyTime=1
} IcmpTimeExceededCode;

typedef enum {
  icmpParameterProblemCodeErroneousHeader=0,
  icmpParameterProblemCodeUnrecognizedNextHeader=1,
  icmpParameterProblemCodeUnrecognizedOption=2
} IcmpParameterProblemCode;

typedef struct {
  byte type;
  byte code;
  uint16_t checksum;
  union {
    struct {
      uint32_t unused;
    } destinationUnreachable,timeExceeded;
    struct {
      uint32_t mtu;
    } packetTooBig;
    struct {
      uint32_t pointer;
    } parameterProblem;
    struct {
      uint16_t identifier,sequenceNumber;
    } echoRequest,echoReply;
  } u;
} IpHeaderICMP;

#endif /*_ICMP_TYPES_H_*/
