
# DO NOT DELETE

contextK.o: kprintfK.h ../include/packos/types.h kputcK.h pagingK.h
contextK.o: ../include/packos/errors.h swapContextK.h
contextK.o: ../include/packos/context.h ../include/packos/packet.h
contextK.o: ../include/packos/checksums.h ../include/ip.h
contextK.o: ../include/icmp-types.h ../include/packos/arch.h
contextK.o: ../include/packos/sys/memoryP.h ../include/packos/memory.h
contextK.o: ../include/packos/sys/packetP.h
contextK.o: ../include/packos/sys/interruptsP.h
contextK.o: ../include/packos/interrupts.h ../include/packos/sys/contextP.h
contextK.o: ../include/packos/config.h ../include/packos/sys/blockP.h
contextK.o: ../include/packos/sys/asm.h ../include/util/string.h
packetK.o: ../include/packos/arch.h ../include/packos/types.h
packetK.o: ../include/packos/sys/contextP.h ../include/packos/context.h
packetK.o: ../include/packos/errors.h ../include/packos/packet.h
packetK.o: ../include/packos/config.h ../include/packos/sys/blockP.h
packetK.o: ../include/packos/sys/packetP.h ../include/packos/sys/memoryP.h
packetK.o: ../include/packos/memory.h kprintfK.h
interruptsK.o: ../include/packos/sys/interruptsP.h
interruptsK.o: ../include/packos/interrupts.h ../include/packos/types.h
interruptsK.o: ../include/packos/errors.h ../include/packos/packet.h
interruptsK.o: ../include/packos/sys/blockP.h ../include/packos/sys/packetP.h
interruptsK.o: ../include/packos/arch.h ../include/packos/checksums.h
interruptsK.o: ../include/ip.h ../include/icmp-types.h ../include/udp.h
interruptsK.o: kprintfK.h
kprintfK.o: ../include/stdarg.h ../include/packos/sys/blockP.h
kprintfK.o: ../include/packos/errors.h ../include/packos/types.h kprintfK.h
kprintfK.o: kputcK.h
kputcK.o: kputcK.h
main.o: main.h ../include/packos/sys/asm.h kprintfK.h
main.o: ../include/packos/types.h kputcK.h pagingK.h
main.o: ../include/packos/errors.h ../include/packos/sys/memoryP.h
main.o: ../include/packos/memory.h ../include/packos/context.h
main.o: ../include/packos/packet.h ../include/packos/sys/contextP.h
main.o: ../include/packos/config.h
pagingK.o: pagingK.h ../include/packos/types.h ../include/packos/errors.h
pagingK.o: kprintfK.h
swapContextK_TSS.o: swapContextK.h ../include/packos/context.h
swapContextK_TSS.o: ../include/packos/types.h ../include/packos/errors.h
swapContextK_TSS.o: ../include/packos/packet.h
swapContextK_TSS.o: ../include/packos/sys/contextP.h
swapContextK_TSS.o: ../include/packos/config.h kprintfK.h pagingK.h
test.o: ../include/packos/interrupts.h ../include/packos/types.h
test.o: ../include/packos/errors.h ../include/packos/memory.h
test.o: ../include/packos/context.h ../include/packos/packet.h
test.o: ../include/packos/sys/contextP.h ../include/packos/config.h
test.o: kprintfK.h
boot.o: multiboot.h
swapContextK.o: swapContextK.h ../include/packos/context.h
swapContextK.o: ../include/packos/types.h ../include/packos/errors.h
swapContextK.o: ../include/packos/packet.h
