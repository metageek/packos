depth:=..
subdirs:=
uses:=
lib:=kernel

LIBOBJS:=contextK.o\
packetK.o interruptsK.o kprintfK.o kputcK.o boot.o data_end.o main.o\
clock.o pagingK.o exceptions.o swapContextK_TSS.o swapContextK.o

TESTOBJS:=test.o

include $(depth)/make.mk

all:: kernel

kernel: $(depth)/common/libpackos.a $(TESTOBJS) $(LIBFILE) Makefile
	$(LD) -melf_i386 --section-start .text=0x100000 --section-start .rodata=0x10b000 --section-start .data=0x10e000 --section-start .bss=0x10f000 $(TESTOBJS) boot.o -L. -L../common -lpackos -lkernel -lpackos -lkernel -o $@

clean::
	$(RM) kernel *.o

depend::
	makedepend $(INCLUDES) -f depend.mk $(SRCS)
