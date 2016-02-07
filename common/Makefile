depth:=..
subdirs:=
uses:=
lib:=packos
LIBOBJS:=context.o packet.o errors.o arch.o checksums.o memory.o\
interrupts.o
# contextQueue.o

include $(depth)/make.mk

clean::
	$(RM) packet_unit_test

packet_unit_test: packet_unit_test.o $(LIBFILE)
	$(CC) $(M32) -g packet_unit_test.o -L. -lpackos -o packet_unit_test

check: packet_unit_test
	./packet_unit_test
