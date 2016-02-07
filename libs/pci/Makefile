depth:=../..
subdirs:=
lib:=pci
LIBOBJS:=lookup.o config.o
TESTOBJS:=

libpci.a: lookup.o

lookup.o: pciIds.c

pciIds.c: pci.ids bridges.ids pci.py
	./pci.py >pciIds.c

include $(depth)/make.mk
