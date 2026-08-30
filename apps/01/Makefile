CC=gcc
LD=ld

LDFLAGS += -melf_i386 -T link.lds -Map 01.map

all:
	@$(CC) -Wall -g -Wextra -Werror -fno-builtin -nodefaultlibs -nostartfiles -nostdlib -fno-pic -fno-pie -m32 -I ../../include/lib -c main.c
	@$(LD) $(LDFLAGS) -o 01 main.o ../../lib/stdio.o ../../lib/string.o ../../lib/system_calls.o ../../lib/stdlib.o

clean:
	@rm -f 01 01.map *.o
