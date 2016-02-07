# DO NOT DELETE

basic.o: ../../include/util/alloc.h ../../include/packos/types.h
basic.o: ../../include/packos/errors.h ../../include/util/stream.h
basic.o: ../../include/util/printf.h ../../include/stdarg.h
basic.o: ../../include/packos/packet.h ../../include/packos/sys/contextP.h
basic.o: ../../include/packos/context.h ../../include/packos/config.h
basic.o: ../../include/packos/interrupts.h ../../include/contextQueue.h
basic.o: ../../include/packos/arch.h ../../include/udp.h ../../include/ip.h
basic.o: ../../include/icmp-types.h ../../include/iface-native.h
basic.o: ../../include/iface.h ../../include/tcp.h ../../include/ip-filter.h
basic.o: ../../include/packet-queue.h ../../include/timer.h
basic.o: ../../include/schedulers/basic.h ../../include/schedulers/common.h
basic.o: timerServer.h controlServer.h
common.o: ../../include/schedulers/common.h ../../include/contextQueue.h
common.o: ../../include/packos/context.h ../../include/packos/types.h
common.o: ../../include/packos/errors.h ../../include/packos/packet.h
timerServer.o: ../../include/packos/arch.h ../../include/packos/types.h
timerServer.o: ../../include/util/stream.h ../../include/packos/errors.h
timerServer.o: ../../include/util/printf.h ../../include/stdarg.h
timerServer.o: ../../include/util/alloc.h ../../include/udp.h
timerServer.o: ../../include/ip.h ../../include/packos/packet.h
timerServer.o: ../../include/icmp-types.h ../../include/timer-protocol.h
timerServer.o: timerServer.h
contextQueue.o: ../../include/util/alloc.h ../../include/packos/types.h
contextQueue.o: ../../include/packos/errors.h ../../include/util/stream.h
contextQueue.o: ../../include/util/printf.h ../../include/stdarg.h
contextQueue.o: ../../include/contextQueue.h ../../include/packos/context.h
contextQueue.o: ../../include/packos/packet.h
controlServer.o: ../../include/schedulers/control-protocol.h
controlServer.o: ../../include/packos/types.h
controlServer.o: ../../include/schedulers/basic.h
controlServer.o: ../../include/schedulers/common.h
controlServer.o: ../../include/contextQueue.h ../../include/packos/context.h
controlServer.o: ../../include/packos/errors.h ../../include/packos/packet.h
controlServer.o: ../../include/tcp.h ../../include/ip.h
controlServer.o: ../../include/icmp-types.h ../../include/util/alloc.h
controlServer.o: ../../include/util/printf.h ../../include/util/stream.h
controlServer.o: ../../include/stdarg.h ../../include/packos/arch.h
controlServer.o: controlServer.h
controlClient.o: ../../include/schedulers/control-protocol.h
controlClient.o: ../../include/packos/types.h
controlClient.o: ../../include/schedulers/control.h
controlClient.o: ../../include/packos/errors.h ../../include/tcp.h
controlClient.o: ../../include/ip.h ../../include/packos/packet.h
controlClient.o: ../../include/icmp-types.h ../../include/util/alloc.h
controlClient.o: ../../include/util/printf.h ../../include/util/stream.h
controlClient.o: ../../include/stdarg.h ../../include/packos/arch.h
controlClient.o: ../../include/packos/context.h
