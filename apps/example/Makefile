LDFLAGS += -melf_i386 -T example.lds -Map example.map

all:
	@$(CC) -Wall -g -Wextra -Werror -fno-builtin -nodefaultlibs -nostartfiles -nostdlib -fno-pic -fno-pie -m32 -I ../../usr/include/lib -c main.c
	@$(LD) $(LDFLAGS) -o example main.o ../../usr/lib/stdio.o ../../usr/lib/string.o ../../usr/lib/system_calls.o ../../usr/lib/stdlib.o

clean:
	@rm -f example example.map *.o
