
# DO NOT DELETE

ip.o: ../../include/ip.h ../../include/packos/packet.h
ip.o: ../../include/packos/types.h ../../include/packos/errors.h
ip.o: ../../include/icmp-types.h ../../include/iface.h ../../include/tcp.h
ip.o: ../../include/ip-filter.h ../../include/packet-queue.h
ip.o: ../../include/util/stream.h ../../include/util/printf.h
ip.o: ../../include/stdarg.h ../../include/util/string.h
ip.o: ../../include/packos/arch.h ../../include/packos/context.h
ip-filter.o: ../../include/ip-filter.h ../../include/ip.h
ip-filter.o: ../../include/packos/packet.h ../../include/packos/types.h
ip-filter.o: ../../include/packos/errors.h ../../include/icmp-types.h
ip-filter.o: ../../include/iface.h ../../include/tcp.h
ip-filter.o: ../../include/packet-queue.h ../../include/util/alloc.h
ip-filter.o: ../../include/util/stream.h ../../include/util/printf.h
ip-filter.o: ../../include/stdarg.h
ipIterator.o: ../../include/ip.h ../../include/packos/packet.h
ipIterator.o: ../../include/packos/types.h ../../include/packos/errors.h
ipIterator.o: ../../include/icmp-types.h ../../include/util/alloc.h
iface.o: ../../include/util/stream.h ../../include/packos/errors.h
iface.o: ../../include/packos/types.h ../../include/util/printf.h
iface.o: ../../include/stdarg.h ../../include/util/alloc.h
iface.o: ../../include/iface.h ../../include/ip.h
iface.o: ../../include/packos/packet.h ../../include/icmp-types.h
iface.o: ../../include/tcp.h ../../include/ip-filter.h
iface.o: ../../include/packet-queue.h ../../include/icmp.h
iface-native.o: ../../include/util/stream.h ../../include/packos/errors.h
iface-native.o: ../../include/packos/types.h ../../include/util/printf.h
iface-native.o: ../../include/stdarg.h ../../include/util/alloc.h
iface-native.o: ../../include/iface.h ../../include/ip.h
iface-native.o: ../../include/packos/packet.h ../../include/icmp-types.h
iface-native.o: ../../include/tcp.h ../../include/ip-filter.h
iface-native.o: ../../include/packet-queue.h
iface-registry.o: ../../include/util/alloc.h ../../include/packos/types.h
iface-registry.o: ../../include/packos/errors.h ../../include/util/stream.h
iface-registry.o: ../../include/util/printf.h ../../include/stdarg.h
iface-registry.o: ../../include/packos/sys/contextP.h
iface-registry.o: ../../include/packos/context.h
iface-registry.o: ../../include/packos/packet.h ../../include/packos/config.h
iface-registry.o: ../../include/iface.h ../../include/ip.h
iface-registry.o: ../../include/icmp-types.h ../../include/tcp.h
iface-registry.o: ../../include/ip-filter.h ../../include/packet-queue.h
udp.o: ../../include/util/alloc.h ../../include/packos/types.h
udp.o: ../../include/packos/errors.h ../../include/util/stream.h
udp.o: ../../include/util/printf.h ../../include/stdarg.h
udp.o: ../../include/contextQueue.h ../../include/packos/context.h
udp.o: ../../include/packos/packet.h ../../include/udp.h ../../include/ip.h
udp.o: ../../include/icmp-types.h ../../include/iface.h ../../include/tcp.h
udp.o: ../../include/ip-filter.h ../../include/packet-queue.h
udp.o: ../../include/packos/sys/contextP.h ../../include/packos/config.h
udp.o: ../../include/packos/arch.h ../../include/packos/checksums.h util.h
udp.o: ../../include/icmp.h
packet-queue.o: ../../include/util/alloc.h ../../include/packos/types.h
packet-queue.o: ../../include/packos/errors.h ../../include/util/stream.h
packet-queue.o: ../../include/util/printf.h ../../include/stdarg.h
packet-queue.o: ../../include/ip.h ../../include/packos/packet.h
packet-queue.o: ../../include/icmp-types.h ../../include/packet-queue.h
icmp.o: ../../include/util/stream.h ../../include/packos/errors.h
icmp.o: ../../include/packos/types.h ../../include/util/printf.h
icmp.o: ../../include/stdarg.h ../../include/util/string.h
icmp.o: ../../include/iface.h ../../include/ip.h
icmp.o: ../../include/packos/packet.h ../../include/icmp-types.h
icmp.o: ../../include/tcp.h ../../include/ip-filter.h
icmp.o: ../../include/packet-queue.h ../../include/packos/arch.h
icmp.o: ../../include/packos/checksums.h ../../include/icmp.h util.h
util.o: util.h ../../include/packos/packet.h ../../include/packos/types.h
util.o: ../../include/packos/errors.h
