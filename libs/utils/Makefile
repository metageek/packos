depth:=../..
subdirs:=
uses:=timer scheduler ip utils
lib:=utils
LIBOBJS:=stream.o stream-file.o printf.o stream-string.o stream-kputs.o\
ctype.o string.o alloc.o

include $(depth)/make.mk

tst::
	echo 16 i 1234567812345678 p | dc
