.PHONY:: all app clean depend libs lib tst check ro

all:: app libs

app::
ro::

ifeq ($(shell uname -m),ppc)
CC:=i386-linux-gcc
AS:=i386-linux-as
LD:=i386-linux-ld
endif

ifneq ($(strip $(app)),)
app:: $(app) libs
endif

libs::

ifneq ($(lib),)
libs:: lib
LIBFILE:=lib$(lib).a
lib: $(LIBFILE)
endif

clean::
	$(RM) *.o *.a *~ $(app) depend.mk.bak

OBJS:=$(APPOBJS) $(LIBOBJS) $(TESTOBJS)
SRCS:=$(foreach o,$(OBJS),$(wildcard $(subst .o,.c,$(o)))) $(foreach o,$(OBJS),$(wildcard $(subst .o,.S,$(o))))

ifneq ($strip($(OBJS)),)
depend::
	makedepend $(INCLUDES) -f depend.mk $(SRCS)
endif

include depend.mk

INCLUDES:=-I$(depth)/include
CFLAGS+=$(INCLUDES) -g -Wall -m32

ifneq ($(runsInHost),yes)
CFLAGS+=-nostdinc -fno-stack-protector
LFLAGS+=-melf_i386
else
LFLAGS+=-m32
endif

LFLAGS+=-g
ifneq ($(lib),)
	LFLAGS+=$(LIBFILE)
endif
LFLAGS+=$(foreach lib, $(drivers), -L$(depth)/drivers/$(lib) -l$(lib)Driver)
LFLAGS+=$(foreach lib, $(uses), -L$(depth)/libs/$(lib) -l$(lib))
LFLAGS+=-L$(depth)/kernel -L$(depth)/common -lpackos -lkernel -lpackos

ifneq ($(LIBFILE),)
$(LIBFILE): $(LIBOBJS)
	$(RM) $(LIBFILE)
	ar r $(LIBFILE) $(LIBOBJS)
endif

ifneq ($(app),)
$(app): $(APPOBJS) $(foreach lib, $(uses), $(depth)/libs/$(lib)/lib$(lib).a) $(foreach driver, $(drivers), $(depth)/drivers/$(driver)/lib$(driver)Driver.a) $(LIBFILE) $(depth)/common/libpackos.a  $(depth)/kernel/libkernel.a
ifeq ($(runsInHost),yes)
	$(CC) $(APPOBJS) $(LFLAGS) -o $(app)
else
	$(LD) -Ttext 0x100000 $(depth)/kernel/boot.o $(APPOBJS) $(LFLAGS) -o $(app)
endif
endif

ifneq ($(TESTOBJS),)
test: $(TESTOBJS) $(foreach lib, $(uses), $(depth)/libs/$(lib)/lib$(lib).a) $(LIBFILE) $(depth)/common/libpackos.a  $(depth)/kernel/libkernel.a
	$(CC) $(TESTOBJS) $(LFLAGS) -o test

clean::
	$(RM) test

tst:: test
	./test
endif

ifneq ($(strip $(subdirs)),)
all clean depend tst ro::
	for dir in $(subdirs); do $(MAKE) -C $$dir $@ ; done
endif

ASFLAGS+=$(INCLUDES) -m32

%.s: %.c
	$(CC) $(CFLAGS) -S -o $@ $<

