CFLAGS += -m32
CFLAGS += -c -I.. -I../include -ffreestanding -fno-builtin -nostdlib
CFLAGS += -Werror -Wall -Wextra
CFLAGS += -Wno-pointer-to-int-cast
# The syscall ABI passes arguments in %ebx; PIC codegen steals %ebx for the GOT
# base, so userspace must be built position-dependent.
CFLAGS += -fno-pic -fno-pie

all:
	@$(CC) $(CFLAGS) unistd.c
	@$(CC) $(CFLAGS) string.c
	@$(CC) $(CFLAGS) stdio.c
	@$(CC) $(CFLAGS) stdlib.c
	@$(CC) $(CFLAGS) system_calls.c

clean:
	@rm -f *.o
