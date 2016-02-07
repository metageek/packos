# DO NOT DELETE

context.o: ../include/packos/context.h ../include/packos/types.h
context.o: ../include/packos/errors.h ../include/packos/packet.h
context.o: ../include/packos/sys/contextP.h ../include/packos/config.h
context.o: ../include/packos/sys/blockP.h ../include/packos/sys/packetP.h
context.o: ../include/packos/sys/memoryP.h ../include/packos/memory.h
context.o: ../include/packos/sys/interruptsP.h ../include/packos/interrupts.h
context.o: ../include/packos/sys/asm.h ../include/packos/sys/entry-points.h
context.o: ../include/util/string.h ../include/util/printf.h
context.o: ../include/util/stream.h ../include/stdarg.h ../kernel/kprintfK.h
context.o: ../kernel/kputcK.h
packet.o: ../include/packos/sys/packetP.h ../include/packos/packet.h
packet.o: ../include/packos/types.h ../include/packos/errors.h
packet.o: ../include/packos/arch.h ../include/packos/sys/contextP.h
packet.o: ../include/packos/context.h ../include/packos/config.h
packet.o: ../include/packos/sys/blockP.h ../include/packos/sys/entry-points.h
packet.o: ../include/packos/interrupts.h
errors.o: ../include/packos/errors.h
arch.o: ../include/packos/arch.h ../include/packos/types.h
checksums.o: ../include/packos/checksums.h ../include/packos/packet.h
checksums.o: ../include/packos/types.h ../include/packos/errors.h
checksums.o: ../include/ip.h ../include/icmp-types.h ../include/packos/arch.h
memory.o: ../include/packos/sys/memoryP.h ../include/packos/memory.h
memory.o: ../include/packos/types.h ../include/packos/context.h
memory.o: ../include/packos/errors.h ../include/packos/packet.h
memory.o: ../include/packos/sys/contextP.h ../include/packos/config.h
memory.o: ../kernel/kprintfK.h ../kernel/kputcK.h
interrupts.o: ../include/packos/sys/interruptsP.h
interrupts.o: ../include/packos/interrupts.h ../include/packos/types.h
interrupts.o: ../include/packos/errors.h ../include/packos/packet.h
interrupts.o: ../include/packos/sys/entry-points.h
interrupts.o: ../include/packos/context.h
