CC=gcc
LD=ld

LDFLAGS += -melf_i386 -T mem.lds -Map mem.map

all:
	@$(CC) -Wall -g -Wextra -Werror -fno-builtin -nodefaultlibs -nostartfiles -nostdlib -fno-pic -fno-pie -m32 -std=gnu23 -I ../../usr/include/lib -c main.c
	@$(LD) $(LDFLAGS) -o mem main.o ../../usr/lib/stdio.o ../../usr/lib/string.o ../../usr/lib/system_calls.o ../../usr/lib/stdlib.o

clean:
	@rm -f mem mem.map *.o
