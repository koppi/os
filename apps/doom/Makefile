CC = gcc
LD = ld
OBJCOPY = objcopy

# Engine configuration - see PORTING.md for the reasoning behind each knob.
#   CMAP256          keep the frame 8-bpp indexed all the way to the blit
#                    syscall, which is what the kernel side takes
#   DOOMGENERIC_RES* render at Doom's native size; the kernel scales it up
#   FILES_DIR        first place d_iwad.c looks for an IWAD
#   NORMALUNIX       selects the non-Windows code paths in the engine
#   FEATURE_SOUND    compile the sound path in and use DG_sound_module
DOOMCFG = -DCMAP256 \
          -DDOOMGENERIC_RESX=320 -DDOOMGENERIC_RESY=200 \
          -DNORMALUNIX -DLINUX -DFEATURE_SOUND \
          -DPRINTF_ALIAS_STANDARD_FUNCTION_NAMES_SOFT=1 \
          '-DFILES_DIR="/rd"'

CFLAGS = -m32 -std=gnu11 -O2 -pipe -ffreestanding -fno-builtin -nostdlib \
         -fno-stack-protector -fno-pic -fno-pie -fno-common \
         -fno-tree-loop-distribute-patterns -fno-strict-aliasing \
         -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
         -I shim -I src $(DOOMCFG)

# The vendored engine is 1990s C and warns copiously; our own files are not,
# so they are built with warnings on.
VENDORCFLAGS = $(CFLAGS) -w
PORTCFLAGS   = $(CFLAGS) -Wall -Wextra -Wno-unused-parameter

LDFLAGS = -melf_i386 -T doom.lds -Map doom.map

DOOMSRC = $(wildcard src/*.c)
DOOMOBJ = $(DOOMSRC:.c=.o)
PORTOBJ = doomgeneric_koppi.o w_file_koppi.o i_sound_koppi.o
SHIMOBJ = shim/printf.o shim/string.o shim/stdlib.o shim/stdio.o \
          shim/libm.o shim/libm_s.o shim/arith64.o
OBJ     = $(DOOMOBJ) $(PORTOBJ) $(SHIMOBJ)

all: doom

# The engine is configured almost entirely through -D, so a change to the
# flags above has to rebuild everything -- otherwise turning FEATURE_SOUND on
# leaves stale objects that were compiled with it off.
$(OBJ): Makefile

doom: $(OBJ) doom.lds
	@$(LD) $(LDFLAGS) -o doom $(OBJ)
	@$(OBJCOPY) --strip-all doom
	@size doom

src/%.o: src/%.c
	@$(CC) $(VENDORCFLAGS) -c $< -o $@

shim/%.o: shim/%.c
	@$(CC) $(PORTCFLAGS) -c $< -o $@

%.o: %.c
	@$(CC) $(PORTCFLAGS) -c $< -o $@

shim/libm_s.o: shim/libm.S
	@$(CC) -m32 -c $< -o $@

# 64-bit soft arithmetic (FixedDiv and friends): build our own copy - the
# kernel's arith64.o may not exist yet when `make` reaches apps/.
shim/arith64.o: ../../arith64.c
	@$(CC) -m32 -O2 -ffreestanding -fno-builtin -nostdlib -fno-pic -fno-pie \
	       -fno-stack-protector -w -c $< -o $@

clean:
	@rm -f doom doom.map $(OBJ)
