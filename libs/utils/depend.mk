# DO NOT DELETE

stream.o: ../../include/util/streamP.h ../../include/util/stream.h
stream.o: ../../include/packos/errors.h ../../include/packos/types.h
stream.o: ../../include/util/printf.h ../../include/stdarg.h
stream.o: ../../include/util/alloc.h stream-kputs.h
stream-file.o: ../../include/util/stream-file.h ../../include/util/stream.h
stream-file.o: ../../include/packos/errors.h ../../include/packos/types.h
stream-file.o: ../../include/util/printf.h ../../include/stdarg.h
stream-file.o: ../../include/file.h ../../include/util/streamP.h
printf.o: ../../include/util/stream.h ../../include/packos/errors.h
printf.o: ../../include/packos/types.h ../../include/util/printf.h
printf.o: ../../include/stdarg.h ../../include/util/ctype.h
printf.o: ../../include/packos/arch.h
stream-string.o: ../../include/util/stream-file.h ../../include/util/stream.h
stream-string.o: ../../include/packos/errors.h ../../include/packos/types.h
stream-string.o: ../../include/util/printf.h ../../include/stdarg.h
stream-string.o: ../../include/file.h ../../include/util/streamP.h
stream-string.o: ../../include/util/alloc.h
stream-kputs.o: stream-kputs.h ../../include/util/stream.h
stream-kputs.o: ../../include/packos/errors.h ../../include/packos/types.h
stream-kputs.o: ../../include/util/printf.h ../../include/stdarg.h
stream-kputs.o: ../../include/util/streamP.h
ctype.o: ../../include/util/ctype.h
string.o: ../../include/util/string.h ../../include/packos/types.h
string.o: ../../include/util/alloc.h ../../include/packos/errors.h
string.o: ../../include/util/ctype.h
alloc.o: ../../include/util/alloc.h ../../include/packos/types.h
alloc.o: ../../include/packos/errors.h ../../include/util/string.h
alloc.o: ../../include/util/stream.h ../../include/util/printf.h
alloc.o: ../../include/stdarg.h ../../include/contextQueue.h
alloc.o: ../../include/packos/context.h ../../include/packos/packet.h
alloc.o: ../../include/packos/sys/contextP.h ../../include/packos/config.h
