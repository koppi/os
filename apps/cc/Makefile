# cc - a tiny self-hosting two-pass C compiler for koppi's OS.
#
#   make            build the OS binary `cc` (bootstrap: host gcc + ld + shim libc)
#   make native     build ./cc-native for development on Linux
#   make selfhost   use cc-native to re-emit cc as an OS ELF (./cc.selfhost)
#   make clean

CC = gcc
LD = ld

LDFLAGS += -melf_i386 -T cc.lds -Map cc.map

SHIM = ../../lib/stdio.o ../../lib/string.o ../../lib/system_calls.o ../../lib/stdlib.o ../../lib/unistd.o

all:
	@$(CC) -w -fno-builtin -nodefaultlibs -nostartfiles -nostdlib -fno-pic -fno-pie -m32 -std=gnu23 \
	    -I ../../include/lib -I ../../include -c cc.c io_os.c
	@$(LD) $(LDFLAGS) -o cc cc.o io_os.o $(SHIM)
	@size cc 2>/dev/null | tail -1 || true

native:
	$(CC) -m32 -g -O1 -fno-pie -no-pie -w -o cc-native cc.c sys_native.c

selfhost: native
	./cc-native --rt prelude.c cc.c -o cc.selfhost

clean:
	@rm -f cc cc-native cc-dbg cc.selfhost *.o cc.map
