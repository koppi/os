# zsh - a minimalistic zsh-ish shell for koppi's OS, running in ring 3.
#
#   make          build the OS binary `zsh`
#   make clean

CC = gcc
LD = ld

CFLAGS  = -m32 -std=gnu11 -Os -pipe -ffreestanding -fno-builtin -nostdlib \
          -fno-pic -fno-pie -fno-stack-protector -fno-common \
          -fno-tree-loop-distribute-patterns \
          -Wall -Wextra -Werror -Wno-pointer-to-int-cast

LDFLAGS = -melf_i386 -T zsh.lds -Map zsh.map

all: zsh

zsh: main.o zsh.lds
	@$(LD) $(LDFLAGS) -o zsh main.o
	@size zsh 2>/dev/null | tail -1 || true

main.o: main.c
	@$(CC) $(CFLAGS) -c main.c -o main.o

clean:
	@rm -f zsh zsh.map *.o
